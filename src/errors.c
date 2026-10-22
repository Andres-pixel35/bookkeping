#include <stdio.h>

#include "../include/errors.h"
#include "../include/helpers.h"

// errors are painted red only when the output is a terminal and NO_COLOR is unset
static void red(int on)
{
    fputs(paint(on ? COLOR_RED : COLOR_RESET), stderr);
}

void dic_not_open(const char *dir_name)
{
    red(1);
    fprintf(stderr, "ERROR: Could not open \"%s\" directory.\n", dir_name);
    red(0);
}

void err_year(void)
{
    red(1);
    fprintf(stderr, "ERROR: There is no year before the year 0000\n"
            "And not, we won't work with years Before the Common Era.\n");
    red(0);
}

void previous_file_not_open(const char *previous_file_name, int month, int year)
{
    red(1);
    fprintf(stderr, "ERROR: Could not open the file \"%s\", it either does not exists or you don't have the permissions to access.\n"
                    "Remember that the program keeps in that file the balances your categories had at the start of the open month (\"%02d_%04d\")"
                    " in order to carry them forward.\n"
                    "Please make sure the files inside the \"data\" folder were created by this program and have not been moved.\n",
                    previous_file_name, month, year);
    red(0);
}

void file_empty(const char *previous_file_name, int month, int year)
{
    red(1);
    fprintf(stderr, "\nERROR: The file \"%s\" is empty, please make sure to have in the \"data\" folder only files created for this program.\n"
                    "The program tried to read it in order to take some data for the open month (\"%02d_%04d\").\n"
                    "But the file is empty, either because someone delete the information or the file was not created for this program.\n",
                    previous_file_name, month, year);
    red(0);
}

void missing_values(const char *previous_file_name)
{
    red(1);
    fprintf(stderr, "\nERROR: The program did not found one of the necessary values in the file \"%s\"\n"
                    "Remember that those attributes are essential to keep track of some values like savings or investment\n"
                    "and reflect them in the following months. Since the program did not found them\n"
                    "the file was either modified or it was not created by this program.\n"
                    "Either way, please restore the file from a backup or fix that line by hand.\n", previous_file_name);
    red(0);
}

void wrong_chose(const char *choose)
{
    red(1);
    fprintf(stderr, "\nERROR: wrong input. We asked you to either enter \"y\" or \"n\" but you entered \"%s\"\n", choose);
    red(0);
}

void memory_error(void)
{
    red(1);
    fprintf(stderr, "\nERROR: Could not allocate memory.\n");
    red(0);
}

void file_not_open(const char *curren_file_name)
{
    red(1);
    fprintf(stderr, "ERROR: Could not open \"%s\" file, either you don't have the permissions to create new files or the program doesn't.\n", curren_file_name);
    red(0);
}

void written_error(void)
{
    red(1);
    fprintf(stderr, "ERROR: Could not write the information inside the file. It might be due to insufficient memory or a lack of permissions.\n");
    red(0);
}

void close_file_error(const char *file_name)
{
    red(1);
    fprintf(stderr, "ERROR: Could not close the file \"%s\". It might be due to insufficient memory.\n", file_name);
    red(0);
}

void add_item_error(size_t MAX_ITEMS)
{
    red(1);
    fprintf(stderr, "You can only add at maximum of %zu items.\n", MAX_ITEMS);
    red(0);
}

void input_ended(void)
{
    red(1);
    fprintf(stderr, "\nERROR: The input ended (end of file or Ctrl-D) before this action was finished.\n"
                    "Nothing of this action was saved, everything saved before it is safe.\n");
    red(0);
}

void wrong_arguments(const char *why)
{
    red(1);
    fprintf(stderr, "ERROR: wrong arguments, %s.\n", why);
    red(0);
}

void wrong_option(const char *option, int max)
{
    red(1);
    fprintf(stderr, "\nERROR: wrong input. We asked you to enter a number between 1 and %d but you entered \"%s\"\n", max, option);
    red(0);
}

void invalid_amount(const char *input, const char *why)
{
    red(1);
    fprintf(stderr, "ERROR: \"%s\" is not a valid amount: %s.\n"
                    "Expected input: \"123\", \"123.4\" or \"123.45\" (no sign, no thousands separators, at most two decimals).\n", input, why);
    red(0);
}

void amount_not_positive(void)
{
    red(1);
    fprintf(stderr, "ERROR: You must write a value greater than 0 per each item.\n");
    red(0);
}

void invalid_date(const char *input)
{
    red(1);
    fprintf(stderr, "ERROR: \"%s\" is not a valid date, expected input: \"YYYY-MM-DD\" or only the day of the month (\"DD\").\n", input);
    red(0);
}

void date_outside_month(const char *input, int year, int month)
{
    red(1);
    fprintf(stderr, "ERROR: The date \"%s\" is outside the open month (%04d-%02d).\n"
                    "Only the open month can receive entries, closed months are never edited again.\n", input, year, month);
    red(0);
}

void no_default_date(int year, int month)
{
    red(1);
    fprintf(stderr, "ERROR: Today is not inside the open month (%04d-%02d), so the date can't default to today.\n"
                    "Please give the date explicitly with \"--date YYYY-MM-DD\", or close the month first.\n", year, month);
    red(0);
}

void month_not_started(int year, int month)
{
    red(1);
    fprintf(stderr, "ERROR: The open month (%04d-%02d) has not started yet, so it can't be closed.\n", year, month);
    red(0);
}

void invalid_type(const char *input)
{
    red(1);
    fprintf(stderr, "ERROR: \"%s\" is not an entry type. Use one of: income, expense, debt, saving, saving-withdraw,"
                    " invest, invest-withdraw, invest-value.\n", input);
    red(0);
}

// the withdrawal named is the one that would go over its balance: the one being added, or a later one that a change
// (a new withdrawal before it, a deleted deposit...) would leave without enough money
void insufficient_balance(const char *category, const char *wanted, const char *date, const char *available)
{
    red(1);
    fprintf(stderr, "ERROR: You haven't that amount of money in \"%s\": the withdrawal of \"%s\" on %s needs it,\n"
                    "but at that point there would only be: \"%s\". Please try again.\n", category, wanted, date, available);
    red(0);
}

void unknown_category(const char *name, const char *kind)
{
    red(1);
    fprintf(stderr, "ERROR: There is no %s category called \"%s\". Use \"bookkeeping -c\" to see them, or add it first.\n", kind, name);
    red(0);
}

void category_exists(const char *name)
{
    red(1);
    fprintf(stderr, "ERROR: A category called \"%s\" already exists (names are compared without caring about upper or lower case).\n", name);
    red(0);
}

// every reason is listed at once, so the user knows everything that stands in the way. balance is NULL when the
// category started the month at 0
void category_not_deletable(const char *name, const char *balance, size_t entries)
{
    red(1);
    fprintf(stderr, "ERROR: The category \"%s\" can't be deleted yet:\n", name);
    if (balance != NULL)
    {
        fprintf(stderr, "  - it started the month with %s (only a category that starts a month at 0 can go away)\n", balance);
    }
    if (entries > 0)
    {
        fprintf(stderr, "  - %zu %s of the open month use%s it\n", entries, entries == 1 ? "entry" : "entries", entries == 1 ? "s" : "");
    }
    if (balance != NULL)
    {
        fprintf(stderr, "Withdraw its money, close the month, and delete it when it starts the next month at 0 with no entries.\n");
    }
    else
    {
        fprintf(stderr, "Delete those entries (or move them to another category with -e) first.\n");
    }
    red(0);
}

void report_exists(const char *file_name)
{
    red(1);
    fprintf(stderr, "ERROR: The report \"%s\" already exists and reports are never overwritten, so nothing has been changed.\n"
                    "If a previous close was interrupted, move the existing reports somewhere else and close the month again.\n", file_name);
    red(0);
}

void entry_not_found(int id)
{
    red(1);
    fprintf(stderr, "ERROR: There is no entry with the ID %d in the open month. Use \"bookkeeping -l\" to see the IDs.\n", id);
    red(0);
}
