#ifndef URSATZ_GUI_THEME_SERVICE_H
#define URSATZ_GUI_THEME_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "ursatz_composer/composer_api.h"

/*
 * Theme service: composes a theme item into a new version with the Composer's theme command (R-166, R-167).
 *
 * A theme version keeps its files beside its body, in the material store's items/<id>/v<n>/ (see
 * material_service.h): request.json (what the Composer read), brief.style.json (when the theme has a brief),
 * plan.json, theme.mid and theme.musicxml. The Composer runs in process through the Composer API
 * (composer_api_theme), on a job worker.
 */

typedef enum {
    URSATZ_GUI_THEME_OK = 0,
    URSATZ_GUI_THEME_ERR_NOT_FOUND,      /* no such item (404) */
    URSATZ_GUI_THEME_ERR_INVALID,        /* not a theme, not composable, a bad option or a Composer usage error (400) */
    URSATZ_GUI_THEME_ERR_UNPROCESSABLE,  /* the Composer is stuck, or the brief names nothing it can use (422) */
    URSATZ_GUI_THEME_ERR_FAILED,         /* any other failure (500) */
    URSATZ_GUI_THEME_ERR_ALLOC,
    URSATZ_GUI_THEME_ERR_CANCELLED       /* the job was cancelled (499) */
} UrsatzGuiThemeResult;

/*
 * Composes the theme item id into a new version.
 *
 * With update_uses, every use whose source has a newer version is first moved to it (ursatz_gui_material_update_use,
 * a version each); that stays when the run then fails. The request is built from the current version: each slot's
 * motif from its use's copy, the pool of its saved forms (a motif item whose derived chain, followed at most 32 steps
 * through derived.source_item, leads to the slot's item; flag approved, and unflagged with the default pool; at most
 * 64 for each motive; rejected forms never), and the brief built into the version's folder as brief.style.json.
 * seed (when has_seed) wins over the body's seed.
 *
 * On OK, *out_item is the new item.json, caller-owned. A new version holds the body as it was plus
 * result {seed, summary (the command's JSON), brief {piece_count, skipped}, files [names]}, with the note
 * "composed seed <n>". job (may be NULL) reports progress and asks for cancel (ERR_CANCELLED). On any other result no version is written and no folder is left; message (when not NULL)
 * receives a one-line reason.
 */
UrsatzGuiThemeResult ursatz_gui_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                              const ComposerJob *job, cJSON **out_item, char *message,
                                              size_t message_size);

/*
 * The theme request of the Composer for the body of a theme version, to be checked with composer_api_validate and not
 * composed (R-276). *complete is false (and *out_request NULL) when the body cannot be composed yet; message then says
 * why ("" when the brief's style is all that is missing). On OK with *complete, *out_request is caller-owned. Writes
 * nothing. A motif the body refers to that cannot be read is URSATZ_GUI_THEME_ERR_INVALID with the reason in message.
 */
UrsatzGuiThemeResult ursatz_gui_theme_validate(const cJSON *body, bool *complete, cJSON **out_request, char *message,
                                               size_t message_size);

/*
 * Shared with piece_service.c: the same steps for a piece (R-173, R-174).
 */

/* Moves every updated use of the item id to its source's current version, once for each source. */
UrsatzGuiThemeResult ursatz_gui_theme_update_sources(const char *id, char *message, size_t message_size);

/* A motive of the request from a motif's members: with item (a pool form), else with id (a slot motive), else neither. */
cJSON *ursatz_gui_theme_motive_json(const char *id, const char *item, const cJSON *motif, const char *role);

/* Adds to forms the saved forms of the motif item slot_item under the pool setting (at most 64 in all). */
UrsatzGuiThemeResult ursatz_gui_theme_add_pool(const cJSON *list, const char *slot_item, const char *pool, cJSON *forms);

/* Builds a brief into path. *out_summary is {piece_count, skipped: [{id, reason}]}, caller-owned. */
UrsatzGuiThemeResult ursatz_gui_theme_build_brief(const cJSON *brief_json, const char *name, const char *path,
                                                  cJSON **out_summary, char *message, size_t message_size);

/* Removes the files of a version folder, then the folder. */
void ursatz_gui_theme_remove_folder(const char *path);

bool ursatz_gui_theme_write_text(const char *path, const char *text);

#endif /* URSATZ_GUI_THEME_SERVICE_H */
