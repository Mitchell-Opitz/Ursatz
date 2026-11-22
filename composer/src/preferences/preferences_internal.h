#ifndef URSATZ_COMPOSER_PREFERENCES_INTERNAL_H
#define URSATZ_COMPOSER_PREFERENCES_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/preferences.h>

/*
 * What the files of the preferences document share (preferences.c: the melody, motif and candidates groups and the key tables;
 * preferences_development.c: the development group). Internal to this folder.
 */

#define WEIGHT_MAX COMPOSER_PREFERENCES_WEIGHT_MAX
#define FIDELITY_DENOMINATOR_MAX COMPOSER_PREFERENCES_FIDELITY_DENOMINATOR_MAX

/* Where a refusal goes: the path and the message of the caller, which may both be absent. */
typedef struct {
    char *path;
    size_t path_size;
    char *message;
    size_t message_size;
} Refusal;

/* Refuses with the path under the document, "melody/keep" for /melody/keep; "" is the document itself. Returns false. */
bool preferences_refuse(const Refusal *refusal, const char *where, const char *limit);

/* The key's path under a group: "keep" under "melody" for /melody/keep. Returns false. */
bool preferences_refuse_in(const Refusal *refusal, const char *group, const char *key, const char *limit);

/* True for a number that is a whole number from low to high; *out is its value. */
bool preferences_whole_number(const cJSON *item, double low, double high, double *out);

/* An unsigned setting from a whole number low to high; says which limit it broke when it is refused. */
bool preferences_read_unsigned_in(const cJSON *item, const char *group, const char *key, unsigned low, unsigned high,
                                  unsigned *out, const Refusal *refusal);

/* "n/d" with 0 <= n <= d and 1 <= d <= denominator_max, digits only. */
bool preferences_parse_ratio(const char *text, unsigned long denominator_max, Rational *out);

bool preferences_add_number(cJSON *object, const char *name, double value);

/* The development group: reads the object into preferences (refusing with the path under "development"), and writes the group. */
bool preferences_read_development(const cJSON *group, ComposerPreferences *preferences, const Refusal *refusal);
cJSON *preferences_write_development(const ComposerPreferences *preferences);

#endif
