#ifndef URSATZ_COMPOSER_CLI_VARIANTS_CMD_H
#define URSATZ_COMPOSER_CLI_VARIANTS_CMD_H

/*
 * The `variants` subcommand: argv[1] is "variants". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument, or a chain file that cannot be read
 *   2  the input cannot be loaded as a motive, or a variant cannot be made as asked
 */
int run_variants(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define VARIANTS_USAGE_LINE                                                                                          \
    "       ursatz-composer variants --list-ops [--json]\n"                                                          \
    "       ursatz-composer variants <motive-input> [--format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] " \
    "[--form <full|rhythm|contour>] [--meter <n/m>] [--key <key>] [--pickup <beats>] [--chain <file.json>] "         \
    "[--count <n>] [--seed <n>] [--json]"

#endif
