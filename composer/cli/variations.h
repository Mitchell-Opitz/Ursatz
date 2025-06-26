#ifndef URSATZ_COMPOSER_CLI_VARIATIONS_H
#define URSATZ_COMPOSER_CLI_VARIATIONS_H

#define VARIATIONS_USAGE_LINE                                                                                          \
    "Usage: ursatz-composer variations <plan.json> (--node <id> | --section <letter>) [--lane "                        \
    "harmony|bass|melody|inner] --count N [--seed N] --out-dir <dir> [--midi] [--musicxml] [--format text|json]"

#define HARMONIZE_USAGE_LINE                                                                                           \
    "Usage: ursatz-composer harmonize <plan.json> --section <letter> --count N [--seed N] --out-dir <dir> [--midi] "   \
    "[--musicxml] [--format text|json]"

/* "ursatz-composer variations ..."; argv[1] is the command. Returns the exit status. */
int run_variations(int argc, char **argv);
/* "ursatz-composer harmonize ..."; the same output, for the harmonizations of a given melody. */
int run_harmonize(int argc, char **argv);

#endif
