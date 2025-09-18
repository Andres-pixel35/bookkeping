#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "../include/finance.h"
#include "../include/errors.h"
#include "../include/struct.h"
#include "../include/helpers.h"

int get_previous_savings(FILE *file, PreviousSavings *savings, double *previous_balance) 
{
    char line[256];
    double value;
    int total; int travels; int purchase; int emergencies; int final_balance;
    
    // Go back to the beginning of the file.
    rewind(file);
    
    // Read line by line
    while (fgets(line, sizeof(line), file)) 
    {
        // Search "- Savings"
        if (strstr(line, "- Savings") != NULL) {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) // it ignores all characters until it finds '$', when it does it stores the float value at the variable &value
            {
                savings->previous_total_saving = value;
                total = 1;
            }
        }
        // Search "Travels"
        else if (strstr(line, "Travels") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                savings->previous_travels = value;
                travels = 1;
            }
        }
        // Search "Planned purchases"
        else if (strstr(line, "Planned purchases") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                savings->previous_purchase = value;
                purchase = 1;
            }
        }
        // Search "Emergencies"
        else if (strstr(line, "Emergencies") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                savings->previous_emergencies = value;
                emergencies = 1;
            }
        }

        else if (strstr(line, "- Final balance") != NULL) //so, this is not part of previous savings, but it is an information the program need for the
        {                                                 // previous month, and I'm not going to make another function for this value only.
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                *previous_balance = value;
                final_balance = 1;
            }
        }
    }

    // it's neccessary to check that all the values were found because if one of them are missing the program won't work as expected
    if (total != 1 || travels != 1 || purchase != 1 || emergencies != 1 || final_balance != 1) 
    {
        return ERR_FILE;
    }
}

// same function, but I wasn't able to figure out a way to generalize it so here we go
int get_previous_investments(FILE *file, PreviousInvestment *investment) 
{
    char line[256];
    double value;
    int total = 0; int real_estate = 0; int currencies = 0; int commodities = 0; int stocks = 0;
    
    rewind(file);
    
    while (fgets(line, sizeof(line), file)) 
    {
        if (strstr(line, "- Investment") != NULL) {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1)
            {
                investment->previous_total_investment = value;
                total = 1;
            }
        }

        else if (strstr(line, "Real estate") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                investment->previous_real_estate = value;
                real_estate = 1;
            }
        }

        else if (strstr(line, "Currencies") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                investment->previous_currencies = value;
                currencies = 1;
            }
        }

        else if (strstr(line, "Commodities") != NULL) 
        {
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                investment->previous_commodities = value;
                commodities = 1;
            }
        }

        else if (strstr(line, "Stocks") != NULL) 
        {                                                 
            if (sscanf(line, "%*[^$]$%lf", &value) == 1) 
            {
                investment->previous_stocks = value;
                stocks = 1;
            }
        }
    }

    if (total != 1 || real_estate != 1 || currencies != 1 || commodities != 1 || stocks != 1) 
    {
        return ERR_FILE;
    }
}

// it checks whether the input is a number and a float, so it keep track of the dots. It if find more than one dot it continues asking until a valid 
// input in entered.
double get_values_double(size_t size)
{
    while (1)
    {
        int dots = 0;
        bool valid = true;
        printf("\nYou may enter decimal numbers if you need to.\n");
        printf("Value: ");
        char *buffer = read_input();
        if (buffer == NULL)
        {
            memory_error();
            return ERR_MEMORY;
        }

        size_t len = strlen(buffer);
        if (len > size)
        {
            printf("You can only enter a maximun of %zu digits and you entered: %zu. Please try again\n"
                    "(With this amount of money, I don't know why are you here. Thanks though.)\n", size, len);

            free(buffer);
            continue;
        }

        for (int i = 0; i < len; i++)
        {
            if (buffer[i] == '.')
            {
                dots = dots + 1;
                
                if (dots > 1)
                {
                    printf("No decimal number uses more than one dot, expected input: \"??.??\". Input received \"%s\"\nPlease try again.\n", buffer);
                    valid = false;
                    break; // stops the for loop
                }
            }
            else if (!isdigit(buffer[i]))
            {
                printf("You must enter only digits and at maximum one dot, but you entered: \"%s\"\n", buffer);
                valid = false; 
                break;
            }
        }
        
        if (!valid)
        {
            free(buffer);
            continue;
        }
        
        double value = atof(buffer);
        free(buffer);
        return value;
    }
}

void build_file_name(char *file_name, size_t size ,int month, int year)
{
    snprintf(file_name, size, "%s%02d_%04d.txt", BASE_PATH, month, year);
}


void build_personal_report(char *file_name, size_t size, int month, int year)
{
    snprintf(file_name, size, "%s%02d_%04d_Personal_Report.txt", BASE_PATH, month, year);
}

// in a loop prints the message, calls get_values_double and check out that the value-spend entered is less or equal than the given value.
// it also update the *value and the *total_value.
double update_savings(const char *message, double *value, double *total_value ,size_t size)
{
    while (1)
    {
        printf("\n%s\n", message);
        double spend = get_values_double(size);

        if (spend == ERR_MEMORY)
        {
            return ERR_MEMORY;
        }
        else if (spend > *value)
        {
            printf("You haven't that amount of money in this item, you tried to withdraw \"$%.2f\", but you only have: \"$%.2f\"."
                    "Please try again. ", spend, *value);
        
            continue;
        }

        *value = *value - spend;
        *total_value = *total_value - spend;
        return spend;
    }
}

double get_previous_values(const char *message, double *value, double *total_value, size_t size)
{
    printf("\n%s\n", message);

    *value = get_values_double(size);
    if (*value == ERR_MEMORY)
    {
        return ERR_MEMORY;
    }

    *total_value = *total_value + *value;
    return 0;
}

double write_headers(FILE *file, const char *header)
{
    if (fprintf(file, "%s", header) < 0)
    {
        return ERR_FILE;
    }
}

// this function writes in the file the item with the corresponding value
double write_items(FILE *file, const char *item, double value)
{
    if (fprintf(file, "\n%s: $%.2f", item, value) < 0)
    {
        return ERR_FILE;
    }
    return 0;
}

// it checks header_written, if false it prints the header and checks that no error happens.
// if true then it will only prints the items.
double write_personal_report(FILE *file, const char *name, double value, bool *header_written)
{
    double check = 0;

    if (!*header_written)
    {
        check = write_headers(file, name);
        if (check == ERR_FILE)
        {
            written_error();
            return ERR_FILE;
        }
        *header_written = true;
    }
    else
    {
        check = write_items(file, name, value);
        if (check == ERR_FILE)
        {
            written_error();
            return ERR_FILE;
        }
    }

    return 0;
}

// this receives the array of items and fill them up with the necessary information, and checks that the number of item is lower than the maximum permited 
bool update_item(Items arr[], int *index, const char *name, double value, size_t MAX_ITEMS)
{
    if (*index == MAX_ITEMS)
    {
        add_item_error(MAX_ITEMS);
        return false;
    }

    strcpy(arr[*index].name, name);
    arr[*index].value = value;
    *index = *index + 1;
    return true;
}

double sum_values_items(Items arr[], int index)
{
    double final_value = 0;
    for (int i = 0; i < index; i++)
    {
        final_value = final_value + arr[i].value;
    }

    return final_value;
}

// it compares the value of the new item updated in get items with the top 3 items from the category within this func is called
// if bigger, it updates the value inside the top 3 and the name as well as the rest of the top if need it.
void compare_items(Items top[], Items new[], int *index)
{
    int real_index = *index - 1; 
    Items temp = new[real_index]; // new item

    for (int i = 0; i < 3; i++)
    {
        if (top[i].value < temp.value)
        {
            // move downwards
            for (int j = 2; j > i; j--)
            {
                top[j] = top[j - 1];
            }

            top[i] = temp;
            break;
        }
    }
}

double get_items(const char *message, Items arr[], Items top[], int *index, size_t MAX_ITEMS)
{
    while (1)
    {
        if (*index == MAX_ITEMS)
        {
            add_item_error(MAX_ITEMS);
            printf("You have reached the maximum number of items you can add for \"%s\".\n", message);
            return ERR_NUMBER_ITEMS;
        }
        printf("\n=== %s ===\n", message);
        printf("Please write \"exit\" in item when you want to stop adding more %s.\n", message);

        printf("Item: ");
        char *item = read_input();
        if (item == NULL)
        {
            memory_error();
            return ERR_MEMORY;
        }
        else if (!checK_item(item))
        {
            free(item);
            continue;
        }
        else if (strcasecmp(item, "exit") == 0)
        {
            free(item);
            return 0;
        }

        double value = get_values_double(15);
        if (value <= 0)
        {
            free(item);
            printf("You must write a value greater than 0 per each item.\n");
            continue;
        }

        if (!update_item(arr, index, item, value, MAX_ITEMS))
        {
            free(item);
            return ERR_NUMBER_ITEMS;
        }

        compare_items(top, arr, index);

        free(item);
        continue;
    }
}

double write_loop_items(FILE *file, Items arr[], int index, bool *header_written)
{
    double check = 0;
    for (int i = 0; i < index; i++)
    {
        check = write_personal_report(file, arr[i].name, arr[i].value, header_written);
        if (check == ERR_FILE)
        {
            return ERR_FILE;
        }
    }
}

// it ask for a value, check it and sum it with the total
double get_financial_input(const char *message, double *value, double *total, size_t size)
{
    printf("\n%s\n", message);
    *value = get_values_double(size);
    if (*value == ERR_MEMORY)
    {
        return ERR_MEMORY;
    }

    *total = *total + *value;
    return 0;
}

double write_savings(FILE *file, Savings s)
{
    double check = 0;
    check = write_items(file, "\n- Savings", s.total_saving);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Travels", s.travels);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Planned purchases", s.purchase);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Emergencies", s.emergencies);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    return 0;
}

double write_investments(FILE *file, Investment iv)
{
    double check = 0;

    check = write_items(file, "\n- Investment", iv.investment);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Real estate", iv.real_estate);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Currencies (forex)", iv.currencies);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Commodities", iv.commodities);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Stocks", iv.stocks);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    return 0;
}

double write_keydrivers_items(FILE *file, const char *item, double value, double percentage)
{
    if (fprintf(file, "\n%s: $%.2f (%.2f%%)", item, value, percentage) < 0)
    {
        return ERR_FILE;
    }
    return 0;
}

double loop_write_keydrivers_items(FILE *file, Items it[], int index)
{
    double check = 0;
    if (index > 3)
    {
        index = 3;
    }

    for (int i = 0; i < index; i++)
    {
        check = write_keydrivers_items(file, it[i].name, it[i].value, it[i].percentage);
        if (check == ERR_FILE)
        {
            return ERR_FILE;
        }
    }

    return 0;
}

double write_saving_keydrivers(FILE *file, Savings sv, double total_saving, double percentage)
{
    double check = 0;

    if (fprintf(file, "\n\nThis month you saved $%.2f (%.2f%% out of your income this month) and the total of your savings is the following:",
         sv.total_saving, percentage) < 0)
    {
        return ERR_FILE;
    }

    check = write_items(file, "- Savings", total_saving);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Travels", sv.travels);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Planned purchases", sv.purchase);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Emergencies", sv.emergencies);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }
}

double write_investments_keydrivers(FILE *file, Investment iv, double total_investment, double percentage)
{
    double check = 0;

    if (fprintf(file, "\n\nThis month you invested $%.2f (%.2f%% out of your income this month) and the total of your investment is the following:",
         iv.investment, percentage) < 0)
    {
        return ERR_FILE;
    }

    check = write_items(file, "- Investment", total_investment);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Real estate", iv.real_estate);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Currencies", iv.currencies);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Commodities", iv.commodities);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }

    check = write_items(file, "Stocks", iv.stocks);
    if (check == ERR_FILE)
    {
        return ERR_FILE;
    }
}