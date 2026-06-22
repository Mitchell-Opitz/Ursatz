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
 * WEAK-BEAT ARRIVAL. With config.allow_weak_arrival (default true), a run below min_arrival_strength is still an
 * arrival when it is a root-position tonic directly after a root-position V that starts on a stronger beat than the
 * tonic (a stronger BeatStrength, or at equal strength a V on the beat against a tonic off it: a weak beat and the weak
 * half beat after it), and the tonic lasts arrival_min_beats, or the soprano rests right after it, or it ends the piece.
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
 *   HALF                   an arrival on a root-position V (or V7: the framework's soft rule allows it) at a cadence
 *                          point, preceded by a tonic or a predominant; a cadential 6/4 directly before the V is part
 *                          of the dominant and the chord before the 6/4 is the one that counts. A 6/4 (a triad in second
 *                          inversion) is never the chord before the V: a neighbor 6/4 between two V is part of the V
 *   DECEPTIVE              root-position V(7) to the chord harmony_deceptive_of names: vi (VI in minor) or IV6
 *   PLAGAL                 IV to I (harmony_plagal_progression_is), only when it directly follows a PERFECT authentic
 *                          cadence: it is then marked `post_cadential`. Elsewhere IV to I is a prolongation, which
 *                          is what standard-harmony says it is, and is not found here
 *   SUBVERTED              a V(7) that resolves to I6 at a cadence point (C7: no cadential approach is needed, the
 *                          competing prolongation reading below carries that doubt), or a cadential dominant (V(7)
 *                          after a predominant, or with a cadential 6/4) that resolves to another chord that is not a
 *                          tonic, the V again, a deceptive substitute or a 6/4. The dominant's own beat is not tested;
 *                          the arrival must be at a cadence point
 *
 * COMPETING READINGS (analysis_cadence_readings.h). A V to I6 is an evaded cadence or a prolongation of the dominant
 * (C7, R-090): the result is the SUBVERTED cadence, `share` its thousandths and `alternative` the PROLONGATION reading
 * with the rest. `share` below 500 means the prolongation is the likelier reading. SUBVERTED already means no goal
 * cadence to every consumer, so phrase ends do not change. Every other cadence has `share` 1000 and no alternative
 *
 * A half cadence is the END of a phrase on V: a V that the next cadence resolves (an authentic, deceptive or subverted
 * cadence whose dominant is this V) is that cadence's dominant, and no half cadence is reported for it.
 *
 * AT A FORMAL BOUNDARY (harmony_analysis_add_boundary_cadences, below): a V that ends the run before a section start
 * is a half cadence of the earlier section, whatever follows it. The tonic after it starts the new section (R-038).
 *
 * A candidate that matches none of these is not a cadence. The soprano degree is the one of the arrival's first segment (the input's `soprano_degree`, from the outer voices);
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
    bool allow_weak_arrival;           /* default true */

    /* Competing readings (analysis_cadence_readings.h): weights in thousandths, all default as listed. */
    int reading_strong_arrival;        /* 150 */
    int reading_cadential_approach;    /* 150 */
    int reading_followup_cadence;      /* 150 */
    int reading_tonic_continues;       /* 100 */
    int reading_long_dominant;         /* 150 */
    int reading_break_after_dominant;  /* 150 */
    int reading_pac_shape;             /* 150 */
    int reading_short_dominant;        /* 100 */
    double reading_followup_beats;     /* default 8 */
    double reading_long_dominant_beats; /* default 4 */
} HarmonyCadenceConfig;

typedef enum {
    HARMONY_CADENCE_READING_CADENCE = 0,
    HARMONY_CADENCE_READING_PROLONGATION
} HarmonyCadenceReading;

HarmonyCadenceConfig harmony_cadence_config_default(void);

/* The chord tests and the arrival rule the cadence adapter and the reading evidence share. A dominant is V or V7 in
   root position with a major third; a tonic is a root-position I without a seventh; a cadential six-four is the
   prepared segment marked `cadential`. harmony_cadence_is_arrival is the cadence-point rule above for the run
   [first, last] after a run starting at before_first. */
bool harmony_cadence_is_dominant_root(const HarmonySegment *segment);
bool harmony_cadence_is_tonic_root(const HarmonySegment *segment);
bool harmony_cadence_is_cadential_six_four(const HarmonySegment *segment);
bool harmony_cadence_is_arrival(const HarmonySegment *segments, size_t count, size_t before_first, size_t first,
                                size_t last, const HarmonyCadenceConfig *config);

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
    int arrival_beat_strength; /* BeatStrength at the arrival's first segment */
    int reading;            /* HarmonyCadenceReading: what this Interpretation claims */
    int rank;               /* 1 for the primary reading, 2 for the alternative */
    int share;              /* thousandths of this reading; the two readings of a cadence sum to 1000 */
    char arrival_segment[96];   /* the arrival's first segment id */
    char dominant_segment[96];  /* the dominant's first segment id (the cadential 6/4's when it has one) */
    char key_ref[64];
} HarmonyCadenceClaim;

/* The competing reading of a cadence that has two (C7, C9). `type` is meaningful only when `reading` is CADENCE. */
typedef struct {
    bool present;
    HarmonyCadenceReading reading;
    CadenceType type;
    size_t arrival_first;
    size_t arrival_last;
    int soprano_degree;           /* at the arrival's first segment; 0 when absent */
    int share;                    /* thousandths */
    Interpretation *interpretation; /* owned; targets the same Cadence id; id = the primary's id + "-alt" */
} HarmonyCadenceAlternative;

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
    BeatStrength arrival_beat_strength; /* at the arrival's first segment */
    const char *key_ref;          /* borrowed from the arrival's input */
    double uncertainty;           /* [0, 1] */
    TimeSpan *span;               /* owned: the dominant's start to the arrival's end */
    Cadence *cadence;             /* owned */
    Interpretation *interpretation; /* owned; targets the Cadence's id */
    int share;                    /* thousandths of the primary reading; 1000 when there is no alternative */
    HarmonyCadenceAlternative alternative;
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

/* Cadences at formal boundaries (a post-pass). section_starts are the start times, in whole notes, of the sections
   the caller found (from the Framework v1 chain's section detection; passed as data, never recomputed here). A
   root-position V that ends a run directly before a section start has two readings (C9, R-091, R-094): a HALF cadence
   ending the earlier section, and the authentic cadence into the tonic that starts the next (PAC, IAC or unspecified,
   by harmony_cadence_classify; offered only when the section start is a root-position tonic). One result carries both:
   the reading with the higher share is the primary (a tie keeps HALF, R-038's reading), the other is its
   `alternative`; the shares come from harmony_cadence_boundary_half_share. A dominant that is no half cadence by the
   usual rule (no tonic or predominant before it) is left alone. The authentic cadence found in `cadences` with that
   dominant and an arrival starting at the section start, and a half cadence already recorded at the dominant, are
   replaced by the one two-reading result. `cadences` is the existing result, updated in place and kept in order of the
   primary's arrival. A start that is no segment's start is ignored. config may be NULL for the defaults. BAD_CONFIG for
   a bad config or a non-finite start. */
HarmonyCadenceAnalysisStatus harmony_analysis_add_boundary_cadences(const HarmonySegment *segments, size_t count,
                                                                    const HarmonyCadenceConfig *config,
                                                                    const double *section_starts, size_t section_count,
                                                                    HarmonyCadences *cadences);

/* Reads the claim back out of a cadence Interpretation. False if it is not one. */
bool harmony_cadence_claim_read(const Interpretation *interpretation, HarmonyCadenceClaim *out);

#endif
