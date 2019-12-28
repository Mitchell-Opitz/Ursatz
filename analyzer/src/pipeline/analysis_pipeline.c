#include "pipeline/analysis_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/key_pipeline.h"
#include "pipeline/pitch_class_util.h"
#include "pipeline/segmentation.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static void copy_label(char *dest, size_t dest_size, const char *src) {
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* Bass pitch class of a SonorityGroup: the pitch class of its lowest
   MIDI note number. Used as a tonic guess -- the first and last chord
   of a piece is very often the tonic triad in root position, i.e. the
   bass note is the tonic. The closing chord is the more reliable of
   the two (a piece almost always ends on its tonic; it can just as
   easily open on an anacrusis, a pickup, or a non-tonic harmony), so
   callers should try it before the opening chord. */
static int bass_pitch_class(const SonorityGroup *group, PitchNumericValue *const *pitch_values) {
    long lowest_note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[0]]);
    for (size_t i = 1; i < group->note_index_count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[i]]);
        if (note_number < lowest_note_number) {
            lowest_note_number = note_number;
        }
    }
    return pitch_class_of(lowest_note_number);
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

    int priority_tonics[2];
    size_t priority_tonic_count = 0;
    priority_tonics[priority_tonic_count++] =
        bass_pitch_class(&grouping.groups[grouping.group_count - 1], notes->pitch_values);
    if (grouping.group_count > 1) {
        priority_tonics[priority_tonic_count++] = bass_pitch_class(&grouping.groups[0], notes->pitch_values);
    }

    KeyEstimate key = estimate_global_key(notes->events, notes->pitch_values, notes->count, notes->key_signature,
                                           priority_tonics, priority_tonic_count);

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
