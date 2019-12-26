#ifndef URSATZ_ANALYZER_KEY_PIPELINE_H
#define URSATZ_ANALYZER_KEY_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"

typedef struct {
    bool found;
    char label[32]; /* e.g. "C major", copied out of KeyEstimation's claim */
    int tonic_pitch_class; /* 0-11, valid only when found */
    bool is_major;         /* valid only when found */
} KeyEstimate;

/*
 * JUDGMENT CALL: this task treats the whole imported note sequence as
 * a single region and estimates one global key for it, rather than
 * segmenting into per-measure/per-phrase regions -- KeyEstimation
 * itself does not propose regions.
 *
 * Precedence: when key_signature.found is true (and carries a
 * plausible -7..7 sharps/flats count), the tonic and mode are derived
 * directly from it via a circle-of-fifths table -- no pitch-class
 * scanning at all. This takes silent precedence over every other
 * source; a malformed/out-of-range key signature is treated the same
 * as key_signature.found == false.
 *
 * Otherwise, this function falls back to a brute-force search: it does
 * not search candidate tonics itself per analysis/key_estimation.h (the
 * caller supplies the candidate), so it tries each of the 12 pitch
 * classes as candidate tonic in order (C, C#, D, ...) and reports the
 * first one whose relative pitch-class collection KeyEstimation
 * recognizes as major or natural minor. If none match, found is false.
 */
KeyEstimate estimate_global_key(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                 MidiKeySignature key_signature);

#endif
