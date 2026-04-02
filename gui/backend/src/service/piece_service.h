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

/*
 * Writes section section_id of version number of piece item id again (R-173): `piece --from <version>/plan.json
 * --request <request.json> --regenerate <section id> [--lane <lane>]`, the eight outputs into the next version's
 * folder. lane is NULL for the whole section, else harmony, bass, melody, inner or all; a section filled with a theme
 * takes harmony or bass only. Only a composed version (one with a result) and only a section of the loop (not the
 * ending) can be regenerated: INVALID otherwise, and for an item that is not a piece. NOT_FOUND for an item or version
 * that is not there. seed (when has_seed) is the first seed tried; without it the Composer picks one.
 *
 * The new version's folder holds its own copy of the request (its theme plan and style paths made to name the copies
 * beside it), the theme plans and the brief's style file. On OK, *out_item is the new item.json (caller-owned); the
 * version's body is the source's with result replaced, the note "regenerated <section id> <lane> seed <n>" (lane "all"
 * without one). On any other result no version is written and no folder is left; message receives a one-line reason.
 */
UrsatzGuiThemeResult ursatz_gui_piece_regenerate(const char *id, int version, const char *section_id, const char *lane,
                                                 bool has_seed, long long seed, cJSON **out_item, char *message,
                                                 size_t message_size);

/*
 * Makes count (1 to 8) variations of a section of version number: `piece --from ... --variations <section id> --lane
 * <lane> --count <n> --out-dir <version>/variations-<k>`, k the next set number of the item (the sets of every version
 * count). The section and lane follow ursatz_gui_piece_regenerate (the lane is required); harmonize is lane harmony.
 * The set's folder holds variation-<i>.json, .mid and .musicxml and the Composer's result.json. No version is written.
 *
 * On OK, *out_result is {set, variations: [{index, seed, score, seam, heard}]} (heard is the cadence heard at the end
 * of the section), caller-owned.
 */
UrsatzGuiThemeResult ursatz_gui_piece_variations(const char *id, int version, const char *section_id, const char *lane,
                                                 int count, bool has_seed, long long seed, cJSON **out_result,
                                                 char *message, size_t message_size);

/*
 * Makes variation index of set `set` (made from version number) the next version: its loop plan is the new version's,
 * and the ending version is built from it again (`--regenerate none`, the eight outputs into the new folder). The body
 * is the source's with result replaced, the note "chose variation <index> seed <n>". NOT_FOUND when the set or the
 * variation is not there; otherwise as ursatz_gui_piece_regenerate.
 */
UrsatzGuiThemeResult ursatz_gui_piece_choose(const char *id, int version, int set, int index, cJSON **out_item,
                                             char *message, size_t message_size);

/*
 * One file of a variations set: result.json or variation-<i>.json, .mid, .musicxml. NOT_FOUND for any other name or a
 * file that is not there. *out_data is caller-owned (free); *out_type is a static content type.
 */
UrsatzGuiThemeResult ursatz_gui_piece_variation_file(const char *id, int version, int set, const char *name,
                                                     unsigned char **out_data, size_t *out_size, const char **out_type);

#endif /* URSATZ_GUI_PIECE_SERVICE_H */
