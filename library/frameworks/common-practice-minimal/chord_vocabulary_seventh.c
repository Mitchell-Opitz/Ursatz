#include "chord_vocabulary_seventh.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "interpretation/uncertainty.h"

typedef struct {
    const char *label;
    const char *rule_id;
    const char *constraint_id;
    const char *chord_interpretation_id_prefix;
} SeventhDescriptor;

static const SeventhDescriptor DESCRIPTORS[COMMON_PRACTICE_SEVENTH_COUNT] = {
    {"dominant7", "common-practice-minimal-rule-dominant7", "common-practice-minimal-constraint-dominant7",
     "common-practice-minimal-chord-dominant7"},
    {"major7", "common-practice-minimal-rule-major7", "common-practice-minimal-constraint-major7",
     "common-practice-minimal-chord-major7"},
    {"minor7", "common-practice-minimal-rule-minor7", "common-practice-minimal-constraint-minor7",
     "common-practice-minimal-chord-minor7"},
    {"half-diminished7", "common-practice-minimal-rule-half-diminished7",
     "common-practice-minimal-constraint-half-diminished7", "common-practice-minimal-chord-half-diminished7"},
    {"diminished7", "common-practice-minimal-rule-diminished7", "common-practice-minimal-constraint-diminished7",
     "common-practice-minimal-chord-diminished7"},
};

static const int SEVENTH_INTERVALS[COMMON_PRACTICE_SEVENTH_COUNT][4] = {
    {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9},
};

static Rule *g_rules[COMMON_PRACTICE_SEVENTH_COUNT];
static Constraint *g_constraints[COMMON_PRACTICE_SEVENTH_COUNT];
static bool g_init_attempted[COMMON_PRACTICE_SEVENTH_COUNT];

static bool valid_seventh(CommonPracticeSeventh seventh) {
    return seventh >= 0 && seventh < COMMON_PRACTICE_SEVENTH_COUNT;
}

bool common_practice_seventh_intervals(CommonPracticeSeventh seventh, int out[4]) {
    if (!valid_seventh(seventh) || out == NULL) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        out[i] = SEVENTH_INTERVALS[seventh][i];
    }
    return true;
}

static void ensure_built(CommonPracticeSeventh seventh) {
    if (!valid_seventh(seventh) || g_init_attempted[seventh]) {
        return;
    }
    g_init_attempted[seventh] = true;

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return;
    }

    EntityID *rule_id;
    if (entity_id_create(DESCRIPTORS[seventh].rule_id, &rule_id) != ENTITY_ID_OK) {
        return;
    }
    Rule *rule;
    if (rule_create(rule_id, framework, NULL, &rule) != RULE_OK) {
        entity_id_destroy(rule_id);
        return;
    }

    EntityID *constraint_id;
    if (entity_id_create(DESCRIPTORS[seventh].constraint_id, &constraint_id) != ENTITY_ID_OK) {
        rule_destroy(rule);
        return;
    }
    Constraint *constraint;
    if (constraint_create(constraint_id, framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &constraint) !=
        CONSTRAINT_OK) {
        entity_id_destroy(constraint_id);
        rule_destroy(rule);
        return;
    }

    g_rules[seventh] = rule;
    g_constraints[seventh] = constraint;
}

const char *common_practice_seventh_label(CommonPracticeSeventh seventh) {
    if (!valid_seventh(seventh)) {
        return NULL;
    }
    return DESCRIPTORS[seventh].label;
}

const Rule *common_practice_seventh_rule(CommonPracticeSeventh seventh) {
    ensure_built(seventh);
    return valid_seventh(seventh) ? g_rules[seventh] : NULL;
}

const Constraint *common_practice_seventh_constraint(CommonPracticeSeventh seventh) {
    ensure_built(seventh);
    return valid_seventh(seventh) ? g_constraints[seventh] : NULL;
}

ChordResult common_practice_build_seventh_chord_with_confidence(CommonPracticeSeventh seventh, EntityID *sonority_target_id,
                                                          unsigned long confidence_numerator,
                                                          unsigned long confidence_denominator,
                                                          const char *interpretation_id_suffix, Chord **out) {
    if (!valid_seventh(seventh)) {
        return CHORD_ERR_NULL_LABEL;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return CHORD_ERR_ALLOC;
    }

    char interpretation_id_value[256];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "%s-%s%s",
             DESCRIPTORS[seventh].chord_interpretation_id_prefix, entity_id_value(sonority_target_id),
             interpretation_id_suffix != NULL ? interpretation_id_suffix : "");

    EntityID *interpretation_id;
    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK) {
        return CHORD_ERR_ALLOC;
    }

    EntityID *framework_target_id;
    if (entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }
    FrameworkReference *framework_reference;
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        entity_id_destroy(framework_target_id);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Value *claim;
    const char *label = DESCRIPTORS[seventh].label;
    if (value_create("common-practice-minimal/seventh-label", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(confidence_numerator, confidence_denominator, &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Interpretation *interpretation;
    InterpretationResult interpretation_result =
        interpretation_create(interpretation_id, sonority_target_id, claim, NULL, NULL, framework_reference,
                               uncertainty, "common-practice-minimal-v1", "diatonic-seventh-vocabulary",
                               INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation);
    if (interpretation_result != INTERPRETATION_OK) {
        provenance_reference_destroy(provenance);
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    ChordResult chord_result = chord_create(interpretation, label, out);
    if (chord_result != CHORD_OK) {
        interpretation_destroy(interpretation);
        return chord_result;
    }

    return CHORD_OK;
}

ChordResult common_practice_build_seventh_chord(CommonPracticeSeventh seventh, EntityID *sonority_target_id, Chord **out) {
    return common_practice_build_seventh_chord_with_confidence(seventh, sonority_target_id, 1, 1, NULL, out);
}
