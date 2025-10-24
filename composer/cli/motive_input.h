#ifndef URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H
#define URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H

#include <ursatz_composer/motive.h>

#include "motive_extras.h"

/* A motive input as the commands that read one take it: the file and the options that say how to read it. */
typedef struct {
    const char *path;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
    const char *form_text;
    const char *pickup_text;
    MotiveExtras extras;
} MotiveInput;

/*
 * Loads the motive as the motive command does: the pickup and the chord onsets are in beats of the motive's meter, which a
 * file may declare, so they are read after a first load. Returns 0, or the status of the error it printed (a usage error
 * for an option, 2 for a file that cannot be loaded).
 */
int motive_input_load(const MotiveInput *input, Motive *out);

#endif
