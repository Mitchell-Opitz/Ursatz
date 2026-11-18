#include "chain/chain_collect.h"
#include "chain/analysis_chain_internal.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/key_regions.h"
#include "model/time/time_span.h"
#include "theory/tonality/common_practice_materials.h"
#include "chain/chain_result_form_plan.h"
#include "chain/chain_result_harmonic_rhythm.h"
#include "chain/chain_result_key_regions.h"
#include "chain/chain_result_melody.h"
#include "chain/chain_result_meter_check.h"
#include "chain/chain_result_outer_voices.h"
#include "chain/chain_result_progressions.h"
#include "chain/chain_result_rhythm.h"
#include "chain/chain_result_schemas.h"
#include "chain/chain_result_markings.h"
#include "chain/chain_result_texture.h"
#include "chain/chain_result_voice_leading.h"
#include "chain/chain_result_numerals.h"
#include "chain/chain_result_spelling.h"

static const AnalysisChordReading NO_READING = {NULL, NULL, NULL, NULL, -1, 0.0, {false, 0, 0}};

/* A Segmentation that views the chain's segments: the exact windows from the library's sonorities and the members' notes
   as ImportedScore indices. The library's segments, ids and sonorities are borrowed; the voice segments carry no
   members in the chain, so theirs are built here from the sonority's events. */
static bool segmentation_view(const ChainCollect *pass, const ChainSegments *source, bool with_members,
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
        segment->start_numerator = (long)window->start.value.num;
        segment->start_denominator = (unsigned long)window->start.value.den;
        segment->end_numerator = (long)window->end.value.num;
        segment->end_denominator = (unsigned long)window->end.value.den;
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
            size_t index = chain_score_input_import_index(&pass->input, id);
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
static bool respell_chords(ChainCollect *chain) {
    const Segmentation *segmentation = &chain->segmentation;
    for (size_t i = 0; i < segmentation->segment_count; i++) {
        if (!respell_chord_reading(&chain->chords[i].best, chain->notes, &segmentation->segments[i]) ||
            !respell_chord_reading(&chain->chords[i].runner_up, chain->notes, &segmentation->segments[i])) {
            return false;
        }
    }
    AnalysisKeyChange *changes = chain->report->key_changes;
    for (size_t c = 0; c < chain->report->key_change_count; c++) {
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
static bool build_entries(ChainCollect *chain) {
    const Segmentation *segmentation = &chain->segmentation;
    size_t count = segmentation->segment_count;
    ChainResult *report = chain->report;
    report->entries = calloc(count, sizeof(ChainEntry));
    if (!report->entries) {
        return false;
    }
    report->entry_count = count;
    for (size_t i = 0; i < count; i++) {
        const Segment *segment = &segmentation->segments[i];
        ChordEstimate *chord = &chain->chords[i];
        ChainEntry *entry = &report->entries[i];
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
static bool collect_non_chord_tones(ChainCollect *chain) {
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
    chain->report->non_chord_tones = tones;
    chain->report->non_chord_tone_count = tone_count;
    return true;
}

/* The home key: the library's key with the longest total duration over its regions. */
static bool collect_pass(ChainCollect *pass) {
    const Segmentation *segmentation = &pass->segmentation;
    ChainResult *report = pass->report;
    const ImportedScore *notes = pass->notes;
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
                              lines, &pass->materials, report) ||
        !collect_non_chord_tones(pass)) {
        return false;
    }
    /* The Analyzer-only stages are switched by their bits. */
#define STAGE_ON(bit) ((pass->stages & (bit)) != 0)
#define RUN(call) \
    do {          \
        if (!(call)) { \
            return false; \
        } \
    } while (0)
#define RUN_STAGE(bit, call)       \
    do {                           \
        if (STAGE_ON(bit)) {       \
            RUN(call);             \
        }                          \
    } while (0)
    RUN(harmony_stage_prepare(pass->library, &pass->harmony));
    RUN(harmony_stage_collect_cadences(&pass->harmony, report));
    RUN(harmony_stage_collect_functions(&pass->harmony, report));
    RUN(harmony_stage_collect_six_fours_and_prolongations(&pass->harmony, report));
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_PROGRESSIONS, progression_stage_collect(report));
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_HARMONIC_RHYTHM, harmonic_rhythm_stage_collect(notes, report));
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_METER_CHECK, meter_check_stage_collect(notes, report));
    RUN(form_stage_read(&pass->form, notes, &pass->input, pass->library));
    RUN(form_stage_collect(&pass->form, report));
    RUN(form_stage_collect_repeats(&pass->form, report));
    RUN(harmony_stage_collect_phrase_model(&pass->harmony, report));
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_FORM_PLAN, form_plan_stage_collect(&pass->form, report));
    RUN(motive_stage_read(&pass->motives, &pass->input, pass->library));
    RUN(motive_stage_collect(&pass->motives, notes, report));
    RUN(melody_stage_collect(notes, &pass->input, pass->library, segmentation, report));
    /* The rhythm input reads the voice lines (analysis_chain_outer_voices), one entry per melody note: the lines the
       library chain's own meter reads (R-377), so the report's meter claims agree with the chain's. */
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_RHYTHM, rhythm_stage_collect(notes, &pass->lines, &pass->form, report));
    RUN(texture_stage_collect(notes, pass->library, report));
    RUN_STAGE(ANALYSIS_CHAIN_STAGE_MARKINGS, markings_stage_collect(notes, report));
    RUN(voice_leading_stage_collect(notes, &pass->input, pass->library, &pass->harmony, report));
    RUN(schema_stage_collect(notes, pass->library, report));
#undef RUN_STAGE
#undef RUN
#undef STAGE_ON
    return true;
}

/* An empty result: no entries, a home duration of 1. */
static void result_reset(ChainResult *result) {
    memset(result, 0, sizeof(*result));
    result->home_duration_denominator = 1;
}

static bool collect_chain(AnalysisChain *chain, ChainCollect *collect) {
    return segmentation_view(collect, analysis_chain_segments(chain), false, &collect->segmentation) &&
           segmentation_view(collect, analysis_chain_voice_segments(chain), true, &collect->voice_segmentation) &&
           collect_pass(collect);
}

AnalysisChainStatus analysis_chain_run_score(const ImportedScore *score, const AnalysisChainConfig *config,
                                             AnalysisChain **out) {
    if (score == NULL || out == NULL) {
        return ANALYSIS_CHAIN_ERR_NULL_ARGUMENT;
    }
    AnalysisChainConfig defaults = analysis_chain_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (config->knowledge == NULL || (config->stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) == 0) {
        return config->knowledge == NULL ? ANALYSIS_CHAIN_ERR_KNOWLEDGE : ANALYSIS_CHAIN_ERR_BAD_CONFIG;
    }
    ChainCollect *collect = calloc(1, sizeof(*collect));
    if (collect == NULL) {
        return ANALYSIS_CHAIN_ERR_ALLOC;
    }
    collect->notes = score;
    collect->stages = config->stages;
    char message[160];
    if (!common_practice_materials_resolve(config->knowledge, &collect->materials, message, sizeof(message))) {
        free(collect);
        return ANALYSIS_CHAIN_ERR_KNOWLEDGE;
    }
    AnalysisChain *chain = NULL;
    if (score->count == 0) {
        /* No notes: a chain with nothing run and an empty result. */
        chain = calloc(1, sizeof(*chain));
        if (chain == NULL) {
            free(collect);
            return ANALYSIS_CHAIN_ERR_ALLOC;
        }
        chain->config = *config;
        chain->materials = collect->materials;
        chain->home = -1;
    } else {
        if (!chain_score_input_build(score, &collect->materials, &collect->input)) {
            free(collect);
            return ANALYSIS_CHAIN_ERR_NOTES;
        }
        AnalysisChainStatus status = analysis_chain_run(&collect->input.set, collect->input.key_events,
                                                        collect->input.key_event_count, config, &chain);
        if (status != ANALYSIS_CHAIN_OK) {
            chain_score_input_destroy(&collect->input);
            free(collect);
            return status;
        }
    }
    chain->score = score;
    chain->collect = collect;
    collect->library = chain;
    collect->report = &chain->result;
    result_reset(&chain->result);
    if (score->count > 0 && (config->stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) == ANALYSIS_CHAIN_STAGE_PERCEPTION &&
        !collect_chain(chain, collect)) {
        analysis_chain_destroy(chain);
        return ANALYSIS_CHAIN_ERR_ALLOC;
    }
    *out = chain;
    return ANALYSIS_CHAIN_OK;
}

void chain_collect_destroy_results(AnalysisChain *chain) {
    ChainCollect *collect = chain->collect;
    if (collect == NULL) {
        return;
    }
    chord_estimates_destroy(collect->chords, collect->segmentation.segment_count);
    roman_numerals_destroy(&collect->romans);
    form_stage_destroy(&collect->form);
    motive_stage_destroy(&collect->motives);
    harmony_stage_destroy(&collect->harmony);
    chain_result_destroy(&chain->result);
    segmentation_view_destroy(&collect->segmentation, false);
    segmentation_view_destroy(&collect->voice_segmentation, true);
}

void chain_collect_destroy_input(AnalysisChain *chain) {
    if (chain->collect == NULL) {
        return;
    }
    chain_score_input_destroy(&chain->collect->input);
    free(chain->collect);
    chain->collect = NULL;
}

const ChainResult *analysis_chain_result(const AnalysisChain *chain) {
    static const ChainResult EMPTY = {.home_duration_denominator = 1};
    return chain != NULL ? &chain->result : &EMPTY;
}

const ImportedScore *analysis_chain_score(const AnalysisChain *chain) {
    return chain != NULL ? chain->score : NULL;
}

void chain_result_take(AnalysisChain *chain, ChainResult *out) {
    *out = chain->result;
    result_reset(&chain->result);
}
