#ifndef URSATZ_ANALYZER_CHORD_PIPELINE_H
#define URSATZ_ANALYZER_CHORD_PIPELINE_H

#include <stdbool.h>

#include "events/note_event.h"
#include "pipeline/key_pipeline.h"
#include "pipeline/segmentation.h"
#include "pitch/pitch_numeric_value.h"

typedef struct {
    bool found;
    char label[32];
} ChordEstimate;

/*
 * Identifies the chord for one SonorityGroup, given the previously
 * estimated key -- ChordIdentification takes each note's scale degree
 * as input (analysis/chord_identification.h), it does not derive
 * degrees from raw pitch itself, so this function computes them from
 * the estimated key's tonic and mode.
 *
 * JUDGMENT CALL: a note whose pitch class is not diatonic in the
 * estimated key (e.g. a chromatic passing tone) has no scale degree
 * and is dropped from the set passed to ChordIdentification, rather
 * than failing the whole sonority over one non-diatonic note.
 *
 * If key is NULL or not found, no scale degrees can be computed and
 * this returns found = false without calling the analyzer.
 */
ChordEstimate identify_chord_for_group(const SonorityGroup *group, NoteEvent *const *events,
                                        PitchNumericValue *const *pitch_values, const KeyEstimate *key);

#endif
