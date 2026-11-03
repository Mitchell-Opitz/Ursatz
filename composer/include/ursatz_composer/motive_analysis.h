#ifndef URSATZ_COMPOSER_MOTIVE_ANALYSIS_H
#define URSATZ_COMPOSER_MOTIVE_ANALYSIS_H

#include <stdbool.h>
#include <stdio.h>

#include <semantics/contour.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_tones.h>

/*
 * Motive analysis: what a motive is made of, read once so that later steps can
 * compare a development against it. The same motive always gives the same
 * analysis.
 *
 * Intervals. One per pair of neighboring notes: the signed semitones, and with
 * a key the signed diatonic steps, counted in the key's scale (a step up from
 * 3 to 4 is +1, a leap up an octave is +7). A pitch outside the scale counts
 * as the scale degree below it, raised by a half step. The Analyzer has no
 * motive or interval analysis to call, so the intervals are worked out here;
 * the contour direction uses Ursatz's ContourDirection.
 *
 * Contour. The direction of each interval (Ursatz's ContourDirection), and the
 * shape of the whole: level, ascending or descending when the motive only goes
 * one way (or stays), an arch when it rises and then falls, a valley when it
 * falls and then rises, and a wave for anything else. A single note has no
 * shape.
 *
 * Rhythm. For each note: its onset, duration, the bar it starts in (from 0),
 * its position in the bar in beats of the meter's beat unit (0 is the downbeat;
 * 1/2 is half a beat after it), whether it falls on a beat, whether its
 * duration is a dotted value (3 or 7 over a power of two, such as 3/8 or 7/16,
 * which a tied pair of the same length also is), and the rest before it.
 *
 * Range. The lowest and highest MIDI pitch and the distance between them.
 *
 * Scale degrees. With a key: the degree (1 to 7) of each note, and an
 * alteration of 1 when it is a raised degree. The first and last notes are the
 * start and the end.
 *
 * Implied harmony. The triads of the key (and, in minor, the one on the raised
 * seventh degree), and V7, that the pitches fit, ranked best first. A chord is
 * scored by the share of the motive that is its chord tones, counting each note
 * for its duration and twice that on a downbeat. A chord none of the notes
 * belongs to is left out. Ties keep the order I, V, IV, vi, ii, iii, vii, V7.
 * Roman numerals and functions come from the standard-harmony framework (see
 * harmony_phrase.h); without a key nothing is implied and harmony_known is
 * false.
 *
 * Base harmony. The chords the notes are heard against (motive_tones.h): the motive's own harmony, else one proposed from
 * the notes. Known only for a full motive with a key.
 *
 * Tones. The harmonic role of every note against the base harmony (the library's motive_tone_roles): a chord tone, a
 * non-chord tone of one of ten types, or unclassified; where a non-chord tone resolves; and the notes that do not
 * resolve. Known with the base harmony.
 */

#define MOTIVE_ANALYSIS_MAX_CANDIDATES 9

typedef struct {
    int semitones;
    bool has_steps;
    int steps;
} MotiveInterval;

typedef enum {
    MOTIVE_SHAPE_SINGLE,
    MOTIVE_SHAPE_LEVEL,
    MOTIVE_SHAPE_ASCENDING,
    MOTIVE_SHAPE_DESCENDING,
    MOTIVE_SHAPE_ARCH,
    MOTIVE_SHAPE_VALLEY,
    MOTIVE_SHAPE_WAVE
} MotiveShape;

typedef struct {
    Rational onset;
    Rational duration;
    int bar;
    Rational beat;
    bool on_beat;
    bool dotted;
    Rational rest_before;
} MotiveRhythmNote;

typedef struct {
    int degree;     /* 1 to 7 */
    int alteration; /* 0, or 1 for a raised degree */
} MotiveDegree;

typedef struct {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    HarmonicFunction function;
    bool has_function;
    /* The share of the motive that is chord tones, 0 to 1, to four places. */
    double fit;
    size_t chord_tones;     /* notes that are chord tones */
    size_t non_chord_tones; /* notes that are not */
} MotiveChordCandidate;

typedef struct {
    size_t note_count;
    /* What the motive holds. A rhythm has only the rhythm below; a contour only the contour (its pitches are made up
       to sound it, so no intervals, range, degrees or harmony); a full motive has all of it. */
    MotiveForm form;
    bool intervals_known;
    bool contour_known;
    bool rhythm_known;
    bool range_known;
    Rational length;
    /* How many bars the motive is: its length over a bar. */
    Rational bars;

    MotiveInterval intervals[MOTIVE_MAX_NOTES];
    size_t interval_count; /* note_count - 1 */

    ContourDirection directions[MOTIVE_MAX_NOTES];
    MotiveShape shape;

    /* bar counts from the first barline: the bars before it (a pickup's) are -1, and the first full bar is 0. beat is the
       position in the bar, in beats from 0. */
    MotiveRhythmNote rhythm[MOTIVE_MAX_NOTES];

    int lowest;
    int highest;
    int range_semitones;

    bool key_known;
    AnalysisKey key;
    MotiveKeySource key_source;

    bool degrees_known; /* the same as key_known */
    MotiveDegree degrees[MOTIVE_MAX_NOTES];

    bool harmony_known; /* the same as key_known */
    MotiveChordCandidate candidates[MOTIVE_ANALYSIS_MAX_CANDIDATES];
    size_t candidate_count;

    bool base_harmony_known;
    MotiveBaseHarmony base_harmony;

    bool tones_known;
    MotiveToneLabel tones[MOTIVE_MAX_NOTES];
} MotiveAnalysis;

/* Analyzes the motive into *out. False for a motive that is not valid (see motive.h), leaving *out untouched. */
bool motive_analyze(const Motive *motive, MotiveAnalysis *out);

/*
 * motive_analyze with the caller's own tone labels (0-based note indices, see analysis/motive_tone_roles.h). False, *out
 * untouched, as motive_analyze, and also when there are overrides and the motive has no base harmony to label against
 * or an override is not valid.
 */
bool motive_analyze_with_tones(const Motive *motive, const MotiveToneOverride *overrides, size_t override_count,
                               MotiveAnalysis *out);

/* "arch", "ascending" and so on. */
const char *motive_shape_name(MotiveShape shape);

/* A duration the way a note list writes it ("q", "e.", "h..") when it is one, otherwise "3/16". */
void motive_duration_name(Rational duration, char *out, size_t size);
/* A pitch as a note list writes it, a black key with a sharp as the spelling is not known: "D4", "A#3". */
void motive_pitch_name(int midi, char *out, size_t size);

/* A human-readable summary: the notes, the intervals, the contour, the rhythm, the range, the degrees and the harmony. */
void motive_analysis_print(const Motive *motive, const MotiveAnalysis *analysis, FILE *out);

/*
 * The motive and its analysis as JSON, in the plan format's vocabulary (docs/plan-format.md, "Motive documents"),
 * 2-space indent, trailing newline. NULL on failure; caller frees.
 */
char *motive_analysis_to_json(const Motive *motive, const MotiveAnalysis *analysis);

/*
 * motive_analysis_to_json with a top-level "rendered" object: {"key": "G minor" (null without a key), "notes":
 * [{"midi_pitch", "offset": {num, den}, "duration": {num, den}}]}, the notes of rendered (the motive moved to the key
 * it is heard in, motive_in_key) with offsets from the first note's onset, in whole notes. NULL when rendered is NULL.
 */
char *motive_analysis_to_json_rendered(const Motive *motive, const MotiveAnalysis *analysis, const Motive *rendered);

#endif
