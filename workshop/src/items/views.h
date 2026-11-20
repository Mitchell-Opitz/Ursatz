#ifndef URSATZ_WORKSHOP_ITEMS_VIEWS_H
#define URSATZ_WORKSHOP_ITEMS_VIEWS_H

#include <cJSON.h>

#include <common/status.h>
#include <text/text_view.h>

/*
 * Ursatz Text from the workshop (R-662, R-663): the tool "text" reads a file or a text of any layer kind, and the queries item.view and
 * library.view read the layers a version or a library piece holds. Every answer is a tree the caller owns. A view a layer cannot give is
 * URSATZ_ERR_REQUEST at "/params/view" with the message of text_view_print; an unknown section is "/params/section" and a measure range
 * outside the piece "/params/measures".
 */

/*
 * The tool: exactly one of `path` (a file) and `text`. The input is JSON when its first non-space character is '{', its kind from "format"
 * or "schema"; otherwise Ursatz Text, its kind from the header (a text is converted with text_to_json first). *out is
 * {"kind": "<kind>", "text": "<the view>"}, or for "json" {"kind", "json": "<the codec's JSON text>"}. "full" prints the input's layer; the
 * other views read the layer the file is. A file that does not read is URSATZ_ERR_NOT_FOUND at "/params/path"; a text that does not parse is
 * URSATZ_ERR_REQUEST at "/params/text" (or "/params/path" for a file) with "line <n>: <message>".
 */
UrsatzStatus workshop_view_input(const char *path, const char *text, const char *view, const TextViewLimits *limits, cJSON **out,
                                 UrsatzError *error);

/*
 * The views of a version of an item: params of item.view are read by the handler. Reads the version folder's plan.json, realization.json and
 * readings/composer.json (the ones present); `view` is a view of text_view.h, "full" (each present layer's text, plan first, joined by a
 * blank line; with no layer the version's body as kind json) or "json" (answered by the handler). *text is the answer's text. A version
 * with no layer refuses every view but full with URSATZ_ERR_NOT_FOUND at "/params/view": "this version holds no plan, realization or
 * reading". An item or version that is not there is NOT_FOUND.
 */
UrsatzStatus workshop_view_item(const char *id, int version, const char *view, const TextViewLimits *limits, char **text, UrsatzError *error);

/*
 * The views of the layers a folder holds: plan.json (when with_plan), realization.json and `reading_file` (a path in the folder, such as
 * "readings/composer.json"), the ones present. `view` is a view of text_view.h or "full" (each present layer's text, plan first, joined by a blank
 * line). *none is true, with OK and no text, when the folder holds no layer. The callers make the answer: workshop_view_item here, the piece's
 * in analysis/piece_view.h.
 */
UrsatzStatus workshop_view_folder(const char *folder, bool with_plan, const char *reading_file, const char *address, const char *view,
                                  const TextViewLimits *limits, char **text, bool *none, UrsatzError *error);

/* The range params of a view call: `measures` ({first, last}, either may be absent) and `section`. */
UrsatzStatus workshop_view_limits(const cJSON *params, TextViewLimits *out, UrsatzError *error);

/* A TextError as the failure of a view call: the path follows the message (view, section or measures). */
UrsatzStatus workshop_view_fail(UrsatzError *error, const TextError *text_error);

#endif
