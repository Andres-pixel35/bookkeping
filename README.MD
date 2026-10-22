# Bookkeeping CLI Application

This is a C-based command-line bookkeeping application to track your money day by day: incomes, expenses, debts, and your own saving and investment categories. Exactly one month is *open* at a time; when you decide it's over you close it, and the program writes two Markdown reports and carries every balance forward to the next month.

Only the C standard library and POSIX are used (no ncurses, no readline, no database).

---

## Features

- **Daily recording:** add entries on any day, in any session, from an interactive menu or with one-line quick commands.
- **Your own categories:** create, rename and delete saving categories (e.g. an emergency fund) and investment categories (e.g. an index fund). Nothing is hard-coded.
- **Investments at market value:** record what an investment is worth whenever you like; the reports show the gain or loss and the return % of each one.
- **Fix mistakes before closing:** list the entries with their IDs, edit any field, delete entries, rename categories, fix opening balances.
- **Month close:** a preview, a y/n confirmation, then two `.md` reports (`reports/MM_YYYY_Personal_Report.md` and `reports/MM_YYYY.md`) that render well on GitHub.
- **Safe money data:** amounts are integer cents (no floating-point drift), files are rewritten through a temporary file + `rename()`, and closed months are never edited again.

---

## Directory Structure

```
bookkeping/
├── Makefile
├── include/
│   ├── cli.h  errors.h  finance.h  helpers.h  ledger.h  report.h  struct.h
├── src/
│   ├── main.c     (hands the arguments to the CLI, returns its exit code)
│   ├── cli.c      (menu, quick commands, setup wizard, help)
│   ├── ledger.c   (CSV files: loading, appending, safe rewrites)
│   ├── finance.c  (categories, entries, month totals, investment gains)
│   ├── report.c   (Markdown reports and month close)
│   ├── helpers.c  (input, money parsing/formatting, dates, colors)
│   └── errors.c   (error messages)
├── tests/
│   ├── test_money.c  test_ledger.c  test_finance.c  test_report.c  test_util.h
│   └── e2e.sh
├── data/      (your data, git-ignored: only .gitkeep is tracked)
└── reports/   (your reports, git-ignored: only .gitkeep is tracked)
```

> The contents of `data/` and `reports/` are personal financial data and are ignored by git, so they never end up on GitHub.

---

## Build

```sh
make            # release build: ./bookkeeping (-O2)
make debug      # build/debug/bookkeeping with -g and the address/undefined sanitizers
make test       # unit tests + tests/e2e.sh, all built with -fsanitize=address,undefined -g
make clean      # removes everything that was built
```

It compiles without warnings with `-std=c11 -Wall -Wextra -Wpedantic`.

---

## First run: the setup wizard

The first time (when there is no `data/state.csv`) the program asks:

1. **The first month to track** (`YYYY-MM`, default: the current month).
2. **Whether you have any opening cash** (the money you have at the start of that month besides savings and investments, e.g. your bank account and wallet). If yes, whether it's a positive amount (it can be negative if you start the month owing money) and how much.
3. **Your saving categories** with their balance at the start of the month, and then **your investment categories** with what each one is worth at the start of the month (its total value, as your bank or broker shows it). Add as many as you want (or none); write `done` (or just press Enter) to finish each list.

Nothing is written until the last answer, so an interrupted wizard leaves no half setup behind.

---

## Daily use

### Interactive menu

```sh
./bookkeeping
```

The header shows the open month with your cash, incomes and expenses so far, then:

```
  1) Add entries
  2) View month so far
  3) Edit entry
  4) Delete entry
  5) Categories
  6) Close month
  7) Help
  8) Quit
```

Each action returns to the menu, and every screen starts with its own title so the output of one action is easy to tell from the next.

**Adding entries:** choose *Add entries*, then the type (income, expense, saving deposit…). You can then record as many entries of that type as you want, one after the other; each one is saved as soon as it is complete. Press Enter (or write `done`) at the item name or category prompt when you are finished: you go back to the list of types, where you can pick another type or *Back to the main menu*. The first date defaults to today and every next one to the date of the previous entry, so a batch of receipts from the same day only needs the date once. Prompts show the expected format, and the default between brackets: Enter accepts it. When an answer is wrong the prompt says why and asks again. Ctrl-D (end of input) at the menu quits; in the middle of an action it stops that action (nothing of it is saved, everything before it is safe).

### Quick commands and flags

Every command has three equivalent forms, the usual Linux short (`-a`) and long (`--add`) flags, plus the word (`add`):

| Short | Long | Word | What it does |
|---|---|---|---|
| `-a` | `--add` | `add` | `<type> <item\|category> <amount> [-d <date>]` records an entry (types below). |
| `-l` | `--list` | `list` | Every entry of the open month, day by day, with its ID. |
| `-s` | `--summary` | `summary` | Totals of the open month so far, with every category. |
| `-e` | `--edit` | `edit` | `<id>` changes any field of an entry (Enter keeps the current value). |
| `-r` | `--delete` | `delete` | `<id>` deletes (removes) an entry, after a y/n confirmation. |
| `-c` | `--categories` | `categories` | Your categories with their opening balance and their balance now, or one of the actions below. |
| `-o` | `--opening-cash` | `opening-cash` | `[-]<amount>` fixes the opening cash of the open month. |
| `-C` | `--close` | `close` | Preview, y/n confirmation, both reports, next month. |
| `-h` | `--help` | `help` | `[flags\|features]` the help (see below). |
| `-d` | `--date` | | `YYYY-MM-DD` or the day of the month, for `-a`; `--date=14` works too. |

Actions of `-c` / `--categories` / `categories`:

| Short | Long | Word | What it does |
|---|---|---|---|
| `-a` | `--add` | `add` | `<saving\|investment> <name> [<opening amount>]` new category. |
| `-n` | `--rename` | `rename` | `<old> <new>` renames it everywhere in the open month. |
| `-r` | `--delete` | `delete` | `<name>` only when it started the month at 0 and has no entries (asks y/n). |
| `-o` | `--set-opening` | `set-opening` | `<name> <amount>` fixes the opening balance (or value) of a category. |

**Help pages:** `-h` (or `--help`, `help`) shows everything; `-h flags` only the commands and flags; `-h features` only how things work (amounts, the open month, cash and net worth, investments with a worked example, files). On a terminal, section titles are bold and commands are green.

Entry types:

| Type | Target | Meaning |
|---|---|---|
| `income`, `expense`, `debt` | an item name (free text) | Money in, money spent, debt paid. |
| `saving` | a saving category | Deposit: cash → saving. |
| `saving-withdraw` | a saving category | Withdrawal: saving → cash. |
| `invest` | an investment category | Contribution: cash → investment. |
| `invest-withdraw` | an investment category | Withdrawal: investment → cash. |
| `invest-value` | an investment category | What the investment is worth today (see below). |

Examples:

```sh
./bookkeeping -a expense food 25000
./bookkeeping --add expense "food, snacks" 12.50 --date 2026-10-14
./bookkeeping add income salary 3000 -d 1             # day 1 of the open month
./bookkeeping -a saving "Emergency fund" 300
./bookkeeping -a invest "Index fund" 200
./bookkeeping -a invest-value "Index fund" 1260
./bookkeeping -c -a investment "Gold" 500             # new investment category worth $500.00
./bookkeeping -l
./bookkeeping -e 5
./bookkeeping -h features
```

Names with spaces or commas go between quotes. The date defaults to today when today is inside the open month; otherwise you must give it (`-d` / `--date`). Any day of the open month is accepted; dates outside it are refused. Invalid arguments print a usage line and return a non-zero exit code.

If today is already past the open month, the program shows a notice suggesting you close it.

---

## Fixing mistakes before closing

Everything in the open month can still change: list the entries (`-l` or menu option 2) to see their IDs, then `-e <id>` (any field: type, item or category, amount, date) or `-r <id>`. Categories can be renamed (the entries follow the new name) and their opening balances fixed. A change is refused if it would make a withdrawal take more than its category had at that point of the month.

---

## Closing a month

`bookkeeping -C` (or `--close`, `close`, or menu option 6) shows the preview summary and asks for confirmation. Then it:

1. writes `reports/MM_YYYY_Personal_Report.md` and `reports/MM_YYYY.md`,
2. carries the closing cash and the closing balance/value of every category forward,
3. opens the next month.

A month can be closed before it ends (the preview warns you), but not before it has started. A closed month is never edited again, and existing reports are never overwritten: the close is refused and explained instead.

---

## Investments: how gains and losses are recorded

The program can't know what the market did, so you tell it what each investment is **worth**, and it works out the gain or loss from that. There are three kinds of investment entries:

| Entry | Command | Meaning |
|---|---|---|
| Contribution | `-a invest <category> <amount>` | Money you put in from your cash. It is **not** a gain. |
| Withdrawal | `-a invest-withdraw <category> <amount>` | Money you take out to your cash. It is **not** a loss. |
| Value update | `-a invest-value <category> <amount>` | What the investment is worth today. |

For a value update, enter the **total current value, including returns, interest and price changes** — what your bank or broker shows, not just the money you put in:

> Enter what this investment is worth today (what you put in + what it has earned or lost).

Record a value update whenever you check your statement, and at least once before closing the month. You can record as many as you want: each one stays in the ledger, but only the **latest one of the month** (by date, then by the order you recorded them) sets the value, the gain and the return %. Without any value update, the program assumes the value only changed by what you put in or took out, so the month's gain is 0. After every investment entry the program prints the value of that investment and its gain so far, so you see the effect immediately.

Per investment category and month:

```
closing_value = the latest value update of the month, moved by the contributions and withdrawals dated after it
                (with no value update: opening_value + contributions - withdrawals, so the gain is 0)
gain          = closing_value - opening_value - contributions + withdrawals
return %      = gain / money_at_work x 100                        (n/a when no money was at work)
money_at_work = opening_value + sum of each contribution (minus each withdrawal) made before the latest value update,
                weighted by the share of days it was invested until that update:
                amount x (update_day - its_day + 1) / update_day
```

The return % weights money by how long it was invested (the *Modified Dietz* method), so the same gain is a better result when the money arrived late. For example, starting with 100, adding 200 and recording a value of 360 on day 20 is always a gain of **+60**, but:

| The 200 came in on | Money at work | Return |
|---|---|---|
| day 1 | 100 + 200 × 20/20 = 300 | +20.00% |
| day 5 | 100 + 200 × 16/20 = 260 | +23.08% |
| day 19 | 100 + 200 × 2/20 = 120 | +50.00% |

**Worked example.** "Index fund" is worth $1,000.00 at the start of the month.

| Day | Command | Value | Gain so far |
|---|---|---|---|
| 3 | `-a invest "Index fund" 200 -d 3` (you put in $200.00) | $1,200.00 | $0.00 |
| 20 | `-a invest-value "Index fund" 1260 -d 20` (your broker shows $1,260.00) | $1,260.00 | 1,260 − 1,000 − 200 + 0 = **+$60.00**; money at work 1,000 + 200 × 18/20 = 1,180, return 60 / 1,180 = **+5.08%** |
| 25 | `-a invest-withdraw "Index fund" 100 -d 25` (you take out $100.00) | $1,160.00 | still **+$60.00 (+5.08%)**: taking money out is not a loss |

Had the broker shown $1,150.00 on day 20, the month would end in a loss: 1,150 − 1,000 − 200 = **−$50.00 (−4.24%)**.

### Limitations of the return %

The gain/loss is exact. The return % is an approximation:

- **It assumes the gain built up evenly over the days.** If the price jumped right before or after a big contribution or withdrawal, the real return differs. An exact (time-weighted) return would need the value of the investment right before every contribution and withdrawal, which the program doesn't compute.
- **It only covers the month up to the latest value update.** After it the value is assumed unchanged, and contributions or withdrawals dated after it don't count in the percentage (they still change the value one to one).
- **Earlier value updates of the month don't change the gain or the percentage**; only the latest one does.
- **Dates are whole days:** money moved on a day counts as invested for that whole day.
- **Little or no money at work:** when almost everything was withdrawn the percentage would be meaningless, so it shows **n/a**; with very little money at work it can look extreme.
- **One month only:** it is the return of the month, not a yearly rate. The total of all investments adds the money at work of each one, each measured up to its own latest value update.

---

## Money formulas

```
closing_cash = opening_cash + incomes - expenses - debts
             - saving_deposits + saving_withdrawals
             - investment_contributions + investment_withdrawals

net_worth    = cash + savings balances + investment values
```

- A withdrawal (from savings or investments) moves money back to your cash. If you spent it, also record that expense.
- A withdrawal can never be more than the balance (or value) of that category at that point of the month.
- The opening cash is shown on its own line, not mixed into the incomes. It may be negative.
- Percentages are relative to the month's incomes (items inside a section: relative to that section).
- Amounts are stored as integer cents. Input accepts `123`, `123.4` or `123.45` (at most two decimals, up to 15 characters, no sign or thousands separators). They are displayed like `$4,538,466.26`.

---

## Reports

- **`reports/MM_YYYY_Personal_Report.md`** (full breakdown): a summary table (opening cash, incomes, expenses, debts, saved, withdrawn from savings, invested, withdrawn from investments, investment gain/loss, closing cash, net worth and its change), the incomes, expenses and debts aggregated by item name (case-insensitive) with %, the savings per category (opening, deposits, withdrawals, closing), the investments table above, and a day-by-day ledger of every entry.
- **`reports/MM_YYYY.md`** (key drivers): the top `TOP_SIZE` incomes, expenses and debts aggregated by name with their % of the category, expenses and debts as % of incomes, % of incomes saved and invested, the gain/loss and return % of each investment and in total, and the net worth change.

The reports are written for people. The program never reads them back: everything carried to the next month comes from the CSV files.

---

## File formats

All files live inside the data directory: the current directory, or the one in `BOOKKEEPING_DIR`.

| File | Content |
|---|---|
| `data/state.csv` | `open_month,opening_cash_cents` — e.g. `2026-10,100000` |
| `data/categories.csv` | `kind,name,opening_balance_cents` — `kind` is `saving` or `investment`; balances/values at the start of the open month |
| `data/YYYY-MM.csv` | `id,date,type,category,name,amount_cents` — one entry per line (`category` for saving/investment types, `name` for incomes, expenses and debts) |
| `reports/*.md` | the generated reports |

The CSV files have a header row and use RFC 4180 quoting: a field with a comma, a quote or a newline is quoted and its quotes doubled (`"food, snacks"`). New entries are appended and flushed at once; edits, deletes and the month close rewrite files through a temporary file in the same directory and `rename()`, so a crash never leaves a half-written file. Closed months keep their `data/YYYY-MM.csv` as history.

---

## Environment variables

- **`BOOKKEEPING_DIR`**: the data directory (default `./`). It must exist; `data/` and `reports/` are created inside it when missing.
- **`NO_COLOR`**: when set, no colors. Colors are also used only when the output is a terminal.

---

## Exit Codes

The program returns specific numeric codes to indicate its exit status. You can check the exit code in your terminal after the program finishes by running:

```sh
echo $?
```

| Exit Code | Macro Name        | Description                                      |
|-----------|-------------------|--------------------------------------------------|
| 0         | (Success)         | Success (no error)                               |
| -51       | ERR_MEMORY        | Memory allocation error                          |
| -52       | ERR_FILE          | A data file cannot be opened, written, or is malformed |
| -53       | ERR_DIC           | The data directory (or `data/`, `reports/`) cannot be opened or created |
| -54       | ERR_YEAR          | Invalid year (0000)                              |
| -55       | ERR_CHOOSE        | Invalid y/n answer                               |
| -56       | ERR_NUMBER_ITEMS  | Exceeded maximum number of allowed items (kept, no limit remains) |
| -57       | ERR_EOF           | The input ended in the middle of an action       |
| -58       | ERR_USAGE         | Invalid command-line arguments, amount or date format |
| -59       | ERR_DATE          | Date outside the open month, no date given when today is outside it, or the month hasn't started (close) |
| -60       | ERR_BALANCE       | A withdrawal would take more than the category has at that point |
| -61       | ERR_CATEGORY      | Unknown or duplicate category, or a delete that isn't allowed |
| -62       | ERR_REPORT_EXISTS | The reports of the month already exist (never overwritten) |
| -63       | ERR_ENTRY         | No entry with that ID in the open month          |

> **Note:**  
> Since shell exit codes are unsigned 8-bit values, negative numbers will appear as their 256-based equivalent (e.g., `-51` appears as `205`).

Refer to [`include/errors.h`](include/errors.h) if you want to change their values.

---

## Customization

- **Top items:** change `TOP_SIZE` in [`include/helpers.h`](include/helpers.h) (default 3) to show a different number of top items in the key drivers report.
- **Maximum amount length:** `MAX_LEN_AMOUNT` in [`include/helpers.h`](include/helpers.h) (default 15 characters).
- **Maximum name length:** `MAX_NAME_LEN` in [`include/struct.h`](include/struct.h) (default 50 characters).
- **Data directory:** `BOOKKEEPING_DIR` (see above).

---

## Tests

`make test` runs the unit tests (`tests/test_*.c`, plain `assert`) and `tests/e2e.sh`, which drives the real binary through the wizard and the quick commands in a temporary `BOOKKEEPING_DIR` and checks both reports. Everything is built with `-fsanitize=address,undefined`, so any leak or undefined behaviour fails the run. The release binary is used once, under `ulimit -v`, to check that a real out-of-memory error stops the program cleanly (sanitizers can't run with that limit). `E2E_DIR=/some/dir sh tests/e2e.sh build/test/bookkeeping` keeps the generated data and reports to look at.

---

## License

This project is provided as-is for personal bookkeeping and educational purposes.
