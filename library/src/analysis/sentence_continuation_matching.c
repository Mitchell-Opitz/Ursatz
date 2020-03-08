#include "analysis/sentence_continuation_matching.h"

#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"

static bool valid_window(const MotifWindow *window) {
    return window != NULL && window->contour != NULL && window->rhythm != NULL && window->event_ids != NULL &&
           window->event_id_count > 0;
}

bool sentence_continuation_matching_is_continuation(const MotifWindow *statement, const MotifWindow *candidate) {
    if (!valid_window(statement) || !valid_window(candidate)) {
        return false;
    }
    if (candidate->event_id_count < SENTENCE_CONTINUATION_MIN_EVENT_COUNT) {
        return false;
    }
    if (candidate->event_id_count >= statement->event_id_count) {
        return false;
    }

    size_t direction_count = contour_direction_count(candidate->contour);
    if (direction_count == 0 || direction_count > contour_direction_count(statement->contour)) {
        return false;
    }
    for (size_t i = 0; i < direction_count; i++) {
        if (contour_direction_at(candidate->contour, i) != contour_direction_at(statement->contour, i)) {
            return false;
        }
    }

    size_t duration_count = rhythmic_pattern_duration_count(candidate->rhythm);
    if (duration_count == 0 || duration_count > rhythmic_pattern_duration_count(statement->rhythm)) {
        return false;
    }
    for (size_t i = 0; i < duration_count; i++) {
        if (!duration_equals(rhythmic_pattern_duration_at(candidate->rhythm, i),
                              rhythmic_pattern_duration_at(statement->rhythm, i))) {
            return false;
        }
    }

    return true;
}
