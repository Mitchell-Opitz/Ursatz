#include "external_expansion_rules.h"

#include <stddef.h>

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

