#include "key_identification.h"

#include <stdbool.h>
#include <stddef.h>

#define KEY_PITCH_CLASS_COUNT 7

/* Tonic-relative pitch-class collections (semitones 0-11) for the two
   modes this framework knows, compared as an unordered set -- see
   header for the tie-break rule. */
static const int MAJOR_PITCH_CLASSES[KEY_PITCH_CLASS_COUNT] = {0, 2, 4, 5, 7, 9, 11};
static const int NATURAL_MINOR_PITCH_CLASSES[KEY_PITCH_CLASS_COUNT] = {0, 2, 3, 5, 7, 8, 10};

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

static bool matches_collection(const int *distinct, size_t distinct_count, const int *collection) {
    for (int i = 0; i < KEY_PITCH_CLASS_COUNT; i++) {
        if (!contains_pitch_class(distinct, distinct_count, collection[i])) {
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

    if (matches_collection(distinct, distinct_count, MAJOR_PITCH_CLASSES)) {
        *out = COMMON_PRACTICE_KEY_MAJOR;
        return true;
    }
    if (matches_collection(distinct, distinct_count, NATURAL_MINOR_PITCH_CLASSES)) {
        *out = COMMON_PRACTICE_KEY_MINOR;
        return true;
    }

    return false;
}
