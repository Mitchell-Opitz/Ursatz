#ifndef URSATZ_CHAIN_CHAIN_INPUTS_H
#define URSATZ_CHAIN_CHAIN_INPUTS_H

#include <stdbool.h>
#include <stddef.h>

#include "key_regions.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_note.h"
#include "semantics/beat_strength.h"
#include "semantics/sonority_segmentation.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "weighted_chord_identification.h"

/*
 * The chain's input builders (R-241): each turns neutral notes (semantics/analysis_note.h) and the results of an earlier stage into the
 * plain input one framework reads. Every default is the library's; nothing is chosen here. Each builder owns what it allocates and has a
 * matching destroy; a failed builder leaves its output untouched.
 */

typedef enum {
    CHAIN_INPUT_OK = 0,
    CHAIN_INPUT_ERR_NULL_ARGUMENT,
    CHAIN_INPUT_ERR_BAD_NOTES,    /* an invalid note set, or no note */
    CHAIN_INPUT_ERR_NO_EVENT,     /* a note without its NoteEvent, which segmentation needs */
    CHAIN_INPUT_ERR_SEGMENTATION, /* adaptive segmentation failed */
    CHAIN_INPUT_ERR_UNKNOWN_NOTE, /* a segment names a note that is not in the set */
    CHAIN_INPUT_ERR_MEMBER,       /* a note that does not sound in a segment it belongs to */
    CHAIN_INPUT_ERR_WEIGHTS,
    CHAIN_INPUT_ERR_KEYS,
    CHAIN_INPUT_ERR_ALLOC
} ChainInputStatus;

/*
 * The segments of a piece: the adaptive sonority segments (the owner, sonority_builder_build_adaptive, on the standard-meter beat grid),
 * each one's id ("seg-<n>") and window in whole notes, and, once chain_build_members has run, the members of every segment.
 */
typedef struct {
    AdaptiveSonorities sonorities;       /* owned */
    char (*ids)[32];                     /* owned; count ids */
    double *starts;                      /* owned; each segment's window in whole notes */
    double *ends;
    size_t count;
    CommonPracticeMember *members;       /* owned; every segment's members, one after another; NULL before chain_build_members */
    CommonPracticeSegment *segments;     /* owned; count entries viewing members; NULL before chain_build_members */
} ChainSegments;

/* Segments the notes. config may be NULL for the library's defaults. Every note needs its event. */
ChainInputStatus chain_build_segments(const AnalysisNoteSet *notes, const AdaptiveSegmentationConfig *config, ChainSegments *out);

/*
 * The strength of an attack at `onset`, read in the signature of its measure (standard-meter's beat strength of the metrical position);
 * weak when it cannot be read. This is the one function the test glue copied twice (weighted_pipeline.h and chorale_chain.c).
 */
BeatStrength chain_attack_strength(const MeterMap *map, const MusicalTime *onset);

/*
 * The members of every segment, through common_practice_member_from_analysis_note: the pitch (midi), the duration sounding inside the
 * segment, the held flag the segmentation recorded, the BeatStrength of the attack (strong for a held note), the voice id and the
 * onset. `notes` must be the set the segments were built from.
 */
ChainInputStatus chain_build_members(const AnalysisNoteSet *notes, ChainSegments *segments);

/* Destroys the sonorities, ids, windows, members and segments. NULL is a no-op; the struct is zeroed. */
void chain_segments_destroy(ChainSegments *segments);

/* The weighted key segment of each of the first `count` segments (window, weights, source members) for key_regions_estimate. weighting
   may be NULL for the defaults. Needs the members. Free with chain_key_segments_destroy. */
ChainInputStatus chain_build_key_segments(const ChainSegments *segments, size_t count, const CommonPracticeWeightedConfig *weighting,
                                          CommonPracticeKeySegment **out);
void chain_key_segments_destroy(CommonPracticeKeySegment *key_segments);

/*
 * The local key regions (the owner, key_regions_estimate) with the key signature events as AnalysisKeyEvents. events may be NULL with
 * event_count 0. config may be NULL for the defaults. Destroy with common_practice_key_regions_destroy.
 */
ChainInputStatus chain_estimate_key_regions(const CommonPracticeKeySegment *key_segments, size_t count, const MeterMap *map,
                                            const AnalysisKeyEvent *events, size_t event_count,
                                            const CommonPracticeKeyRegionsConfig *config, CommonPracticeKeyRegions *out);

#endif
