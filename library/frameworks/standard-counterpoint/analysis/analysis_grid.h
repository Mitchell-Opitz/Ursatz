#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_GRID_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_ANALYSIS_ANALYSIS_GRID_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"

/*
 * The standard-counterpoint analysis adapter (L3): turns separated voices into this framework's inputs and returns
 * Interpretations. This file is the input, the VOICE-PITCH GRID. standard-counterpoint does not include
 * standard-harmony: standard-harmony's adapter defines the same grid in its own header with identical semantics, and
 * the caller (tests/integration/) builds both from the same voice assignment.
 *
 * The grid has one row per chord segment, in time order, and one column per voice. Voice 0 is the TOP voice (the
 * soprano) and the last voice is the bottom (the bass), as voice separation (analysis/voice_separation.h) indexes
 * them. cells[segment * voice_count + voice] is the voice's sounding note in that segment:
 *
 *   present      a note of this voice sounds in the segment; false when the voice rests
 *   pitch        the note's PitchNumericValue (a position in semitones under a 12-tone tuning, denominator 1)
 *   spelling     OPTIONAL: the note's PitchSpelling. The rules count motion in letter names and read interval
 *                quality from spellings, so a spelling is used when given (it wins over `pitch`). With none, the
 *                spelling is DERIVED from the semitone with a sharp-preferring default (C C-sharp D D-sharp E F F-sharp
 *                G G-sharp A A-sharp B; MIDI middle C, 60, is C4): right for the white keys and the common sharps, but a
 *                flat key's D-flat or E-flat is spelled C-sharp or D-sharp, so a caller with spellings should pass them
 *   note_id      the note's id (non-empty when present); the id the Interpretations refer to. Borrowed
 *   attacked     the note was attacked in this segment; false when it was held over from an earlier segment
 *   confidence   [0, 1], the voice assignment's confidence for the note (voice separation's), used in uncertainty
 *
 * segment_ids names each segment (its Sonority id). Every pointer is borrowed: the caller keeps the grid, its cells and
 * the PitchNumericValues, PitchSpellings and strings alive while it is used.
 *
 * Motion is evaluated between CONSECUTIVE segments, for each pair of voices where both voices sound in both segments.
 *
 * PREPARING. standard_counterpoint_grid_prepare validates the grid and gives each present cell an OWNED PitchSpelling
 * (a copy of the given one, or the derived one) and its semitone, so the rules read one representation.
 */

typedef struct {
    bool present;
    const PitchNumericValue *pitch;
    const PitchSpelling *spelling;
    const char *note_id;
    bool attacked;
    double confidence;
} StandardCounterpointVoiceCell;

typedef struct {
    size_t segment_count;
    size_t voice_count;
    const char *const *segment_ids;
    const StandardCounterpointVoiceCell *cells;
} StandardCounterpointVoiceGrid;

typedef struct {
    const StandardCounterpointVoiceGrid *grid;
    PitchSpelling **spellings; /* owned; segment_count * voice_count entries, NULL where the voice rests */
} StandardCounterpointPreparedGrid;

typedef enum {
    STANDARD_COUNTERPOINT_GRID_OK = 0,
    STANDARD_COUNTERPOINT_GRID_ERR_NULL_ARGUMENT,
    STANDARD_COUNTERPOINT_GRID_ERR_BAD_GRID, /* no segments or voices, a missing id, a present cell with no pitch or
                                                note id, a confidence outside [0, 1], or a pitch that is not a whole
                                                semitone */
    STANDARD_COUNTERPOINT_GRID_ERR_ALLOC
} StandardCounterpointGridStatus;

/* On OK, *out is owned by the caller (standard_counterpoint_grid_destroy); otherwise untouched. */
StandardCounterpointGridStatus standard_counterpoint_grid_prepare(const StandardCounterpointVoiceGrid *grid,
                                                                  StandardCounterpointPreparedGrid *out);

void standard_counterpoint_grid_destroy(StandardCounterpointPreparedGrid *prepared);

/* The cell and its spelling, or NULL for a segment or voice out of range and for a resting voice. */
const StandardCounterpointVoiceCell *standard_counterpoint_grid_cell(const StandardCounterpointPreparedGrid *prepared,
                                                                     size_t segment, size_t voice);
const PitchSpelling *standard_counterpoint_grid_spelling(const StandardCounterpointPreparedGrid *prepared,
                                                         size_t segment, size_t voice);

#endif
