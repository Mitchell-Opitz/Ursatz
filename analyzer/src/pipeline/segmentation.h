#ifndef URSATZ_ANALYZER_SEGMENTATION_H
#define URSATZ_ANALYZER_SEGMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"

/*
 * JUDGMENT CALL: Ursatz provides no logic for grouping NoteEvents into
 * Sonority-sized chunks -- this is new logic. Notes are grouped by
 * exact onset-time equality (same MusicalTime position), not general
 * temporal overlap. This keeps grouping simple and deterministic and
 * matches the common case of a stacked chord, at the cost of not
 * merging notes whose onsets differ slightly but whose durations
 * otherwise overlap (e.g. a suspension tied across a chord change).
 */
typedef struct {
    size_t *note_indices;
    size_t note_index_count;
} SonorityGroup;

typedef struct {
    SonorityGroup *groups;
    size_t group_count;
} SonorityGrouping;

/*
 * Groups notes (assumed time-ordered, per MIDIImporter's contract) by
 * exact onset-time equality. On success, *out is owned by the caller
 * (release with sonority_grouping_destroy). Returns false only on
 * allocation failure.
 */
bool segment_by_onset(NoteEvent *const *events, size_t count, SonorityGrouping *out);
void sonority_grouping_destroy(SonorityGrouping *grouping);

#endif
