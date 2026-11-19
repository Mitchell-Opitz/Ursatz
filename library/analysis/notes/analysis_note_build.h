#ifndef MUSIC_SEMANTICS_ANALYSIS_NOTE_BUILD_H
#define MUSIC_SEMANTICS_ANALYSIS_NOTE_BUILD_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/notes/analysis_note.h"
#include "model/events/note_event.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/time/meter_map.h"

/*
 * The one builder of the neutral note set (R-614): imports, the chain and the library tests all read their notes from it.
 *
 * AnalysisNoteSource borrows parallel arrays of count events and their sounding pitches. tracks (NULL for none) holds each
 * note's track or part index; a NULL tracks gives ANALYSIS_INDEX_NONE. use_spelling false leaves every spelling NULL, true
 * borrows the event's notated one. map is the meter map the set is read against (borrowed, may be NULL here but a set read
 * by the chain needs one).
 */
typedef struct {
    const NoteEvent *const *events;
    PitchNumericValue *const *pitch_values;
    const size_t *tracks;
    size_t count;
    bool use_spelling;
    const MeterMap *map;
} AnalysisNoteSource;

/*
 * Builds *out: one AnalysisNote per event, sorted by analysis_note_compare, with the source's map. *order (when non-NULL)
 * is a new array in which order[p] is the source index of sorted note p. Both arrays are new allocations; free them with
 * analysis_note_set_built_destroy. Borrowed from the source: events, note ids, spellings and voice ids.
 *
 * Fails (false, *out and *order untouched) on a NULL argument, an allocation failure, or an event that makes no valid
 * AnalysisNote (a zero-length span or a pitch outside 0 to 127). A source with no notes gives an empty set (NULL notes, count
 * 0) and a NULL order.
 */
bool analysis_note_set_build(const AnalysisNoteSource *source, AnalysisNoteSet *out, size_t **order);

/* Frees what analysis_note_set_build allocated (the set's notes and the order) and clears the set. NULL is allowed. */
void analysis_note_set_built_destroy(AnalysisNoteSet *set, size_t *order);

#endif
