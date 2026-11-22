#ifndef URSATZ_COMPOSER_CRITIC_H
#define URSATZ_COMPOSER_CRITIC_H

#include <stdbool.h>
#include <stddef.h>

#include <model/core/rational.h>
#include <theory/knowledge/knowledge_snapshot.h>

#include <plan/plan.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/render.h>

/*
 * The critic (R-716 to R-722, docs/critic.md): a judgment of a candidate in her words. It reads the candidate's melody line, eight
 * evaluators measure what she has said about a melody (a monotonous rhythm, a contrast too close or too far, too many skips and
 * leaps, an ending that does not end, a stray syncopation, one rhythm across a set, repeated notes, a range wider than a tenth),
 * and the score is minus the weighted sum of their penalties. Every value is an exact Rational. An evaluator's code, its function
 * name and its parameters are the data of the knowledge entry michelle.rule.<code> (R-717): a test pins the table here to the
 * entries both ways, and the parameters are read from the snapshot, never written in the code.
 */

typedef enum {
    CRITIC_OK = 0,
    CRITIC_ERR_NULL,
    CRITIC_ERR_ALLOC,
    CRITIC_ERR_RENDER,    /* the plan or motive could not be rendered, or has no meter */
    CRITIC_ERR_EMPTY,     /* the candidate has no melody note */
    CRITIC_ERR_KNOWLEDGE, /* an evaluator's entry or one of its parameters is missing from the snapshot */
    CRITIC_ERR_OVERFLOW
} CriticResult;

typedef enum {
    CRITIC_RHYTHMIC_MONOTONY,
    CRITIC_CONTRAST_DISTANCE,
    CRITIC_LEAP_LOAD,
    CRITIC_OPEN_ENDING,
    CRITIC_STRAY_SYNCOPATION,
    CRITIC_ONE_RHYTHM,
    CRITIC_REPEATED_NOTES,
    CRITIC_WIDE_RANGE,
    CRITIC_EVALUATOR_COUNT
} CriticEvaluator;

/* The evaluator's code ("rhythmic_monotony"): the settings key leaf and the code of its entry. NULL for an invalid value. */
const char *critic_evaluator_code(CriticEvaluator evaluator);
/* Reads a code back (exact). False, output untouched, for NULL or an unknown code. */
bool critic_evaluator_from_code(const char *code, CriticEvaluator *out);
/* The name of the function that computes the evaluator, as its entry's `function` holds it ("critic_rhythmic_monotony"). */
const char *critic_evaluator_function(CriticEvaluator evaluator);

/* The settings of the critic (the preferences' `critic` group): the weight of each evaluator, which count the other way, and ranking. */
typedef struct {
    int weights[CRITIC_EVALUATOR_COUNT]; /* 0 to 20 */
    bool reversed[CRITIC_EVALUATOR_COUNT];
    bool rank;         /* rank theme candidates by the critic (off by default) */
    int candidates;    /* 2 to 8: how many candidates for each one kept */
    int keep_rejected; /* 0 to 16: how many rejected candidates a run reports */
} CriticSettings;

/* Weights 1, nothing reversed, rank false, candidates 4, keep_rejected 8. */
void critic_settings_default(CriticSettings *settings);

/*
 * One note of the melody line. onset and duration are in whole notes from the start of the plan's render (a motive's own timeline
 * starts at 0); bar is the bar number the MusicXML export writes (a pickup is bar 0, the first full bar 1); position is the onset
 * from the start of that bar.
 */
typedef struct {
    Rational onset;
    Rational duration;
    int pitch;
    int bar;
    Rational position;
} CriticNote;

#define CRITIC_PAIR_MAX 4
#define CRITIC_NAME_MAX 64

/* A contrasting idea and the idea it contrasts against, as runs of the line's notes. */
typedef struct {
    char idea[CRITIC_NAME_MAX];
    char against[CRITIC_NAME_MAX];
    size_t idea_first, idea_count;
    size_t against_first, against_count;
    Rational idea_start, against_start;   /* where each idea begins, in the line's time */
    Rational idea_length, against_length; /* the length of each idea's span */
} CriticIdeaPair;

typedef struct {
    CriticNote *notes; /* owned, in time order */
    size_t count;
    Rational beat; /* the length of one beat in the first note's measure */
    CriticIdeaPair pairs[CRITIC_PAIR_MAX];
    size_t pair_count;
} CriticLine;

/* The melody line of a render of `plan`: the notes of the melody lane's voice 0, and the contrasting ideas of the plan's themes. */
CriticResult critic_line_from_render(const Plan *plan, const RenderedPlan *rendered, CriticLine *out);

/* The line of a motive, rendered as its own one-voice plan; no idea pairs. */
CriticResult critic_line_from_motive(const Motive *motive, CriticLine *out);

/* Frees the notes and zeroes the struct. Safe on a zeroed struct. */
void critic_line_destroy(CriticLine *line);

/* What one evaluator found: its penalty (0 to 1) and where and by how much. bar is -1 when the finding has no place. */
typedef struct {
    CriticEvaluator evaluator;
    Rational penalty;
    int bar;
    Rational beat; /* 1-based, as a ratio when the place is between beats */
    int first, last, count;
    Rational value, limit;
    char idea[CRITIC_NAME_MAX];
    char against[CRITIC_NAME_MAX];
    char aspect[16];
} CriticFinding;

/*
 * Every evaluator over the line: out[i] is the finding of evaluator i. `set` holds the lines of the candidates judged together
 * (the line itself among them, skipped by pointer); with none or one, one_rhythm is 0. The parameters of each evaluator are read
 * from michelle.rule.<code> of the snapshot; a missing entry or parameter is CRITIC_ERR_KNOWLEDGE.
 */
CriticResult critic_evaluate(const CriticLine *line, const CriticLine *const *set, size_t set_count, const KnowledgeSnapshot *knowledge,
                             CriticFinding out[CRITIC_EVALUATOR_COUNT]);

/* Minus the sum of weight times penalty; an evaluator in settings->reversed counts plus. 0/1 on overflow. */
Rational critic_score(const CriticFinding findings[CRITIC_EVALUATOR_COUNT], const CriticSettings *settings);


/*
 * The critic's words (R-720). An observation is a plain-word note from the entry's message template: a scored one is an evaluator's
 * finding with a penalty above 0; the others say what the hearing of a theme found against its plan (a planned cadence heard as another,
 * a planned form heard as another, a contrast target the melody misses) and are never scored.
 */
#define CRITIC_OBSERVATION_MAX 32
#define CRITIC_MESSAGE_MAX 200

typedef struct {
    char rule[32];  /* the code of the entry michelle.rule.<code> */
    bool scored;    /* true for an evaluator's */
    int bar;        /* -1 when there is no place */
    Rational beat;  /* 1-based */
    Rational penalty; /* the evaluator's, 0 for an observation that is not scored */
    char message[CRITIC_MESSAGE_MAX];
} CriticObservation;

typedef struct {
    Rational score;
    CriticFinding findings[CRITIC_EVALUATOR_COUNT];
    CriticObservation observations[CRITIC_OBSERVATION_MAX];
    size_t observation_count;
} CriticJudgment;

/* The values a message may name that a finding does not hold: {planned}, {heard} and {amount}. */
typedef struct {
    char planned[32];
    char heard[32];
    Rational amount;
    bool has_amount;
} CriticFields;

/*
 * Writes `template` into `out` (size bytes, always ended when size is above 0) with each {name} replaced by the value of that field of
 * the finding or of `fields` (either may be NULL): a Rational as an integer or n/d, a beat 1-based, a bar or text as it is. A name with
 * no value stays as written, as does an unknown one. Returns the length written.
 */
size_t critic_message(const char *template, const CriticFinding *finding, const CriticFields *fields, char *out, size_t size);

/*
 * critic_evaluate and critic_score over the line, and one scored observation for each finding with a penalty above 0, in evaluator
 * order, its message from the entry michelle.rule.<code>.
 */
CriticResult critic_judge(const CriticLine *line, const CriticLine *const *set, size_t set_count, const KnowledgeSnapshot *knowledge,
                          const CriticSettings *settings, CriticJudgment *out);

typedef struct ComposerHearing ComposerHearing;

/*
 * Appends, to a judgment, the observations of the Composer's hearing of the theme node against its plan: cadence_heard for each planned
 * cadence the Analyzer hears as another, theme_heard when it hears another form than the pairing asks for, contrast_heard for each
 * contrast target the line's melody misses. None is scored. A theme the hearing cannot read adds none. The plan is the one the hearing
 * was made from; the line is the one made from the hearing's render.
 */
CriticResult critic_observe(ComposerHearing *hearing, const Plan *plan, NodeId theme, const CriticLine *line, const KnowledgeSnapshot *knowledge,
                            CriticJudgment *inout);

#endif
