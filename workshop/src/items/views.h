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

/* The range params of a view call: `measures` ({first, last}, either may be absent) and `section`. */
UrsatzStatus workshop_view_limits(const cJSON *params, TextViewLimits *out, UrsatzError *error);

/* A TextError as the failure of a view call: the path follows the message (view, section or measures). */
UrsatzStatus workshop_view_fail(UrsatzError *error, const TextError *text_error);

#endif
