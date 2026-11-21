#ifndef URSATZ_REALIZATION_REALIZATION_JSON_INTERNAL_H
#define URSATZ_REALIZATION_REALIZATION_JSON_INTERNAL_H

/* What realization_json_write.c and realization_json_read.c share: the format name, the unit of a tempo, the names of the closed
   vocabularies the file spells out, and the error setter. Private to the group. */

#include <stddef.h>

#include "realization/realization_json.h"

#define FORMAT_NAME "ursatz-realization"
#define MICROSECONDS_PER_MINUTE 60000000LL
static const char *const RELATION_NAMES[] = {"parallel", "imitation", "answers", "call-response"};
static const char *const TRANSFORM_NAMES[] = {"none", "octave", "interval"};
static const char *const KEY_MODE_NAMES[] = {"major", "minor", "unknown"};
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* Records the problem and its path in `error` (NULL is allowed): printf-style message, cut to the buffer. */
void realization_json_set_error(RealizationJsonError *error, const char *path, const char *format, ...);

#endif
