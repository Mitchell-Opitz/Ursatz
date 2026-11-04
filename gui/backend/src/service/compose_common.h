#ifndef URSATZ_GUI_COMPOSE_COMMON_H
#define URSATZ_GUI_COMPOSE_COMMON_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "theme_service.h"
#include "ursatz_composer/composer_api.h"

/* What the theme and the piece service share when they compose an item into a new version: the small JSON helpers and
 * the steps around the Composer run. The caller holds its own compose lock. */

/* Copies text into message when there is room for it. */
void ursatz_gui_compose_say(char *message, size_t size, const char *text);

/* The string member name of object, or "" when it is missing or not a string. */
const char *ursatz_gui_compose_text_of(const cJSON *object, const char *name);

/* Adds child to parent as name, or frees it. false on allocation failure. */
bool ursatz_gui_compose_attach(cJSON *parent, const char *name, cJSON *child);

/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
bool ursatz_gui_compose_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name);

/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
bool ursatz_gui_compose_add_copy_if(cJSON *dst, const cJSON *src, const char *name);

/* What a kind's run is given: the folder the files go to while the run is going (stage), the folder the version will
 * have (named; the summary names its files there), the request as the Composer command read it (paths and all) and
 * the options. */
typedef struct {
    const char *stage;
    const char *named;
    const cJSON *request;
    const cJSON *member;
    bool use_seed;
    long long seed;
    const ComposerJob *job;
} UrsatzGuiComposeRun;

/* What differs between composing a theme and composing a piece. */
typedef struct {
    const char *member;         /* the body's member and the item type: "theme" or "piece" */
    const char *not_kind_text;  /* the reason when the item has another type */
    /* True when the member can be composed; else it says why in message. */
    bool (*composable)(const cJSON *member, bool has_brief, char *message, size_t message_size);
    /* Builds the Composer request from the body; style_path is the brief's style file or NULL. */
    UrsatzGuiThemeResult (*build_request)(const cJSON *body, const char *dir, const char *style_path, cJSON **out_request,
                                          char *message, size_t message_size);
    /* Runs the Composer API on run->request and writes the version's files into run->stage. */
    UrsatzGuiThemeResult (*run)(const UrsatzGuiComposeRun *run, cJSON **out_summary, char *message, size_t message_size);
    /* The note of the new version. */
    void (*note)(const cJSON *summary, char *note, size_t note_size);
    /* The body of the new version; takes ownership of summary and brief_summary. */
    cJSON *(*body_with_result)(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief);
} UrsatzGuiComposeKind;

/*
 * Composes the item id into a new version: updates the uses (when update_uses), reads the item, makes a staging folder
 * beside the version's, builds the brief and the request, runs the Composer API (job reports progress and asks for
 * cancel; may be NULL), adds the version and moves the staging folder to the version's number (another writer may have
 * added a version meanwhile, so composes of one item may run at once). On OK *out_item is the new item.json (caller
 * frees); otherwise no folder is left.
 */
UrsatzGuiThemeResult ursatz_gui_compose_item(const UrsatzGuiComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, const ComposerJob *job,
                                             cJSON **out_item, char *message, size_t message_size);

/* Maps a Composer API status to the result of the services: the request refused is INVALID, nothing satisfies it
 * UNPROCESSABLE, a cancel CANCELLED, the rest FAILED (ALLOC for memory). The refusal text goes to message. */
UrsatzGuiThemeResult ursatz_gui_compose_failure(ComposerStatus status, const ComposerResponse *response, char *message,
                                                size_t message_size);

/* Writes the plan object as the library writes plan files (composer_api_export with plan_file) to path. */
UrsatzGuiThemeResult ursatz_gui_compose_write_plan(const cJSON *plan, const char *path, char *message,
                                                   size_t message_size);

/* Renders plan once through composer_api_export: midi and/or musicxml, tempo qpm (0 for none), loop markers (or NULL).
 * On OK response holds the files; the caller destroys it. */
UrsatzGuiThemeResult ursatz_gui_compose_export(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml,
                                               const cJSON *loop, const ComposerJob *job, ComposerResponse *response,
                                               char *message, size_t message_size);

/* Replaces the prefix from by to in every string of tree that starts with it. */
void ursatz_gui_compose_rebase(cJSON *tree, const char *from, const char *to);

/* Adds files (each "<dir>/<name>") as the array "files" of summary. false on allocation failure. */
bool ursatz_gui_compose_add_files(cJSON *summary, const char *dir, const char *const *names, size_t count);

#endif
