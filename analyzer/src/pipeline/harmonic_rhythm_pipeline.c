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

/* Whether entry starts different harmony from previous: another base numeral, or another key region. */
static bool is_change(const AnalysisReportEntry *previous, const AnalysisReportEntry *entry) {
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
        if (i == 0 || is_change(&report->entries[i - 1], &report->entries[i])) {
            rhythm[measures[i] - first].changes++;
        }
    }
    free(measures);
    report->harmonic_rhythm = rhythm;
    report->harmonic_rhythm_count = measure_count;
    return true;
}
