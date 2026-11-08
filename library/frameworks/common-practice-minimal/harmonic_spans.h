#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_HARMONIC_SPANS_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_HARMONIC_SPANS_H

/*
 * Owner of harmonic span grouping and of the bass each segment is read over (docs/analytic-job-owners.md).
 *
 * A harmonic span is the stretch of music one chord governs. Segmentation cuts at every change of sounding
 * notes, which is far finer than the rate the music changes harmony; this module reads the bass of each
 * segment (R-370) and groups segments into spans (R-369). It is pure: it reads segments and the metric level
 * the caller gives each one, and includes no other framework (the caller reads the levels with standard-meter).
 *
 * THE BASS (R-370). The bass of a segment is read at the segment's start, from the members sounding there
 * (attacked there or held into it):
 *   - the lowest sounding pitch;
 *   - unless that is 12 or more semitones above the most recent bass of the same measure, or nothing sounds:
 *     then the most recent bass persists (a bass that has stopped keeps governing while the upper parts move);
 *   - the first segment of a measure takes its lowest sounding pitch, or the previous measure's bass when
 *     nothing sounds.
 * The bass is -1 only when nothing has sounded yet.
 *
 * THE READING. A part (the cells on one side of a boundary, or a whole measure) is read as ONE segment: the union
 * of its cells' members, held as at the part's start, over its first cell's bass, with require_bass_chord_tone set.
 * It is read in no key: the best chord over all 24 keys (tonic 0 to 11 in major, then in minor; in each key the
 * first ranked candidate of an allowed quality; a later key replaces the best only when strictly higher). The
 * allowed qualities are the major, minor and diminished triads, the dominant seventh and the diminished seventh.
 * Two chords a third apart (I and vi) would read as one major or minor seventh at full score, so those sevenths are
 * left out of grouping only (the chain still identifies chords on spans with the full vocabulary); and a home key
 * would miss every chord outside it, and so every modulation. A part is matched when its best score reaches the
 * config's min_coverage.
 *
 * THE SPANS (R-369). One span per measure to start with, then split where the music changes chord. For a span,
 * the boundaries are tried from the strongest level present, HALF_BAR boundaries first and then BEAT ones, in
 * time order, and the span splits at the first boundary that passes every test (a part is the cells on one side,
 * read as ONE segment: the union of its cells' members, held as at the part's start, its first cell's bass as the
 * segment bass, require_bass_chord_tone set, over the reading above):
 *   (a) both parts match and their best chords have different roots;
 *   (b) each part is complete: three of its best chord's pitch classes have weight, or its root and third do and
 *       its bass pitch class is a chord tone;
 *   (c) each part's bass pitch class is a tone of its best chord;
 *   (d) (score_left * dur_left + score_right * dur_right) / dur_span >= score_whole + HARMONIC_SPAN_SPLIT_MARGIN.
 * Both parts are then split the same way, down to BEAT boundaries. When no HALF_BAR or BEAT boundary passes, a
 * boundary INSIDE a beat is tried last, only at the exact half of a beat and only when, besides (a) to (d), each
 * part's bass is its best chord's root. A span with no boundary passing stays whole. The result is deterministic:
 * doubles are summed in cell order.
 */

#include <stdbool.h>
#include <stddef.h>

#include "non_chord_tones.h"
#include "weighted_identification.h"

/* The metric level of a segment's START. */
typedef enum {
    HARMONIC_SPAN_LEVEL_BAR = 0, /* a measure start */
    HARMONIC_SPAN_LEVEL_HALF_BAR, /* the middle of a 4/4, 2/2, 6/8 or 12/8 measure, and beat 3 of 4/4 */
    HARMONIC_SPAN_LEVEL_BEAT,     /* another beat */
    HARMONIC_SPAN_LEVEL_INNER     /* inside a beat */
} HarmonicSpanLevel;

/* One segment of the piece, in time order. segment is a view (not owned): its members are every note sounding
   in it, attacked or held. start and end are whole notes (end > start); measure numbers the measure the segment
   lies in, equal for every segment of one measure. */
typedef struct {
    const CommonPracticeSegment *segment;
    double start;
    double end;
    int measure;
    HarmonicSpanLevel level;
} HarmonicSpanCell;

/*
 * The bass of each cell at its start by the rule above, in out_bass[0..count) (semitone number, C4 = 60, or -1).
 * A member sounds at the cell start when it is held or its onset is not after the start. False, out_bass
 * untouched, for a NULL cells or out_bass with a non-zero count, or a cell without a segment; count 0 is true.
 */
bool harmonic_span_basses(const HarmonicSpanCell *cells, size_t count, int *out_bass);

/* How much better the two parts of a split must score, duration-weighted, than the whole (test (d)). */
#define HARMONIC_SPAN_SPLIT_MARGIN 0.10

typedef enum {
    HARMONIC_SPAN_OK = 0,
    HARMONIC_SPAN_ERR_NULL_ARGUMENT,
    HARMONIC_SPAN_ERR_BAD_INPUT, /* a cell without a segment, an empty or non-finite time range, an invalid level, a bass
                                    below -1 or an invalid config */
    HARMONIC_SPAN_ERR_ALLOC
} HarmonicSpanStatus;

/* The spans, in time order: span i covers cells first[i] to last[i] inclusive, and its bass is the bass of its first
   cell. Together they cover every cell once. Owned (harmonic_spans_destroy). */
typedef struct {
    size_t count;
    size_t *first;
    size_t *last;
    int *bass;
} HarmonicSpans;

/*
 * Groups the cells into spans by the rule above. bass is harmonic_span_basses of the same cells. config may be NULL
 * for the defaults; its require_bass_chord_tone is set for every reading regardless, and its min_coverage is the
 * score a reading needs to match. count 0 gives no spans. On OK, *out owns its arrays; otherwise *out is untouched.
 */
HarmonicSpanStatus harmonic_spans_group(const HarmonicSpanCell *cells, size_t count, const int *bass,
                                        const CommonPracticeWeightedConfig *config, HarmonicSpans *out);

void harmonic_spans_destroy(HarmonicSpans *spans);

#endif
