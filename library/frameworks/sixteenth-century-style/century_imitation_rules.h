#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_IMITATION_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_IMITATION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "century_score.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "standard-counterpoint/imitation_answer.h"

/*
 * Imitation rules for sixteenth-century-style-v1. Imitation is two or more
 * voices entering separately with the same melody, or versions of it,
 * especially to begin a movement or a section with a new line of text. The
 * melody is a point of imitation. The caller says where a point is: the
 * entries of the voices in the order they enter, and how many notes the
 * point has. Which passages are points is not worked out from the music.
 *
 *   SixteenthCenturyEntry   a voice and the index (in that voice's list of
 *                           notes, ties not counted) of the first note of
 *                           the point in it
 *   SixteenthCenturyPoint   the score, the entries (the first is the leader,
 *                           the rest answer it), and the number of notes of
 *                           the point (at least two)
 *
 * A point is valid when the score is valid, there are two to MAX_VOICES
 * entries, every entry names a voice with enough notes for the point, and
 * the entries begin at later and later slots.
 *
 * Hard:
 *
 *   RHYTHM_PRESERVED   every answer keeps the rhythm of the leader's first
 *                      three notes (or all of them, if the point is shorter)
 *   CONTOUR_PRESERVED  every answer moves in the same direction as the leader
 *                      through those notes
 *
 * Soft:
 *
 *   NOT_FREE           every answer stays close to the leader through the
 *                      whole point: it is a real or a tonal answer
 *                      (imitation_answer.h), not a free one
 *   ENTRY_INTERVAL     the second entry sounds a perfect consonance (unison,
 *                      fourth, fifth, or octave) from the first, the usual
 *                      interval between imitative parts at the start of a
 *                      section
 *   METRIC_MATCH       every answer begins at a metrically comparable
 *                      position (strong tactus, weak tactus, sub-tactus) to
 *                      the leader
 *   OVERLAP            every entry begins before the point has finished in
 *                      the entry before it
 *
 * Every check returns false for an invalid point.
 */

#define SIXTEENTH_CENTURY_IMITATION_CORE_NOTES 3

typedef struct {
    size_t voice;
    size_t note;
} SixteenthCenturyEntry;

typedef struct {
    const SixteenthCenturyScore *score;
    const SixteenthCenturyEntry *entries;
    size_t entry_count;
    size_t note_count;
} SixteenthCenturyPoint;

typedef enum {
    SIXTEENTH_CENTURY_IMITATION_RHYTHM_PRESERVED = 0,
    SIXTEENTH_CENTURY_IMITATION_CONTOUR_PRESERVED,
    SIXTEENTH_CENTURY_IMITATION_NOT_FREE,
    SIXTEENTH_CENTURY_IMITATION_ENTRY_INTERVAL,
    SIXTEENTH_CENTURY_IMITATION_METRIC_MATCH,
    SIXTEENTH_CENTURY_IMITATION_OVERLAP,
    SIXTEENTH_CENTURY_IMITATION_RULE_COUNT
} SixteenthCenturyImitationRule;

const Rule *sixteenth_century_imitation_rule(SixteenthCenturyImitationRule which);
const Constraint *sixteenth_century_imitation_constraint(SixteenthCenturyImitationRule which);

/* "rhythm-preserved" "contour-preserved" "not-free" "entry-interval" "metric-match"
   "overlap"; NULL for an invalid rule. */
const char *sixteenth_century_imitation_rule_name(SixteenthCenturyImitationRule which);

/* True when the point is valid (see above). */
bool sixteenth_century_point_valid(const SixteenthCenturyPoint *point);

/* How entry `entry` (1 or more) answers the leader: real when every interval
   of the point is the same size in semitones and direction, tonal when every
   interval moves in the same direction and is within one generic size of the
   leader's, free otherwise. False (*out untouched) for an invalid point or an
   entry that is not an answer. */
bool sixteenth_century_point_answer(const SixteenthCenturyPoint *point, size_t entry, ImitationAnswer *out);

/* True when the point satisfies the rule. False when it does not, and for an
   invalid point or an invalid rule. */
bool sixteenth_century_imitation_rule_holds(SixteenthCenturyImitationRule which,
                                            const SixteenthCenturyPoint *point);

/* The rules the point breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid point. */
bool sixteenth_century_imitation_check(const SixteenthCenturyPoint *point, unsigned *hard_violations,
                                       unsigned *soft_violations);

#endif
