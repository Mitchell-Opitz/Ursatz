#include "analysis/harmony/key_identification.h"

#include <stdbool.h>
#include <stddef.h>

#include "theory/tonality/tonal_key.h"
#include "theory/tonality/common_practice_materials.h"

static bool contains_pitch_class(const int *pitch_classes, size_t count, int pitch_class) {
    for (size_t i = 0; i < count; i++) {
        if (pitch_classes[i] == pitch_class) {
            return true;
        }
    }
    return false;
}

static size_t count_distinct(const int *pitch_classes, size_t pitch_class_count, int *out_distinct,
                              size_t out_capacity) {
    size_t distinct_count = 0;
    for (size_t i = 0; i < pitch_class_count; i++) {
        int pitch_class = pitch_classes[i];
        if (pitch_class < 0 || pitch_class > 11) {
            continue;
        }
        if (!contains_pitch_class(out_distinct, distinct_count, pitch_class) && distinct_count < out_capacity) {
            out_distinct[distinct_count++] = pitch_class;
        }
    }
    return distinct_count;
}

/* True when every pitch class of the mode's scale on tonic 0 is among the distinct ones, compared as an unordered
   set -- see header for the tie-break rule. */
static bool matches_collection(const int *distinct, size_t distinct_count, const PitchMaterial *material) {
    unsigned scale = standard_tonality_material_pitch_classes(0, material);
    for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
        if ((scale & (1u << pitch_class)) != 0 && !contains_pitch_class(distinct, distinct_count, pitch_class)) {
            return false;
        }
    }
    return true;
}

bool common_practice_identify_key_mode(const int *tonic_relative_pitch_classes, size_t pitch_class_count,
                                        CommonPracticeKeyMode *out) {
    if (tonic_relative_pitch_classes == NULL || out == NULL) {
        return false;
    }

    int distinct[12];
    size_t distinct_count = count_distinct(tonic_relative_pitch_classes, pitch_class_count, distinct, 12);

    /* The built-in materials: a remaining site until P58 B13 (the Composer's motive import is the one caller and holds no snapshot). */
    const CommonPracticeMaterials *materials = common_practice_materials_builtin();
    if (matches_collection(distinct, distinct_count, materials->major)) {
        *out = COMMON_PRACTICE_KEY_MAJOR;
        return true;
    }
    if (matches_collection(distinct, distinct_count, materials->natural_minor)) {
        *out = COMMON_PRACTICE_KEY_MINOR;
        return true;
    }

    return false;
}
