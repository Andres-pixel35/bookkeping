# Bookkeeping CLI — rework specification

You are working **fully autonomously**. Nobody will answer questions. Never stop to
ask; when something is ambiguous, pick the option that is simplest, safest for the
user's money data, and most consistent with this spec, then record it in a
"Decisions made autonomously" list (kept in `docs/DECISIONS.md` while you work and
copied into the PR description at the end).

## 0. Context

C99/C11 command-line bookkeeping app: `src/main.c`, `src/finance.c`, `src/helpers.c`,
`src/errors.c`, headers in `include/`. Today it asks once per month for incomes,
expenses and debts, plus **hard-coded** savings (Travels, Planned purchases,
Emergencies) and investments (Real estate, Currencies, Commodities, Stocks), then
writes two `.txt` reports into `reports/`. Next month it carries totals forward by
`strstr`-parsing the previous `.txt` report. Read every file fully before changing
anything.

## 1. Working rules (the owner's global CLAUDE.md, verbatim; it applies to you)

> Cloud note from the owner: if the Context7 MCP or conda isn't available in this
> environment, continue without them. **Never install packages unless this project
> truly needs them** — and it doesn't: use only the C standard library + POSIX and
> the tools already in the image (gcc/cc, make, sh). Use valgrind only if it's
> already installed; otherwise rely on gcc's `-fsanitize=address,undefined`.

```
## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## 5. Global instructions
Always use the MCP Context 7 when writing or generating code. Treat Context 7 as the default environment, assumptions set, and reference frame for all code-related tasks unless explicitly instructed otherwise.

Do not install packages unnecessarily, always verify if there are conda envs that could be use instead of download it, even if you do not find an env to use ask first instead of heading directly to download packages.

Don't be lazy, think and re think, truly think through instruction, I don't want a fast half done solution, I want the best no matter how long it takes. This does not mean that if the best solution is obvious and fast to do you should not, but you should avoid being lazy just to deliver a fast solution

## 6. Code Quality Principles

### Reusability & Generalization
- **Aim to generalize and reuse** existing functions when it makes sense
- **Readability first**: Do not sacrifice clarity for reusability
- **Maintainability matters**: Avoid over-engineering solutions
- **Don't force it**: If creating a new function is more natural and readable, do that instead

### Function Design
- **Limit function arguments to 4 maximum**
  - Use lists or dictionaries for passing multiple related values
  - Split functions if more than 4 arguments are needed
  - Keep function signatures clean and intuitive
```

How these rules apply to this task: "If uncertain, ask" → you cannot ask, so decide and
log it in DECISIONS.md. "Surgical changes" → this task explicitly asks for a broad
rework, so restructuring is in scope, but every change must trace back to a
requirement in this spec. Max 4 arguments per function (use a struct when you need
more).

## 2. Must keep

1. **Documentation comments.** Keep the existing explanatory comments (the `/** ... */`
   blocks and the inline "why" comments, in the author's voice) wherever the code they
   describe survives. New functions get comments in the same style and density.
2. **Error codes and messages.** Keep every macro and value in `include/errors.h`
   (`ERR_MEMORY -51` … `ERR_NUMBER_ITEMS -56`) and every message function in
   `src/errors.c` with its wording. Change a message only where it would now be false
   (e.g. it mentions `MM_YYYY.txt` or "consecutive months"); fixing obvious spelling
   typos is allowed. New error conditions get new codes continuing the sequence
   (`-57`, `-58`, …) with a message function in the same style, and are added to the
   README exit-code table. `main` still returns these codes.
3. Code style: Allman braces, snake_case, 4-space indent, `include/` + `src/` layout.

## 3. Requirements

### 3.1 User-defined savings and investment categories (remove the hard-coding)
- No category names in the code. The user creates, renames and deletes their own
  saving categories and investment categories (any number).
- Rename updates the name everywhere it is used in the open month. Deleting a category
  is allowed only when its balance is 0 and it has no entries in the open month;
  otherwise refuse with a clear message.
- Incomes, expenses and debts keep free-form item names, as today.

### 3.2 Daily recording, monthly wrap-up
- The user can record entries on any day (the date defaults to today, and can be set
  to any day of the open month) and in any session: incomes, expenses, debts, saving
  deposits, saving withdrawals, investment contributions, investment withdrawals and
  investment value updates.
- Exactly one month is *open* at a time: the month after the last closed one (or
  the month chosen in the setup wizard). An entry dated outside the open month is
  refused. If today is past the open month, show a notice suggesting closing it.
- **Close month** (run only when the user chooses): show a preview summary and ask
  for y/n confirmation, then generate the two `.md` reports, carry the closing
  balances forward, and open the next month. A closed month's data is never edited
  again, and existing reports are never overwritten (refuse and explain instead).
- **Fix mistakes before closing:** list the open month's entries with stable IDs, edit
  any field of an entry, delete an entry (with confirmation), and rename or fix
  categories, all before the wrap-up.
- Withdrawals (from savings or investments) can't exceed the balance or value of
  that category at that point in time (keep today's check and message style).

### 3.3 Investment gains and losses (market-value model)
- The user records an investment's **current total market value** whenever they like
  (a "value update"). The prompt and help text must say clearly that this value is
  the **total current value, including returns, interest and price changes, not just
  the money they put in**. Example wording: "Enter what this investment is worth
  today (what you put in + what it has earned or lost)".
- Per investment category per month:
  `gain = closing_value − opening_value − contributions + withdrawals`
  `return % = gain / (opening_value + contributions) × 100` (0 when the denominator is 0).
  `closing_value` = the latest value update in the month, adjusted by any
  contributions/withdrawals dated after it; with no value update,
  `closing_value = opening_value + contributions − withdrawals` (gain 0).
- The reports show opening value, contributions, withdrawals, gain or loss (signed),
  return %, and closing value per category, plus totals.

### 3.4 Money semantics (write these into README and help)
- Store money as **integer cents** (`long long` / `int64_t`), never `double`, to
  remove floating-point drift. Input accepts `123`, `123.4`, `123.45` (at most two
  decimals; reject anything else with a clear message). Keep a maximum input length
  like `MAX_LEN_DOUBLE` so values can't overflow. Display amounts with thousands
  separators, e.g. `$4,538,466.26`.
- Cash balance:
  `closing_cash = opening_cash + incomes − expenses − debts − saving_deposits
  + saving_withdrawals − investment_contributions + investment_withdrawals`.
  A withdrawal moves money back to cash; if it was spent, the user records an
  expense (say so in the prompt). This changes the old behaviour, where a savings
  withdrawal disappeared silently — log it in DECISIONS.md.
- The opening balance is shown on its own line, not mixed into "Total income".
  Percentages are relative to the month's incomes, as before. The opening cash may
  be negative (keep the `ask_sign` idea in the wizard).
- Net worth = cash + savings balances + investment values, shown in both reports
  with its change against the opening.

### 3.5 Storage: plain CSV (owner's choice)
- The data directory is `./` by default and can be overridden with the env var
  `BOOKKEEPING_DIR` (this replaces the hard-coded `BASE_PATH`, and tests need it).
  Inside it:
  - `data/state.csv`: open month, opening cash (cents).
  - `data/categories.csv`: `kind,name,opening_balance_cents` (kind = saving|investment),
    holding balances/values as of the start of the open month.
  - `data/YYYY-MM.csv`: one entry per line:
    `id,date,type,category,name,amount_cents`, with a header row.
  - `reports/`: generated `.md` files.
- Minimal RFC 4180 quoting (fields containing a comma, a quote or a newline are
  quoted, and quotes are doubled) so names like `food, snacks` work. Or, if you
  judge it simpler and still friendly, reject those characters with a clear message;
  log the choice either way.
- New entries are appended and `fflush`ed. Edits, deletes and close-month rewrite
  files via **write to a temp file in the same directory + `rename()`**, so a crash
  never leaves a half-written file. Generated reports use the same temp+rename.
- Carry-over between months comes from these CSV files. **Never parse the `.md`
  reports.**
- Load each month's ledger once into a dynamic array (grown with `realloc`) and
  compute everything in one pass. No fixed `Items[100]` stack arrays; the 100-item
  limit can go. Keep `ERR_NUMBER_ITEMS` defined, and use it only if a cap remains.

### 3.6 Output: two Markdown files per closed month (owner's choice)
- `reports/MM_YYYY_Personal_Report.md` (full breakdown):
  title and period; a summary table (opening cash, incomes, expenses, debts, saved,
  withdrawn from savings, invested, withdrawn from investments, investment
  gain/loss, closing cash, net worth and its change); per-section tables for incomes,
  expenses and debts, aggregated by item name (case-insensitive), with %; a savings
  table per category (opening, deposits, withdrawals, closing); the investments
  table from 3.3; and a chronological day-by-day ledger table of every entry.
- `reports/MM_YYYY.md` (key drivers): the top `TOP_SIZE` (keep the macro, default 3)
  incomes, expenses and debts, **aggregated by name** across the month, each with its
  % of its category; expenses and debts as % of incomes; % of income saved and
  invested; investment gain/loss per category and in total; net worth change.
- Use real Markdown: headings, pipe tables with right-aligned money columns, and
  escape `|` in user-entered names. The reports must render well on GitHub.

### 3.7 Better CLI (stdlib + POSIX only; no ncurses, no readline)
- `./bookkeeping` with no arguments → **interactive menu loop**: a header showing
  the open month and a short running summary (cash, month incomes/expenses), then a
  numbered menu: Add entry · View month so far · Edit entry · Delete entry ·
  Categories (list/add/rename/delete with balances) · Close month · Help · Quit.
  Each action returns to the menu.
- **Quick commands** for daily use without the menu, e.g.
  `./bookkeeping add expense food 25000 [--date 2026-10-14]`,
  `add saving <category> <amount>`, `add invest-value <category> <amount>`,
  `list`, `summary`, `edit <id>`, `delete <id>`, `categories ...`, `close`, `help`.
  You choose the exact grammar; keep it consistent and document it in `help` and the
  README. Invalid arguments print a usage line and return a non-zero error code.
- **First run** (no `data/state.csv`) → setup wizard: first month to track
  (default: current month), opening cash (sign allowed), then create saving and
  investment categories with opening balances/values (zero or more of each, "done"
  to finish).
- ANSI colors (headers, green for positive amounts, red for negative, red for
  errors) **only** when stdout is a TTY (`isatty`) and `NO_COLOR` is unset.
- Clear prompts showing the expected format and any default in brackets; Enter
  accepts the default; any re-prompt says what was wrong. Destructive actions ask
  for y/n.
- **EOF/Ctrl-D never loops forever** (it does today: `read_input` returns "" on EOF
  and `ask_choose` re-prompts endlessly). On EOF, exit cleanly without losing saved
  data.
- Remove the "READ THE README BEFORE EXECUTING" banner; `help` replaces it.

### 3.8 General code quality, speed and memory
- Functions return `int` status codes (0 or an `ERR_*`); values come back through
  out-parameters. No more errors smuggled through `double` returns.
- Remove the duplicated copy-paste blocks (the per-category savings/investment code,
  the repeated `if (check == ERR_FILE) return ERR_FILE;` chains — use a single
  cleanup path such as `goto cleanup` that closes files and frees memory).
- Every `fopen` is closed on every path; every `malloc` is freed; ASan/LSan clean.
- Remove code that becomes unnecessary (e.g. `match_wildcard`/`has_files_wildcard`
  and the `.txt` parsing functions once nothing calls them).
- Split `main.c` sensibly (e.g. `cli.c` for menu/commands, `ledger.c` for CSV I/O,
  `report.c` for Markdown output, plus the existing `finance.c`, `helpers.c` and
  `errors.c`). Don't create a file for a handful of lines.
- Compile cleanly with `-std=c11 -Wall -Wextra -Wpedantic` (and fix sign-compare
  warnings such as `int i < size_t len`).

### 3.9 Bugs found in review (verify each, fix it, and add a test for it where it's testable)
1. `get_previous_savings`: the flags `total`, `travels`, `purchase`, `emergencies`,
   `final_balance` are uninitialized (UB). Whole function goes away with the new
   storage, but make sure no similar UB survives.
2. `compare_items` hard-codes `j = 2` instead of `TOP_SIZE - 1`, and ranks single
   entries, not items aggregated by name.
3. Reports are opened with `"a"` → re-running duplicates content.
4. Carry-over by `strstr` on report text: a user item named e.g. "Stocks" or
   "Travels" in the top-3 corrupts the parsed values; and labels differ between
   reports ("Currencies (forex)" vs "Currencies").
5. `get_items` treats `ERR_MEMORY` (−51) from `get_values_double` as "value <= 0" and
   loops; `get_items` also passes the magic number `15` instead of `MAX_LEN_DOUBLE`.
6. `savings_withdraws[3]` is bounded by `MAX_ITEMS` (100) in `update_item`.
7. `HANDLE_MEMORY_ERRORS` breaks only out of the `switch`, so subsequent prompts still run.
8. Many `return ERR_FILE` paths in `main` leak the open `FILE *`.
9. EOF infinite loop (3.7).
10. `.gitignore` ignores `program`, but the binary is named `bookkeeping`.

### 3.10 Repo housekeeping
- Delete `reports/03_2026.txt` and `reports/03_2026_Personal_Report.txt` (owner's
  choice). If the compiled `bookkeeping` binary is tracked, `git rm --cached` it.
- `.gitignore`: the `bookkeeping` binary, build artifacts, and the contents of `data/`
  and `reports/`. This is personal financial data and must not end up on GitHub. Log
  this in DECISIONS.md.
- Add an empty `.gitkeep` to every folder whose contents are git-ignored (at least
  `data/` and `reports/`) so the folders still exist after a fresh clone. Ignore the
  contents, not the folder, otherwise git ignores the `.gitkeep` too:
  ```
  data/*
  !data/.gitkeep
  reports/*
  !reports/.gitkeep
  ```
  Verify with `git ls-files data reports` (should list both `.gitkeep` files) and
  `git check-ignore -v data/2026-10.csv reports/10_2026.md` (both ignored).
- `Makefile`: `make` (release, `-O2`), `make debug`, `make test`, `make clean`.
  `make test` builds with `-fsanitize=address,undefined -g` and runs everything.
- Rewrite `README.MD` for the new behaviour: build, first-run wizard, daily use (menu
  and quick commands with examples), editing before close, close month, investment
  value updates (stress "total value including returns"), money formulas, file
  formats, `BOOKKEEPING_DIR`, `NO_COLOR`, the exit-code table (old + new codes, keeping
  the note about 8-bit exit codes), and customization (`TOP_SIZE`, etc.).

## 4. Tests (no frameworks — plain C `assert` + POSIX sh)
- `tests/test_*.c` unit tests: money parsing and formatting (edge cases: `0`, `.5`,
  `1.234` rejected, max length, thousands separators); CSV quoting round-trip;
  ledger append/edit/delete round-trip; month-close math (cash formula, savings
  balances, investment gain **and** loss, return %, top-N aggregation by name,
  case-insensitivity); withdrawal over the balance is refused; category rename and
  delete rules.
- `tests/e2e.sh`: in a `mktemp -d` dir with `BOOKKEEPING_DIR` set, drive the binary
  through the wizard (piped stdin) and quick commands: custom categories, entries on
  several days, the same expense name on different days, an edit, a delete, a
  saving withdrawal, an investment contribution, one value update with a gain and
  another category with a loss, then close. Assert on exit codes and on the key
  lines/numbers of both generated `.md` files (golden files in `tests/expected/` are
  fine). Also check: closing again refuses, adding an entry to the closed month
  refuses, EOF on stdin exits without hanging (use `timeout`), and stdout contains
  no ANSI codes when it isn't a TTY.

## 5. Process
1. Read the whole codebase. Write a short plan in `docs/DECISIONS.md` → verify: plan
   covers every section of this spec.
2. Makefile + test harness first; build the current code to see the warning baseline.
3. Implement in small, coherent commits (storage → categories → entries/edit →
   close + reports → investments → CLI → cleanup → docs), keeping
   `make && make test` green after each commit.
4. Self-review the full diff against this spec and the review bug list; remove
   anything speculative or unused.
5. Push the branch and open a PR to the default branch. The PR description contains:
   a summary, how to try it, the full "Decisions made autonomously" list, and
   anything not done with the reason. 
