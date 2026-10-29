#ifndef URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_RULES_H
#define URSATZ_FRAMEWORKS_HIGH_BAROQUE_FUGUE_FUGUE_RULES_H

#include <stdbool.h>

#include "fugue_exposition.h"
#include "standard-counterpoint/slot_note.h"

/*
 * Checking a whole exposition against the rule groups of
 * high-baroque-fugue-v1:
 *
 *   ENTRIES         fugue_entry_rules.h
 *   ANSWER          fugue_answer_rules.h
 *   COUNTERSUBJECT  fugue_countersubject_rules.h
 *
 * The result sets a bit (1u << group) in hard_violations for each group with
 * a hard rule broken, and in soft_violations for each group with a soft rule
 * broken. An exposition is valid when no hard rule is broken; soft violations
 * are advice. To learn exactly which rule failed, ask the group's own check.
 */

typedef enum {
    FUGUE_GROUP_ENTRIES = 0,
    FUGUE_GROUP_ANSWER,
    FUGUE_GROUP_COUNTERSUBJECT,
    FUGUE_GROUP_COUNT
} FugueGroup;

/* "entries" "answer" "countersubject"; NULL for an invalid group. */
const char *fugue_group_name(FugueGroup group);

/* The hard and soft violations of a run (standard-counterpoint/slot_note.h). */
typedef StandardCounterpointRuleResult FugueResult;

/* Runs the groups over the exposition. False (*out untouched) for an invalid
   exposition or NULL. */
bool fugue_check(const FugueExposition *exposition, FugueResult *out);

/* True when the exposition is valid and breaks no hard rule. */
bool fugue_valid(const FugueExposition *exposition);

#endif
