#ifndef URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H
#define URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H

#include <stdbool.h>
#include <stdio.h>

#include <cJSON.h>

#include "cli_options.h"

/*
 * The options that put a motive's own harmony, labelled features and role on it: --harmony "<text>", --feature
 * <label:first-last> (repeatable) and --role melodic|accompaniment. Shared by the commands that read a motive
 * (motive, variants, grow); random-motive takes --role only.
 */
typedef struct {
    const char *harmony_text;
    const char *feature_texts[CLI_LIST_CAPACITY];
    size_t feature_count;
    const char *role_text;
} MotiveExtras;

/*
 * Reads --harmony, --feature and --role (only --role when role_only) of the parsed arguments into extras. Returns 0, or the status of the
 * usage error printed (more features than the API allows).
 */
int motive_extras_read(MotiveExtras *extras, const CliArgs *args, bool role_only);

/* Whether any of the options was given. */
bool motive_extras_given(const MotiveExtras *extras);

/* Adds the options given to a request object as "harmony", "features" and "role". False when out of memory. */
bool motive_extras_add_members(const MotiveExtras *extras, cJSON *object);

#endif
