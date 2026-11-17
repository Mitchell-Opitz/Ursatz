#ifndef MUSIC_SEMANTICS_CONTOUR_INTERNAL_H
#define MUSIC_SEMANTICS_CONTOUR_INTERNAL_H

#include "analysis/notes/contour.h"

/* The fields of a Contour: contour.c writes them; tests read them through tests/support/semantics_test_access.h. */
struct Contour {
    PitchIdentity **pitch_ids;
    size_t pitch_id_count;
    ContourDirection *directions;
    size_t direction_count;
};

#endif
