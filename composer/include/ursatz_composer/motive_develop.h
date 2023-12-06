#ifndef URSATZ_COMPOSER_MOTIVE_DEVELOP_H
#define URSATZ_COMPOSER_MOTIVE_DEVELOP_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
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
    MOTIVE_DEV_ERR_INVALID         /* the spec asks for something that is not a parameter */
} MotiveDevResult;

/* Applies spec to source. *out is written only on MOTIVE_DEV_OK; its meter and key are the source's. */
MotiveDevResult motive_develop(const Motive *source, const MotiveDevSpec *spec, Motive *out);

/* Short text naming the transformation and its parameters: "transpose +3 steps (tonal)". */
void motive_dev_label(const MotiveDevSpec *spec, char *out, size_t size);

/* "ok", "no key" and so on. */
const char *motive_dev_result_name(MotiveDevResult result);

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

/* The motive and the spec of a candidate one of these operators proposed. NULL for any other candidate. */
const Motive *motive_develop_candidate_motive(const FillCandidate *candidate);
const MotiveDevSpec *motive_develop_candidate_spec(const FillCandidate *candidate);

#endif
