#ifndef URSATZ_COMPOSER_CLI_VARIANTS_CMD_H
#define URSATZ_COMPOSER_CLI_VARIANTS_CMD_H

#include "commands.h"

/*
 * The `variants` subcommand: argv[1] is "variants". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument, or a chain file that cannot be read
 *   2  the input cannot be loaded as a motive, or a variant cannot be made as asked
 */
int run_variants(const CliArgs *args);

#endif
