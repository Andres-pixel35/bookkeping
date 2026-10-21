#include "test_util.h"
#include "../include/errors.h"
#include "../include/helpers.h"

// the month of the e2e scenario, checked number by number against the formulas of the README
static void test_month_math(void)
{
    Book book = new_book("", 100000);
    MonthSummary s;

    add_category(&book, KIND_SAVING, "Emergency fund", 50000);
    add_category(&book, KIND_SAVING, "New laptop", 20000);
    add_category(&book, KIND_INVESTMENT, "Index fund", 100000);
    add_category(&book, KIND_INVESTMENT, "Crypto coins", 50000);
    add_category(&book, KIND_INVESTMENT, "Bonds", 30000);

    push(&book, make_entry(ENTRY_INCOME, "salary", 300000, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "rent", 90000, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "food", 12050, 2));
    push(&book, make_entry(ENTRY_EXPENSE, "Food", 7950, 4));
    push(&book, make_entry(ENTRY_DEBT, "credit card", 15000, 5));
    push(&book, make_entry(ENTRY_INCOME, "freelance", 40000, 3));
    push(&book, make_entry(ENTRY_SAVING, "Emergency fund", 30000, 2));
    push(&book, make_entry(ENTRY_SAVING_WITHDRAW, "New laptop", 15000, 4));
    push(&book, make_entry(ENTRY_INVEST, "Index fund", 20000, 3));
    push(&book, make_entry(ENTRY_INVEST_VALUE, "Index fund", 126000, 5));   // gain
    push(&book, make_entry(ENTRY_INVEST_VALUE, "Crypto coins", 40000, 4));  // loss
    push(&book, make_entry(ENTRY_INVEST_WITHDRAW, "Crypto coins", 5000, 5)); // after the value update
    push(&book, make_entry(ENTRY_INVEST, "Bonds", 10000, 6));                // no value update at all
    push(&book, make_entry(ENTRY_INVEST_WITHDRAW, "Bonds", 2000, 7));

    assert(compute_summary(&book, &s) == 0);
    assert(s.incomes == 340000 && s.expenses == 110000 && s.debts == 15000);
    assert(s.saving_deposits == 30000 && s.saving_withdrawals == 15000);
    assert(s.invest_contributions == 30000 && s.invest_withdrawals == 7000);

    // closing_cash = opening + incomes - expenses - debts - deposits + withdrawals - contributions + investment withdrawals
    assert(s.closing_cash == 100000 + 340000 - 110000 - 15000 - 30000 + 15000 - 30000 + 7000);

    assert(s.categories[0].closing == 80000 && s.categories[1].closing == 5000);
    assert(s.savings_opening == 70000 && s.savings_closing == 85000);

    // gain = closing - opening - contributions + withdrawals, return % = gain / (opening + contributions)
    assert(s.categories[2].closing == 126000 && s.categories[2].gain == 6000);
    assert(return_percentage(&s.categories[2]) > 4.999 && return_percentage(&s.categories[2]) < 5.001);
    assert(s.categories[3].closing == 35000 && s.categories[3].gain == -10000);
    assert(return_percentage(&s.categories[3]) < -19.999 && return_percentage(&s.categories[3]) > -20.001);
    assert(s.categories[4].closing == 38000 && s.categories[4].gain == 0); // opening + contributions - withdrawals
    assert(s.invest_gain == -4000);

    // the change of net worth is exactly what was earned, spent and gained
    assert(s.opening_net_worth == 100000 + 70000 + 180000);
    assert(s.closing_net_worth == s.closing_cash + 85000 + 126000 + 35000 + 38000);
    assert(s.closing_net_worth - s.opening_net_worth == s.incomes - s.expenses - s.debts + s.invest_gain);

    free_summary(&s);
    free_book(&book);
}

static void test_value_update_then_contribution(void)
{
    Book book = new_book("", 0);
    MonthSummary s;
    CategoryTotals empty = { 0, 0, 0, 0, 0 };

    add_category(&book, KIND_INVESTMENT, "Stocks", 100000);
    push(&book, make_entry(ENTRY_INVEST_VALUE, "Stocks", 90000, 10));
    push(&book, make_entry(ENTRY_INVEST, "Stocks", 5000, 12)); // after the update: added on top of it

    assert(compute_summary(&book, &s) == 0);
    assert(s.categories[0].closing == 95000);
    assert(s.categories[0].gain == -10000);
    assert(return_percentage(&empty) == 0); // nothing invested: 0%, never a division by zero
    free_summary(&s);
    free_book(&book);
}

// review bug 2: the ranking adds the entries up by name (case-insensitive) and keeps TOP_SIZE items
static void test_top_aggregation(void)
{
    Book book = new_book("", 0);
    Items *items = NULL;
    size_t count = 0;

    push(&book, make_entry(ENTRY_EXPENSE, "rent", 25000, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "food", 10000, 2));
    push(&book, make_entry(ENTRY_EXPENSE, "Food", 10000, 9));
    push(&book, make_entry(ENTRY_EXPENSE, "FOOD", 10000, 20));
    push(&book, make_entry(ENTRY_EXPENSE, "bus", 2000, 3));
    push(&book, make_entry(ENTRY_EXPENSE, "taxi", 3000, 3));
    push(&book, make_entry(ENTRY_EXPENSE, "gym", 3000, 4));
    push(&book, make_entry(ENTRY_INCOME, "salary", 100000, 1));

    assert(aggregate_items(&book, ENTRY_EXPENSE, &items, &count) == 0);
    assert(count == 5);
    assert(strcmp(items[0].name, "food") == 0 && items[0].value == 30000); // single entries would have put rent first
    assert(strcmp(items[1].name, "rent") == 0 && items[1].value == 25000);
    assert(strcmp(items[2].name, "gym") == 0 && strcmp(items[3].name, "taxi") == 0); // same value: alphabetical
    assert(items[0].percentage > 47.61 && items[0].percentage < 47.63);              // 30000 / 63000
    assert(TOP_SIZE == 3);
    free(items);

    assert(aggregate_items(&book, ENTRY_DEBT, &items, &count) == 0 && count == 0 && items == NULL);
    free_book(&book);
}

// a withdrawal can't take more than the category has at that point of the month
static void test_withdrawals(void)
{
    Book book = new_book("", 0);

    add_category(&book, KIND_SAVING, "Trip", 10000);
    add_category(&book, KIND_INVESTMENT, "Gold", 50000);

    push(&book, make_entry(ENTRY_SAVING_WITHDRAW, "Trip", 10000, 5)); // all of it: allowed
    assert(check_balances(&book) == 0);

    push(&book, make_entry(ENTRY_SAVING_WITHDRAW, "Trip", 1, 6)); // one cent too much
    assert(check_balances(&book) == ERR_BALANCE);
    book.entry_count--;

    push(&book, make_entry(ENTRY_SAVING, "Trip", 5000, 10));
    push(&book, make_entry(ENTRY_SAVING_WITHDRAW, "Trip", 5000, 8)); // the deposit comes later in the month
    assert(check_balances(&book) == ERR_BALANCE);
    book.entries[book.entry_count - 1].day = 11;
    assert(check_balances(&book) == 0);

    push(&book, make_entry(ENTRY_INVEST_VALUE, "Gold", 20000, 3));
    push(&book, make_entry(ENTRY_INVEST_WITHDRAW, "Gold", 30000, 4)); // more than its value after the update
    MonthSummary s;
    assert(compute_summary(&book, &s) == ERR_BALANCE);
    assert(s.violation != NULL && s.violation->amount == 30000 && s.available == 20000);
    free_summary(&s);

    free_book(&book);
}

static void test_category_rules(void)
{
    Book book = new_book("", 0);

    add_category(&book, KIND_SAVING, "Trip", 0);
    add_category(&book, KIND_SAVING, "Emergencies", 1000);
    add_category(&book, KIND_INVESTMENT, "Stocks", 0);
    push(&book, make_entry(ENTRY_SAVING, "Trip", 500, 1));
    push(&book, make_entry(ENTRY_EXPENSE, "Trip", 700, 2)); // an item with the same name is not the category

    assert(rename_category(&book, 0, "Holidays") == 0);
    assert(strcmp(book.categories[0].name, "Holidays") == 0);
    assert(strcmp(book.entries[0].category, "Holidays") == 0);
    assert(strcmp(book.entries[1].name, "Trip") == 0);
    assert(find_category(&book, "holidays") == 0 && find_category(&book, "Trip") == -1);

    assert(rename_category(&book, 0, "stocks") == ERR_CATEGORY);   // used by another category, whatever the case
    assert(rename_category(&book, 0, "HOLIDAYS") == 0);            // only the case changes
    assert(rename_category(&book, 0, "") == ERR_USAGE);
    assert(strcmp(book.categories[0].name, "HOLIDAYS") == 0);

    assert(check_category_delete(&book, 0) == ERR_CATEGORY); // it has entries
    assert(check_category_delete(&book, 1) == ERR_CATEGORY); // it holds money
    assert(check_category_delete(&book, 2) == 0);            // 0 and no entries

    free_book(&book);
}

static void test_carry_forward(void)
{
    Book book = new_book("", 100);
    MonthSummary s;

    book.month = 12;
    add_category(&book, KIND_SAVING, "Trip", 1000);
    push(&book, make_entry(ENTRY_SAVING, "Trip", 500, 31));
    push(&book, make_entry(ENTRY_INCOME, "salary", 1000, 1));

    assert(compute_summary(&book, &s) == 0);
    carry_forward(&book, &s);
    assert(book.year == 2027 && book.month == 1); // after december comes january of the next year
    assert(book.opening_cash == 600 && book.categories[0].opening == 1500 && book.entry_count == 0);
    assert(next_entry_id(&book) == 1);
    free_summary(&s);
    free_book(&book);
}

int main(void)
{
    test_month_math();
    test_value_update_then_contribution();
    test_top_aggregation();
    test_withdrawals();
    test_category_rules();
    test_carry_forward();
    printf("test_finance: all tests passed\n");
    return 0;
}
