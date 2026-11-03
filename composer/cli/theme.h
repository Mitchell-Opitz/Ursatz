#ifndef URSATZ_COMPOSER_CLI_THEME_H
#define URSATZ_COMPOSER_CLI_THEME_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

#include "grow.h"

/*
 * The `theme` subcommand: argv[1] is "theme". Returns the process exit status, as grow's:
 *   0  success
 *   1  usage or an invalid request (a request that does not read, a rejected form)
 *   2  the request cannot be satisfied (a motive that cannot be used, no seed works)
 *   3  an output file could not be written
 */
int run_theme(int argc, char **argv);

/*
 * The request readers the `piece` command shares with `theme` (R-173). Each prints the usage error it found and fails
 * with false or a non-zero status (CLI_EXIT_USAGE).
 */
/* A member that is a string (null or absent when not required, then *out is NULL). */
bool request_member_string(const cJSON *object, const char *name, bool required, const char **out);
/* The "pickup" member as text: a string, or a number of beats formatted into text. NULL for none. */
bool request_member_pickup(const cJSON *object, char *text, size_t size, const char **out);
/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
bool request_parse_pickup(const char *text, int beat_unit, Rational *out);
/* A motive of a request, a degree list in the key with its pickup, harmony, features and role (role forced when not NULL). */
int request_read_motive(const char *what, const cJSON *object, const char *role, const PlanMeter *meter, const AnalysisKey *key,
                        Motive *out);
/* The GROW_FUNCTION_* bits a form's "functions" list names. */
bool request_read_functions(const char *what, const cJSON *functions, unsigned *out);
/* The brief of a run as theme prints it: text lines, or the JSON member "brief":{...} (no leading comma). */
void theme_print_brief(const GrowExt *base, const GrowBrief *brief, bool json);

/* The line of the usage text that names the subcommand. */
#define THEME_USAGE_LINE                                                                                          \
    "       ursatz-composer theme <request.json> [--seed <n>] [--suggest <n>] [--check] [--out <file.json>] "     \
    "[--midi <file>] [--musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]"

#endif
