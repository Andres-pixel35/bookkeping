#ifndef FINANCE_H
#define FINANCE_H

#include "struct.h"
#include "helpers.h"

int get_previous_savings(FILE *file, PreviousSavings *savings, double *previous_balance);
int get_previous_investments(FILE *file, PreviousInvestment *investment); 
double get_values_double(size_t size);
void build_file_name(char *file_name, size_t size ,int month, int year);
void build_personal_report(char *file_name, size_t size, int month, int year);
double update_savings(const char *message, double *value, double *total_value ,size_t size);
double write_items(FILE *file, const char *item, double value);
double write_headers(FILE *file, const char *header);
double write_personal_report(FILE *file, const char *name, double value, bool *header_written);
bool update_item(Items arr[], int *index, const char *name, double value, size_t MAX_ITEMS);
double sum_values_items(Items arr[], int index);
double get_previous_values(const char *message, double *value, double *total_value, size_t size);
double get_items(const char *message, Items arr[], Items top[], int *index, size_t MAX_ITEMS);
double write_loop_items(FILE *file, Items arr[], int index, bool *header_written);
void compare_items(Items top[], Items new[], int *index);
double get_financial_input(const char *message, double *value, double *total, size_t size);
double write_savings(FILE *file, Savings s);
double write_investments(FILE *file, Investment iv);
double write_investments_keydrivers(FILE *file, Investment iv, double total_investment, double percentage);
double write_saving_keydrivers(FILE *file, Savings sv, double total_saving, double percentage);
double loop_write_keydrivers_items(FILE *file, Items it[], int index);
double write_keydrivers_items(FILE *file, const char *item, double value, double percentage);

#endif