#ifndef URSATZ_COMPOSER_MOTIVE_DEVELOP_H
#define URSATZ_COMPOSER_MOTIVE_DEVELOP_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/motive.h>

/*
 * Motivic development: related motives made from a source motive.
 *
 * motive_develop applies one transformation to a motive and returns the result as a new Motive; the source is
 * never changed. The operators below use it as Grow operators for the fill loop, writing only the Realized melody
 * lane span of a motive node. They do not place anything against harmony and do not build phrases.
 *
 * The real transformations call Ursatz's transforms (transposition, inversion, retrograde, intervallic manipulation)
 * through NoteEvents. Ursatz has no diatonic transposition or inversion, so the tonal ones are made here from the
 * key's scale (the same steps motive analysis counts).
 *
 * A development that is the same as its source is refused (MOTIVE_DEV_ERR_UNCHANGED), except an exact restatement,
 * which is meant to be.
 */

typedef enum {
    MOTIVE_DEV_EXACT,         /* the source again */
    MOTIVE_DEV_TRANSPOSE,     /* every pitch moved by amount: semitones, or steps of the key's scale when tonal */
    MOTIVE_DEV_INVERT,        /* mirrored about a pitch: real (in semitones) or tonal (in scale steps) */
    MOTIVE_DEV_RETROGRADE,    /* the notes in reverse order, each keeping its duration */
    MOTIVE_DEV_INTERVAL_CHANGE, /* an interval, or every one, widened or narrowed by amount semitones */
    MOTIVE_DEV_FRAGMENT,      /* a run of the notes: the first, the last or the middle ones */
    MOTIVE_DEV_LIQUIDATE,     /* the notes with the least important ones dropped */
    MOTIVE_DEV_AUGMENT,       /* every onset and duration longer by a ratio: 2:1 by default */
    MOTIVE_DEV_DIMINISH       /* every onset and duration shorter by a ratio: 1:2 by default */
} MotiveDevKind;

/* The shortest result, in notes, that a fragment or a liquidation may have when the spec does not say. */
#define MOTIVE_DEV_MIN_LENGTH 2

typedef struct {
    MotiveDevKind kind;
    /* TRANSPOSE, INVERT: in steps of the key's scale. Needs a key. */
    bool tonal;
    /* TRANSPOSE: semitones, or steps when tonal. INTERVAL_CHANGE: semitones to widen, or to narrow when negative. */
    int amount;
    /* INVERT: the pitch mirrored about, a MIDI note number; the source's first note when has_axis is false. */
    bool has_axis;
    int axis;
    /* INTERVAL_CHANGE: the 1-based interval changed, or every one when 0; and whether a direction may not change. */
    int interval;
    bool keep_contour;
    /*
     * FRAGMENT: the position (not PLAN_FRAGMENT_ANY) and how many notes. The fragment starts at 0, so it opens on
     * its first note.
     */
    PlanFragmentPosition position;
    size_t length;
    /*
     * LIQUIDATE: how many notes to drop. The weakest go first: notes off the beat, then notes on a beat that is not
     * the downbeat, then downbeats, the later of two equals first. With keep_rhythm the notes left keep their onsets
     * and a dropped note is a rest (or a leading rest, if it was first); without it they follow one another from 0.
     */
    size_t drop;
    bool keep_rhythm;
    /* FRAGMENT, LIQUIDATE: the fewest notes the result may have; 0 means MOTIVE_DEV_MIN_LENGTH. */
    size_t min_length;
    /*
     * AUGMENT, DIMINISH: the factor times and durations are multiplied by, ratio_num:ratio_den. Augmentation needs
     * it above 1 and diminution below 1; 0:0 means 2:1 and 1:2. The pitches and their order are not touched. The
     * result must be expressible in the meter: every duration a plain or dotted value (w h q e s t with up to two
     * dots, as a note list writes them), every onset a multiple of a thirty-second, and, when the source is a whole
     * number of beats long, the result too. Otherwise MOTIVE_DEV_ERR_NOT_EXPRESSIBLE.
     */
    unsigned long ratio_num;
    unsigned long ratio_den;
} MotiveDevSpec;

typedef enum {
    MOTIVE_DEV_OK = 0,
    MOTIVE_DEV_ERR_NULL,
    MOTIVE_DEV_ERR_ALLOC,
    MOTIVE_DEV_ERR_NO_KEY,         /* a tonal transformation needs a key */
    MOTIVE_DEV_ERR_RANGE,          /* a pitch would fall outside MIDI 0 to 127 */
    MOTIVE_DEV_ERR_UNCHANGED,      /* the result is the source */
    MOTIVE_DEV_ERR_NOT_APPLICABLE, /* nothing to do: one note has no intervals, the contour would change */
    MOTIVE_DEV_ERR_TOO_SHORT,      /* a fragment or liquidation with fewer notes than the minimum */
    MOTIVE_DEV_ERR_NOT_EXPRESSIBLE, /* the rhythm cannot be written in the meter, see MotiveDevSpec */
    MOTIVE_DEV_ERR_INVALID,        /* the spec asks for something that is not a parameter */
    MOTIVE_DEV_ERR_NO_FEATURE,     /* a feature operation on a motive with no labelled feature */
    MOTIVE_DEV_ERR_NO_ACCOMPANIMENT /* an accompaniment operation on a motive whose role is melodic */
} MotiveDevResult;

/* Applies spec to source. *out is written only on MOTIVE_DEV_OK; its meter and key are the source's. */
MotiveDevResult motive_develop(const Motive *source, const MotiveDevSpec *spec, Motive *out);

/* Short text naming the transformation and its parameters: "transpose +3 steps (tonal)". */
void motive_dev_label(const MotiveDevSpec *spec, char *out, size_t size);

/* "ok", "no key" and so on. */
const char *motive_dev_result_name(MotiveDevResult result);

/* --- recognizability ---------------------------------------------------- */

/*
 * How well a development can be recognized as its source: a score from 0 to 1 made from four parts, each from 0 to 1.
 *
 *   contour    the longest run of neighboring intervals whose directions (up, down, same) the two share, in the same
 *              places or not, over the count of intervals in the longer of the two
 *   intervals  how many intervals the two share by interval class (the size in semitones, an octave and more folded
 *              to within one octave, and a size and its complement the same: 1 and 11, 2 and 10), counted as the
 *              smaller of the two counts of each class, over the count of intervals in the longer of the two
 *   rhythm     the longest run of neighboring ratios the two share, over the count of ratios in the longer of the two;
 *              a ratio is one inter-onset interval (the last note's duration, for the last) over the next, so a
 *              rhythm made twice as slow or fast still scores 1 and a rest counts for its length
 *   length     the notes of the shorter over the notes of the longer
 *
 * When neither motive has intervals (or ratios), that part is 1; when only one has, 0.
 *
 *   score = (w.contour * contour + w.intervals * intervals + w.rhythm * rhythm + w.length * length)
 *           / (w.contour + w.intervals + w.rhythm + w.length)
 *
 * The weights are whole numbers; the default is 3, 2, 2, 1, so that the contour counts most, the rhythm and the
 * interval classes the same, and the length least. Every part and the score are exact fractions, as the fill loop's
 * scores are.
 */
typedef struct {
    unsigned long numerator;
    unsigned long denominator; /* at least 1; the fraction is in lowest terms */
} MotiveScore;

typedef struct {
    unsigned contour;
    unsigned intervals;
    unsigned rhythm;
    unsigned length;
} MotiveScoreWeights;

/* 3, 2, 2, 1. */
MotiveScoreWeights motive_score_default_weights(void);

typedef struct {
    MotiveScore contour;
    MotiveScore intervals;
    MotiveScore rhythm;
    MotiveScore length;
    MotiveScore total;
} MotiveRecognizability;

/* weights may be NULL for the default. False for a NULL motive or weights that are all 0; *out is untouched then. */
bool motive_recognizability(const Motive *source, const Motive *development, const MotiveScoreWeights *weights,
                            MotiveRecognizability *out);

struct GrowPoolForm;

/*
 * What the operators know that the plan does not say. The key and meter of the source come from the plan's key and
 * meter lanes over its span when it has them, and from here otherwise. All-zero, set by
 * motive_develop_context_default, is the default configuration.
 */
typedef struct {
    PlanMeter meter;
    bool has_key;
    PlanKey key;
    /* How many candidates an operator proposes when the constraint leaves the parameters open. */
    size_t candidates;
    /* The fewest notes a fragment or liquidation may have; 0 means MOTIVE_DEV_MIN_LENGTH. */
    size_t min_length;
    /* For motive_develop_checker: the weights of the score (all 0 means the default) and a floor under it. */
    MotiveScoreWeights weights;
    MotiveScore floor; /* a development scoring below it is rejected; a denominator of 0 means no floor */
    /*
     * The basic motive of a grow run with its own harmony, features and role (a plan keeps only the notes). The motive-form
     * operator reads them for the motive node m0 when the notes are the plan's; NULL for none. Must outlive the registry.
     */
    const Motive *basic;
    /*
     * For the motive-form operator (motive_connection_limited): the remoteness over which a form's connection is
     * halved. A denominator of 0 means 3/4.
     */
    MotiveScore remoteness_limit;
    /*
     * For the motive-form operator: the saved motive-forms it prefers (grow_form.h, R-166); NULL for none. Must outlive
     * the registry.
     */
    const struct GrowPoolForm *pool;
    size_t pool_count;
} MotiveDevelopContext;

void motive_develop_context_default(MotiveDevelopContext *context);

#define MOTIVE_RESTATE_OPERATOR_NAME "motive-restate"
#define MOTIVE_FRAGMENT_OPERATOR_NAME "motive-fragment"
#define MOTIVE_RHYTHM_OPERATOR_NAME "motive-rhythm"

/*
 * Registers the motivic development operators with the fill loop. Each is a Grow operator for a Constrained motive
 * node that carries a motive_ref constraint (constraint.h) naming a source motive node. The source must have a Given
 * or Realized melody lane span (voice 0); the operator writes a Realized melody lane span on the target, the length
 * of the result, and only when the result fits inside the target's span.
 *
 *   motive-restate   restated, transposed, inverted, retrograde, interval_changed
 *   motive-fragment  fragmented, liquidated
 *   motive-rhythm    augmented, diminished
 *
 * With the constraint's parameters left open (see PlanMotiveRef) an operator proposes context->candidates, their
 * parameters drawn from the run's seeded stream; with them set it proposes that one. context may be NULL for the
 * defaults, and must outlive the registry otherwise. FILL_OK, or the registry's error.
 */
FillResult motive_develop_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context);

/*
 * The checker for the candidates of these operators; pass it as FillConfig.checker, with the MotiveDevelopContext as
 * FillConfig.checker_context. The score of a candidate is its recognizability (motive_recognizability) against the
 * source motive, with the context's weights: it ranks the candidates in the fill loop and is what the
 * GenerationRecord and the suggestions carry. A candidate below the context's floor is a hard violation. A
 * candidate of any other operator is accepted at 0/1, as the default checker does.
 */
bool motive_develop_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                            const FillCandidate *candidate, void *context, FillCheck *out);

/* The motive and the spec of a candidate one of these operators proposed. NULL for any other candidate. */
const Motive *motive_develop_candidate_motive(const FillCandidate *candidate);
const MotiveDevSpec *motive_develop_candidate_spec(const FillCandidate *candidate);

/* --- invention ---------------------------------------------------------- */

#define MOTIVE_INVENT_OPERATOR_NAME "motive-invent"

/*
 * Registers motive-invent, a Fill operator for a Constrained melody lane span (voice 0) of a motive node that carries
 * an invent_motive constraint and is one bar of its meter long, starting on a barline. It proposes
 * context->candidates motives drawn from the run's seeded stream, each of 3 to 6 notes filling the bar: the rhythm
 * from the meter's cells (a beat of a simple meter, a dotted quarter of 6/8, each one note or two, the last note
 * allowed to hold the last cells), the first note on scale degree 1, 3 or 5, then steps and leaps of at most a sixth
 * in the key's scale, every pitch from C4 to C6; only motives that keep the checker's hard melodic rules are proposed.
 * The key and meter are the plan's lanes over the span, else the
 * context's. Applying a candidate writes the notes to the span and marks the motive node Realized. context may be
 * NULL for the defaults and must outlive the registry otherwise.
 */
FillResult motive_invent_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context);

/*
 * The checker for motive-invent's candidates. Hard: standard-counterpoint's hard melodic rules on the motive as one
 * line (melodic_rules.h: no forbidden leap or outline; the clausula vera is not asked). Soft: the motive's
 * recognizability, the mean of motive_recognizability against the three developments the form templates make of it
 * (a tonal inversion, a diminution, the first interval widened by a semitone), a development that cannot be made
 * counting 0, with the context's weights. A candidate of any other operator is accepted at 0/1.
 */
bool motive_invent_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out);

/* The motive a motive-invent candidate proposes. NULL for any other candidate. */
const Motive *motive_invent_candidate_motive(const FillCandidate *candidate);

#endif
