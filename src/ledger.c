#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/ledger.h"
#include "../include/errors.h"
#include "../include/finance.h"
#include "../include/helpers.h"

#define STATE_FILE "data/state.csv"
#define CATEGORIES_FILE "data/categories.csv"
#define STATE_HEADER "open_month,opening_cash_cents"
#define CATEGORIES_HEADER "kind,name,opening_balance_cents"
#define ENTRIES_HEADER "id,date,type,category,name,amount_cents"
#define LOAD_MISSING 2 // read_csv_file did not find the file, for the month file that only means nothing was recorded yet
#define SIZE_NUMBER 32

typedef int (*RecordHandler)(Book *book, const CsvRecord *record);

// fields containing a comma, a quote or a newline are quoted and their quotes doubled (RFC 4180),
// so a name like "food, snacks" survives the trip to the file and back
void csv_write_field(FILE *file, const char *field, char end)
{
    if (strpbrk(field, ",\"\n\r") == NULL)
    {
        fputs(field, file);
    }
    else
    {
        fputc('"', file);
        for (const char *c = field; *c != '\0'; c++)
        {
            if (*c == '"')
            {
                fputc('"', file);
            }
            fputc(*c, file);
        }
        fputc('"', file);
    }
    fputc(end, file);
}

/**
 * Reads one record (one line, or more when a quoted field has newlines inside) of a csv file.
 *
 * param file The file to read from
 * param record Receives the fields
 * return 0 when a record was read, CSV_END at the end of the file, ERR_FILE when the record is malformed
 *        (a quote that never closes, too many fields, or a field too long to come from this program)
 */
int csv_read_record(FILE *file, CsvRecord *record)
{
    int ch = getc(file);
    size_t len = 0;
    bool quoted = false; // inside a quoted field
    bool closed = false; // a quoted field just ended, only a separator may follow

    if (ch == EOF)
    {
        return CSV_END;
    }

    record->count = 0;
    while (1)
    {
        if (quoted)
        {
            if (ch == EOF)
            {
                return ERR_FILE;
            }
            if (ch == '"')
            {
                ch = getc(file);
                if (ch != '"') // a single quote closes the field, a doubled one is a quote inside it
                {
                    quoted = false;
                    closed = true;
                    continue; // ch is looked at again as the character after the field
                }
            }
        }
        else if (ch == ',' || ch == '\n' || ch == EOF)
        {
            if (record->count == CSV_MAX_FIELDS)
            {
                return ERR_FILE;
            }
            record->fields[record->count][len] = '\0';
            record->count++;
            if (ch != ',')
            {
                return 0;
            }
            len = 0;
            closed = false;
            ch = getc(file);
            continue;
        }
        else if (ch == '\r') // in case the file was edited on Windows
        {
            ch = getc(file);
            continue;
        }
        else if (closed)
        {
            return ERR_FILE;
        }
        else if (ch == '"')
        {
            if (len != 0)
            {
                return ERR_FILE;
            }
            quoted = true;
            ch = getc(file);
            continue;
        }

        if (record->count == CSV_MAX_FIELDS || len + 1 >= CSV_FIELD_SIZE)
        {
            return ERR_FILE;
        }
        record->fields[record->count][len++] = (char)ch;
        ch = getc(file);
    }
}

// the data directory is "./" unless BOOKKEEPING_DIR says otherwise
int get_data_dir(char *dir, size_t size)
{
    const char *env = getenv("BOOKKEEPING_DIR");
    if (env == NULL || env[0] == '\0')
    {
        env = ".";
    }

    if (strlen(env) >= size)
    {
        dic_not_open(env);
        return ERR_DIC;
    }

    snprintf(dir, size, "%s", env);
    return 0;
}

// creates data/ and reports/ inside the data directory when they don't exist yet. Only the owner can read them,
// they hold personal financial data
int ensure_dirs(const char *dir)
{
    static const char *SUBDIRS[] = { "data", "reports" };
    char path[SIZE_FILE_NAME];

    for (size_t i = 0; i < sizeof(SUBDIRS) / sizeof(SUBDIRS[0]); i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, SUBDIRS[i]);
        if (mkdir(path, 0700) != 0 && errno != EEXIST)
        {
            dic_not_open(path);
            return ERR_DIC;
        }
    }
    return 0;
}

void build_path(const Book *book, const char *relative, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", book->dir, relative);
}

static void build_ledger_path(const Book *book, char *out, size_t size)
{
    snprintf(out, size, "%s/data/%04d-%02d.csv", book->dir, book->year, book->month);
}

// the setup wizard runs only when there is no data/state.csv yet
bool book_exists(const char *dir)
{
    char path[SIZE_FILE_NAME];
    snprintf(path, sizeof(path), "%s/%s", dir, STATE_FILE);
    return access(path, F_OK) == 0;
}

static bool parse_number(const char *text, long long *value)
{
    char *end = NULL;

    if (text[0] == '\0' || isspace((unsigned char)text[0]))
    {
        return false;
    }

    errno = 0;
    long long number = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0')
    {
        return false;
    }

    *value = number;
    return true;
}

// the same rules as check_item, but quiet: a bad name in a data file is reported once as a malformed file
static bool valid_name(const char *name)
{
    size_t len = strlen(name);

    if (len < 1 || len > MAX_NAME_LEN || isspace((unsigned char)name[0]))
    {
        return false;
    }
    for (size_t i = 0; i < len; i++)
    {
        if (iscntrl((unsigned char)name[i]))
        {
            return false;
        }
    }
    return true;
}

// valid_name already checked that the name fits in MAX_NAME_LEN characters
static void copy_name(char *dest, const char *name)
{
    memcpy(dest, name, strlen(name) + 1);
}

static bool digits_at(const char *text, size_t from, size_t to)
{
    for (size_t i = from; i < to; i++)
    {
        if (!isdigit((unsigned char)text[i]))
        {
            return false;
        }
    }
    return true;
}

static int state_record(Book *book, const CsvRecord *record)
{
    const char *month = record->fields[0];
    long long cash = 0;

    // only one record is allowed, and it must be "YYYY-MM,cents"
    if (book->month != 0 || record->count != 2 || strlen(month) != 7 || month[4] != '-'
        || !digits_at(month, 0, 4) || !digits_at(month, 5, 7) || !parse_number(record->fields[1], &cash))
    {
        return ERR_FILE;
    }

    int year = atoi(month);
    int number = atoi(month + 5);
    if (year < 1 || number < 1 || number > 12)
    {
        return ERR_FILE;
    }

    book->year = year;
    book->month = number;
    book->opening_cash = cash;
    return 0;
}

static int category_record(Book *book, const CsvRecord *record)
{
    Category category;

    if (record->count != 3 || parse_kind(record->fields[0], &category.kind) != 0 || !valid_name(record->fields[1])
        || find_category(book, record->fields[1]) >= 0 || !parse_number(record->fields[2], &category.opening) || category.opening < 0)
    {
        return ERR_FILE;
    }

    copy_name(category.name, record->fields[1]);
    return book_push_category(book, &category);
}

static int entry_record(Book *book, const CsvRecord *record)
{
    Entry entry = { 0 };
    long long id = 0;
    const char *date = record->fields[1];

    if (record->count != 6 || !parse_number(record->fields[0], &id) || id <= 0 || id > INT_MAX
        || id < next_entry_id(book) || parse_type(record->fields[2], &entry.type) != 0
        || !parse_number(record->fields[5], &entry.amount) || entry.amount < 0)
    {
        return ERR_FILE;
    }
    entry.id = (int)id;

    // the date must be a real day of the month this file belongs to
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-' || !digits_at(date, 0, 4) || !digits_at(date, 5, 7)
        || !digits_at(date, 8, 10) || atoi(date) != book->year || atoi(date + 5) != book->month)
    {
        return ERR_FILE;
    }
    entry.day = atoi(date + 8);
    if (entry.day < 1 || entry.day > days_in_month(book->year, book->month))
    {
        return ERR_FILE;
    }

    if (entry.type != ENTRY_INVEST_VALUE && entry.amount == 0)
    {
        return ERR_FILE;
    }

    if (type_uses_category(entry.type))
    {
        int index = find_category(book, record->fields[3]);
        if (index < 0 || book->categories[index].kind != type_kind(entry.type) || record->fields[4][0] != '\0')
        {
            return ERR_FILE;
        }
        snprintf(entry.category, sizeof(entry.category), "%s", book->categories[index].name);
    }
    else
    {
        if (record->fields[3][0] != '\0' || !valid_name(record->fields[4]))
        {
            return ERR_FILE;
        }
        copy_name(entry.name, record->fields[4]);
    }

    return book_push_entry(book, &entry);
}

/**
 * Reads a whole csv file: checks the header and gives every record to handler, so the three data files
 * share one reading (and one cleanup) path.
 *
 * return 0, LOAD_MISSING when the file does not exist, ERR_FILE (the reason is already printed) or ERR_MEMORY
 */
static int read_csv_file(const char *path, const char *header, RecordHandler handler, Book *book)
{
    CsvRecord record;
    char found[CSV_MAX_FIELDS * CSV_FIELD_SIZE] = "";
    int status = 0;

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        if (errno == ENOENT)
        {
            return LOAD_MISSING;
        }
        file_not_open(path);
        return ERR_FILE;
    }

    status = csv_read_record(file, &record);
    if (status == CSV_END)
    {
        if (book->month != 0)
        {
            file_empty(path, book->month, book->year);
        }
        else
        {
            missing_values(path);
        }
        status = ERR_FILE;
        goto cleanup;
    }

    for (size_t i = 0; status == 0 && i < record.count; i++)
    {
        if (i > 0)
        {
            strcat(found, ",");
        }
        strcat(found, record.fields[i]);
    }

    if (status != 0 || strcmp(found, header) != 0)
    {
        missing_values(path);
        status = ERR_FILE;
        goto cleanup;
    }

    while ((status = csv_read_record(file, &record)) == 0)
    {
        if (record.count == 1 && record.fields[0][0] == '\0')
        {
            continue; // a blank line, e.g. at the end of a file edited by hand
        }

        status = handler(book, &record);
        if (status != 0)
        {
            break;
        }
    }

    if (status == CSV_END)
    {
        status = 0;
    }
    else if (status == ERR_FILE) // if one value is missing it returns ERR_FILE and stops the program, it can't continue without it
    {
        missing_values(path);
    }

cleanup:
    fclose(file);
    return status;
}

/**
 * Loads the open month: data/state.csv, data/categories.csv and data/YYYY-MM.csv (which may not exist yet).
 * Everything the reports and the summaries need comes from these files, the .md reports are never read back.
 *
 * param dir The data directory
 * param book Receives the open month, it must be freed with free_book (also when this fails)
 * return 0, ERR_FILE or ERR_MEMORY
 */
int load_book(const char *dir, Book *book)
{
    char path[SIZE_FILE_NAME];
    int status = 0;

    memset(book, 0, sizeof(*book));
    snprintf(book->dir, sizeof(book->dir), "%s", dir);

    build_path(book, STATE_FILE, path, sizeof(path));
    status = read_csv_file(path, STATE_HEADER, state_record, book);
    if (status == 0 && book->month == 0)
    {
        missing_values(path); // only the header was there
        status = ERR_FILE;
    }
    if (status == LOAD_MISSING)
    {
        file_not_open(path);
        status = ERR_FILE;
    }
    if (status != 0)
    {
        return status;
    }

    build_path(book, CATEGORIES_FILE, path, sizeof(path));
    status = read_csv_file(path, CATEGORIES_HEADER, category_record, book);
    if (status == LOAD_MISSING)
    {
        previous_file_not_open(path, book->month, book->year);
        status = ERR_FILE;
    }
    if (status != 0)
    {
        return status;
    }

    build_ledger_path(book, path, sizeof(path));
    status = read_csv_file(path, ENTRIES_HEADER, entry_record, book);
    if (status == LOAD_MISSING)
    {
        status = 0; // nothing recorded in the open month yet
    }
    return status;
}

void free_book(Book *book)
{
    free(book->categories);
    free(book->entries);
    book->categories = NULL;
    book->entries = NULL;
    book->category_count = 0;
    book->category_capacity = 0;
    book->entry_count = 0;
    book->entry_capacity = 0;
}

/**
 * Writes a file through a temporary file in the same directory that is renamed over the real one at the end,
 * rename() replaces it in one step, so a crash leaves either the old file or the new one, never half of it.
 *
 * param path The final file
 * param writer Writes the content, returns 0 or an ERR_* code
 * param context Passed to writer
 * return 0 or ERR_FILE
 */
int write_file_atomic(const char *path, int (*writer)(FILE *, const void *), const void *context)
{
    char tmp[SIZE_FILE_NAME + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *file = fopen(tmp, "w");
    if (file == NULL)
    {
        file_not_open(tmp);
        return ERR_FILE;
    }

    // it's necessary to check that the writing worked: ferror says whether any of the fprintf calls failed
    int status = writer(file, context);
    if (status == 0 && (fflush(file) != 0 || ferror(file) || fsync(fileno(file)) != 0))
    {
        written_error();
        status = ERR_FILE;
    }

    if (fclose(file) == EOF && status == 0)
    {
        close_file_error(tmp);
        status = ERR_FILE;
    }

    if (status == 0 && rename(tmp, path) != 0)
    {
        written_error();
        status = ERR_FILE;
    }

    if (status != 0)
    {
        remove(tmp);
    }
    return status;
}

static void write_entry_record(FILE *file, const Book *book, const Entry *entry)
{
    char id[SIZE_NUMBER];
    char date[SIZE_NUMBER];
    char amount[SIZE_NUMBER];

    snprintf(id, sizeof(id), "%d", entry->id);
    snprintf(date, sizeof(date), "%04d-%02d-%02d", book->year, book->month, entry->day);
    snprintf(amount, sizeof(amount), "%lld", entry->amount);

    csv_write_field(file, id, ',');
    csv_write_field(file, date, ',');
    csv_write_field(file, type_keyword(entry->type), ',');
    csv_write_field(file, entry->category, ',');
    csv_write_field(file, entry->name, ',');
    csv_write_field(file, amount, '\n');
}

static int write_entries_file(FILE *file, const void *context)
{
    const Book *book = context;

    fputs(ENTRIES_HEADER "\n", file);
    for (size_t i = 0; i < book->entry_count; i++)
    {
        write_entry_record(file, book, &book->entries[i]);
    }
    return 0;
}

static int write_categories_file(FILE *file, const void *context)
{
    const Book *book = context;
    char amount[SIZE_NUMBER];

    fputs(CATEGORIES_HEADER "\n", file);
    for (size_t i = 0; i < book->category_count; i++)
    {
        snprintf(amount, sizeof(amount), "%lld", book->categories[i].opening);
        csv_write_field(file, kind_keyword(book->categories[i].kind), ',');
        csv_write_field(file, book->categories[i].name, ',');
        csv_write_field(file, amount, '\n');
    }
    return 0;
}

static int write_state_file(FILE *file, const void *context)
{
    const Book *book = context;

    fprintf(file, STATE_HEADER "\n%04d-%02d,%lld\n", book->year, book->month, book->opening_cash);
    return 0;
}

int save_entries(const Book *book)
{
    char path[SIZE_FILE_NAME];
    build_ledger_path(book, path, sizeof(path));
    return write_file_atomic(path, write_entries_file, book);
}

int save_categories(const Book *book)
{
    char path[SIZE_FILE_NAME];
    build_path(book, CATEGORIES_FILE, path, sizeof(path));
    return write_file_atomic(path, write_categories_file, book);
}

// state.csv is written last when a month is closed, it's the file that says which month is open
int save_state(const Book *book)
{
    char path[SIZE_FILE_NAME];
    build_path(book, STATE_FILE, path, sizeof(path));
    return write_file_atomic(path, write_state_file, book);
}

// a new entry is appended (with the header when the month file is new) and flushed right away, so nothing typed is lost
static int append_entry(const Book *book, const Entry *entry)
{
    char path[SIZE_FILE_NAME];
    int status = 0;

    build_ledger_path(book, path, sizeof(path));
    FILE *file = fopen(path, "a");
    if (file == NULL)
    {
        file_not_open(path);
        return ERR_FILE;
    }

    if (fseek(file, 0, SEEK_END) == 0 && ftell(file) == 0)
    {
        fputs(ENTRIES_HEADER "\n", file);
    }
    write_entry_record(file, book, entry);

    if (fflush(file) != 0 || ferror(file))
    {
        written_error();
        status = ERR_FILE;
    }

    if (fclose(file) == EOF && status == 0)
    {
        close_file_error(path);
        status = ERR_FILE;
    }
    return status;
}

// the entry only gets in when no saving or investment goes below zero because of it, entry->id receives the new ID
int ledger_add_entry(Book *book, Entry *entry)
{
    entry->id = next_entry_id(book);

    int status = book_push_entry(book, entry);
    if (status != 0)
    {
        return status;
    }

    status = check_balances(book);
    if (status == 0)
    {
        status = append_entry(book, entry);
    }

    if (status != 0)
    {
        book->entry_count--; // the open month stays exactly as it was
    }
    return status;
}

// edits keep the ID and the position of the entry, and the month file is rewritten safely through a temporary file
int ledger_replace_entry(Book *book, size_t index, const Entry *updated)
{
    Entry old = book->entries[index];

    book->entries[index] = *updated;
    book->entries[index].id = old.id;

    int status = check_balances(book);
    if (status == 0)
    {
        status = save_entries(book);
    }

    if (status != 0)
    {
        book->entries[index] = old;
    }
    return status;
}

// deleting a deposit that a later withdrawal relied on is refused like any withdrawal over the balance
int ledger_delete_entry(Book *book, size_t index)
{
    Entry removed = book->entries[index];

    book_remove_entry(book, index);

    int status = check_balances(book);
    if (status == 0)
    {
        status = save_entries(book);
    }

    if (status != 0) // put it back where it was, the array still has the room it had
    {
        memmove(&book->entries[index + 1], &book->entries[index], (book->entry_count - index) * sizeof(Entry));
        book->entries[index] = removed;
        book->entry_count++;
    }
    return status;
}

int ledger_add_category(Book *book, const Category *category)
{
    if (!check_item(category->name))
    {
        return ERR_USAGE;
    }

    if (find_category(book, category->name) >= 0)
    {
        category_exists(category->name);
        return ERR_CATEGORY;
    }

    int status = book_push_category(book, category);
    if (status != 0)
    {
        return status;
    }

    status = save_categories(book);
    if (status != 0)
    {
        book->category_count--;
    }
    return status;
}

// the entries are rewritten first, then the categories, both through temporary files
int ledger_rename_category(Book *book, size_t index, const char *new_name)
{
    char old_name[MAX_NAME_LEN + 1];
    snprintf(old_name, sizeof(old_name), "%s", book->categories[index].name);

    int status = rename_category(book, index, new_name);
    if (status != 0)
    {
        return status;
    }

    if (book->entry_count > 0)
    {
        status = save_entries(book);
    }
    if (status == 0)
    {
        status = save_categories(book);
    }

    if (status != 0)
    {
        rename_category(book, index, old_name);
    }
    return status;
}

int ledger_delete_category(Book *book, size_t index)
{
    int status = check_category_delete(book, index);
    if (status != 0)
    {
        return status;
    }

    Category removed = book->categories[index];
    memmove(&book->categories[index], &book->categories[index + 1], (book->category_count - index - 1) * sizeof(Category));
    book->category_count--;

    status = save_categories(book);
    if (status != 0)
    {
        memmove(&book->categories[index + 1], &book->categories[index], (book->category_count - index) * sizeof(Category));
        book->categories[index] = removed;
        book->category_count++;
    }
    return status;
}

// fixes the opening balance (or value) of a category, a lower one is refused if a withdrawal of the month relied on it
int ledger_set_opening(Book *book, size_t index, long long cents)
{
    long long old = book->categories[index].opening;

    book->categories[index].opening = cents;

    int status = check_balances(book);
    if (status == 0)
    {
        status = save_categories(book);
    }

    if (status != 0)
    {
        book->categories[index].opening = old;
    }
    return status;
}

// the opening cash can be negative, the user may start the month owing money
int ledger_set_opening_cash(Book *book, long long cents)
{
    long long old = book->opening_cash;

    book->opening_cash = cents;

    int status = save_state(book);
    if (status != 0)
    {
        book->opening_cash = old;
    }
    return status;
}
