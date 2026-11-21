#ifndef WORKSHOP_THEME_SERVICE_H
#define WORKSHOP_THEME_SERVICE_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "ursatz_composer/composer_api.h"
#include "base/last_error.h"
#include "compose_common.h"

/*
 * Theme service: composes a theme item into a new version with the Composer's theme command (R-166, R-167).
 *
 * A theme version keeps its files beside its body, in the material store's items/<id>/v<n>/ (see
 * store.h): request.json (what the Composer read), brief.style.json (when the theme has a brief),
 * plan.json, theme.mid and theme.musicxml. The Composer runs in process through the Composer API
 * (composer_api_theme), on a job worker.
 */

/*
 * Composes the theme item id into a new version.
 *
 * With update_uses, every use whose source has a newer version is first moved to it (workshop_material_update_use,
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
UrsatzStatus workshop_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses, const WorkshopComposeSettings *settings,
                                              const UrsatzJob *job, cJSON **out_item, UrsatzError *error);

/*
 * The theme request of the Composer for the body of a theme version, to be checked with composer_api_validate and not
 * composed (R-276). *complete is false (and *out_request NULL) when the body cannot be composed yet; message then says
 * why ("" when the brief's style is all that is missing). On OK with *complete, *out_request is caller-owned. Writes
 * nothing. A motif the body refers to that cannot be read is URSATZ_ERR_REQUEST with the reason in message.
 */
UrsatzStatus workshop_theme_validate(const cJSON *body, bool *complete, cJSON **out_request, UrsatzError *error);

/*
 * Shared with piece_service.c: the same steps for a piece (R-173, R-174).
 */

/* Moves every updated use of the item id to its source's current version, once for each source. */
UrsatzStatus workshop_theme_update_sources(const char *id, UrsatzError *error);

/* Builds a brief into path. *out_summary is {piece_count, skipped: [{id, reason}]}, caller-owned. */
UrsatzStatus workshop_theme_build_brief(const cJSON *brief_json, const char *name, const char *path,
                                                  cJSON **out_summary, UrsatzError *error);

#endif /* WORKSHOP_THEME_SERVICE_H */
