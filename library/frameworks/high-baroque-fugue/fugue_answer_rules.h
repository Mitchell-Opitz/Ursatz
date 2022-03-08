#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_ANSWER_RULES_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_ANSWER_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "fugue_exposition.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/imitation_answer.h"

/*
 * Answer rules for high-baroque-fugue-v1. The answer is a transposition of
 * the subject that is also sometimes slightly altered. A real answer
 * transposes the subject by a perfect fifth or fourth and keeps every
 * generic interval; a tonal answer is altered further so that it stays in the
 * key. Each answer statement is compared with the first subject statement.
 *
 * Hard:
 *
 *   ANSWER_AT_FIFTH        the answer begins a perfect fifth or a perfect
 *                          fourth (compound included) from the subject
 *   TONIC_DOMINANT_START   a subject that begins on the tonic is answered on
 *                          the dominant, and a subject that begins on the
 *                          dominant (sol) is answered on the tonic (do), not
 *                          transposed up to re. Holds when the tonic is NULL
 *                          or the subject begins on another degree
 *   ANSWER_NOT_FREE        the answer is a real or a tonal answer: it has the
 *                          subject's number of notes, moves in the same
 *                          direction at every interval, and no interval
 *                          differs from the subject's by more than one in
 *                          generic size
 *   RHYTHM_PRESERVED       the answer keeps the subject's rhythm, note by note
 *
 * Soft:
 *
 *   MINIMAL_ADJUSTMENT     the answer is altered as little as possible: no
 *                          more intervals differ in generic size from the
 *                          subject's than the limit allows (FugueLimits)
 *
 * The rules hold when the exposition has no answer. A rule fails when it has
 * an answer but no subject. Every check returns false for an invalid
 * exposition.
 */

typedef enum {
    FUGUE_ANSWER_AT_FIFTH = 0,
    FUGUE_ANSWER_TONIC_DOMINANT_START,
    FUGUE_ANSWER_NOT_FREE,
    FUGUE_ANSWER_RHYTHM_PRESERVED,
    FUGUE_ANSWER_MINIMAL_ADJUSTMENT,
    FUGUE_ANSWER_RULE_COUNT
} FugueAnswerRule;

const Rule *fugue_answer_rule(FugueAnswerRule which);
const Constraint *fugue_answer_constraint(FugueAnswerRule which);

/* "answer-at-fifth" "tonic-dominant-start" "answer-not-free" "rhythm-preserved"
   "minimal-adjustment"; NULL for an invalid rule. */
const char *fugue_answer_rule_name(FugueAnswerRule which);

/* How the answer statement at `statement` answers the first subject: real
   when every interval is the same size in semitones and direction, tonal when
   every interval moves the same way and is within one generic size of the
   subject's, free otherwise (including a different number of notes). False
   (*out untouched) for an invalid exposition, a statement that is not an
   answer, or no subject. */
bool fugue_answer_kind(const FugueExposition *exposition, size_t statement, ImitationAnswer *out);

/* True when the exposition satisfies the rule. False when it does not, and
   for an invalid exposition or an invalid rule. */
bool fugue_answer_rule_holds(FugueAnswerRule which, const FugueExposition *exposition);

/* The rules the exposition breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exposition. */
bool fugue_answer_check(const FugueExposition *exposition, unsigned *hard_violations, unsigned *soft_violations);

#endif
