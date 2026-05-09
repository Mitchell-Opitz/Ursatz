#ifndef MUSIC_SEMANTICS_SONORITY_SEGMENTATION_H
#define MUSIC_SEMANTICS_SONORITY_SEGMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "semantics/sonority.h"
#include "time/meter_map.h"
#include "time/musical_time.h"

/*
 * Adaptive sonority segmentation (semantics): a meter-aware way to cut a stream of NoteEvents into
 * Sonority segments. It sits next to the fixed-grid SonorityBuilder (semantics/sonority_builder.h),
 * which is unchanged: that builder groups notes by attack on a fixed quarter-note grid, ignores
 * meter, and does not carry a held note into later beats. Callers move over later.
 *
 * Segment content is EVERY note sounding in the segment, those attacked in it and those held over
 * from earlier. Each member records which: AdaptiveSonority::held is parallel to the Sonority's event
 * ids. Weighting by duration or metric position is not done here. The bass is the lowest sounding
 * pitch of a segment.
 *
 * The algorithm runs in order (each step is its own internal function, with its own tests):
 *   1. Cut at every beat boundary of the current meter (this file's grid cut).
 *   2. Split a beat where the bass changes, at the attack of the new lowest note, unless the new bass
 *      lasts less than the minimum split duration.
 *   3. Merge adjacent segments whose sounding pitch-class set AND bass pitch class are identical.
 *      Merging may cross beats, but not a barline unless configured.
 *   4. Merge a segment into the previous one when its new attacks are only non-bass notes and every
 *      pitch class of the previous chord is still held (a melody or passing note over unchanged
 *      harmony). On by default (merge_unchanged_harmony); the same barline rule applies.
 *
 * The beat grid is INJECTED. The kernel cannot include a framework, and what a beat is (a dotted
 * quarter in 6/8, a quarter in 3/4) is frameworks/standard-meter's reading of a TimeSignature, which
 * provides the standard BeatGridProvider. This is the same injection pattern the cadence ranking
 * uses: the caller passes a function (and a context pointer for it).
 *
 * Pitch comes from each NoteEvent's PitchSpelling, as a semitone number (12 * (octave + 1) + the
 * letter's semitone + the accidental, so C4 is 60): the pitch class is that number mod 12, and the
 * bass is the lowest number. A NoteEvent without a spelling is AD_SEGMENTATION_ERR_NO_PITCH, not
 * skipped. The MIDI importer always sets one.
 *
 * Time arithmetic is exact and checked (AD_SEGMENTATION_ERR_OVERFLOW, never a wrapped value).
 */

/*
 * A beat-grid provider: the beat starts of one measure of `map`, as absolute times in ascending
 * order. The first is the start of the measure (as meter_map_measure_span gives it) and each beat
 * lasts until the next start; the last lasts until the end of the measure. The provider must handle
 * short measures (a meter change mid-measure) and a pickup measure (whose first beat may start
 * partway through a beat). On success *out_beat_starts is a newly allocated array of *out_count new
 * MusicalTimes, which the builder destroys. Returns false on failure.
 */
typedef bool (*BeatGridProvider)(const MeterMap *map, int measure, void *context, MusicalTime ***out_beat_starts,
                                  size_t *out_count);

/*
 * min_split_numerator / min_split_denominator: the minimum split duration as a fraction of a beat
 * (default 1/2). merge_across_barlines: whether the merge steps may join segments across a barline
 * (default false). merge_unchanged_harmony: whether step 4 runs (default true).
 */
typedef struct {
    unsigned long min_split_numerator;
    unsigned long min_split_denominator;
    bool merge_across_barlines;
    bool merge_unchanged_harmony;
} AdaptiveSegmentationConfig;

/* 1/2 of a beat, not across barlines, merging over unchanged harmony. */
AdaptiveSegmentationConfig adaptive_segmentation_config_default(void);

typedef enum {
    AD_SEGMENTATION_OK = 0,
    AD_SEGMENTATION_ERR_NULL_ARGUMENT,
    AD_SEGMENTATION_ERR_NULL_NOTE,
    AD_SEGMENTATION_ERR_NO_PITCH,
    AD_SEGMENTATION_ERR_BAD_CONFIG,
    AD_SEGMENTATION_ERR_BEAT_GRID,
    AD_SEGMENTATION_ERR_METER_MAP,
    AD_SEGMENTATION_ERR_OVERFLOW,
    AD_SEGMENTATION_ERR_ALLOC
} AdaptiveSegmentationResult;

/*
 * A segment: the Sonority (its time_window is the segment's TimeSpan [start, end), left-closed; its
 * event_ids are fresh EntityID clones of every sounding note, in input order; no voice restriction;
 * domain SOUNDING) and held[i], true when event id i was sounding before the segment began and false
 * when it was attacked in it.
 */
typedef struct {
    Sonority *sonority; /* owned */
    bool *held;         /* owned; sonority_event_id_count entries */
} AdaptiveSonority;

typedef struct {
    AdaptiveSonority *items; /* owned; NULL when count is 0 */
    size_t count;
} AdaptiveSonorities;

/*
 * Segments notes[0..note_count) on the beat grid of `map`, as provided by `provider`, and returns the
 * non-empty segments in time order. A stretch with nothing sounding produces no segment. config may
 * be NULL for the defaults. On OK, *out owns its segments (adaptive_sonorities_destroy); otherwise
 * *out is untouched.
 *
 * notes must be non-NULL with note_count > 0 and no NULL entry.
 */
AdaptiveSegmentationResult sonority_builder_build_adaptive(NoteEvent *const *notes, size_t note_count,
                                                             const MeterMap *map, BeatGridProvider provider,
                                                             void *provider_context,
                                                             const AdaptiveSegmentationConfig *config,
                                                             AdaptiveSonorities *out);

/* Destroys every Sonority and held array, and the list. NULL is a no-op. */
void adaptive_sonorities_destroy(AdaptiveSonorities *sonorities);

#endif
