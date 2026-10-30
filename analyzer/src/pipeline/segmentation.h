#ifndef URSATZ_ANALYZER_SEGMENTATION_H
#define URSATZ_ANALYZER_SEGMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/pitch_class_util.h"
#include "semantics/sonority_segmentation.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "common-practice-minimal/weighted_chord_identification.h"

/*
 * One sonority segment of the library chain (analysis_chain_segments, analysis_chain_voice_segments: Ursatz's adaptive
 * segmentation on the beat grid of the file's MeterMap): every note sounding in its time window, those attacked in it
 * and those held over from earlier.
 *
 * note_indices are ImportedNotes indices, parallel to members. The members are the plain per-note view Ursatz's
 * weighted chord identification reads (pitch, duration inside the segment, held flag, beat strength of the attack,
 * voice, onset); their note_id and voice_id borrow from the ImportedNotes. A Segmentation is a view onto the chain's
 * segments: the chain must outlive it.
 */
typedef struct {
    size_t *note_indices;
    CommonPracticeMember *members;
    size_t member_count;
    long start_numerator; /* window start, whole notes */
    unsigned long start_denominator;
    long end_numerator; /* window end (exclusive), whole notes */
    unsigned long end_denominator;
} Segment;

typedef struct {
    Segment *segments;
    size_t segment_count;
    CommonPracticeSegment *library_segments; /* parallel to segments; the chain's, borrowed */
    char (*segment_ids)[32];                 /* backs library_segments[i].id; the chain's, borrowed */
    AdaptiveSonorities sonorities;           /* parallel to segments; the library's own segments, borrowed */
} Segmentation;

/* Pitch class (0-11) of the lowest note sounding in a segment. */
static inline int segment_bass_pitch_class(const Segment *segment) {
    int lowest = segment->members[0].pitch;
    for (size_t i = 1; i < segment->member_count; i++) {
        if (segment->members[i].pitch < lowest) {
            lowest = segment->members[i].pitch;
        }
    }
    return pitch_class_of(lowest);
}

#endif
