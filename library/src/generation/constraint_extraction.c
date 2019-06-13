#include "generation/constraint_extraction.h"

#include <stdlib.h>

struct ConstraintExtraction {
    Constraint **constraints;
    size_t constraint_count;
    Preference **preferences;
    size_t preference_count;
};

static void destroy_constraints(Constraint **constraints, size_t count) {
    if (constraints == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        constraint_destroy(constraints[i]);
    }
    free(constraints);
}

static void destroy_preferences(Preference **preferences, size_t count) {
    if (preferences == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        preference_destroy(preferences[i]);
    }
    free(preferences);
}

ConstraintExtractionResult constraint_extraction_create(Constraint **constraints, size_t constraint_count,
                                                          Preference **preferences, size_t preference_count,
                                                          ConstraintExtraction **out) {
    if (constraint_count > 0) {
        if (constraints == NULL) {
            return CONSTRAINT_EXTRACTION_ERR_NULL_CONSTRAINT;
        }
        for (size_t i = 0; i < constraint_count; i++) {
            if (constraints[i] == NULL) {
                return CONSTRAINT_EXTRACTION_ERR_NULL_CONSTRAINT;
            }
        }
    }
    if (preference_count > 0) {
        if (preferences == NULL) {
            return CONSTRAINT_EXTRACTION_ERR_NULL_PREFERENCE;
        }
        for (size_t i = 0; i < preference_count; i++) {
            if (preferences[i] == NULL) {
                return CONSTRAINT_EXTRACTION_ERR_NULL_PREFERENCE;
            }
        }
    }

    ConstraintExtraction *extraction = (ConstraintExtraction *)malloc(sizeof(ConstraintExtraction));
    if (extraction == NULL) {
        return CONSTRAINT_EXTRACTION_ERR_ALLOC;
    }

    Constraint **copied_constraints = NULL;
    if (constraint_count > 0) {
        copied_constraints = (Constraint **)malloc(sizeof(Constraint *) * constraint_count);
        if (copied_constraints == NULL) {
            free(extraction);
            return CONSTRAINT_EXTRACTION_ERR_ALLOC;
        }
        for (size_t i = 0; i < constraint_count; i++) {
            copied_constraints[i] = constraints[i];
        }
    }

    Preference **copied_preferences = NULL;
    if (preference_count > 0) {
        copied_preferences = (Preference **)malloc(sizeof(Preference *) * preference_count);
        if (copied_preferences == NULL) {
            free(copied_constraints);
            free(extraction);
            return CONSTRAINT_EXTRACTION_ERR_ALLOC;
        }
        for (size_t i = 0; i < preference_count; i++) {
            copied_preferences[i] = preferences[i];
        }
    }

    extraction->constraints = copied_constraints;
    extraction->constraint_count = constraint_count;
    extraction->preferences = copied_preferences;
    extraction->preference_count = preference_count;

    *out = extraction;
    return CONSTRAINT_EXTRACTION_OK;
}

void constraint_extraction_destroy(ConstraintExtraction *extraction) {
    if (extraction == NULL) {
        return;
    }
    destroy_constraints(extraction->constraints, extraction->constraint_count);
    destroy_preferences(extraction->preferences, extraction->preference_count);
    free(extraction);
}

size_t constraint_extraction_constraint_count(const ConstraintExtraction *extraction) {
    return extraction->constraint_count;
}

const Constraint *constraint_extraction_constraint_at(const ConstraintExtraction *extraction, size_t index) {
    return extraction->constraints[index];
}

size_t constraint_extraction_preference_count(const ConstraintExtraction *extraction) {
    return extraction->preference_count;
}

const Preference *constraint_extraction_preference_at(const ConstraintExtraction *extraction, size_t index) {
    return extraction->preferences[index];
}