#include "segmentation_rules.h"

bool standard_form_idea_length_valid(size_t measures) {
    return measures >= 1;
}

bool standard_form_ideas_fill_phrase(const size_t *idea_measures, size_t idea_count, size_t phrase_measures) {
    if (idea_measures == NULL || idea_count == 0) {
        return false;
    }
    size_t total = 0;
    for (size_t i = 0; i < idea_count; i++) {
        if (!standard_form_idea_length_valid(idea_measures[i])) {
            return false;
        }
        total += idea_measures[i];
    }
    return total == phrase_measures;
}
