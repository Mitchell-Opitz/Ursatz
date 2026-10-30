#include "pipeline/meter_check_pipeline.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "pipeline/harmonic_rhythm_pipeline.h"
#include "standard-meter/analysis/meter_analysis_check.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"

/* The evidence the library scores: the times of the harmony changes, the times the cadences arrive and every note onset,
   on Rational. The scoring, the candidates and the choice of the reading to offer are standard_meter_check_meter's. */
typedef struct {
    Rational *changes;
    size_t change_count;
    Rational *arrivals;
    size_t arrival_count;
    Rational *onsets;
    size_t onset_count;
} Evidence;

static void evidence_destroy(Evidence *evidence) {
    free(evidence->changes);
    free(evidence->arrivals);
    free(evidence->onsets);
    memset(evidence, 0, sizeof(*evidence));
}

static bool entry_time(const AnalysisReportEntry *entry, Rational *time) {
    return entry->time_denominator <= (unsigned long)LLONG_MAX &&
           rational_make((long long)entry->time_numerator, (long long)entry->time_denominator, time) == RATIONAL_OK;
}

static bool evidence_collect(const ImportedNotes *notes, const AnalysisReportEntry *entries, size_t entry_count,
                             const AnalysisCadence *cadences, size_t cadence_count, Evidence *evidence) {
    memset(evidence, 0, sizeof(*evidence));
    bool *is_change = NULL;
    evidence->changes = calloc(entry_count, sizeof(Rational));
    evidence->arrivals = calloc(cadence_count + 1, sizeof(Rational));
    evidence->onsets = calloc(notes->count + 1, sizeof(Rational));
    bool ok = evidence->changes && evidence->arrivals && evidence->onsets &&
              harmonic_rhythm_change_flags(entries, entry_count, &is_change);
    for (size_t i = 0; ok && i < entry_count; i++) {
        if (is_change[i]) {
            ok = entry_time(&entries[i], &evidence->changes[evidence->change_count++]);
        }
    }
    for (size_t c = 0; ok && c < cadence_count; c++) {
        if (cadences[c].arrival_first < entry_count) {
            ok = entry_time(&entries[cadences[c].arrival_first], &evidence->arrivals[evidence->arrival_count++]);
        }
    }
    for (size_t i = 0; ok && i < notes->count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        ok = rational_of_musical_time(onset, &evidence->onsets[evidence->onset_count++]) == RATIONAL_OK;
    }
    free(is_change);
    if (!ok) {
        evidence_destroy(evidence);
    }
    return ok;
}

static AnalysisMeterReading reading_of(const StandardMeterReading *reading) {
    AnalysisMeterReading out;
    memset(&out, 0, sizeof(out));
    out.beats = (int)reading->beats;
    out.beat_type = (int)reading->beat_type;
    out.scale_numerator = (long)reading->scale.num;
    out.scale_denominator = (unsigned long)reading->scale.den;
    out.pickup_numerator = (long)reading->pickup.num;
    out.pickup_denominator = (unsigned long)reading->pickup.den;
    out.score = reading->score;
    return out;
}

bool meter_check_run(const ImportedNotes *notes, const AnalysisReportEntry *entries, size_t entry_count,
                     const AnalysisCadence *cadences, size_t cadence_count, AnalysisMeterCheck *out) {
    memset(out, 0, sizeof(*out));
    const MeterMap *map = notes->meter.meter_map;
    if (entry_count == 0 || !map) {
        return true;
    }
    Evidence evidence;
    if (!evidence_collect(notes, entries, entry_count, cadences, cadence_count, &evidence)) {
        return false;
    }
    StandardMeterCheckEvidence input = {evidence.changes, evidence.change_count, evidence.arrivals,
                                        evidence.arrival_count, evidence.onsets, evidence.onset_count};
    StandardMeterCheck check;
    StandardMeterAnalysisStatus status = standard_meter_check_meter(map, &input, &check);
    evidence_destroy(&evidence);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    if (!check.present) {
        return true;
    }
    out->present = true;
    out->effective = reading_of(&check.effective);
    out->has_other = check.has_other;
    if (check.has_other) {
        out->other = reading_of(&check.other);
    }
    out->has_alternative = check.has_alternative;
    out->uncertainty = check.uncertainty;
    return true;
}

bool meter_check_stage_collect(const ImportedNotes *notes, AnalysisReport *report) {
    return meter_check_run(notes, report->entries, report->entry_count, report->cadences, report->cadence_count,
                           &report->meter_check);
}
