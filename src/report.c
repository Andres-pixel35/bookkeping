#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/report.h"
#include "../include/errors.h"
#include "../include/finance.h"
#include "../include/helpers.h"
#include "../include/ledger.h"

typedef struct ReportContext
{
    const Book *book;
    const MonthSummary *summary;
}
ReportContext;

void build_report_paths(const Book *book, char *personal_report, char *key_drivers, size_t size)
{
    snprintf(personal_report, size, "%s/reports/%02d_%04d_Personal_Report.md", book->dir, book->month, book->year);
    snprintf(key_drivers, size, "%s/reports/%02d_%04d.md", book->dir, book->month, book->year);
}

// existing reports are never overwritten, the close is refused before anything is written
int check_reports_free(const Book *book)
{
    char personal_report[SIZE_FILE_NAME];
    char key_drivers[SIZE_FILE_NAME];

    build_report_paths(book, personal_report, key_drivers, SIZE_FILE_NAME);
    if (access(personal_report, F_OK) == 0)
    {
        report_exists(personal_report);
        return ERR_REPORT_EXISTS;
    }
    if (access(key_drivers, F_OK) == 0)
    {
        report_exists(key_drivers);
        return ERR_REPORT_EXISTS;
    }
    return 0;
}

// writes a name inside a markdown table cell, a "|" written by the user would otherwise split the cell in two
static void md_text(FILE *file, const char *text)
{
    for (const char *c = text; *c != '\0'; c++)
    {
        if (*c == '|')
        {
            fputs("\\|", file);
        }
        else
        {
            fputc(*c, file);
        }
    }
}

static void md_money(FILE *file, long long cents, bool show_plus)
{
    char money[SIZE_MONEY];
    format_money(cents, show_plus, money, sizeof(money));
    fprintf(file, " %s |", money);
}

// one "| label | amount | % of incomes |" row, the percentage stays empty when incomes is NULL
static void summary_row(FILE *file, const char *label, long long cents, const long long *incomes)
{
    fprintf(file, "| %s |", label);
    md_money(file, cents, false);
    if (incomes != NULL)
    {
        fprintf(file, " %.2f%% |\n", calculate_percentage(*incomes, cents));
    }
    else
    {
        fputs(" |\n", file);
    }
}

// gains, losses and changes always show their sign
static void signed_row(FILE *file, const char *label, long long cents)
{
    fprintf(file, "| %s |", label);
    md_money(file, cents, true);
    fputs(" |\n", file);
}

static const char *plural(EntryType type)
{
    return type == ENTRY_INCOME ? "incomes" : (type == ENTRY_EXPENSE ? "expenses" : "debts");
}

// the items of one type added up by name, biggest first, with the percentage each one is of its type. limit cuts the ranking
static int items_table(FILE *file, const Book *book, EntryType type, size_t limit)
{
    Items *items = NULL;
    size_t count = 0;

    int status = aggregate_items(book, type, &items, &count);
    if (status != 0)
    {
        return status;
    }

    if (count == 0)
    {
        fprintf(file, "_No %s recorded this month._\n\n", plural(type));
        return 0;
    }

    fprintf(file, "| # | Item | Amount | %% of %s |\n|--:|:--|--:|--:|\n", plural(type));
    for (size_t i = 0; i < count && i < limit; i++)
    {
        fprintf(file, "| %zu | ", i + 1);
        md_text(file, items[i].name);
        fputs(" |", file);
        md_money(file, items[i].value, false);
        fprintf(file, " %.2f%% |\n", items[i].percentage);
    }
    fputs("\n", file);

    free(items);
    return 0;
}

static void savings_table(FILE *file, const ReportContext *context)
{
    const Book *book = context->book;
    const MonthSummary *summary = context->summary;
    bool any = false;

    for (size_t i = 0; i < book->category_count; i++)
    {
        const CategoryTotals *totals = &summary->categories[i];

        if (book->categories[i].kind != KIND_SAVING)
        {
            continue;
        }
        if (!any)
        {
            fputs("| Category | Opening | Deposits | Withdrawals | Closing |\n|:--|--:|--:|--:|--:|\n", file);
            any = true;
        }
        fputs("| ", file);
        md_text(file, book->categories[i].name);
        fputs(" |", file);
        md_money(file, totals->opening, false);
        md_money(file, totals->deposits, false);
        md_money(file, totals->withdrawals, false);
        md_money(file, totals->closing, false);
        fputs("\n", file);
    }

    if (!any)
    {
        fputs("_No saving categories._\n\n", file);
        return;
    }

    fputs("| **Total** |", file);
    md_money(file, summary->savings_opening, false);
    md_money(file, summary->saving_deposits, false);
    md_money(file, summary->saving_withdrawals, false);
    md_money(file, summary->savings_closing, false);
    fputs("\n\n", file);
}

// full is the table of the personal report, otherwise only the gain or loss of each investment (key drivers)
static void investments_table(FILE *file, const ReportContext *context, bool full)
{
    const Book *book = context->book;
    const MonthSummary *summary = context->summary;
    bool any = false;

    for (size_t i = 0; i < book->category_count; i++)
    {
        const CategoryTotals *totals = &summary->categories[i];

        if (book->categories[i].kind != KIND_INVESTMENT)
        {
            continue;
        }
        if (!any)
        {
            fputs(full ? "| Category | Opening value | Contributions | Withdrawals | Gain/loss | Return % | Closing value |\n"
                         "|:--|--:|--:|--:|--:|--:|--:|\n"
                       : "| Category | Gain/loss | Return % |\n|:--|--:|--:|\n", file);
            any = true;
        }
        fputs("| ", file);
        md_text(file, book->categories[i].name);
        fputs(" |", file);
        if (full)
        {
            md_money(file, totals->opening, false);
            md_money(file, totals->deposits, false);
            md_money(file, totals->withdrawals, false);
        }
        md_money(file, totals->gain, true);
        fprintf(file, " %+.2f%% |", return_percentage(totals));
        if (full)
        {
            md_money(file, totals->closing, false);
        }
        fputs("\n", file);
    }

    if (!any)
    {
        fputs("_No investment categories._\n\n", file);
        return;
    }

    // the total return uses the same formula with the totals of every investment
    CategoryTotals total = { summary->invest_opening, summary->invest_contributions, summary->invest_withdrawals,
                             summary->invest_closing, summary->invest_gain };

    fputs("| **Total** |", file);
    if (full)
    {
        md_money(file, total.opening, false);
        md_money(file, total.deposits, false);
        md_money(file, total.withdrawals, false);
    }
    md_money(file, total.gain, true);
    fprintf(file, " %+.2f%% |", return_percentage(&total));
    if (full)
    {
        md_money(file, total.closing, false);
    }
    fputs("\n\n", file);
}

// every entry of the month, day by day
static int ledger_table(FILE *file, const Book *book)
{
    const Entry **sorted = NULL;

    int status = sort_entries(book, &sorted);
    if (status != 0)
    {
        return status;
    }

    if (book->entry_count == 0)
    {
        fputs("_No entries this month._\n", file);
        return 0;
    }

    fputs("| ID | Date | Type | Category / item | Amount |\n|--:|:--|:--|:--|--:|\n", file);
    for (size_t i = 0; i < book->entry_count; i++)
    {
        const Entry *entry = sorted[i];
        fprintf(file, "| %d | %04d-%02d-%02d | %s | ", entry->id, book->year, book->month, entry->day, type_label(entry->type));
        md_text(file, entry_target(entry));
        fputs(" |", file);
        md_money(file, entry->amount, false);
        fputs("\n", file);
    }

    free(sorted);
    return 0;
}

static int write_personal_report(FILE *file, const void *data)
{
    const ReportContext *context = data;
    const Book *book = context->book;
    const MonthSummary *s = context->summary;
    const long long *incomes = &s->incomes;
    char money[SIZE_MONEY];
    int status = 0;

    fprintf(file, "# Personal Finance Report - %s %04d\n\n", month_name(book->month), book->year);
    fprintf(file, "**Period:** %04d-%02d-01 to %04d-%02d-%02d\n\n", book->year, book->month,
            book->year, book->month, days_in_month(book->year, book->month));

    fputs("## Summary\n\n| Concept | Amount | % of incomes |\n|:--|--:|--:|\n", file);
    summary_row(file, "Opening cash", s->opening_cash, NULL);
    summary_row(file, "Incomes", s->incomes, incomes);
    summary_row(file, "Expenses", s->expenses, incomes);
    summary_row(file, "Debts", s->debts, incomes);
    summary_row(file, "Saved (deposits into savings)", s->saving_deposits, incomes);
    summary_row(file, "Withdrawn from savings", s->saving_withdrawals, incomes);
    summary_row(file, "Invested (contributions)", s->invest_contributions, incomes);
    summary_row(file, "Withdrawn from investments", s->invest_withdrawals, incomes);
    signed_row(file, "Investment gain/loss", s->invest_gain);
    summary_row(file, "**Closing cash**", s->closing_cash, NULL);
    summary_row(file, "Net worth at the start", s->opening_net_worth, NULL);
    summary_row(file, "**Net worth at the end**", s->closing_net_worth, NULL);
    signed_row(file, "Net worth change", s->closing_net_worth - s->opening_net_worth);
    fputs("\n", file);

    format_money(s->incomes, false, money, sizeof(money));
    fprintf(file, "## Incomes - %s\n\n", money);
    status = items_table(file, book, ENTRY_INCOME, book->entry_count);

    format_money(s->expenses, false, money, sizeof(money));
    fprintf(file, "## Expenses - %s (%.2f%% of incomes)\n\n", money, calculate_percentage(s->incomes, s->expenses));
    if (status == 0)
    {
        status = items_table(file, book, ENTRY_EXPENSE, book->entry_count);
    }

    format_money(s->debts, false, money, sizeof(money));
    fprintf(file, "## Debts - %s (%.2f%% of incomes)\n\n", money, calculate_percentage(s->incomes, s->debts));
    if (status == 0)
    {
        status = items_table(file, book, ENTRY_DEBT, book->entry_count);
    }

    fputs("## Savings\n\n", file);
    savings_table(file, context);

    fputs("## Investments\n\n", file);
    fputs("Closing value = the latest value update of the month, moved by the contributions and withdrawals after it.\n"
          "Gain/loss = closing value - opening value - contributions + withdrawals.\n"
          "Return % = gain/loss / (opening value + contributions).\n\n", file);
    investments_table(file, context, true);

    fputs("## Ledger\n\n", file);
    if (status == 0)
    {
        status = ledger_table(file, book);
    }
    return status;
}

static int write_key_drivers(FILE *file, const void *data)
{
    const ReportContext *context = data;
    const Book *book = context->book;
    const MonthSummary *s = context->summary;
    const long long *incomes = &s->incomes;
    char money[SIZE_MONEY];
    int status = 0;

    fprintf(file, "# Financial Overview & Key Drivers - %s %04d\n\n", month_name(book->month), book->year);

    format_money(s->incomes, false, money, sizeof(money));
    fprintf(file, "## Top incomes - %s\n\n", money);
    status = items_table(file, book, ENTRY_INCOME, TOP_SIZE);

    format_money(s->expenses, false, money, sizeof(money));
    fprintf(file, "## Top expenses - %s (%.2f%% of incomes)\n\n", money, calculate_percentage(s->incomes, s->expenses));
    if (status == 0)
    {
        status = items_table(file, book, ENTRY_EXPENSE, TOP_SIZE);
    }

    format_money(s->debts, false, money, sizeof(money));
    fprintf(file, "## Top debts - %s (%.2f%% of incomes)\n\n", money, calculate_percentage(s->incomes, s->debts));
    if (status == 0)
    {
        status = items_table(file, book, ENTRY_DEBT, TOP_SIZE);
    }

    fputs("## Savings and investments\n\n| Concept | Amount | % of incomes |\n|:--|--:|--:|\n", file);
    summary_row(file, "Saved this month", s->saving_deposits, incomes);
    summary_row(file, "Withdrawn from savings", s->saving_withdrawals, incomes);
    summary_row(file, "Invested this month", s->invest_contributions, incomes);
    summary_row(file, "Withdrawn from investments", s->invest_withdrawals, incomes);
    summary_row(file, "Savings at the end of the month", s->savings_closing, NULL);
    summary_row(file, "Investments value at the end of the month", s->invest_closing, NULL);
    fputs("\n", file);

    fputs("## Investment gain/loss\n\n", file);
    investments_table(file, context, false);

    fputs("## Net worth\n\n| Concept | Amount |\n|:--|--:|\n", file);
    fputs("| Closing cash |", file);
    md_money(file, s->closing_cash, false);
    fputs("\n| Net worth at the start |", file);
    md_money(file, s->opening_net_worth, false);
    fputs("\n| Net worth at the end |", file);
    md_money(file, s->closing_net_worth, false);
    fputs("\n| **Net worth change** |", file);
    md_money(file, s->closing_net_worth - s->opening_net_worth, true);
    fputs("\n", file);

    return status;
}

/**
 * Writes reports/MM_YYYY_Personal_Report.md and reports/MM_YYYY.md, each through a temporary file and rename().
 * Existing reports are never overwritten, and if the second one fails the first one is removed again,
 * so a month never ends up with only half of its reports.
 *
 * return 0, ERR_REPORT_EXISTS, ERR_FILE or ERR_MEMORY
 */
int write_reports(const Book *book, const MonthSummary *summary)
{
    char personal_report[SIZE_FILE_NAME];
    char key_drivers[SIZE_FILE_NAME];
    ReportContext context = { book, summary };

    int status = check_reports_free(book);
    if (status != 0)
    {
        return status;
    }

    build_report_paths(book, personal_report, key_drivers, SIZE_FILE_NAME);
    status = write_file_atomic(personal_report, write_personal_report, &context);
    if (status != 0)
    {
        return status;
    }

    status = write_file_atomic(key_drivers, write_key_drivers, &context);
    if (status != 0)
    {
        remove(personal_report);
    }
    return status;
}

/**
 * Closes the open month: writes both reports, carries the closing balances forward and opens the next month.
 * state.csv is written last, it's the file that says which month is open.
 *
 * param book The open month, it becomes the next month (with no entries)
 * param summary The totals of the month being closed (from compute_summary)
 * return 0, ERR_REPORT_EXISTS, ERR_FILE or ERR_MEMORY
 */
int close_month(Book *book, const MonthSummary *summary)
{
    int status = write_reports(book, summary);
    if (status != 0)
    {
        return status;
    }

    carry_forward(book, summary);

    status = save_categories(book);
    if (status == 0)
    {
        status = save_state(book);
    }
    return status;
}
