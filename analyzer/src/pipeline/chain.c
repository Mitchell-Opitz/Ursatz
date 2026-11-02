#include "pipeline/chain.h"

#include <stdlib.h>
#include <string.h>

#include "common-practice-minimal/key_regions.h"
#include "time/time_span.h"
#include "pipeline/form_plan_pipeline.h"
#include "pipeline/harmonic_rhythm_pipeline.h"
#include "pipeline/key_region_pipeline.h"
#include "pipeline/melody_pipeline.h"
#include "pipeline/meter_check_pipeline.h"
#include "pipeline/outer_voice_pipeline.h"
#include "pipeline/progression_pipeline.h"
#include "pipeline/rhythm_pipeline.h"
#include "pipeline/schema_pipeline.h"
#include "pipeline/markings_pipeline.h"
#include "pipeline/texture_pipeline.h"
#include "pipeline/voice_leading_pipeline.h"
#include "pipeline/roman_numeral_pipeline.h"
#include "pipeline/spelling_util.h"

static const AnalysisChordReading NO_READING = {NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};

/* A Segmentation that views the chain's segments: the exact windows from the library's sonorities and the members' notes
   as ImportedNotes indices. The library's segments, ids and sonorities are borrowed; the voice segments carry no
   members in the chain, so theirs are built here from the sonority's events. */
static bool segmentation_view(const AnalyzerPass *pass, const ChainSegments *source, bool with_members,
                              Segmentation *out) {
    *out = (Segmentation){NULL, 0, NULL, NULL, {NULL, 0}};
    if (!source || source->count == 0) {
        return true;
    }
    out->segments = calloc(source->count, sizeof(Segment));
    if (!out->segments) {
        return false;
    }
    out->segment_count = source->count;
    out->library_segments = source->segments;
    out->segment_ids = source->ids;
    out->sonorities = source->sonorities;
    for (size_t s = 0; s < source->count; s++) {
        const AdaptiveSonority *item = &source->sonorities.items[s];
        const TimeSpan *window = sonority_time_window(item->sonority);
        Segment *segment = &out->segments[s];
        segment->start_numerator = musical_time_numerator(time_span_start(window));
        segment->start_denominator = musical_time_denominator(time_span_start(window));
        segment->end_numerator = musical_time_numerator(time_span_end(window));
        segment->end_denominator = musical_time_denominator(time_span_end(window));
        size_t count = sonority_event_id_count(item->sonority);
        segment->member_count = count;
        segment->note_indices = malloc(sizeof(size_t) * (count > 0 ? count : 1));
        if (!segment->note_indices) {
            return false;
        }
        if (!with_members) {
            segment->members = (CommonPracticeMember *)source->segments[s].members;
        } else {
            segment->members = NULL;
        }
        CommonPracticeMember *own = with_members ? calloc(count > 0 ? count : 1, sizeof(CommonPracticeMember)) : NULL;
        if (with_members && !own) {
            return false;
        }
        for (size_t j = 0; j < count; j++) {
            const char *id = entity_id_value(sonority_event_id_at(item->sonority, j));
            size_t index = chain_input_import_index(&pass->input, id);
            if (index == (size_t)-1) {
                free(own);
                return false;
            }
            segment->note_indices[j] = index;
            if (own) {
                own[j].note_id = id;
                own[j].pitch = (int)pitch_numeric_value_position_numerator(pass->notes->pitch_values[index]);
                own[j].held = item->held[j];
            }
        }
        if (own) {
            segment->members = own;
        }
    }
    return true;
}

static void segmentation_view_destroy(Segmentation *view, bool owns_members) {
    for (size_t i = 0; i < view->segment_count; i++) {
        free(view->segments[i].note_indices);
        if (owns_members) {
            free(view->segments[i].members);
        }
    }
    free(view->segments);
    *view = (Segmentation){NULL, 0, NULL, NULL, {NULL, 0}};
}

/* Names the roots and basses of the readings as the notated score spells them. */
static bool respell_chords(AnalyzerPass *chain) {
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
static bool build_entries(AnalyzerPass *chain) {
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
static bool collect_non_chord_tones(AnalyzerPass *chain) {
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

/* Asks the cancel callback; true (and pass->cancelled set) when it wants the run to stop. */
static bool stop_requested(AnalyzerPass *pass) {
    if (pass->options.cancel && pass->options.cancel(pass->options.cancel_context)) {
        pass->cancelled = true;
        return true;
    }
    return false;
}

/* The home key: the library's key with the longest total duration over its regions. */
static bool collect_pass(AnalyzerPass *pass) {
    const Segmentation *segmentation = &pass->segmentation;
    AnalysisReport *report = &pass->report;
    const ImportedNotes *notes = pass->notes;
    if (segmentation->segment_count == 0) {
        return true;
    }

    if (!key_regions_collect(analysis_chain_key_regions(pass->library), report) ||
        !identify_pivot_chords(segmentation, report->key_regions, report->key_changes, report->key_change_count) ||
        !collect_chords(segmentation, analysis_chain_chords(pass->library), report->key_regions,
                        report->key_region_count, &pass->chords)) {
        return false;
    }
    if (notes->source.spelling_notated && !respell_chords(pass)) {
        return false;
    }
    const OuterVoiceLines *lines = analysis_chain_outer_voices(pass->library);
    if (lines) {
        pass->lines = *lines;
    }
    if (!build_entries(pass) || !read_roman_numerals(pass->library, &pass->romans) ||
        !assign_roman_numerals(&pass->romans, pass->chords, report->entries) ||
        !collect_outer_voices(notes, &pass->voice_segmentation, segmentation, pass->chords, report->key_regions,
                              lines, report->entries) ||
        !collect_non_chord_tones(pass)) {
        return false;
    }
    /* The Analyzer-only stages are switched by their bits; the cancel callback is asked before each. */
#define STAGE_ON(bit) ((pass->options.stages & (bit)) != 0)
#define RUN(call) \
    do {          \
        if (!(call)) { \
            return false; \
        } \
    } while (0)
#define RUN_STAGE(bit, call)       \
    do {                           \
        if (STAGE_ON(bit)) {       \
            if (stop_requested(pass)) { \
                return false;      \
            }                      \
            RUN(call);             \
        }                          \
    } while (0)
    RUN(harmony_stage_prepare(pass->library, &pass->harmony));
    RUN(harmony_stage_collect_cadences(&pass->harmony, report));
    RUN(harmony_stage_collect_functions(&pass->harmony, report));
    RUN(harmony_stage_collect_six_fours_and_prolongations(&pass->harmony, report));
    RUN_STAGE(ANALYZER_STAGE_PROGRESSIONS, progression_stage_collect(report));
    RUN_STAGE(ANALYZER_STAGE_HARMONIC_RHYTHM, harmonic_rhythm_stage_collect(notes, report));
    RUN_STAGE(ANALYZER_STAGE_METER_CHECK, meter_check_stage_collect(notes, report));
    RUN(form_stage_read(&pass->form, notes, &pass->input, pass->library));
    RUN(form_stage_collect(&pass->form, report));
    RUN(harmony_stage_collect_phrase_model(&pass->harmony, report));
    RUN_STAGE(ANALYZER_STAGE_FORM_PLAN, form_plan_stage_collect(&pass->form, report));
    RUN(motive_stage_read(&pass->motives, &pass->input, pass->library));
    RUN(motive_stage_collect(&pass->motives, notes, report));
    RUN(melody_stage_collect(notes, &pass->input, pass->library, segmentation, report));
    RUN_STAGE(ANALYZER_STAGE_RHYTHM, rhythm_stage_collect(notes, &pass->lines, &pass->form, report));
    RUN(texture_stage_collect(notes, pass->library, report));
    RUN_STAGE(ANALYZER_STAGE_MARKINGS, markings_stage_collect(notes, report));
    RUN(voice_leading_stage_collect(notes, &pass->input, pass->library, &pass->harmony, report));
    RUN(schema_stage_collect(notes, pass->library, report));
#undef RUN_STAGE
#undef RUN
#undef STAGE_ON
    return true;
}

bool analyzer_pass_run(AnalyzerPass *pass, const ImportedNotes *notes, const AnalyzerOptions *options) {
    memset(pass, 0, sizeof(*pass));
    pass->notes = notes;
    pass->options = options ? *options : analyzer_options_default();
    pass->report.home_duration_denominator = 1;
    if (notes->count == 0) {
        return true;
    }
    if (stop_requested(pass)) {
        return false;
    }
    if (!chain_input_build(notes, &pass->input)) {
        return false;
    }
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = pass->options.stages & ANALYSIS_CHAIN_STAGE_ALL;
    config.refine_keys = true;
    config.inject_kernel_owners = true;
    if (analysis_chain_run(&pass->input.set, pass->input.key_events, pass->input.key_event_count, &config,
                           &pass->library) != ANALYSIS_CHAIN_OK) {
        return false;
    }
    if ((pass->options.stages & ANALYSIS_CHAIN_STAGE_ALL) != ANALYSIS_CHAIN_STAGE_ALL) {
        return true; /* a partial chain: the results are the chain's, the report stays empty */
    }
    return segmentation_view(pass, analysis_chain_segments(pass->library), false, &pass->segmentation) &&
           segmentation_view(pass, analysis_chain_voice_segments(pass->library), true, &pass->voice_segmentation) &&
           collect_pass(pass);
}

void analyzer_pass_destroy(AnalyzerPass *pass) {
    chord_estimates_destroy(pass->chords, pass->segmentation.segment_count);
    roman_numerals_destroy(&pass->romans);
    form_stage_destroy(&pass->form);
    motive_stage_destroy(&pass->motives);
    harmony_stage_destroy(&pass->harmony);
    analysis_report_destroy(&pass->report);
    segmentation_view_destroy(&pass->segmentation, false);
    segmentation_view_destroy(&pass->voice_segmentation, true);
    analysis_chain_destroy(pass->library);
    chain_input_destroy(&pass->input);
    memset(pass, 0, sizeof(*pass));
}
