#ifndef MUSIC_SEMANTICS_VOICE_GRID_H
#define MUSIC_SEMANTICS_VOICE_GRID_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"

/*
 * AnalysisVoiceGrid: each voice's sounding note in each chord segment, the one grid the harmony and
 * counterpoint adapters accept (R-242). One row per segment in time order, one column per voice; voice 0
 * is the TOP voice (the soprano) and the last voice is the bottom (the bass), as voice separation indexes
 * them. cells is row-major by segment: cells[segment * voice_count + voice].
 *
 * A cell:
 *   present     a note of this voice sounds in the segment; false when the voice rests
 *   pitch       the note's PitchNumericValue: a semitone position under a 12-tone tuning (denominator 1)
 *   spelling    OPTIONAL: the note's PitchSpelling. How it combines with `pitch` is the adapter's rule
 *   note_id     the note's id (non-empty when present). Borrowed
 *   attacked    the note was attacked in this segment; false when held over from an earlier one
 *   confidence  [0, 1], the voice assignment's confidence for the note
 *
 * segment_ids names each segment and may be NULL when the adapter's segments carry their ids. Every
 * pointer is borrowed.
 */
typedef struct {
    bool present;
    const PitchNumericValue *pitch;
    const PitchSpelling *spelling;
    const char *note_id;
    bool attacked;
    double confidence;
} AnalysisVoiceCell;

typedef struct {
    size_t segment_count;
    size_t voice_count;
    const char *const *segment_ids;
    const AnalysisVoiceCell *cells;
} AnalysisVoiceGrid;

/* The cell of a segment and voice, or NULL when either is out of range or the grid has no cells. */
const AnalysisVoiceCell *analysis_voice_grid_cell(const AnalysisVoiceGrid *grid, size_t segment, size_t voice);

#endif
