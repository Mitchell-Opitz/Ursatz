#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_LABEL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_LABEL_RULES_H

#include <stdbool.h>

#include "harmony_phrase_labels.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Phrase model labeling rules for standard-harmony-v1, on a phrase and the cadence
 * the listener heard at its end (harmony_phrase_labels.h).
 *
 * Hard:
 *
 *   ENDS_AS_CADENCE        the phrase ends as its cadence says: an authentic cadence
 *                          (perfect or imperfect) ends with V or V7 and then the tonic,
 *                          a half cadence ends on V. A subverted cadence is not a
 *                          cadence, so this fails for it
 *
 * Soft ("rarely", "usually"):
 *
 *   STARTS_ON_TONIC        the phrase begins on a tonic, which gets the Tb label; a
 *                          phrase may rarely delay it or omit it, more often in
 *                          Romantic music
 *   HAS_STRONG_PREDOMINANT a strong predominant comes immediately before the cadential
 *                          dominant, and gets the PD label (a cadential 6/4 may sit
 *                          between them); a phrase may go straight to V from tonic or
 *                          from a weak predominant
 *
 * Every check returns false for an invalid heard phrase.
 */

typedef struct {
    HarmonyPhrase phrase;
    CadenceType cadence;
} HarmonyHeardPhrase;

typedef enum {
    HARMONY_LABELS_ENDS_AS_CADENCE = 0,
    HARMONY_LABELS_STARTS_ON_TONIC,
    HARMONY_LABELS_HAS_STRONG_PREDOMINANT,
    HARMONY_LABELS_RULE_COUNT
} HarmonyLabelsRule;

/* True for a valid phrase and a valid cadence type. */
bool harmony_heard_phrase_valid(const HarmonyHeardPhrase *heard);

const Rule *harmony_labels_rule(HarmonyLabelsRule which);
const Constraint *harmony_labels_constraint(HarmonyLabelsRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_labels_preference(HarmonyLabelsRule which);

/* "ends-as-cadence" "starts-on-tonic" "has-strong-predominant"; NULL for an invalid
   rule. */
const char *harmony_labels_rule_name(HarmonyLabelsRule which);

/* True when the heard phrase satisfies the rule. False when it does not, and for an
   invalid heard phrase or an invalid rule. */
bool harmony_labels_rule_holds(HarmonyLabelsRule which, const HarmonyHeardPhrase *heard);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid heard
   phrase. */
bool harmony_labels_check(const HarmonyHeardPhrase *heard, unsigned *hard_violations, unsigned *soft_violations);

#endif
