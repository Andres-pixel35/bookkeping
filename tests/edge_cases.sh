#!/bin/sh
# Edge cases: a change of year (December -> January) and a leap February, then every kind of wrong input:
# unknown flags and commands, missing or extra arguments, invalid amounts, dates, names, IDs and categories,
# wrong answers in the menu and the wizard, the end of the input in the middle of an action, a missing data
# directory and corrupted data files. Every refused command must exit with its documented code and leave the
# data files exactly as they were.
#
# usage: sh tests/edge_cases.sh path/to/bookkeeping

set -u

BIN=${1:?usage: sh tests/edge_cases.sh path/to/bookkeeping}
case $BIN in
    /*) ;;
    *) BIN=$(pwd)/$BIN ;;
esac

ROOT=$(mktemp -d)
trap 'rm -rf "$ROOT"' EXIT
unset NO_COLOR

OUT="$ROOT/output.log"   # everything every command printed
LAST="$ROOT/last.log"    # what the last command printed
FAILS=0
COMMANDS=0
CHECKS=0

# shells see the negative ERR_* codes modulo 256
ERR_FILE=204
ERR_DIC=203
ERR_EOF=199
ERR_USAGE=198
ERR_DATE=197
ERR_BALANCE=196
ERR_CATEGORY=195
ERR_REPORT_EXISTS=194
ERR_ENTRY=193

fail()
{
    echo "FAIL: $*"
    FAILS=$((FAILS + 1))
}

# run <expected exit code> <stdin text> <arguments...> in $BOOKKEEPING_DIR; timeout catches endless loops (124)
run()
{
    expected=$1
    input=$2
    shift 2
    printf '%b' "$input" | timeout 10 "$BIN" "$@" >"$LAST" 2>&1
    code=$?
    COMMANDS=$((COMMANDS + 1))
    CHECKS=$((CHECKS + 1))
    cat "$LAST" >>"$OUT"
    [ "$code" -eq "$expected" ] || fail "bookkeeping $* exited with $code, expected $expected"
}

# a checksum of every data file, to prove that a refused command changed nothing
snapshot()
{
    cat "$BOOKKEEPING_DIR"/data/* 2>/dev/null | cksum
}

# refuse <expected exit code> <stdin text> <arguments...>: the command fails and the data files stay identical
refuse()
{
    before=$(snapshot)
    run "$@"
    shift 2
    CHECKS=$((CHECKS + 1))
    [ "$(snapshot)" = "$before" ] || fail "bookkeeping $* was refused but changed the data files"
}

said()
{
    CHECKS=$((CHECKS + 1))
    grep -F -q -- "$1" "$LAST" || fail "the output does not contain: $1"
}

contains()
{
    CHECKS=$((CHECKS + 1))
    grep -F -q -- "$2" "$1" || fail "$(basename "$1") does not contain: $2"
}

# ================================================================ 1. change of year and leap year
export BOOKKEEPING_DIR="$ROOT/years"
mkdir "$BOOKKEEPING_DIR"

# December 2023: opening cash 1,000, a saving and an investment
run 0 '2023-12\ny\ny\n1000\nRainy day\n200\n\nShares\n1000\n\n8\n'
contains "$BOOKKEEPING_DIR/data/state.csv" "2023-12,100000"
run 0 '' -a income salary 2000 -d 1
run 0 '' -a expense rent 500 -d 2023-12-31                         # the 31st exists in December
refuse $ERR_DATE '' -a expense food 5 -d 2024-01-01                # the next year is not open yet
refuse $ERR_USAGE '' -a expense food 5 -d 32
run 0 '' -a saving "Rainy day" 100 -d 15
run 0 '' -a invest-value Shares 1100 -d 31
run 0 'y\n' -C
said "The open month is now 2024-01."
contains "$BOOKKEEPING_DIR/data/state.csv" "2024-01,240000"        # 1,000 + 2,000 - 500 - 100
contains "$BOOKKEEPING_DIR/data/categories.csv" "saving,Rainy day,30000"
contains "$BOOKKEEPING_DIR/data/categories.csv" "investment,Shares,110000"
contains "$BOOKKEEPING_DIR/reports/12_2023_Personal_Report.md" "# Personal Finance Report - December 2023"
contains "$BOOKKEEPING_DIR/reports/12_2023_Personal_Report.md" "**Period:** 2023-12-01 to 2023-12-31"
contains "$BOOKKEEPING_DIR/reports/12_2023_Personal_Report.md" "| 2 | 2023-12-31 | Expense | rent | \$500.00 |"
contains "$BOOKKEEPING_DIR/reports/12_2023.md" "| Shares | +\$100.00 | +10.00% |"

# January 2024: December is closed for good, the new year continues from its balances
refuse $ERR_DATE '' -a expense food 5 -d 2023-12-20
run 0 '' -a income salary 2000 -d 2024-01-31
run 0 '' -a expense rent 500 -d 1
run 0 '' -a invest Shares 100 -d 10
run 0 'y\n' --close
contains "$BOOKKEEPING_DIR/data/state.csv" "2024-02,380000"        # 2,400 + 2,000 - 500 - 100
contains "$BOOKKEEPING_DIR/data/categories.csv" "investment,Shares,120000"
contains "$BOOKKEEPING_DIR/reports/01_2024_Personal_Report.md" "| Opening cash | \$2,400.00 | |"
contains "$BOOKKEEPING_DIR/reports/01_2024_Personal_Report.md" "| Net worth at the start | \$3,800.00 | |"

# February 2024 is a leap February: the 29th exists, the 30th doesn't
run 0 '' -a expense food 10 -d 2024-02-29
run 0 '' -a expense food 10 -d 29
refuse $ERR_USAGE '' -a expense food 10 -d 2024-02-30
refuse $ERR_USAGE '' -a expense food 10 -d 30
run 0 'y\n' close
contains "$BOOKKEEPING_DIR/reports/02_2024_Personal_Report.md" "**Period:** 2024-02-01 to 2024-02-29"
contains "$BOOKKEEPING_DIR/reports/02_2024_Personal_Report.md" "| 1 | food | \$20.00 | 100.00% |"
contains "$BOOKKEEPING_DIR/data/state.csv" "2024-03,378000"

# an existing report blocks the close and nothing is written
echo "not ours" >"$BOOKKEEPING_DIR/reports/03_2024.md"
refuse $ERR_REPORT_EXISTS 'y\n' -C
said "already exists and reports are never overwritten"
contains "$BOOKKEEPING_DIR/reports/03_2024.md" "not ours"
[ -e "$BOOKKEEPING_DIR/reports/03_2024_Personal_Report.md" ] && fail "a report was written although the close was refused"

# ================================================================ 2. wrong answers in the wizard and the menus
export BOOKKEEPING_DIR="$ROOT/errors"
mkdir "$BOOKKEEPING_DIR"
M=$(date +%Y-%m)
LONG=$(printf '%051d' 0)    # 51 characters, one more than a name may have

WIZARD='2026-13\n26-10\n0000-05\nabc\n\n'           # bad months, then enter: the current month
WIZARD="$WIZARD"'maybe\ny\nx\ny\n'                  # bad y/n answers
WIZARD="$WIZARD"'1.234\n-5\nabc\n\n0\n500\n'        # bad amounts, then 500
WIZARD="$WIZARD$LONG"'\n lead\nFund A\n1,000\n100\nfund a\n\n'   # bad names, bad amount, a duplicate, done
WIZARD="$WIZARD"'Stocks\n50\n\n'
WIZARD="$WIZARD"'0\n9\nabc\n\n1\n0\n10\n9\n8\n'     # bad options in the main menu and in the list of types
run 0 "$WIZARD"
said "You must write the number of a month (1-12)"
said "You must write the month as \"YYYY-MM\""
said "There is no year before the year 0000"
said "We asked you to either enter \"y\" or \"n\" but you entered \"maybe\""
said "\"1.234\" is not a valid amount: money only has two decimals"
said "\"-5\" is not a valid amount"
said "You must write a value greater than 0 per each item."
said "more than 50 characters"
said "can not start the name of your item with a space"
said "A category called \"fund a\" already exists"
[ "$(grep -c 'between 1 and 8 but you entered' "$LAST")" -eq 4 ] || fail "the main menu did not refuse its 4 wrong options"
[ "$(grep -c 'between 1 and 9 but you entered' "$LAST")" -eq 2 ] || fail "the list of types did not refuse its 2 wrong options"
contains "$BOOKKEEPING_DIR/data/state.csv" "$M,50000"
contains "$BOOKKEEPING_DIR/data/categories.csv" "saving,Fund A,10000"
contains "$BOOKKEEPING_DIR/data/categories.csv" "investment,Stocks,5000"

run 0 '' -a income salary 1000 -d 1
run 0 '' -a saving-withdraw "Fund A" 90 -d 2

# ================================================================ 3. flags, commands and arguments that don't exist or don't fit
refuse $ERR_USAGE '' -x
refuse $ERR_USAGE '' --bogus
refuse $ERR_USAGE '' -A
refuse $ERR_USAGE '' --ADD expense food 5
refuse $ERR_USAGE '' bogus
refuse $ERR_USAGE '' ''
refuse $ERR_USAGE '' --
said "Usage: bookkeeping"
refuse $ERR_USAGE '' -l extra
refuse $ERR_USAGE '' -s extra
refuse $ERR_USAGE '' -C extra
refuse $ERR_USAGE '' -o
refuse $ERR_USAGE '' -h bogus
refuse $ERR_USAGE '' -h flags extra
run 0 '' help

# IDs
refuse $ERR_USAGE '' -e
refuse $ERR_USAGE '' -e abc
refuse $ERR_USAGE '' -e 0
refuse $ERR_USAGE '' -e -1
refuse $ERR_USAGE '' -e 1.5
refuse $ERR_USAGE '' -r 1 2
refuse $ERR_ENTRY '' -e 99
refuse $ERR_ENTRY '' --delete 99
said "There is no entry with the ID 99"

# add: missing, extra and invalid arguments
refuse $ERR_USAGE '' -a
refuse $ERR_USAGE '' -a expense
refuse $ERR_USAGE '' -a expense food
refuse $ERR_USAGE '' -a expense food 5 extra
refuse $ERR_USAGE '' -a lunch food 5
said "\"lunch\" is not an entry type"
for amount in 1.234 -5 0 1,000 abc '' . 1e5 '5 ' 1234567890123456; do
    refuse $ERR_USAGE '' -a expense food "$amount" -d 1
done
refuse $ERR_USAGE '' -a expense '' 5 -d 1
refuse $ERR_USAGE '' -a expense ' lead' 5 -d 1
refuse $ERR_USAGE '' -a expense "$LONG" 5 -d 1
refuse $ERR_USAGE '' -a expense "$(printf 'a\tb')" 5 -d 1
refuse $ERR_USAGE '' -a expense food 5 -d
refuse $ERR_USAGE '' -a expense food 5 --date=
for date in '' 0 2026-02-30 14th 2026/10/14 99999; do
    refuse $ERR_USAGE '' -a expense food 5 -d "$date"
done
refuse $ERR_DATE '' -a expense food 5 -d 1999-01-01
said "is outside the open month"

# categories that don't exist or are of the other kind
refuse $ERR_CATEGORY '' -a saving Nope 5 -d 1
refuse $ERR_CATEGORY '' -a saving Stocks 5 -d 1
refuse $ERR_CATEGORY '' -a invest "Fund A" 5 -d 1
said "There is no investment category called \"Fund A\""

# withdrawals over the balance (Fund A has 10.00 left, Stocks is worth 50.00)
refuse $ERR_BALANCE '' -a saving-withdraw "Fund A" 10.01 -d 3
refuse $ERR_BALANCE '' -a invest-withdraw Stocks 50.01 -d 3
refuse $ERR_BALANCE '' -a saving-withdraw "Fund A" 10.01 -d 1       # on day 1 there are 100, but the 90 of day 2 needs them
said "the withdrawal of \"\$90.00\" on $M-02 needs it"
said "at that point there would only be: \"\$89.99\""
refuse $ERR_BALANCE '' -c -o "Fund A" 50                            # the withdrawal of 90 relied on the 100

# categories command
refuse $ERR_USAGE '' -c bogus
refuse $ERR_USAGE '' -c -a
refuse $ERR_USAGE '' -c -a gold Bar
refuse $ERR_CATEGORY '' -c -a saving "FUND a"
refuse $ERR_USAGE '' -c -a saving "$LONG"
refuse $ERR_USAGE '' -c -a saving "$(printf 'a\nb')"
refuse $ERR_USAGE '' -c -a saving Gold 1.234
refuse $ERR_USAGE '' -c -a saving Gold -5
refuse $ERR_CATEGORY '' -c -n Nope Other
refuse $ERR_CATEGORY '' -c -n "Fund A" stocks
refuse $ERR_USAGE '' -c -n "Fund A" ''
refuse $ERR_CATEGORY 'y\n' -c -r Nope
refuse $ERR_CATEGORY 'y\n' -c -r "Fund A"                           # it has money and entries
refuse $ERR_CATEGORY 'y\n' -c -r Stocks                             # it has money
refuse $ERR_CATEGORY '' -c -o Nope 5
refuse $ERR_USAGE '' -c -o "Fund A" abc
refuse $ERR_USAGE '' -c -o "Fund A" -5

# opening cash
refuse $ERR_USAGE '' -o abc
refuse $ERR_USAGE '' -o --5
refuse $ERR_USAGE '' -o ''
refuse $ERR_USAGE '' -o 1.234

# ================================================================ 4. wrong answers that get corrected, and the end of the input
run 0 'lunch\nincome\n\n1.234\n1200\n99\n1999-01-01\n\n' -e 1     # bad type, amount and dates, then valid ones
said "\"lunch\" is not an entry type"
contains "$BOOKKEEPING_DIR/data/$M.csv" "1,$M-01,income,,salary,120000"
refuse 0 'maybe\nn\n' -r 2                                          # a wrong answer, then no: nothing deleted
said "Nothing was deleted."
refuse $ERR_EOF '' -r 2                                             # the input ends at the confirmation
refuse $ERR_EOF 'income\n' -e 1                                     # the input ends in the middle of an edit
refuse $ERR_EOF '1\n2\nfood\n'                                      # ... and in the middle of a new entry
said "The input ended"
refuse $ERR_EOF '' -c -r "Fund A"
run 0 '' -c -a saving Empty
run 0 'y\n' -c -r Empty                                             # 0 and no entries: allowed

# ================================================================ 5. the data directory and its files
BOOKKEEPING_DIR="$ROOT/missing" run $ERR_DIC '' -l
BOOKKEEPING_DIR="$ROOT/$(printf '%0300d' 0)" run $ERR_DIC '' -l
[ -e "$ROOT/missing" ] && fail "a missing data directory was created"

# corrupted files: every one is refused with ERR_FILE and left exactly as it was
corrupt()
{
    file=$1
    content=$2
    rm -rf "$ROOT/bad"
    cp -r "$ROOT/errors" "$ROOT/bad"
    if [ "$content" = "<delete>" ]; then
        rm "$ROOT/bad/data/$file"
    else
        printf '%b' "$content" >"$ROOT/bad/data/$file"
    fi
    before=$(cat "$ROOT"/bad/data/* | cksum)
    BOOKKEEPING_DIR="$ROOT/bad" run $ERR_FILE '' -l
    [ "$(cat "$ROOT"/bad/data/* | cksum)" = "$before" ] || fail "a corrupted $file was changed"
}
corrupt state.csv 'bad,header\n2026-10,0\n'
corrupt state.csv 'open_month,opening_cash_cents\n2026-13,0\n'
corrupt state.csv 'open_month,opening_cash_cents\n2026-10,12.50\n'
corrupt state.csv ''
corrupt categories.csv 'kind,name,opening_balance_cents\ngold,Bar,0\n'
corrupt categories.csv 'kind,name,opening_balance_cents\nsaving,A,0\nsaving,a,0\n'
corrupt categories.csv 'kind,name,opening_balance_cents\nsaving,A,-1\n'
corrupt categories.csv '<delete>'
corrupt "$M.csv" "id,date,type,category,name,amount_cents\n1,1999-01-01,expense,,food,100\n"
corrupt "$M.csv" "id,date,type,category,name,amount_cents\n1,$M-01,expense,,\"food,100\n"
corrupt "$M.csv" "id,date,type,category,name,amount_cents\n1,$M-01,saving,Nope,,100\n"
corrupt "$M.csv" "id,date,type,category,name,amount_cents\n2,$M-01,expense,,a,1\n1,$M-01,expense,,b,1\n"
corrupt "$M.csv" "id,date,type,category,name,amount_cents\n1,$M-01,lunch,,food,100\n"

if [ "$FAILS" -ne 0 ]; then
    echo "edge cases: $FAILS check(s) failed"
    exit 1
fi
echo "edge cases: all checks passed ($COMMANDS commands, $CHECKS checks)"
