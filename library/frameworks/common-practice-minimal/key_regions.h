#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_REGIONS_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_REGIONS_H

#include <stdbool.h>
#include <stddef.h>

#include "key_vocabulary.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "weighted_chord_identification.h"
#include "weighted_identification.h"

/*
 * Local key regions for common-practice-minimal-v1: a key per stretch of the piece instead of one key for
 * the whole piece. This is a NEW function beside key_identification.h and analysis/key_estimation.h, which are
 * unchanged and still give the whole-piece answer to their current callers.
 *
 * This file, step 1: windowed key scoring. The piece is cut into overlapping windows of window_measures
 * measures (default 4, a hop of one measure; real phrases replace the fixed window when phrase detection
 * exists), and each window's pitch-class weights are correlated against the 24 keys (12 major, 12 minor).
 *
 * INPUT. Each segment is a plain struct, as the rest of this framework takes its input: it includes no other
 * framework and no NoteEvent. The caller cuts the piece into adaptive segments (semantics/sonority_
 * segmentation.h), weights each with common_practice_weight_segment (the L2.5 weighting: duration times the
 * beat-strength factor, the bass boosted), and passes the segment's time window with those weights. A segment
 * belongs to the measure of its START (a segment that crosses a barline is counted in the measure it begins
 * in; the default segmentation never crosses one). `source` is the segment's members, or NULL; it is only read
 * to find pivot chords. Segments must be in time order.
 *
 * KEYS. A key is indexed 0..23: 0..11 the major keys on tonic pitch class 0..11 (C ... B), 12..23 the minor
 * keys on tonic 0..11. Minor is the profile's minor, which gives the raised sixth and seventh weight that a
 * natural-minor collection test cannot.
 *
 * PROFILES. The default is the Krumhansl-Kessler probe-tone profiles (Krumhansl & Kessler, "Tracing the
 * dynamic changes in perceived tonal organization in a spatial representation of musical keys", Psychological
 * Review 89, 1982, 334-368), each indexed by the pitch class's distance above the tonic. Both are
 * configurable. They are a stated method, not a learned one; they describe listeners' ratings of Western
 * tonal music and fit chromatic music less well.
 *
 * SCORE. For a window, the Pearson correlation between its summed pitch-class weights and the key's profile
 * rotated to the key's tonic, in [-1, 1]; a window with no weight, or a flat distribution, scores 0 for every
 * key.
 */

#define COMMON_PRACTICE_KEY_COUNT 24

typedef struct {
    double major[12]; /* indexed by semitones above the tonic */
    double minor[12];
} CommonPracticeKeyProfiles;

/* Krumhansl-Kessler: major 6.35 2.23 3.48 2.33 4.38 4.09 2.52 5.19 2.39 3.66 2.29 2.88, minor 6.33 2.68 3.52
   5.38 2.60 3.53 2.54 4.75 3.98 2.69 3.34 3.17. */
CommonPracticeKeyProfiles common_practice_krumhansl_kessler_profiles(void);

/* Tonic pitch class (0..11) and mode of key index 0..23; -1 and MAJOR for an invalid index. */
int common_practice_key_index_tonic(int key_index);
CommonPracticeKeyMode common_practice_key_index_mode(int key_index);
/* The index of a tonic and mode, or -1. */
int common_practice_key_index_of(int tonic_pitch_class, CommonPracticeKeyMode mode);
/* "C", "Db" ... the tonic name used for key labels (C Db D Eb E F F# G Ab A Bb B); NULL if invalid. */
const char *common_practice_key_index_tonic_name(int key_index);

typedef struct {
    const MusicalTime *start;                  /* the segment's window, in whole notes; borrowed */
    const MusicalTime *end;
    CommonPracticeWeights weights;
    const CommonPracticeSegment *source;       /* the segment's members, or NULL */
} CommonPracticeKeySegment;

typedef struct {
    size_t window_measures;                    /* default 4 */
    CommonPracticeKeyProfiles profiles;        /* default Krumhansl-Kessler */
} CommonPracticeKeyRegionsConfig;

/* window_measures 4, Krumhansl-Kessler profiles. */
CommonPracticeKeyRegionsConfig common_practice_key_regions_config_default(void);

typedef enum {
    COMMON_PRACTICE_KEY_REGIONS_OK = 0,
    COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS,
    COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG,
    COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT,
    COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP,
    COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC
} CommonPracticeKeyRegionsStatus;

typedef struct {
    int first_measure;                         /* the measures the window covers (MeterMap measure indices) */
    int last_measure;
    double correlation[COMMON_PRACTICE_KEY_COUNT];
} CommonPracticeKeyWindow;

typedef struct {
    CommonPracticeKeyWindow *items;            /* owned; windows in order */
    size_t count;
    int first_measure;                         /* the measures the segments span */
    int last_measure;
} CommonPracticeKeyWindows;

/*
 * Scores every window: from the first measure that holds a segment start to the last, window i covers
 * measures [first + i, first + i + window_measures - 1], and a piece with fewer measures than the window gets
 * one window over what it has. BAD_CONFIG for a zero window or a non-finite profile value; BAD_SEGMENT for a
 * segment with a missing or negative time, an end not after its start, a segment out of order, or a negative
 * or non-finite weight. config may be NULL for the defaults. On OK, *out owns its windows
 * (common_practice_key_windows_destroy); otherwise *out is untouched.
 */
CommonPracticeKeyRegionsStatus key_window_scores(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                  CommonPracticeKeyWindows *out);

void common_practice_key_windows_destroy(CommonPracticeKeyWindows *windows);

#endif
