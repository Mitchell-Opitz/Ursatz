#ifndef URSATZ_COMPOSER_CLI_THEME_H
#define URSATZ_COMPOSER_CLI_THEME_H

#include "commands.h"

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "grow.h"

/*
 * The `theme` subcommand: argv[1] is "theme". Returns the process exit status, as grow's:
 *   0  success
 *   1  usage or an invalid request (a request that does not read, a rejected form)
 *   2  the request cannot be satisfied (a motive that cannot be used, no seed works)
 *   3  an output file could not be written
 */
int run_theme(const CliArgs *args);

#endif
