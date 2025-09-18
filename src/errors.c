#include <stdio.h> 

#include "../include/errors.h"
#include "../include/helpers.h"

void dic_not_open(void)
{
    fprintf(stderr, "ERROR: Could not open \"%s\" directory.\n", BASE_PATH);
}

void err_year(void)
{
    fprintf(stderr, "ERROR: There is no year before the year 0000\n"
            "And not, we won't work with years Before the Common Era.\n");
}

void previous_file_not_open(char *previous_file_name, int month, int year)
{
    fprintf(stderr, "ERROR: Could not open the file \"%s\", it either does not exists or you don't have the permissions to access.\n"
                    "Remeber that the program found a file with the structure \"MM_YYYY.txt\" in your current directory.\n"
                    "Therefore, the program tried to access to the month before the one you enter (\"%02d_%04d\")"
                    " in order to take some data and use it for this month.\n"
                    "It failed, so please make sure to always enter consecutive months.\n", previous_file_name, month, year);
}

void file_empty(char *previous_file_name, int month, int year)
{
    fprintf(stderr, "\nERROR: The file \"%s\" is empty, please make sure to have in the current directory only files created for this program.\n"
                    "Remeber that the program found a file with the structure \"MM_YYYY.txt\" in your current directory.\n"
                    "Therefore, the program tried to access to the month before the one you enter (\"%02d_%04d\")"
                    " in order to take some data and use it for this month.\n"
                    "But the file is empty, either because someone delete the information or the file was not created for this program.\n"
                    "Please make sure that the month you enter is after the last month you created here.\n", previous_file_name, month, year);
}

void missing_values(char *previous_file_name)
{
    fprintf(stderr, "\nERROR: The file program did not found one of the neccessaries values in the file \"%s\"\n"
                    "Remeber that those atributes are esential to keep track of some values like savings or investment\n"
                    "and reflect them in the following months. Since the program did not found them\n"
                    "the file was either modified of it was not created by this program.\n"
                    "Either way, please make sure that the month you enter is after the last month you created here.\n", previous_file_name);
}

void wrong_chose(char *choose)
{
    fprintf(stderr, "\nERROR: wrong input. We asked you to either enter \"y\" or \"n\" but you entered \"%s\"\n", choose);
}

void memory_error(void)
{
    fprintf(stderr, "\nERROR: Could not allocate memory.\n");
}

void file_not_open(char *curren_file_name)
{
    fprintf(stderr, "ERROR: Could not open \"%s\" file, either you don't have the permissions to create new files or the program doesn't.\n", curren_file_name);
}

void written_error(void)
{
    fprintf(stderr, "ERROR: Could not write the information inside the file. It might be due to insufficient memory or a lack of permissions.\n");
}

void close_file_error(char *file_name)
{
    fprintf(stderr, "ERROR: Could not close the file \"%s\". It might be due to insufficient memory.”\n", file_name);
}

void add_item_error(size_t MAX_ITEMS)
{
    fprintf(stderr, "You can only add at maximum of %zu items.\n", MAX_ITEMS);
}