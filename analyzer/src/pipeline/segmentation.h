#ifndef URSATZ_ANALYZER_SEGMENTATION_H
#define URSATZ_ANALYZER_SEGMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/midi_pipeline.h>

#include "weighted_chord_identification.h"

/*
 * One sonority segment cut by Ursatz's adaptive segmentation
 * (semantics/sonority_segmentation.h): every note sounding in its time
 * window, those attacked in it and those held over from earlier.
 *
 * note_indices are ImportedNotes indices, parallel to members. The
 * members are the plain per-note view Ursatz's weighted chord
 * identification reads (pitch, duration inside the segment, held flag,
 * beat strength of the attack, voice, onset); their note_id and voice_id
 * borrow from the ImportedNotes, which must outlive the Segmentation.
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
    CommonPracticeSegment *library_segments; /* parallel to segments; views onto their members */
    char (*segment_ids)[32];                 /* backs library_segments[i].id */
} Segmentation;

/*
 * Cuts notes into sonority segments on the beat grid of the file's
 * MeterMap using Ursatz's adaptive segmentation, and builds the weighted
 * identification members for each. On success *out is owned by the
 * caller (release with segmentation_destroy). Returns false on a library
 * or allocation failure, with *out left empty.
 */
bool segment_notes(const ImportedNotes *notes, Segmentation *out);
void segmentation_destroy(Segmentation *segmentation);

#endif
