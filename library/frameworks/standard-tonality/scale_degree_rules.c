#include "scale_degree_rules.h"

#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "tonal_key.h"
#include "interpretation/uncertainty.h"
#include "major_scale_rules.h"
#include "rule_pair.h"
#include "tonal_pitch.h"

static const char *const NAMES[STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT] = {
    "scale-degree-numbers", "scale-degree-solfege", "scale-degree-names", "scale-degree-name-derivation",
    "scale-degree-leading-tone"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_tonality_framework, "standard-tonality",
    NAMES, STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT);

static bool valid_rule(StandardTonalityScaleDegreeRule which) {
    return which >= 0 && which < STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT;
}

const Rule *standard_tonality_scale_degree_rule(StandardTonalityScaleDegreeRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_tonality_scale_degree_constraint(StandardTonalityScaleDegreeRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

static const char *const SOLFEGE[STANDARD_TONALITY_SCALE_DEGREE_COUNT] = {"do", "re", "mi", "fa", "sol", "la", "ti"};

static const char *const DEGREE_NAMES[STANDARD_TONALITY_SCALE_DEGREE_COUNT] = {
    "tonic", "supertonic", "mediant", "subdominant", "dominant", "submediant", "leading tone"};

static bool valid_degree(int degree) {
    return degree >= 1 && degree <= STANDARD_TONALITY_SCALE_DEGREE_CLOSING_TONIC;
}

static int degree_index(int degree) {
    return degree == STANDARD_TONALITY_SCALE_DEGREE_CLOSING_TONIC ? 0 : degree - 1;
}

const char *standard_tonality_solfege(int degree) {
    if (!valid_degree(degree)) {
        return NULL;
    }
    return SOLFEGE[degree_index(degree)];
}

const char *standard_tonality_degree_name(int degree) {
    if (!valid_degree(degree)) {
        return NULL;
    }
    return DEGREE_NAMES[degree_index(degree)];
}

bool standard_tonality_degree_derivation(int degree, int *generic_interval, bool *below) {
    if (generic_interval == NULL || below == NULL) {
        return false;
    }
    switch (degree) {
        case 2:
            *generic_interval = 2;
            *below = false;
            return true;
        case 3:
            *generic_interval = 3;
            *below = false;
            return true;
        case 5:
            *generic_interval = 5;
            *below = false;
            return true;
        case 4:
            *generic_interval = 5;
            *below = true;
            return true;
        case 6:
            *generic_interval = 3;
            *below = true;
            return true;
        default:
            return false;
    }
}

bool standard_tonality_is_leading_tone(const PitchSpelling *tonic, const PitchSpelling *note) {
    if (tonic == NULL || note == NULL) {
        return false;
    }
    return standard_tonality_semitone_number(tonic) - standard_tonality_semitone_number(note) == 1;
}

int standard_tonality_scale_degree_of(const PitchSpelling *tonic, const PitchSpelling *note) {
    if (tonic == NULL || note == NULL) {
        return 0;
    }
    return standard_tonality_scale_degree_in_mode(pitch_spelling_letter(tonic), pitch_spelling_accidental(tonic),
                                                  STANDARD_TONALITY_MODE_MAJOR, pitch_spelling_letter(note),
                                                  pitch_spelling_accidental(note));
}

ScaleDegreeResult standard_tonality_build_scale_degree(int degree, EntityID *scale_target_id, PitchIdentity *pitch,
                                                       ScaleDegree **out) {
    const char *label = standard_tonality_degree_name(degree);
    if (label == NULL) {
        return SCALE_DEGREE_ERR_NULL_LABEL;
    }

    char interpretation_id_value[128];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "standard-tonality-scale-degree-%d", degree);

    return standard_tonality_build_labeled_scale_degree(label, interpretation_id_value, "major-scale-degree-vocabulary",
                                                        scale_target_id, pitch, out);
}

ScaleDegreeResult standard_tonality_build_labeled_scale_degree(const char *label, const char *interpretation_id_value,
                                                               const char *method, EntityID *scale_target_id,
                                                               PitchIdentity *pitch, ScaleDegree **out) {
    if (label == NULL || label[0] == '\0' || interpretation_id_value == NULL || method == NULL) {
        return SCALE_DEGREE_ERR_NULL_LABEL;
    }

    const Framework *framework = standard_tonality_framework();
    if (framework == NULL) {
        return SCALE_DEGREE_ERR_ALLOC;
    }

    EntityID *interpretation_id;
    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK) {
        return SCALE_DEGREE_ERR_ALLOC;
    }

    EntityID *framework_target_id;
    if (entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }
    FrameworkReference *framework_reference;
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        entity_id_destroy(framework_target_id);
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }

    Value *claim;
    if (value_create("standard-tonality/scale-degree-label", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }

    Interpretation *interpretation;
    InterpretationResult interpretation_result =
        interpretation_create(interpretation_id, scale_target_id, claim, NULL, NULL, framework_reference,
                               uncertainty, "standard-tonality-v1", method,
                               INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation);
    if (interpretation_result != INTERPRETATION_OK) {
        provenance_reference_destroy(provenance);
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        return SCALE_DEGREE_ERR_ALLOC;
    }

    ScaleDegreeResult scale_degree_result = scale_degree_create(interpretation, pitch, label, out);
    if (scale_degree_result != SCALE_DEGREE_OK) {
        interpretation_destroy(interpretation);
        return scale_degree_result;
    }

    return SCALE_DEGREE_OK;
}
