#include "analysis_function.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "core/value.h"
#include "harmony_applied.h"
#include "harmony_functions.h"
#include "identity/entity_id.h"

static bool function_of(const HarmonySegment *segment, HarmonyFunctionResult *result) {
    const HarmonySegmentInput *input = segment->input;
    HarmonicFunction function;
    if (segment->applied) {
        if (!harmony_applied_function(input->mode, &segment->applied_chord, &function)) {
            return false;
        }
        result->applied = true;
        result->applied_to = segment->applied_chord.target;
    } else if (!harmony_function_of(input->mode, &segment->chord, &function)) {
        return false;
    }
    result->function = function;
    return true;
}

HarmonyFunctionStatus harmony_analysis_assign_functions(const HarmonySegment *segments, size_t count,
                                                        HarmonyFunctionResult **out) {
    if (out == NULL || (segments == NULL && count > 0)) {
        return HARMONY_FUNCTION_ERR_NULL_ARGUMENT;
    }
    HarmonyFunctionResult *results = (HarmonyFunctionResult *)calloc(count > 0 ? count : 1, sizeof(*results));
    if (results == NULL) {
        return HARMONY_FUNCTION_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        HarmonyFunctionResult *result = &results[i];
        const HarmonySegmentInput *input = segments[i].input;
        result->segment_id = input->segment_id;
        result->uncertainty = input->uncertainty;
        if (!segments[i].valid || !function_of(&segments[i], result)) {
            continue;
        }
        HarmonyFunctionClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.function = (int)result->function;
        claim.applied = result->applied;
        claim.applied_to = result->applied_to;
        claim.applied_kind = result->applied ? (int)segments[i].applied_chord.kind : 0;

        char id[256];
        snprintf(id, sizeof(id), "standard-harmony-function-%s", input->segment_id);
        result->interpretation = harmony_analysis_make_interpretation(id, input->segment_id,
                                                                      HARMONY_FUNCTION_CLAIM_TYPE, &claim,
                                                                      sizeof(claim), input->uncertainty,
                                                                      "harmonic-function-adapter");
        if (result->interpretation == NULL) {
            harmony_function_results_destroy(results, count);
            return HARMONY_FUNCTION_ERR_ALLOC;
        }
        result->has_function = true;
    }
    *out = results;
    return HARMONY_FUNCTION_OK;
}

void harmony_function_results_destroy(HarmonyFunctionResult *results, size_t count) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(results[i].interpretation);
    }
    free(results);
}

bool harmony_function_claim_read(const Interpretation *interpretation, HarmonyFunctionClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), HARMONY_FUNCTION_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
