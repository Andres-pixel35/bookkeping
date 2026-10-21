#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include "struct.h"

#define TOP_SIZE 3
#define MAX_LEN_MONTH 2   // this is to check the length of the input, if it has more than 2 characters it's obviously not a month
#define MAX_LEN_YEAR 4    // here it's the same, and it also works to avoid overflow the atoi
#define MAX_LEN_AMOUNT 15 // if you realy have this amount of money, I don't know what are you doing here
#define SIZE_FILE_NAME 512 // since it also includes the path, I decided to store enough space just in case
#define SIZE_MONEY 40     // "-$999,999,999,999,999.99" fits with room to spare

#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RESET "\033[0m"

int read_input(char **out);
int prompt_line(const char *prompt, const char *default_value, char **out);
int check_choose(char *choose);
int ask_choose(const char *question, char default_answer, bool *yes);
int ask_sign(bool *negative);
int ask_amount(const char *prompt, const char *default_value, bool allow_zero, long long *cents);
bool checK_item(const char *buffer);
bool check_month(int *variable);
int parse_amount(const char *text, long long *cents, const char **why);
void format_money(long long cents, bool show_plus, char *out, size_t size);
void amount_to_text(long long cents, char *out, size_t size);
double calculate_percentage(long long base_value, long long part_value);
int days_in_month(int year, int month);
void get_today(int *year, int *month, int *day);
int parse_day(const char *text, int year, int month, int *day);
int parse_month(const char *text, int *year, int *month);
const char *month_name(int month);
const char *paint(const char *code);
void print_money(long long cents, bool show_plus);

#endif
