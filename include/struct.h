#ifndef STRUCT_H
#define STRUCT_H

#include <stddef.h>

#define MAX_NAME_LEN 50 // the same limit check_item has always used for item names
#define SIZE_DIR_NAME 256 // the data directory, the file names are built on top of it

// every amount in the program is stored as integer cents (long long), so no floating point drift can ever change a balance

typedef struct Items
{
    char name[MAX_NAME_LEN + 1];
    long long value;
    double percentage;
}
Items;

typedef enum EntryType
{
    ENTRY_INCOME,
    ENTRY_EXPENSE,
    ENTRY_DEBT,
    ENTRY_SAVING,           // deposit into a saving category
    ENTRY_SAVING_WITHDRAW,  // money back from a saving category to cash
    ENTRY_INVEST,           // contribution into an investment category
    ENTRY_INVEST_WITHDRAW,  // money back from an investment category to cash
    ENTRY_INVEST_VALUE,     // total current market value of an investment category
    ENTRY_TYPE_COUNT
}
EntryType;

typedef enum CategoryKind
{
    KIND_SAVING,
    KIND_INVESTMENT
}
CategoryKind;

// one line of data/YYYY-MM.csv. Incomes, expenses and debts use name, the other types use category
typedef struct Entry
{
    int id;
    int day;
    EntryType type;
    char category[MAX_NAME_LEN + 1];
    char name[MAX_NAME_LEN + 1];
    long long amount;
}
Entry;

// opening is the balance (savings) or value (investments) at the start of the open month
typedef struct Category
{
    CategoryKind kind;
    char name[MAX_NAME_LEN + 1];
    long long opening;
}
Category;

// everything the program knows about the open month, loaded once from the data directory
typedef struct Book
{
    char dir[SIZE_DIR_NAME];
    int year;
    int month;
    long long opening_cash;
    Category *categories;
    size_t category_count;
    size_t category_capacity;
    Entry *entries;
    size_t entry_count;
    size_t entry_capacity;
}
Book;

typedef struct CategoryTotals
{
    long long opening;
    long long deposits;     // deposits or contributions
    long long withdrawals;
    long long closing;      // balance or value at the end of the month (so far)
    long long gain;         // investments only
}
CategoryTotals;

typedef struct MonthSummary
{
    long long opening_cash;
    long long incomes;
    long long expenses;
    long long debts;
    long long saving_deposits;
    long long saving_withdrawals;
    long long invest_contributions;
    long long invest_withdrawals;
    long long invest_gain;
    long long savings_opening;
    long long savings_closing;
    long long invest_opening;
    long long invest_closing;
    long long closing_cash;
    long long opening_net_worth;
    long long closing_net_worth;
    CategoryTotals *categories; // same order as Book.categories
    const Entry *violation;     // the first withdrawal that went over its balance, when compute_summary returns ERR_BALANCE
    long long available;        // what that category had at that point
}
MonthSummary;

#endif
