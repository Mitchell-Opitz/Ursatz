#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_MATCHING_RULES_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_MATCHING_RULES_H

#include <stdbool.h>

#include "galant_exemplar.h"
#include "galant_prototype.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Matching rules for galant-schemas-v1: whether an exemplar resembles a
 * prototype. An exemplar is understood as a version of a prototype stage by
 * stage, so the rules compare them in turn. A degree is judged against the
 * exemplar's tonic (galant_degree.h); the harmony of a prototype (its figures
 * and numerals) is not checked.
 *
 * Hard:
 *
 *   STAGE_COUNT        the exemplar has the prototype's number of stages
 *   METER              every stage falls on the strength of beat the prototype
 *                      gives it (strong or weak)
 *   MELODY             every melody pitch is the degree the prototype gives
 *                      that stage (with an accepted alteration), and a stage has
 *                      a second melody pitch exactly when the prototype's does
 *   BASS_DEFINING      when the prototype's bass is defining (the Prinner),
 *                      every bass pitch is the degree it gives that stage;
 *                      holds when the bass is not defining
 *
 * Soft:
 *
 *   BASS_USUAL         when the prototype's bass is less fixed than its melody
 *                      (the Meyer), every bass pitch is the degree it usually
 *                      gives that stage; holds when the bass is defining
 *   TENTHS             when the prototype's melody accompanies its bass in
 *                      parallel tenths, at every stage the melody is a third
 *                      above the bass, or a tenth (compound intervals count as
 *                      simple)
 *
 * With a different number of stages the rules that compare stages do not hold.
 * Every check returns false for an invalid match.
 */

typedef struct {
    const GalantPrototype *prototype;
    const GalantExemplar *exemplar;
} GalantMatch;

typedef enum {
    GALANT_MATCH_STAGE_COUNT = 0,
    GALANT_MATCH_METER,
    GALANT_MATCH_MELODY,
    GALANT_MATCH_BASS_DEFINING,
    GALANT_MATCH_BASS_USUAL,
    GALANT_MATCH_TENTHS,
    GALANT_MATCH_RULE_COUNT
} GalantMatchRule;

const Rule *galant_matching_rule(GalantMatchRule which);
const Constraint *galant_matching_constraint(GalantMatchRule which);

/* "stage-count" "meter" "melody" "bass-defining" "bass-usual" "tenths"; NULL for an
   invalid rule. */
const char *galant_matching_rule_name(GalantMatchRule which);

/* True when the match has a prototype and a valid exemplar. */
bool galant_match_valid(const GalantMatch *match);

/* True when the exemplar satisfies the rule against the prototype. False when
   it does not, and for an invalid match or an invalid rule. */
bool galant_matching_rule_holds(GalantMatchRule which, const GalantMatch *match);

/* The rules the exemplar breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid match. */
bool galant_matching_check(const GalantMatch *match, unsigned *hard_violations, unsigned *soft_violations);

#endif
