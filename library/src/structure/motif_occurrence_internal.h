#ifndef MUSIC_STRUCTURE_MOTIF_OCCURRENCE_INTERNAL_H
#define MUSIC_STRUCTURE_MOTIF_OCCURRENCE_INTERNAL_H

#include "structure/motif_occurrence.h"

/* The fields of a MotifOccurrence: motif_occurrence.c writes them; tests read them through tests/support/structure_test_access.h. */
struct MotifOccurrence {
    Occurrence *occurrence;
    EntityID **region_event_ids;
    size_t region_event_id_count;
};

#endif
