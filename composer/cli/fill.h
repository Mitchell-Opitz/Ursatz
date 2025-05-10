#ifndef URSATZ_COMPOSER_CLI_FILL_H
#define URSATZ_COMPOSER_CLI_FILL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/piece_plan.h>

#define FILL_USAGE_LINE                                                                                                \
    "Usage: ursatz-composer fill <plan.json> [--seed N] [--check] [--candidates N] [--style <file>] [--style-weight N/D] [--blend <file>] [--blend-weight N/D | --blend-weights D=N/D,...] [--out <plan.json>] [--midi <file>] [--musicxml <file>] "       \
    "[--record <file.json>] [--format text|json]"

/* "ursatz-composer fill ..."; argv[1] is "fill". Returns the exit status. */
int run_fill(int argc, char **argv);

/*
 * What the commands that compose a plan (fill, regenerate) share. cli_load_plan_text reads a plan file and gives its
 * JSON text, so each seed can start from a new copy; on failure it prints the error and returns the exit status.
 */
int cli_load_plan_text(const char *path, char **text);

/* The targets left open and why, for a plan no seed from first to last completed. */
void cli_report_stuck(const PiecePlan *plan, const ComposeOutcome *outcome, unsigned long first, unsigned long last);

/* The files a run writes; a NULL path is not written. */
typedef struct {
    const char *plan;
    const char *midi;
    const char *musicxml;
    const char *record;
} CliOutputPaths;

#define CLI_MAX_OUTPUTS 4

/*
 * Writes the files, all or none, then gives the paths written in files (room for CLI_MAX_OUTPUTS). Returns 0, or the
 * exit status of the error it printed.
 */
int cli_write_outputs(const CliOutputPaths *paths, const PiecePlan *plan, const ComposeOutcome *outcome,
                      const char **files, size_t *file_count);

/*
 * Prints what fill prints for a composed plan: the seed, the form and cadences heard in each section, the returns
 * heard, and the files written. json_extra, when not NULL, is members of the JSON object (no braces) put after the seed.
 */
void cli_print_composed(bool json, unsigned long seed, const PiecePlan *plan, const ComposeOutcome *outcome,
                        const char *json_extra, const char *const *files, size_t file_count);

#endif
