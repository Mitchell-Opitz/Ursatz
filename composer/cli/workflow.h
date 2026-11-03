#ifndef URSATZ_COMPOSER_CLI_WORKFLOW_H
#define URSATZ_COMPOSER_CLI_WORKFLOW_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

/*
 * What the plan workflow commands (plan, fill, lock, regenerate, variations, harmonize) share: files in and out of the
 * composer API, and the text they print from its response documents.
 */

/* --- reading --------------------------------------------------------------------------------------- */

/*
 * A file read into a JSON value. Returns 0, or 2 after printing "cannot read plan '<path>': <why>" when it is not
 * there or not JSON. what names the file in the message ("plan").
 */
int cli_read_plan_file(const char *path, cJSON **plan);

/*
 * A file whose failure is held back: *plan is NULL and *message the text to print, so that the options of the command
 * are checked (by the API) before a file that cannot be read is reported.
 */
typedef struct {
    char message[1100];
    bool failed;
} CliHeldFailure;

/* As cli_read_plan_file, but the failure is kept in held instead of printed. */
void cli_read_plan_held(const char *path, cJSON **plan, CliHeldFailure *held);

/*
 * Prints the error of an API entry for a command that works on the plan file plan_file: "cannot read plan: x" becomes
 * "cannot read plan '<file>': x". When the failure is at /plan and the file could not be read (held), the held message
 * is printed instead. Returns the exit status.
 */
int cli_fail_plan_api(ComposerStatus status, const ComposerError *error, const char *plan_file, const CliHeldFailure *held);

/* --- calling --------------------------------------------------------------------------------------- */

/* Calls the entry with the request (taken and deleted). */
typedef ComposerStatus (*CliEntry)(const char *request_json, const ComposerJob *job, ComposerResponse *out);
ComposerStatus cli_call(CliEntry entry, cJSON *request, ComposerResponse *out);

/* --- the node a command works on ------------------------------------------------------------------- */

typedef struct {
    const char *node_text;
    const char *section_text;
    const char *lane_text;
} CliTargetOptions;

/* Checks the three texts of target (exactly one of node and section, a lane word). Returns 0 or the usage status printed. */
int cli_check_target(const CliTargetOptions *target, const char *command, const char *usage);
/* Adds "node" or "section", and "lane", to the request. */
bool cli_add_target(cJSON *request, const CliTargetOptions *target);

/* --- writing --------------------------------------------------------------------------------------- */

typedef enum {
    CLI_FILE_PLAN,    /* data is the plan object (cJSON) */
    CLI_FILE_BYTES,   /* a binary file */
    CLI_FILE_TEXT     /* text, written as it is */
} CliFileKind;

typedef struct {
    const char *path;
    CliFileKind kind;
    const cJSON *plan;
    const void *data;
    size_t size;
    bool detailed; /* the failure says why: "cannot write 'x': could not write 'x.tmp'" */
} CliFile;

/* Writes every file beside its final name and renames them all only when every one was written. Returns 0 or 3. */
int cli_write_files(const CliFile *files, size_t count);

/* The MIDI file of a plan object through composer_api_export; the caller destroys *out. Returns 0 or the exit status. */
int cli_export_midi(const cJSON *plan, const char *tempo_text, bool plain_midi, ComposerResponse *out);

/* --- printing -------------------------------------------------------------------------------------- */

const char *cli_json_string(const cJSON *object, const char *name);
double cli_json_number(const cJSON *object, const char *name);

/* Prints "Warning: <text>" to stderr for each of "notices" and removes the member. */
void cli_print_notices(cJSON *document);

/* "A heard as period, cadences PAC; B heard as ..." for the "sections" array of a result; no newline. */
void cli_print_heard_line(const cJSON *sections);

/* What fill prints for a composed plan, from its result document; files are the paths written. */
void cli_print_composed_text(const cJSON *document, const char *const *files, size_t file_count);
/* The candidates of a fill, when it made several. */
void cli_print_candidates_text(const cJSON *document);
/* Adds "files", removes the members that only the text uses, and prints the document on one line. */
void cli_print_composed_json(cJSON *document, const char *const *files, size_t file_count);

/* The stuck report of a failed fill or regenerate: the error line, the targets left open and why. */
int cli_report_stuck(const ComposerError *error, const ComposerResponse *response);

#endif
