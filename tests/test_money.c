#include <limits.h>
#include <unistd.h>

#include "test_util.h"
#include "../include/errors.h"
#include "../include/helpers.h"

static long long amount(const char *text)
{
    long long cents = -1;
    const char *why = NULL;
    assert(parse_amount(text, &cents, &why) == 0);
    return cents;
}

static void rejected(const char *text)
{
    long long cents = 0;
    const char *why = NULL;
    assert(parse_amount(text, &cents, &why) == ERR_USAGE);
    assert(why != NULL && why[0] != '\0'); // every refusal says why
}

static void money(long long cents, bool show_plus, const char *expected)
{
    char out[SIZE_MONEY];
    format_money(cents, show_plus, out, sizeof(out));
    if (strcmp(out, expected) != 0)
    {
        fprintf(stderr, "format_money(%lld) = \"%s\", expected \"%s\"\n", cents, out, expected);
    }
    assert(strcmp(out, expected) == 0);
}

static void test_parse_amount(void)
{
    assert(amount("0") == 0);
    assert(amount(".5") == 50);
    assert(amount("5.") == 500);
    assert(amount("123") == 12300);
    assert(amount("123.4") == 12340);
    assert(amount("123.45") == 12345);
    assert(amount("0.01") == 1);
    assert(amount("25000") == 2500000);
    assert(amount("999999999999.99") == 99999999999999LL);    // MAX_LEN_AMOUNT characters
    assert(amount("999999999999999") == 99999999999999900LL); // the biggest whole number still fits in a long long
    rejected("1.234");
    rejected("1..2");
    rejected("1.2.3");
    rejected("");
    rejected(".");
    rejected("-5");
    rejected("+5");
    rejected("1,000");
    rejected("12a");
    rejected(" 12");
    rejected("1e5");
    rejected("1234567890123456"); // one character over MAX_LEN_AMOUNT
}

static void test_format_money(void)
{
    money(0, false, "$0.00");
    money(5, false, "$0.05");
    money(12345, false, "$123.45");
    money(99999, false, "$999.99");
    money(100000, false, "$1,000.00");
    money(453846626, false, "$4,538,466.26");
    money(100000000, false, "$1,000,000.00");
    money(-100000, false, "-$1,000.00");
    money(-5, true, "-$0.05");
    money(6000, true, "+$60.00");
    money(0, true, "$0.00");
    money(99999999999999900LL, false, "$999,999,999,999,999.00");

    char out[SIZE_MONEY];
    format_money(LLONG_MIN, false, out, sizeof(out)); // must not overflow (UBSan would stop here)
    assert(out[0] == '-');

    amount_to_text(12050, out, sizeof(out));
    assert(strcmp(out, "120.50") == 0);
    amount_to_text(-150, out, sizeof(out));
    assert(strcmp(out, "-1.50") == 0);
    assert(amount("120.50") == 12050); // the text form is accepted back as a default answer
}

static void test_dates(void)
{
    int day = 0;
    int year = 0;
    int month = 0;

    assert(parse_day("14", 2026, 10, &day) == 0 && day == 14);
    assert(parse_day("2026-10-31", 2026, 10, &day) == 0 && day == 31);
    assert(parse_day("2026-11-01", 2026, 10, &day) == ERR_DATE);
    assert(parse_day("2025-10-14", 2026, 10, &day) == ERR_DATE);
    assert(parse_day("32", 2026, 10, &day) == ERR_USAGE);
    assert(parse_day("0", 2026, 10, &day) == ERR_USAGE);
    assert(parse_day("2026-02-29", 2026, 2, &day) == ERR_USAGE);
    assert(parse_day("2024-02-29", 2024, 2, &day) == 0 && day == 29);
    assert(parse_day("2026/10/14", 2026, 10, &day) == ERR_USAGE);
    assert(parse_day("", 2026, 10, &day) == ERR_USAGE);
    assert(days_in_month(2026, 2) == 28 && days_in_month(2000, 2) == 29 && days_in_month(1900, 2) == 28);
    assert(days_in_month(2026, 12) == 31 && days_in_month(2026, 4) == 30);

    assert(parse_month("2026-10", &year, &month) == 0 && year == 2026 && month == 10);
    assert(parse_month("2026-13", &year, &month) == ERR_USAGE);
    assert(parse_month("0000-05", &year, &month) == ERR_YEAR);
    assert(parse_month("26-10", &year, &month) == ERR_USAGE);
}

static void test_percentage(void)
{
    assert(calculate_percentage(0, 500) == 0); // no incomes: 0%, never a division by zero
    assert(calculate_percentage(400, 100) == 25.0);
}

// feeds text to stdin, as if the user typed it and then pressed Ctrl-D
static void set_stdin(const char *text)
{
    FILE *file = freopen("/dev/null", "r", stdin);
    assert(file != NULL);
    FILE *input = tmpfile();
    assert(input != NULL);
    fputs(text, input);
    rewind(input);
    assert(dup2(fileno(input), fileno(stdin)) >= 0);
    fclose(input);
    clearerr(stdin);
}

// review bug 9 (and 5): the end of the input is reported instead of looping forever, and errors are not taken for values
static void test_end_of_input(void)
{
    char *line = NULL;
    bool yes = false;
    long long cents = 0;

    set_stdin("abc");
    assert(read_input(&line) == 0 && strcmp(line, "abc") == 0); // the last line has no newline
    free(line);
    assert(read_input(&line) == ERR_EOF && line == NULL);

    set_stdin("");
    assert(read_input(&line) == ERR_EOF);

    set_stdin("maybe\n");
    assert(ask_choose("Question?", 'n', &yes) == ERR_EOF); // one wrong answer, then the input ends: no endless re-prompt

    set_stdin("\n");
    assert(ask_choose("Question?", 'y', &yes) == 0 && yes); // enter takes the default

    set_stdin("1.234\n0\n");
    assert(ask_amount("Amount", NULL, false, &cents) == ERR_EOF); // invalid, then zero (refused), then the end

    set_stdin("\n");
    assert(ask_amount("Amount", "12.5", false, &cents) == 0 && cents == 1250);
}

int main(void)
{
    test_parse_amount();
    test_format_money();
    test_dates();
    test_percentage();
    test_end_of_input();
    printf("test_money: all tests passed\n");
    return 0;
}
