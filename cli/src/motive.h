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

/* The line of the usage text that names the subcommand. */
#define MOTIVE_USAGE_LINE \
    "       ursatz motive <input> [--meter <n/m>] [--key <key>] [--input-format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] [--form <full|rhythm|contour>] [--pickup <beats>] [--harmony \"<text>\"] [--feature <label:first-last>]... [--role <melodic|accompaniment>] [--tone <n>=<role>]... [--render-key <key>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--format <text|json>]"

#endif
