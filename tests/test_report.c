#include "test_util.h"
#include "../include/errors.h"
#include "../include/helpers.h"
#include "../include/report.h"

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "r");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    assert(text != NULL && fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    fclose(file);
    return text;
}

static size_t occurrences(const char *text, const char *part)
{
    size_t count = 0;
    for (const char *at = strstr(text, part); at != NULL; at = strstr(at + 1, part))
    {
        count++;
    }
    return count;
}

// review bug 3: reports are written once (never appended to) and never overwritten
static void test_reports(void)
{
    char dir[SIZE_DIR_NAME];
    char personal[SIZE_FILE_NAME];
    char key[SIZE_FILE_NAME];
    MonthSummary s;

    make_temp_dir(dir, sizeof(dir));
    Book book = new_book(dir, 100000);
    add_category(&book, KIND_SAVING, "Rainy | days", 0);
    add_category(&book, KIND_INVESTMENT, "World fund", 100000);
    push(&book, make_entry(ENTRY_INCOME, "salary", 500000, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "rent | flat", 100000, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "food", 1000, 2));
    push(&book, make_entry(ENTRY_EXPENSE, "Food", 2000, 3));
    push(&book, make_entry(ENTRY_EXPENSE, "bus", 500, 3));
    push(&book, make_entry(ENTRY_EXPENSE, "cinema", 400, 3));
    push(&book, make_entry(ENTRY_SAVING, "Rainy | days", 5000, 4));
    push(&book, make_entry(ENTRY_INVEST_VALUE, "World fund", 110000, 5));

    assert(compute_summary(&book, &s) == 0);
    assert(write_reports(&book, &s) == 0);
    build_report_paths(&book, personal, key, SIZE_FILE_NAME);

    char *text = read_file(personal);
    assert(occurrences(text, "# Personal Finance Report - October 2026") == 1);
    assert(strstr(text, "| rent \\| flat | $1,000.00 |") != NULL);   // "|" escaped inside the cell
    assert(strstr(text, "| Rainy \\| days | $0.00 | $50.00 | $0.00 | $50.00 |") != NULL);
    assert(strstr(text, "| 2 | food | $30.00 |") != NULL);             // aggregated by name
    assert(strstr(text, "| World fund | $1,000.00 | $0.00 | $0.00 | +$100.00 | +10.00% | $1,100.00 |") != NULL);
    assert(strstr(text, "| **Closing cash** | $4,911.00 | |") != NULL);
    assert(strstr(text, "| 7 | 2026-10-04 | Saving deposit | Rainy \\| days | $50.00 |") != NULL);
    free(text);

    text = read_file(key);
    assert(strstr(text, "| 3 | bus | $5.00 |") != NULL && strstr(text, "cinema") == NULL); // only the top TOP_SIZE
    assert(strstr(text, "| **Net worth change** | +$4,061.00 |") != NULL);
    free(text);

    // a second close of the same month is refused and leaves both reports as they were
    text = read_file(personal);
    assert(write_reports(&book, &s) == ERR_REPORT_EXISTS);
    assert(close_month(&book, &s) == ERR_REPORT_EXISTS && book.month == 10);
    char *again = read_file(personal);
    assert(strcmp(text, again) == 0);
    free(text);
    free(again);

    assert(remove(key) == 0); // even only one of them is enough to refuse
    assert(check_reports_free(&book) == ERR_REPORT_EXISTS);

    free_summary(&s);
    free_book(&book);
    remove_dir(dir);
}

static void test_empty_month(void)
{
    char dir[SIZE_DIR_NAME];
    char personal[SIZE_FILE_NAME];
    char key[SIZE_FILE_NAME];
    MonthSummary s;

    make_temp_dir(dir, sizeof(dir));
    Book book = new_book(dir, -5000);
    assert(save_categories(&book) == 0 && save_state(&book) == 0);
    assert(compute_summary(&book, &s) == 0);
    assert(close_month(&book, &s) == 0);
    assert(book.month == 11 && book.opening_cash == -5000);

    book.month = 10;
    build_report_paths(&book, personal, key, SIZE_FILE_NAME);
    char *text = read_file(personal);
    assert(strstr(text, "_No incomes recorded this month._") != NULL && strstr(text, "| Opening cash | -$50.00 | |") != NULL);
    free(text);

    free_summary(&s);
    free_book(&book);
    remove_dir(dir);
}

int main(void)
{
    test_reports();
    test_empty_month();
    printf("test_report: all tests passed\n");
    return 0;
}
