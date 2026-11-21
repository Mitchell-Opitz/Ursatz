#ifndef URSATZ_WORKSHOP_BASE_PARAMS_H
#define URSATZ_WORKSHOP_BASE_PARAMS_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>
#include "base/last_error.h"

/*
 * Reading parameters and normalizing the text a caller gives, shared by the coordinator, the items, composition and rendering
 * (R-691). A parameter is a member of a call's params object or of a request body.
 */

/* The string member of params, NULL when it is absent or null; *bad (when not NULL) is set when it is present but not a string. */
const char *workshop_param_string(const cJSON *params, const char *name, bool *bad);

/* The string member; refuses the call (a REQUEST at "/params/<name>") when it is absent or not a string. */
bool workshop_param_require_string(const cJSON *params, const char *name, const char **out, UrsatzError *error);

/*
 * A copy of text (NULL is "") with the whitespace around it trimmed. It is refused with `refusal` (a REQUEST) when it has more than
 * max_chars UTF-8 characters, holds a control character, or, when `required`, is empty after the trim; with "out of memory" (ALLOC)
 * when the copy fails. On success *out is the copy (the caller frees it); on a refusal it is NULL.
 */
UrsatzStatus workshop_text_normalize(const char *text, size_t max_chars, bool required, const char *refusal, char **out);

#endif
