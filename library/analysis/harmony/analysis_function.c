#include "analysis/harmony/analysis_function.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/harmony/harmony_applied.h"
#include "theory/harmony/harmony_functions.h"

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
        result->segment_id = input->segment.id;
        result->uncertainty = input->uncertainty;
        if (!segments[i].valid || !function_of(&segments[i], result)) {
            continue;
        }
        result->has_function = true;
    }
    *out = results;
    return HARMONY_FUNCTION_OK;
}

void harmony_function_results_destroy(HarmonyFunctionResult *results, size_t count) {
    (void)count;
    free(results);
}

