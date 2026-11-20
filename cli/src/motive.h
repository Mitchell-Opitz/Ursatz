#ifndef URSATZ_COMPOSER_CLI_MOTIVE_H
#define URSATZ_COMPOSER_CLI_MOTIVE_H

#include "commands.h"

/*
 * The `motive` subcommand: argv[1] is "motive". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the input cannot be loaded as a motive
 */
int run_motive(const CliArgs *args);

#endif
