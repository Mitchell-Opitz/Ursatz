#ifndef URSATZ_REALIZATION_DERIVATION_H
#define URSATZ_REALIZATION_DERIVATION_H

#include <stdbool.h>
#include <stddef.h>

#include "realization/realization.h"

/*
 * Where a note came from (R-601): the ids of the notes it was made from, the operation that made it and the run that did. A value
 * with no heap: copy it. A zeroed struct is "made from nothing" (independent material). Operation names used in P59: `score` (a
 * performer note made from a line note), `copy`, `motive` and `return` (the plan's links, mapped by the Composer's export).
 */

#define NOTE_DERIVATION_SOURCES_MAX 4

typedef struct {
    char sources[NOTE_DERIVATION_SOURCES_MAX][REALIZATION_ID_MAX];
    size_t source_count;
    char operation[32]; /* "" when none, else ^[a-z]+(-[a-z]+)*$ */
    char run[64];       /* "" when not recorded, else printable text */
} NoteDerivation;

/* Appends a source note id. BAD_ID for an invalid id, BAD_VALUE when the sources are full, NULL for a NULL argument. */
RealizationResult note_derivation_add_source(NoteDerivation *derivation, const char *note_id);

/* Sets the operation name: lowercase words joined by hyphens, at most 31 characters (BAD_VALUE otherwise). */
RealizationResult note_derivation_set_operation(NoteDerivation *derivation, const char *name);

/* Sets the run: printable text of at most 63 characters (BAD_VALUE otherwise); "" means not recorded. */
RealizationResult note_derivation_set_run(NoteDerivation *derivation, const char *run);

/* True when the derivation names a source, an operation or a run. False for NULL. */
bool note_derivation_present(const NoteDerivation *derivation);

#endif
