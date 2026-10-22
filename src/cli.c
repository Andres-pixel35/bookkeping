#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../include/cli.h"
#include "../include/errors.h"
#include "../include/finance.h"
#include "../include/help.h"
#include "../include/helpers.h"
#include "../include/ledger.h"
#include "../include/report.h"

#define MENU_OPTIONS 8
#define CATEGORY_OPTIONS 6
#define SIZE_PROMPT 256
#define ADD_DONE 1 // the user answered "done": the loop of one entry type ends
#define RULER "============================================================"

static const char *USAGE =
    "Usage: bookkeeping [-h [flags|features] | -l | -s | -C | -e <id> | -r <id>\n"
    "                   | -a <type> <item|category> <amount> [-d YYYY-MM-DD]\n"
    "                   | -c [-a <saving|investment> <name> [<opening>] | -n <old> <new> | -r <name> | -o <name> <amount>]\n"
    "                   | -o [-]<amount>]\n"
    "Every command also has a long and a word form (-a, --add, add). Run \"bookkeeping -h flags\" for the list.\n";

static int usage(const char *why)
{
    wrong_arguments(why);
    fputs(USAGE, stderr);
    return ERR_USAGE;
}

// every screen of the menu starts with a title after a blank line, so the output of one action is easy to tell from the next
static void print_title(const char *title)
{
    printf("\n%s--- %s ---%s\n\n", paint(COLOR_BOLD), title, paint(COLOR_RESET));
}

// arg is any of the names in names ("add|--add|-a"): every command has a word, a long and a short form
static bool matches(const char *arg, const char *names)
{
    size_t len = strlen(arg);

    while (*names != '\0')
    {
        size_t name_len = strcspn(names, "|");
        if (name_len == len && strncmp(arg, names, len) == 0)
        {
            return true;
        }
        names += name_len + (names[name_len] == '|');
    }
    return false;
}

// the memory, the files or the end of the input stop the menu, any other error was already explained and the menu goes on
static bool is_fatal(int status)
{
    return status == ERR_MEMORY || status == ERR_FILE || status == ERR_DIC || status == ERR_EOF;
}

// the date of a new entry defaults to today only when today belongs to the open month, 0 means there is no default
static int default_day(const Book *book)
{
    int year = 0;
    int month = 0;
    int day = 0;

    get_today(&year, &month, &day);
    return (year == book->year && month == book->month) ? day : 0;
}

static void print_notice(const Book *book)
{
    int year = 0;
    int month = 0;
    int day = 0;

    get_today(&year, &month, &day);
    if (year > book->year || (year == book->year && month > book->month))
    {
        printf("%sNotice:%s today is past the open month (%04d-%02d). When everything is recorded,"
               " close it with \"bookkeeping -C\" (or option 6 of the menu).\n",
               paint(COLOR_BOLD), paint(COLOR_RESET), book->year, book->month);
    }
}

static void print_return(const CategoryTotals *totals)
{
    double percentage = 0;

    if (return_percentage(totals, &percentage))
    {
        printf("%+.2f%%", percentage);
    }
    else
    {
        printf("return n/a");
    }
}

static void print_line(const char *label, long long cents, bool show_plus)
{
    printf("  %-34s ", label);
    print_money(cents, show_plus);
    printf("\n");
}

static void print_entry(const Book *book, const Entry *entry)
{
    printf("  %4d  %04d-%02d-%02d  %-24s  %-30s  ", entry->id, book->year, book->month, entry->day,
           type_label(entry->type), entry_target(entry));
    print_money(entry->amount, false);
    printf("\n");
}

static int print_entries(const Book *book)
{
    const Entry **sorted = NULL;

    if (book->entry_count == 0)
    {
        printf("No entries in the open month yet.\n");
        return 0;
    }

    int status = sort_entries(book, &sorted);
    if (status != 0)
    {
        return status;
    }

    printf("%s  %4s  %-10s  %-24s  %-30s  %s%s\n", paint(COLOR_BOLD), "ID", "Date", "Type", "Category / item", "Amount", paint(COLOR_RESET));
    for (size_t i = 0; i < book->entry_count; i++)
    {
        print_entry(book, sorted[i]);
    }

    free(sorted);
    return 0;
}

static void print_category_balances(const Book *book, const MonthSummary *summary)
{
    for (int kind = KIND_SAVING; kind <= KIND_INVESTMENT; kind++)
    {
        bool any = false;

        printf("%s%s%s categories%s (opening -> now):\n", kind == KIND_SAVING ? "" : "\n", paint(COLOR_BOLD), kind == KIND_SAVING ? "Saving" : "Investment", paint(COLOR_RESET));
        for (size_t i = 0; i < book->category_count; i++)
        {
            if ((int)book->categories[i].kind != kind)
            {
                continue;
            }
            any = true;
            printf("  %-34s ", book->categories[i].name);
            print_money(summary->categories[i].opening, false);
            printf(" -> ");
            print_money(summary->categories[i].closing, false);
            if (kind == KIND_INVESTMENT)
            {
                printf("  (gain/loss ");
                print_money(summary->categories[i].gain, true);
                printf(", ");
                print_return(&summary->categories[i]);
                printf(")");
            }
            printf("\n");
        }
        if (!any)
        {
            printf("  (none)\n");
        }
    }
}

static void print_summary(const Book *book, const MonthSummary *s)
{
    printf("%s=== %s %04d (open month) ===%s\n", paint(COLOR_BOLD), month_name(book->month), book->year, paint(COLOR_RESET));
    print_line("Opening cash", s->opening_cash, false);
    print_line("Incomes", s->incomes, false);
    print_line("Expenses", s->expenses, false);
    print_line("Debts", s->debts, false);
    print_line("Saved (deposits into savings)", s->saving_deposits, false);
    print_line("Withdrawn from savings", s->saving_withdrawals, false);
    print_line("Invested (contributions)", s->invest_contributions, false);
    print_line("Withdrawn from investments", s->invest_withdrawals, false);
    print_line("Investment gain/loss", s->invest_gain, true);
    print_line("Cash", s->closing_cash, false);
    print_line("Net worth", s->closing_net_worth, false);
    print_line("Net worth change", s->closing_net_worth - s->opening_net_worth, true);
    printf("\n");
    print_category_balances(book, s);
}

static int show_summary(const Book *book)
{
    MonthSummary summary;
    int status = compute_summary(book, &summary);
    if (status == 0)
    {
        print_summary(book, &summary);
    }
    free_summary(&summary);
    return status;
}

static int show_categories(const Book *book)
{
    MonthSummary summary;
    int status = compute_summary(book, &summary);
    if (status == 0)
    {
        print_category_balances(book, &summary);
    }
    free_summary(&summary);
    return status;
}

// strict positive number, used for the menu options and the entry IDs
static bool parse_id(const char *text, int *id)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (text[0] < '0' || text[0] > '9' || *end != '\0' || value < 1 || value > 1000000000L)
    {
        return false;
    }
    *id = (int)value;
    return true;
}

static int ask_option(const char *prompt, int max, int *option)
{
    while (1)
    {
        char *input = NULL;
        int status = prompt_line(prompt, NULL, &input);
        if (status != 0)
        {
            return status;
        }

        if (parse_id(input, option) && *option <= max)
        {
            free(input);
            return 0;
        }

        wrong_option(input, max);
        free(input);
    }
}

static int ask_name(const char *prompt, const char *default_name, char *name)
{
    while (1)
    {
        char *input = NULL;
        int status = prompt_line(prompt, default_name, &input);
        if (status != 0)
        {
            return status;
        }

        if (check_item(input))
        {
            snprintf(name, MAX_NAME_LEN + 1, "%s", input);
            free(input);
            return 0;
        }
        free(input);
    }
}

static bool category_listed(const Book *book, const CategoryKind *kind, size_t index)
{
    return kind == NULL || book->categories[index].kind == *kind;
}

// lists the categories (of one kind, or all of them when kind is NULL) with a number, and returns how many there are
static size_t list_categories(const Book *book, const CategoryKind *kind)
{
    size_t count = 0;

    for (size_t i = 0; i < book->category_count; i++)
    {
        if (category_listed(book, kind, i))
        {
            printf("  %zu) %s (%s)\n", ++count, book->categories[i].name, kind_keyword(book->categories[i].kind));
        }
    }

    if (count == 0)
    {
        printf("You have no %s categories yet, add one first in \"Categories\".\n", kind == NULL ? "" : kind_keyword(*kind));
    }
    return count;
}

// the user can pick a listed category by its number or by its name
static int resolve_category(const Book *book, const CategoryKind *kind, const char *input, size_t *index)
{
    int number = 0;
    int found = find_category(book, input);

    if (parse_id(input, &number))
    {
        // the number is the position among the listed categories
        for (size_t i = 0; i < book->category_count; i++)
        {
            if (category_listed(book, kind, i) && --number == 0)
            {
                *index = i;
                return 0;
            }
        }
    }
    else if (found >= 0 && category_listed(book, kind, (size_t)found))
    {
        *index = (size_t)found;
        return 0;
    }

    unknown_category(input, kind == NULL ? "saving or investment" : kind_keyword(*kind));
    return ERR_CATEGORY;
}

static int ask_category(const Book *book, const CategoryKind *kind, const char *default_name, size_t *index)
{
    if (list_categories(book, kind) == 0)
    {
        return ERR_CATEGORY;
    }
    printf("\n");

    while (1)
    {
        char *input = NULL;
        int status = prompt_line("Category (number or name)", default_name, &input);
        if (status != 0)
        {
            return status;
        }

        status = resolve_category(book, kind, input, index);
        free(input);
        if (status == 0)
        {
            return 0;
        }
    }
}

static int ask_date(const Book *book, int day_default, int *day)
{
    char date[SIZE_PROMPT];

    if (day_default == 0)
    {
        printf("Today is not inside the open month (%04d-%02d), so please write the date.\n", book->year, book->month);
    }
    snprintf(date, sizeof(date), "%04d-%02d-%02d", book->year, book->month, day_default);

    while (1)
    {
        char *input = NULL;
        int status = prompt_line("Date (YYYY-MM-DD or the day of the month)", day_default > 0 ? date : NULL, &input);
        if (status != 0)
        {
            return status;
        }

        status = parse_day(input, book->year, book->month, day);
        free(input);
        if (status == 0)
        {
            return 0;
        }
    }
}

static const char *amount_prompt(EntryType type)
{
    switch (type)
    {
        case ENTRY_INVEST_VALUE:
            return "Enter what this investment is worth today (what you put in + what it has earned or lost), e.g. 1234.56";

        case ENTRY_SAVING_WITHDRAW:
        case ENTRY_INVEST_WITHDRAW:
            return "Amount withdrawn, e.g. 150.25 (it goes back to your cash; if you spent it, also record that expense)";

        default:
            return "Amount, e.g. 25000 or 25000.50";
    }
}

// asks every field of an entry that is being edited. The current values are the defaults, so pressing enter keeps them
static int ask_entry_fields(const Book *book, Entry *entry)
{
    Entry old = *entry;
    char amount[SIZE_MONEY];
    size_t index = 0;
    int status = 0;

    while (1)
    {
        char *input = NULL;
        status = prompt_line("Type (income, expense, debt, saving, saving-withdraw, invest, invest-withdraw, invest-value)",
                             type_keyword(old.type), &input);
        if (status != 0)
        {
            return status;
        }
        status = parse_type(input, &entry->type);
        if (status != 0)
        {
            invalid_type(input);
        }
        free(input);
        if (status == 0)
        {
            break;
        }
    }

    if (type_uses_category(entry->type))
    {
        CategoryKind kind = type_kind(entry->type);
        bool same_kind = type_uses_category(old.type) && type_kind(old.type) == kind;

        status = ask_category(book, &kind, same_kind ? old.category : NULL, &index);
        if (status != 0)
        {
            return status;
        }
        snprintf(entry->category, sizeof(entry->category), "%s", book->categories[index].name);
        entry->name[0] = '\0';
    }
    else
    {
        status = ask_name("Item name (e.g. food)", type_uses_category(old.type) ? NULL : old.name, entry->name);
        if (status != 0)
        {
            return status;
        }
        entry->category[0] = '\0';
    }

    amount_to_text(old.amount, amount, sizeof(amount));
    status = ask_amount(amount_prompt(entry->type), amount, entry->type == ENTRY_INVEST_VALUE, &entry->amount);
    if (status != 0)
    {
        return status;
    }

    return ask_date(book, old.day, &entry->day);
}

static bool has_value_update(const Book *book, const char *category)
{
    for (size_t i = 0; i < book->entry_count; i++)
    {
        if (book->entries[i].type == ENTRY_INVEST_VALUE && strcasecmp(book->entries[i].category, category) == 0)
        {
            return true;
        }
    }
    return false;
}

// after every investment entry: what the investment is worth now and what it has gained or lost so far this month
static int print_investment_result(const Book *book, const char *category)
{
    MonthSummary summary;
    int status = compute_summary(book, &summary);

    if (status == 0)
    {
        const CategoryTotals *totals = &summary.categories[find_category(book, category)];

        printf("  %s is now worth ", category);
        print_money(totals->closing, false);
        printf(", gain/loss so far this month: ");
        print_money(totals->gain, true);
        printf(" (");
        print_return(totals);
        printf(")\n");
        if (!has_value_update(book, category))
        {
            printf("  No value update this month yet, so the value only moved by what you put in or took out (no gain).\n"
                   "  Record what it is worth with a value update to get the real gain or loss.\n");
        }
    }
    free_summary(&summary);
    return status;
}

static int save_new_entry(Book *book, Entry *entry)
{
    int status = ledger_add_entry(book, entry);
    if (status == 0)
    {
        printf("Saved:\n");
        print_entry(book, entry);
        if (type_uses_category(entry->type) && type_kind(entry->type) == KIND_INVESTMENT)
        {
            status = print_investment_result(book, entry->category);
        }
    }
    return status;
}

// what each investment entry means, shown when the user picks one of them in the menu
static void explain_investments(EntryType type)
{
    printf("How investments work: you record the money you put in (contributions), the money you take out\n"
           "(withdrawals) and, from time to time, what the investment is WORTH (value updates). The program\n"
           "works out the gain or loss: closing value - opening value - contributions + withdrawals.\n"
           "The return %% weights every contribution and withdrawal by the days it was invested until the latest value\n"
           "update. It is an approximation: see its limitations in \"bookkeeping -h features\".\n");

    if (type == ENTRY_INVEST)
    {
        printf("A contribution is money moved from your cash into the investment: it is not a gain.\n");
    }
    else if (type == ENTRY_INVEST_WITHDRAW)
    {
        printf("A withdrawal moves money back to your cash: it is not a loss. If you spent it, also record that expense.\n");
    }
    else
    {
        printf("Write the TOTAL value your bank or broker shows today (what you put in + what it has earned or lost),\n"
               "not only what you put in. Only the latest value update of the month counts.\n");
    }
    printf("Example and details: bookkeeping -h features\n\n");
}

// asks the item name (or the category) of the next entry of the loop; "done", the default, ends the loop with ADD_DONE
static int ask_target(const Book *book, Entry *entry)
{
    CategoryKind kind = type_kind(entry->type);
    bool uses_category = type_uses_category(entry->type);
    size_t index = 0;

    // the list is shown again for every entry, so the numbers are always on the screen
    if (uses_category && list_categories(book, &kind) == 0)
    {
        return ERR_CATEGORY;
    }

    while (1)
    {
        char *input = NULL;
        int status = prompt_line(uses_category ? "Category (number or name)" : "Item name (e.g. food)", "done", &input);
        if (status != 0)
        {
            return status;
        }

        if (strcasecmp(input, "done") == 0)
        {
            status = ADD_DONE;
        }
        else if (uses_category)
        {
            status = resolve_category(book, &kind, input, &index);
            if (status == 0)
            {
                snprintf(entry->category, sizeof(entry->category), "%s", book->categories[index].name);
            }
        }
        else if (check_item(input))
        {
            snprintf(entry->name, sizeof(entry->name), "%s", input);
        }
        else
        {
            status = ERR_USAGE;
        }

        free(input);
        if (status == 0 || status == ADD_DONE)
        {
            return status;
        }
    }
}

/**
 * Records as many entries of one type as the user wants, each one saved as soon as it is complete,
 * so adding ten expenses doesn't mean going back through the menus ten times.
 * The first date defaults to today and every next one to the date of the previous entry (receipts of the same day).
 *
 * return 0 when the user answers "done", or the status that stopped the loop
 */
static int add_entries(Book *book, EntryType type)
{
    int day = default_day(book);

    print_title(type_label(type));
    if (type_uses_category(type) && type_kind(type) == KIND_INVESTMENT)
    {
        explain_investments(type);
    }
    printf("Add as many as you need, each one is saved at once.\n"
           "Press enter (or write \"done\") at the %s prompt when you are finished.\n",
           type_uses_category(type) ? "category" : "item name");

    while (1)
    {
        Entry entry = { 0 };
        entry.type = type;

        printf("\n");
        int status = ask_target(book, &entry);
        if (status == ADD_DONE)
        {
            return 0;
        }
        if (status == 0)
        {
            status = ask_amount(amount_prompt(type), NULL, type == ENTRY_INVEST_VALUE, &entry.amount);
        }
        if (status == 0)
        {
            status = ask_date(book, day, &entry.day);
        }
        if (status == 0)
        {
            day = entry.day;
            status = save_new_entry(book, &entry);
        }

        // a withdrawal over the balance was already explained and the loop goes on, anything else ends it
        if (status != 0 && status != ERR_BALANCE)
        {
            return status;
        }
    }
}

// the list of entry types: after the loop of one type the user comes back here, not to the main menu
static int menu_add(Book *book)
{
    while (1)
    {
        int option = 0;

        print_title("Add entries");
        printf("What do you want to record?\n");
        for (int i = 0; i < ENTRY_TYPE_COUNT; i++)
        {
            printf("  %d) %s%s\n", i + 1, type_label((EntryType)i),
                   i == ENTRY_INVEST_VALUE ? " (what it is worth today: this is how gains and losses are recorded)" : "");
        }
        printf("  %d) Back to the main menu\n\n", ENTRY_TYPE_COUNT + 1);

        int status = ask_option("Choose an option [1-9]", ENTRY_TYPE_COUNT + 1, &option);
        if (status != 0)
        {
            return status;
        }
        if (option == ENTRY_TYPE_COUNT + 1)
        {
            return 0;
        }

        status = add_entries(book, (EntryType)(option - 1));
        if (is_fatal(status))
        {
            return status;
        }
    }
}

static int edit_entry(Book *book, int id)
{
    int index = find_entry(book, id);
    if (index < 0)
    {
        entry_not_found(id);
        return ERR_ENTRY;
    }

    Entry updated = book->entries[index];
    printf("\nEditing (press enter to keep a value):\n");
    print_entry(book, &updated);
    printf("\n");

    int status = ask_entry_fields(book, &updated);
    if (status == 0)
    {
        status = ledger_replace_entry(book, (size_t)index, &updated);
    }
    if (status == 0)
    {
        printf("\nUpdated:\n");
        print_entry(book, &book->entries[index]);
    }
    return status;
}

static int delete_entry(Book *book, int id)
{
    bool yes = false;
    int index = find_entry(book, id);
    if (index < 0)
    {
        entry_not_found(id);
        return ERR_ENTRY;
    }

    printf("\n");
    print_entry(book, &book->entries[index]);
    printf("\n");
    int status = ask_choose("Delete this entry?", 'n', &yes);
    if (status != 0)
    {
        return status;
    }
    if (!yes)
    {
        printf("Nothing was deleted.\n");
        return 0;
    }

    status = ledger_delete_entry(book, (size_t)index);
    if (status == 0)
    {
        printf("Entry #%d deleted.\n", id);
    }
    return status;
}

static int ask_entry_id(const Book *book, int *id)
{
    int status = print_entries(book);
    if (status != 0 || book->entry_count == 0)
    {
        return status != 0 ? status : ERR_ENTRY;
    }

    printf("\n");
    while (1)
    {
        char *input = NULL;
        status = prompt_line("Entry ID", NULL, &input);
        if (status != 0)
        {
            return status;
        }
        if (parse_id(input, id))
        {
            free(input);
            return 0;
        }
        printf("You must write the ID of an entry (a positive number) and you wrote \"%s\", please try again:\n", input);
        free(input);
    }
}

static void opening_prompt(const Category *category, char *prompt, size_t size)
{
    if (category->kind == KIND_SAVING)
    {
        snprintf(prompt, size, "Balance of \"%s\" at the start of the open month, e.g. 1234.56", category->name);
    }
    else
    {
        snprintf(prompt, size, "What \"%s\" is worth at the start of the open month (what you put in + what it has earned or lost)",
                 category->name);
    }
}

static int menu_category_add(Book *book)
{
    Category category = { 0 };
    char prompt[SIZE_PROMPT];
    int option = 0;

    printf("\n  1) Saving category\n  2) Investment category\n\n");
    int status = ask_option("Kind [1-2]", 2, &option);
    if (status == 0)
    {
        category.kind = option == 1 ? KIND_SAVING : KIND_INVESTMENT;
        status = ask_name("Name of the new category", NULL, category.name);
    }
    if (status == 0)
    {
        opening_prompt(&category, prompt, sizeof(prompt));
        status = ask_amount(prompt, "0", true, &category.opening);
    }
    if (status == 0)
    {
        status = ledger_add_category(book, &category);
    }
    return status;
}

static int menu_category_change(Book *book, int option)
{
    char name[MAX_NAME_LEN + 1];
    char prompt[SIZE_PROMPT];
    char amount[SIZE_MONEY];
    size_t index = 0;
    bool yes = false;
    long long cents = 0;

    int status = ask_category(book, NULL, NULL, &index);
    if (status != 0)
    {
        return status;
    }

    switch (option)
    {
        case 2:
            status = ask_name("New name", book->categories[index].name, name);
            if (status == 0)
            {
                status = ledger_rename_category(book, index, name);
            }
            break;

        case 3:
            // the rules are checked before asking, so nobody confirms a delete that is then refused
            status = check_category_delete(book, index);
            if (status == 0)
            {
                snprintf(prompt, sizeof(prompt), "Delete the category \"%s\"?", book->categories[index].name);
                status = ask_choose(prompt, 'n', &yes);
            }
            if (status == 0 && yes)
            {
                status = ledger_delete_category(book, index);
            }
            break;

        default:
            opening_prompt(&book->categories[index], prompt, sizeof(prompt));
            amount_to_text(book->categories[index].opening, amount, sizeof(amount));
            status = ask_amount(prompt, amount, true, &cents);
            if (status == 0)
            {
                status = ledger_set_opening(book, index, cents);
            }
            break;
    }
    return status;
}

static int menu_opening_cash(Book *book)
{
    char amount[SIZE_MONEY];
    bool negative = false;
    long long cents = 0;

    printf("Opening cash: the money you had at the start of the open month besides savings and investments.\n");
    int status = ask_sign(&negative);
    if (status == 0)
    {
        amount_to_text(book->opening_cash < 0 ? -book->opening_cash : book->opening_cash, amount, sizeof(amount));
        status = ask_amount("Opening cash amount, e.g. 1234.56", amount, true, &cents);
    }
    if (status == 0)
    {
        status = ledger_set_opening_cash(book, negative ? -cents : cents);
    }
    return status;
}

static int menu_categories(Book *book)
{
    while (1)
    {
        int option = 0;

        print_title("Categories");
        int status = show_categories(book);
        if (status != 0)
        {
            return status;
        }

        printf("\n  1) Add a category\n  2) Rename a category\n  3) Delete a category\n"
               "  4) Fix the opening balance of a category\n  5) Fix the opening cash\n  6) Back to the main menu\n\n");
        status = ask_option("Choose an option [1-6]", CATEGORY_OPTIONS, &option);
        if (status == 0)
        {
            switch (option)
            {
                case 1:
                    status = menu_category_add(book);
                    break;

                case 5:
                    status = menu_opening_cash(book);
                    break;

                case 6:
                    return 0;

                default:
                    status = menu_category_change(book, option);
                    break;
            }
        }

        if (is_fatal(status))
        {
            return status;
        }
    }
}

// a month can be closed early (the user decides), but not before it has started
static int run_close(Book *book)
{
    char personal_report[SIZE_FILE_NAME];
    char key_drivers[SIZE_FILE_NAME];
    char question[SIZE_PROMPT];
    MonthSummary summary;
    bool yes = false;
    int year = 0;
    int month = 0;
    int day = 0;

    get_today(&year, &month, &day);
    if (book->year > year || (book->year == year && book->month > month))
    {
        month_not_started(book->year, book->month);
        return ERR_DATE;
    }

    int status = check_reports_free(book);
    if (status != 0)
    {
        return status;
    }

    status = compute_summary(book, &summary);
    if (status != 0)
    {
        free_summary(&summary);
        return status;
    }

    print_summary(book, &summary);
    if (book->year == year && book->month == month)
    {
        printf("\n%sNotice:%s the month is not over yet (today is day %d of %d). You can still close it now.\n",
               paint(COLOR_BOLD), paint(COLOR_RESET), day, days_in_month(year, month));
    }

    snprintf(question, sizeof(question), "Close %04d-%02d, write both reports and open the next month? Closed months can't be edited",
             book->year, book->month);
    build_report_paths(book, personal_report, key_drivers, SIZE_FILE_NAME);

    printf("\n");
    status = ask_choose(question, 'n', &yes);
    if (status == 0 && !yes)
    {
        printf("Nothing was changed.\n");
    }
    else if (status == 0)
    {
        status = close_month(book, &summary);
        if (status == 0)
        {
            printf("Both \"%s\" and \"%s\" have been created successfully.\nThe open month is now %04d-%02d.\n",
                   personal_report, key_drivers, book->year, book->month);
        }
    }

    free_summary(&summary);
    return status;
}

static int run_menu(Book *book)
{
    while (1)
    {
        MonthSummary summary;
        int option = 0;
        int id = 0;
        int status = compute_summary(book, &summary);
        if (status != 0)
        {
            free_summary(&summary);
            return status;
        }

        printf("\n\n%s%s\n Bookkeeping - %s %04d (open month)%s\n Cash: ", paint(COLOR_BOLD), RULER, month_name(book->month),
               book->year, paint(COLOR_RESET));
        print_money(summary.closing_cash, false);
        printf("   Incomes: ");
        print_money(summary.incomes, false);
        printf("   Expenses: ");
        print_money(summary.expenses, false);
        printf("\n%s%s%s\n", paint(COLOR_BOLD), RULER, paint(COLOR_RESET));
        free_summary(&summary);

        printf("  1) Add entries\n  2) View month so far\n  3) Edit entry\n  4) Delete entry\n"
               "  5) Categories\n  6) Close month\n  7) Help\n  8) Quit\n\n");
        status = ask_option("Choose an option [1-8]", MENU_OPTIONS, &option);
        if (status == ERR_EOF)
        {
            printf("\n");
            return 0; // the end of the input at the menu is the same as Quit, everything is already saved
        }
        if (status != 0)
        {
            return status;
        }

        switch (option)
        {
            case 1:
                status = menu_add(book);
                break;

            case 2:
                print_title("Month so far");
                status = show_summary(book);
                if (status == 0)
                {
                    printf("\n");
                    status = print_entries(book);
                }
                break;

            case 3:
                print_title("Edit entry");
                status = ask_entry_id(book, &id);
                if (status == 0)
                {
                    status = edit_entry(book, id);
                }
                break;

            case 4:
                print_title("Delete entry");
                status = ask_entry_id(book, &id);
                if (status == 0)
                {
                    status = delete_entry(book, id);
                }
                break;

            case 5:
                status = menu_categories(book);
                break;

            case 6:
                print_title("Close month");
                status = run_close(book);
                break;

            case 7:
                print_title("Help");
                print_help(NULL);
                break;

            default:
                return 0;
        }

        if (is_fatal(status))
        {
            return status;
        }
    }
}

// the categories of one kind for the setup wizard, in memory until the wizard ends. "done" (or enter) finishes
static int wizard_categories(Book *book, CategoryKind kind)
{
    char prompt[SIZE_PROMPT];

    if (kind == KIND_SAVING)
    {
        printf("\nNow your saving categories (e.g. an emergency fund), with their balance at the start of the month.\n\n");
    }
    else
    {
        printf("\nNow your investment categories (e.g. an index fund), with what each one is worth at the start of the month:\n"
               "its total value (what you put in + what it has earned or lost), as your bank or broker shows it.\n"
               "During the month you'll record contributions, withdrawals and value updates, and the program works out\n"
               "the gain or loss (see \"bookkeeping -h features\").\n\n");
    }

    while (1)
    {
        Category category = { 0 };
        char *input = NULL;

        category.kind = kind;
        snprintf(prompt, sizeof(prompt), "%s category name (or \"done\" to finish)", kind == KIND_SAVING ? "Saving" : "Investment");
        int status = prompt_line(prompt, "done", &input);
        if (status != 0)
        {
            return status;
        }

        if (strcasecmp(input, "done") == 0)
        {
            free(input);
            return 0;
        }

        if (!check_item(input) || find_category(book, input) >= 0)
        {
            if (find_category(book, input) >= 0)
            {
                category_exists(input);
            }
            free(input);
            continue;
        }

        snprintf(category.name, sizeof(category.name), "%s", input);
        free(input);

        opening_prompt(&category, prompt, sizeof(prompt));
        status = ask_amount(prompt, "0", true, &category.opening);
        if (status == 0)
        {
            status = book_push_category(book, &category);
        }
        if (status != 0)
        {
            return status;
        }
        printf("\n");
    }
}

// first run: nothing is written until every answer is in, so an interrupted wizard leaves no half setup behind
static int run_wizard(Book *book)
{
    char month[SIZE_PROMPT];
    bool has_cash = false;
    bool negative = false; // to know the sign of the opening cash, if necessary
    long long cash = 0;
    int year = 0;
    int number = 0;
    int day = 0;
    int status = 0;

    get_today(&year, &number, &day);
    snprintf(month, sizeof(month), "%04d-%02d", year, number);
    printf("%s=== Welcome to the setup for new users ===%s\nNo data was found in \"%s/data\", let's set things up.\n"
           "(Run \"bookkeeping -h\" any time to see how everything works.)\n\n", paint(COLOR_BOLD), paint(COLOR_RESET), book->dir);

    // a loop to ask for the month until the user writes the required input, aka "YYYY-MM" with a month between 1 and 12
    while (1)
    {
        char *input = NULL;
        status = prompt_line("First month to track (YYYY-MM)", month, &input);
        if (status != 0)
        {
            return status;
        }
        status = parse_month(input, &book->year, &book->month);
        free(input);
        if (status == 0)
        {
            break;
        }
    }

    printf("\nOpening cash: the money you have at the start of %s %04d besides savings and investments\n"
           "(e.g. your bank account and your wallet).\n\n", month_name(book->month), book->year);
    status = ask_choose("Do you have any opening cash?", 'y', &has_cash);
    if (status == 0 && has_cash)
    {
        status = ask_sign(&negative);
    }
    if (status == 0 && has_cash)
    {
        status = ask_amount("Opening cash amount, e.g. 1234.56", NULL, false, &cash);
    }
    if (status == 0)
    {
        book->opening_cash = negative ? -cash : cash;
        status = wizard_categories(book, KIND_SAVING);
    }
    if (status == 0)
    {
        status = wizard_categories(book, KIND_INVESTMENT);
    }
    if (status == 0)
    {
        status = save_categories(book);
    }
    if (status == 0)
    {
        status = save_state(book); // written last: without it the wizard runs again
    }
    if (status == 0)
    {
        printf("\nAll set, the open month is %04d-%02d.\n", book->year, book->month);
    }
    return status;
}

static int command_add(Book *book, int argc, char **argv)
{
    const char *positional[3] = { NULL, NULL, NULL };
    const char *date = NULL;
    const char *why = NULL;
    int count = 0;
    Entry entry = { 0 };

    for (int i = 0; i < argc; i++)
    {
        if (matches(argv[i], "--date|-d"))
        {
            if (i + 1 >= argc)
            {
                return usage("-d, --date needs a date (YYYY-MM-DD or the day of the month)");
            }
            date = argv[++i];
        }
        else if (strncmp(argv[i], "--date=", 7) == 0)
        {
            date = argv[i] + 7;
        }
        else if (count < 3)
        {
            positional[count++] = argv[i];
        }
        else
        {
            return usage("-a, --add takes <type> <item|category> <amount> [-d YYYY-MM-DD]");
        }
    }

    if (count != 3)
    {
        return usage("-a, --add takes <type> <item|category> <amount> [-d YYYY-MM-DD]");
    }

    if (parse_type(positional[0], &entry.type) != 0)
    {
        invalid_type(positional[0]);
        return usage("unknown entry type");
    }

    if (type_uses_category(entry.type))
    {
        int index = find_category(book, positional[1]);
        if (index < 0 || book->categories[index].kind != type_kind(entry.type))
        {
            unknown_category(positional[1], kind_keyword(type_kind(entry.type)));
            return ERR_CATEGORY;
        }
        snprintf(entry.category, sizeof(entry.category), "%s", book->categories[index].name);
    }
    else if (!check_item(positional[1]))
    {
        return usage("invalid item name");
    }
    else
    {
        snprintf(entry.name, sizeof(entry.name), "%s", positional[1]);
    }

    if (parse_amount(positional[2], &entry.amount, &why) != 0)
    {
        invalid_amount(positional[2], why);
        return ERR_USAGE;
    }
    if (entry.amount == 0 && entry.type != ENTRY_INVEST_VALUE)
    {
        amount_not_positive();
        return ERR_USAGE;
    }

    if (date != NULL)
    {
        int status = parse_day(date, book->year, book->month, &entry.day);
        if (status != 0)
        {
            return status;
        }
    }
    else if ((entry.day = default_day(book)) == 0)
    {
        no_default_date(book->year, book->month);
        return ERR_DATE;
    }

    return save_new_entry(book, &entry);
}

static int lookup_category(const Book *book, const char *name, size_t *index)
{
    int found = find_category(book, name);
    if (found < 0)
    {
        unknown_category(name, "saving or investment");
        return ERR_CATEGORY;
    }
    *index = (size_t)found;
    return 0;
}

static int parse_opening(const char *text, long long *cents)
{
    const char *why = NULL;
    if (parse_amount(text, cents, &why) != 0)
    {
        invalid_amount(text, why);
        return ERR_USAGE;
    }
    return 0;
}

static int command_categories(Book *book, int argc, char **argv)
{
    char prompt[SIZE_PROMPT];
    size_t index = 0;
    bool yes = false;
    int status = 0;

    if (argc == 0)
    {
        return show_categories(book);
    }

    if (matches(argv[0], "add|--add|-a") && (argc == 3 || argc == 4))
    {
        Category category = { 0 };
        if (parse_kind(argv[1], &category.kind) != 0)
        {
            return usage("the kind of a category is \"saving\" or \"investment\"");
        }
        if (strlen(argv[2]) > MAX_NAME_LEN)
        {
            check_item(argv[2]); // explains the limit
            return ERR_USAGE;
        }
        if (argc == 4 && parse_opening(argv[3], &category.opening) != 0)
        {
            return ERR_USAGE;
        }
        snprintf(category.name, sizeof(category.name), "%s", argv[2]);
        status = ledger_add_category(book, &category);
    }
    else if (matches(argv[0], "rename|--rename|-n") && argc == 3)
    {
        status = lookup_category(book, argv[1], &index);
        if (status == 0 && strlen(argv[2]) > MAX_NAME_LEN)
        {
            check_item(argv[2]);
            status = ERR_USAGE;
        }
        if (status == 0)
        {
            status = ledger_rename_category(book, index, argv[2]);
        }
    }
    else if (matches(argv[0], "delete|--delete|-r") && argc == 2)
    {
        status = lookup_category(book, argv[1], &index);
        if (status == 0)
        {
            status = check_category_delete(book, index);
        }
        if (status == 0)
        {
            snprintf(prompt, sizeof(prompt), "Delete the category \"%s\"?", book->categories[index].name);
            status = ask_choose(prompt, 'n', &yes);
        }
        if (status == 0 && yes)
        {
            status = ledger_delete_category(book, index);
        }
        else if (status == 0)
        {
            printf("Nothing was deleted.\n");
        }
    }
    else if (matches(argv[0], "set-opening|--set-opening|-o") && argc == 3)
    {
        long long cents = 0;
        status = lookup_category(book, argv[1], &index);
        if (status == 0)
        {
            status = parse_opening(argv[2], &cents);
        }
        if (status == 0)
        {
            status = ledger_set_opening(book, index, cents);
        }
    }
    else
    {
        return usage("unknown categories command or wrong number of arguments");
    }

    if (status == 0)
    {
        status = show_categories(book);
    }
    return status;
}

static int command_opening_cash(Book *book, const char *text)
{
    long long cents = 0;
    bool negative = text[0] == '-';

    int status = parse_opening(negative ? text + 1 : text, &cents);
    if (status == 0)
    {
        status = ledger_set_opening_cash(book, negative ? -cents : cents);
    }
    if (status == 0)
    {
        printf("The opening cash of %04d-%02d is now ", book->year, book->month);
        print_money(book->opening_cash, false);
        printf(".\n");
    }
    return status;
}

static int dispatch(Book *book, int argc, char **argv)
{
    const char *command = argv[1];
    int id = 0;

    if (matches(command, "add|--add|-a"))
    {
        return command_add(book, argc - 2, argv + 2);
    }
    if (matches(command, "categories|--categories|-c"))
    {
        return command_categories(book, argc - 2, argv + 2);
    }
    if (matches(command, "list|--list|-l") && argc == 2)
    {
        print_notice(book);
        return print_entries(book);
    }
    if (matches(command, "summary|--summary|-s") && argc == 2)
    {
        print_notice(book);
        return show_summary(book);
    }
    if (matches(command, "close|--close|-C") && argc == 2)
    {
        return run_close(book);
    }
    if (matches(command, "opening-cash|--opening-cash|-o") && argc == 3)
    {
        return command_opening_cash(book, argv[2]);
    }
    if ((matches(command, "edit|--edit|-e") || matches(command, "delete|--delete|-r")) && argc == 3)
    {
        if (!parse_id(argv[2], &id))
        {
            return usage("the ID of an entry is a positive number");
        }
        return matches(command, "edit|--edit|-e") ? edit_entry(book, id) : delete_entry(book, id);
    }
    return usage("unknown command or wrong number of arguments");
}

/**
 * Entry point of the program: without arguments it opens the interactive menu, otherwise it runs one quick command.
 * The first time (no data/state.csv) the setup wizard runs before anything else.
 *
 * return 0 or the ERR_* code that stopped the program
 */
int run_cli(int argc, char **argv)
{
    char dir[SIZE_DIR_NAME];
    Book book;
    int status = 0;

    memset(&book, 0, sizeof(book));

    // the help works before any setup, with an optional topic: flags or features
    if (argc >= 2 && matches(argv[1], "help|--help|-h"))
    {
        if (argc > 3 || print_help(argc == 3 ? argv[2] : NULL) != 0)
        {
            return usage("the help takes no topic, \"flags\" or \"features\"");
        }
        return 0;
    }

    status = get_data_dir(dir, sizeof(dir));
    if (status == 0)
    {
        status = ensure_dirs(dir);
    }
    if (status == 0 && !book_exists(dir))
    {
        snprintf(book.dir, sizeof(book.dir), "%s", dir);
        status = run_wizard(&book);
        free_book(&book);
    }
    if (status == 0)
    {
        status = load_book(dir, &book);
    }
    if (status == 0)
    {
        // the notice to close a finished month is shown when the menu starts and with -l / -s, the commands where the
        // user looks at the month; repeating it after every add would only be noise while catching up on a past month
        if (argc == 1)
        {
            print_notice(&book);
            status = run_menu(&book);
        }
        else
        {
            status = dispatch(&book, argc, argv);
        }
    }

    if (status == ERR_EOF)
    {
        input_ended();
    }
    free_book(&book);
    return status;
}
