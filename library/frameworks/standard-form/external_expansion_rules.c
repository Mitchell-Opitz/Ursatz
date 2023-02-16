#include "external_expansion_rules.h"

#include "rule_pair.h"
#include <stddef.h>

static const char *const NAMES[STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT] = {
    "external-expansion-prefix",
    "external-expansion-suffix",
    "external-expansion-suffix-kind",
    "external-expansion-suffix-length",
    "external-expansion-diagram-line"};

static StandardFormRulePair g_pairs[STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT];
static bool g_built[STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT];

static bool valid_rule(StandardFormExternalExpansionRule which) {
    return which >= 0 && which < STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT;
}

static void ensure_built(StandardFormExternalExpansionRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_external_expansion_rule(StandardFormExternalExpansionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_external_expansion_constraint(StandardFormExternalExpansionRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_form_external_kind_is_suffix(ExternalExpansionKind kind) {
    return kind == EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION || kind == EXTERNAL_EXPANSION_KIND_CODETTA ||
           kind == EXTERNAL_EXPANSION_KIND_CODA;
}

bool standard_form_external_expansion_outside(size_t phrase_start, size_t phrase_end, size_t position,
                                              ExternalExpansionKind kind) {
    if (kind == EXTERNAL_EXPANSION_KIND_PREFIX) {
        return position < phrase_start;
    }
    return standard_form_external_kind_is_suffix(kind) && position >= phrase_end;
}

bool standard_form_classify_suffix(StandardFormSuffixLocation location, ExternalExpansionKind *out) {
    switch (location) {
        case STANDARD_FORM_SUFFIX_LOCATION_WITHIN_SECTION:
            *out = EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION;
            return true;
        case STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION:
            *out = EXTERNAL_EXPANSION_KIND_CODETTA;
            return true;
        case STANDARD_FORM_SUFFIX_LOCATION_AFTER_PIECE:
            *out = EXTERNAL_EXPANSION_KIND_CODA;
            return true;
        default:
            return false;
    }
}

bool standard_form_suffix_length_typical(ExternalExpansionKind kind, size_t measures) {
    switch (kind) {
        case EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION:
            return measures >= 1 && measures <= 2;
        case EXTERNAL_EXPANSION_KIND_CODETTA:
            return measures >= 4 && measures <= 8;
        case EXTERNAL_EXPANSION_KIND_CODA:
            return measures >= 8;
        default:
            return false;
    }
}

bool standard_form_suffix_length_agrees_with_location(StandardFormSuffixLocation location, size_t measures) {
    ExternalExpansionKind kind;
    return standard_form_classify_suffix(location, &kind) && standard_form_suffix_length_typical(kind, measures);
}

bool standard_form_placement_diagram_line(ExpansionPlacement placement, StandardFormDiagramLine *out) {
    if (!expansion_placement_valid(placement)) {
        return false;
    }
    *out = placement == EXPANSION_PLACEMENT_EXTERNAL ? STANDARD_FORM_DIAGRAM_LINE_DASHED
                                                     : STANDARD_FORM_DIAGRAM_LINE_SOLID;
    return true;
}
