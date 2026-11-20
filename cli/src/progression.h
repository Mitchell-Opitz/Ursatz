#ifndef URSATZ_COMPOSER_CLI_PROGRESSION_H
#define URSATZ_COMPOSER_CLI_PROGRESSION_H

#include "commands.h"

/*
 * The `progression` subcommand: argv[1] is "progression". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied
 *   3  an output file could not be written
 */
int run_progression(const CliArgs *args);

#endif
