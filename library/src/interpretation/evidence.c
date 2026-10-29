#include "interpretation/evidence.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct Evidence {
    EntityID *id;

    unsigned long strength_numerator;
    unsigned long strength_denominator;

    unsigned long relevance_numerator;
    unsigned long relevance_denominator;

    unsigned long reliability_numerator;
    unsigned long reliability_denominator;

    bool independence;
    char *method;
    char *source;
};

static EvidenceResult check_fraction(unsigned long numerator, unsigned long denominator) {
    if (denominator == 0) {
        return EVIDENCE_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (numerator > denominator) {
        return EVIDENCE_ERR_SCORE_OUT_OF_RANGE;
    }
    return EVIDENCE_OK;
}

EvidenceResult evidence_create(EntityID *id, unsigned long strength_numerator, unsigned long strength_denominator,
                                unsigned long relevance_numerator, unsigned long relevance_denominator,
                                unsigned long reliability_numerator, unsigned long reliability_denominator,
                                bool independence, const char *method, const char *source, Evidence **out) {
    if (id == NULL) {
        return EVIDENCE_ERR_NULL_ID;
    }

    EvidenceResult fraction_result = check_fraction(strength_numerator, strength_denominator);
    if (fraction_result != EVIDENCE_OK) {
        return fraction_result;
    }
    fraction_result = check_fraction(relevance_numerator, relevance_denominator);
    if (fraction_result != EVIDENCE_OK) {
        return fraction_result;
    }
    fraction_result = check_fraction(reliability_numerator, reliability_denominator);
    if (fraction_result != EVIDENCE_OK) {
        return fraction_result;
    }

    if (method == NULL || method[0] == '\0') {
        return EVIDENCE_ERR_NULL_METHOD;
    }
    if (source == NULL || source[0] == '\0') {
        return EVIDENCE_ERR_NULL_SOURCE;
    }

    Evidence *evidence = (Evidence *)malloc(sizeof(Evidence));
    if (evidence == NULL) {
        return EVIDENCE_ERR_ALLOC;
    }

    evidence->method = ursatz_string_copy(method);
    if (evidence->method == NULL) {
        free(evidence);
        return EVIDENCE_ERR_ALLOC;
    }

    evidence->source = ursatz_string_copy(source);
    if (evidence->source == NULL) {
        free(evidence->method);
        free(evidence);
        return EVIDENCE_ERR_ALLOC;
    }

    evidence->id = id;
    evidence->strength_numerator = strength_numerator;
    evidence->strength_denominator = strength_denominator;
    evidence->relevance_numerator = relevance_numerator;
    evidence->relevance_denominator = relevance_denominator;
    evidence->reliability_numerator = reliability_numerator;
    evidence->reliability_denominator = reliability_denominator;
    evidence->independence = independence;

    *out = evidence;
    return EVIDENCE_OK;
}

void evidence_destroy(Evidence *evidence) {
    if (evidence == NULL) {
        return;
    }
    entity_id_destroy(evidence->id);
    free(evidence->method);
    free(evidence->source);
    free(evidence);
}

const EntityID *evidence_id(const Evidence *evidence) {
    return evidence->id;
}

unsigned long evidence_strength_numerator(const Evidence *evidence) {
    return evidence->strength_numerator;
}

unsigned long evidence_strength_denominator(const Evidence *evidence) {
    return evidence->strength_denominator;
}

unsigned long evidence_relevance_numerator(const Evidence *evidence) {
    return evidence->relevance_numerator;
}

unsigned long evidence_relevance_denominator(const Evidence *evidence) {
    return evidence->relevance_denominator;
}

unsigned long evidence_reliability_numerator(const Evidence *evidence) {
    return evidence->reliability_numerator;
}

unsigned long evidence_reliability_denominator(const Evidence *evidence) {
    return evidence->reliability_denominator;
}

bool evidence_independence(const Evidence *evidence) {
    return evidence->independence;
}

const char *evidence_method(const Evidence *evidence) {
    return evidence->method;
}

const char *evidence_source(const Evidence *evidence) {
    return evidence->source;
}