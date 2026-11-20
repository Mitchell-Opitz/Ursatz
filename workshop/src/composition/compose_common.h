#ifndef WORKSHOP_COMPOSE_COMMON_H
#define WORKSHOP_COMPOSE_COMMON_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "ursatz_composer/composer_api.h"

/* What the theme and the piece service share when they compose an item into a new version: the small JSON helpers and
 * the steps around the Composer run. Composes of one item may run at once (each makes its files in a staging folder). */

/* What a kind's run is given: the folder the files go to while the run is going (stage), the folder the version will
 * have (named; the summary names its files there), the request as the Composer command read it (paths and all), the
 * body it came from (its options, check and seed) and the options. */
typedef struct {
    const char *stage;
    const char *named;
    const cJSON *request;
    const cJSON *body;
    bool use_seed;
    long long seed;
    const UrsatzJob *job;
} WorkshopComposeRun;

/* What differs between composing a theme and composing a piece. */
typedef struct {
    const char *member;         /* the body's member and the item type: "theme" or "piece" */
    const char *not_kind_text;  /* the reason when the item has another type */
    /* True when the body's request can be composed; else it says why in message. */
    bool (*composable)(const cJSON *body, UrsatzError *error);
    /* Runs the Composer API on run->request and writes the version's files into run->stage. */
    UrsatzStatus (*run)(const WorkshopComposeRun *run, cJSON **out_summary, UrsatzError *error);
    /* Brings the uses of item id up to their latest versions (the compose of a piece or a theme with update_uses). */
    UrsatzStatus (*update_sources)(const char *id, UrsatzError *error);
    /* Builds the brief's style file at path from brief_json and answers its summary (caller frees). */
    UrsatzStatus (*build_brief)(const cJSON *brief_json, const char *name, const char *path, cJSON **out_summary,
                                       UrsatzError *error);
    /* The note of the new version. */
    void (*note)(const cJSON *summary, char *note, size_t note_size);
    /* The body of the new version; takes ownership of summary and brief_summary. */
    cJSON *(*body_with_result)(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief);
} WorkshopComposeKind;

/*
 * Composes the item id into a new version: updates the uses (when update_uses), reads the item, makes a staging folder
 * beside the version's, builds the brief and the request, runs the Composer API (job reports progress and asks for
 * cancel; may be NULL), adds the version and moves the staging folder to the version's number (another writer may have
 * added a version meanwhile, so composes of one item may run at once). On OK *out_item is the new item.json (caller
 * frees); otherwise no folder is left.
 */
UrsatzStatus workshop_compose_item(const WorkshopComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, const UrsatzJob *job,
                                             cJSON **out_item, UrsatzError *error);

#define WORKSHOP_COMPOSE_PATH_BYTES 800

/* The folder a version's files are made in while the Composer runs: beside the version's own, with a name of its own, so
 * composes of one item may run at once. `absolute` is its resolved path, `named` the version folder it will become. */
typedef struct {
    char path[WORKSHOP_COMPOSE_PATH_BYTES + 32];
    char absolute[WORKSHOP_COMPOSE_PATH_BYTES + 32];
    char named[WORKSHOP_COMPOSE_PATH_BYTES];
} WorkshopComposeStage;

/* Makes the staging folder for version number of item id. */
UrsatzStatus workshop_compose_stage_make(const char *id, int number, WorkshopComposeStage *stage, UrsatzError *error);

/* The staging folder becomes the version's folder when the version is written (workshop_material_item_add_version_staged). */

/*
 * Moves the realization and the reading the Composer answered with layers (R-634) out of document into the stage as realization.json and
 * readings/composer.json, so the version keeps its layers (R-644); the members are gone from document, which is what the version's result keeps.
 * A document without them is left as it is.
 */
UrsatzStatus workshop_compose_write_layers(const char *stage, cJSON *document, UrsatzError *error);

/* Removes the staging folder and what it holds. */
void workshop_compose_stage_discard(const WorkshopComposeStage *stage);

/* The Composer's refusal as the service's: the error (message and the request member at fault in path) is copied to *error, and
 * the status is the Composer's (REQUEST, UNSATISFIABLE, CANCELLED, ALLOC), any other being INTERNAL. */
UrsatzStatus workshop_compose_failure(UrsatzStatus status, const ComposerResponse *response, UrsatzError *error);

/* Writes the plan object as the library writes plan files (composer_api_export with plan_file) to path. */
UrsatzStatus workshop_compose_write_plan(const cJSON *plan, const char *path, UrsatzError *error);

/* Renders plan once through composer_api_export: midi and/or musicxml, tempo qpm (0 for none), loop markers (or NULL).
 * On OK response holds the files; the caller destroys it. */
UrsatzStatus workshop_compose_export(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml,
                                               const cJSON *loop, const UrsatzJob *job, ComposerResponse *response,
                                               UrsatzError *error);

/* Replaces the prefix from by to in every string of tree that starts with it. */
void workshop_compose_rebase(cJSON *tree, const char *from, const char *to);

/* Adds files (each "<dir>/<name>") as the array "files" of summary. false on allocation failure. */
bool workshop_compose_add_files(cJSON *summary, const char *dir, const char *const *names, size_t count);

#endif
