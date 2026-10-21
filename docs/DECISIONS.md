# Rework plan and decisions

## Plan (covers every section of docs/SPEC.md)

1. Read the whole codebase, measure the warning baseline (17 warnings with `-std=c11 -Wall -Wextra -Wpedantic`).
   → verify: baseline recorded.
2. Housekeeping + `Makefile` (`make`, `make debug`, `make test`, `make clean`) — SPEC 3.10, 3.9.10.
   → verify: `git ls-files data reports` lists the two `.gitkeep`, `git check-ignore` ignores the data.
3. Money and input (`src/helpers.c`): integer cents, `parse_amount`, `format_money`, `read_input` with EOF,
   prompts with defaults — SPEC 3.4, 3.7, 3.9.5, 3.9.9. → verify: `tests/test_money.c`.
4. Storage (`src/ledger.c`): CSV with RFC 4180 quoting, `BOOKKEEPING_DIR`, load once into dynamic arrays,
   append + `fflush`, temp file + `rename()` rewrites — SPEC 3.5, 3.9.4, 3.9.6, 3.9.8. → verify: `tests/test_ledger.c`.
5. Categories, entries and month math (`src/finance.c`): user categories, rename/delete rules, chronological
   balance checks, cash formula, market-value investments, aggregation by name — SPEC 3.1, 3.2, 3.3, 3.4, 3.9.1,
   3.9.2. → verify: `tests/test_finance.c`.
6. Markdown reports and close month (`src/report.c`) — SPEC 3.2, 3.6, 3.9.3. → verify: `tests/test_report.c`.
7. CLI (`src/cli.c`, `src/main.c`): menu loop, quick commands, wizard, help, colors — SPEC 3.7, 3.9.7, 3.9.9.
   → verify: `tests/e2e.sh`.
8. Code quality: status codes everywhere, single cleanup paths, no leaks, no warnings, ≤ 4 parameters — SPEC 3.8.
   → verify: `make clean && make` silent, `make test` under ASan/UBSan/LSan.
9. README rewrite + this file — SPEC 3.10, 2.2. → verify: README covers every listed topic and every exit code.
10. Self-review of the whole diff against the spec and the bug list, push, PR — SPEC 5.

## Decisions made autonomously

### Repository and process
1. **Sample reports.** The spec names `reports/03_2026.txt` and `reports/03_2026_Personal_Report.txt`, which do not
   exist. The tracked samples are `12_2023`, `01_2024`, `09_2024` … `12_2024` (both kinds, `.txt`). They were all
   deleted: same intent (example data in a retired format, in a folder that is now git-ignored).
2. **Binary.** `bookkeeping` was not tracked (commit `1f6b22a` had already stopped tracking the old `program`),
   so there was nothing to `git rm --cached`.
3. **`.gitignore`.** `/bookkeeping` (anchored, so only the binary at the root), `build/`, `*.o`, `data/*` and
   `reports/*` with `!data/.gitkeep` / `!reports/.gitkeep`. The contents of `data/` and `reports/` are personal
   financial data and must never reach GitHub.
4. **Commit granularity.** The old interactive `main` read and wrote the `.txt` reports directly, so it could not
   compile against the new storage and types, and the old code had no tests to keep green. Instead of
   intermediate commits that don't build, the history is: (1) housekeeping (ignore rules, `.gitkeep`, sample
   reports removed), (2) the rework of `src/` + `include/` together with the `Makefile`, the unit tests and
   `tests/e2e.sh`, (3) documentation. Every commit from (2) on passes `make && make test`.
5. **Tools.** Context7 MCP and conda are not available in this environment, so work continued without them.
   Nothing was installed. valgrind is installed but `make test` relies on `-fsanitize=address,undefined` (with
   `detect_leaks=1` and `-fno-sanitize-recover=all`); LeakSanitizer was checked to work in this container with a
   program that leaks on purpose.

### Errors
6. **New exit codes** continue the sequence: `ERR_EOF -57` (input ended in the middle of an action), `ERR_USAGE -58`
   (invalid arguments or input in a quick command), `ERR_DATE -59` (date invalid for the action: outside the open
   month, no date when today is outside it, closing a month that hasn't started), `ERR_BALANCE -60` (withdrawal
   over the balance/value), `ERR_CATEGORY -61` (unknown or duplicate category, delete refused), `ERR_REPORT_EXISTS
   -62`, `ERR_ENTRY -63` (no entry with that ID). Each has a message function in `src/errors.c`.
7. **Kept but unused:** `ERR_NUMBER_ITEMS` and `add_item_error` (no item cap remains; the spec says to keep every
   macro and message function). `ERR_YEAR`/`err_year` are used by the wizard for the year 0000.
8. **Rewritten messages** (only where they had become false): `previous_file_not_open`, `file_empty` and
   `missing_values` no longer mention `MM_YYYY.txt` or consecutive months; `dic_not_open` takes the directory
   (there is no `BASE_PATH` any more). Typos fixed: "Remeber", "atributes", "esential", "neccessaries", "of it",
   and a stray `”` in `close_file_error`. All messages are red only under the color rule (decision 33).
9. **A missing `BOOKKEEPING_DIR` is an error (ERR_DIC), not created.** A mistyped variable must not silently start a
   new, empty book somewhere else. `data/` and `reports/` inside it are created (mode 0700) when missing.

### Money and input
10. **Amounts** are `long long` cents. Accepted: `123`, `123.4`, `123.45`, also `.5` (= 0.50) and `5.` (= 5.00).
    Rejected with the reason: more than two decimals, more than one dot, signs, spaces, thousands separators,
    exponents, empty input, and more than `MAX_LEN_AMOUNT` (15) characters. `MAX_LEN_DOUBLE` was renamed
    `MAX_LEN_AMOUNT` because there are no doubles any more; the author's comment was kept.
11. **Zero:** incomes, expenses, debts, deposits, contributions and withdrawals must be > 0 (old message "You must write
    a value greater than 0 per each item"). A value update may be 0 (an investment can lose everything). Opening
    balances may be 0; only the opening cash may be negative.
12. **No overflow checks on sums.** With at most 15 input characters a single amount is < 10^17 cents; reaching
    `LLONG_MAX` (9.2·10^18) needs absurd volumes, so per "no error handling for impossible scenarios" it isn't checked.
13. **Percentages** are computed as `double` from cents for display only; they never feed back into a balance.

### Storage
14. **RFC 4180 quoting** was chosen over rejecting characters, so `food, snacks` and `say "hi"` work. Names still
    can't contain control characters (newline, tab…): `checK_item` refuses them because they would break the
    Markdown tables. The reader still understands quoted newlines.
15. **Column use:** `type` holds the quick-command keyword (`income`, `expense`, `debt`, `saving`, `saving-withdraw`,
    `invest`, `invest-withdraw`, `invest-value`). Income/expense/debt entries leave `category` empty; saving and
    investment entries leave `name` empty.
16. **IDs** are the last ID + 1, entries keep their position when edited, so IDs in the file are strictly
    increasing (checked on load). The state file stays exactly `open_month,opening_cash_cents` as specified, so
    deleting the newest entry lets its ID be reused by the next one; no other ID ever changes.
17. **Strict loading.** Headers must match exactly, blank lines are skipped, anything else malformed (unknown
    category, date of another month, IDs going back, non-integer cents, a 0 expense…) stops with `missing_values`
    and ERR_FILE rather than guessing with money data.
18. **Durability.** Rewrites go to `<file>.tmp` in the same directory, are `fflush`ed and `fsync`ed, then `rename()`d.
    New entries are appended and `fflush`ed.
19. **Known limitation:** operations that change two files (close month: categories then state; category rename:
    entries then categories) are atomic per file but not across files. The window is between two consecutive
    `rename()` calls. `state.csv` is always written last, being the file that says which month is open.
20. **History.** The closed month keeps its `data/YYYY-MM.csv`, which is never written again.

### Categories and entries
21. **Category names are unique across both kinds**, compared case-insensitively (the user's spelling is stored);
    so `bookkeeping categories rename/delete <name>` never needs a kind.
22. **Delete rule:** a category can be deleted only when it started the month at 0 and has no entries in the open
    month (with no entries, its balance is its opening balance). Otherwise the message explains what to do.
23. **"Fix categories"** also means fixing opening balances: `categories set-opening <name> <amount>` and
    `opening-cash [-]<amount>` (menu: Categories → 4 and 5), e.g. for a typo in the wizard. A lower opening balance
    is refused if a withdrawal of the month relied on it. The menu has no other place for the opening cash, so it
    lives in the Categories submenu.
24. **"At that point in time"** = chronological order by day, and by ID (recording order) within a day. After every
    add, edit, delete and opening fix the whole month is re-checked, so a change can't break a later withdrawal
    either (deleting a deposit that a withdrawal needed is refused with the same message).
25. **Same-day value updates** follow recording order: a contribution recorded after the update on the same day is
    added on top of it.
26. **Default date:** today, only when today is inside the open month. Otherwise the interactive prompt has no
    default (it says why) and quick commands require `--date` (ERR_DATE). An entry is never silently dated to a
    day the user didn't choose. `--date` and the prompt accept `YYYY-MM-DD` or just the day (`14`).
27. **Edit** is interactive (each field with its current value as default, Enter keeps it), both from the menu and
    from `bookkeeping edit <id>`. No flag-based edit, to keep the grammar small.
28. **Savings withdrawals now return money to cash** (behaviour change: the old program let a withdrawal vanish).
    The prompt says to record an expense if the money was spent.

### Close month and reports
29. **Closing is allowed once the month has started**, even before it ends (the preview warns "the month is not
    over yet"). A month that hasn't started can't be closed: this is what makes a second `close` right after the
    first one refuse (ERR_DATE), since the newly opened month is in the future.
30. **Existing reports** make the close refuse before the preview (ERR_REPORT_EXISTS); if the second report can't be
    written the first one is removed again. The message explains how to recover from an interrupted close.
31. **Percentages:** the summary tables are % of the month's incomes; the per-section item tables (personal report)
    and the top-`TOP_SIZE` tables (key drivers) are % of their own section, as before. The total return % applies
    the same formula to the totals. Ties in rankings are broken alphabetically. Aggregated items keep the first
    spelling seen. Month names are English. Only `|` is escaped in names.
32. **Net worth** is shown at the start, at the end and as a signed change in both reports.

### CLI
33. **Colors** only when `isatty(STDOUT_FILENO)` and `NO_COLOR` is unset (any value, even empty, disables them);
    errors on stderr follow the same rule.
34. **EOF:** at the main menu prompt it behaves like Quit (exit 0, everything is already saved). In the middle of
    an action, the wizard or a quick command prompt it prints a message and exits with ERR_EOF; nothing partial is
    saved (each action saves only when complete, the wizard writes nothing until its last answer).
35. **First run with a quick command** (e.g. `bookkeeping add …` with no `data/state.csv`): the wizard runs first,
    then the command. `help` works without setup.
36. **Confirmations** (delete entry, delete category, close) default to "n"; piping `y` works for scripts.
37. **Menu option input** must be a number in range; any other input re-prompts saying what was expected.
38. **Comments.** Every original comment whose code survives is kept: word for word where the code is unchanged,
    adapted where the code changed (e.g. the `get_values_double` comment now describes `parse_amount`, the
    `update_savings` one describes `check_balances`). Comments describing removed code (the `.txt` parsing, the
    wildcard search, the `HANDLE_*` macros, the old one-shot flow of `main`) went away with that code.
