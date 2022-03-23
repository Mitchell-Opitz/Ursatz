#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_ENTRY_RULES_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_ENTRY_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "fugue_exposition.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Entry rules for high-baroque-fugue-v1. In the exposition each voice enters
 * one by one with the subject or the answer (the statements with those roles,
 * in the order they begin), while the voice before it continues with a
 * countersubject. Entries beyond the number of voices are redundant entries.
 * "The first voice-count entries" below means the entries before any
 * redundant one.
 *
 * Hard:
 *
 *   FIRST_IS_SUBJECT        the first entry is the subject
 *   ENTRIES_ALTERNATE       the first voice-count entries alternate subject,
 *                           answer, subject, answer, and so on
 *   EACH_VOICE_ENTERS_ONCE  the first voice-count entries are in different
 *                           voices: every voice enters before any enters
 *                           again
 *   ENTRIES_ALIGNED         every entry, redundant ones included, begins at
 *                           the same position in the bar (metrically
 *                           comparable positions)
 *
 * Soft:
 *
 *   NEW_VOICE_AT_EXTREME    each new voice among the first voice-count
 *                           entries is the highest or the lowest of the voices
 *                           that have entered so far: an inner part is
 *                           uncommon as the last to enter
 *   COUNTERSUBJECT_FOLLOWS  the voice of each entry (but the last of the first
 *                           voice-count) continues with a countersubject that
 *                           begins where its entry ends or later, and no later
 *                           than the start of the next entry
 *
 * Every check returns false for an invalid exposition.
 */

typedef enum {
    FUGUE_ENTRY_FIRST_IS_SUBJECT = 0,
    FUGUE_ENTRY_ENTRIES_ALTERNATE,
    FUGUE_ENTRY_EACH_VOICE_ENTERS_ONCE,
    FUGUE_ENTRY_ENTRIES_ALIGNED,
    FUGUE_ENTRY_NEW_VOICE_AT_EXTREME,
    FUGUE_ENTRY_COUNTERSUBJECT_FOLLOWS,
    FUGUE_ENTRY_RULE_COUNT
} FugueEntryRule;

const Rule *fugue_entry_rule(FugueEntryRule which);
const Constraint *fugue_entry_constraint(FugueEntryRule which);

/* "first-is-subject" "entries-alternate" "each-voice-enters-once" "entries-aligned"
   "new-voice-at-extreme" "countersubject-follows"; NULL for an invalid rule. */
const char *fugue_entry_rule_name(FugueEntryRule which);

/* The number of subject and answer statements of a valid exposition; 0
   otherwise. */
size_t fugue_exposition_entry_count(const FugueExposition *exposition);

/* The number of redundant entries: entries beyond the number of voices. 0 for
   an invalid exposition. */
size_t fugue_exposition_redundant_entries(const FugueExposition *exposition);

/* True when the exposition satisfies the rule. False when it does not, and
   for an invalid exposition or an invalid rule. */
bool fugue_entry_rule_holds(FugueEntryRule which, const FugueExposition *exposition);

/* The rules the exposition breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exposition. */
bool fugue_entry_check(const FugueExposition *exposition, unsigned *hard_violations, unsigned *soft_violations);

#endif
