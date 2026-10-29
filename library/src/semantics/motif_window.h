#ifndef MUSIC_SEMANTICS_MOTIF_WINDOW_H
#define MUSIC_SEMANTICS_MOTIF_WINDOW_H

#include <stddef.h>

#include "identity/entity_id.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"

/*
 * MotifWindow (Value, L6): one candidate-motif window: its Contour direction sequence, its RhythmicPattern duration
 * sequence and the ids of the events it covers. Borrowed: the window owns none of the three. The one window value of
 * the motif matching in analysis/motif_window_matching.h and of standard-form's injected window matcher
 * (StandardFormMotifWindow is this type).
 */
typedef struct {
    const Contour *contour;
    const RhythmicPattern *rhythm;
    const EntityID *const *event_ids;
    size_t event_id_count;
} MotifWindow;

#endif
