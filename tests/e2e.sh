#!/bin/sh
# End-to-end test: drives the real binary through the setup wizard (piped stdin) and the quick commands
# inside a temporary BOOKKEEPING_DIR, closes the month and checks both Markdown reports.
#
# usage: sh tests/e2e.sh path/to/bookkeeping [path/to/release/bookkeeping]
# The optional release binary (built without sanitizers, which can't run under ulimit -v) is used for the
# out-of-memory check.
# Set E2E_DIR to keep the data directory (and its reports) somewhere instead of a removed mktemp -d.

set -u

BIN=${1:?usage: sh tests/e2e.sh path/to/bookkeeping}
case $BIN in
    /*) ;;
    *) BIN=$(pwd)/$BIN ;;
esac

if [ -n "${E2E_DIR:-}" ]; then
    DIR=$E2E_DIR
    rm -rf "$DIR"
    mkdir -p "$DIR"
else
    DIR=$(mktemp -d)
    trap 'rm -rf "$DIR"' EXIT
fi

export BOOKKEEPING_DIR="$DIR"
unset NO_COLOR # stdout is not a terminal here, so there must be no colors even without NO_COLOR

M=$(date +%Y-%m)      # the wizard's default month is the current one, the entries go on days 01-05 of it
MM=$(date +%m)
YYYY=$(date +%Y)
OUT="$DIR/e2e-output.log"
FAILS=0

# shells see the negative ERR_* codes modulo 256
ERR_EOF=199
ERR_USAGE=198
ERR_DATE=197
ERR_BALANCE=196
ERR_CATEGORY=195
ERR_ENTRY=193

fail()
{
    echo "FAIL: $*"
    FAILS=$((FAILS + 1))
}

# run <expected exit code> <stdin text> <arguments...>; timeout catches any prompt that loops forever (exit 124)
run()
{
    expected=$1
    input=$2
    shift 2
    printf '%b' "$input" | timeout 10 "$BIN" "$@" >>"$OUT" 2>&1
    code=$?
    [ "$code" -eq "$expected" ] || fail "bookkeeping $* exited with $code, expected $expected"
}

contains()
{
    grep -F -q -- "$2" "$1" || fail "$(basename "$1") does not contain: $2"
}

lacks()
{
    if grep -F -q -- "$2" "$1"; then fail "$(basename "$1") should not contain: $2"; fi
}

# 1. the end of the input in the middle of the wizard: exits, and nothing half done is saved
ERR_DIC=203
BOOKKEEPING_DIR="$DIR/missing" run $ERR_DIC '' --list # a data directory that does not exist is never created silently
mkdir "$DIR/interrupted"
BOOKKEEPING_DIR="$DIR/interrupted" run $ERR_EOF '\ny\n'
[ -e "$DIR/interrupted/data/state.csv" ] && fail "an interrupted wizard left a data/state.csv"

# 2. the wizard (opening cash: yes, positive, 1000) with user-named categories, then Quit (8) from the menu
run 0 '\ny\ny\n1000\nEmergency fund\n500\nNew laptop\n200\ndone\nIndex fund\n1000\nCrypto coins\n500\n\n8\n'
contains "$DIR/data/categories.csv" "saving,Emergency fund,50000"
contains "$DIR/data/categories.csv" "investment,Crypto coins,50000"
contains "$DIR/data/state.csv" "$M,100000"

# 3. invalid arguments print the usage and fail
run 0 '' --help
run $ERR_USAGE '' bogus
run $ERR_USAGE '' --add expense food 1.234 --date "$M-02"
run $ERR_USAGE '' --add expense food
run $ERR_USAGE '' --add lunch food 12 --date "$M-02"
run $ERR_CATEGORY '' --add saving "Holiday" 5 --date "$M-02"
run $ERR_CATEGORY '' --add invest "Emergency fund" 5 --date "$M-02"
run $ERR_DATE '' --add expense food 5 --date 2001-01-01

# 4. entries on several days, the same expense name on different days, a name with a comma
run 0 '' --add income salary 3000 --date "$M-01"
run 0 '' --add expense food 120.50 --date "$M-02"
run 0 '' --add expense Food 79.50 --date 4
run 0 '' --add expense rent 900 --date "$M-01"
run 0 '' --add expense transport 60 --date "$M-03"
run 0 '' --add expense "snacks, drinks" 20 --date "$M-03"
run 0 '' --add debt "credit card" 150 --date "$M-05"
run 0 '' --add income freelance 400 --date "$M-03"
run 0 '' --add saving "emergency FUND" 300 --date "$M-02"
run 0 '' --add saving-withdraw "New laptop" 150 --date "$M-04"
run 0 '' --add invest "Index fund" 200 --date "$M-03"
run 0 '' --add invest-value "Index fund" 1260 --date "$M-05"
run 0 '' --add invest-value "Crypto coins" 400 --date "$M-04"
run 0 '' --add invest-withdraw "Crypto coins" 50 --date "$M-05"
run 0 '' --add expense typo 999 --date "$M-02"
contains "$DIR/data/$M.csv" '6,'"$M"'-03,expense,,"snacks, drinks",2000'

# 5. a withdrawal over the balance is refused, and so is one over the value an investment has after its update
run $ERR_BALANCE '' --add saving-withdraw "New laptop" 100 --date "$M-05"
run $ERR_BALANCE '' --add invest-withdraw "Crypto coins" 351 --date "$M-06"

# 6. fix mistakes before closing: edit (enter keeps a field), delete (with confirmation), rename and delete categories
run 0 '\n\n65\n\n' --edit 5
run 0 'n\n' --delete 15
contains "$DIR/data/$M.csv" "typo"
run 0 'y\n' --delete 15
lacks "$DIR/data/$M.csv" "typo"
run $ERR_ENTRY '' --edit 15
run 0 '' --categories --rename "New laptop" "Laptop fund"
contains "$DIR/data/$M.csv" "saving-withdraw,Laptop fund"
run 0 '' --categories --add investment "Old box"
run 0 'y\n' --categories --delete "Old box"
run $ERR_CATEGORY 'y\n' --categories --delete "Emergency fund"
run 0 '' --list
run 0 '' --summary

# 6b. the menu's add loop: many entries of one type in a row, done goes back to the list of types (not the main menu),
#     the date of each entry defaults to the previous one, and a refused withdrawal doesn't end the loop
mkdir "$DIR/loop"
LOOP_INPUT='\nn\nPiggy bank\n100\n\n\n'             # wizard: no opening cash, one saving category, no investments
LOOP_INPUT="$LOOP_INPUT"'1\n2\ncoffee\n3.50\n03\nlunch\n12\n\n\n'  # Add entries > Expense: two items, the 2nd keeps day 03, done
LOOP_INPUT="$LOOP_INPUT"'4\n1\n20\n04\nPiggy bank\n5\n\n\n'      # Saving deposit by number and by name, done
LOOP_INPUT="$LOOP_INPUT"'5\n1\n500\n05\n\n'                      # Saving withdrawal over the balance: refused, still in the loop
LOOP_INPUT="$LOOP_INPUT"'9\n8\n'                                    # back to the main menu, quit
BOOKKEEPING_DIR="$DIR/loop" run 0 "$LOOP_INPUT"
contains "$DIR/loop/data/$M.csv" "1,$M-03,expense,,coffee,350"
contains "$DIR/loop/data/$M.csv" "2,$M-03,expense,,lunch,1200"
contains "$DIR/loop/data/$M.csv" "3,$M-04,saving,Piggy bank,,2000"
contains "$DIR/loop/data/$M.csv" "4,$M-04,saving,Piggy bank,,500"
lacks "$DIR/loop/data/$M.csv" "saving-withdraw"
[ "$(grep -c "What do you want to record?" "$OUT")" -eq 4 ] || fail "the list of entry types was not shown again after each loop"

# 6c. every command has a short, a long and a word form; -d/--date/--date= for the date
contains "$DIR/loop/data/state.csv" "$M,0"   # the wizard answer "no opening cash" leaves it at 0
L="$DIR/loop"
BOOKKEEPING_DIR="$L" run 0 '' -a expense parking 4 -d 05
BOOKKEEPING_DIR="$L" run 0 '' --add income tips 10 --date=06
contains "$L/data/$M.csv" "5,$M-05,expense,,parking,400"
contains "$L/data/$M.csv" "6,$M-06,income,,tips,1000"
BOOKKEEPING_DIR="$L" run 0 '' -l
BOOKKEEPING_DIR="$L" run 0 '' --list
BOOKKEEPING_DIR="$L" run 0 '' -s
BOOKKEEPING_DIR="$L" run 0 '' --summary
BOOKKEEPING_DIR="$L" run 0 '\n\n7\n\n' -e 5
contains "$L/data/$M.csv" "5,$M-05,expense,,parking,700"
BOOKKEEPING_DIR="$L" run 0 'y\n' -r 6
lacks "$L/data/$M.csv" "tips"
BOOKKEEPING_DIR="$L" run $ERR_ENTRY '' --delete 99
BOOKKEEPING_DIR="$L" run 0 '' -c -a investment "Gold bar" 100
BOOKKEEPING_DIR="$L" run 0 '' --categories --rename "Gold bar" Gold
BOOKKEEPING_DIR="$L" run 0 '' -c -o Gold 120
contains "$L/data/categories.csv" "investment,Gold,12000"
BOOKKEEPING_DIR="$L" run 0 '' -o -50
contains "$L/data/state.csv" "$M,-5000"
BOOKKEEPING_DIR="$L" run 0 '' --opening-cash 0
BOOKKEEPING_DIR="$L" run $ERR_USAGE '' -x
BOOKKEEPING_DIR="$L" run $ERR_USAGE '' -a expense food 5 -d

# 6d. after every investment entry the value and the gain so far are printed
BOOKKEEPING_DIR="$L" run 0 '' -a invest Gold 10 -d 04
contains "$OUT" "Gold is now worth \$130.00, gain/loss so far this month: \$0.00 (+0.00%)"
contains "$OUT" "No value update this month yet"
BOOKKEEPING_DIR="$L" run 0 '' -a invest-value Gold 150 -d 05
contains "$OUT" "Gold is now worth \$150.00, gain/loss so far this month: +\$20.00 (+16.13%)"

# 6e. the three help pages
"$BIN" -h >"$DIR/help-all.txt" 2>&1 || fail "-h failed"
"$BIN" --help flags >"$DIR/help-flags.txt" 2>&1 || fail "--help flags failed"
"$BIN" --help features >"$DIR/help-features.txt" 2>&1 || fail "help features failed"
contains "$DIR/help-all.txt" "COMMANDS AND FLAGS"
contains "$DIR/help-all.txt" "INVESTMENTS: HOW GAINS AND LOSSES ARE RECORDED"
contains "$DIR/help-flags.txt" "-a, --add <type>"
lacks "$DIR/help-flags.txt" "INVESTMENTS: HOW"
contains "$DIR/help-features.txt" "INVESTMENTS: HOW GAINS AND LOSSES ARE RECORDED"
lacks "$DIR/help-features.txt" "COMMANDS AND FLAGS"
run $ERR_USAGE '' -h bogus

# 7. the end of the input never hangs: at the menu it is a normal exit, in the middle of an edit it is ERR_EOF
run 0 ''
run $ERR_EOF '' --edit 1
run $ERR_EOF '\n' --edit 1

# 8. close: preview, confirmation, both reports
run 0 'n\n' --close
[ -e "$DIR/reports/${MM}_${YYYY}.md" ] && fail "answering n to the close still wrote a report"
run 0 'y\n' --close

PERSONAL="$DIR/reports/${MM}_${YYYY}_Personal_Report.md"
KEY="$DIR/reports/${MM}_${YYYY}.md"
[ -f "$PERSONAL" ] || fail "missing $PERSONAL"
[ -f "$KEY" ] || fail "missing $KEY"

if [ -f "$PERSONAL" ] && [ -f "$KEY" ]; then
    contains "$PERSONAL" "| Opening cash | \$1,000.00 | |"
    contains "$PERSONAL" "| Incomes | \$3,400.00 | 100.00% |"
    contains "$PERSONAL" "| Expenses | \$1,185.00 | 34.85% |"
    contains "$PERSONAL" "| Withdrawn from savings | \$150.00 | 4.41% |"
    contains "$PERSONAL" "| **Closing cash** | \$2,765.00 | |"
    contains "$PERSONAL" "| **Net worth at the end** | \$5,225.00 | |"
    contains "$PERSONAL" "| Net worth change | +\$2,025.00 | |"
    contains "$PERSONAL" "| 2 | food | \$200.00 | 16.88% |"
    contains "$PERSONAL" "| 3 | transport | \$65.00 | 5.49% |"
    contains "$PERSONAL" "| Emergency fund | \$500.00 | \$300.00 | \$0.00 | \$800.00 |"
    contains "$PERSONAL" "| Laptop fund | \$200.00 | \$0.00 | \$150.00 | \$50.00 |"
    contains "$PERSONAL" "| Index fund | \$1,000.00 | \$200.00 | \$0.00 | +\$60.00 | +5.36% | \$1,260.00 |"
    contains "$PERSONAL" "| Crypto coins | \$500.00 | \$0.00 | \$50.00 | -\$100.00 | -20.00% | \$350.00 |"
    contains "$PERSONAL" "| 6 | $M-03 | Expense | snacks, drinks | \$20.00 |"
    contains "$PERSONAL" "| 3 | $M-04 | Expense | Food | \$79.50 |"
    lacks "$PERSONAL" "typo"
    lacks "$PERSONAL" "Old box"

    contains "$KEY" "## Top expenses - \$1,185.00 (34.85% of incomes)"
    contains "$KEY" "| 1 | rent | \$900.00 | 75.95% |"
    contains "$KEY" "| 2 | food | \$200.00 | 16.88% |"
    lacks "$KEY" "snacks"
    contains "$KEY" "| Saved this month | \$300.00 | 8.82% |"
    contains "$KEY" "| Invested this month | \$200.00 | 5.88% |"
    contains "$KEY" "| Index fund | +\$60.00 | +5.36% |"
    contains "$KEY" "| Crypto coins | -\$100.00 | -20.00% |"
    contains "$KEY" "| **Total** | -\$40.00 | -2.47% |"
    contains "$KEY" "| Closing cash | \$2,765.00 |"
    contains "$KEY" "| **Net worth change** | +\$2,025.00 |"

    for old in Travels "Planned purchases" Emergencies "Real estate" Currencies Commodities Stocks; do
        lacks "$PERSONAL" "$old"
        lacks "$KEY" "$old"
    done
fi

# 9. the closing balances were carried forward from the csv files
contains "$DIR/data/categories.csv" "saving,Emergency fund,80000"
contains "$DIR/data/categories.csv" "investment,Crypto coins,35000"
contains "$DIR/data/state.csv" ",276500"

# 10. closing again is refused (the next month has not started), and so is any entry for the closed month
run $ERR_DATE 'y\n' --close
run $ERR_DATE '' --add expense food 5 --date "$M-10"

# 11. review bug 7: a real memory error in the middle of the wizard stops it, no later prompt runs and nothing is saved
RELEASE=${2:-}
case $RELEASE in
    ''|/*) ;;
    *) RELEASE=$(pwd)/$RELEASE ;;
esac
if [ -n "$RELEASE" ]; then
    mkdir "$DIR/oom"
    ( ulimit -v 60000; head -c 120000000 /dev/zero | tr '\0' a | BOOKKEEPING_DIR="$DIR/oom" timeout 20 "$RELEASE" >"$DIR/oom.log" 2>&1 )
    code=$?
    [ "$code" -eq 205 ] || fail "out of memory in the wizard exited with $code, expected 205 (ERR_MEMORY)"
    contains "$DIR/oom.log" "Could not allocate memory"
    lacks "$DIR/oom.log" "Opening cash"
    [ -e "$DIR/oom/data/state.csv" ] && fail "a memory error in the wizard left a data/state.csv"
fi

# 12. no ANSI escape codes when stdout is not a terminal
ESC=$(printf '\033')
if grep -q "$ESC" "$OUT"; then fail "the output contains ANSI escape codes although it is not a terminal"; fi

if [ "$FAILS" -ne 0 ]; then
    echo "e2e: $FAILS check(s) failed, the output of every command is in $OUT"
    [ -n "${E2E_DIR:-}" ] || cat "$OUT"
    exit 1
fi
echo "e2e: all checks passed"
