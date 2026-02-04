#ifndef URSATZ_COMPOSER_CLI_RANDOM_MOTIVE_H
#define URSATZ_COMPOSER_CLI_RANDOM_MOTIVE_H

/*
 * The `random-motive` subcommand: argv[1] is "random-motive". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument, or a style file that cannot be read
 *   2  no motive could be made
 */
int run_random_motive(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define RANDOM_MOTIVE_USAGE_LINE                                                                                       \
    "       ursatz-composer random-motive --mode <major|minor> --meter <n/m> [--key <key>] "                         \
    "[--form <full|rhythm|contour>] [--role <melodic|accompaniment>] [--style <file>] [--style-without <category>]... [--count <n>] [--seed <n>] [--json]"

#endif
