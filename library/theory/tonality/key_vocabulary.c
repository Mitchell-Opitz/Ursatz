#include "theory/tonality/key_vocabulary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/framework_reference.h"
#include "model/core/provenance_reference.h"
#include "model/core/value.h"
#include "theory/tonality/common_practice_framework.h"
#include "model/interpretation/uncertainty.h"
#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/common_practice_materials.h"

static const char *mode_suffix(CommonPracticeKeyMode mode) {
    switch (mode) {
        case COMMON_PRACTICE_KEY_MAJOR:
            return "major";
        case COMMON_PRACTICE_KEY_MINOR:
            return "minor";
        default:
            return NULL;
    }
}

/* The pitch class of a tonic name: a letter A-G then any number of '#' or 'b' accidentals. -1 if it is not
   one, in which case the Key is built without a structure. The accidental loop stays here on purpose (R-582): a name
   with three or more accidentals is read as written, which no owner function does. */
static int tonic_pitch_class_of(const char *name) {
    PitchLetter letter;
    if (!pitch_letter_from_char(name[0], &letter)) {
        return -1;
    }
    int pitch_class = pitch_letter_semitone(letter);
    for (const char *c = name + 1; *c != '\0'; c++) {
        if (*c == '#') {
            pitch_class++;
        } else if (*c == 'b') {
            pitch_class--;
        } else {
            return -1;
        }
    }
    return pitch_class_of_midi(pitch_class);
}

KeyResult common_practice_build_key(const PitchMaterial *material, const char *tonic_name, EntityID *context_target_id,
                                     Key **out) {
    if (tonic_name == NULL || tonic_name[0] == '\0') {
        return KEY_ERR_NULL_LABEL;
    }
    CommonPracticeKeyMode mode;
    if (!common_practice_key_mode_of(material, &mode)) {
        return KEY_ERR_BAD_STRUCTURE;
    }
    const char *suffix = mode_suffix(mode);
    if (suffix == NULL) {
        return KEY_ERR_NULL_LABEL;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return KEY_ERR_ALLOC;
    }

    size_t label_len = strlen(tonic_name) + 1 + strlen(suffix);
    char *label = (char *)malloc(label_len + 1);
    if (label == NULL) {
        return KEY_ERR_ALLOC;
    }
    strcpy(label, tonic_name);
    strcat(label, " ");
    strcat(label, suffix);

    char interpretation_id_value[128];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "common-practice-minimal-key-%s", label);

    EntityID *interpretation_id;
    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK) {
        free(label);
        return KEY_ERR_ALLOC;
    }

    EntityID *framework_target_id;
    if (entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }
    FrameworkReference *framework_reference;
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        entity_id_destroy(framework_target_id);
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }

    Value *claim;
    if (value_create("common-practice-minimal/key-label", label, strlen(label) + 1, &claim) != VALUE_OK) {
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }

    Interpretation *interpretation;
    InterpretationResult interpretation_result =
        interpretation_create(interpretation_id, context_target_id, claim, NULL, NULL, framework_reference,
                               uncertainty, "common-practice-minimal-v1", "major-minor-key-vocabulary",
                               INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation);
    if (interpretation_result != INTERPRETATION_OK) {
        provenance_reference_destroy(provenance);
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        framework_reference_destroy(framework_reference);
        entity_id_destroy(interpretation_id);
        free(label);
        return KEY_ERR_ALLOC;
    }

    int tonic_pitch_class = tonic_pitch_class_of(tonic_name);
    KeyResult key_result;
    if (tonic_pitch_class >= 0) {
        KeyStructure structure = {tonic_pitch_class, material};
        key_result = key_create_with_structure(interpretation, label, &structure, out);
    } else {
        key_result = key_create(interpretation, label, out);
    }
    free(label);
    if (key_result != KEY_OK) {
        interpretation_destroy(interpretation);
        return key_result;
    }

    return KEY_OK;
}