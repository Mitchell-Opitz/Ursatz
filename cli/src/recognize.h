#ifndef URSATZ_COMPOSER_CLI_RECOGNIZE_H
#define URSATZ_COMPOSER_CLI_RECOGNIZE_H

#include "commands.h"

/*
 * The `recognize` subcommand: argv[1] is "recognize". Returns the process exit status:
 *   0  the search ran, whether or not it found a chain
 *   1  usage, an invalid argument, or an input that cannot be loaded as a motive
 */
int run_recognize(const CliArgs *args);

#endif
