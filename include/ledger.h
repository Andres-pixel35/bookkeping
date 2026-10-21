#ifndef LEDGER_H
#define LEDGER_H

#include <stdbool.h>
#include <stdio.h>
#include "struct.h"

#define CSV_MAX_FIELDS 6   // the entries have the most columns: id,date,type,category,name,amount_cents
#define CSV_FIELD_SIZE 64  // a name has at most 50 characters, a longer field means the file was not written by this program
#define CSV_END 1          // csv_read_record found the end of the file

typedef struct CsvRecord
{
    char fields[CSV_MAX_FIELDS][CSV_FIELD_SIZE];
    size_t count;
}
CsvRecord;

void csv_write_field(FILE *file, const char *field, char end);
int csv_read_record(FILE *file, CsvRecord *record);
int get_data_dir(char *dir, size_t size);
int ensure_dirs(const char *dir);
void build_path(const Book *book, const char *relative, char *out, size_t size);
bool book_exists(const char *dir);
int load_book(const char *dir, Book *book);
void free_book(Book *book);
int write_file_atomic(const char *path, int (*writer)(FILE *, const void *), const void *context);
int save_entries(const Book *book);
int save_categories(const Book *book);
int save_state(const Book *book);
int ledger_add_entry(Book *book, Entry *entry);
int ledger_replace_entry(Book *book, size_t index, const Entry *updated);
int ledger_delete_entry(Book *book, size_t index);
int ledger_add_category(Book *book, const Category *category);
int ledger_rename_category(Book *book, size_t index, const char *new_name);
int ledger_delete_category(Book *book, size_t index);
int ledger_set_opening(Book *book, size_t index, long long cents);
int ledger_set_opening_cash(Book *book, long long cents);

#endif
