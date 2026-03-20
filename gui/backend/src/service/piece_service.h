#ifndef URSATZ_GUI_PIECE_SERVICE_H
#define URSATZ_GUI_PIECE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "theme_service.h"

/*
 * Piece service: composes a piece item into a new version with the Composer's piece command (R-173, R-174).
 *
 * A piece version keeps its files beside its body, in the material store's items/<id>/v<n>/ (see material_service.h):
 * request.json (what the Composer read), brief.style.json (when the piece has a brief), plan.json (the loop version),
 * ending-plan.json, loop.mid, loop.musicxml, ending.mid and ending.musicxml, and theme-<section id>.plan.json, a copy
 * of the plan of each theme version a section uses, so the version keeps its own. The Composer runs through
 * composer_service.c and nothing here changes the working directory: every path handed to it is absolute.
 *
 * The results are the theme service's (UrsatzGuiThemeResult).
 */

/*
 * Composes the piece item id into a new version.
 *
 * With update_uses, every use whose source has a newer version is first moved to it (ursatz_gui_material_update_use,
 * a version each); that stays when the run then fails. The request is built from the current version: the motive and
 * every motif fill from their uses' copies (degrees, pickup, harmony, features), each motif fill with the pool of its
 * saved forms (as the theme service builds it, for the pool setting of the piece), a theme fill as the absolute path of
 * the copy of that theme version's plan.json in the new version's folder, a progression fill as given, and the brief
 * built into the folder as brief.style.json. seed (when has_seed) wins over the body's seed.
 *
 * On OK, *out_item is the new item.json, caller-owned. A new version holds the body as it was plus
 * result {seed, summary (the command's JSON), brief {piece_count, skipped}, loop {start, end}, files [names]}, with the
 * note "composed seed <n>". On any other result no version is written and no folder is left; message (when not NULL)
 * receives a one-line reason.
 */
UrsatzGuiThemeResult ursatz_gui_piece_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                              cJSON **out_item, char *message, size_t message_size);

#endif /* URSATZ_GUI_PIECE_SERVICE_H */
