#ifndef URSATZ_ANALYZER_KEY_PIPELINE_H
#define URSATZ_ANALYZER_KEY_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>

#include "events/note_event.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"

typedef struct {
    bool found;
    char *label; /* owned; e.g. "C major", copied out of KeyEstimation's claim; NULL unless found */
    int tonic_pitch_class; /* 0-11, valid only when found */
    bool is_major;         /* valid only when found */
    AnalysisKeyMethod method;      /* step that found the key; NONE unless found */
    AnalysisConfidence confidence; /* from KeyEstimation's Interpretation; absent for a key signature */
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
 * Otherwise, priority_tonics (priority_tonic_count entries, each 0-11,
 * duplicates allowed) is tried next, in order -- meant for the bass
 * pitch class of the piece's first and last chord, the caller's best
 * guess at the tonic when no key signature is present. The first
 * priority tonic whose relative pitch-class collection KeyEstimation
 * recognizes as major or natural minor wins.
 *
 * Only if every priority tonic misses does this function fall back to
 * a brute-force search: it does not search candidate tonics itself per
 * analysis/key_estimation.h (the caller supplies the candidate), so it
 * tries each of the 12 pitch classes as candidate tonic in order (C,
 * C#, D, ...) and reports the first one whose relative pitch-class
 * collection KeyEstimation recognizes as major or natural minor. If
 * none match, found is false.
 *
 * Fills *out and returns true; returns false only when the label could
 * not be allocated. Release *out with key_estimate_destroy.
 */
bool estimate_global_key(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                         MidiKeySignature key_signature, const int *priority_tonics, size_t priority_tonic_count,
                         KeyEstimate *out);

void key_estimate_destroy(KeyEstimate *estimate);

#endif
