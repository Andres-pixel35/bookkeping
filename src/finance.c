#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "../include/finance.h"
#include "../include/errors.h"
#include "../include/struct.h"
#include "../include/helpers.h"

#define FIRST_CAPACITY 16 // the dynamic arrays start with this size and double every time they get full

// the keyword is what the data files and the quick commands use, the label is what the user reads
static const char *TYPE_KEYWORDS[ENTRY_TYPE_COUNT] =
{
    "income", "expense", "debt", "saving", "saving-withdraw", "invest", "invest-withdraw", "invest-value"
};

static const char *TYPE_LABELS[ENTRY_TYPE_COUNT] =
{
    "Income", "Expense", "Debt", "Saving deposit", "Saving withdrawal",
    "Investment contribution", "Investment withdrawal", "Investment value update"
};

const char *type_keyword(EntryType type)
{
    return TYPE_KEYWORDS[type];
}

const char *type_label(EntryType type)
{
    return TYPE_LABELS[type];
}

int parse_type(const char *text, EntryType *type)
{
    for (int i = 0; i < ENTRY_TYPE_COUNT; i++)
    {
        if (strcasecmp(text, TYPE_KEYWORDS[i]) == 0)
        {
            *type = (EntryType)i;
            return 0;
        }
    }
    return ERR_USAGE;
}

// incomes, expenses and debts keep free-form item names, every other type belongs to one of the user's categories
bool type_uses_category(EntryType type)
{
    return type != ENTRY_INCOME && type != ENTRY_EXPENSE && type != ENTRY_DEBT;
}

CategoryKind type_kind(EntryType type)
{
    return (type == ENTRY_SAVING || type == ENTRY_SAVING_WITHDRAW) ? KIND_SAVING : KIND_INVESTMENT;
}

const char *kind_keyword(CategoryKind kind)
{
    return kind == KIND_SAVING ? "saving" : "investment";
}

int parse_kind(const char *text, CategoryKind *kind)
{
    if (strcasecmp(text, "saving") == 0)
    {
        *kind = KIND_SAVING;
        return 0;
    }
    if (strcasecmp(text, "investment") == 0)
    {
        *kind = KIND_INVESTMENT;
        return 0;
    }
    return ERR_USAGE;
}

// what an entry is about: the category for savings and investments, the item name for the rest
const char *entry_target(const Entry *entry)
{
    return type_uses_category(entry->type) ? entry->category : entry->name;
}

// category names are unique among both kinds and compared without caring about upper or lower case
int find_category(const Book *book, const char *name)
{
    for (size_t i = 0; i < book->category_count; i++)
    {
        if (strcasecmp(book->categories[i].name, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

int find_entry(const Book *book, int id)
{
    for (size_t i = 0; i < book->entry_count; i++)
    {
        if (book->entries[i].id == id)
        {
            return (int)i;
        }
    }
    return -1;
}

// the entries are kept in the order they were added and their IDs only grow, so the next ID is the last one plus one
int next_entry_id(const Book *book)
{
    return book->entry_count == 0 ? 1 : book->entries[book->entry_count - 1].id + 1;
}

// makes room for one more element in a dynamic array, doubling its capacity when it is full
static int grow(void **array, size_t *capacity, size_t count, size_t element_size)
{
    if (count < *capacity)
    {
        return 0;
    }

    size_t new_capacity = *capacity == 0 ? FIRST_CAPACITY : *capacity * 2;
    void *tmp = realloc(*array, new_capacity * element_size);
    if (tmp == NULL)
    {
        memory_error();
        return ERR_MEMORY;
    }

    *array = tmp;
    *capacity = new_capacity;
    return 0;
}

int book_push_entry(Book *book, const Entry *entry)
{
    void *array = book->entries;
    int status = grow(&array, &book->entry_capacity, book->entry_count, sizeof(Entry));
    book->entries = array;
    if (status != 0)
    {
        return status;
    }

    book->entries[book->entry_count++] = *entry;
    return 0;
}

int book_push_category(Book *book, const Category *category)
{
    void *array = book->categories;
    int status = grow(&array, &book->category_capacity, book->category_count, sizeof(Category));
    book->categories = array;
    if (status != 0)
    {
        return status;
    }

    book->categories[book->category_count++] = *category;
    return 0;
}

// removes the entry keeping the order of the rest, so the IDs are still sorted
void book_remove_entry(Book *book, size_t index)
{
    memmove(&book->entries[index], &book->entries[index + 1], (book->entry_count - index - 1) * sizeof(Entry));
    book->entry_count--;
}

// chronological order: by day, and inside the same day in the order the entries were recorded
static int compare_entries(const void *a, const void *b)
{
    const Entry *x = *(const Entry *const *)a;
    const Entry *y = *(const Entry *const *)b;

    if (x->day != y->day)
    {
        return x->day < y->day ? -1 : 1;
    }
    return (x->id > y->id) - (x->id < y->id);
}

// gives back an array (to free) of pointers to the entries in chronological order, without moving the entries themselves
int sort_entries(const Book *book, const Entry ***sorted)
{
    *sorted = NULL;
    if (book->entry_count == 0)
    {
        return 0;
    }

    const Entry **list = malloc(book->entry_count * sizeof(*list));
    if (list == NULL)
    {
        memory_error();
        return ERR_MEMORY;
    }

    for (size_t i = 0; i < book->entry_count; i++)
    {
        list[i] = &book->entries[i];
    }
    qsort(list, book->entry_count, sizeof(*list), compare_entries);

    *sorted = list;
    return 0;
}

/**
 * Computes every total of the open month in one chronological pass over the entries.
 * Each saving and investment category is followed day by day, so a withdrawal is checked against what the
 * category had at that point in time (a value update sets the value of an investment, later contributions and
 * withdrawals move it from there).
 *
 * param book The open month
 * param summary Receives the totals, summary->categories must be freed with free_summary
 * return 0, ERR_MEMORY, or ERR_BALANCE when a withdrawal goes over its balance (summary->violation and summary->available say where)
 */
int compute_summary(const Book *book, MonthSummary *summary)
{
    const Entry **sorted = NULL;
    int status = 0;

    memset(summary, 0, sizeof(*summary));
    summary->opening_cash = book->opening_cash;

    if (book->category_count > 0)
    {
        summary->categories = calloc(book->category_count, sizeof(CategoryTotals));
        if (summary->categories == NULL)
        {
            memory_error();
            return ERR_MEMORY;
        }
    }

    for (size_t i = 0; i < book->category_count; i++)
    {
        summary->categories[i].opening = book->categories[i].opening;
        summary->categories[i].closing = book->categories[i].opening;
    }

    status = sort_entries(book, &sorted);
    if (status != 0)
    {
        goto cleanup;
    }

    for (size_t i = 0; i < book->entry_count; i++)
    {
        const Entry *entry = sorted[i];
        CategoryTotals *totals = NULL;

        if (type_uses_category(entry->type))
        {
            totals = &summary->categories[find_category(book, entry->category)];
        }

        switch (entry->type)
        {
            case ENTRY_INCOME:
                summary->incomes += entry->amount;
                break;

            case ENTRY_EXPENSE:
                summary->expenses += entry->amount;
                break;

            case ENTRY_DEBT:
                summary->debts += entry->amount;
                break;

            case ENTRY_SAVING:
            case ENTRY_INVEST:
                totals->deposits += entry->amount;
                totals->closing += entry->amount;
                break;

            case ENTRY_SAVING_WITHDRAW:
            case ENTRY_INVEST_WITHDRAW:
                if (entry->amount > totals->closing)
                {
                    summary->violation = entry;
                    summary->available = totals->closing;
                    status = ERR_BALANCE;
                    goto cleanup;
                }
                totals->withdrawals += entry->amount;
                totals->closing -= entry->amount;
                break;

            case ENTRY_INVEST_VALUE:
                totals->closing = entry->amount;
                break;

            case ENTRY_TYPE_COUNT:
                break;
        }
    }

    for (size_t i = 0; i < book->category_count; i++)
    {
        CategoryTotals *totals = &summary->categories[i];

        if (book->categories[i].kind == KIND_SAVING)
        {
            summary->saving_deposits += totals->deposits;
            summary->saving_withdrawals += totals->withdrawals;
            summary->savings_opening += totals->opening;
            summary->savings_closing += totals->closing;
        }
        else
        {
            // whatever the value moved that was not money put in or taken out is what the market gave or took
            totals->gain = totals->closing - totals->opening - totals->deposits + totals->withdrawals;
            summary->invest_contributions += totals->deposits;
            summary->invest_withdrawals += totals->withdrawals;
            summary->invest_gain += totals->gain;
            summary->invest_opening += totals->opening;
            summary->invest_closing += totals->closing;
        }
    }

    summary->closing_cash = summary->opening_cash + summary->incomes - summary->expenses - summary->debts
                          - summary->saving_deposits + summary->saving_withdrawals
                          - summary->invest_contributions + summary->invest_withdrawals;
    summary->opening_net_worth = summary->opening_cash + summary->savings_opening + summary->invest_opening;
    summary->closing_net_worth = summary->closing_cash + summary->savings_closing + summary->invest_closing;

cleanup:
    free(sorted);
    if (status != 0 && status != ERR_BALANCE)
    {
        free_summary(summary);
    }
    return status;
}

void free_summary(MonthSummary *summary)
{
    free(summary->categories);
    summary->categories = NULL;
}

// in a chronological pass it checks out that no withdrawal takes more than the category had at that point,
// it's called after every change so a later withdrawal can't lose the money it relied on either
int check_balances(const Book *book)
{
    MonthSummary summary;
    int status = compute_summary(book, &summary);

    if (status == ERR_BALANCE)
    {
        char wanted[SIZE_MONEY];
        char available[SIZE_MONEY];
        format_money(summary.violation->amount, false, wanted, sizeof(wanted));
        format_money(summary.available, false, available, sizeof(available));
        insufficient_balance(summary.violation->category, wanted, available);
    }

    free_summary(&summary);
    return status;
}

// gain / (opening value + contributions), 0 when nothing was invested
double return_percentage(const CategoryTotals *totals)
{
    return calculate_percentage(totals->opening + totals->deposits, totals->gain);
}

// biggest value first, and for the same value alphabetical order so the ranking never depends on luck
static int compare_items(const void *a, const void *b)
{
    const Items *x = a;
    const Items *y = b;

    if (x->value != y->value)
    {
        return x->value > y->value ? -1 : 1;
    }
    return strcasecmp(x->name, y->name);
}

/**
 * Adds up the entries of one type by item name (without caring about upper or lower case, the first spelling is kept)
 * so "food" on the 2nd and "Food" on the 4th count as one item, then sorts them from the biggest to the smallest.
 * Each item also gets the percentage it represents of the whole type.
 *
 * param book The open month
 * param type ENTRY_INCOME, ENTRY_EXPENSE or ENTRY_DEBT
 * param items Receives a dynamic array that must be freed (NULL when there is nothing)
 * param count Receives the number of items
 * return 0 or ERR_MEMORY
 */
int aggregate_items(const Book *book, EntryType type, Items **items, size_t *count)
{
    void *list = NULL;
    size_t used = 0;
    size_t capacity = 0;
    long long total = 0;

    for (size_t i = 0; i < book->entry_count; i++)
    {
        const Entry *entry = &book->entries[i];
        size_t j = 0;

        if (entry->type != type)
        {
            continue;
        }

        total += entry->amount;
        Items *array = list;
        while (j < used && strcasecmp(array[j].name, entry->name) != 0)
        {
            j++;
        }

        if (j == used)
        {
            if (grow(&list, &capacity, used, sizeof(Items)) != 0)
            {
                free(list);
                return ERR_MEMORY;
            }
            array = list;
            snprintf(array[j].name, sizeof(array[j].name), "%s", entry->name);
            array[j].value = 0;
            used++;
        }
        array[j].value += entry->amount;
    }

    Items *array = list;
    for (size_t i = 0; i < used; i++)
    {
        array[i].percentage = calculate_percentage(total, array[i].value);
    }

    if (used > 0)
    {
        qsort(array, used, sizeof(Items), compare_items);
    }

    *items = array;
    *count = used;
    return 0;
}

// the new name must be valid and not used by any other category, and every entry of the open month follows the new name
int rename_category(Book *book, size_t index, const char *new_name)
{
    char old_name[MAX_NAME_LEN + 1];
    Category *category = &book->categories[index];

    if (!checK_item(new_name))
    {
        return ERR_USAGE;
    }

    int other = find_category(book, new_name);
    if (other >= 0 && (size_t)other != index)
    {
        category_exists(new_name);
        return ERR_CATEGORY;
    }

    snprintf(old_name, sizeof(old_name), "%s", category->name);
    for (size_t i = 0; i < book->entry_count; i++)
    {
        Entry *entry = &book->entries[i];
        if (type_uses_category(entry->type) && strcasecmp(entry->category, old_name) == 0)
        {
            snprintf(entry->category, sizeof(entry->category), "%s", new_name);
        }
    }

    snprintf(category->name, sizeof(category->name), "%s", new_name);
    return 0;
}

// a category can only go away when it holds nothing and nothing in the open month uses it, otherwise money would vanish from the books
int check_category_delete(const Book *book, size_t index)
{
    const Category *category = &book->categories[index];
    size_t uses = 0;

    if (category->opening != 0)
    {
        char balance[SIZE_MONEY];
        format_money(category->opening, false, balance, sizeof(balance));
        category_not_empty(category->name, balance);
        return ERR_CATEGORY;
    }

    for (size_t i = 0; i < book->entry_count; i++)
    {
        if (type_uses_category(book->entries[i].type) && strcasecmp(book->entries[i].category, category->name) == 0)
        {
            uses++;
        }
    }

    if (uses > 0)
    {
        category_in_use(category->name, uses);
        return ERR_CATEGORY;
    }

    return 0;
}

// the closing balances of this month are the opening balances of the next one, and the next month starts with no entries
void carry_forward(Book *book, const MonthSummary *summary)
{
    for (size_t i = 0; i < book->category_count; i++)
    {
        book->categories[i].opening = summary->categories[i].closing;
    }

    book->opening_cash = summary->closing_cash;
    book->entry_count = 0;

    if (book->month == 12) // after december comes january of the next year
    {
        book->month = 1;
        book->year = book->year + 1;
    }
    else
    {
        book->month = book->month + 1;
    }
}
