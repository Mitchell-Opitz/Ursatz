#include "outer_scale_degrees.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "pitch/midi_pitch.h"

typedef struct {
    int degree;     /* 1..7 */
    int alteration; /* -1, 0 or 1 */
} DegreeOfOffset;

/* Indexed by semitones above the tonic. Major: 1 b2 2 b3 3 4 #4 5 b6 6 b7 7. Natural minor: 1 b2 2 3 #3 4 #4 5 6 #6 7 #7. */
static const DegreeOfOffset MAJOR_DEGREES[12] = {{1, 0}, {2, -1}, {2, 0}, {3, -1}, {3, 0}, {4, 0},
                                                 {4, 1}, {5, 0},  {6, -1}, {6, 0}, {7, -1}, {7, 0}};
static const DegreeOfOffset MINOR_DEGREES[12] = {{1, 0}, {2, -1}, {2, 0}, {3, 0}, {3, 1}, {4, 0},
                                                 {4, 1}, {5, 0},  {6, 0}, {6, 1}, {7, 0}, {7, 1}};

/* [degree - 1][alteration + 1]: flat, natural, sharp. */
static const char *const DEGREE_LABELS[7][3] = {{"b1", "1", "#1"}, {"b2", "2", "#2"}, {"b3", "3", "#3"},
                                                {"b4", "4", "#4"}, {"b5", "5", "#5"}, {"b6", "6", "#6"},
                                                {"b7", "7", "#7"}};

bool common_practice_scale_degree_of(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode, int *degree,
                                     int *alteration) {
    if (degree == NULL || alteration == NULL || pitch_class < 0 || pitch_class > 11 || tonic_pitch_class < 0 ||
        tonic_pitch_class > 11 || (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR)) {
        return false;
    }
    int offset = pitch_class_of_midi(pitch_class - tonic_pitch_class);
    const DegreeOfOffset *entry = mode == COMMON_PRACTICE_KEY_MAJOR ? &MAJOR_DEGREES[offset] : &MINOR_DEGREES[offset];
    *degree = entry->degree;
    *alteration = entry->alteration;
    return true;
}

const char *common_practice_scale_degree_label(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode) {
    int degree = 0;
    int alteration = 0;
    if (!common_practice_scale_degree_of(pitch_class, tonic_pitch_class, mode, &degree, &alteration)) {
        return NULL;
    }
    return DEGREE_LABELS[degree - 1][alteration + 1];
}

void common_practice_outer_degrees_destroy(CommonPracticeOuterDegreeList *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        scale_degree_destroy(list->items[i].soprano);
        scale_degree_destroy(list->items[i].bass);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* One ScaleDegree for a present entry. `line` names the line in the Interpretation's id. */
static CommonPracticeOuterStatus build_degree(const OuterVoiceEntry *entry, const char *line, const Key *key,
                                               int tonic_pitch_class, CommonPracticeKeyMode mode,
                                               ScaleDegree **out) {
    if (pitch_numeric_value_position_denominator(entry->pitch) != 1 ||
        pitch_numeric_value_position_numerator(entry->pitch) < 0) {
        return COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_PITCH;
    }
    int pitch_class = (int)(pitch_numeric_value_position_numerator(entry->pitch) % 12);
    const char *label = common_practice_scale_degree_label(pitch_class, tonic_pitch_class, mode);
    if (label == NULL) {
        return COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_MODE;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework == NULL) {
        return COMMON_PRACTICE_OUTER_ERR_ALLOC;
    }

    char id_value[256];
    snprintf(id_value, sizeof(id_value), "common-practice-minimal-scale-degree-%s-%s-%zu", line,
             entity_id_value(entry->note_id), entry->segment_index);
    EntityID *new_id = NULL;
    EntityID *target_id = NULL;
    EntityID *context_id = NULL;
    EntityID *framework_target_id = NULL;
    FrameworkReference *framework_reference = NULL;
    Value *claim = NULL;
    Uncertainty *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    PitchIdentity *pitch = NULL;
    Interpretation *interpretation = NULL;
    ScaleDegree *degree = NULL;
    CommonPracticeOuterStatus failure = COMMON_PRACTICE_OUTER_ERR_ALLOC;

    if (entity_id_create(id_value, &new_id) != ENTITY_ID_OK ||
        entity_id_create(entity_id_value(entry->note_id), &target_id) != ENTITY_ID_OK ||
        entity_id_create(entity_id_value(interpretation_id(key_interpretation(key))), &context_id) != ENTITY_ID_OK ||
        entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK ||
        framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        goto cleanup;
    }
    framework_target_id = NULL; /* the reference owns it */
    if (value_create("common-practice-minimal/scale-degree", label, strlen(label) + 1, &claim) != VALUE_OK ||
        uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK ||
        pitch_numeric_value_to_identity(entry->pitch, &pitch) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
        goto cleanup;
    }
    if (interpretation_create(new_id, target_id, claim, context_id, NULL, framework_reference,
                              uncertainty, "common-practice-minimal-v1", "outer-voice-scale-degree",
                              INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
        INTERPRETATION_OK) {
        goto cleanup;
    }
    /* interpretation_create took the ids, claim, reference, uncertainty and provenance. */
    new_id = target_id = context_id = NULL;
    framework_reference = NULL;
    claim = NULL;
    uncertainty = NULL;
    provenance = NULL;

    if (scale_degree_create(interpretation, pitch, label, &degree) != SCALE_DEGREE_OK) {
        interpretation_destroy(interpretation);
        interpretation = NULL;
        goto cleanup;
    }
    pitch = NULL; /* the ScaleDegree owns the interpretation and the pitch now */
    *out = degree;
    return COMMON_PRACTICE_OUTER_OK;

cleanup:
    entity_id_destroy(new_id);
    entity_id_destroy(target_id);
    entity_id_destroy(context_id);
    entity_id_destroy(framework_target_id);
    framework_reference_destroy(framework_reference);
    value_destroy(claim);
    uncertainty_destroy(uncertainty);
    provenance_reference_destroy(provenance);
    pitch_identity_destroy(pitch);
    return failure;
}

CommonPracticeOuterStatus common_practice_outer_scale_degrees(const OuterVoiceLines *lines, const Key *key,
                                                               int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                               CommonPracticeOuterDegreeList *out) {
    if (lines == NULL || key == NULL || out == NULL || (lines->count > 0 && (lines->soprano == NULL || lines->bass == NULL))) {
        return COMMON_PRACTICE_OUTER_ERR_NULL_ARGUMENT;
    }
    if (tonic_pitch_class < 0 || tonic_pitch_class > 11) {
        return COMMON_PRACTICE_OUTER_ERR_BAD_TONIC;
    }
    if (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR) {
        return COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_MODE;
    }

    CommonPracticeOuterDegreeList list = {NULL, 0};
    if (lines->count > 0) {
        list.items = (CommonPracticeOuterDegrees *)calloc(lines->count, sizeof(CommonPracticeOuterDegrees));
        if (list.items == NULL) {
            return COMMON_PRACTICE_OUTER_ERR_ALLOC;
        }
    }
    list.count = lines->count;

    CommonPracticeOuterStatus status = COMMON_PRACTICE_OUTER_OK;
    for (size_t i = 0; i < lines->count && status == COMMON_PRACTICE_OUTER_OK; i++) {
        if (lines->soprano[i].present) {
            status = build_degree(&lines->soprano[i], "soprano", key, tonic_pitch_class, mode, &list.items[i].soprano);
        }
        if (status == COMMON_PRACTICE_OUTER_OK && lines->bass[i].present) {
            status = build_degree(&lines->bass[i], "bass", key, tonic_pitch_class, mode, &list.items[i].bass);
        }
    }
    if (status != COMMON_PRACTICE_OUTER_OK) {
        common_practice_outer_degrees_destroy(&list);
        return status;
    }
    *out = list;
    return COMMON_PRACTICE_OUTER_OK;
}
