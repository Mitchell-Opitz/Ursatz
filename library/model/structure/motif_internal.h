#ifndef MUSIC_STRUCTURE_MOTIF_INTERNAL_H
#define MUSIC_STRUCTURE_MOTIF_INTERNAL_H

#include "model/structure/motif.h"

/* The fields of a Motif: motif.c writes them; tests read them through tests/support/structure_test_access.h. */
struct Motif {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

#endif
