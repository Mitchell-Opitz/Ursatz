#include "counterpoint_line.h"

#include <stddef.h>

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
