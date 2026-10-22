#ifndef ERRORS_H
#define ERRORS_H

#include <stddef.h>

#define ERR_MEMORY -51
#define ERR_FILE -52
#define ERR_DIC -53
#define ERR_YEAR -54
#define ERR_CHOOSE -55
#define ERR_NUMBER_ITEMS -56
#define ERR_EOF -57
#define ERR_USAGE -58
#define ERR_DATE -59
#define ERR_BALANCE -60
#define ERR_CATEGORY -61
#define ERR_REPORT_EXISTS -62
#define ERR_ENTRY -63

void dic_not_open(const char *dir_name);
void err_year(void);
void previous_file_not_open(const char *previous_file_name, int month, int year);
void file_empty(const char *previous_file_name, int month, int year);
void missing_values(const char *previous_file_name);
void wrong_chose(const char *choose);
void memory_error(void);
void file_not_open(const char *curren_file_name);
void written_error(void);
void close_file_error(const char *file_name);
void add_item_error(size_t MAX_ITEMS);
void input_ended(void);
void wrong_arguments(const char *why);
void wrong_option(const char *option, int max);
void invalid_amount(const char *input, const char *why);
void amount_not_positive(void);
void invalid_date(const char *input);
void date_outside_month(const char *input, int year, int month);
void no_default_date(int year, int month);
void month_not_started(int year, int month);
void invalid_type(const char *input);
void insufficient_balance(const char *category, const char *wanted, const char *date, const char *available);
void unknown_category(const char *name, const char *kind);
void category_exists(const char *name);
void category_not_empty(const char *name, const char *balance);
void category_in_use(const char *name, size_t entries);
void report_exists(const char *file_name);
void entry_not_found(int id);

#endif
