#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include "struct.h"

extern const char *BASE_PATH;

bool match_wildcard(const char *pattern, const char *str);
int has_files_wildcard(const char *pattern);
char *read_input(void);
bool only_numbers(int *variable, int size);
bool check_month(int *variable);
int get_previous_month(int month);
int get_previous_year(int year);
int check_choose(char *choose);
char *ask_choose(void); 
bool checK_item(char *buffer);
double calculate_final_balance(double income, double expense, double debt, double saving, double investment);
double ask_sign(void);
void savings_sum(Savings *sv, PreviousSavings psv);
void investment_sum(Investment *iv, PreviousInvestment piv);
double calculate_percentage(double base_value, double part_value);
void loop_items_calculate_percentage(Items it[], int index, double base_value);

#endif