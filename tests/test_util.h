#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/struct.h"
#include "../include/finance.h"
#include "../include/ledger.h"

// a fresh data directory (with data/ and reports/) for one test
static inline void make_temp_dir(char *dir, size_t size)
{
    const char *base = getenv("TMPDIR");
    snprintf(dir, size, "%s/bookkeeping_test_XXXXXX", base != NULL ? base : "/tmp");
    assert(mkdtemp(dir) != NULL);
    assert(ensure_dirs(dir) == 0);
}

static inline void remove_dir(const char *dir)
{
    char command[SIZE_DIR_NAME + 16];
    snprintf(command, sizeof(command), "rm -rf '%s'", dir);
    assert(system(command) == 0);
}

// an empty open month 2026-10 with the given opening cash, stored in dir (which may be "" for in-memory tests)
static inline Book new_book(const char *dir, long long opening_cash)
{
    Book book;
    memset(&book, 0, sizeof(book));
    snprintf(book.dir, sizeof(book.dir), "%s", dir);
    book.year = 2026;
    book.month = 10;
    book.opening_cash = opening_cash;
    return book;
}

static inline void add_category(Book *book, CategoryKind kind, const char *name, long long opening)
{
    Category category = { kind, "", opening };
    snprintf(category.name, sizeof(category.name), "%s", name);
    assert(book_push_category(book, &category) == 0);
}

// an entry for the in-memory book: target is the category or the item name depending on the type
static inline Entry make_entry(EntryType type, const char *target, long long amount, int day)
{
    Entry entry;
    memset(&entry, 0, sizeof(entry));
    entry.type = type;
    entry.amount = amount;
    entry.day = day;
    snprintf(type_uses_category(type) ? entry.category : entry.name, MAX_NAME_LEN + 1, "%s", target);
    return entry;
}

// pushes the entry in memory only (no file), with the next ID
static inline void push(Book *book, Entry entry)
{
    entry.id = next_entry_id(book);
    assert(book_push_entry(book, &entry) == 0);
}

#endif
