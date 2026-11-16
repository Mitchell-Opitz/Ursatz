#ifndef MUSIC_STRUCTURE_PHRASE_OCCURRENCE_INTERNAL_H
#define MUSIC_STRUCTURE_PHRASE_OCCURRENCE_INTERNAL_H

#include "structure/phrase_occurrence.h"

/* The fields of a PhraseOccurrence: phrase_occurrence.c writes them; tests read them through tests/support/structure_test_access.h. */
struct PhraseOccurrence {
    Occurrence *occurrence;
    EntityID **region_event_ids;
    size_t region_event_id_count;
};

#endif
