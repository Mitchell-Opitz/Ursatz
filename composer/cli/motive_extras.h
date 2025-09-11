#ifndef URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H
#define URSATZ_COMPOSER_CLI_MOTIVE_EXTRAS_H

#include <stdbool.h>
#include <stdio.h>

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

/*
 * Reads the texts into options (has_harmony, has_features, has_role and their values). meter is the motive's meter,
 * which the chord onsets are counted in; it may be NULL only when no --harmony was given. Returns 0, or the status of
 * the usage error printed.
 */
int motive_extras_options(const MotiveExtras *extras, const PlanMeter *meter, MotiveOptions *options);

/* The help lines of the options. */
void motive_extras_print_help(FILE *out, bool role_only);

#endif
