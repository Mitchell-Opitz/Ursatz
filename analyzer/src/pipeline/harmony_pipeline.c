#include "pipeline/harmony_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "standard-harmony/harmonic_function.h"
#include "standard-harmony/harmony_functions.h"
#include "standard-harmony/harmony_key_change.h"
#include "standard-meter/metric_position_rules.h"
#include "standard-harmony/phrase_model_label.h"
#include "standard-harmony/analysis/analysis_phrase.h"
#include "semantics/key_change_extent.h"
#include "standard-harmony/six_four_type.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/time_rational.h"
#include "time/time_span.h"

/* The chord's root-relative semitone intervals, from the quality the library's Roman numeral claim names
   (StandardTonalityTriadQuality, StandardTonalitySeventhQuality). False for a quality outside the library's tables. */
static bool intervals_of(const StandardTonalityRomanClaim *claim, int out[HARMONY_SEGMENT_MAX_INTERVALS],
                         size_t *count) {
    static const int TRIADS[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    static const int SEVENTHS[5][4] = {{0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    memset(out, 0, HARMONY_SEGMENT_MAX_INTERVALS * sizeof(int));
    if (claim->seventh) {
        if (claim->quality < 0 || claim->quality >= (int)STANDARD_TONALITY_SEVENTH_QUALITY_COUNT) {
            return false;
        }
        memcpy(out, SEVENTHS[claim->quality], 4 * sizeof(int));
        *count = 4;
    } else {
        if (claim->quality < 0 || claim->quality >= (int)STANDARD_TONALITY_TRIAD_QUALITY_COUNT) {
            return false;
        }
        memcpy(out, TRIADS[claim->quality], 3 * sizeof(int));
        *count = 3;
    }
    return true;
}

/* The scale degree 1..7 of an outer voice, from the library's own label for it in the local key ("3"; a chromatic
   "#4" or "b6" is not a degree of the key's scale), or 0 when the voice is absent or its note chromatic. */
static int degree_of(const AnalysisOuterNote *note) {
    if (note->status != ANALYSIS_OUTER_PRESENT || !note->degree || note->degree[0] < '1' || note->degree[0] > '7' ||
        note->degree[1] != '\0') {
        return 0;
    }
    return note->degree[0] - '0';
}

/* The length of one beat in the measure the position lies in, or NULL when the meter gives none. */
static Duration *beat_length_at(const MeterMap *map, const MetricalPosition *position) {
    Rational length;
    Duration *beat = NULL;
    if (meter_map_beat_length(map, metrical_position_measure(position), &length) != METER_MAP_OK ||
        duration_from_rational(length, &beat) != TIME_RATIONAL_OK) {
        return NULL;
    }
    return beat;
}

/* One segment's input: its place on the meter, and the chord from the library's Roman numeral claim. */
static void fill_input(const RomanNumerals *romans, const AnalysisReportEntry *entry, size_t i,
                       HarmonySegmentInput *input) {
    const StandardTonalityRomanResult *result = &romans->results[i];
    const StandardTonalityChordInput *chord = &romans->inputs[i];
    input->degree = 0; /* not a chord this framework reads */
    input->interval_count = 3;
    int intervals[HARMONY_SEGMENT_MAX_INTERVALS];
    size_t interval_count = 0;
    if (!result->has_numeral || !intervals_of(&result->claim, intervals, &interval_count)) {
        return;
    }
    memcpy(input->intervals, intervals, sizeof(intervals));
    input->interval_count = interval_count;
    input->degree = result->claim.degree;
    input->inversion = result->claim.inversion;
    input->applied_to = result->claim.applied ? result->claim.applied_target_degree : 0;
    input->mode = chord->mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
    input->uncertainty = result->uncertainty;
    input->bass_degree = degree_of(&entry->bass);
    input->soprano_degree = degree_of(&entry->soprano);
}

bool harmony_stage_prepare(const ImportedNotes *notes, const Segmentation *segmentation, const RomanNumerals *romans,
                           const AnalysisReportEntry *entries, HarmonyStage *out) {
    *out = (HarmonyStage){0};
    size_t count = segmentation->segment_count;
    if (count == 0 || romans->count != count) {
        return true;
    }
    const MeterMap *map = notes->meter.meter_map;
    out->inputs = calloc(count, sizeof(HarmonySegmentInput));
    out->positions = calloc(count, sizeof(MetricalPosition *));
    out->beat_lengths = calloc(count, sizeof(Duration *));
    if (!out->inputs || !out->positions || !out->beat_lengths) {
        return false;
    }
    out->count = count;
    for (size_t i = 0; i < count; i++) {
        const TimeSpan *window = sonority_time_window(segmentation->sonorities.items[i].sonority);
        if (metrical_position_from_time(map, time_span_start(window), &out->positions[i]) != METRICAL_POSITION_OK) {
            return true; /* a segment off the meter: the library cannot prepare the piece, so it has no cadences */
        }
        out->beat_lengths[i] = beat_length_at(map, out->positions[i]);
        HarmonySegmentInput *input = &out->inputs[i];
        input->segment_id = segmentation->segment_ids[i];
        input->span = window;
        input->position = out->positions[i];
        input->beat_strength = beat_strength_at(map, time_span_start(window));
        input->key_ref = entries[i].key_label;
        input->mode = HARMONY_MODE_MAJOR;
        input->beat_length = out->beat_lengths[i];
        fill_input(romans, &entries[i], i, input);
    }
    HarmonyAnalysisStatus status = harmony_analysis_prepare(out->inputs, count, &out->segments);
    if (status == HARMONY_ANALYSIS_ERR_ALLOC) {
        return false;
    }
    if (status != HARMONY_ANALYSIS_OK) {
        out->segments = NULL; /* the library declined the inputs: no cadences */
    }
    return true;
}

bool harmony_stage_find_cadences(HarmonyStage *stage) {
    if (!stage->segments) {
        return true;
    }
    HarmonyCadenceAnalysisStatus status = harmony_analysis_find_cadences(stage->segments, stage->count, NULL,
                                                                          &stage->cadences);
    return status != HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
}

static double time_double(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

/* The start times, in whole notes, of the sections: each key region that returns to the home key after leaving it
   (a recapitulation). Returns how many were written to out, which holds region_count. */
static size_t section_starts(const CommonPracticeKeyRegions *regions, int home_key_index, double *out) {
    size_t count = 0;
    for (size_t i = 1; i < regions->region_count; i++) {
        if (regions->regions[i].key_index == home_key_index && regions->regions[i - 1].key_index != home_key_index) {
            out[count++] = time_double(time_span_start(regions->regions[i].span));
        }
    }
    return count;
}

typedef struct {
    CadenceType type;
    size_t arrival_first;
} CadenceKey;

bool harmony_stage_add_boundary_cadences(HarmonyStage *stage, const CommonPracticeKeyRegions *regions,
                                         int home_key_index) {
    if (!stage->segments || regions->region_count < 2) {
        return true;
    }
    double *starts = malloc(sizeof(double) * regions->region_count);
    CadenceKey *before = malloc(sizeof(CadenceKey) * (stage->cadences.count > 0 ? stage->cadences.count : 1));
    if (!starts || !before) {
        free(starts);
        free(before);
        return false;
    }
    size_t start_count = section_starts(regions, home_key_index, starts);
    size_t before_count = stage->cadences.count;
    for (size_t i = 0; i < before_count; i++) {
        before[i] = (CadenceKey){stage->cadences.items[i].type, stage->cadences.items[i].arrival_first};
    }
    HarmonyCadenceAnalysisStatus status = HARMONY_CADENCE_ANALYSIS_OK;
    if (start_count > 0) {
        status = harmony_analysis_add_boundary_cadences(stage->segments, stage->count, NULL, starts, start_count,
                                                        &stage->cadences);
    }
    free(starts);
    bool ok = status != HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    stage->boundary = ok ? calloc(stage->cadences.count > 0 ? stage->cadences.count : 1, sizeof(bool)) : NULL;
    ok = ok && stage->boundary;
    for (size_t i = 0; ok && i < stage->cadences.count; i++) {
        bool existed = false;
        for (size_t b = 0; b < before_count && !existed; b++) {
            existed = before[b].type == stage->cadences.items[i].type &&
                      before[b].arrival_first == stage->cadences.items[i].arrival_first;
        }
        stage->boundary[i] = !existed;
    }
    free(before);
    return ok;
}

bool harmony_stage_confirm_key_changes(const HarmonyStage *stage, const Segmentation *segmentation,
                                       CommonPracticeKeyRegions *regions) {
    if (regions->change_count == 0) {
        return true;
    }
    double *arrivals = NULL;
    if (stage->segments) {
        arrivals = calloc(stage->cadences.count + 1, sizeof(double));
        if (!arrivals) {
            return false;
        }
        for (size_t k = 0; k < stage->cadences.count; k++) {
            const Segment *arrival = &segmentation->segments[stage->cadences.items[k].arrival_first];
            arrivals[k] = (double)arrival->start_numerator / (double)arrival->start_denominator;
        }
    }
    for (size_t i = 0; i < regions->change_count; i++) {
        CommonPracticeKeyChange *change = &regions->changes[i];
        if (change->is_return) {
            continue;
        }
        const CommonPracticeKeyRegion *into = &regions->regions[change->to_region];
        KeyChangeExtent confirmed = change->extent;
        if (harmony_key_change_confirm(change->extent, time_double(time_span_start(into->span)),
                                       time_double(time_span_end(into->span)), arrivals, stage->cadences.count,
                                       &confirmed)) {
            change->extent = confirmed;
        }
        change->cadence_confirmed = arrivals != NULL && key_change_extent_is_modulation(change->extent);
    }
    for (size_t i = 1; i < regions->change_count; i++) {
        if (regions->changes[i].is_return) {
            regions->changes[i].extent = regions->changes[i - 1].extent;
            regions->changes[i].cadence_confirmed = regions->changes[i - 1].cadence_confirmed;
        }
    }
    free(arrivals);
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
    harmony_cadences_destroy(&stage->cadences);
    harmony_analysis_destroy(stage->segments);
    for (size_t i = 0; stage->positions && i < stage->count; i++) {
        metrical_position_destroy(stage->positions[i]);
        if (stage->beat_lengths) {
            duration_destroy(stage->beat_lengths[i]);
        }
    }
    free(stage->positions);
    free(stage->beat_lengths);
    free(stage->inputs);
    *stage = (HarmonyStage){0};
}
