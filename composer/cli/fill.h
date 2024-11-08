#ifndef URSATZ_COMPOSER_CLI_FILL_H
#define URSATZ_COMPOSER_CLI_FILL_H

#define FILL_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer fill <plan.json> [--seed N] [--out <plan.json>] [--midi <file>] [--musicxml <file>] "       \
    "[--record <file.json>] [--format text|json]"

/* "ursatz-composer fill ..."; argv[1] is "fill". Returns the exit status. */
int run_fill(int argc, char **argv);

#endif
