#ifndef URSATZ_TESTS_SUPPORT_REALIZATION_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_REALIZATION_TEST_ACCESS_H

#include <string.h>

#include "realization/performer_part.h"

/* What the tests do to a PerformerPart beyond its public entries: set the flags a person's edit leaves on a note. */

/* Sets the edited and detached flags of the note with this id. UNKNOWN_REFERENCE when the part has no such note. */
static inline RealizationResult performer_part_set_note_flags(PerformerPart *part, const char *note_id, bool edited, bool detached) {
    if (part == NULL || note_id == NULL) {
        return REALIZATION_ERR_NULL;
    }
    for (size_t i = 0; i < performer_part_note_count(part); i++) {
        PerformerNote *note = (PerformerNote *)performer_part_note_at(part, i);
        if (strcmp(note->id, note_id) == 0) {
            note->edited = edited;
            note->detached = detached;
            return REALIZATION_OK;
        }
    }
    return REALIZATION_ERR_UNKNOWN_REFERENCE;
}

#endif
