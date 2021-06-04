#include "counterpoint_line.h"

#include <stddef.h>

#include "counterpoint_pitch.h"

bool standard_counterpoint_line_valid(const StandardCounterpointLine *line) {
    if (line == NULL || line->notes == NULL || line->count == 0) {
        return false;
    }
    for (size_t i = 0; i < line->count; i++) {
        if (line->notes[i] == NULL) {
            return false;
        }
    }
    return true;
}

bool standard_counterpoint_pair_valid(const StandardCounterpointPair *pair) {
    return pair != NULL && standard_counterpoint_line_valid(pair->upper) &&
           standard_counterpoint_line_valid(pair->lower) && pair->upper->count == pair->lower->count;
}

bool standard_counterpoint_line_interval_at(const StandardCounterpointLine *line, size_t index,
                                            StandardCounterpointInterval *out) {
    if (!standard_counterpoint_line_valid(line) || index + 1 >= line->count) {
        return false;
    }
    return standard_counterpoint_interval_between(line->notes[index], line->notes[index + 1], out);
}

int standard_counterpoint_line_direction_at(const StandardCounterpointLine *line, size_t index) {
    if (!standard_counterpoint_line_valid(line) || index + 1 >= line->count) {
        return 0;
    }
    return standard_counterpoint_direction(line->notes[index], line->notes[index + 1]);
}

size_t standard_counterpoint_line_highest_index(const StandardCounterpointLine *line) {
    if (!standard_counterpoint_line_valid(line)) {
        return 0;
    }
    size_t best = 0;
    for (size_t i = 1; i < line->count; i++) {
        if (standard_counterpoint_direction(line->notes[best], line->notes[i]) > 0) {
            best = i;
        }
    }
    return best;
}

size_t standard_counterpoint_line_lowest_index(const StandardCounterpointLine *line) {
    if (!standard_counterpoint_line_valid(line)) {
        return 0;
    }
    size_t best = 0;
    for (size_t i = 1; i < line->count; i++) {
        if (standard_counterpoint_direction(line->notes[best], line->notes[i]) < 0) {
            best = i;
        }
    }
    return best;
}
