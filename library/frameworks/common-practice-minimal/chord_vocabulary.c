#include "chord_vocabulary.h"

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
} TriadDescriptor;

/* vii\xC2\xB0 (diminished) uses the "-dim" suffix in identifiers; the
   label itself carries the real degree symbol. */
static const TriadDescriptor DESCRIPTORS[COMMON_PRACTICE_TRIAD_COUNT] = {
    {"I", "common-practice-minimal-rule-I", "common-practice-minimal-constraint-I",
     "common-practice-minimal-chord-I"},
    {"ii", "common-practice-minimal-rule-ii", "common-practice-minimal-constraint-ii",
     "common-practice-minimal-chord-ii"},
    {"iii", "common-practice-minimal-rule-iii", "common-practice-minimal-constraint-iii",
     "common-practice-minimal-chord-iii"},
    {"IV", "common-practice-minimal-rule-IV", "common-practice-minimal-constraint-IV",
     "common-practice-minimal-chord-IV"},
    {"V", "common-practice-minimal-rule-V", "common-practice-minimal-constraint-V",
     "common-practice-minimal-chord-V"},
    {"vi", "common-practice-minimal-rule-vi", "common-practice-minimal-constraint-vi",
     "common-practice-minimal-chord-vi"},
    {"vii\xC2\xB0", "common-practice-minimal-rule-vii-dim", "common-practice-minimal-constraint-vii-dim",
     "common-practice-minimal-chord-vii-dim"},
};

static Rule *g_rules[COMMON_PRACTICE_TRIAD_COUNT];
static Constraint *g_constraints[COMMON_PRACTICE_TRIAD_COUNT];
static bool g_init_attempted[COMMON_PRACTICE_TRIAD_COUNT];

static bool valid_triad(CommonPracticeTriad triad) {
    return triad >= 0 && triad < COMMON_PRACTICE_TRIAD_COUNT;
}

static void ensure_built(CommonPracticeTriad triad) {
    if (!valid_triad(triad) || g_init_attempted[triad]) {
        return;
    }
    g_init_attempted[triad] = true;

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return;
    }

    EntityID *rule_id;
    if (entity_id_create(DESCRIPTORS[triad].rule_id, &rule_id) != ENTITY_ID_OK) {
        return;
    }
    Rule *rule;
    if (rule_create(rule_id, framework, NULL, &rule) != RULE_OK) {
        entity_id_destroy(rule_id);
        return;
    }

    EntityID *constraint_id;
    if (entity_id_create(DESCRIPTORS[triad].constraint_id, &constraint_id) != ENTITY_ID_OK) {
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

    g_rules[triad] = rule;
    g_constraints[triad] = constraint;
}

const char *common_practice_triad_label(CommonPracticeTriad triad) {
    if (!valid_triad(triad)) {
        return NULL;
    }
    return DESCRIPTORS[triad].label;
}

const Rule *common_practice_triad_rule(CommonPracticeTriad triad) {
    ensure_built(triad);
    return valid_triad(triad) ? g_rules[triad] : NULL;
}

const Constraint *common_practice_triad_constraint(CommonPracticeTriad triad) {
    ensure_built(triad);
    return valid_triad(triad) ? g_constraints[triad] : NULL;
}

ChordResult common_practice_build_triad_chord(CommonPracticeTriad triad, EntityID *sonority_target_id, Chord **out) {
    if (!valid_triad(triad)) {
        return CHORD_ERR_NULL_LABEL;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return CHORD_ERR_ALLOC;
    }

    char interpretation_id_value[256];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "%s-%s",
             DESCRIPTORS[triad].chord_interpretation_id_prefix, entity_id_value(sonority_target_id));

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
    const char *label = DESCRIPTORS[triad].label;
    if (value_create("common-practice-minimal/triad-label", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return CHORD_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
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
                               uncertainty, "common-practice-minimal-v1", "diatonic-triad-vocabulary",
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