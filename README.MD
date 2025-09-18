# Bookkeeping CLI Application

This is a C-based command-line bookkeeping application designed to help users track their monthly finances, including incomes, expenses, debts, savings, and investments. The program generates detailed reports for each month and maintains continuity by reading data from previous months.

---

## Features

- **Monthly Data Entry:**  
  Enter detailed lists of incomes, expenses, and debts for each month, with support for up to 100 items per category.

- **Savings & Investments Tracking:**  
  Track savings for specific goals (travels, planned purchases, emergencies) and investments (real estate, currencies, commodities, stocks).

- **Withdrawals from Savings:**  
  Record money withdrawn from savings for various purposes.
  > *Withdrawals from investments are not yet supported.*

- **Automatic Report Generation:**  
  Generates two types of reports for each month:
  - **Personal Report:** Detailed breakdown of all financial activities.
  - **Key Drivers Report:** Highlights top contributors in each category and provides percentage breakdowns.

- **Continuity:**  
  Reads previous month's data to maintain running totals for savings and investments.

- **User-Friendly Input:**  
  Prompts guide the user through all required data entry, with validation and error handling.

---

## Directory Structure

```
program/
├── include/
│   ├── errors.h
│   ├── finance.h
│   ├── helpers.h
│   └── struct.h
├── reports/
│   ├── 01_2024_Personal_Report.txt
│   ├── 01_2024.txt
│   └── ... (all your monthly reports)
├── src/
│   ├── errors.c
│   ├── finance.c
│   ├── helpers.c
│   └── main.c
```

> **Important:**  
> Before running the program for the first time, make sure to remove all example files from the `reports/` directory (or your chosen `BASE_PATH`). This ensures the program starts with a clean slate and works as expected for new users.

---

## How It Works

1. **Startup:**  
    The program checks for existing monthly report files in the directory specified by `BASE_PATH` (default: `reports/`) to determine if this is the user's first run or a continuation. You can change `BASE_PATH` in the code to use a different directory for your reports.

2. **User Prompts:**  
   - Enter the month and year for bookkeeping.
   - If not the first run, previous month's data is loaded for continuity.
   - Enter previous balances, savings, and investments if applicable.
   - Enter current month's incomes, expenses, debts, savings, investments, and any withdrawals from savings.

   > **Note:** Withdrawing money from **investments** is not yet supported. You can only record withdrawals from savings at this time.

3. **Data Validation:**  
   - Input is validated for correct format, length, and logical consistency.
   - Errors are reported with clear messages.

4. **Report Generation:**  
   - Two files are created in `reports/`:
     - `MM_YYYY_Personal_Report.txt`: Full breakdown.
     - `MM_YYYY.txt`: Key drivers and summary.

---

## Usage

### Compilation

Compile the program using `gcc` (or any C99-compatible compiler):

```sh
gcc -o bookkeeping src/*.c -Iinclude
```

### Running

Run the program from the root directory:

```sh
./bookkeeping
```

Follow the on-screen prompts to enter your financial data for the month.

---

## Input Details

- **Incomes, Expenses, Debts:**  
  Enter as many items as needed (up to 100 per category). For each, provide a name and value.

- **Savings & Investments:**  
  Enter amounts for each subcategory. If unchanged, enter `0` or press enter.

- **Withdrawals:**  
  Specify amounts withdrawn from each savings subcategory.

- **Previous Data:**  
  On first run, you may enter previous balances, savings, and investments to initialize the system.

---

## Output

- **Personal Report (`MM_YYYY_Personal_Report.txt`):**  
  - Lists all incomes, expenses, debts, savings, investments, and final balance.
  - Shows withdrawals from savings if any.

- **Key Drivers Report (`MM_YYYY.txt`):**  
  - Highlights TOP SIZE (default 3) items in incomes, expenses, and debts.
  - Shows percentage breakdowns for each category.
  - Summarizes savings and investments (add the previous one with the current one)

> **Note about percentages in the reports:**  
> - The percentage shown next to each item in incomes, expenses, or debts indicates the proportion that item represents within its own category (for example, an expense that is X% of total expenses).
> - The total for **expenses** and **debts** also includes a percentage showing how much they represent relative to the total **incomes**.

---

## Error Handling

- All errors (file access, memory allocation, invalid input) are reported with descriptive messages.
- The program ensures data integrity by validating all user input and file operations.

---

## Exit Codes

The program returns specific numeric codes to indicate its exit status. You can check the exit code in your terminal after the program finishes by running:

```sh
echo $?
```

| Exit Code | Macro Name        | Description                                      |
|-----------|------------------|--------------------------------------------------|
| 0         | (Success)        | Success (no error)                               |
| -51       | ERR_MEMORY       | Memory allocation error                          |
| -52       | ERR_FILE         | File not found or cannot open file               |
| -53       | ERR_DIC          | Dictionary file could not be opened              |
| -54       | ERR_YEAR         | Invalid or missing year input                    |
| -55       | ERR_CHOOSE       | Invalid menu or option selection                 |
| -56       | ERR_NUMBER_ITEMS | Exceeded maximum number of allowed items         |

> **Note:**  
> Since shell exit codes are unsigned 8-bit values, negative numbers will appear as their 256-based equivalent (e.g., `-51` appears as `205`).

Refer to [`include/errors.h`](include/errors.h) if you want to change their values.


## Customization

- **Maximum Items:**  
  Change `MAX_ITEMS` in [`src/main.c`](src/main.c) to allow more or fewer items per category.
- **Report Directory:**  
  Change `BASE_PATH` in [`src/helpers.c`](src/helpers.c) to use a different directory for reports.
- **Top Items:**  
  Change `TOP SIZE` in [`src/helpers.h`](include/helpers.h) to show a different amount of top items per category.

---

## File Reference

- **Main Logic:** [`src/main.c`](src/main.c)
- **Finance Functions:** [`src/finance.c`](src/finance.c)
- **Helpers & Utilities:** [`src/helpers.c`](src/helpers.c)
- **Error Handling:** [`src/errors.c`](src/errors.c)
- **Data Structures:** [`include/struct.h`](include/struct.h)

---

## Example Reports

See the `reports/` directory for sample output files.

---

## License

This project is provided as-is for personal bookkeeping and educational purposes.

---