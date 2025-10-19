#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_HEARING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_HEARING_RULES_H

#include <stdbool.h>

#include "harmony_cadence_rules.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Cadence hearing rules for standard-harmony-v1: whether a potential cadence
 * point is a true cadence. The steps are to mark a potential point of rest,
 * check the harmonies involved, and listen for a sense of beginning after it.
 * Since a true cadence ends a phrase, it is very common for a new beginning to
 * follow it, either the start of the phrase just ended or new material that
 * starts a new phrase. If material from the middle of the phrase repeats
 * instead, the potential cadence is subverted.
 *
 * The subject is the potential cadence (harmony_cadence_rules.h; its
 * `subverted` flag is whether the music after it repeats material from the
 * middle of the phrase), whether a sense of beginning follows, and the length of
 * the phrase it would end, in measures, with whether the tempo is slow. The
 * caller says what is heard; this module does not listen.
 *
 * Hard:
 *
 *   POTENTIAL_CADENCE   the harmonies are V to I or x to V; any other ending is
 *                       not a cadence, though it may still be some kind of ending
 *   BEGINNING_FOLLOWS   a sense of beginning follows the point
 *   NOT_SUBVERTED       what follows does not repeat material from the middle of
 *                       the phrase
 *
 * Soft:
 *
 *   PHRASE_LENGTH       the phrase is more than two measures long unless the
 *                       tempo is quite slow: it is not common for phrases to be two
 *                       measures long, so a two-measure span is usually a point
 *                       marked too soon
 *
 * Every check returns false for an invalid hearing.
 */

typedef struct {
    HarmonyCadence candidate;
    bool beginning_follows;
    int phrase_measures;
    bool slow_tempo;
} HarmonyCadenceHearing;

typedef enum {
    HARMONY_HEARING_POTENTIAL_CADENCE = 0,
    HARMONY_HEARING_BEGINNING_FOLLOWS,
    HARMONY_HEARING_NOT_SUBVERTED,
    HARMONY_HEARING_PHRASE_LENGTH,
    HARMONY_HEARING_RULE_COUNT
} HarmonyHearingRule;

/* True for a valid candidate and a phrase of one or more measures. */
bool harmony_hearing_valid(const HarmonyCadenceHearing *hearing);

const Rule *harmony_hearing_rule(HarmonyHearingRule which);
const Constraint *harmony_hearing_constraint(HarmonyHearingRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_hearing_preference(HarmonyHearingRule which);

/* "potential-cadence" "beginning-follows" "not-subverted" "phrase-length"; NULL
   for an invalid rule. */
const char *harmony_hearing_rule_name(HarmonyHearingRule which);

/* True when the hearing satisfies the rule. False when it does not, and for an
   invalid hearing or an invalid rule. */
bool harmony_hearing_rule_holds(HarmonyHearingRule which, const HarmonyCadenceHearing *hearing);

/* The rules the hearing breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid hearing. */
bool harmony_hearing_check(const HarmonyCadenceHearing *hearing, unsigned *hard_violations,
                           unsigned *soft_violations);

#endif
