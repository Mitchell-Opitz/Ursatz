#include "analysis/motif_window_matching.h"

#include <stdbool.h>
#include <stdlib.h>

static bool windows_match(const MotifWindow *a, const MotifWindow *b) {
    size_t direction_count = contour_direction_count(a->contour);
    if (direction_count != contour_direction_count(b->contour)) {
        return false;
    }
    for (size_t i = 0; i < direction_count; i++) {
        if (contour_direction_at(a->contour, i) != contour_direction_at(b->contour, i)) {
            return false;
        }
    }

    size_t duration_count = rhythmic_pattern_duration_count(a->rhythm);
    if (duration_count != rhythmic_pattern_duration_count(b->rhythm)) {
        return false;
    }
    for (size_t i = 0; i < duration_count; i++) {
        if (!duration_equals(rhythmic_pattern_duration_at(a->rhythm, i), rhythmic_pattern_duration_at(b->rhythm, i))) {
            return false;
        }
    }

    return true;
}

MotifWindowMatchingResult motif_window_matching_compare(const MotifWindow *a, size_t a_offset, const MotifWindow *b,
                                                          size_t b_offset, size_t length,
                                                          MotifWindowSimilarity *out) {
    if (a == NULL || b == NULL || out == NULL || a->contour == NULL || a->rhythm == NULL || b->contour == NULL ||
        b->rhythm == NULL || length < 2) {
        return MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOW;
    }
    if (a_offset + length > rhythmic_pattern_duration_count(a->rhythm) ||
        b_offset + length > rhythmic_pattern_duration_count(b->rhythm) ||
        a_offset + length - 1 > contour_direction_count(a->contour) ||
        b_offset + length - 1 > contour_direction_count(b->contour)) {
        return MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOW;
    }

    size_t equal_directions = 0;
    for (size_t i = 0; i + 1 < length; i++) {
        if (contour_direction_at(a->contour, a_offset + i) == contour_direction_at(b->contour, b_offset + i)) {
            equal_directions++;
        }
    }
    size_t equal_durations = 0;
    for (size_t i = 0; i < length; i++) {
        if (duration_equals(rhythmic_pattern_duration_at(a->rhythm, a_offset + i),
                             rhythmic_pattern_duration_at(b->rhythm, b_offset + i))) {
            equal_durations++;
        }
    }
    out->contour_score = (double)equal_directions / (double)(length - 1);
    out->rhythm_score = (double)equal_durations / (double)length;
    return MOTIF_WINDOW_MATCHING_OK;
}

MotifWindowMatchingResult motif_window_matching_find(const MotifWindow *windows, size_t window_count,
                                                       MotifWindowOccurrence **out_occurrences,
                                                       size_t *out_occurrence_count) {
    if (window_count > 0 && windows == NULL) {
        return MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOWS;
    }
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].contour == NULL || windows[i].rhythm == NULL || windows[i].event_ids == NULL ||
            windows[i].event_id_count == 0) {
            return MOTIF_WINDOW_MATCHING_ERR_NULL_WINDOW;
        }
    }

    if (window_count == 0) {
        *out_occurrences = NULL;
        *out_occurrence_count = 0;
        return MOTIF_WINDOW_MATCHING_OK;
    }

    bool *has_match = (bool *)calloc(window_count, sizeof(bool));
    if (has_match == NULL) {
        return MOTIF_WINDOW_MATCHING_ERR_ALLOC;
    }
    for (size_t i = 0; i < window_count; i++) {
        for (size_t j = 0; j < window_count; j++) {
            if (i == j || has_match[i]) {
                continue;
            }
            if (windows_match(&windows[i], &windows[j])) {
                has_match[i] = true;
            }
        }
    }

    size_t matched_count = 0;
    for (size_t i = 0; i < window_count; i++) {
        if (has_match[i]) {
            matched_count++;
        }
    }

    if (matched_count == 0) {
        free(has_match);
        *out_occurrences = NULL;
        *out_occurrence_count = 0;
        return MOTIF_WINDOW_MATCHING_OK;
    }

    MotifWindowOccurrence *occurrences =
        (MotifWindowOccurrence *)calloc(matched_count, sizeof(MotifWindowOccurrence));
    if (occurrences == NULL) {
        free(has_match);
        return MOTIF_WINDOW_MATCHING_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i < window_count; i++) {
        if (!has_match[i]) {
            continue;
        }
        occurrences[out_index].matched_event_ids = windows[i].event_ids;
        occurrences[out_index].matched_event_id_count = windows[i].event_id_count;
        occurrences[out_index].confidence_numerator = 1;
        occurrences[out_index].confidence_denominator = 1;
        out_index++;
    }

    free(has_match);
    *out_occurrences = occurrences;
    *out_occurrence_count = matched_count;
    return MOTIF_WINDOW_MATCHING_OK;
}

void motif_window_matching_destroy_occurrences(MotifWindowOccurrence *occurrences) { free(occurrences); }
