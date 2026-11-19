#include "analysis/counterpoint/analysis_motion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/counterpoint/motion_rules.h"

static double least_confidence(const StandardCounterpointVoiceCell *const cells[4]) {
    double least = 1.0;
    for (int i = 0; i < 4; i++) {
        if (cells[i]->confidence < least) {
            least = cells[i]->confidence;
        }
    }
    return least;
}

void standard_counterpoint_motions_destroy(StandardCounterpointMotions *motions) {
    if (motions == NULL) {
        return;
    }
    free(motions->items);
    motions->items = NULL;
    motions->count = 0;
}

StandardCounterpointMotionStatus standard_counterpoint_classify_motions(const StandardCounterpointPreparedGrid *prepared,
                                                                        StandardCounterpointMotions *out) {
    if (prepared == NULL || prepared->grid == NULL || out == NULL) {
        return STANDARD_COUNTERPOINT_MOTION_ERR_NULL_ARGUMENT;
    }
    const StandardCounterpointVoiceGrid *grid = prepared->grid;
    size_t capacity = grid->segment_count > 1
                          ? (grid->segment_count - 1) * (grid->voice_count * (grid->voice_count - 1) / 2)
                          : 0;
    StandardCounterpointMotions found = {NULL, 0};
    if (capacity > 0) {
        found.items = (StandardCounterpointMotion *)calloc(capacity, sizeof(StandardCounterpointMotion));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MOTION_ERR_ALLOC;
        }
    }
    for (size_t s = 0; s + 1 < grid->segment_count; s++) {
        for (size_t upper = 0; upper < grid->voice_count; upper++) {
            for (size_t lower = upper + 1; lower < grid->voice_count; lower++) {
                const StandardCounterpointVoiceCell *cells[4] = {
                    standard_counterpoint_grid_cell(prepared, s, upper),
                    standard_counterpoint_grid_cell(prepared, s + 1, upper),
                    standard_counterpoint_grid_cell(prepared, s, lower),
                    standard_counterpoint_grid_cell(prepared, s + 1, lower)};
                if (cells[0] == NULL || cells[1] == NULL || cells[2] == NULL || cells[3] == NULL) {
                    continue;
                }
                MotionType type;
                if (!standard_counterpoint_classify_motion(
                        standard_counterpoint_grid_spelling(prepared, s, lower),
                        standard_counterpoint_grid_spelling(prepared, s + 1, lower),
                        standard_counterpoint_grid_spelling(prepared, s, upper),
                        standard_counterpoint_grid_spelling(prepared, s + 1, upper), &type)) {
                    continue;
                }
                StandardCounterpointMotion *motion = &found.items[found.count];
                motion->segment_from = s;
                motion->segment_to = s + 1;
                motion->upper_voice = upper;
                motion->lower_voice = lower;
                motion->type = type;
                motion->uncertainty = 1.0 - least_confidence(cells);
                found.count++;
            }
        }
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_MOTION_OK;
}

