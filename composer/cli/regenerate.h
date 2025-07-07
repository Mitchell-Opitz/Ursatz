#ifndef URSATZ_COMPOSER_CLI_REGENERATE_H
#define URSATZ_COMPOSER_CLI_REGENERATE_H

#define LOCK_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer lock <plan.json> (--node <id> | --section <letter>) [--lane harmony|bass|melody|inner] "   \
    "--out <plan.json> [--format text|json]"

#define REGENERATE_USAGE_LINE                                                                                          \
    "Usage: ursatz-composer regenerate <plan.json> (--node <id> | --section <letter>) [--lane "                        \
    "harmony|bass|melody|inner] [--seed N] --out <plan.json> [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format text|json]"

/* "ursatz-composer lock ..." and "ursatz-composer regenerate ..."; argv[1] is the command. Return the exit status. */
int run_lock(int argc, char **argv);
int run_regenerate(int argc, char **argv);

#endif
