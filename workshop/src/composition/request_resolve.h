#ifndef WORKSHOP_REQUEST_RESOLVE_H
#define WORKSHOP_REQUEST_RESOLVE_H

#include <common/status.h>

#include <cJSON.h>

/*
 * A theme or piece version stores the Composer request it composes with (R-650), with a reference in place of what the store
 * would inline:
 *
 *   {"$item": {"item_id", "version"}}   a motif version (a theme's `motives[]` entry keeps its "id" beside it, a piece's `motive`
 *                                       and a motif fill's `motive` stand alone, an accompaniment pattern "motive" is the
 *                                       accompaniment's)
 *   {"$item": {"item_id", "version", "file": "plan.json"}}   the stored plan of a theme fill (`plan`)
 *   {"$pool": {"item_id", "pool"}}      the saved forms of a motif (a motif fill's `forms`), for the pool word
 *
 * A harmony version (R-767) is also a reference: {"$item": {"item_id", "version"}} as a theme's `chords` (the version's chords, bars from 0) or a
 * piece section's `harmony` ({"chords": [...]}, bars from 1); the version must be composed.
 *
 * workshop_request_resolve answers the request the Composer reads: kind is "theme" or "piece", body the stored body with its
 * `uses` (the copy of each used version) and its `request`. dir is the folder the plans of theme fills are copied into
 * (NULL copies nothing: the request names the source plans, as a validate does); style_path is the brief's style file (NULL
 * without a brief, which also drops the request's style_weight and without). A reference to a version the body does not use, or
 * to a motif without degrees, is URSATZ_ERR_NOT_FOUND at the path of the reference.
 */
UrsatzStatus workshop_request_resolve(const char *kind, const cJSON *body, const char *dir, const char *style_path,
                                      cJSON **resolved, UrsatzError *error);

#endif
