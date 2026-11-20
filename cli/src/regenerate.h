#ifndef URSATZ_COMPOSER_CLI_REGENERATE_H
#define URSATZ_COMPOSER_CLI_REGENERATE_H

#include "commands.h"

#define LOCK_USAGE_LINE                                                                                                \
    "Usage: ursatz lock <plan.json> (--node <id> | --section <letter>) [--lane harmony|bass|melody|inner] "   \
    "--out <plan.json> [--format text|json]"

#define REGENERATE_USAGE_LINE                                                                                          \
    "Usage: ursatz regenerate <plan.json> (--node <id> | --section <letter>) [--lane "                        \
    "harmony|bass|melody|inner] [--seed N] --out <plan.json> [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format text|json]"

/* "ursatz lock ..." and "ursatz regenerate ..."; argv[1] is the command. Return the exit status. */
int run_lock(const CliArgs *args);
int run_regenerate(const CliArgs *args);

#endif
