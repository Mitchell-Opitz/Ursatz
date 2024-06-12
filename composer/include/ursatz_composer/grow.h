#ifndef URSATZ_COMPOSER_GROW_H
#define URSATZ_COMPOSER_GROW_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Growing a theme from a motive (R-047): the plan of a theme, built from one motive, and the fill stages that write
 * it. A request names the motive, the key, the meter, how many bars (8, with a two-bar basic idea, or 4, with a
 * one-bar one), how the theme begins and ends (any of the nine pairings of standard-form: a presentation, an
 * antecedent or a compound basic idea, with a continuation, a consequent or a cadential ending), and the cadence it
 * ends in.
 *
 * grow_build_plan makes the plan:
 *   - a Form over the theme and the source material, with a meter and a key lane span, both Given, over all of it;
 *   - a Section "theme" over the bars, Constrained, with an ARCHETYPE (sentence for a presentation and a continuation,
 *     period for an antecedent and a consequent, hybrid for the other seven), a THEME_PAIRING, the CADENCE and the
 *     KEY; a theme with an antecedent has a second CADENCE, the weak one the antecedent ends in;
 *   - a Section "material" after it, marked MATERIAL, so it is not part of the piece, holding the motive node "m0":
 *     the motive as a Given melody lane span. The motives made from it go beside it.
 *
 * The stages, in order (grow_run), each one fill_run with only its operators registered and its own checker:
 *   structure  grow-theme, which gives the Section its nodes, one motive node in the material for each idea after
 *              the basic idea, and the open lane spans: harmony, bass and melody;
 *   motives    the motive development operators write those motives from m0;
 *   harmony    harmony-prolongation for the presentation, harmony-phrase-model for the rest;
 *   bass       harmony-bass under the chords;
 *   melody     melody-fill, idea by idea, over harmony and bass;
 *   texture    accompaniment, over the melody and the bass, when the request asks for one; or voicing-satb, over the
 *              harmony and the bass, when it asks for a voicing: it writes the alto and the tenor under the melody, or
 *              all three upper voices of a chorale, the melody too (else the stage has nothing to do).
 * Stage k runs with the seed config.seed + k, so a stage's choices do not change when another stage's do.
 *
 * The theme of 8 bars, in bars (4 bars halves every length). The first half is one of:
 *   presentation       Subphrase 0-4 in the theme's one Phrase: Ideas basic idea (0-2) and repetition (2-4)
 *   antecedent         its own Phrase 0-4: Ideas basic idea (0-2) and contrasting idea (2-4), ending in a weak cadence
 *   compound basic idea  Subphrase 0-4 in the one Phrase: Ideas basic idea (0-2) and contrasting idea (2-4)
 * and the second half one of:
 *   continuation       Subphrase 4-8: Ideas fragment 1 (4-5), fragment 2 (5-6) and cadential (6-8)
 *   consequent         Subphrase 4-8, or its own Phrase after an antecedent: Ideas consequent basic idea (4-6) and
 *                      consequent contrasting idea (6-8)
 *   cadential          Subphrase 4-8: Ideas cadential 1 (4-6) and cadential 2 (6-8)
 * A theme with an antecedent has two Phrases, the second holding the second half; any other has one Phrase of 8 bars.
 */

#define GROW_THEME_OPERATOR_NAME "grow-theme"

typedef struct {
    const Motive *motive;
    PlanKey key;
    PlanMeter meter;
    size_t bars;              /* 4 or 8 */
    PlanThemePairing pairing; /* any of the nine beginning and ending pairings */
    PlanCadenceType cadence;  /* PAC, IAC or HC: the cadence the theme ends in */
    /*
     * The weak cadence an antecedent beginning ends in: HC or IAC, and weaker than `cadence`. Zero (PAC) means the
     * default, HC. Read only for an antecedent beginning.
     */
    PlanCadenceType first_cadence;
    /*
     * An accompaniment under the melody and bass, in the inner voices (accompaniment.h). Read only with
     * has_accompaniment; a texture of -1 asks for none in particular, and one that is not homophonic is refused.
     */
    PlanAccompaniment accompaniment;
    bool has_accompaniment;
    /*
     * A four-part voicing of the harmony and bass (voicing.h), in the alto and the tenor. Read only with has_voicing,
     * and refused together with an accompaniment. With write_soprano the voicing writes the melody too, in one note a
     * chord (a chorale texture), and the melody stage has nothing to do; the form is then heard from that soprano.
     */
    PlanVoicing voicing;
    bool has_voicing;
} GrowRequest;

typedef enum {
    GROW_OK = 0,
    GROW_ERR_NULL,
    GROW_ERR_ALLOC,
    GROW_ERR_MOTIVE_TOO_LONG, /* the motive does not fit in one bar of the meter */
    GROW_ERR_BAD_REQUEST,     /* a meter the harmony operators do not support, bars not 4 or 8, a pairing not built
                                 yet, a cadence other than PAC, IAC or HC, a motive of one note, an accompaniment
                                 that is not valid or is Alberti in a compound meter, an accompaniment and a
                                 voicing together */
    GROW_ERR_STUCK            /* a stage left a target open (GrowOutcome.stuck_stage) */
} GrowResult;

const char *grow_result_name(GrowResult result);

/* Builds the plan of the request into *out, which the caller destroys with plan_destroy. */
GrowResult grow_build_plan(const GrowRequest *request, PiecePlan **out);

/* --- the structure operator -------------------------------------------- */

typedef struct {
    size_t candidates; /* the structures grow-theme proposes; 0 means 4 */
} GrowContext;

#define GROW_DEFAULT_CANDIDATES 4

void grow_context_default(GrowContext *context);

/*
 * Registers grow-theme: a Grow operator for a Constrained section with an ARCHETYPE and a THEME_PAIRING constraint,
 * whose plan holds the motive m0. context may be NULL for the defaults and must outlive the registry otherwise.
 */
FillResult grow_register(FillOperatorRegistry *registry, const GrowContext *context);

/*
 * The checker for grow-theme's candidates, from standard-form's own rule functions on the planned structure.
 * Hard: the subphrase roles in order, the phrase sentential, the pairing a sentence, the basic idea's length valid,
 * every fragment shorter than the basic idea. Soft, the score being the share that hold: the sentence archetypal,
 * the presentation of the usual length, the continuation balancing it, the repetition reading as one (its contour
 * the basic idea's). A candidate of any other operator goes to harmony_phrase_checker.
 */
bool grow_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                  const FillCandidate *candidate, void *context, FillCheck *out);

/* --- running the stages -------------------------------------------------- */

typedef enum {
    GROW_STAGE_STRUCTURE,
    GROW_STAGE_MOTIVES,
    GROW_STAGE_HARMONY,
    GROW_STAGE_BASS,
    GROW_STAGE_MELODY,
    GROW_STAGE_TEXTURE,
    GROW_STAGE_COUNT
} GrowStage;

const char *grow_stage_name(GrowStage stage);

/* All-zero is the default configuration (seed 0). */
typedef struct {
    unsigned long seed;
    GrowContext context;
    const MelodyFillParams *melody; /* NULL for the defaults; must outlive grow_run */
    size_t max_iterations;          /* per stage; 0 means FILL_DEFAULT_MAX_ITERATIONS */
} GrowConfig;

typedef struct {
    unsigned long numerator;
    unsigned long denominator; /* at least 1 */
} GrowScore;

typedef struct {
    FillRecord *records[GROW_STAGE_COUNT]; /* owned; NULL for a stage that did not run */
    bool stuck;
    GrowStage stuck_stage;                 /* the stage that left a target open, when stuck */
    FormHearResult heard_result;           /* FORM_HEAR_ERR_NOT_WRITTEN when the run did not complete */
    FormHeard heard;                       /* owned; the themes form_hear found over the theme section */
    int heard_form;                        /* PhraseLevelForm of the first theme heard, -1 for none */
    int requested_form;                    /* PhraseLevelForm the theme section asks for */
    int requested_beginning;               /* PhraseBeginning and PhraseEnding of the pairing it asks for, -1 for none */
    int requested_ending;
    bool heard_as_requested;               /* the requested form; for a hybrid, with the requested beginning and ending */
    GrowScore score;                       /* the share of the structure's soft rules and the heard form that hold */
} GrowOutcome;

void grow_outcome_destroy(GrowOutcome *outcome);

/*
 * Runs the stages on a plan made by grow_build_plan. GROW_OK with the plan complete, or GROW_ERR_STUCK with the
 * stage that failed in outcome->stuck_stage (the plan is then partly filled). After a complete run the theme is
 * heard (form_hear): heard as the requested form adds to the score; a mismatch is reported, not rejected.
 */
GrowResult grow_run(PiecePlan *plan, const GrowConfig *config, GrowOutcome *outcome);

#endif
