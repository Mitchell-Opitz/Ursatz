#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "interpretation/interpretation.h"
#include "semantics/beat_strength.h"
#include "semantics/cadence_type.h"
#include "structure/cadence.h"
#include "time/time_span.h"

/*
 * Cadences from real music (the standard-harmony analysis adapter, step 5): found from harmony and meter alone, since
 * phrases do not exist as analysis yet (L3.6). The input is the prepared neutral segments (analysis_input.h): Roman
 * numeral degree, quality, inversion, beat strength, the soprano degree. The classification is the existing
 * rules' (harmony_cadence_classify, harmony_deceptive_of, harmony_plagal_progression_is, harmony_function_of); this
 * file chooses the candidates and supplies what those rules need.
 *
 * CADENCE POINTS. Chords are runs (analysis_runs.h). A candidate is a run of a diatonic chord, the ARRIVAL, directly
 * after another diatonic chord, where
 *
 *   the arrival starts on a beat at least as strong as config.min_arrival_strength (default MEDIUM: a downbeat or
 *   the middle beat of a measure of four, not a weak beat); and
 *   it lasts at least config.arrival_min_beats (default 1) beats, a beat being the segment's own `beat_length`
 *   or, with none, config.beat_length; or the soprano rests right after it (the next segment has no soprano) and
 *   config.soprano_rest_counts (default true).
 *
 * THE DOMINANT. For an authentic, deceptive or subverted cadence the dominant is the V (or V7) run just before the
 * arrival, in root position; a cadential 6/4 (analysis_six_four.h) directly before that V is part of the dominant
 * and recorded with it. An inverted V, or an arrival on an inverted tonic, is not a cadence here: it prolongs, as
 * L3.2 reads it.
 *
 * TYPES.
 *   PERFECT_AUTHENTIC      root-position V(7) to root-position I with the soprano on scale degree 1 at the arrival
 *   IMPERFECT_AUTHENTIC    the same with the soprano on any other degree (3 or 5 usually; the existing rule,
 *                          harmony_cadence_classify, calls every other degree imperfect)
 *   AUTHENTIC_UNSPECIFIED  the same with NO soprano at the arrival: PAC or IAC cannot be told, and the uncertainty
 *                          carries config.soprano_absent_penalty (default 0.3). This is the only case left unclassified
 *
 * The soprano degree is the one of the arrival's first segment (the input's `soprano_degree`, from the outer voices);
 * 0 means absent. viio to I is not an authentic cadence in standard-harmony's rule and is not found here.
 *
 * The result of each cadence is a Cadence (structure/cadence.h, the L5 shell: an id and event refs, here the segment
 * ids from the dominant's first segment to the arrival's last) and an Interpretation targeting the Cadence's id:
 * FrameworkReference standard-harmony, claim = a HarmonyCadenceClaim, the uncertainty below. The span from the
 * dominant's start to the arrival's end is carried as a TimeSpan on the result.
 *
 * UNCERTAINTY = 1 - (1 - u)(1 - penalty), u the largest input uncertainty of the chords involved (the dominant, a
 * cadential 6/4 in it, and the arrival), penalty config.soprano_absent_penalty when the type is
 * AUTHENTIC_UNSPECIFIED for want of a soprano, and 0 otherwise.
 */

#define HARMONY_CADENCE_CLAIM_TYPE "standard-harmony/cadence"

typedef struct {
    BeatStrength min_arrival_strength; /* default MEDIUM */
    double arrival_min_beats;          /* default 1 */
    bool soprano_rest_counts;          /* default true */
    double beat_length;                /* whole notes, when a segment has none: default 1/4 */
    double soprano_absent_penalty;     /* default 0.3 */
} HarmonyCadenceConfig;

HarmonyCadenceConfig harmony_cadence_config_default(void);

/* A candidate cadence point: the arrival run and the run before it, as segment index ranges (inclusive). */
typedef struct {
    size_t arrival_first;
    size_t arrival_last;
    size_t before_first;
    size_t before_last;
} HarmonyCadencePoint;

typedef enum {
    HARMONY_CADENCE_ANALYSIS_OK = 0,
    HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT,
    HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG,
    HARMONY_CADENCE_ANALYSIS_ERR_ALLOC
} HarmonyCadenceAnalysisStatus;

/* The candidates, in order of arrival. On OK, *out is owned by the caller (free) and *out_count counts them (NULL
   with none). config may be NULL for the defaults. BAD_CONFIG for a negative or non-finite number. */
HarmonyCadenceAnalysisStatus harmony_analysis_cadence_points(const HarmonySegment *segments, size_t count,
                                                             const HarmonyCadenceConfig *config,
                                                             HarmonyCadencePoint **out, size_t *out_count);

typedef struct {
    int type;               /* CadenceType */
    int soprano_degree;     /* at the arrival's first segment; 0 when absent */
    bool post_cadential;    /* a plagal cadence that directly follows a PAC */
    bool has_six_four;      /* the dominant includes a cadential 6/4 */
    char arrival_segment[96];   /* the arrival's first segment id */
    char dominant_segment[96];  /* the dominant's first segment id (the cadential 6/4's when it has one) */
    char key_ref[64];
} HarmonyCadenceClaim;

typedef struct {
    CadenceType type;
    size_t dominant_first;        /* the dominant's first segment (the cadential 6/4's, when it has one) */
    size_t dominant_last;
    bool has_six_four;
    size_t six_four_first;        /* meaningful only when has_six_four */
    size_t six_four_last;
    size_t arrival_first;
    size_t arrival_last;
    int soprano_degree;           /* at the arrival's first segment; 0 when absent */
    bool post_cadential;
    const char *key_ref;          /* borrowed from the arrival's input */
    double uncertainty;           /* [0, 1] */
    TimeSpan *span;               /* owned: the dominant's start to the arrival's end */
    Cadence *cadence;             /* owned */
    Interpretation *interpretation; /* owned; targets the Cadence's id */
} HarmonyCadenceResult;

typedef struct {
    HarmonyCadenceResult *items;  /* owned; in order of arrival; NULL when none */
    size_t count;
} HarmonyCadences;

/* Finds and classifies the cadences. A candidate matching no type gives none. On OK, *out is owned by the caller
   (harmony_cadences_destroy); otherwise untouched. config may be NULL for the defaults. */
HarmonyCadenceAnalysisStatus harmony_analysis_find_cadences(const HarmonySegment *segments, size_t count,
                                                            const HarmonyCadenceConfig *config,
                                                            HarmonyCadences *out);

void harmony_cadences_destroy(HarmonyCadences *cadences);

/* Reads the claim back out of a cadence Interpretation. False if it is not one. */
bool harmony_cadence_claim_read(const Interpretation *interpretation, HarmonyCadenceClaim *out);

#endif
