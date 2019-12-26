#include "pipeline/analysis_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/key_pipeline.h"
#include "pipeline/segmentation.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static void copy_label(char *dest, size_t dest_size, const char *src) {
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    report->entries = NULL;
    report->entry_count = 0;
    report->key_found = false;

    if (notes->count == 0) {
        return true;
    }

    SonorityGrouping grouping;
    if (!segment_by_onset(notes->events, notes->count, &grouping)) {
        return false;
    }

    KeyEstimate key = estimate_global_key(notes->events, notes->pitch_values, notes->count, notes->key_signature);

    AnalysisReportEntry *entries = malloc(sizeof(AnalysisReportEntry) * grouping.group_count);
    if (!entries) {
        sonority_grouping_destroy(&grouping);
        return false;
    }

    for (size_t i = 0; i < grouping.group_count; i++) {
        const SonorityGroup *group = &grouping.groups[i];
        const NoteEvent *first = notes->events[group->note_indices[0]];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(first));

        AnalysisReportEntry *entry = &entries[i];
        entry->time_numerator = musical_time_numerator(onset);
        entry->time_denominator = musical_time_denominator(onset);

        copy_label(entry->key_label, sizeof(entry->key_label), key.found ? key.label : "no match");

        ChordEstimate chord = identify_chord_for_group(group, notes->events, notes->pitch_values, &key);
        copy_label(entry->chord_label, sizeof(entry->chord_label), chord.found ? chord.label : "no match");
    }

    size_t group_count = grouping.group_count;
    sonority_grouping_destroy(&grouping);

    report->entries = entries;
    report->entry_count = group_count;
    report->key_found = key.found;
    return true;
}
