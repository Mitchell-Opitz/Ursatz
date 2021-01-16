#include <ursatz_analyzer/analysis_pipeline.h>

#include <stdlib.h>
#include <string.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/key_pipeline.h"
#include "pipeline/pitch_class_util.h"
#include "pipeline/segmentation.h"

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    report->entries = NULL;
    report->entry_count = 0;
    report->key_found = false;
    report->key_method = ANALYSIS_KEY_METHOD_NONE;
    report->key_confidence = (AnalysisConfidence){false, 0, 0};

    if (notes->count == 0) {
        return true;
    }

    Segmentation segmentation;
    if (!segment_notes(notes, &segmentation)) {
        return false;
    }
    if (segmentation.segment_count == 0) {
        segmentation_destroy(&segmentation);
        return true;
    }

    /* Tonic guess: the bass of the closing segment, then of the opening one. A piece almost always
       ends on its tonic; it can just as easily open on an anacrusis, a pickup, or a non-tonic
       harmony. */
    int priority_tonics[2];
    size_t priority_tonic_count = 0;
    priority_tonics[priority_tonic_count++] =
        segment_bass_pitch_class(&segmentation.segments[segmentation.segment_count - 1]);
    if (segmentation.segment_count > 1) {
        priority_tonics[priority_tonic_count++] = segment_bass_pitch_class(&segmentation.segments[0]);
    }

    KeyEstimate key;
    if (!estimate_global_key(notes->events, notes->pitch_values, notes->count, notes->key_signature,
                             priority_tonics, priority_tonic_count, &key)) {
        segmentation_destroy(&segmentation);
        return false;
    }

    ChordEstimate *chords = NULL;
    if (!identify_chords(&segmentation, &key, &chords)) {
        key_estimate_destroy(&key);
        segmentation_destroy(&segmentation);
        return false;
    }

    AnalysisReportEntry *entries = calloc(segmentation.segment_count, sizeof(AnalysisReportEntry));
    if (!entries) {
        chord_estimates_destroy(chords, segmentation.segment_count);
        key_estimate_destroy(&key);
        segmentation_destroy(&segmentation);
        return false;
    }

    for (size_t i = 0; i < segmentation.segment_count; i++) {
        const Segment *segment = &segmentation.segments[i];

        AnalysisReportEntry *entry = &entries[i];
        entry->time_numerator = segment->start_numerator;
        entry->time_denominator = segment->start_denominator;
        entry->end_numerator = segment->end_numerator;
        entry->end_denominator = segment->end_denominator;

        entry->key_label = strdup(key.found ? key.label : "no match");
        entry->chord_label = strdup(chords[i].found ? chords[i].best.label : "no match");
        entry->chord_confidence = chords[i].best.confidence;
        entry->chord = chords[i].best;
        entry->runner_up = chords[i].runner_up;
        chords[i].best = (AnalysisChordReading){NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};
        chords[i].runner_up = chords[i].best;

        entry->note_ids = malloc(sizeof(size_t) * segment->member_count);
        if (entry->note_ids) {
            memcpy(entry->note_ids, segment->note_indices, sizeof(size_t) * segment->member_count);
            entry->note_id_count = segment->member_count;
        }

        if (!entry->key_label || !entry->chord_label || !entry->note_ids) {
            AnalysisReport partial = {entries, i + 1, false, ANALYSIS_KEY_METHOD_NONE, {false, 0, 0}};
            analysis_report_destroy(&partial);
            chord_estimates_destroy(chords, segmentation.segment_count);
            key_estimate_destroy(&key);
            segmentation_destroy(&segmentation);
            return false;
        }
    }

    size_t segment_count = segmentation.segment_count;
    chord_estimates_destroy(chords, segment_count);
    segmentation_destroy(&segmentation);

    report->entries = entries;
    report->entry_count = segment_count;
    report->key_found = key.found;
    report->key_method = key.method;
    report->key_confidence = key.confidence;
    key_estimate_destroy(&key);
    return true;
}
