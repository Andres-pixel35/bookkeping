#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "../include/helpers.h"
#include "../include/errors.h"
#include "../include/struct.h"

#define TEXT(x) #x
#define TEXT_OF(x) TEXT(x) // turns the value of a macro into a string, used to tell the user the maximum length

// stores dynamically the input from the user
// it returns 0 and the line in *out, ERR_EOF when the input ended before any character was read, or ERR_MEMORY
int read_input(char **out)
{
    char *buffer = NULL;
    size_t size = 0;
    size_t capacity = 10;
    int ch;

    *out = NULL;
    buffer = malloc(capacity);
    if (buffer == NULL)
    {
        memory_error();
        return ERR_MEMORY;
    }

    while ((ch = getchar()) != '\n' && ch != EOF)
    {
        if (size + 1 >= capacity)
        {
            capacity *= 2;

            char *tmp = realloc(buffer, capacity);
            if (tmp == NULL)
            {
                free(buffer);
                memory_error();
                return ERR_MEMORY;
            }
            buffer = tmp;
        }
        buffer[size++] = (char)ch;
    }

    // without this check an empty read at the end of the input looks exactly like the user pressing enter,
    // and that is what made every prompt loop forever after Ctrl-D
    if (ch == EOF && size == 0)
    {
        free(buffer);
        return ERR_EOF;
    }

    buffer[size] = '\0';
    *out = buffer;
    return 0;
}

// prints the prompt (and the default between brackets when there is one), then reads a line.
// if the user just presses enter the default is returned, so every caller gets the same behaviour
int prompt_line(const char *prompt, const char *default_value, char **out)
{
    if (default_value != NULL && default_value[0] != '\0')
    {
        printf("%s [%s]: ", prompt, default_value);
    }
    else
    {
        printf("%s: ", prompt);
    }
    fflush(stdout);

    int status = read_input(out);
    if (status != 0)
    {
        return status;
    }

    if ((*out)[0] == '\0' && default_value != NULL)
    {
        size_t len = strlen(default_value);
        char *copy = realloc(*out, len + 1);
        if (copy == NULL)
        {
            free(*out);
            *out = NULL;
            memory_error();
            return ERR_MEMORY;
        }
        memcpy(copy, default_value, len + 1);
        *out = copy;
    }

    return 0;
}

int check_choose(char *choose)
{
    size_t len = strlen(choose);
    if (len != 1)
    {
        return ERR_CHOOSE;
    }

    choose[0] = (char)tolower((unsigned char)choose[0]);

    if (choose[0] != 'y' && choose[0] != 'n')
    {
        return ERR_CHOOSE;
    }
    return 0;
}

// asks a y/n question until the answer is valid. default_answer ('y', 'n' or '\0' for none) is taken when the user only presses enter
int ask_choose(const char *question, char default_answer, bool *yes)
{
    char label[SIZE_FILE_NAME];
    const char *def = default_answer == 'y' ? "y" : (default_answer == 'n' ? "n" : NULL);

    snprintf(label, sizeof(label), "%s (y/n)", question);

    while (1)
    {
        char *choose = NULL;
        int status = prompt_line(label, def, &choose);
        if (status != 0)
        {
            return status; // only memory errors and the end of the input break
        }

        if (check_choose(choose) == ERR_CHOOSE)
        {
            wrong_chose(choose);
            free(choose);
            continue; // ask again
        }

        *yes = (choose[0] == 'y');
        free(choose);
        return 0; // valid answer
    }
}

// it checks the len of the item and return true if it's lower than 50, false otherwise
bool check_item(const char *buffer)
{
    size_t len = strlen(buffer);

    if (len < 1)
    {
        printf("You must at least write one character.\n");
        return false;
    }
    else if (len > MAX_NAME_LEN)
    {
        printf("You can not write an item with more than %d characters, and you write \"%zu\". Please try again.\n", MAX_NAME_LEN, len);
        return false;
    }
    else if (isspace((unsigned char)buffer[0]))
    {
        printf("You can not start the name of your item with a space or tab.\n");
        return false;
    }

    // a newline or any other control character would break the lines of the data files and the tables of the reports
    for (size_t i = 0; i < len; i++)
    {
        if (iscntrl((unsigned char)buffer[i]))
        {
            printf("You can not use control characters (like tabs or new lines) in the name of your item.\n");
            return false;
        }
    }

    return true;
}

bool check_month(int *variable)
{
    bool result = true;

    if (*variable <= 0)
    {
        printf("You must write a positive integer greater than 0 and you wrote %d, please try again:\n", *variable);
        result = false;
    }
    else if (*variable > 12)
    {
        printf("You must write the number of a month (1-12) and you wrote %d, please try again:\n", *variable);
        result = false;
    }

    return result;
}

// asks whether the opening cash is positive, it can be negative if the user started the month owing money
int ask_sign(bool *negative)
{
    bool positive = true;
    int status = ask_choose("Is it a positive amount?", 'y', &positive);
    if (status != 0)
    {
        return status;
    }

    *negative = !positive;
    return 0;
}

// it checks whether the input is a number, so it keep track of the dots. If it finds more than one dot, more than two decimals
// or anything that is not a digit it refuses the input and says why in *why.
// The value is built digit by digit as integer cents, so no floating point is ever involved.
int parse_amount(const char *text, long long *cents, const char **why)
{
    size_t len = strlen(text);
    long long whole = 0;
    long long fraction = 0;
    int decimals = 0;
    bool dot = false;
    bool digit = false;

    if (len > MAX_LEN_AMOUNT)
    {
        *why = "you can only enter a maximum of " TEXT_OF(MAX_LEN_AMOUNT) " characters"
               " (with this amount of money, I don't know why are you here. Thanks though)";
        return ERR_USAGE;
    }

    for (size_t i = 0; i < len; i++)
    {
        if (text[i] == '.')
        {
            if (dot)
            {
                *why = "no decimal number uses more than one dot";
                return ERR_USAGE;
            }
            dot = true;
        }
        else if (isdigit((unsigned char)text[i]))
        {
            digit = true;
            if (!dot)
            {
                whole = whole * 10 + (text[i] - '0');
            }
            else if (decimals == 2)
            {
                *why = "money only has two decimals (cents)";
                return ERR_USAGE;
            }
            else
            {
                fraction = fraction * 10 + (text[i] - '0');
                decimals++;
            }
        }
        else
        {
            *why = "you must enter only digits and at maximum one dot";
            return ERR_USAGE;
        }
    }

    if (!digit)
    {
        *why = "you must write at least one digit";
        return ERR_USAGE;
    }

    if (decimals == 1)
    {
        fraction = fraction * 10; // "123.4" means 40 cents, not 4
    }

    *cents = whole * 100 + fraction;
    return 0;
}

// keeps asking until the user enters a valid amount, and every re-prompt says what was wrong
int ask_amount(const char *prompt, const char *default_value, bool allow_zero, long long *cents)
{
    while (1)
    {
        char *buffer = NULL;
        const char *why = NULL;
        int status = prompt_line(prompt, default_value, &buffer);
        if (status != 0)
        {
            return status;
        }

        if (parse_amount(buffer, cents, &why) != 0)
        {
            invalid_amount(buffer, why);
            free(buffer);
            continue;
        }
        free(buffer);

        if (!allow_zero && *cents == 0)
        {
            amount_not_positive();
            continue;
        }

        return 0;
    }
}

// writes cents as "$4,538,466.26" ("-$..." for negative amounts, "+$..." for positive ones when show_plus is set)
// so every amount on the screen and in the reports looks the same
void format_money(long long cents, bool show_plus, char *out, size_t size)
{
    char digits[32];
    char grouped[48];
    size_t used = 0;
    // the subtraction in unsigned arithmetic avoids the overflow of -LLONG_MIN
    unsigned long long value = cents < 0 ? 0ULL - (unsigned long long)cents : (unsigned long long)cents;

    snprintf(digits, sizeof(digits), "%llu", value / 100);
    size_t len = strlen(digits);

    for (size_t i = 0; i < len; i++)
    {
        if (i > 0 && (len - i) % 3 == 0)
        {
            grouped[used++] = ',';
        }
        grouped[used++] = digits[i];
    }
    grouped[used] = '\0';

    const char *sign = cents < 0 ? "-" : ((show_plus && cents > 0) ? "+" : "");
    snprintf(out, size, "%s$%s.%02llu", sign, grouped, value % 100);
}

// the plain form of an amount ("1234.50"), the one parse_amount accepts, used as default answer in the prompts
void amount_to_text(long long cents, char *out, size_t size)
{
    unsigned long long value = cents < 0 ? 0ULL - (unsigned long long)cents : (unsigned long long)cents;
    snprintf(out, size, "%s%llu.%02llu", cents < 0 ? "-" : "", value / 100, value % 100);
}

double calculate_percentage(long long base_value, long long part_value)
{
    if (base_value == 0)
    {
        return 0;
    }

    double percentage = ((double)part_value / (double)base_value) * 100;

    return percentage;
}

int days_in_month(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;

    if (month == 2 && leap)
    {
        return 29;
    }
    return days[month - 1];
}

void get_today(int *year, int *month, int *day)
{
    time_t now = time(NULL);
    struct tm local;

    localtime_r(&now, &local);
    *year = local.tm_year + 1900;
    *month = local.tm_mon + 1;
    *day = local.tm_mday;
}

static bool all_digits(const char *text, size_t from, size_t to)
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

// accepts "YYYY-MM-DD" or only the day of the month ("DD"), and refuses any day outside the open month
// because closed months are never edited again
int parse_day(const char *text, int year, int month, int *day)
{
    size_t len = strlen(text);
    int y = year;
    int m = month;
    int d = 0;

    if (len >= 1 && len <= 2 && all_digits(text, 0, len))
    {
        d = atoi(text);
    }
    else if (len == 10 && text[4] == '-' && text[7] == '-' && all_digits(text, 0, 4) && all_digits(text, 5, 7) && all_digits(text, 8, 10))
    {
        y = atoi(text);       // atoi stops at the first '-'
        m = atoi(text + 5);
        d = atoi(text + 8);
    }
    else
    {
        invalid_date(text);
        return ERR_USAGE;
    }

    if (m < 1 || m > 12 || d < 1 || d > days_in_month(y, m))
    {
        invalid_date(text);
        return ERR_USAGE;
    }

    if (y != year || m != month)
    {
        date_outside_month(text, year, month);
        return ERR_DATE;
    }

    *day = d;
    return 0;
}

// "YYYY-MM", used by the setup wizard to know the first month to track
int parse_month(const char *text, int *year, int *month)
{
    size_t len = strlen(text);

    // I decided to check the size to ensure that it never overflows the atoi
    if (len != MAX_LEN_YEAR + 1 + MAX_LEN_MONTH || text[MAX_LEN_YEAR] != '-' || !all_digits(text, 0, MAX_LEN_YEAR)
        || !all_digits(text, MAX_LEN_YEAR + 1, len))
    {
        printf("You must write the month as \"YYYY-MM\" (e.g; 2026-10) and you wrote \"%s\", please try again:\n", text);
        return ERR_USAGE;
    }

    int y = atoi(text);
    int m = atoi(text + MAX_LEN_YEAR + 1);

    if (!check_month(&m))
    {
        return ERR_USAGE;
    }

    if (y == 0) // the program won't work with years Before the Common Era
    {
        err_year();
        return ERR_YEAR;
    }

    *year = y;
    *month = m;
    return 0;
}

const char *month_name(int month)
{
    static const char *NAMES[12] =
    {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return NAMES[month - 1];
}

// colors only make sense on a terminal, a file or a pipe would get the escape codes as garbage. NO_COLOR turns them off too
const char *paint(const char *code)
{
    static int enabled = -1;

    if (enabled < 0)
    {
        enabled = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
    }
    return enabled ? code : "";
}

// prints an amount on the terminal, green when it's positive and red when it's negative
void print_money(long long cents, bool show_plus)
{
    char money[SIZE_MONEY];
    format_money(cents, show_plus, money, sizeof(money));

    const char *code = cents > 0 ? COLOR_GREEN : (cents < 0 ? COLOR_RED : "");
    printf("%s%s%s", code[0] ? paint(code) : "", money, code[0] ? paint(COLOR_RESET) : "");
}
