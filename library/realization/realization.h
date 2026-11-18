#ifndef URSATZ_REALIZATION_REALIZATION_H
#define URSATZ_REALIZATION_REALIZATION_H

#include <stdbool.h>

/*
 * The realization group (target architecture 4.8, 4.11; R-600): music held as lines, scoring assignments and performer parts, then
 * as movements and works. Every id in it is a short text of the form below, so a note id can be written unchanged as a MusicXML
 * xml:id. A result of the group's functions is one of these; nothing changes on a failure.
 */

#define REALIZATION_ID_MAX 64 /* an id and its NUL: at most 63 characters */

typedef enum {
    REALIZATION_OK,
    REALIZATION_ERR_NULL,
    REALIZATION_ERR_BAD_ID,
    REALIZATION_ERR_DUPLICATE_ID,
    REALIZATION_ERR_BAD_VALUE,
    REALIZATION_ERR_UNKNOWN_REFERENCE,
    REALIZATION_ERR_NOT_BUILT,
    REALIZATION_ERR_ALLOC
} RealizationResult;

/* True when `id` matches ^[A-Za-z][A-Za-z0-9_.-]{0,62}$. False for NULL. */
bool realization_id_valid(const char *id);

#endif
