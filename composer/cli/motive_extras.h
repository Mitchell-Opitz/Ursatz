#ifndef URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H
#define URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H

#include <stdbool.h>
#include <stdio.h>

#include <cJSON.h>
#include <ursatz_composer/motive.h>

/*
 * The options that put a motive's own harmony, labelled features and role on it: --harmony "<text>", --feature
 * <label:first-last> (repeatable) and --role melodic|accompaniment. Shared by the commands that read a motive
 * (motive, variants, grow); random-motive takes --role only.
 */
typedef struct {
    const char *harmony_text;
    const char *feature_texts[MOTIVE_MAX_FEATURES];
    size_t feature_count;
    const char *role_text;
} MotiveExtras;

/*
 * If argv[*index] is --harmony, --feature or --role (only --role when role_only), takes it and its value into extras,
 * moves *index to the value, and returns true. *status is then 0, or the status of the usage error printed (a missing
 * value, an option given twice, more than MOTIVE_MAX_FEATURES features). Returns false for any other argument.
 */
bool motive_extras_take(MotiveExtras *extras, int argc, char **argv, int *index, bool role_only, int *status);

/* Whether any of the options was given. */
bool motive_extras_given(const MotiveExtras *extras);

/* Adds the options given to a request object as "harmony", "features" and "role". False when out of memory. */
bool motive_extras_add_members(const MotiveExtras *extras, cJSON *object);

/* The help lines of the options. */
void motive_extras_print_help(FILE *out, bool role_only);

#endif
