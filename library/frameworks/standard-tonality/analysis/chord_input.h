#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_CHORD_INPUT_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ANALYSIS_CHORD_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/chord.h"
#include "interpretation/key.h"
#include "standard-tonality/roman_numeral_analysis_rules.h"
#include "time/musical_time.h"

/*
 * The standard-tonality analysis adapter (L3): turns L2 facts into this framework's inputs. This file is
 * the first step: it builds, for each chord segment, the plain record the Roman numeral rules are read
 * from. It reads KERNEL-LEVEL data only and includes no other framework:
 *
 *   - the segment's kernel Chord Interpretation (interpretation/chord.h) and its ChordStructure: the root
 *     pitch class, the quality as a root-relative interval set, and the bass pitch class;
 *   - the kernel Key Interpretation of each key region (interpretation/key.h) and its KeyStructure: the
 *     tonic pitch class and the mode;
 *   - the segment's 12 pitch-class weights, as a plain array (index 0 = C ... 11 = B).
 *
 * BASS. When the segment passes its sounding pitches, bass_pitch_class is the pitch class of the lowest
 * sounding pitch that is a member of the identified chord: lower non-chord tones are ignored. When no
 * sounding pitch is a chord member, the previous segment's bass is kept if it is a member of this chord;
 * otherwise the bass is -1 (read as root position). A segment passing no pitches keeps the Chord's own
 * bass pitch class. sounding_bass_pitch_class always keeps the Chord's own bass pitch class.
 *
 * Whoever produced the Chords and Keys (common-practice-minimal does) owns them; the adapter only borrows.
 *
 * LOCAL KEY. Each segment is read in the key region that contains the segment's START time. Regions are
 * given in time order and each runs from its start to the next region's start; the last runs on with no
 * end. A segment that starts before the first region has no key.
 *
 * NO NUMERAL. A segment passes through as no-numeral (has_numeral false) when it has no Chord (the chord
 * identification found no match), when its Chord carries no structure, when it has no key region or its
 * Key carries no structure. It is kept in the output so the output lines up with the input.
 *
 * UNCERTAINTY. chord_uncertainty is 1 minus the Chord Interpretation's scalar confidence (1 for any other
 * kind of uncertainty). key_uncertainty is the region's own, as passed in: a Key's Interpretation does not
 * carry the region's uncertainty, the region does.
 */

typedef struct {
    const Key *key;            /* borrowed; the region's kernel Key */
    const MusicalTime *start;  /* borrowed; the region starts here (inclusive) */
    double uncertainty;        /* [0, 1] */
} StandardTonalityKeyRegionInput;

typedef struct {
    const char *segment_id;    /* borrowed; the id of the segment's Sonority, the target of its Interpretations */
    const MusicalTime *start;  /* borrowed */
    const Chord *chord;        /* borrowed; NULL when no chord was identified */
    const double *weights;     /* borrowed; the 12 pitch-class weights, or NULL when not available */
    const int *pitches;        /* borrowed; the sounding pitches (semitone numbers, any order), or NULL */
    size_t pitch_count;
} StandardTonalitySegmentInput;

#define STANDARD_TONALITY_CHORD_INPUT_MAX_INTERVALS CHORD_STRUCTURE_MAX_INTERVALS

typedef struct {
    const char *segment_id;
    const MusicalTime *start;
    bool has_numeral;          /* false: pass through as no-numeral; nothing below is meaningful */
    const Chord *chord;        /* borrowed */
    int root_pitch_class;      /* 0..11 */
    int bass_pitch_class;      /* the lowest sounding chord tone, 0..11, or -1 when unknown (read as root position) */
    int sounding_bass_pitch_class; /* the Chord's own bass pitch class (the lowest sounding note, chord tone or not),
                                      0..11, or -1; the applied-chord reading scores candidates against it */
    int intervals[STANDARD_TONALITY_CHORD_INPUT_MAX_INTERVALS]; /* root-relative semitones, ascending */
    size_t interval_count;
    int tonic_pitch_class;     /* the local key's tonic, 0..11 */
    StandardTonalityRomanKeyMode mode;
    size_t region_index;       /* the key region read, an index into the regions passed in */
    double chord_uncertainty;
    double key_uncertainty;
    bool has_weights;
    double weights[12];
} StandardTonalityChordInput;

typedef enum {
    STANDARD_TONALITY_CHORD_INPUT_OK = 0,
    STANDARD_TONALITY_CHORD_INPUT_ERR_NULL_ARGUMENT,
    STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_REGION,   /* a region with no Key or start, or out of time order */
    STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_SEGMENT,  /* a segment with no id or start */
    STANDARD_TONALITY_CHORD_INPUT_ERR_ALLOC
} StandardTonalityChordInputStatus;

/*
 * One StandardTonalityChordInput per segment, in the segment order given. Segments need not be sorted;
 * regions must be in time order with distinct starts. region_count may be 0 (every segment is then
 * no-numeral). On OK, *out is owned by the caller (standard_tonality_chord_inputs_destroy) and
 * *out_count holds segment_count; otherwise both are untouched.
 */
StandardTonalityChordInputStatus standard_tonality_build_chord_inputs(
    const StandardTonalitySegmentInput *segments, size_t segment_count,
    const StandardTonalityKeyRegionInput *regions, size_t region_count,
    StandardTonalityChordInput **out, size_t *out_count);

void standard_tonality_chord_inputs_destroy(StandardTonalityChordInput *inputs);

#endif
