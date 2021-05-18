#include <ursatz_analyzer/analysis_pipeline.h>

#include <stdlib.h>
#include <string.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/key_region_pipeline.h"
#include "pipeline/outer_voice_pipeline.h"
#include "pipeline/segmentation.h"
#include "pipeline/spelling_util.h"

/* Names the roots and basses of the readings as the notated score spells them. */
static bool respell_chords(const ImportedNotes *notes, const Segmentation *segmentation, ChordEstimate *chords,
                           AnalysisKeyChange *changes, size_t change_count) {
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        if (!respell_chord_reading(&chords[i].best, notes, &segmentation->segments[i]) ||
            !respell_chord_reading(&chords[i].runner_up, notes, &segmentation->segments[i])) {
            return false;
        }
    }
    for (size_t c = 0; c < change_count; c++) {
        if (changes[c].has_pivot && changes[c].pivot_entry < segmentation->segment_count &&
            !respell_chord_reading(&changes[c].pivot, notes, &segmentation->segments[changes[c].pivot_entry])) {
            return false;
        }
    }
    return true;
}

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    report->entries = NULL;
    report->entry_count = 0;
    report->non_chord_tones = NULL;
    report->non_chord_tone_count = 0;
    report->key_regions = NULL;
    report->key_region_count = 0;
    report->key_changes = NULL;
    report->key_change_count = 0;
    report->home_region = 0;
    report->home_duration_numerator = 0;
    report->home_duration_denominator = 1;

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

    AnalysisKeyRegion *regions = NULL;
    size_t region_count = 0;
    AnalysisKeyChange *changes = NULL;
    size_t change_count = 0;
    if (!estimate_key_regions(notes, &segmentation, &regions, &region_count, &changes, &change_count)) {
        segmentation_destroy(&segmentation);
        return false;
    }
    AnalysisReport keys = {NULL, 0, NULL, 0, regions, region_count, changes, change_count, 0, 0, 1};

    ChordEstimate *chords = NULL;
    if (!identify_pivot_chords(&segmentation, regions, changes, change_count) ||
        !identify_chords(&segmentation, regions, region_count, &chords)) {
        analysis_report_destroy(&keys);
        segmentation_destroy(&segmentation);
        return false;
    }

    if (notes->source.spelling_notated &&
        !respell_chords(notes, &segmentation, chords, changes, change_count)) {
        analysis_report_destroy(&keys);
        chord_estimates_destroy(chords, segmentation.segment_count);
        segmentation_destroy(&segmentation);
        return false;
    }

    AnalysisReportEntry *entries = calloc(segmentation.segment_count, sizeof(AnalysisReportEntry));
    if (!entries) {
        analysis_report_destroy(&keys);
        chord_estimates_destroy(chords, segmentation.segment_count);
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

        entry->key_label = strdup(chords[i].in_region ? regions[chords[i].region].label : "no match");
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
            AnalysisReport partial = {entries, i + 1, NULL, 0, NULL, 0, NULL, 0, 0, 0, 1};
            analysis_report_destroy(&partial);
            analysis_report_destroy(&keys);
            chord_estimates_destroy(chords, segmentation.segment_count);
            segmentation_destroy(&segmentation);
            return false;
        }
    }

    if (!extract_outer_voices(notes, &segmentation, chords, regions, entries)) {
        AnalysisReport partial = {entries, segmentation.segment_count, NULL, 0, NULL, 0, NULL, 0, 0, 0, 1};
        analysis_report_destroy(&partial);
        analysis_report_destroy(&keys);
        chord_estimates_destroy(chords, segmentation.segment_count);
        segmentation_destroy(&segmentation);
        return false;
    }

    size_t segment_count = segmentation.segment_count;
    size_t tone_count = 0;
    for (size_t i = 0; i < segment_count; i++) {
        tone_count += chords[i].tone_count;
    }
    AnalysisNonChordTone *tones = calloc(tone_count > 0 ? tone_count : 1, sizeof(AnalysisNonChordTone));
    if (!tones) {
        AnalysisReport partial = {entries, segment_count, NULL, 0, NULL, 0, NULL, 0, 0, 0, 1};
        analysis_report_destroy(&partial);
        analysis_report_destroy(&keys);
        chord_estimates_destroy(chords, segment_count);
        segmentation_destroy(&segmentation);
        return false;
    }
    size_t next_tone = 0;
    for (size_t i = 0; i < segment_count; i++) {
        for (size_t t = 0; t < chords[i].tone_count; t++) {
            tones[next_tone] = chords[i].tones[t];
            tones[next_tone].chord_index = i;
            chords[i].tones[t].type = NULL; /* moved */
            next_tone++;
        }
    }
    chord_estimates_destroy(chords, segment_count);
    segmentation_destroy(&segmentation);

    report->entries = entries;
    report->entry_count = segment_count;
    report->non_chord_tones = tones;
    report->non_chord_tone_count = tone_count;
    report->key_regions = regions;
    report->key_region_count = region_count;
    report->key_changes = changes;
    report->key_change_count = change_count;
    if (region_count > 0) {
        report->home_region = home_key_region(regions, region_count, &report->home_duration_numerator,
                                              &report->home_duration_denominator);
    }
    return true;
}
