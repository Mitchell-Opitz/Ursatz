#ifndef URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H
#define URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H

#include <stdbool.h>
#include <stdio.h>

#include <cJSON.h>
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

/* The largest input file read, as for the library's file reader. */
#define MOTIVE_INPUT_FILE_LIMIT (64L * 1024L * 1024L)

/*
 * Reads the input file into memory (*bytes, freed by the caller). Returns 0, or CLI_EXIT_UNSATISFIABLE with error set to
 * the message ("cannot load '<path>' (io) at <path>: cannot open the file"); nothing is printed.
 */
int motive_input_read_file(const char *path, unsigned char **bytes, size_t *size, char *error, size_t error_size);

/*
 * The composer API request for the input: "meter", "key", "pickup", the extras and, for a text input, "input" {"format",
 * "form", "text"} with the bytes as the text. *midi is set when the bytes are read as a MIDI file (given as format midi, or
 * detected and not given another form): the request then has no "input" and goes to the bytes entry. NULL when out of
 * memory. bytes may be NULL for a file that could not be read; the text is then empty.
 */
cJSON *motive_input_request(const MotiveInput *input, const unsigned char *bytes, size_t size, bool *midi);

/*
 * Loads the motive as the motive command does: the pickup and the chord onsets are in beats of the motive's meter, which a
 * file may declare, so they are read after a first load. Returns 0, or the status of the error it printed (a usage error
 * for an option, 2 for a file that cannot be loaded).
 */
int motive_input_load(const MotiveInput *input, Motive *out);

/*
 * The basic motive of a family of forms, the tree root, which remoteness is measured from (R-142): --basic <file>, with
 * --basic-format, --basic-pickup and --basic-harmony. It is read in the source's meter, key, form and role.
 */
typedef struct {
    const char *path;
    const char *format_text;
    const char *pickup_text;
    const char *harmony_text;
} BasicInput;

/*
 * If argv[*index] is --basic, --basic-format, --basic-pickup or --basic-harmony, takes it and its value, moves *index to
 * the value and returns true; *status is then 0, or the status of the usage error printed (a missing value, an option
 * given twice). Returns false for any other argument.
 */
bool motive_input_take_basic(BasicInput *basic, int argc, char **argv, int *index, int *status);

/* Whether any of the basic options was given. */
bool motive_input_basic_given(const BasicInput *basic);

/* Loads the basic motive as the source is read (motive_input_load). Returns 0 or the status of the error printed. */
int motive_input_load_basic(const BasicInput *basic, const MotiveInput *source, Motive *out);

/* The help lines of the basic options. */
void motive_input_print_basic_help(FILE *out);

#endif
