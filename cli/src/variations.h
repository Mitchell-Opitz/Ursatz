#ifndef URSATZ_COMPOSER_CLI_VARIATIONS_H
#define URSATZ_COMPOSER_CLI_VARIATIONS_H

#include "commands.h"

#define VARIATIONS_USAGE_LINE                                                                                          \
    "Usage: ursatz variations <plan.json> (--node <id> | --section <letter>) [--lane "                        \
    "harmony|bass|melody|inner] --count N [--seed N] --out-dir <dir> [--write-midi] [--write-musicxml] [--tempo <bpm>] [--plain-midi] [--format text|json]"

#define HARMONIZE_USAGE_LINE                                                                                           \
    "Usage: ursatz harmonize <plan.json> --section <letter> --count N [--seed N] --out-dir <dir> [--write-midi] "   \
    "[--write-musicxml] [--tempo <bpm>] [--plain-midi] [--format text|json]"

/* "ursatz variations ..."; argv[1] is the command. Returns the exit status. */
int run_variations(const CliArgs *args);
/* "ursatz harmonize ..."; the same output, for the harmonizations of a given melody. */
int run_harmonize(const CliArgs *args);

#endif
