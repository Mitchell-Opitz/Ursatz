#ifndef URSATZ_COMPOSER_CLI_OPTIONS_H
#define URSATZ_COMPOSER_CLI_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>


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
int cli_fail_api(ComposerStatus status, const ComposerError *error, const char *input_name);

/* Prints the document with 2-space indent and one space after each colon, as the commands write JSON. */
void cli_print_json(const cJSON *document);

/* The --style-without values as given; the API checks that each is a style category and how many there are. */
typedef struct {
    const char *names[CLI_LIST_CAPACITY];
    size_t count;
} CliStyleWithout;

/* Adds one --style-without value. Returns 0, or the status of the usage error it printed (more than the CLI has room for). */
int cli_style_without_add(CliStyleWithout *list, const char *name);

/* The name of alternative k (from 1) of a file: before the extension, "-k". With one result the name as given. */
bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size);

/* A file written beside its final name and renamed into place once every file of the command is written. */
typedef struct {
    char final_path[1024];
    char temporary[1032];
} CliPendingFile;

/* Removes the temporary files of the first `count` pending files. */
void cli_discard_pending(CliPendingFile *pending, size_t count);

/* Writes data to path; false when it cannot be (a partly written file is removed). */
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
                    ComposerResponse *out);

#endif
