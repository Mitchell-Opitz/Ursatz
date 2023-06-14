#ifndef MUSIC_SEMANTICS_SONORITY_SEGMENTATION_INTERNAL_H
#define MUSIC_SEMANTICS_SONORITY_SEGMENTATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "semantics/sonority_segmentation.h"
#include "time/exact_rational.h"

/*
 * Internal to adaptive sonority segmentation: the three algorithm steps as separate functions, so each
 * has its own unit tests. Not part of the public API (semantics/sonority_segmentation.h is): tests
 * include this header, nothing else should.
 */

/* One input note, reduced to what the steps need. */
typedef struct {
    ExactRational onset;
    ExactRational release; /* onset + duration */
    int pitch;             /* semitone number, C4 = 60 */
    int pitch_class;       /* pitch mod 12, 0..11 */
} NoteSpan;

/* One note sounding in a segment. */
typedef struct {
    size_t note;    /* index into the NoteSpan array */
    bool attacked;  /* attacked in the segment; false means held over from before it */
} SegmentMember;

typedef struct {
    ExactRational start;
    ExactRational end;
    SegmentMember *members; /* owned; NULL when member_count is 0 */
    size_t member_count;
    bool starts_measure; /* the segment starts at a barline */
} Segment;

typedef struct {
    Segment *items; /* owned; NULL when count is 0 */
    size_t count;
} SegmentList;

void segment_list_destroy(SegmentList *list);

/* Reduces notes to NoteSpans: onset, release and the pitch from the PitchSpelling. On OK, *out is a
   newly allocated array of note_count entries that the caller frees. */
AdaptiveSegmentationResult segmentation_prepare_notes(NoteEvent *const *notes, size_t note_count, NoteSpan **out);

/* The members of the span [start, end): every note with onset < end and release > start, in note
   order. attacked is onset >= start. On OK, *out_members is allocated (NULL when there are none) and
   the caller frees it. */
AdaptiveSegmentationResult segmentation_members_of(const NoteSpan *spans, size_t span_count, ExactRational start,
                                                    ExactRational end, SegmentMember **out_members,
                                                    size_t *out_count);

/*
 * Step 1, the grid cut: one segment per beat of every measure of `map` from its first measure until
 * the end of the last sounding note, the beats being the provider's. Empty segments are kept (they
 * stop a merge) and dropped when the result is assembled. On OK, *out owns its segments.
 */
AdaptiveSegmentationResult segmentation_cut(const NoteSpan *spans, size_t span_count, const MeterMap *map,
                                             BeatGridProvider provider, void *provider_context, SegmentList *out);

/*
 * Step 2, the bass split: splits a beat segment where the bass changes, at the ATTACK of the new
 * lowest note, unless the new bass lasts less than the minimum split duration. Edited in place; the
 * pieces of a split segment hold the notes sounding in each (a note held across the split is held in
 * the later piece), and only the first piece keeps starts_measure.
 *
 *   - A bass change is a change of pitch class: an octave leap of the same pitch class does not split.
 *   - It is made by an attack: the new lowest sounding note starts at that time. A bass that changes
 *     because the lowest note releases (and a higher note was already sounding) is not split.
 *   - The new bass must last min_split_numerator / min_split_denominator of the beat (the beat being
 *     the segment as the grid cut made it): from its attack to the next bass change, or the end of the
 *     beat. So every piece a split makes is at least that long.
 *   - A bass attack after silence in the beat counts as a change from no bass.
 */
AdaptiveSegmentationResult segmentation_split_bass(const NoteSpan *spans, const AdaptiveSegmentationConfig *config,
                                                    SegmentList *segments);

/*
 * Step 3, the merge: joins adjacent segments whose sounding pitch-class set AND bass pitch class are
 * both identical, repeatedly, so a run of unchanged beats becomes one segment. A merge may cross a
 * beat but not a barline (a segment with starts_measure set) unless config->merge_across_barlines.
 * An empty segment (silence) is never merged, and so keeps the segments either side of it apart.
 * The merged segment spans both, holds each sounding note once, and a note is attacked in it if it
 * was attacked in either part. `segments` is edited in place.
 */
AdaptiveSegmentationResult segmentation_merge(const NoteSpan *spans, const AdaptiveSegmentationConfig *config,
                                               SegmentList *segments);

/* The lowest sounding pitch of a segment, and the pitch-class set of its members as a 12-bit mask
   (bit pc set). A segment with no members has bass pitch -1 and an empty set. */
int segment_bass_pitch(const NoteSpan *spans, const Segment *segment);
unsigned segment_pitch_class_set(const NoteSpan *spans, const Segment *segment);

#endif
