#ifndef URSATZ_COMPOSER_CLI_THEME_H
#define URSATZ_COMPOSER_CLI_THEME_H

/*
 * The `theme` subcommand: argv[1] is "theme". Returns the process exit status, as grow's:
 *   0  success
 *   1  usage or an invalid request (a request that does not read, a rejected form)
 *   2  the request cannot be satisfied (a motive that cannot be used, no seed works)
 *   3  an output file could not be written
 */
int run_theme(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define THEME_USAGE_LINE                                                                                          \
    "       ursatz-composer theme <request.json> [--seed <n>] [--suggest <n>] [--check] [--out <file.json>] "     \
    "[--midi <file>] [--musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]"

#endif
