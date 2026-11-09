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
 * first ranked candidate of an allowed quality; a later key replaces the best only when strictly higher, or when it ties
 * the best and is rooted on the bass while the best is not: a diminished seventh over its bass is rooted on the bass, R-387). The
 * allowed qualities are the major, minor and diminished triads, the dominant seventh and the diminished seventh.
 * Two chords a third apart (I and vi) would read as one major or minor seventh at full score, so those sevenths are
 * left out of grouping only (the chain still identifies chords on spans with the full vocabulary); and a home key
 * would miss every chord outside it, and so every modulation. A part is matched when its best score reaches the
 * config's min_coverage.
 *
 * THE SPAN BASS (R-373). A part is read over its first cell's bass. When that reading is not valid (not matched, not
 * complete, or the bass is no chord tone) and the first later cell of the part with a different bass lies inside the
 * part's first beat (only INNER cells up to it) and is 1 or 2 semitones from the first, the part is read over that bass
 * instead, and that reading is the part's when it is valid. An accented passing tone, appoggiatura or suspension in
 * the bass that resolves by step then does not break the part. The Alberti bass keeps R-370's (the bass follows the
 * figure). A span's bass is the bass its reading used.
 *
 * TERMS (R-381, R-383, R-387). A step is two pitch classes 1 or 2 apart the shorter way round; a semitone is 1. A new bass
 * is the bass of a cell at level BAR, HALF_BAR or BEAT (not the part's first cell) that differs from the bass of the cell
 * before it, with a member on that pitch attacked in the cell and another member above it sounding at the cell's start: a
 * note with nothing above it is a melody the bass rule took for the bass while the left hand rests, not a new bass. The bass after
 * cell c is the bass of the first later cell (in any measure) whose bass differs from bass[c]; none at the piece's end. An
 * altered chord tone of a chord's pitch-class mask is the bass of a cell whose pitch class is not in the mask and lies one
 * semitone from a tone c of it, while c sounds at the cell's start in a member above the bass note (held, or attacked there),
 * and the bass after it is a semitone from it: a chromatic bass against the chord tone it alters is no new chord.
 *
 * THE SPANS (R-372). One span per measure to start with (a span never crosses a barline), then the measure is cut
 * where the music changes chord, by scoring whole partitions instead of single splits. The candidate boundaries of a
 * measure are its cells at level HALF_BAR or BEAT after its first cell, except a cell that keeps the bass of the cell
 * before it with that bass note held over (a held bass note is no new bass, so the measure is not cut inside it, R-379 (4)),
 * and except, when the whole measure's reading is matched, a cell whose bass is an altered chord tone of that reading
 * (R-381, see TERMS), and every non-empty subset of them is a
 * partition (2^n, n = 3 in 4/4). A partition is valid when every part is matched, complete (three of its best chord's
 * pitch classes have weight, or its root and third do and its bass pitch class is a chord tone, or its root and fifth
 * do and the bass is the root: a bare fifth over the root is a complete two-voice reading, R-379) and has its bass as a
 * chord tone, and adjacent parts read different roots. Its value is the parts' scores weighted by duration. The
 * measure takes the valid partition with the highest value less HARMONIC_SPAN_PART_COST per part after the first,
 * among those whose value is at least the whole measure's score plus HARMONIC_SPAN_SPLIT_MARGIN; with none it stays
 * whole. Ties (within 1e-9) go to fewer parts, then to the partition whose first differing boundary is earlier. Each
 * part is read once. A resulting span inside one beat (its first cell at level BEAT or stronger, every other INNER)
 * splits at the cell starting at the exact half of its time range when both halves are valid (matched, complete, bass
 * a chord tone) and read different roots, and they beat the span's score by the margin; once, and no deeper. A greedy split, one boundary at a time against a margin over the whole, could not cut a bar that
 * changes chord every beat: a part scores at most 1.05, so a whole at 0.95 or more never splits, and each step had to
 * clear the margin alone. The result is deterministic: doubles are summed in cell order.
 */

#include <stdbool.h>
#include <stddef.h>

#include "non_chord_tones.h"
#include "weighted_identification.h"

/* How much better a partition of a measure must score, duration-weighted and before the part cost, than the whole
   measure read as one chord. */
#define HARMONIC_SPAN_SPLIT_MARGIN 0.03

/* What each part after the first costs a partition: of two partitions that score alike, the one with fewer parts wins. */
#define HARMONIC_SPAN_PART_COST 0.01

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


typedef enum {
    HARMONIC_SPAN_OK = 0,
    HARMONIC_SPAN_ERR_NULL_ARGUMENT,
    HARMONIC_SPAN_ERR_BAD_INPUT, /* a cell without a segment, an empty or non-finite time range, an invalid level, a bass
                                    below -1 or an invalid config */
    HARMONIC_SPAN_ERR_ALLOC
} HarmonicSpanStatus;

/* The spans, in time order: span i covers cells first[i] to last[i] inclusive, and its bass is the bass its reading
   used (usually the bass of its first cell; see THE SPAN BASS above). Together they cover every cell once. Owned (harmonic_spans_destroy). */
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

/*
 * RESOLVED ACCENTS (R-382). Sets CommonPracticeMember.resolves_in_segment on the members of one span or part, which sound in
 * the window [start, end) (whole notes) over bass_pitch (semitone number; -1: none). Every flag is cleared first. A member m is a
 * candidate when it has a voice_id, sounds at `start` (held, or its onset within 1e-9 of start), lies above bass_pitch, and its
 * resolution r (the member of the same voice with the smallest onset after m's, by more than 1e-9) is attacked inside the window
 * (not held, onset before `end` by more than 1e-9) one or two semitones from m, while another member on bass_pitch sounds at `start`
 * and still sounds at r's onset: an appoggiatura, suspension or accented passing tone over a held bass. The members are then read
 * as THE READING above (every key, the allowed qualities, the chord holds bass_pitch) with every candidate flagged, and a candidate
 * keeps its flag only when r's pitch class is a tone of that reading and m's is not; the others are cleared. One pass, no
 * iteration. bass_pitch -1 or no candidate: every flag is false. config may be NULL for the defaults. BAD_INPUT for a non-finite
 * or empty window or a bass_pitch below -1; NULL_ARGUMENT for NULL members with a count. Deterministic.
 */
HarmonicSpanStatus harmonic_span_mark_resolved_accents(CommonPracticeMember *members, size_t count, double start, double end,
                                                       int bass_pitch, const CommonPracticeWeightedConfig *config);

#endif
