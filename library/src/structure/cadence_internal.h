#ifndef MUSIC_STRUCTURE_CADENCE_INTERNAL_H
#define MUSIC_STRUCTURE_CADENCE_INTERNAL_H

#include "structure/cadence.h"

/* The fields of a Cadence: cadence.c writes them; tests read them through tests/support/structure_test_access.h. */
struct Cadence {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
    Value *optional_harmonic_context;
};

#endif
