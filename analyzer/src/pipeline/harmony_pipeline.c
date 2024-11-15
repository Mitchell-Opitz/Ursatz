#include "pipeline/harmony_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "metric_position_rules.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/metrical_position.h"
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

/* The length of one beat in the signature of the measure the position lies in, or NULL when the meter gives none. */
static Duration *beat_length_at(const MeterMap *map, const MetricalPosition *position) {
    const TimeSignature *reading = NULL;
    Duration *beat = NULL;
    unsigned beats = 0;
    if (metrical_position_reading_signature(map, metrical_position_measure(position), &reading) !=
            METRICAL_POSITION_OK ||
        !standard_meter_beat_grid(reading, &beat, &beats)) {
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

void harmony_stage_destroy(HarmonyStage *stage) {
    if (!stage) {
        return;
    }
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
