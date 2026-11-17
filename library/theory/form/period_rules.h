#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_PERIOD_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_PERIOD_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include "theory/form/phrase_rules.h"
#include "theory/vocabulary/cadence_type.h"
#include "theory/form/idea_label.h"
#include "theory/form/phrase_role.h"

/*
 * Period rules for standard-form-v1. A period is a phrase-level form of two phrases, an
 * antecedent and a consequent, each a basic idea followed by a contrasting idea:
 *
 *   STRUCTURE            an antecedent comes first, then a consequent
 *   PHRASE_IDEAS         each phrase is a basic idea followed by a contrasting idea
 *   CADENCES             both phrases end in a phrase goal and the consequent's cadence is
 *                        stronger than the antecedent's (the antecedent "asks", the
 *                        consequent "answers")
 *   ANTECEDENT_CADENCE   the antecedent most often ends with a half cadence
 *   CONSEQUENT_CADENCE   the consequent most often ends with a perfect authentic cadence
 *   LENGTH               the antecedent is often four measures; the consequent is as long
 *                        or longer, and rarely shorter
 *   MODULATION           a period may stay in one key or modulate; the antecedent usually
 *                        stays in the tonic key and any change of key usually happens in the
 *                        consequent
 *   CONSEQUENT_IDEAS     a consequent's basic idea is usually similar or identical to the
 *                        antecedent's, and its contrasting idea almost always differs; when
 *                        the basic ideas differ they are numbered b.i.1 and b.i.2
 *
 * Lengths are whole measures. Cadence order comes from standard-harmony's
 * harmony_cadence_strength, the one owner of cadence strength.
 */

/* STRUCTURE: true only for an antecedent followed by a consequent. */
bool standard_form_phrase_roles_in_order(PhraseRole first, PhraseRole second);

/* PHRASE_IDEAS: true only for a basic idea followed by a contrasting idea. */
bool standard_form_period_phrase_ideas_valid(IdeaLabel first, IdeaLabel second);

/* CADENCES: true when both cadences are phrase goals and the consequent's is stronger than the
   antecedent's. Equal cadences are not a period (see the repeated phrase). */
bool standard_form_period_cadences_valid(CadenceType antecedent, CadenceType consequent);

/* ANTECEDENT_CADENCE: true for the usual half cadence. Another goal is possible, just less
   usual. */
bool standard_form_antecedent_cadence_usual(CadenceType type);

/* CONSEQUENT_CADENCE: true for the usual perfect authentic cadence. */
bool standard_form_consequent_cadence_usual(CadenceType type);

/* LENGTH: true for the usual antecedent length, four. */
bool standard_form_antecedent_length_usual(size_t measures);

/* LENGTH: true when the consequent is at least as long as the antecedent (a zero-length
   antecedent is not valid). A shorter consequent is rare, not wrong. */
bool standard_form_consequent_length_usual(size_t antecedent, size_t consequent);

/* CONSEQUENT_IDEAS: true when the consequent's contrasting idea differs from the antecedent's,
   which is almost always. */
bool standard_form_consequent_contrast_usual(bool contrasting_idea_differs);

#endif
