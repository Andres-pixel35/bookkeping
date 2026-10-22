#include <stdio.h>
#include <string.h>

#include "../include/help.h"
#include "../include/errors.h"
#include "../include/helpers.h"

// The help is split in lines because ISO C only promises string literals of 4095 characters.
// A line starting with '#' is a section title (bold) and one starting with '>' is a command (green); on a terminal
// that is what makes them easy to find, and everywhere else the blank lines and the indentation do the same job.

static const char *INTRO[] =
{
    "Bookkeeping: record your money day by day and close each month into two Markdown reports.\n",
    "Run it without arguments for the interactive menu, or give it one of the commands below.\n",
    "\n",
    "Help pages: -h (everything), -h flags (only commands and flags), -h features (only how things work).\n",
};

static const char *FLAGS[] =
{
    "\n",
    "#COMMANDS AND FLAGS\n",
    "Every command has a short form (-a), a long form (--add) and a word form (add): use whichever you like.\n",
    "Names with spaces go between quotes, e.g. -a expense \"food, snacks\" 12.50\n",
    "\n",
    ">  -a, --add, add <type> <item|category> <amount> [-d, --date <date>]\n",
    "      Record an entry. <type> is one of:\n",
    "        income, expense, debt <item>   free-form item name\n",
    "        saving <category>              deposit: cash -> saving category\n",
    "        saving-withdraw <category>     withdrawal: saving category -> cash\n",
    "        invest <category>              contribution: cash -> investment\n",
    "        invest-withdraw <category>     withdrawal: investment -> cash\n",
    "        invest-value <category>        what the investment is worth today: this is how gains and losses\n",
    "                                       are recorded (see INVESTMENTS in -h features)\n",
    "      -d, --date: YYYY-MM-DD or only the day of the month (-d 14, --date 2026-10-14, --date=14).\n",
    "      Without it the date is today, which works only when today is inside the open month.\n",
    "      Example: bookkeeping -a expense food 25.50 -d 14\n",
    "\n",
    ">  -l, --list, list\n",
    "      Every entry of the open month, day by day, with its ID.\n",
    "\n",
    ">  -s, --summary, summary\n",
    "      Totals of the open month so far, with the balance or value of every category.\n",
    "\n",
    ">  -e, --edit, edit <id>\n",
    "      Change any field of an entry (press enter to keep the current value).\n",
    "\n",
    ">  -r, --delete, delete <id>\n",
    "      Delete (remove) an entry, after a y/n confirmation.\n",
    "\n",
    ">  -c, --categories, categories [<action>]\n",
    "      Without an action: your saving and investment categories with their balances. Actions:\n",
    "        -a, --add, add <saving|investment> <name> [<opening amount>]\n",
    "        -n, --rename, rename <old name> <new name>\n",
    "        -r, --delete, delete <name>                only when it starts the month at 0 and has no entries\n",
    "        -o, --set-opening, set-opening <name> <amount>   fix its balance or value at the start of the month\n",
    "      Example: bookkeeping -c -a investment \"Index fund\" 1000\n",
    "\n",
    ">  -o, --opening-cash, opening-cash [-]<amount>\n",
    "      Fix the opening cash of the open month (it can be negative).\n",
    "\n",
    ">  -C, --close, close\n",
    "      Preview the month, confirm, write both reports and open the next month.\n",
    "\n",
    ">  -h, --help, help [flags|features]\n",
    "      This help: everything, only the commands and flags, or only how things work.\n",
};

static const char *FEATURES[] =
{
    "\n",
    "#THE MENU\n",
    "Run bookkeeping without arguments. \"Add entries\" asks for the type once (income, expense, saving deposit...)\n",
    "and then records as many entries of that type as you want, each one saved at once. Press enter (or write\n",
    "\"done\") at the item name or category prompt to go back to the list of types. The first date defaults to\n",
    "today and every next one to the date of the previous entry.\n",
    "\n",
    "#AMOUNTS\n",
    "\"123\", \"123.4\" or \"123.45\": at most two decimals, no sign, no thousands separators, up to 15 characters.\n",
    "They are stored as integer cents, so no rounding ever changes a balance.\n",
    "\n",
    "#THE OPEN MONTH\n",
    "Only one month is open. Entries can go on any day of it, entries outside it are refused, and a closed month\n",
    "is never edited again. Fix mistakes before closing: list, edit, delete, rename categories, fix opening balances.\n",
    "Closing (-C) writes reports/MM_YYYY_Personal_Report.md and reports/MM_YYYY.md and carries every balance forward.\n",
    "\n",
    "#CASH AND NET WORTH\n",
    "  closing cash = opening cash + incomes - expenses - debts - saving deposits + saving withdrawals\n",
    "                 - investment contributions + investment withdrawals\n",
    "  net worth    = cash + savings + investment values\n",
    "A withdrawal moves money back to your cash; if you spent it, also record that expense.\n",
    "A withdrawal can never take more than the category has at that point of the month.\n",
    "Percentages in the reports are relative to the incomes of the month.\n",
    "\n",
    "#INVESTMENTS: HOW GAINS AND LOSSES ARE RECORDED\n",
    "The program can't know what the market did, so you tell it what each investment is WORTH and it works out\n",
    "the gain or loss from that. Three kinds of entries:\n",
    "  invest            money you put in from your cash (a contribution: it is not a gain)\n",
    "  invest-withdraw   money you take out to your cash (a withdrawal: it is not a loss)\n",
    "  invest-value      what the investment is worth today: the TOTAL value your bank or broker shows\n",
    "                    (what you put in + what it has earned or lost: returns, interest, price changes)\n",
    "Record a value update whenever you check your statement, and at least once before closing the month;\n",
    "only the latest one of the month counts. Without any, the program assumes the value only changed by what\n",
    "you put in or took out, so the gain of the month is 0.\n",
    "\n",
    "  gain/loss     = closing value - opening value - contributions + withdrawals\n",
    "  return %      = gain/loss / (opening value + contributions) x 100\n",
    "  closing value = the latest value update, moved by the contributions and withdrawals dated after it\n",
    "\n",
    "Example: \"Index fund\" is worth $1,000.00 at the start of the month.\n",
    ">  day 3   bookkeeping -a invest \"Index fund\" 200 -d 3\n",
    "          you put in $200.00: it is worth $1,200.00 and the gain is still $0.00\n",
    ">  day 20  bookkeeping -a invest-value \"Index fund\" 1260 -d 20\n",
    "          your broker shows $1,260.00: gain = 1,260 - 1,000 - 200 + 0 = +$60.00,\n",
    "          return = 60 / (1,000 + 200) = +5.00%\n",
    ">  day 25  bookkeeping -a invest-withdraw \"Index fund\" 100 -d 25\n",
    "          you take out $100.00: it is worth $1,160.00 and the gain stays +$60.00\n",
    "Had the broker shown $1,150.00 on day 20, the result would be a loss: 1,150 - 1,000 - 200 = -$50.00 (-4.17%).\n",
    "After every investment entry the program prints the value of that investment and its gain so far.\n",
    "\n",
    "#FILES AND ENVIRONMENT\n",
    "data/state.csv, data/categories.csv, data/YYYY-MM.csv and reports/*.md live in the data directory: the current\n",
    "directory, or the one in BOOKKEEPING_DIR. NO_COLOR (or output that is not a terminal) turns the colors off.\n",
};

static void print_lines(const char *lines[], size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (lines[i][0] == '#')
        {
            printf("%s%s%s", paint(COLOR_BOLD), lines[i] + 1, paint(COLOR_RESET));
        }
        else if (lines[i][0] == '>')
        {
            printf("%s%s%s", paint(COLOR_GREEN), lines[i] + 1, paint(COLOR_RESET));
        }
        else
        {
            fputs(lines[i], stdout);
        }
    }
}

/**
 * Prints the help: everything (topic NULL), only the commands and flags ("flags") or only how things work ("features").
 *
 * return 0, or ERR_USAGE for an unknown topic
 */
int print_help(const char *topic)
{
    if (topic == NULL)
    {
        print_lines(INTRO, sizeof(INTRO) / sizeof(INTRO[0]));
        print_lines(FLAGS, sizeof(FLAGS) / sizeof(FLAGS[0]));
        print_lines(FEATURES, sizeof(FEATURES) / sizeof(FEATURES[0]));
    }
    else if (strcmp(topic, "flags") == 0)
    {
        print_lines(FLAGS + 1, sizeof(FLAGS) / sizeof(FLAGS[0]) - 1); // without the blank line that separates it from the intro
    }
    else if (strcmp(topic, "features") == 0)
    {
        print_lines(FEATURES + 1, sizeof(FEATURES) / sizeof(FEATURES[0]) - 1);
    }
    else
    {
        return ERR_USAGE;
    }
    return 0;
}
