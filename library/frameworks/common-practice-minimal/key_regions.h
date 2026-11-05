#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_REGIONS_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_KEY_REGIONS_H

/*
 * Owner of key identification (key_regions_estimate) (docs/analytic-job-owners.md).
 * Owner of the home key (docs/analytic-job-owners.md).
 */

#include <stdbool.h>
#include <stddef.h>

#include "core/rational.h"
#include "interpretation/key.h"
#include "key_vocabulary.h"
#include "semantics/analysis_key.h"
#include "semantics/key_change_extent.h"
#include "time/time_span.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "weighted_chord_identification.h"
#include "weighted_identification.h"

/*
 * Local key regions for common-practice-minimal-v1: a key per stretch of the piece instead of one key for
 * the whole piece. This is a NEW function beside key_identification.h, which is
 * unchanged and still gives the whole-piece answer to their current callers.
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
 *
 * FUNCTIONAL EVIDENCE. A pitch-class profile cannot tell a key from its dominant or subdominant when a passage
 * is built on V, IV or ii. A segment may therefore carry its identified chord as plain data (`chord`, below; the
 * caller reads it from weighted chord identification, this framework does not run it here), and a window then
 * adds evidence_weight times the key's evidence per chord to the key's score. For a chord sequence in a window and
 * the key on tonic t:
 *
 *   - a dominant to tonic motion (a chord rooted on t+7 with a major third, then a tonic chord: rooted on t with
 *     the key's third, the major third t+4 for a major key and the minor third t+3 for a minor key) counts 1;
 *   - a leading tone resolving up (a chord holding t+11 followed by such a tonic chord, without the first already
 *     counted as a dominant to tonic motion) counts 0.5;
 *   - in a minor key, a raised seventh (a chord holding t+11 and not the natural seventh t+10) after a chord holding
 *     the minor third t+3 or the minor sixth t+8 counts 0.5. A raised seventh alone does not tell a minor key from
 *     its parallel major, whose dominant has it too, so it needs the minor colour before it.
 *
 * A motion is credited to the measure of the chord it arrives on. The window's evidence is the sum over its
 * measures divided by the number of chords it holds, so it does not grow with the window's length or chord rate
 * (at most 1 for a chord). A window with no chords, or a caller that passes none, adds nothing and the score is the correlation.
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

/* A segment's identified chord as plain data, for functional evidence. Zero-initialised means no chord. */
typedef struct {
    bool present;
    int root_pitch_class;                      /* 0..11 */
    unsigned pitch_classes;                    /* the chord's tones as a 12-bit mask (bit pc set) */
} CommonPracticeKeyChord;

typedef struct {
    const MusicalTime *start;                  /* the segment's window, in whole notes; borrowed */
    const MusicalTime *end;
    CommonPracticeWeights weights;
    const CommonPracticeSegment *source;       /* the segment's members, or NULL */
    CommonPracticeKeyChord chord;              /* optional: the segment's chord; .present false when not given */
} CommonPracticeKeySegment;

/* The chord of a weighted identification result as evidence: present only when the result matched a chord. */
CommonPracticeKeyChord common_practice_key_chord_of(const CommonPracticeWeightedResult *result);

typedef struct {
    size_t window_measures;                    /* default 4 */
    CommonPracticeKeyProfiles profiles;        /* default Krumhansl-Kessler */
    double change_penalty;                     /* default 0.2: the cost of one key change, in correlation units */
    size_t tonicization_max_measures;          /* default 4 */
    const char *context_id_prefix;             /* default "key-region": region n is "<prefix>-<n>" (from 1) */
    size_t pivot_search_measures;              /* default 2: how far back a pivot chord is looked for */
    double evidence_weight;                    /* default 0.8: the weight of functional chord evidence in a window's score */
    const MusicalTime *const *phrase_boundaries; /* optional: phrase boundary times, whole notes, any order; borrowed */
    size_t phrase_boundary_count;              /* default 0 */
    size_t snap_reach_measures;                /* default 1: how far an edge may move to a phrase boundary */
    double hold_tolerance;                     /* default 0.05: the weight share outside the old key that allows a switch */
} CommonPracticeKeyRegionsConfig;

/* window_measures 4, Krumhansl-Kessler profiles, change_penalty 0.2, tonicization_max_measures 4, prefix
   "key-region", pivot_search_measures 2, evidence_weight 0.8, no phrase boundaries, snap_reach_measures 1, hold_tolerance 0.05. */
CommonPracticeKeyRegionsConfig common_practice_key_regions_config_default(void);

typedef enum {
    COMMON_PRACTICE_KEY_REGIONS_OK = 0,
    COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT,
    COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS,
    COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG,
    COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT,
    COMMON_PRACTICE_KEY_REGIONS_ERR_METER_MAP,
    COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT,
    COMMON_PRACTICE_KEY_REGIONS_ERR_KEY,
    COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC
} CommonPracticeKeyRegionsStatus;

typedef struct {
    int first_measure;                         /* the measures the window covers (MeterMap measure numbers) */
    int last_measure;
    double correlation[COMMON_PRACTICE_KEY_COUNT];
    double evidence[COMMON_PRACTICE_KEY_COUNT]; /* evidence_weight times the window's functional evidence; 0 without chords */
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
 * one window over what it has. BAD_CONFIG for a zero window, a non-finite profile value, or a negative or
 * non-finite evidence_weight; BAD_SEGMENT for a
 * segment with a missing or negative time, an end not after its start, a segment out of order, a negative
 * or non-finite weight, or a chord with a root outside 0..11. config may be NULL for the defaults. On OK, *out owns its windows
 * (common_practice_key_windows_destroy); otherwise *out is untouched.
 */
CommonPracticeKeyRegionsStatus key_window_scores(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeterMap *map, const CommonPracticeKeyRegionsConfig *config,
                                                  CommonPracticeKeyWindows *out);

void common_practice_key_windows_destroy(CommonPracticeKeyWindows *windows);

/*
 * REGION PATH (step 2). key_regions_estimate chooses one key per window by dynamic programming, then reads
 * the windows as measures and merges equal neighbours into regions.
 *
 *   cost of a path = the sum over windows of -(correlation(window, key) + evidence(window, key))
 *                    + change_penalty for every window whose key differs from the one before it.
 *
 * The path with the least cost wins. It is deterministic: an exactly tied cost (within 1e-12) keeps the
 * previous window's key rather than changing, and among tied keys to change from the lowest key index wins.
 * change_penalty (default 0.2, about the gap between a clearly right key and its dominant or relative
 * neighbour in one window) is what keeps a single odd window from becoming a region; 0 follows the best key of
 * every window, and a penalty above the correlation range (2) allows no change. It is a stated v1 value, not a
 * learned one.
 *
 * Window i labels the measure at its CENTRE (its first measure + window length / 2, rounded down); the measures
 * before the first centre take the first window's key and those after the last centre the last window's. A
 * region is a run of measures with one key, so its edges are barlines, and the regions are ordered, gap-free
 * and cover every measure from the first segment's to the last's.
 *
 * Each region carries a Key (common_practice_build_key: major or natural-minor label such as "G major", a
 * Context id "<prefix>-<n>", the common-practice-minimal-v1 FrameworkReference), a TimeSpan (the first measure's
 * start to the last measure's end, left-closed), its first and last measure, its key index, and an
 * UNCERTAINTY in [0, 1]:
 *
 *   uncertainty = 1 - (r(chosen key) - r(best other key)), clamped to [0, 1]
 *
 * where r is the correlation of the region's pooled pitch-class weights with a key's profile. It is 0 when the
 * chosen key beats every other by a full unit, and 1 when another key fits as well or better (the penalty can
 * keep a worse key). The Key's own Interpretation keeps the vocabulary's confidence of 1/1; the region's
 * uncertainty is this field.
 *
 * CHANGES. Between each pair of neighbouring regions there is one change, labelled with the existing
 * KeyChangeExtent values by the length and return of the region it goes INTO (region n):
 *
 *   region n is shorter than tonicization_max_measures AND region n+1 goes back to region n-1's key
 *       -> TONICIZATION
 *   region n goes back to region n-1's key at the next change, and is not shorter
 *       -> WEAK_MODULATION
 *   otherwise -> MODULATION
 *
 * A change that is the return out of such a region (the key goes back to the one before the excursion) is
 * flagged is_return and carries the extent of the excursion it closes. EXTENDED_TONICIZATION is not used here (it
 * needs a count of chords). The extent is read from region lengths ALONE: no cadence is looked for here, so
 * cadence_confirmed is false as returned, and MODULATION / WEAK_MODULATION mean "a lasting new
 * key region", not "a cadence established it". The caller confirms the extent with the cadences it finds
 * (standard-harmony's harmony_key_change_confirm, as the Framework v1 chain does) and sets cadence_confirmed.
 */

/*
 * HOME KEY HELD (R-017). Profile correlation reads a passage built on IV, V or ii of the current key as the
 * dominant, the subdominant or the relative of it. After the path is chosen, a region (not the first of the piece
 * or of a key signature span) is therefore folded back into the key before it when ALL of these hold:
 *
 *   - its key is closely related to that key: the dominant or subdominant in the same mode, or the relative
 *     major or minor;
 *   - it has chords (segments with `chord.present`), every one rooted on the old key's IV, V or ii and made only
 *     of the old key's tones (its natural collection, plus the raised seventh in minor);
 *   - no pitch class outside the old key's collection carries more than hold_tolerance of the region's weight.
 *
 * A switch therefore needs a non-diatonic pitch (the new key's leading tone, or a raised sixth or seventh) or a
 * chord outside IV, V and ii; a dominant of the new key always has one. A region with no chord data is never
 * folded back, so a caller that passes no chords gets the path as it was.
 */

/*
 * PHRASE BOUNDARIES. A window labels the measure at its centre, so the path's edges land up to two measures from
 * where the music turns. When config.phrase_boundaries is given (times in whole notes, each snapped to the nearest
 * barline), every edge the path drew is moved to the nearest boundary measure within snap_reach_measures of it (default
 * 1; 0 keeps every edge), ties to the earlier. An edge never moves onto or past a neighbouring edge, and an edge at a key signature event (an
 * authoritative boundary, below) never moves. With no boundaries the edges stay where the path put them. The
 * boundaries come from phrase detection, which needs cadences, which need keys, so the caller runs this twice: once
 * without boundaries, then again with the boundaries found from that first answer.
 */

/*
 * PIVOT CHORDS. At each change whose extent is a modulation (WEAK_MODULATION or MODULATION: a tonicization
 * has no pivot, and neither does a change with no extent worth a pivot), the pivot is the LAST segment, looking
 * back from the boundary (the start of the new region's first measure), whose chord is diatonic in both the old
 * key and the new. The search takes the segments that start at or before the boundary and no more than
 * pivot_search_measures measures before the new region's first measure. A segment's chord is its best chord from
 * weighted identification (common_practice_identify_weighted, default weights) read in the OLD key and its own mode (a
 * minor old key is read in the minor vocabulary, with its raised sixth and seventh); a segment
 * with no members or no matching chord is skipped. "Diatonic in a key" is the natural collection: a major
 * key's seven notes, and for a minor key those of its relative major, so a chord that needs the raised sixth or
 * seventh of a minor key (the major V, the diminished vii) is not diatonic there in this version. If no segment
 * qualifies the change is DIRECT: has_pivot is false. No Roman numeral is assigned: the pivot is a segment of
 * the caller's list, whose chord the caller can read in either key.
 */

/*
 * KEY SIGNATURE EVENTS. A key signature change in the source (the file's key signature meta-events) is an
 * AUTHORITATIVE boundary: the piece's key regions are found separately between the events, so a region can
 * never straddle one. The caller passes the events as plain structs (this framework includes no io/ and no
 * other framework): the position of the event and its sharps_flats count (-7..7, negative flats), in time
 * order, as midi_key_signature_scan_all returns them. The mode hint is ignored (the byte is unreliable).
 *
 * Each event, snapped to the nearest barline, starts a span of the piece. Consecutive events with the same
 * sharps_flats are one signature, so the repeat is not a boundary. The FIRST region of a span that starts at an
 * event is restricted to that signature's major key and its relative minor (candidates: the 2 of the 24 keys);
 * the region ends at the first change the path makes, and after it every key is a candidate again, because
 * a piece modulates inside one signature all the time (an accidental, not a new signature). An event at or
 * before the start of the piece restricts only the first region and forces no boundary. An event after the last
 * measure is ignored. Such a region has source KEY_SIGNATURE (when it begins at the span's start; if the
 * restricted key equals the one before it the two merge into one region and the source stays PROFILE). Many
 * exporters write a default C-major event at tick 0 whatever the key: the first region is then restricted
 * to C major or A minor, which is a limit of trusting the signature.
 */

typedef struct {
    const MusicalTime *position;   /* whole notes; borrowed */
    int sharps_flats;              /* -7..7 */
} CommonPracticeKeySignatureEvent;

/* The key signature event an AnalysisKeyEvent places: position is the event's time (borrowed) and sharps_flats the signature of its key,
   the major tonic's place on the line of fifths and the minor tonic's three places less. False, *out untouched, for a NULL argument, a
   NULL time or a signature beyond -7..7. */
bool common_practice_key_signature_event_from_analysis(const AnalysisKeyEvent *event, CommonPracticeKeySignatureEvent *out);

typedef enum {
    COMMON_PRACTICE_REGION_SOURCE_PROFILE = 0,
    COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE
} CommonPracticeRegionSource;

typedef struct {
    Key *key;                      /* owned */
    int key_index;                 /* 0..23 */
    TimeSpan *span;                /* owned */
    int first_measure;             /* MeterMap measure numbers (the first measure is 1), inclusive */
    int last_measure;
    double uncertainty;            /* [0, 1] */
    CommonPracticeRegionSource source;
} CommonPracticeKeyRegion;

/* The AnalysisKey of a region's key_index: the tonic spelled as common_practice_key_index_tonic_name spells it, the mode major or minor,
   id NULL. False, *out untouched, for a NULL argument or an index outside 0..23. */
bool common_practice_key_region_analysis_key(const CommonPracticeKeyRegion *region, AnalysisKey *out);

typedef struct {
    size_t from_region;            /* indices into regions[]; to_region == from_region + 1 */
    size_t to_region;
    KeyChangeExtent extent;
    bool is_return;                /* goes back to the key before an excursion that this extent describes */
    bool cadence_confirmed;        /* false as returned; the caller sets it after confirming with cadences */
    bool has_pivot;                /* a pivot chord was found (see below); false for a direct change */
    size_t pivot_segment;          /* its index in the caller's segments; meaningful only when has_pivot */
} CommonPracticeKeyChange;

typedef struct {
    CommonPracticeKeyRegion *regions; /* owned; ordered, gap-free */
    size_t region_count;
    CommonPracticeKeyChange *changes; /* owned; region_count - 1 of them */
    size_t change_count;
} CommonPracticeKeyRegions;

/*
 * Estimates the regions. events may be NULL with event_count 0 (no signature information: boundaries come only
 * from the path). BAD_EVENT for a missing or negative position, sharps_flats outside -7..7, or events out of
 * time order. BAD_CONFIG additionally for a negative or non-finite change_penalty or hold_tolerance, a zero
 * tonicization_max_measures, an empty prefix, or a phrase boundary count with no list. BAD_EVENT also covers a
 * missing or negative phrase boundary time. config may be NULL for the defaults. On OK, *out owns its
 * regions and changes (common_practice_key_regions_destroy); otherwise *out is untouched.
 */
CommonPracticeKeyRegionsStatus key_regions_estimate(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                     const MeterMap *map,
                                                     const CommonPracticeKeySignatureEvent *events, size_t event_count,
                                                     const CommonPracticeKeyRegionsConfig *config,
                                                     CommonPracticeKeyRegions *out);

/* Destroys every region's Key and TimeSpan and the arrays. NULL is a no-op. */
void common_practice_key_regions_destroy(CommonPracticeKeyRegions *regions);

/*
 * The home key of a piece: the key (tonic and mode) with the longest total duration over all its regions, the
 * earliest such key on a tie. *home is the index of the first region of that key and *duration, when not NULL,
 * its total. The sums are checked Rational sums. False, outputs untouched, for NULL regions or home, a count of
 * 0, a region without a span or a key index outside 0..23, or an overflow.
 */
bool common_practice_home_key_region(const CommonPracticeKeyRegion *regions, size_t count, size_t *home,
                                     Rational *duration);

#endif
