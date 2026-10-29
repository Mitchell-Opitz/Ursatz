#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_VOICES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_VOICES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "semantics/voice_grid.h"

/*
 * The voice-pitch grid of the standard-harmony analysis adapter: each voice's sounding note in each chord segment, which
 * extends the neutral per-segment input (analysis_input.h) with per-voice pitches. It is the shared AnalysisVoiceGrid
 * (semantics/voice_grid.h), the same grid the standard-counterpoint adapter reads; the caller builds one from a voice assignment.
 *
 * One row per chord segment, in time order (the rows of the prepared segments, one to one), one column per voice. Voice 0 is
 * the TOP voice (the soprano) and the last voice is the bottom (the bass), as voice separation indexes them.
 * cells[segment * voice_count + voice]:
 *
 *   present      a note of this voice sounds in the segment; false when the voice rests
 *   pitch        the note's PitchNumericValue: a position in semitones under a 12-tone tuning (denominator 1)
 *   spelling     OPTIONAL: the note's PitchSpelling. Harmony reads semitones only, so a spelling is used only when `pitch`
 *                is NULL, to give the semitone (12 * (octave + 1) + letter base + accidental; MIDI middle C, 60, is C4)
 *   note_id      the note's id (non-empty when present); the id the Interpretations refer to. Borrowed
 *   attacked     the note was attacked in this segment; false when it was held over from an earlier segment
 *   confidence   [0, 1], the voice assignment's confidence for the note (voice separation's), used in uncertainty
 *
 * segment_ids is not read here: the segments carry their ids. Every pointer is borrowed.
 */

typedef AnalysisVoiceCell HarmonyVoiceCell;
typedef AnalysisVoiceGrid HarmonyVoiceGrid;

/* The semitone of a present cell, or false (*out untouched) for a cell with no usable pitch (no pitch and no spelling, or a
   position that is not a whole semitone). */
bool harmony_voice_cell_semitone(const HarmonyVoiceCell *cell, long *out);

#endif
