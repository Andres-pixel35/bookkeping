#ifndef REPORT_H
#define REPORT_H

#include <stddef.h>
#include "struct.h"

void build_report_paths(const Book *book, char *personal_report, char *key_drivers, size_t size);
int check_reports_free(const Book *book);
int write_reports(const Book *book, const MonthSummary *summary);
int close_month(Book *book, const MonthSummary *summary);

#endif
