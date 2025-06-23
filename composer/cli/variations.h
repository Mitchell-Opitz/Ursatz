#ifndef URSATZ_COMPOSER_CLI_VARIATIONS_H
#define URSATZ_COMPOSER_CLI_VARIATIONS_H

#define VARIATIONS_USAGE_LINE                                                                                          \
    "Usage: ursatz-composer variations <plan.json> (--node <id> | --section <letter>) [--lane "                        \
    "harmony|bass|melody|inner] --count N [--seed N] --out-dir <dir> [--midi] [--musicxml] [--format text|json]"

/* "ursatz-composer variations ..."; argv[1] is the command. Returns the exit status. */
int run_variations(int argc, char **argv);

#endif
