#ifndef URSATZ_COMPOSER_GROW_H
#define URSATZ_COMPOSER_GROW_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>
#include <ursatz_composer/theme_shape.h>

struct GrowPoolForm;

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
 *     the motive as a Given melody lane span. The motives made from it go beside it. The slot motives of the request
 *     are the nodes "s1" to "s3" after m0, a Given melody span each (the material is a bar longer for each). An
 *     accompaniment motive is the node "acc", in the last bar of the material.
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

/* The slots a theme's motives fill, GrowSlot, are in theme_shape.h with the ideas and their roles. */

/* "basic", "contrasting", "continuation" or "cadential". */
const char *grow_slot_name(GrowSlot slot);
/* The function of the slot's ideas (R-143): "beginning", "contrast", "continuation" or "cadential". */
const char *grow_slot_function(GrowSlot slot);


/* "basic idea", "repetition", "contrasting idea", ... as the plan's idea nodes are named. */
const char *grow_idea_name(GrowIdea idea);
/* Whether a theme of the pairing has the idea. */
bool grow_idea_in_pairing(const PlanThemePairing *pairing, GrowIdea idea);

typedef struct {
    const Motive *motive;
    const PlanSource *source; /* NULL, or where a corpus gave the motive (corpus_seed.h): a SOURCE constraint on m0 */
    AnalysisKey key;
    PlanMeter meter;
    size_t bars;              /* 4 or 8 */
    PlanThemePairing pairing; /* any of the nine beginning and ending pairings */
    CadenceType cadence;  /* PAC, IAC or HC: the cadence the theme ends in */
    /*
     * The weak cadence an antecedent beginning ends in: HC or IAC, and weaker than `cadence`. Zero (PAC) means the
     * default, HC. Read only for an antecedent beginning.
     */
    CadenceType first_cadence;
    /*
     * An accompaniment under the melody and bass, in the inner voices (accompaniment.h). Read only with
     * has_accompaniment; a texture of -1 asks for none in particular, and one that is not homophonic is refused.
     */
    PlanAccompaniment accompaniment;
    bool has_accompaniment;
    /*
     * A saved accompaniment-role motive as the accompaniment pattern (R-165), NULL for none. It is added to the
     * material as the node "acc" (a Given melody span) and the ACCOMPANIMENT constraint names it, pattern MOTIVE; the
     * texture, low and high of `accompaniment` are kept when has_accompaniment is set, the pattern is replaced. Refused
     * (GROW_ERR_BAD_REQUEST) with a motive of the melodic role, one longer than a bar, or together with a voicing.
     */
    const Motive *accompaniment_motive;
    /*
     * The motives of the other slots (R-166), NULL for none; the entry for GROW_SLOT_BASIC is not read (`motive` is the
     * basic motive). Each is added to the material as a node (s1 contrasting, s2 continuation, s3 cadential), a Given
     * melody span. The first idea of the slot is the motive itself (exact) or a form of it; the slot's later ideas
     * (the consequent contrasting idea, fragment 2, cadential 2) are made from it as the basic motive's are, and the
     * remoteness of their forms is measured from it. Refused (GROW_ERR_BAD_REQUEST) with a motive that is not a full
     * motive of two notes or more in the theme's key and meter, longer than the idea it begins (two bars of eight for
     * the contrasting idea and the cadential idea or cadential 1, one for fragment 1; a motive as long as the idea
     * fills it whole, as written), or for a slot the pairing has no
     * idea for (a contrasting slot in a sentence, a continuation slot without a continuation). Must outlive
     * grow_build_plan only.
     */
    const Motive *slot_motives[GROW_SLOT_COUNT];
    /*
     * The motive of single ideas (R-206), NULL for none; the entry for GROW_IDEA_BASIC is not read, the basic idea is
     * `motive`. Each is added to the material as the node "i<k>" (k the GrowIdea value), a Given melody span after the
     * slot motives, and the idea's melody restates it exactly. It takes the place of the slot's motive for that one
     * idea; the other ideas of the slot are made as before. The same Motive may be given for several ideas and slots.
     * Refused (GROW_ERR_BAD_REQUEST) with an idea the pairing does not have, an entry for the basic idea, a motive that
     * is not a full motive of two notes or more in the theme's key and meter, or one longer than its idea (a bar, or
     * the idea's eighths of the theme when less). Must outlive grow_build_plan only.
     */
    const Motive *idea_motives[GROW_IDEA_COUNT];
    /*
     * Chords Given over the theme (R-208), onsets from the theme's start (plan_give_chords), NULL for none. The harmony
     * stage fills what they leave open. Refused (GROW_ERR_BAD_REQUEST) when plan_give_chords refuses them. Must outlive
     * grow_build_plan only.
     */
    const GivenChord *chords;
    size_t chord_count;
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
    GROW_ERR_MOTIVE_TOO_LONG, /* the motive is longer than the basic idea it fills (two bars of an 8-bar theme, one of 4) */
    GROW_ERR_BAD_REQUEST,     /* a meter the harmony operators do not support, bars not 4 or 8, a pairing not built
                                 yet, a cadence other than PAC, IAC or HC, a motive of one note, an accompaniment
                                 that is not valid or is Alberti in a compound meter, an accompaniment and a
                                 voicing together */
    GROW_ERR_STUCK            /* a stage left a target open (GrowOutcome.stuck_stage) */
} GrowResult;

const char *grow_result_name(GrowResult result);

/* Why a request was refused before any seeding, and the request path of the field that caused it (B3 of P33). */
typedef struct {
    GrowResult result;
    char path[64];     /* the JSON pointer of the field, "" when no field is to blame */
    char message[256];
} GrowRefusal;

/* Builds the plan of the request into *out, which the caller destroys with plan_destroy. */
GrowResult grow_build_plan(const GrowRequest *request, PiecePlan **out);

/*
 * grow_build_plan with the reason for a refusal: when refusal is not NULL and the result is not GROW_OK, it holds the
 * result, the path of the field and a message. Only the first refusal is reported.
 */
GrowResult grow_build_plan_checked(const GrowRequest *request, PiecePlan **out, GrowRefusal *refusal);

/* --- the structure operator -------------------------------------------- */

/* How far a theme's presentation and continuation stray from its motif (ch. XI). All-zero is balanced. */
typedef enum { GROW_DEVELOP_BALANCED = 0, GROW_DEVELOP_MELODY, GROW_DEVELOP_THEME, GROW_DEVELOP_COUNT } GrowDevelopment;

/* How long a continuation's fragment is: its whole segment, half a bar of it, or a beat of it. */
typedef enum { GROW_FRAGMENT_SEGMENT, GROW_FRAGMENT_HALF_BAR, GROW_FRAGMENT_BEAT, GROW_FRAGMENT_LENGTH_COUNT } GrowFragmentLength;

/*
 * How a sentence's continuation accelerates (R-452, R-462 to R-464). All zero is the P34 continuation, and
 * fragment_lengths all 0 reads as {1, 0, 0}.
 * every_segment: both fragments are drawn from every segment of m0, never both from the same one.
 * fragment_lengths: the weights of a fragment being its whole segment, half a bar of it, or a beat of it.
 * diminish: a fragment that is a whole segment is that segment in diminution 1:2.
 * liquidate: fragment 2 is liquidated, and the cadential idea liquidates a step further than it.
 */
typedef struct {
    bool every_segment;
    unsigned fragment_lengths[GROW_FRAGMENT_LENGTH_COUNT];
    bool diminish;
    bool liquidate;
} GrowContinuation;


/*
 * The least fidelity an idea of a role may have under each development setting before the melody stage refuses it (R-355),
 * a setting of the preferences (R-411). The default is the table in grow_fidelity.c. GROW_ROLE_NONE and GROW_ROLE_CONTRAST
 * stay 0 and are not settings.
 */
typedef struct {
    MotiveScore minimum[GROW_DEVELOP_COUNT][GROW_ROLE_CONTRAST + 1];
} GrowFidelityTable;

typedef struct {
    size_t candidates; /* the structures grow-theme proposes; 0 means 4 */
    GrowDevelopment development; /* the weights of the repetition, the fragments and the liquidation (grow_beginning.c, grow_ending.c) */
    GrowContinuation continuation; /* all zero: today's continuation; set by the compose run and api_grow from the preferences */
    /*
     * The basic motive the request was made from, for the features (segments) and pickup the plan does not keep. Set by
     * the compose run from GrowConfig.basic; NULL for none. grow-theme uses it only when its notes are m0's.
     */
    const Motive *basic;
} GrowContext;

#define GROW_DEFAULT_CANDIDATES 4

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
    const ComposerStyle *style;     /* NULL for none; must outlive grow_run (compose.h) */
    ComposerRatio style_weight;     /* a denominator of 0 is 1/1 */
    /*
     * The tempo of the piece in quarter notes per minute, a denominator of 0 for none (R-163). The grow CLI takes it
     * from --tempo, else from the style; the stages that follow the tempo read it from here.
     */
    ComposerRatio tempo;
    /*
     * The basic motive the request was made from, for the harmony, features and role it carries, which the plan does
     * not keep: the motive-form operator reads them for m0. NULL for none; its notes must be m0's. Must outlive grow_run.
     */
    const Motive *basic;
    /*
     * Saved motive-forms the motives stage prefers over the registry's (grow_form.h GrowPoolForm), NULL for none. Must
     * outlive grow_run, as must the motives they point to.
     */
    const struct GrowPoolForm *pool;
    size_t pool_count;
    /* The fidelity minimums (R-355, R-411). NULL for the defaults; must outlive grow_run. */
    const GrowFidelityTable *fidelity;
    /* NULL for none. Reported once at the start of the run ("grow", 0, 1: one candidate), then as ComposeConfig.job. A cancel
       ends the run with GROW_ERR_STUCK and outcome->cancelled. */
    const ComposerJob *job;
} GrowConfig;

typedef struct {
    unsigned long numerator;
    unsigned long denominator; /* at least 1 */
} GrowScore;

typedef struct {
    FillRecord *records[GROW_STAGE_COUNT]; /* owned; NULL for a stage that did not run */
    bool stuck;
    GrowStage stuck_stage;                 /* the stage that left a target open, when stuck */
    bool cancelled;                        /* the job was cancelled (GrowConfig.job); stuck is false */
    FormHearResult heard_result;           /* FORM_HEAR_ERR_NOT_WRITTEN when the run did not complete */
    FormHeard heard;                       /* owned; the themes form_hear found over the theme section */
    int heard_form;                        /* PhraseLevelForm of the first theme heard, -1 for none */
    int requested_form;                    /* PhraseLevelForm the theme section asks for */
    int requested_beginning;               /* PhraseBeginning and PhraseEnding of the pairing it asks for, -1 for none */
    int requested_ending;
    bool heard_as_requested;               /* the requested form; for a hybrid, with the requested beginning and ending */
    GrowScore score;                       /* the share of the structure's soft rules and the heard form that hold */
    bool style_measured;                   /* with a style, after a complete run: the Analyzer's distance from it (compose.h) */
    StyleComparison style_distance;        /* owned */
    ComposeRejection *rejections;          /* owned; the runs thrown away before this one (compose.h), NULL when none */
    size_t rejection_count;
} GrowOutcome;

void grow_outcome_destroy(GrowOutcome *outcome);

/*
 * Runs the stages on a plan made by grow_build_plan. GROW_OK with the plan complete, or GROW_ERR_STUCK with the
 * stage that failed in outcome->stuck_stage (the plan is then partly filled). After a complete run the theme is
 * heard (form_hear): heard as the requested form adds to the score; a mismatch is reported, not rejected.
 */
GrowResult grow_run(PiecePlan *plan, const GrowConfig *config, GrowOutcome *outcome);

#endif
