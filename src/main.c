#include "../include/cli.h"

// main
// main only hands the arguments to the command line interface (src/cli.c), every ERR_* code it returns
// becomes the exit code of the program
int main(int argc, char **argv)
{
    return run_cli(argc, argv);
}
