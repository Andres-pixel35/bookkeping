#ifndef FINANCE_H
#define FINANCE_H

#include <stdbool.h>
#include "struct.h"

const char *type_keyword(EntryType type);
const char *type_label(EntryType type);
int parse_type(const char *text, EntryType *type);
bool type_uses_category(EntryType type);
CategoryKind type_kind(EntryType type);
const char *kind_keyword(CategoryKind kind);
int parse_kind(const char *text, CategoryKind *kind);
const char *entry_target(const Entry *entry);
int find_category(const Book *book, const char *name);
int find_entry(const Book *book, int id);
int next_entry_id(const Book *book);
int book_push_entry(Book *book, const Entry *entry);
int book_push_category(Book *book, const Category *category);
void book_remove_entry(Book *book, size_t index);
int sort_entries(const Book *book, const Entry ***sorted);
int compute_summary(const Book *book, MonthSummary *summary);
void free_summary(MonthSummary *summary);
int check_balances(const Book *book);
double return_percentage(const CategoryTotals *totals);
int aggregate_items(const Book *book, EntryType type, Items **items, size_t *count);
int rename_category(Book *book, size_t index, const char *new_name);
int check_category_delete(const Book *book, size_t index);
void carry_forward(Book *book, const MonthSummary *summary);

#endif
