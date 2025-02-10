#ifndef URSATZ_COMPOSER_CLI_OPTIONS_H
#define URSATZ_COMPOSER_CLI_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/style.h>

/* The exit statuses every command shares. */
#define CLI_EXIT_USAGE 1
#define CLI_EXIT_UNSATISFIABLE 2
#define CLI_EXIT_WRITE 3

/* The largest seed, and how many seeds a command tries, one after another from the one given, before it gives up. */
#define CLI_SEED_MAX 4294967295UL
#define CLI_MAX_SEED_TRIES 16

/* Prints "Error: <message>" to stderr and returns status. */
int cli_fail(int status, const char *format, ...);

bool cli_equals_ignore_case(const char *a, const char *b);

/* A whole number from 0 to maximum, digits only. */
bool cli_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out);

/* A tonic letter, an optional # or b, whitespace, then major or minor ("D minor", "F# major"); any case for the mode. */
bool cli_parse_key(const char *text, PlanKey *key);
const char *cli_key_name(const PlanKey *key, char *out, size_t size);

/* "3/4", "6/8". The meters the harmony operators support are checked by the caller (harmony_meter_supported). */
bool cli_parse_meter(const char *text, PlanMeter *meter);

/* PAC, IAC, HC, DC or PC, any case. */
bool cli_parse_cadence(const char *text, PlanCadenceType *cadence);
const char *cli_cadence_name(PlanCadenceType cadence);

/* "any", "block", "arpeggiated", "alberti", "oom-pah", by PlanAccompanimentPattern. */
const char *cli_accompaniment_name(PlanAccompanimentPattern pattern);

/*
 * Reads --accompaniment (pattern_text) and --texture (texture_text), either of which may be NULL, into *out. Returns 0, or
 * the status of the usage error it printed. meter_text is the --meter argument, for the message about alberti.
 */
int cli_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text, const PlanMeter *meter,
                           PlanAccompaniment *out);

/* "3/2" or "3": a weight, both parts whole numbers up to 4294967295 and the denominator at least 1. */
bool cli_parse_ratio(const char *text, ComposerRatio *out);

/* Reads the style model file at path into *out. Returns 0, or the status of the usage error it printed. */
int cli_load_style(const char *path, ComposerStyle **out);

/* The Analyzer's distance from the style, per dimension and overall (0 is the same, 1 is nothing alike): text lines. */
void cli_print_style_distance(const StyleComparison *comparison);
/* The same as the JSON member "style_distance":{...}, with no leading comma. */
void cli_print_style_distance_json(const StyleComparison *comparison);

/* Prints text as a JSON string to stdout: quotes and backslashes escaped, control characters dropped. */
void cli_print_json_string(const char *text);

/* A seed from the clock, for a command given none. */
unsigned long cli_default_seed(void);

/* The name of alternative k (from 1) of a file: before the extension, "-k". With one result the name as given. */
bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size);

#endif
