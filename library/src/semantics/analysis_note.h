#ifndef MUSIC_SEMANTICS_ANALYSIS_NOTE_H
#define MUSIC_SEMANTICS_ANALYSIS_NOTE_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "pitch/pitch_spelling.h"
#include "time/meter_map.h"
#include "time/time_span.h"

/*
 * AnalysisNote: the neutral note every framework adapter accepts (R-242). Imports and chain stages build
 * it once; each adapter maps it field for field into its own input, so no caller writes the converter.
 *
 * All pointers are borrowed. event, spelling and voice_id may be NULL. midi is the sounding semitone
 * number (C4 = 60) or ANALYSIS_PITCH_UNKNOWN. track and voice may be ANALYSIS_INDEX_NONE. voice_confidence
 * is in [0, 1], and 1 for a voice the source gave.
 *
 * AnalysisNoteSet is a sorted run of notes (analysis_note_compare) read against one MeterMap.
 */

#define ANALYSIS_PITCH_UNKNOWN (-1)
#define ANALYSIS_INDEX_NONE ((size_t)-1)

typedef struct {
    const char *note_id;
    const NoteEvent *event;
    const TimeSpan *span;
    int midi;
    const PitchSpelling *spelling;
    const char *voice_id;
    size_t track;
    size_t voice;
    double voice_confidence;
} AnalysisNote;

typedef struct {
    const AnalysisNote *notes;
    size_t count;
    const MeterMap *map;
} AnalysisNoteSet;

/* Non-NULL id and span, a span that ends after it starts, midi -1 or 0 to 127, confidence in [0, 1]. */
bool analysis_note_valid(const AnalysisNote *note);

/* For qsort over AnalysisNote: span start, then midi, then note_id. */
int analysis_note_compare(const void *a, const void *b);

/* Every note valid, sorted by analysis_note_compare, and a non-NULL map. */
bool analysis_note_set_valid(const AnalysisNoteSet *set);

#endif
