#ifndef URSATZ_THEORY_KNOWLEDGE_RULE_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_RULE_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"

/*
 * K13 rule (target architecture 5.2, R-717): a judgment of a candidate or a limit on it, as an entry a person can read, cite and
 * change. The `rule` kind has three subtypes. A constraint is a declared limit the composing refuses (R-539) with the stage it
 * belongs to, whether a user may waive it and the setting that does. An evaluator is a weighted judgment in her words: a
 * default weight from 0 to 20 and up to four named parameters, each an integer or a ratio. An observation is a plain-word note on
 * what was heard; it is never scored and never refuses. Every rule has a `code`, the scope it judges, a `message` with {name}
 * placeholders from a fixed list, and the `function` that checks it: the name is data, a test of the application that owns the
 * function pins it. A snapshot owns the entries; the strings live as long as it does.
 */

#define RULE_PARAMETER_MAX 4
#define RULE_PARAMETER_NAME_MAX 24

typedef enum { RULE_CONSTRAINT, RULE_EVALUATOR, RULE_OBSERVATION } RuleSubtype;

typedef enum {
    RULE_SCOPE_NOTE,
    RULE_SCOPE_LINE,
    RULE_SCOPE_LINE_PAIR,
    RULE_SCOPE_SONORITY,
    RULE_SCOPE_PROGRESSION,
    RULE_SCOPE_PHRASE,
    RULE_SCOPE_SECTION,
    RULE_SCOPE_MOVEMENT,
    RULE_SCOPE_PERFORMER_PART,
    RULE_SCOPE_SET
} RuleScope;

typedef struct {
    char name[RULE_PARAMETER_NAME_MAX];
    Rational value;
} RuleParameter;

typedef struct {
    const char *id;
    const char *name;
    RuleSubtype subtype;
    const char *code;
    RuleScope scope;
    const char *message;
    const char *function;
    int default_weight; /* evaluator only, 0 to 20 */
    size_t parameter_count;
    RuleParameter parameters[RULE_PARAMETER_MAX]; /* evaluator only */
    const char *stage;   /* constraint only: motive, harmony, melody, texture or counterpoint */
    bool waivable;       /* constraint only */
    const char *setting; /* constraint only: the setting that waives it */
} RuleEntry;

#endif
