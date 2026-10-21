#include "test_util.h"
#include "../include/errors.h"
#include "../include/helpers.h"
#include "../include/report.h"

static void write_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static void test_csv_round_trip(void)
{
    const char *fields[] = { "plain", "food, snacks", "say \"hi\"", "two\nlines", "", "\"" };
    size_t count = sizeof(fields) / sizeof(fields[0]);
    CsvRecord record;
    FILE *file = tmpfile();
    assert(file != NULL);

    for (size_t i = 0; i < count; i++)
    {
        csv_write_field(file, fields[i], i + 1 == count ? '\n' : ',');
    }
    fputs("last,line", file); // no newline at the end of the file
    rewind(file);

    assert(csv_read_record(file, &record) == 0);
    assert(record.count == count);
    for (size_t i = 0; i < count; i++)
    {
        assert(strcmp(record.fields[i], fields[i]) == 0);
    }
    assert(csv_read_record(file, &record) == 0 && record.count == 2 && strcmp(record.fields[1], "line") == 0);
    assert(csv_read_record(file, &record) == CSV_END);
    fclose(file);

    const char *malformed[] = { "\"never closed\n", "a,b,c,d,e,f,g\n", "ab\"c\n", "\"x\"y\n" };
    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++)
    {
        file = tmpfile();
        fputs(malformed[i], file);
        rewind(file);
        assert(csv_read_record(file, &record) == ERR_FILE);
        fclose(file);
    }
}

static void reload(const Book *book, Book *loaded)
{
    assert(load_book(book->dir, loaded) == 0);
    assert(loaded->year == book->year && loaded->month == book->month && loaded->opening_cash == book->opening_cash);
    assert(loaded->category_count == book->category_count && loaded->entry_count == book->entry_count);
    for (size_t i = 0; i < book->entry_count; i++)
    {
        const Entry *a = &book->entries[i];
        const Entry *b = &loaded->entries[i];
        assert(a->id == b->id && a->day == b->day && a->type == b->type && a->amount == b->amount);
        assert(strcmp(a->name, b->name) == 0 && strcmp(a->category, b->category) == 0);
    }
}

// append, edit and delete go to the files and come back identical; review bug 6: no limit of 100 items any more
static void test_ledger_round_trip(void)
{
    char dir[SIZE_DIR_NAME];
    Book loaded;

    make_temp_dir(dir, sizeof(dir));
    Book book = new_book(dir, -2500);
    add_category(&book, KIND_SAVING, "Trip, summer", 1000);
    add_category(&book, KIND_INVESTMENT, "Index \"world\"", 0);
    assert(save_categories(&book) == 0 && save_state(&book) == 0);
    assert(book_exists(dir));

    for (int i = 0; i < 150; i++)
    {
        Entry entry = make_entry(ENTRY_EXPENSE, i % 2 ? "food, snacks" : "rent", 100 + i, 1 + i % 28);
        assert(ledger_add_entry(&book, &entry) == 0 && entry.id == i + 1);
    }
    Entry deposit = make_entry(ENTRY_SAVING, "trip, SUMMER", 500, 3);
    snprintf(deposit.category, sizeof(deposit.category), "%s", "Trip, summer");
    assert(ledger_add_entry(&book, &deposit) == 0);

    reload(&book, &loaded);
    free_book(&loaded);

    Entry edited = book.entries[10];
    edited.type = ENTRY_INVEST_VALUE;
    snprintf(edited.category, sizeof(edited.category), "%s", "Index \"world\"");
    edited.name[0] = '\0';
    edited.amount = 0; // a value update may be 0 (everything lost)
    edited.day = 31;
    assert(ledger_replace_entry(&book, 10, &edited) == 0);
    assert(book.entries[10].id == 11);

    assert(ledger_delete_entry(&book, 0) == 0);
    assert(book.entries[0].id == 2 && find_entry(&book, 1) == -1 && find_entry(&book, 151) == 149); // IDs never move

    reload(&book, &loaded);
    assert(loaded.entries[9].type == ENTRY_INVEST_VALUE && loaded.entries[9].day == 31);
    free_book(&loaded);

    // refused changes leave the files untouched
    Entry withdraw = make_entry(ENTRY_SAVING_WITHDRAW, "Trip, summer", 1501, 30);
    assert(ledger_add_entry(&book, &withdraw) == ERR_BALANCE);
    assert(ledger_delete_entry(&book, 149) == 0); // the deposit (ID 151), nothing needed it
    withdraw.amount = 1000;
    assert(ledger_add_entry(&book, &withdraw) == 0);
    assert(ledger_set_opening(&book, 0, 999) == ERR_BALANCE); // the withdrawal relied on the opening balance
    assert(book.categories[0].opening == 1000);
    reload(&book, &loaded);
    free_book(&loaded);

    // the rename reaches the entries in the file too
    assert(ledger_rename_category(&book, 0, "Holidays") == 0);
    reload(&book, &loaded);
    assert(strcmp(loaded.entries[loaded.entry_count - 1].category, "Holidays") == 0);
    free_book(&loaded);

    assert(ledger_delete_category(&book, 0) == ERR_CATEGORY);
    assert(ledger_delete_category(&book, 1) == ERR_CATEGORY); // still used by the edited entry
    Category bonds = { KIND_INVESTMENT, "Bonds", 0 };
    assert(ledger_add_category(&book, &bonds) == 0);
    Category duplicate = { KIND_SAVING, "BONDS", 0 };
    assert(ledger_add_category(&book, &duplicate) == ERR_CATEGORY);
    assert(ledger_delete_category(&book, 2) == 0);
    assert(ledger_set_opening_cash(&book, 12345) == 0);
    reload(&book, &loaded);
    free_book(&loaded);

    free_book(&book);
    remove_dir(dir);
}

// review bug 4: the carry-over comes from the csv files, so names that used to confuse the strstr parsing do nothing,
// and the reports are never read (deleting them changes nothing)
static void test_carry_over_ignores_names_and_reports(void)
{
    char dir[SIZE_DIR_NAME];
    char path[SIZE_FILE_NAME];
    MonthSummary s;
    Book loaded;

    make_temp_dir(dir, sizeof(dir));
    Book book = new_book(dir, 100000);
    add_category(&book, KIND_SAVING, "Travels", 20000);
    add_category(&book, KIND_INVESTMENT, "Stocks", 30000);
    assert(save_categories(&book) == 0 && save_state(&book) == 0);

    const char *tricky[] = { "- Savings", "Stocks", "Travels", "- Final balance", "Currencies (forex)" };
    for (size_t i = 0; i < sizeof(tricky) / sizeof(tricky[0]); i++)
    {
        Entry entry = make_entry(ENTRY_EXPENSE, tricky[i], 777777, 2);
        assert(ledger_add_entry(&book, &entry) == 0);
    }
    Entry income = make_entry(ENTRY_INCOME, "Stocks", 9000000, 1);
    Entry value = make_entry(ENTRY_INVEST_VALUE, "Stocks", 31000, 9);
    assert(ledger_add_entry(&book, &income) == 0 && ledger_add_entry(&book, &value) == 0);

    assert(compute_summary(&book, &s) == 0);
    long long cash = s.closing_cash;
    assert(close_month(&book, &s) == 0);
    free_summary(&s);

    snprintf(path, sizeof(path), "%s/reports/10_2026.md", dir);
    assert(remove(path) == 0);
    snprintf(path, sizeof(path), "%s/reports/10_2026_Personal_Report.md", dir);
    assert(remove(path) == 0);

    assert(load_book(dir, &loaded) == 0);
    assert(loaded.year == 2026 && loaded.month == 11 && loaded.entry_count == 0);
    assert(loaded.opening_cash == cash && cash == 100000 + 9000000 - 5 * 777777);
    assert(loaded.categories[0].opening == 20000 && loaded.categories[1].opening == 31000);
    free_book(&loaded);

    // the closed month keeps its own file, untouched
    snprintf(path, sizeof(path), "%s/data/2026-10.csv", dir);
    FILE *closed = fopen(path, "r");
    assert(closed != NULL);
    fclose(closed);

    free_book(&book);
    remove_dir(dir);
}

static void test_malformed_files(void)
{
    char dir[SIZE_DIR_NAME];
    char path[SIZE_FILE_NAME];
    Book book;

    make_temp_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/data/state.csv", dir);
    write_file(path, "open_month,opening_cash_cents\n2026-10,100\n");
    assert(load_book(dir, &book) == ERR_FILE); // no categories.csv
    free_book(&book);

    snprintf(path, sizeof(path), "%s/data/categories.csv", dir);
    write_file(path, "kind,name,opening_balance_cents\nsaving,Trip,500\n\n");
    assert(load_book(dir, &book) == 0 && book.category_count == 1 && book.entry_count == 0); // blank line ignored
    free_book(&book);

    const char *bad_ledgers[] =
    {
        "id,date,type,category,name,amount_cents\n1,2026-11-01,expense,,food,100\n",     // another month
        "id,date,type,category,name,amount_cents\n1,2026-10-01,saving,Unknown,,100\n",   // unknown category
        "id,date,type,category,name,amount_cents\n2,2026-10-01,expense,,a,1\n1,2026-10-01,expense,,b,1\n", // IDs going back
        "id,date,type,category,name,amount_cents\n1,2026-10-01,expense,,food,12.5\n",    // not cents
        "id,date,type,category,name,amount_cents\n1,2026-10-01,expense,,food,0\n",       // zero expense
        "",                                                                               // empty
    };
    snprintf(path, sizeof(path), "%s/data/2026-10.csv", dir);
    for (size_t i = 0; i < sizeof(bad_ledgers) / sizeof(bad_ledgers[0]); i++)
    {
        write_file(path, bad_ledgers[i]);
        assert(load_book(dir, &book) == ERR_FILE);
        free_book(&book);
    }

    remove_dir(dir);
}

int main(void)
{
    test_csv_round_trip();
    test_ledger_round_trip();
    test_carry_over_ignores_names_and_reports();
    test_malformed_files();
    printf("test_ledger: all tests passed\n");
    return 0;
}
