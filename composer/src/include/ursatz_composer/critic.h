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

#endif
