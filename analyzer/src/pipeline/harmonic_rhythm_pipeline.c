#include "pipeline/harmonic_rhythm_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "time/meter_map.h"
#include "time/metrical_position.h"
#include "time/musical_time.h"

/* The measure the time lies in on the meter map, or false when the map cannot place it. */
static bool measure_at(const MeterMap *map, long numerator, unsigned long denominator, int *measure) {
    MusicalTime *time = NULL;
    if (musical_time_create(numerator, denominator, &time) != MUSICAL_TIME_OK) {
        return false;
    }
    MetricalPosition *position = NULL;
    MetricalPositionResult result = metrical_position_from_time(map, time, &position);
    musical_time_destroy(time);
    if (result != METRICAL_POSITION_OK) {
        return false;
    }
    *measure = metrical_position_measure(position);
    metrical_position_destroy(position);
    return true;
}

bool harmonic_rhythm_is_change(const AnalysisReportEntry *previous, const AnalysisReportEntry *entry) {
    if (previous->in_region != entry->in_region || (entry->in_region && previous->region != entry->region)) {
        return true;
    }
    const char *before = previous->roman.labelled ? previous->roman.base_numeral : NULL;
    const char *after = entry->roman.labelled ? entry->roman.base_numeral : NULL;
    if (!before || !after) {
        return before != after;
    }
    return strcmp(before, after) != 0;
}

/* The changes in measures from..to (inclusive) that the piece has, and how many measures that is. */
static void window_changes(const AnalysisMeasureChanges *rhythm, size_t count, int from, int to, size_t *changes,
                           size_t *measures) {
    *changes = 0;
    *measures = 0;
    for (size_t m = 0; m < count; m++) {
        if (rhythm[m].measure >= from && rhythm[m].measure <= to) {
            *changes += rhythm[m].changes;
            (*measures)++;
        }
    }
}

/* One approach per cadence, from the arrival's measure (measures[i] is the measure of entry i). */
static bool collect_approaches(const int *measures, AnalysisReport *report) {
    if (report->cadence_count == 0) {
        return true;
    }
    AnalysisCadenceApproach *approaches = calloc(report->cadence_count, sizeof(AnalysisCadenceApproach));
    if (!approaches) {
        return false;
    }
    for (size_t c = 0; c < report->cadence_count; c++) {
        AnalysisCadenceApproach *approach = &approaches[c];
        approach->cadence = c;
        approach->arrival_measure = measures[report->cadences[c].arrival_first];
        window_changes(report->harmonic_rhythm, report->harmonic_rhythm_count, approach->arrival_measure - 4,
                       approach->arrival_measure - 3, &approach->before_changes, &approach->before_measures);
        window_changes(report->harmonic_rhythm, report->harmonic_rhythm_count, approach->arrival_measure - 2,
                       approach->arrival_measure - 1, &approach->approach_changes, &approach->approach_measures);
    }
    report->cadence_approaches = approaches;
    report->cadence_approach_count = report->cadence_count;
    return true;
}

bool harmonic_rhythm_stage_collect(const ImportedNotes *notes, AnalysisReport *report) {
    size_t count = report->entry_count;
    if (count == 0) {
        return true;
    }
    const MeterMap *map = notes->meter.meter_map;
    int *measures = malloc(count * sizeof(int));
    if (!measures) {
        return false;
    }
    int first = 0;
    int last = 0;
    for (size_t i = 0; i < count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!measure_at(map, entry->time_numerator, entry->time_denominator, &measures[i])) {
            free(measures);
            return true; /* a segment off the meter: no harmonic rhythm, as with the cadences */
        }
        if (i == 0 || measures[i] < first) {
            first = measures[i];
        }
        if (i == 0 || measures[i] > last) {
            last = measures[i];
        }
    }
    size_t measure_count = (size_t)(last - first) + 1;
    AnalysisMeasureChanges *rhythm = calloc(measure_count, sizeof(AnalysisMeasureChanges));
    if (!rhythm) {
        free(measures);
        return false;
    }
    for (size_t m = 0; m < measure_count; m++) {
        rhythm[m].measure = first + (int)m;
    }
    for (size_t i = 0; i < count; i++) {
        if (i == 0 || harmonic_rhythm_is_change(&report->entries[i - 1], &report->entries[i])) {
            rhythm[measures[i] - first].changes++;
        }
    }
    report->harmonic_rhythm = rhythm;
    report->harmonic_rhythm_count = measure_count;
    bool ok = collect_approaches(measures, report);
    free(measures);
    return ok;
}
