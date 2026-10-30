#include "pipeline/harmony_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "standard-harmony/harmonic_function.h"
#include "standard-harmony/harmony_functions.h"
#include "standard-harmony/phrase_model_label.h"
#include "standard-harmony/analysis/analysis_phrase.h"
#include "standard-harmony/six_four_type.h"

bool harmony_stage_prepare(const MeterMap *map, const AnalysisChain *library, HarmonyStage *out) {
    *out = (HarmonyStage){0};
    size_t count = analysis_chain_segment_count(library);
    const ChainSegments *segments = analysis_chain_segments(library);
    const HarmonyCadences *cadences = analysis_chain_cadences(library);
    if (count == 0 || !segments || !cadences || !analysis_chain_chord_inputs(library) ||
        !analysis_chain_numerals(library) || !analysis_chain_outer_voices(library)) {
        return true;
    }
    ChainInputStatus built =
        chain_build_harmony_segments(segments, count, map, analysis_chain_chord_inputs(library),
                                     analysis_chain_numerals(library), analysis_chain_outer_voices(library),
                                     &out->inputs);
    if (built == CHAIN_INPUT_ERR_ALLOC) {
        return false;
    }
    if (built != CHAIN_INPUT_OK) {
        return true; /* a segment off the meter: the library cannot prepare the piece, so it has no functions */
    }
    out->count = count;
    out->cadences = *cadences;
    /* A half cadence with an alternative reading is the boundary step's result (R-038, C9): the dominant before a
       section start. */
    out->boundary = calloc(cadences->count > 0 ? cadences->count : 1, sizeof(bool));
    if (!out->boundary) {
        return false;
    }
    for (size_t i = 0; i < cadences->count; i++) {
        out->boundary[i] = cadences->items[i].alternative.present && cadences->items[i].type == CADENCE_TYPE_HALF;
    }
    HarmonyAnalysisStatus status = harmony_analysis_prepare(out->inputs.inputs, count, &out->segments);
    if (status == HARMONY_ANALYSIS_ERR_ALLOC) {
        return false;
    }
    if (status != HARMONY_ANALYSIS_OK) {
        out->segments = NULL; /* the library declined the inputs: no functions */
    }
    return true;
}

bool harmony_stage_collect_cadences(const HarmonyStage *stage, AnalysisReport *report) {
    size_t count = stage->cadences.count;
    if (count == 0) {
        return true;
    }
    report->cadences = calloc(count, sizeof(AnalysisCadence));
    if (!report->cadences) {
        return false;
    }
    report->cadence_count = count;
    for (size_t i = 0; i < count; i++) {
        const HarmonyCadenceResult *found = &stage->cadences.items[i];
        const AnalysisReportEntry *arrival = &report->entries[found->arrival_first];
        AnalysisCadence *cadence = &report->cadences[i];
        const char *type = cadence_type_name(found->type);
        cadence->type = strdup(type ? type : "unknown");
        cadence->soprano_degree = arrival->soprano.degree ? strdup(arrival->soprano.degree) : NULL;
        cadence->arrival_first = found->arrival_first;
        cadence->arrival_last = found->arrival_last;
        cadence->dominant_first = found->dominant_first;
        cadence->dominant_last = found->dominant_last;
        cadence->arrival_beat_strength = beat_strength_name(found->arrival_beat_strength);
        cadence->boundary = stage->boundary && stage->boundary[i];
        cadence->in_region = arrival->in_region;
        cadence->region = arrival->region;
        cadence->uncertainty = found->uncertainty;
        cadence->share = found->share;
        if (found->alternative.present) {
            const HarmonyCadenceAlternative *alternative = &found->alternative;
            bool is_cadence = alternative->reading == HARMONY_CADENCE_READING_CADENCE;
            const char *alternative_type = is_cadence ? cadence_type_name(alternative->type) : NULL;
            cadence->has_alternative = true;
            cadence->alternative.reading = is_cadence ? "cadence" : "prolongation";
            cadence->alternative.type = is_cadence ? strdup(alternative_type ? alternative_type : "unknown") : NULL;
            cadence->alternative.arrival_first = alternative->arrival_first;
            cadence->alternative.arrival_last = alternative->arrival_last;
            cadence->alternative.share = alternative->share;
            if (is_cadence && !cadence->alternative.type) {
                return false;
            }
        }
        if (!cadence->type || (arrival->soprano.degree && !cadence->soprano_degree)) {
            return false;
        }
    }
    return true;
}

bool harmony_stage_collect_functions(HarmonyStage *stage, AnalysisReport *report) {
    if (!stage->segments || stage->count == 0) {
        return true;
    }
    harmony_analysis_mark_cadential(stage->segments, stage->count);
    HarmonyFunctionResult *results = NULL;
    if (harmony_analysis_assign_functions(stage->segments, stage->count, &results) != HARMONY_FUNCTION_OK) {
        return false;
    }
    for (size_t i = 0; i < stage->count && i < report->entry_count; i++) {
        AnalysisReportEntry *entry = &report->entries[i];
        if (!results[i].has_function) {
            continue;
        }
        entry->function = harmonic_function_name(results[i].function);
        entry->function_applied = results[i].applied;
        entry->function_applied_to = results[i].applied ? results[i].applied_to : 0;
        entry->function_uncertainty = results[i].uncertainty;
        HarmonyPredominantStrength strength;
        const HarmonySegment *segment = &stage->segments[i];
        if (results[i].function == HARMONIC_FUNCTION_PREDOMINANT && segment->valid && !segment->applied &&
            harmony_predominant_strength(segment->input->mode, &segment->chord, &strength)) {
            entry->predominant_strength = strength == HARMONY_PREDOMINANT_STRONG ? "strong" : "weak";
        }
    }
    harmony_function_results_destroy(results, stage->count);
    return true;
}

bool harmony_stage_collect_six_fours_and_prolongations(HarmonyStage *stage, AnalysisReport *report) {
    if (!stage->segments || stage->count == 0) {
        return true;
    }
    HarmonySixFourResult *six_fours = NULL;
    if (harmony_analysis_classify_six_fours(stage->segments, stage->count, &six_fours) != HARMONY_SIX_FOUR_OK) {
        return false;
    }
    for (size_t i = 0; i < stage->count && i < report->entry_count; i++) {
        if (six_fours[i].classified) {
            report->entries[i].six_four = six_four_type_name(six_fours[i].type);
        }
    }
    harmony_six_four_results_destroy(six_fours, stage->count);

    HarmonyProlongations found = {0};
    if (harmony_analysis_find_prolongations(stage->segments, stage->count, &found) != HARMONY_PROLONGATION_OK) {
        return false;
    }
    bool ok = true;
    if (found.count > 0) {
        report->prolongations = calloc(found.count, sizeof(AnalysisProlongation));
        ok = report->prolongations != NULL;
    }
    for (size_t i = 0; ok && i < found.count; i++) {
        const HarmonyProlongationSpan *span = &found.spans[i];
        AnalysisProlongation *prolongation = &report->prolongations[i];
        report->prolongation_count = i + 1;
        prolongation->function = harmonic_function_name(span->function);
        prolongation->first_entry = span->first_segment;
        prolongation->last_entry = span->closing_segment;
        prolongation->interior_count = span->interior_count;
        prolongation->reasons = calloc(span->interior_count > 0 ? span->interior_count : 1, sizeof(const char *));
        ok = prolongation->reasons != NULL;
        for (size_t r = 0; ok && r < span->interior_count; r++) {
            prolongation->reasons[r] = harmony_prolongation_reason_name(span->reasons[r]);
        }
        const AnalysisReportEntry *first = &report->entries[span->first_segment];
        prolongation->in_region = first->in_region;
        prolongation->region = first->region;
        for (size_t e = span->first_segment; e <= span->closing_segment && e < report->entry_count; e++) {
            if (report->entries[e].function && report->entries[e].function_uncertainty > prolongation->uncertainty) {
                prolongation->uncertainty = report->entries[e].function_uncertainty;
            }
        }
    }
    harmony_prolongations_destroy(&found);
    return ok;
}

bool harmony_stage_collect_phrase_model(const HarmonyStage *stage, AnalysisReport *report) {
    if (!stage->segments || stage->count == 0 || report->phrase_count == 0) {
        return true;
    }
    HarmonyPhraseSpan *spans = calloc(report->phrase_count, sizeof(HarmonyPhraseSpan));
    size_t *owners = calloc(report->phrase_count, sizeof(size_t));
    if (!spans || !owners) {
        free(spans);
        free(owners);
        return false;
    }
    size_t span_count = 0;
    for (size_t p = 0; p < report->phrase_count; p++) {
        const AnalysisPhrase *phrase = &report->phrases[p];
        if (phrase->entry_count == 0 || phrase->first_entry + phrase->entry_count > stage->count) {
            continue;
        }
        HarmonyPhraseSpan *span = &spans[span_count];
        span->first_segment = phrase->first_entry;
        span->last_segment = phrase->first_entry + phrase->entry_count - 1;
        span->cadence = CADENCE_TYPE_UNKNOWN;
        if (phrase->has_cadence && phrase->cadence < stage->cadences.count) {
            const HarmonyCadenceResult *cadence = &stage->cadences.items[phrase->cadence];
            span->cadence = cadence->type;
            if (cadence->arrival_last >= span->first_segment && cadence->arrival_last < span->last_segment) {
                span->last_segment = cadence->arrival_last;
            }
        }
        owners[span_count++] = p;
    }
    HarmonyPhraseModel model = {0};
    HarmonyPhraseModelStatus status =
        span_count > 0 ? harmony_analysis_phrase_model_label(stage->segments, stage->count, spans, span_count, &model)
                       : HARMONY_PHRASE_MODEL_ERR_BAD_SPAN;
    free(spans);
    if (status != HARMONY_PHRASE_MODEL_OK) {
        free(owners);
        return status != HARMONY_PHRASE_MODEL_ERR_ALLOC;
    }
    for (size_t i = 0; i < model.segment_count && i < report->entry_count; i++) {
        if (model.segments[i].has_label) {
            report->entries[i].phrase_model = phrase_model_label_abbreviation(model.segments[i].label);
        }
    }
    for (size_t k = 0; k < span_count && k < model.phrase_count; k++) {
        report->phrases[owners[k]].phrase_model_labeled = model.phrase_labeled[k];
    }
    harmony_phrase_model_destroy(&model);
    free(owners);
    return true;
}

void harmony_stage_destroy(HarmonyStage *stage) {
    if (!stage) {
        return;
    }
    free(stage->boundary);
    harmony_analysis_destroy(stage->segments);
    chain_harmony_inputs_destroy(&stage->inputs);
    *stage = (HarmonyStage){0};
}
