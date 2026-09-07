#ifndef URSATZ_COMPOSER_CLI_PIECE_H
#define URSATZ_COMPOSER_CLI_PIECE_H

/*
 * The `piece` subcommand: argv[1] is "piece". Composes a looping game track from one JSON request (schema
 * ursatz-piece-request, version 1; docs/piece-request.md): the loop version, and the ending version built from it
 * (R-172, R-173). Returns the process exit status, as theme's:
 *   0  success
 *   1  usage or an invalid request (a request that does not read, a structure that is not one)
 *   2  the request cannot be satisfied (a motive that cannot be used, no seed works)
 *   3  an output file could not be written
 */
int run_piece(int argc, char **argv);

/* The line of the usage text that names the subcommand. */
#define PIECE_USAGE_LINE                                                                                          \
    "       ursatz-composer piece <request.json> [--seed <n>] [--check] [--out <loop-plan.json>] "                \
    "[--ending-out <ending-plan.json>] [--midi <loop.mid>] [--musicxml <loop.musicxml>] [--ending-midi <file>] " \
    "[--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]"

/* The line that regenerates a section of a loop plan the command wrote. */
#define PIECE_REGENERATE_USAGE_LINE                                                                               \
    "       ursatz-composer piece --from <loop-plan.json> --request <request.json> --regenerate <section id>|none " \
    "[--lane harmony|bass|melody|inner|all] [--seed <n>] [--check] [--out <loop-plan.json>] [--ending-out <file>] " \
    "[--midi <file>] [--musicxml <file>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] " \
    "[--explain <file>] [--format text|json]"

/* The line that makes variations of a section of a loop plan. */
#define PIECE_VARIATIONS_USAGE_LINE                                                                               \
    "       ursatz-composer piece --from <loop-plan.json> --request <request.json> --variations <section id> "     \
    "--lane harmony|bass|melody|inner|all --count <1..8> --out-dir <dir> [--seed <n>] [--check] [--plain-midi] "  \
    "[--format text|json]"

#endif
