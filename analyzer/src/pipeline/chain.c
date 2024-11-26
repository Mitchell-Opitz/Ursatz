#include "pipeline/chain.h"

#include <stdlib.h>
#include <string.h>

#include "pipeline/key_region_pipeline.h"
#include "pipeline/outer_voice_pipeline.h"
#include "pipeline/roman_numeral_pipeline.h"
#include "pipeline/spelling_util.h"

static const AnalysisChordReading NO_READING = {NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};

bool chain_init(AnalysisChain *chain, const ImportedNotes *notes) {
    memset(chain, 0, sizeof(*chain));
    chain->notes = notes;
    if (notes->count == 0) {
        return true;
    }
    return segment_notes(notes, true, &chain->segmentation) && segment_notes(notes, false, &chain->voice_segmentation);
}

/* Names the roots and basses of the readings as the notated score spells them. */
static bool respell_chords(AnalysisChain *chain) {
    const Segmentation *segmentation = &chain->segmentation;
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        if (!respell_chord_reading(&chain->chords[i].best, chain->notes, &segmentation->segments[i]) ||
            !respell_chord_reading(&chain->chords[i].runner_up, chain->notes, &segmentation->segments[i])) {
            return false;
        }
    }
    AnalysisKeyChange *changes = chain->report.key_changes;
    for (size_t c = 0; c < chain->report.key_change_count; c++) {
        if (changes[c].has_pivot && changes[c].pivot_entry < segmentation->segment_count &&
            !respell_chord_reading(&changes[c].pivot, chain->notes, &segmentation->segments[changes[c].pivot_entry])) {
            return false;
        }
    }
    return true;
}

static AnalysisChordContext chord_context(const ChordEstimate *chord) {
    if (!chord->found) {
        return ANALYSIS_CHORD_CONTEXT_NONE;
    }
    switch (chord->context) {
        case COMMON_PRACTICE_CONTEXT_PREVIOUS:
            return ANALYSIS_CHORD_CONTEXT_PREVIOUS;
        case COMMON_PRACTICE_CONTEXT_NEXT:
            return ANALYSIS_CHORD_CONTEXT_NEXT;
        case COMMON_PRACTICE_CONTEXT_ARPEGGIO:
            return ANALYSIS_CHORD_CONTEXT_ARPEGGIO;
        default:
            return ANALYSIS_CHORD_CONTEXT_NONE;
    }
}

/* One entry per chord segment: its span, notes, key and chord. The readings move out of the estimates. */
static bool build_entries(AnalysisChain *chain) {
    const Segmentation *segmentation = &chain->segmentation;
    size_t count = segmentation->segment_count;
    AnalysisReport *report = &chain->report;
    report->entries = calloc(count, sizeof(AnalysisReportEntry));
    if (!report->entries) {
        return false;
    }
    report->entry_count = count;
    for (size_t i = 0; i < count; i++) {
        const Segment *segment = &segmentation->segments[i];
        ChordEstimate *chord = &chain->chords[i];
        AnalysisReportEntry *entry = &report->entries[i];
        entry->time_numerator = segment->start_numerator;
        entry->time_denominator = segment->start_denominator;
        entry->end_numerator = segment->end_numerator;
        entry->end_denominator = segment->end_denominator;
        entry->key_label = strdup(chord->in_region ? report->key_regions[chord->region].label : "no match");
        entry->chord_label = strdup(chord->found ? chord->best.label : "no match");
        entry->chord_confidence = chord->best.confidence;
        entry->chord = chord->best;
        entry->runner_up = chord->runner_up;
        entry->chord_context = chord_context(chord);
        chord->best = NO_READING;
        chord->runner_up = NO_READING;
        entry->in_region = chord->in_region;
        entry->region = chord->region;
        entry->note_ids = malloc(sizeof(size_t) * segment->member_count);
        if (entry->note_ids) {
            memcpy(entry->note_ids, segment->note_indices, sizeof(size_t) * segment->member_count);
            entry->note_id_count = segment->member_count;
        }
        if (!entry->key_label || !entry->chord_label || !entry->note_ids) {
            return false;
        }
    }
    return true;
}

/* The estimates' non-chord tones, in time order, each against its chord's entry. The tones move out. */
static bool collect_non_chord_tones(AnalysisChain *chain) {
    size_t count = chain->segmentation.segment_count;
    size_t tone_count = 0;
    for (size_t i = 0; i < count; i++) {
        tone_count += chain->chords[i].tone_count;
    }
    AnalysisNonChordTone *tones = calloc(tone_count > 0 ? tone_count : 1, sizeof(AnalysisNonChordTone));
    if (!tones) {
        return false;
    }
    size_t next = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t t = 0; t < chain->chords[i].tone_count; t++) {
            tones[next] = chain->chords[i].tones[t];
            tones[next].chord_index = i;
            chain->chords[i].tones[t].type = NULL; /* moved */
            next++;
        }
    }
    chain->report.non_chord_tones = tones;
    chain->report.non_chord_tone_count = tone_count;
    return true;
}

bool chain_pass(AnalysisChain *chain, const MusicalTime *const *boundaries, size_t boundary_count) {
    const Segmentation *segmentation = &chain->segmentation;
    AnalysisReport *report = &chain->report;
    if (segmentation->segment_count == 0) {
        return true;
    }

    if (!estimate_key_regions(chain->notes, segmentation, chain->key_chords, boundaries, boundary_count,
                              &chain->key_regions, &report->key_regions, &report->key_region_count,
                              &report->key_changes, &report->key_change_count) ||
        !identify_pivot_chords(segmentation, report->key_regions, report->key_changes, report->key_change_count) ||
        !identify_chords(segmentation, report->key_regions, report->key_region_count, &chain->chords)) {
        return false;
    }
    if (chain->notes->source.spelling_notated && !respell_chords(chain)) {
        return false;
    }
    if (!build_entries(chain) ||
        !build_roman_numerals(segmentation, chain->chords, &chain->key_regions, &chain->romans) ||
        !assign_roman_numerals(&chain->romans, chain->chords, report->entries) ||
        !extract_outer_voices(chain->notes, &chain->voice_segmentation, segmentation, chain->chords,
                              report->key_regions, report->entries, &chain->lines) ||
        !collect_non_chord_tones(chain)) {
        return false;
    }
    if (report->key_region_count > 0) {
        report->home_region = home_key_region(report->key_regions, report->key_region_count,
                                              &report->home_duration_numerator, &report->home_duration_denominator);
    }
    if (!harmony_stage_prepare(chain->notes, segmentation, &chain->romans, report->entries, &chain->harmony) ||
        !harmony_stage_find_cadences(&chain->harmony) ||
        !harmony_stage_add_boundary_cadences(&chain->harmony, &chain->key_regions,
                                             report->key_region_count > 0
                                                 ? chain->key_regions.regions[report->home_region].key_index
                                                 : -1) ||
        !harmony_stage_confirm_key_changes(&chain->harmony, segmentation, &chain->key_regions) ||
        !harmony_stage_collect_cadences(&chain->harmony, report) ||
        !harmony_stage_collect_functions(&chain->harmony, report)) {
        return false;
    }
    sync_key_change_extents(&chain->key_regions, report->key_changes);
    return true;
}

bool chain_keep_chord_evidence(AnalysisChain *chain) {
    size_t count = chain->segmentation.segment_count;
    if (count == 0 || !chain->chords) {
        return true;
    }
    if (!chain->key_chords) {
        chain->key_chords = calloc(count, sizeof(CommonPracticeKeyChord));
        if (!chain->key_chords) {
            return false;
        }
    }
    for (size_t i = 0; i < count; i++) {
        chain->key_chords[i] = common_practice_key_chord_of(&chain->chords[i].result);
    }
    return true;
}

void chain_pass_destroy(AnalysisChain *chain) {
    chord_estimates_destroy(chain->chords, chain->segmentation.segment_count);
    chain->chords = NULL;
    roman_numerals_destroy(&chain->romans);
    common_practice_key_regions_destroy(&chain->key_regions);
    chain->key_regions = (CommonPracticeKeyRegions){0};
    outer_voice_lines_destroy(&chain->lines);
    chain->lines = (OuterVoiceLines){NULL, NULL, 0};
    harmony_stage_destroy(&chain->harmony);
    analysis_report_destroy(&chain->report);
    memset(&chain->report, 0, sizeof(chain->report));
    chain->report.home_duration_denominator = 1;
}

void chain_destroy(AnalysisChain *chain) {
    chain_pass_destroy(chain);
    segmentation_destroy(&chain->segmentation);
    segmentation_destroy(&chain->voice_segmentation);
    free(chain->key_chords);
    chain->key_chords = NULL;
}
