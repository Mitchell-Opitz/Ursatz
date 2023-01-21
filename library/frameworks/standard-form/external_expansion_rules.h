#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_EXTERNAL_EXPANSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_EXTERNAL_EXPANSION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/expansion_placement.h"
#include "semantics/external_expansion_kind.h"

/*
 * External expansion rules for standard-form-v1. An external expansion adds length outside the
 * bounds of a phrase, either before its beginning (a prefix) or after its cadence (a suffix):
 *
 *   PREFIX          comes before the beginning of the phrase, usually as an introduction; small
 *                   (an instrumental introduction to a song) or large (a slow introduction)
 *   SUFFIX          comes after the phrase has cadenced
 *   SUFFIX_KIND     the kind of suffix follows its location: within a section it is a
 *                   post-cadential extension, after the end of a section a codetta, after the
 *                   end of a piece or movement a coda
 *   SUFFIX_LENGTH   a guideline for how long each is: a post-cadential extension 1-2 measures,
 *                   a codetta 4-8, a coda 8 or more. The terms are used flexibly, so the length
 *                   never overrules the location; it is checked against it
 *   DIAGRAM_LINE    external expansions are drawn with dashed arcs or brackets, internal
 *                   ones with solid
 *
 * Positions and lengths are in a single consistent unit chosen by the caller (measures in the
 * examples).
 */

typedef enum {
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_PREFIX = 0,
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_SUFFIX,
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_SUFFIX_KIND,
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_SUFFIX_LENGTH,
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_DIAGRAM_LINE,
    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT
} StandardFormExternalExpansionRule;

const Rule *standard_form_external_expansion_rule(StandardFormExternalExpansionRule which);
const Constraint *standard_form_external_expansion_constraint(StandardFormExternalExpansionRule which);

/* SUFFIX: true for post-cadential extension, codetta, and coda; false for prefix and for an
   invalid kind. */
bool standard_form_external_kind_is_suffix(ExternalExpansionKind kind);

/* PREFIX and SUFFIX: true when position lies outside the phrase [phrase_start, phrase_end) on the
   side the kind belongs: before phrase_start for a prefix, at or after phrase_end for a suffix. */
bool standard_form_external_expansion_outside(size_t phrase_start, size_t phrase_end, size_t position,
                                              ExternalExpansionKind kind);

typedef enum {
    STANDARD_FORM_SUFFIX_LOCATION_WITHIN_SECTION = 0,
    STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION,
    STANDARD_FORM_SUFFIX_LOCATION_AFTER_PIECE,
    STANDARD_FORM_SUFFIX_LOCATION_COUNT
} StandardFormSuffixLocation;

/* SUFFIX_KIND: the suffix a location gives: post-cadential extension within a section, codetta
   after a section, coda after a piece or movement. False, leaving *out untouched, for an invalid
   location. */
bool standard_form_classify_suffix(StandardFormSuffixLocation location, ExternalExpansionKind *out);

/* SUFFIX_LENGTH: true when measures is within the guideline for the kind: 1-2 for a
   post-cadential extension, 4-8 for a codetta, 8 or more for a coda. False for a prefix, which
   has no guideline, and for an invalid kind. 3 measures is typical of none; 8 is typical of both a
   codetta and a coda. */
bool standard_form_suffix_length_typical(ExternalExpansionKind kind, size_t measures);

/* SUFFIX_LENGTH: true when the length is typical of the suffix kind the location gives. */
bool standard_form_suffix_length_agrees_with_location(StandardFormSuffixLocation location, size_t measures);

typedef enum {
    STANDARD_FORM_DIAGRAM_LINE_SOLID = 0,
    STANDARD_FORM_DIAGRAM_LINE_DASHED,
    STANDARD_FORM_DIAGRAM_LINE_COUNT
} StandardFormDiagramLine;

/* DIAGRAM_LINE: dashed for an external expansion, solid for an internal one. False, leaving *out
   untouched, for an invalid placement. */
bool standard_form_placement_diagram_line(ExpansionPlacement placement, StandardFormDiagramLine *out);

#endif
