#ifndef URSATZ_COMPOSER_SEAM_HEAR_H
#define URSATZ_COMPOSER_SEAM_HEAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The seam (R-171): the bar after the loop's end, where the loop starts again. Four soft rules say how well the end
 * leads into the start; they score a piece and are never a reason to reject one.
 *
 *   progression    the last chord before the seam and the first after it follow the phrase model (a tonic to a
 *                  predominant, a dominant or a tonic; a predominant to a dominant; a dominant to a tonic), or are the
 *                  same tonic
 *   no_parallels   bass and melody do not move in parallel fifths or octaves across the seam
 *   step           the melody moves at most an octave across the seam, and a leap over a fifth (a leap over 7
 *                  semitones) is followed by a step the other way
 *   leading_tone   a melody degree 7 before the seam goes to degree 1 (true when the melody there is not degree 7)
 *
 * held is how many of the four hold.
 */
typedef struct {
    bool progression;
    bool no_parallels;
    bool step;
    bool leading_tone;
    size_t held;
} SeamHeard;

#define SEAM_RULE_COUNT 4

/*
 * What the rules read. A pitch is a MIDI pitch, -1 for none. A rule whose notes are missing holds, except the
 * progression, which holds only with both chords.
 */
typedef struct {
    PlanKey key;
    bool has_chords;
    HarmonicFunction before_function;     /* of the last chord before the seam, when before_has_function */
    bool before_has_function;             /* false to read it from the numeral */
    char before_roman[PLAN_ROMAN_NUMERAL_MAX];
    HarmonicFunction after_function;      /* of the first chord after it, when after_has_function */
    bool after_has_function;
    char after_roman[PLAN_ROMAN_NUMERAL_MAX];
    int bass_before; /* the last bass note before the seam, and the first after it */
    int bass_after;
    int melody_previous; /* the melody note before melody_before */
    int melody_before;   /* the last melody note before the seam */
    int melody_after;    /* the first melody note after it */
    int melody_next;     /* the one after that */
} SeamNotes;

/* Judges the notes. */
void seam_hear_notes(const SeamNotes *notes, SeamHeard *out);

typedef enum {
    SEAM_HEAR_OK = 0,
    SEAM_HEAR_ERR_NULL,
    SEAM_HEAR_ERR_NOT_SEAM,    /* the node is not a seam section */
    SEAM_HEAR_ERR_NOT_WRITTEN  /* no chord before the seam or none after it */
} SeamHearResult;

/*
 * Hears the seam of a plan: the lanes right before the seam section starts, and the ones it holds. The key is the one
 * the key lane has at the seam.
 */
SeamHearResult seam_hear(const PiecePlan *plan, NodeId seam, SeamHeard *out);

/* The seam section that starts at the time, or PLAN_NODE_NONE. */
NodeId seam_section_at(const PiecePlan *plan, Rational time);

#endif
