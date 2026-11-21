#ifndef URSATZ_COMPOSER_CLI_OPTIONS_H
#define URSATZ_COMPOSER_CLI_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include "cli_tools.h"
#include "commands.h"
#include "flags.h"

/* The exit statuses every command shares. */
#define CLI_EXIT_USAGE 1
#define CLI_EXIT_UNSATISFIABLE 2
#define CLI_EXIT_WRITE 3

/* The largest seed, and how many seeds a command tries, one after another from the one given, before it gives up. */
#define CLI_SEED_MAX 4294967295UL
#define CLI_MAX_SEED_TRIES 16

/* Reads a whole file into a NUL-terminated buffer (*size is its length, may be NULL; the caller frees *text). False when it cannot be read. */
bool cli_read_file(const char *path, char **text, size_t *size);

/* Parses a whole number (digits only) not above maximum. */
bool cli_whole_number(const char *text, unsigned long maximum, unsigned long *out);

/* Whether two ASCII texts are equal ignoring case. */
bool cli_equals_ignore_case(const char *a, const char *b);

/* A size the API limits requests to (composer_api_vocabulary "limits": motive_notes, motive_chords, motive_features, variant_chain, variant_functions, random_motive_count); 0 when unknown. */
long cli_limit(const char *name);

/* The key word of a material the reply named by id ("minor" for "omt.material.natural-minor"), from the vocabulary, written into out; the id itself when the vocabulary names none. */
const char *cli_material_word(const char *id, char *out, size_t size);

/* Room for the repeatable options that the limits cap (--tone, --feature, --function): the API refuses more than its limit. */
#define CLI_LIST_CAPACITY 64

/* Whether a repeatable option already holds as many values as the limit allows (or as the CLI has room for). */
bool cli_list_full(size_t count, const char *limit_name);

/* Prints "Error: <message>" to stderr and returns status. */
int cli_fail(int status, const char *format, ...);

/*
 * Prints the error of a composer API entry as "Error: <message>" and returns the exit status of its status (REQUEST 1,
 * UNSATISFIABLE 2, any other 1). The API names request members where the command line names options, so the message is
 * put back in the command's words: a message that starts with a member (format, form, harmony, feature, role, tone,
 * tempo) starts with the option "--format" and so on, and "the input" becomes the quoted input_name when it is not NULL.
 */
int cli_fail_api(UrsatzStatus status, const UrsatzError *error, const char *input_name);

/* Prints the document with 2-space indent and one space after each colon, as the commands write JSON. */
void cli_print_json(const cJSON *document);

/*
 * The layers of a composed result into dir (made, with dir/readings): plan.json (the response's plan_json), realization.json and
 * readings/composer.json (the "realization" and "reading" members of the response document, printed as cli_print_json does). The
 * response was asked with "layers": true. Returns 0, or CLI_EXIT_WRITE after printing "cannot write '<file>'".
 */
int cli_write_layers(const char *dir, const CliResponse *response);

/* The --style-without values as given; the API checks that each is a style category and how many there are. */
typedef struct {
    const char *names[CLI_LIST_CAPACITY];
    size_t count;
} CliStyleWithout;

/* Reads the --style-without values of the parsed arguments. Returns 0, or the status of the usage error it printed (more than the CLI has room for). */
int cli_style_without_read(CliStyleWithout *list, const CliArgs *args);

/* The name of alternative k (from 1) of a file: before the extension, "-k". With one result the name as given. */
bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size);

/* A file written beside its final name and renamed into place once every file of the command is written. */
typedef struct {
    char final_path[1024];
    char temporary[1032];
} CliPendingFile;

/* Removes the temporary files of the first `count` pending files. */
void cli_discard_pending(CliPendingFile *pending, size_t count);

/* Renames the first `count` pending files into place; if one cannot be, prints "cannot write '<final path>'", removes the temporary
   files and returns CLI_EXIT_WRITE. Returns 0 when every file is in place. */
int cli_pending_commit(CliPendingFile *pending, size_t count);

/* Writes data to path atomically (common_file_write_atomic); false when it cannot be (the folder in front of it is not made). An
   existing file is kept when the write fails. */
bool cli_write_bytes(const char *path, const void *data, size_t size);

/*
 * The plan object (a plan of the composer API) as the library writes plan files, into the temporary file. Returns 0, or
 * CLI_EXIT_WRITE after printing "cannot write '<final_path>'".
 */
int cli_write_plan_object(const cJSON *plan, const char *temporary, const char *final_path);

/*
 * The MIDI file and the MusicXML of a plan object through composer_api_export, for the files of a command. qpm is the tempo
 * (0 for the plan's own), loop the {"start", "end"} object of the loop markers or NULL. Returns 0, or CLI_EXIT_WRITE after
 * printing "cannot render the plan". The caller destroys *out.
 */
int cli_export_plan(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml, const cJSON *loop,
                    CliResponse *out);

/* "12-16" as an inclusive range of measure numbers (the first may be 0, a pickup measure); false for anything else. */
bool cli_parse_measure_range(const char *text, int *first, int *last);

#endif
