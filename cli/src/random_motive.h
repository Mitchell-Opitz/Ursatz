#ifndef URSATZ_COMPOSER_CLI_RANDOM_MOTIVE_H
#define URSATZ_COMPOSER_CLI_RANDOM_MOTIVE_H

#include "commands.h"

/*
 * The `random-motive` subcommand: argv[1] is "random-motive". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument, or a style file that cannot be read
 *   2  no motive could be made
 */
int run_random_motive(const CliArgs *args);

#endif
