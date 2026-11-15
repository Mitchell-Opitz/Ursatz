#ifndef URSATZ_COMPOSER_CLI_FILL_H
#define URSATZ_COMPOSER_CLI_FILL_H

#define FILL_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer fill <plan.json> [--seed N] [--check] [--candidates N] [--style <file>] [--style-weight N/D] [--style-without <category>]... [--blend <file>] [--blend-weight N/D | --blend-weights D=N/D,...] [--out <plan.json>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] "       \
    "[--record <file.json>] [--explain <file>] [--format text|json]"

/* "ursatz-composer fill ..."; argv[1] is "fill". Returns the exit status. */
int run_fill(int argc, char **argv);

#endif
