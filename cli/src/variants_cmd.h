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

/* The line of the usage text that names the subcommand. */
#define VARIANTS_USAGE_LINE                                                                                          \
    "       ursatz variants --list-ops [--format <text|json>]\n"                                                          \
    "       ursatz variants <motive-input> [--input-format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] " \
    "[--form <full|rhythm|contour>] [--meter <n/m>] [--key <key>] [--pickup <beats>] [--harmony \"<text>\"] [--feature <label:first-last>]... [--role <melodic|accompaniment>] [--chain <file.json>] "         \
    "[--each-op] [--distance <close|medium|far>] [--function <name>]... [--basic <file>] [--basic-format <format>] [--basic-pickup <beats>] [--basic-harmony \"<text>\"] [--count <n>] [--seed <n>] [--format <text|json>]"

#endif
