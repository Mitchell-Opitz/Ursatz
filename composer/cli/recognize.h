#ifndef URSATZ_COMPOSER_CLI_RECOGNIZE_H
#define URSATZ_COMPOSER_CLI_RECOGNIZE_H

/*
 * The `recognize` subcommand: argv[1] is "recognize". Returns the process exit status:
 *   0  the search ran, whether or not it found a chain
 *   1  usage, an invalid argument, or an input that cannot be loaded as a motive
 */
int run_recognize(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define RECOGNIZE_USAGE_LINE                                                                                           \
    "       ursatz-composer recognize <source> <target> [--format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] " \
    "[--form <full|rhythm|contour>] --meter <n/m> [--key <key>] [--pickup <beats>] [--harmony \"<text>\"] "              \
    "[--feature <label:first-last>]... [--role <melodic|accompaniment>] [--target-format <format>] "                      \
    "[--target-pickup <beats>] [--target-harmony \"<text>\"] [--target-feature <label:first-last>]... "                   \
    "[--depth <n>] [--json]"

#endif
