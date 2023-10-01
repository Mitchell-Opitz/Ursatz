#ifndef URSATZ_COMPOSER_CLI_PROGRESSION_H
#define URSATZ_COMPOSER_CLI_PROGRESSION_H

/*
 * The `progression` subcommand: argv[1] is "progression". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied
 *   3  an output file could not be written
 */
int run_progression(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define PROGRESSION_USAGE_LINE \
    "       ursatz-composer progression --key <key> --meter <n/m> --bars <n> --cadence <PAC|IAC|HC|DC|PC> [options]"

#endif
