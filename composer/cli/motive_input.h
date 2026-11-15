#ifndef URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H
#define URSATZ_COMPOSER_CLI_MOTIVE_INPUT_H

#include <stdbool.h>
#include <stdio.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

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

/* The basic motive's input: its file and options, read in the source's meter, key, form and role. Returns 0, or the usage
   status printed when basic options were given with no --basic file. */
int motive_input_basic_input(const BasicInput *basic, const MotiveInput *source, MotiveInput *out);

/*
 * The input a file read failed on, and what to say: the first file that could not be read. A bad option is reported before a
 * missing file, so the message is used only when the API has no complaint of its own about the request.
 */
typedef struct {
    int read_status;
    char read_error[512];
} MotiveFile;

/*
 * As motive_input_request for the entries that take text only (variants, recognize): the file is read into *file, and a MIDI
 * file is read by composer_api_motive_from_midi, its motive document becoming the input (format json). Returns 0 with
 * *request set, or the status of the error printed.
 */
int motive_input_text_request(const MotiveInput *input, cJSON **request, MotiveFile *file);

/* The names of the input files an API error path points at: "" or "/source" the first, "/target", "/basic". */
typedef struct {
    const char *source;
    const char *target;
    const char *basic;
    const char *accompaniment; /* "/accompaniment_motive"; NULL: the source */
} MotiveInputNames;

/*
 * The status and message of a failed entry that read files: the file's own read error unless the request itself was at
 * fault, else the API error with the name of the file its path points at. Returns the exit status.
 */
int motive_input_fail(const MotiveFile *file, ComposerStatus status, const ComposerError *error,
                      const MotiveInputNames *names);

/* The help lines of the basic options. */
void motive_input_print_basic_help(FILE *out);

#endif
