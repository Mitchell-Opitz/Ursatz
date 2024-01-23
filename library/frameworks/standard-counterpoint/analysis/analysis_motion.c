#include "analysis_motion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "motion_rules.h"

static double least_confidence(const StandardCounterpointVoiceCell *const cells[4]) {
    double least = 1.0;
    for (int i = 0; i < 4; i++) {
        if (cells[i]->confidence < least) {
            least = cells[i]->confidence;
        }
    }
    return least;
}

static void destroy_motion(StandardCounterpointMotion *motion) {
    interpretation_destroy(motion->interpretation);
    structure_destroy(motion->shell);
    memset(motion, 0, sizeof(*motion));
}

void standard_counterpoint_motions_destroy(StandardCounterpointMotions *motions) {
    if (motions == NULL) {
        return;
    }
    for (size_t i = 0; i < motions->count; i++) {
        destroy_motion(&motions->items[i]);
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

                char key[256];
                snprintf(key, sizeof(key), "%s-%s-v%zu-v%zu", grid->segment_ids[s], grid->segment_ids[s + 1], upper,
                         lower);
                char shell_id[300];
                snprintf(shell_id, sizeof(shell_id), "standard-counterpoint-motion-notes-%s", key);
                const char *note_ids[4] = {cells[0]->note_id, cells[1]->note_id, cells[2]->note_id,
                                           cells[3]->note_id};
                motion->shell = standard_counterpoint_analysis_note_group(shell_id, note_ids, 4);

                StandardCounterpointMotionClaim claim;
                memset(&claim, 0, sizeof(claim));
                claim.motion_type = (int)type;
                claim.upper_voice = (int)upper;
                claim.lower_voice = (int)lower;
                claim.segment_from = (int)s;
                claim.segment_to = (int)(s + 1);
                char interpretation_id[320];
                snprintf(interpretation_id, sizeof(interpretation_id), "standard-counterpoint-motion-%s", key);
                if (motion->shell != NULL) {
                    motion->interpretation = standard_counterpoint_analysis_interpretation(
                        interpretation_id, entity_id_value(entity_id(structure_entity(motion->shell))),
                        STANDARD_COUNTERPOINT_MOTION_CLAIM_TYPE, &claim, sizeof(claim), motion->uncertainty,
                        "motion-adapter");
                }
                if (motion->shell == NULL || motion->interpretation == NULL) {
                    destroy_motion(motion);
                    standard_counterpoint_motions_destroy(&found);
                    return STANDARD_COUNTERPOINT_MOTION_ERR_ALLOC;
                }
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

bool standard_counterpoint_motion_claim_read(const Interpretation *interpretation,
                                             StandardCounterpointMotionClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_COUNTERPOINT_MOTION_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
