#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../include/helpers.h"
#include "../include/struct.h"
#include "../include/errors.h"
#include "../include/finance.h"

#define FILE_FOUND 2
#define FILE_NOT_FOUND 1
#define MAX_LEN_MONTH 2   // this is to check the length of the input, if it has more than 2 characters it's obviously not a month
#define MAX_LEN_YEAR 4    // here it's the same, and it also works to avoid overflow the atoi in helpers.c
#define SIZE_FILE_NAME 512 // since it also includes the path, I decided to store enough space just in case
#define SIZE_PERSONAL_REPORT_FILE 512 // the same
#define FIRST_MONTH 1     // i need to check if the current month is 12, if so then I have to set it to 12 and reduce the year by 1
#define MAX_LEN_DOUBLE 15 // if you realy have this amount of money, I don't know what are you doing here
#define MAX_ITEMS 100 // the max number of items the user would be able to enter
#define TOP_SIZE 3
#define SAVINGS_WITHDRAWS_SIZE 3

// Sets is_valid = false and breaks out of the current block/loop.
// Use for generic error handling inside switch/case or loops.
#define HANDLE_MEMORY_ERRORS()\
        valid = false; \
        break;         

// Same as HANDLE_ERROR but also closes a file before breaking.
// Use when a file must be cleaned up on error.
#define HANDLE_FILE_MEMORY_ERRORS(file)\
        valid = false;          \
        fclose(file);           \
        break;  

// Similar to the previous one, but this one stops the program immediately
#define HANDLE_FILE_WRITTEN_ERRORS(file, choose)\
        fclose(file);                   \
        free(choose);                   \
        return ERR_FILE;

// main
int main(void)
{
    printf("=== BEFORE EXECUTING THIS PROGRAM, MAKE SURE YOU HAVE READ THE README FILE ===\n\n");
    // here i will define several local functions for main
    bool first_time_general = true;  
    int month = 0;                        
    int year = 0;
    double previous_balance = 0;
    double new_balance = 0;
    double income = 0;
    double expense = 0;
    double debt = 0;
    char key_drivers_name[SIZE_FILE_NAME];
    char personal_report_name[SIZE_PERSONAL_REPORT_FILE];
    PreviousInvestment previous_investment = {0};
    PreviousSavings previous_savings = {0};
    Investment new_investment = {0};
    Savings new_savings = {0};
    Items incomes[MAX_ITEMS] = { "", 0, 0 };
    Items expenses[MAX_ITEMS] = { "", 0, 0 };
    Items debts[MAX_ITEMS] = { "", 0, 0 };
    Items top_incomes[TOP_SIZE] = { { "", 0, 0 } };
    Items top_expenses[TOP_SIZE] = { { "", 0, 0 } };
    Items top_debts[TOP_SIZE] = { { "", 0, 0 } };

    int result = has_files_wildcard("??_????.txt");
    if (result == ERR_DIC)
    {
        dic_not_open();
        return ERR_DIC;
    }
    else if (result == FILE_FOUND)
    {
        first_time_general = false;
        printf("A file has been found, therefore the program will use the month before the one you add afterwards to make your bookkeping.\n");
    }
    else if (result == FILE_NOT_FOUND)
    {
        first_time_general = true;
        printf("No file has been found, therefore this program will work assuming this is your first time\n");
    }

    // a do while to ask for the month until the user writes the required input, aka a positive integer greater than 0 and smaller than 13
    do
    {
        do
        {
            printf("\nMonth (2 digits, e.g; 01-12): ");
        } while (!only_numbers(&month, MAX_LEN_MONTH));
    } while (!check_month(&month));

    do
    {
        printf("\nYear (4 digits, e.g; 2023): ");
    } while (!only_numbers(&year, MAX_LEN_YEAR));

    // The program will only search for the information in the previous month if it exists, aka it's there at least one file that fulfil the requirements
    if (!first_time_general)
    {
        int previous_month = get_previous_month(month);
        int previous_year = year;
        char previous_file_name[SIZE_FILE_NAME];

        if (month == FIRST_MONTH) // if the user enters january, then the year is set to the previous year
        {
            previous_year = get_previous_year(year);
            if (previous_year == ERR_YEAR)
            {
                // except if the current year is 0000, the program won't work with years Before the Common Era
                err_year();
                return ERR_YEAR;
            }
        }

        build_file_name(previous_file_name, SIZE_FILE_NAME, previous_month, previous_year);

        FILE *previous_bookkeping = fopen(previous_file_name, "r");
        if (!previous_bookkeping)
        {
            previous_file_not_open(previous_file_name, month, year);
            return ERR_FILE;
        }

        bool is_empty = (fgetc(previous_bookkeping) == EOF);

        if (is_empty)
        {
            file_empty(previous_file_name, month, year);
            fclose(previous_bookkeping);
            return ERR_FILE;
        }

        int is_modified = get_previous_savings(previous_bookkeping, &previous_savings, &previous_balance);

        if (is_modified == ERR_FILE) // if get previous savings does not find one value, it returns ERR_FILE and stops the program since it can't continue
        {                            // if those values are missing
            missing_values(previous_file_name);
            fclose(previous_bookkeping);
            return ERR_FILE;
        }

        is_modified = get_previous_investments(previous_bookkeping, &previous_investment);

        if (is_modified == ERR_FILE)
        {
            missing_values(previous_file_name);
            fclose(previous_bookkeping);
            return ERR_FILE;
        }

        if (fclose(previous_bookkeping) == EOF)
        {
            close_file_error(previous_file_name);
            return ERR_FILE;
        }

        // now I need to know if the user have spend some of the money of their savings in order to be able to reflect as accurate as possible the savings they have

        bool valid = true;
        double value = 0; // it gets the value entered for the user and then it's printed in personal report
        double check = 0; // it's neccessary to check the value fprintf is returning when called.
        bool header_written = false; // to only write once the header of savings withdraws
        Items savings_withdraws[SAVINGS_WITHDRAWS_SIZE]; 
        int index = 0;
        double final_value;

        printf("Have you spent some money from your savings? ");
        char *choose = ask_choose();
        if (choose == NULL)
        {
            return ERR_MEMORY;
        }

        switch (choose[0])
        {
        case 'y':
            build_personal_report(personal_report_name, SIZE_PERSONAL_REPORT_FILE, month, year);
            FILE *personal_report_file = fopen(personal_report_name, "a");
            if (!personal_report_file)
            {
                free(choose);
                file_not_open(personal_report_name);
                return ERR_FILE;
            }

            printf("\nPlease enter the amount of money that you have spent in each of the following items:");
            printf("\n(If you have changed nothing in any of them you may enter \"0\" or just press \"enter\").\n");

            value = update_savings("\n=== SAVINGS FOR FUTURE TRAVELS ===", 
                &previous_savings.previous_travels, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (value == ERR_MEMORY)
            {
                HANDLE_FILE_MEMORY_ERRORS(personal_report_file);
            }

            update_item(savings_withdraws, &index, "Travels", value, MAX_ITEMS);

            value = update_savings("\n=== SAVINGS FOR FUTURE PURCHASES ===", 
                &previous_savings.previous_purchase, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (value == ERR_MEMORY)
            {
                HANDLE_FILE_MEMORY_ERRORS(personal_report_file);
            }

            update_item(savings_withdraws, &index, "Planned purchases", value, MAX_ITEMS);

            value = update_savings("\n=== SAVINGS FOR EMERGENCIES ===",
                &previous_savings.previous_emergencies, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (value == ERR_MEMORY)
            {
                HANDLE_FILE_MEMORY_ERRORS(personal_report_file);
            }

            update_item(savings_withdraws, &index, "Emergencies", value, MAX_ITEMS);

            // this one is only to print the header.
            check = write_personal_report(personal_report_file, "Withdrawals from Savings\n", 0, &header_written);
            if (check == ERR_FILE)
            {
                HANDLE_FILE_WRITTEN_ERRORS(personal_report_file, choose);
            }

            final_value = sum_values_items(savings_withdraws, index);

            //prints firstly the value of the withdraws added
            check = write_personal_report(personal_report_file, "- Savings", final_value, &header_written);
            if (check == ERR_FILE)
            {
                HANDLE_FILE_WRITTEN_ERRORS(personal_report_file, choose);
            }

            // prints all the withdraws in order
            check = write_loop_items(personal_report_file, savings_withdraws, index, &header_written);
            if (check == ERR_FILE)
            {
                HANDLE_FILE_WRITTEN_ERRORS(personal_report_file, choose);
            }
            
            if (fclose(personal_report_file) == EOF)
            {
                close_file_error(personal_report_name);
                free(choose);
                return ERR_FILE;
            }
            break;
        
        case 'n':
            break;
        }
        free(choose);

        if (!valid)
        {
            return ERR_MEMORY;
        }
    }
    else
    {
        bool valid = true; // to control memory errors
        double sign; // to know the sign of the previous balance, if necessary

        printf("\n=== Welcome to the interface for new users ===\n\nDo you have any previous balance that you would like to add?\n"
               "Remeber that is money besides savings and investments and it's from the previous month ");
        char *choose = ask_choose();
        if (choose == NULL)
        {
            return ERR_MEMORY;
        }

        switch (choose[0])
        {
        case 'y':

            sign = ask_sign();
            if (sign == ERR_MEMORY)
            {
                free(choose);
                return ERR_MEMORY;
            }

            previous_balance = get_values_double(MAX_LEN_DOUBLE);
            if (previous_balance == ERR_MEMORY)
            {
                free(choose);
                return ERR_MEMORY;
            }

            if (sign == 1)
            {
                previous_balance = -previous_balance;
            }

            break;

        case 'n':
            break;
        }
        free(choose);

        printf("\nNice, now, do you have any previous savings that you would like to add?\n"
               "Remember, savings not including what you've added this month ");
        choose = ask_choose();
        if (choose == NULL)
        {
            return ERR_MEMORY;
        }

        switch (choose[0])
        {
        case 'y':
            printf("\nIf you have nothing in any of the following items you may enter \"0\" or just press \"enter\".\n");

            get_previous_values("=== SAVINGS FOR FUTURE TRAVELS ===", 
                &previous_savings.previous_travels, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (previous_savings.previous_travels == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            get_previous_values("=== SAVINGS FOR FUTURE PURCHASES ===", 
                &previous_savings.previous_purchase, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (previous_savings.previous_purchase == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            get_previous_values("=== SAVINGS FOR EMERGENCIES ===", 
                &previous_savings.previous_emergencies, &previous_savings.previous_total_saving, MAX_LEN_DOUBLE);
            if (previous_savings.previous_emergencies == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            break;

        case 'n':
            break;
        }
        free(choose);

        if (!valid)
        {
            return ERR_MEMORY;
        }

        printf("\nNice, now, do you have any previous investment that you would like to add?\n"
                "Remember, investment not including what you've added this month ");
        
        choose = ask_choose();
        if (choose == NULL)
        {
            return ERR_MEMORY;
        }

        switch (choose[0])
        {
            case 'y':
                printf("\nIf you have nothing in any of the following items you may enter \"0\" or just press \"enter\".\n");

                get_previous_values("=== REAL ESTATE INVESTMENT ===", 
                    &previous_investment.previous_real_estate, &previous_investment.previous_total_investment, MAX_LEN_DOUBLE);
                if (previous_investment.previous_real_estate == ERR_MEMORY)
                {
                    HANDLE_MEMORY_ERRORS();
                }

                get_previous_values("=== CURRENCIES INVESTMENT ===", 
                    &previous_investment.previous_currencies, &previous_investment.previous_total_investment, MAX_LEN_DOUBLE);
                if (previous_investment.previous_currencies == ERR_MEMORY)
                {
                    HANDLE_MEMORY_ERRORS();
                }

                get_previous_values("=== COMMODITIES INVESTMENT ===", 
                    &previous_investment.previous_commodities, &previous_investment.previous_total_investment, MAX_LEN_DOUBLE);
                if (previous_investment.previous_commodities == ERR_MEMORY)
                {
                    HANDLE_MEMORY_ERRORS();
                }

                get_previous_values("=== STOCKS ===", 
                    &previous_investment.previous_stocks, &previous_investment.previous_total_investment, MAX_LEN_DOUBLE);
                if (previous_investment.previous_stocks == ERR_MEMORY)
                {
                    HANDLE_MEMORY_ERRORS();
                }      

                break;
                
            case 'n':
                break;
        }
        free(choose);

        if (!valid)
        {
            return ERR_MEMORY;
        }
    }

    // now that the program has all the previous values, it is time to collect information from this month
    build_personal_report(personal_report_name, SIZE_PERSONAL_REPORT_FILE, month, year);
    FILE *personal_report_file = fopen(personal_report_name, "a");
    if (!personal_report_file)
    {
        file_not_open(personal_report_name);
        return ERR_FILE;
    }

    int index_incomes = 0;
    int index_expenses = 0;
    int index_debts = 0;
    double check = 0;
    bool header_written = false; 
    bool valid = true;

    printf("\nWelcome to the interface where you may add all the items of your incomes, expenses and debts you had this month.\n"
        "Let's begin with your incomes, please write afterwards all you incomes you had this month.\n"
        "(Remember add the name of the item and its value, do not forget that you can only add %d differents items per category).\n", MAX_ITEMS);

    check = get_items("Incomes", incomes, top_incomes, &index_incomes, MAX_ITEMS);
    if (check == ERR_MEMORY)
    {
        return ERR_MEMORY;
    }

    income = (sum_values_items(incomes, index_incomes) + previous_balance);

    check = get_items("Expenses", expenses, top_expenses, &index_expenses, MAX_ITEMS);
    if (check == ERR_MEMORY)
    {
        return ERR_MEMORY;
    }

    expense = sum_values_items(expenses, index_expenses);

    check = get_items("Debts", debts, top_debts, &index_debts, MAX_ITEMS);
    if (check == ERR_MEMORY)
    {
        return ERR_MEMORY;
    }

    debt = sum_values_items(debts, index_debts);

    printf("\nHave you saved any amount of money this month? ");
    char *choose = ask_choose();
    if (choose == NULL)
    {
        return ERR_MEMORY;
    }

    switch (choose[0])
    {
        case 'y':

            printf("\nIf you have saved nothing in any of the following items you may enter \"0\" or just press \"enter\".\n");

            check = get_financial_input("=== SAVINGS FOR FUTURE TRAVELS ===",
            &new_savings.travels, &new_savings.total_saving, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            check = get_financial_input("=== SAVINGS FOR FUTURE PURCHASES ===",
            &new_savings.purchase, &new_savings.total_saving, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            check = get_financial_input("=== SAVINGS EMERGENCIES ===",
            &new_savings.emergencies, &new_savings.total_saving, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }
            break;
        
        case 'n':
            break;
    }
    free(choose);

    if (!valid)
    {
        return ERR_MEMORY;
    }

    printf("Have you invested any amount of money this month? ");
    choose = ask_choose();
    if (choose == NULL)
    {
        return ERR_MEMORY;
    }

    switch (choose[0])
    {
        case 'y':
            printf("\nIf you have saved nothing in any of the following items you may enter \"0\" or just press \"enter\".\n");

            check = get_financial_input("=== REAL ESTATE INVESTMENT ===",
            &new_investment.real_estate, &new_investment.investment, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            check = get_financial_input("=== CURRENCIES INVESTMENT ===",
            &new_investment.currencies, &new_investment.investment, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            check = get_financial_input("=== COMMODITIES INVESTMENT ===",
            &new_investment.commodities, &new_investment.investment, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }

            check = get_financial_input("=== STOCKS ===",
            &new_investment.stocks, &new_investment.investment, MAX_LEN_DOUBLE);
            if (check == ERR_MEMORY)
            {
                HANDLE_MEMORY_ERRORS();
            }
            break;
        
        case 'n':
            break;
    }
    free(choose);

    if (!valid)
    {
        return ERR_MEMORY;
    }

    new_balance = calculate_final_balance(income, expense, debt, new_savings.total_saving, new_investment.investment);

    //after get all the values, it prints all the information into the file

    check = write_personal_report(personal_report_file, "\n\nPersonal Finance Report\n", 0, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_personal_report(personal_report_file, "- Total income", income, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_loop_items(personal_report_file, incomes, index_incomes, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(personal_report_file, "Previous balance", previous_balance);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_personal_report(personal_report_file, "\n- Total expenses", expense, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_loop_items(personal_report_file, expenses, index_expenses, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_personal_report(personal_report_file, "\n- Debts", debt, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_loop_items(personal_report_file, debts, index_debts, &header_written);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_savings(personal_report_file, new_savings);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_investments(personal_report_file, new_investment);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(personal_report_file, "\n- Final balance", new_balance);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    if (fclose(personal_report_file) == EOF)
    {
        close_file_error(personal_report_name);
        return ERR_FILE;
    }

    // from here forth everything is doing in order to write the keydrivers file.

    build_file_name(key_drivers_name, SIZE_FILE_NAME, month, year);
    FILE *key_drivers_file = fopen(key_drivers_name, "a");
    if (!key_drivers_file)
    {
        file_not_open(key_drivers_name);
        return ERR_FILE;
    }

    // calculate de percentage that each total represents of the income
    double new_savings_percentage = calculate_percentage(income, new_savings.total_saving);
    double new_investments_percentage = calculate_percentage(income, new_investment.investment);
    double expense_percentage = calculate_percentage(income, expense);
    double debt_percentage = calculate_percentage(income, debt);

    //update the value of saving addind to the new savings the previous one.
    savings_sum(&new_savings, previous_savings);
    investment_sum(&new_investment, previous_investment);
    double total_saving = new_savings.total_saving + previous_savings.previous_total_saving;
    double total_investment = new_investment.investment + previous_investment.previous_total_investment;

    // percentage of the money saved and investment this month
    double saving_percentage = calculate_percentage(income, new_savings.total_saving);
    double investment_percentage = calculate_percentage(income, new_investment.investment);

    // get the percentages for each top 3
    loop_items_calculate_percentage(top_incomes, index_incomes, income);
    loop_items_calculate_percentage(top_expenses, index_expenses, expense);
    loop_items_calculate_percentage(top_debts, index_debts, debt);

    // write all the information inside keydrivers

    check = write_headers(key_drivers_file, "Financial Overview & Key Drivers");
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(key_drivers_file, "\n- Total income", income);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = loop_write_keydrivers_items(key_drivers_file, top_incomes, index_incomes);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_keydrivers_items(key_drivers_file, "\n- Total expenses", expense, expense_percentage);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = loop_write_keydrivers_items(key_drivers_file, top_expenses, index_expenses);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_keydrivers_items(key_drivers_file, "\n- Debts", debt, debt_percentage);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = loop_write_keydrivers_items(key_drivers_file, top_debts, index_debts);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_saving_keydrivers(key_drivers_file, new_savings, total_saving, saving_percentage);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_investments_keydrivers(key_drivers_file, new_investment, total_investment, investment_percentage);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(key_drivers_file, "\n- Final balance", new_balance);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    if (fclose(key_drivers_file) == EOF)
    {
        close_file_error(key_drivers_name);
        return ERR_FILE;
    }

    printf("\nBoth \"%s\" and \"%s\" have been created successfully.\n", personal_report_name, key_drivers_name);
    return 0;
}