#ifndef URSATZ_COMPOSER_CLI_GROW_H
#define URSATZ_COMPOSER_CLI_GROW_H

#include "commands.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * The `grow` subcommand: argv[1] is "grow". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied (the motive cannot be loaded or is too long, no seed works)
 *   3  an output file could not be written
 */
int run_grow(const CliArgs *args);

#endif
