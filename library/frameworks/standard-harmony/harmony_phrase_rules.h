#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRASE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Phrase model rules for standard-harmony-v1. The phrase model is the typical
 * order and flow of harmonic functions in a phrase: a phrase needs at least
 * tonic and dominant to show a sense of trajectory, usually has a predominant
 * between them to heighten it, and almost always runs from left to right
 * (tonic, predominant, dominant, and optionally tonic again), not right to left.
 * A phrase is read as the run of its chords in a mode.
 *
 * Hard:
 *
 *   TONIC_THEN_DOMINANT   a tonic chord comes before a dominant chord: the least
 *                         a phrase needs to move toward a goal
 *
 * Soft:
 *
 *   NO_BACKWARD_MOTION    no chord moves right to left in the model: a
 *                         predominant never follows a dominant, and a tonic
 *                         never follows a predominant (a tonic after a dominant
 *                         is the final tonic, and staying on one function is
 *                         not backward). The chapter says "almost always", with
 *                         exceptions discussed later, so this is advice
 *   WEAK_PREDOMINANT_LEADS_ON
 *                         a weak predominant is followed by another predominant,
 *                         typically a stronger one; a weak predominant that ends
 *                         the phrase breaks nothing
 *
 * Every check returns false for an invalid phrase.
 */

typedef struct {
    HarmonyMode mode;
    const HarmonyChord *chords;
    size_t chord_count;
} HarmonyPhrase;

typedef enum {
    HARMONY_PHRASE_TONIC_THEN_DOMINANT = 0,
    HARMONY_PHRASE_NO_BACKWARD_MOTION,
    HARMONY_PHRASE_WEAK_PREDOMINANT_LEADS_ON,
    HARMONY_PHRASE_RULE_COUNT
} HarmonyPhraseRule;

/* True for a valid mode and one or more valid chords. */
bool harmony_phrase_valid(const HarmonyPhrase *phrase);

const Rule *harmony_phrase_rule(HarmonyPhraseRule which);
const Constraint *harmony_phrase_constraint(HarmonyPhraseRule which);

/* "tonic-then-dominant" "no-backward-motion" "weak-predominant-leads-on"; NULL
   for an invalid rule. */
const char *harmony_phrase_rule_name(HarmonyPhraseRule which);

/* True when the phrase satisfies the rule. False when it does not, and for an
   invalid phrase or an invalid rule. */
bool harmony_phrase_rule_holds(HarmonyPhraseRule which, const HarmonyPhrase *phrase);

/* The rules the phrase breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid phrase. */
bool harmony_phrase_check(const HarmonyPhrase *phrase, unsigned *hard_violations, unsigned *soft_violations);

#endif
