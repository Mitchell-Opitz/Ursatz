#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_CHAIN_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_prolongation_rules.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Chained tonic prolongation for standard-harmony-v1. By chaining several of the
 * three-chord progressions (harmony_prolongation.h), composers extend the tonic's
 * influence for quite a while at a phrase beginning: I V4/3 I6 does not chain, but
 * I V6/5 I V4/3 I6 does, the last chord of one progression being the first of the next.
 * Every progression begins on I in root position, so the progressions join at a
 * root-position tonic, and a chain's bass is a run of the lines do ti do, do re do,
 * and so on, ending in the last progression's own (do re mi, do fa mi, do ti do).
 *
 * The subject is a run of progressions (harmony_prolongation_rules.h), in order, each
 * written in four voices.
 *
 * Hard:
 *
 *   LINKS_VALID   every progression breaks no hard rule of
 *                 harmony_prolongation_rules.h
 *   LINKED        the last chord of each progression is the first of the next: the
 *                 same chord, in the same voices, on the same beat strength
 *   SAME_MODE     the progressions are all in the same mode
 *
 * Every check returns false for an invalid chain.
 */

#define HARMONY_PROLONGATION_CHAIN_MAX 8

typedef struct {
    size_t link_count;
    HarmonyTonicProlongation links[HARMONY_PROLONGATION_CHAIN_MAX];
} HarmonyProlongationChain;

typedef enum {
    HARMONY_CHAIN_LINKS_VALID = 0,
    HARMONY_CHAIN_LINKED,
    HARMONY_CHAIN_SAME_MODE,
    HARMONY_CHAIN_RULE_COUNT
} HarmonyChainRule;

/* True for one to HARMONY_PROLONGATION_CHAIN_MAX valid progressions. */
bool harmony_chain_valid(const HarmonyProlongationChain *chain);

const Rule *harmony_chain_rule(HarmonyChainRule which);
const Constraint *harmony_chain_constraint(HarmonyChainRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_chain_preference(HarmonyChainRule which);

/* "links-valid" "linked" "same-mode"; NULL for an invalid rule. */
const char *harmony_chain_rule_name(HarmonyChainRule which);

/* True when the chain satisfies the rule. False when it does not, and for an invalid
   chain or an invalid rule. */
bool harmony_chain_rule_holds(HarmonyChainRule which, const HarmonyProlongationChain *chain);

/* The rules the chain breaks: hard_violations has bit (1u << rule) set for each hard
   rule that fails; soft_violations is always 0 (there are no soft rules). False
   (both untouched) for an invalid chain. */
bool harmony_chain_check(const HarmonyProlongationChain *chain, unsigned *hard_violations,
                         unsigned *soft_violations);

#endif
