#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_ANALYSIS_GALANT_ANALYSIS_INPUT_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_ANALYSIS_GALANT_ANALYSIS_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "galant_degree.h"
#include "semantics/beat_strength.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

/*
 * The galant-schemas analysis adapter (L3.10): schema prototypes matched against the outer-voice degrees of a piece's segments. The
 * adapter includes no framework but its one dependency, standard-counterpoint, and no kernel analysis (the architecture test enforces
 * both): the input is the neutral per-segment struct below, which the caller fills (tests/integration/ builds it from the outer voices
 * L2.6, the local key regions L2.8, the Roman numerals L3.1 and the meter). scale-harmonization's sequence adapter, whose declared
 * dependency this framework is, reads the same struct.
 *
 * THE INPUT is built with the functions below: init, add segments in time order, run an analysis, destroy. Segment ids are copied;
 * TimeSpans and MetricalPositions are BORROWED and must outlive the input and every result computed from it.
 *
 *   id, span        the segment's ref and its TimeSpan (the segments are in order, a segment starting no earlier than the one before)
 *   position        the MetricalPosition of its start, optional (carried for the caller; the adapters read `strength`)
 *   strength        the BeatStrength at its start: STRONG (a downbeat), MEDIUM (a secondary beat), WEAK (any other)
 *   beat            the length of a beat there, in whole notes (the unit gaps are counted in)
 *   key_id, key     the local key's ref, and its tonic as a degree and alteration of the piece's HOME key (1 0 is the home key itself;
 *                   in C major a passage in A minor has the tonic 6 0 and one in D minor, 2 0). Degrees below are in this local key
 *   bass, soprano   the scale degree of the outer voices, in the local key, with its alteration against the major scale on the tonic
 *                   (the lowered third of a minor key is 3 -1), or absent
 *   numeral         the Roman numeral: `root` is the degree of the chord's sounding root in the local key (an applied V/V in C has the
 *                   root 2), `quality` and `seventh` the caller's codes, `inversion` 0 to 3, `applied_to` the degree of the target
 *                   of an applied chord or 0; absent when `has_numeral` is false
 *   uncertainty     the segment's uncertainty, 0 to 1 (the analysis' own: the key region's, the numeral's)
 */

#define GALANT_ANALYSIS_ID_MAX 64

typedef enum {
    GALANT_ANALYSIS_OK = 0,
    GALANT_ANALYSIS_ERR_NULL_ARGUMENT,
    GALANT_ANALYSIS_ERR_INVALID,
    GALANT_ANALYSIS_ERR_ALLOC
} GalantAnalysisStatus;

typedef struct {
    char id[GALANT_ANALYSIS_ID_MAX];
    const TimeSpan *span;
    const MetricalPosition *position; /* may be NULL */
    BeatStrength strength;
    long beat_numerator;
    unsigned long beat_denominator;
    char key_id[GALANT_ANALYSIS_ID_MAX];
    GalantPitchDegree key;
    bool has_bass;
    GalantPitchDegree bass;
    bool has_soprano;
    GalantPitchDegree soprano;
    bool has_numeral;
    GalantPitchDegree root;
    int quality;
    bool seventh;
    int inversion;
    int applied_to;
    double uncertainty;
} GalantSegment;

typedef struct {
    GalantSegment *items;
    size_t count;
    size_t capacity;
} GalantSegmentList;

void galant_segments_init(GalantSegmentList *list);

/* Adds a copy of the segment. ERR_INVALID for an empty or too long id, a NULL span or one that is not ahead (end after start), a segment
   that starts before the one before it, a strength that is not a BeatStrength, a beat of no length, a degree outside 1 to 7 or an alteration
   outside -2 to 2 (in the key, the bass, the soprano or the root when present), an inversion outside 0 to 3 or an uncertainty outside [0, 1]. */
GalantAnalysisStatus galant_segments_add(GalantSegmentList *list, const GalantSegment *segment);

void galant_segments_destroy(GalantSegmentList *list);

/*
 * Degree arithmetic over the letters, the same convention as galant_degree.h: a degree is a letter distance from the tonic (1 to 7) and its
 * alteration is the semitones against the major scale. Adding the tonic of a key (as a degree of the home key) to a degree of that key gives
 * the degree in the home key; subtracting gives the other way. The octave is dropped.
 */
GalantPitchDegree galant_degree_plus(GalantPitchDegree a, GalantPitchDegree b);
GalantPitchDegree galant_degree_minus(GalantPitchDegree a, GalantPitchDegree b);
bool galant_degree_equal(GalantPitchDegree a, GalantPitchDegree b);

/* A degree of the segment's local key read in the key whose tonic is `frame` (a degree of the home key). */
GalantPitchDegree galant_degree_in_frame(GalantPitchDegree local, GalantPitchDegree segment_key, GalantPitchDegree frame);

/* A degree of the segment's local key read in the HOME key. */
GalantPitchDegree galant_degree_in_home(GalantPitchDegree local, GalantPitchDegree segment_key);

#endif
