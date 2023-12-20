#ifndef URSATZ_COMPOSER_CLI_DEVELOP_H
#define URSATZ_COMPOSER_CLI_DEVELOP_H

/*
 * The `develop` subcommand: argv[1] is "develop". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the input cannot be loaded as a motive, or nothing can be developed from it as asked
 *   3  an output file could not be written
 */
int run_develop(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define DEVELOP_USAGE_LINE                                                                                    \
    "       ursatz-composer develop <motive-input> --op <restate|invert|retrograde|interval|fragment|"       \
    "liquidate|augment|diminish> [--seed <n>] [--suggest <n>] [--meter <n/m>] [--key <key>] [options]"

#endif
