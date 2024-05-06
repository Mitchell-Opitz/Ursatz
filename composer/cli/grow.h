#ifndef URSATZ_COMPOSER_CLI_GROW_H
#define URSATZ_COMPOSER_CLI_GROW_H

/*
 * The `grow` subcommand: argv[1] is "grow". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied (the motive cannot be loaded or is too long, no seed works)
 *   3  an output file could not be written
 */
int run_grow(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define GROW_USAGE_LINE                                                                                          \
    "       ursatz-composer grow <motive-input> --into <sentence|period|hybrid> [--hybrid <beginning>+<ending>] " \
    "--key <key> --meter <n/m> [options]"

#endif
