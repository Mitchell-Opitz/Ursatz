#include "ursatz_chain/analysis_chain.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "interpretation/chord.h"
#include "outer_scale_degrees.h"
#include "segmentation_beat_grid.h"
#include "time/time_span.h"
#include "ursatz_chain/form_bridge.h"

/* The results of the stages from the key regions to the form, which run twice when the keys are refined. */
typedef struct {
    CommonPracticeKeyRegions regions;
    bool have_regions;
    CommonPracticeWeightedResult *chords; /* one per segment */
    StandardTonalityKeyRegionInput *region_inputs;
    StandardTonalitySegmentInput *segment_inputs;
    int *segment_pitches; /* the members' pitches, segment after segment, for the chord bass */
    StandardTonalityChordInput *tonal_inputs;
    size_t tonal_count;
    StandardTonalityRomanResult *romans;
    ChainHarmonyInputs harmony;
    bool have_harmony;
    HarmonySegment *prepared;
    HarmonyCadences cadences;
    bool have_cadences;
    ChainFormInput form_input;
    bool have_form_input;
    StandardFormPhrases phrases;
    bool have_phrases;
    StandardFormSegmentation ideas;
    bool have_ideas;
    StandardFormThemes themes;
    bool have_themes;
    StandardFormExpansions expansions;
    bool have_expansions;
} Pass;

struct AnalysisChain {
    AnalysisChainConfig config;
    unsigned stages;
    const AnalysisNoteSet *notes;
    const AnalysisKeyEvent *key_events;
    size_t key_event_count;

    ChainPitches pitches;
    bool have_pitches;
    ChainSegments segs;
    bool have_segs;
    size_t count; /* the segments inside the scope */
    ChainSegments voice_segs;
    bool have_voice_segs;
    OuterVoiceLines lines;
    bool have_lines;
    CommonPracticeKeySegment *key_segments;
    MusicalTime **boundaries; /* the second pass: the phrase ends of the first, owned copies */
    size_t boundary_count;
    int home;

    Pass pass;

    ChainFormInput motive_input;
    bool have_motive_input;
    StandardFormPhrases motive_phrases;
    bool have_motive_phrases;
    StandardFormSegmentation motive_ideas;
    bool have_motive_ideas;
    StandardFormThemes motive_themes;
    bool have_motive_themes;
    StandardFormMotives motives;
    bool have_motives;

    ChainMeterInput meter_input;
    bool have_meter_input;
    AnalysisChainMeter meter;
    bool have_syncopations;
    bool have_hemiolas;
    bool have_hypermeter;
    bool have_densities;

    ChainTextureInput texture_input;
    bool have_texture_input;
    StandardTextureAnalysis texture;
    bool have_texture;

    ChainVoiceGrid grid;
    bool have_grid;
    StandardCounterpointPreparedGrid prepared_grid;
    bool have_prepared_grid;
    AnalysisChainVoiceLeading voice_leading;
    bool have_motions;
    bool have_perfects;
    bool have_resolutions;

    AnalysisChainMelody melody;
    bool have_melody_line;
    bool have_intervals;
    bool have_summaries;
    bool have_climaxes;
    bool have_recoveries;
    bool have_counts;

    ChainGalantSegments galant;
    bool have_galant;
    GalantSchemaMatches schemas;
    bool have_schemas;
    ScaleSequenceMatches sequences;
    bool have_sequences;
};

AnalysisChainConfig analysis_chain_config_default(void) {
    AnalysisChainConfig config;
    memset(&config, 0, sizeof(config));
    config.stages = ANALYSIS_CHAIN_STAGE_ALL;
    config.segmentation = adaptive_segmentation_config_default();
    config.weighting = common_practice_weighted_config_default();
    config.key_regions = common_practice_key_regions_config_default();
    config.outer_voices = outer_voice_config_default();
    config.cadences = harmony_cadence_config_default();
    config.form = standard_form_config_default();
    config.meter = standard_meter_config_default();
    config.texture = standard_texture_config_default();
    config.resolutions = harmony_resolution_config_default();
    config.melody = standard_counterpoint_melody_config_default();
    config.schemas = galant_match_config_default();
    config.sequences = scale_detect_config_default();
    config.refine_keys = true;
    config.first_measure = 1;
    config.last_measure = 0;
    config.home_key_index = -1;
    return config;
}

const char *analysis_chain_status_name(AnalysisChainStatus status) {
    static const char *const NAMES[] = {"ok",     "null argument", "bad config", "notes",   "segments", "keys",         "chords",
                                        "numerals", "outer voices", "cadences",  "form",    "meter",    "texture",      "voice leading",
                                        "melody", "schemas",       "alloc"};
    return status >= 0 && (size_t)status < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[status] : "unknown";
}

/* A stage pulls in the stages it needs. */
static unsigned stage_closure(unsigned stages) {
    unsigned before;
    do {
        before = stages;
        if ((stages & ANALYSIS_CHAIN_STAGE_KEYS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_CHORDS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_KEYS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_NUMERALS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CHORDS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_OUTER_VOICES) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_CADENCES) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_NUMERALS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_FORM) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CADENCES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_METER) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_FORM | ANALYSIS_CHAIN_STAGE_CHORDS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_VOICE_LEADING) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CADENCES | ANALYSIS_CHAIN_STAGE_SEGMENTS;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_MELODY) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_CHORDS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
        if ((stages & ANALYSIS_CHAIN_STAGE_SCHEMAS) != 0) {
            stages |= ANALYSIS_CHAIN_STAGE_NUMERALS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
        }
    } while (stages != before);
    return stages;
}

static bool wants(const AnalysisChain *chain, unsigned stage) {
    return (chain->stages & stage) != 0;
}

/* --- destroying --- */

static void pass_destroy(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    if (p->have_expansions) {
        standard_form_expansions_destroy(&p->expansions);
    }
    if (p->have_themes) {
        standard_form_themes_destroy(&p->themes);
    }
    if (p->have_ideas) {
        standard_form_segmentation_destroy(&p->ideas);
    }
    if (p->have_phrases) {
        standard_form_phrases_destroy(&p->phrases);
    }
    if (p->have_form_input) {
        chain_form_input_destroy(&p->form_input);
    }
    if (p->have_cadences) {
        harmony_cadences_destroy(&p->cadences);
    }
    harmony_analysis_destroy(p->prepared);
    if (p->have_harmony) {
        chain_harmony_inputs_destroy(&p->harmony);
    }
    if (p->romans != NULL) {
        standard_tonality_roman_results_destroy(p->romans, p->tonal_count);
    }
    standard_tonality_chord_inputs_destroy(p->tonal_inputs);
    free(p->segment_inputs);
    free(p->segment_pitches);
    free(p->region_inputs);
    if (p->chords != NULL) {
        for (size_t s = 0; s < chain->count; s++) {
            common_practice_weighted_result_destroy(&p->chords[s]);
        }
        free(p->chords);
    }
    if (p->have_regions) {
        common_practice_key_regions_destroy(&p->regions);
    }
    memset(p, 0, sizeof(*p));
}

void analysis_chain_destroy(AnalysisChain *chain) {
    if (chain == NULL) {
        return;
    }
    if (chain->have_sequences) {
        scale_sequence_matches_destroy(&chain->sequences);
    }
    if (chain->have_schemas) {
        galant_schema_matches_destroy(&chain->schemas);
    }
    if (chain->have_galant) {
        chain_galant_segments_destroy(&chain->galant);
    }
    if (chain->have_counts) {
        standard_counterpoint_nct_count_list_destroy(&chain->melody.counts);
    }
    if (chain->have_recoveries) {
        standard_counterpoint_leap_recoveries_destroy(&chain->melody.recoveries);
    }
    if (chain->have_climaxes) {
        standard_counterpoint_climaxes_destroy(&chain->melody.climaxes);
    }
    if (chain->have_summaries) {
        standard_counterpoint_contour_summaries_destroy(&chain->melody.summaries);
    }
    if (chain->have_intervals) {
        standard_counterpoint_melodic_intervals_destroy(&chain->melody.intervals);
    }
    if (chain->have_melody_line) {
        chain_melody_line_destroy(&chain->melody.line);
    }
    if (chain->have_resolutions) {
        harmony_resolutions_destroy(&chain->voice_leading.resolutions);
    }
    if (chain->have_perfects) {
        standard_counterpoint_violations_destroy(&chain->voice_leading.perfects);
    }
    if (chain->have_motions) {
        standard_counterpoint_motions_destroy(&chain->voice_leading.motions);
    }
    if (chain->have_prepared_grid) {
        standard_counterpoint_grid_destroy(&chain->prepared_grid);
    }
    if (chain->have_grid) {
        chain_voice_grid_destroy(&chain->grid);
    }
    if (chain->have_texture) {
        standard_texture_analysis_destroy(&chain->texture);
    }
    if (chain->have_texture_input) {
        chain_texture_input_destroy(&chain->texture_input);
    }
    if (chain->have_densities) {
        standard_meter_densities_destroy(&chain->meter.densities);
    }
    if (chain->have_hypermeter) {
        standard_meter_hypermeter_destroy(&chain->meter.hypermeter);
    }
    if (chain->have_hemiolas) {
        standard_meter_hemiolas_destroy(&chain->meter.hemiolas);
    }
    if (chain->have_syncopations) {
        standard_meter_syncopations_destroy(&chain->meter.syncopations);
    }
    if (chain->have_meter_input) {
        chain_meter_input_destroy(&chain->meter_input);
    }
    if (chain->have_motives) {
        standard_form_motives_destroy(&chain->motives);
    }
    if (chain->have_motive_themes) {
        standard_form_themes_destroy(&chain->motive_themes);
    }
    if (chain->have_motive_ideas) {
        standard_form_segmentation_destroy(&chain->motive_ideas);
    }
    if (chain->have_motive_phrases) {
        standard_form_phrases_destroy(&chain->motive_phrases);
    }
    if (chain->have_motive_input) {
        chain_form_input_destroy(&chain->motive_input);
    }
    pass_destroy(chain);
    for (size_t i = 0; i < chain->boundary_count; i++) {
        musical_time_destroy(chain->boundaries[i]);
    }
    free(chain->boundaries);
    chain_key_segments_destroy(chain->key_segments);
    if (chain->have_lines) {
        outer_voice_lines_destroy(&chain->lines);
    }
    if (chain->have_voice_segs) {
        chain_segments_destroy(&chain->voice_segs);
    }
    if (chain->have_segs) {
        chain_segments_destroy(&chain->segs);
    }
    if (chain->have_pitches) {
        chain_pitches_destroy(&chain->pitches);
    }
    free(chain);
}

/* --- the stages --- */

static double time_double(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

/* The region holding the segment that starts at `start` whole notes: the last whose start is not after it. */
static size_t region_at(const CommonPracticeKeyRegions *regions, double start) {
    size_t found = 0;
    for (size_t i = 0; i < regions->region_count; i++) {
        if (time_double(time_span_start(regions->regions[i].span)) <= start + 1e-12) {
            found = i;
        }
    }
    return found;
}

/* Weighted chord identification in each region's own key and mode (a minor region in the minor vocabulary). */
static bool identify_chords(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    size_t count = chain->count;
    p->chords = calloc(count, sizeof(CommonPracticeWeightedResult));
    if (p->chords == NULL) {
        return false;
    }
    size_t s = 0;
    while (s < count) {
        size_t region = region_at(&p->regions, chain->segs.starts[s]);
        size_t end = s + 1;
        while (end < count && region_at(&p->regions, chain->segs.starts[end]) == region) {
            end++;
        }
        int key_index = p->regions.regions[region].key_index;
        CommonPracticeWeightedResults group = {0};
        if (common_practice_identify_weighted_sequence(&chain->segs.segments[s], end - s, common_practice_key_index_tonic(key_index),
                                                       common_practice_key_index_mode(key_index), &chain->config.weighting,
                                                       &group) != COMMON_PRACTICE_WEIGHTED_OK) {
            return false;
        }
        for (size_t i = 0; i < group.count; i++) {
            p->chords[s + i] = group.items[i]; /* ownership moves to the pass */
        }
        free(group.items);
        s = end;
    }
    return true;
}

/* Roman numerals in the chain's keys. */
static bool read_numerals(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    size_t count = chain->count;
    p->region_inputs = calloc(p->regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    p->segment_inputs = calloc(count, sizeof(StandardTonalitySegmentInput));
    size_t pitch_total = 0;
    for (size_t i = 0; i < count; i++) {
        pitch_total += chain->segs.segments[i].member_count;
    }
    p->segment_pitches = calloc(pitch_total > 0 ? pitch_total : 1, sizeof(int));
    if (p->region_inputs == NULL || p->segment_inputs == NULL || p->segment_pitches == NULL) {
        return false;
    }
    pitch_total = 0;
    for (size_t i = 0; i < p->regions.region_count; i++) {
        p->region_inputs[i].key = p->regions.regions[i].key;
        p->region_inputs[i].start = time_span_start(p->regions.regions[i].span);
        p->region_inputs[i].uncertainty = p->regions.regions[i].uncertainty;
    }
    for (size_t i = 0; i < count; i++) {
        p->segment_inputs[i].segment_id = chain->segs.ids[i];
        p->segment_inputs[i].start = chain->key_segments[i].start;
        p->segment_inputs[i].chord = p->chords[i].matched ? p->chords[i].best_chord : NULL;
        p->segment_inputs[i].weights = chain->key_segments[i].weights.pitch_class;
        p->segment_inputs[i].pitches = &p->segment_pitches[pitch_total];
        p->segment_inputs[i].pitch_count = chain->segs.segments[i].member_count;
        for (size_t j = 0; j < chain->segs.segments[i].member_count; j++) {
            p->segment_pitches[pitch_total++] = chain->segs.segments[i].members[j].pitch;
        }
    }
    return standard_tonality_build_chord_inputs(p->segment_inputs, count, p->region_inputs, p->regions.region_count,
                                                &p->tonal_inputs, &p->tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK &&
           standard_tonality_build_roman_numerals(p->tonal_inputs, p->tonal_count, &p->romans) == STANDARD_TONALITY_ROMAN_OK;
}

static int resolved_home(const AnalysisChain *chain) {
    if (chain->config.home_key_index >= 0) {
        return chain->config.home_key_index;
    }
    return chain->pass.have_regions && chain->pass.regions.region_count > 0 ? chain->pass.regions.regions[0].key_index : -1;
}

static AnalysisChainStatus cadence_stage(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    size_t count = chain->count;
    if (chain_build_harmony_segments(&chain->segs, count, chain->notes->map, p->tonal_inputs, p->romans, &chain->lines,
                                     &p->harmony) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_CADENCES;
    }
    p->have_harmony = true;
    if (harmony_analysis_prepare(p->harmony.inputs, count, &p->prepared) != HARMONY_ANALYSIS_OK ||
        harmony_analysis_find_cadences(p->prepared, count, &chain->config.cadences, &p->cadences) != HARMONY_CADENCE_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_CADENCES;
    }
    p->have_cadences = true;
    chain->home = resolved_home(chain);
    if (chain_add_boundary_cadences(p->prepared, count, &p->regions, chain->home, &p->cadences) != CHAIN_INPUT_OK ||
        chain_confirm_key_changes(&p->regions, chain->segs.starts, &p->cadences) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_CADENCES;
    }
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus form_stage(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    int last = chain->config.last_measure > 0 ? chain->config.last_measure : INT_MAX;
    if (chain_build_form_input(chain->notes, &chain->segs, &chain->lines, chain->config.first_measure, last, &p->cadences,
                               &p->form_input) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    p->have_form_input = true;
    const StandardFormInput *input = &p->form_input.input;
    if (standard_form_segment_phrases(input, &chain->config.form, chain_form_boundary_detect, NULL, &p->phrases) !=
        STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    p->have_phrases = true;
    if (standard_form_segment_ideas(input, &p->phrases, &chain->config.form, &p->ideas) != STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    p->have_ideas = true;
    if (standard_form_identify_themes(input, &p->phrases, &p->ideas, &chain->config.form, &p->themes) != STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    p->have_themes = true;
    if (standard_form_detect_expansions(input, &p->phrases, &p->ideas, &p->themes, &chain->config.form, &p->expansions) !=
        STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    p->have_expansions = true;
    return ANALYSIS_CHAIN_OK;
}

/* The stages from the key regions to the form, over the segments. A pass can be run again after pass_destroy. */
static AnalysisChainStatus run_pass(AnalysisChain *chain, const MusicalTime *const *boundaries, size_t boundary_count) {
    Pass *p = &chain->pass;
    if (wants(chain, ANALYSIS_CHAIN_STAGE_KEYS)) {
        CommonPracticeKeyRegionsConfig key_config = chain->config.key_regions;
        key_config.phrase_boundaries = boundaries;
        key_config.phrase_boundary_count = boundary_count;
        if (chain_estimate_key_regions(chain->key_segments, chain->count, chain->notes->map, chain->key_events,
                                       chain->key_event_count, &key_config, &p->regions) != CHAIN_INPUT_OK) {
            return ANALYSIS_CHAIN_ERR_KEYS;
        }
        p->have_regions = true;
        chain->home = resolved_home(chain);
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_CHORDS) && !identify_chords(chain)) {
        return ANALYSIS_CHAIN_ERR_CHORDS;
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_NUMERALS) && !read_numerals(chain)) {
        return ANALYSIS_CHAIN_ERR_NUMERALS;
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_CADENCES)) {
        AnalysisChainStatus status = cadence_stage(chain);
        if (status != ANALYSIS_CHAIN_OK) {
            return status;
        }
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_FORM)) {
        return form_stage(chain);
    }
    return ANALYSIS_CHAIN_OK;
}

/* The motive families, on a note-level soprano with its own phrases, ideas and themes. */
static AnalysisChainStatus motive_stage(AnalysisChain *chain) {
    int last = chain->config.last_measure > 0 ? chain->config.last_measure : INT_MAX;
    int home = chain->home;
    if (home < 0 ||
        chain_build_motive_input(chain->notes, &chain->segs, chain->config.first_measure, last, common_practice_key_index_tonic(home),
                                 common_practice_key_index_mode(home), &chain->pass.cadences, &chain->motive_input) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    chain->have_motive_input = true;
    const StandardFormInput *input = &chain->motive_input.input;
    if (standard_form_segment_phrases(input, &chain->config.form, chain_form_boundary_detect, NULL, &chain->motive_phrases) !=
        STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    chain->have_motive_phrases = true;
    if (standard_form_segment_ideas(input, &chain->motive_phrases, &chain->config.form, &chain->motive_ideas) !=
        STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    chain->have_motive_ideas = true;
    if (standard_form_identify_themes(input, &chain->motive_phrases, &chain->motive_ideas, &chain->config.form,
                                      &chain->motive_themes) != STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    chain->have_motive_themes = true;
    if (standard_form_find_motives(input, &chain->motive_phrases, &chain->motive_ideas, &chain->motive_themes, &chain->config.form,
                                   chain_form_match_windows, NULL, &chain->motives) != STANDARD_FORM_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_FORM;
    }
    chain->have_motives = true;
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus meter_stage(AnalysisChain *chain) {
    Pass *p = &chain->pass;
    const char **labels = calloc(chain->segs.count > 0 ? chain->segs.count : 1, sizeof(char *));
    if (labels == NULL) {
        return ANALYSIS_CHAIN_ERR_ALLOC;
    }
    for (size_t s = 0; s < chain->count; s++) {
        labels[s] = p->chords[s].matched ? chord_label(p->chords[s].best_chord) : NULL;
    }
    int last = chain->config.last_measure > 0 ? chain->config.last_measure : INT_MAX;
    ChainInputStatus built = chain_build_meter_input(chain->notes, &chain->lines, &chain->segs, labels, chain->config.first_measure,
                                                     last, &p->form_input.input, &p->phrases, &p->expansions, &chain->meter_input);
    free(labels);
    if (built != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_METER;
    }
    chain->have_meter_input = true;
    const StandardMeterInput *input = &chain->meter_input.input;
    if (standard_meter_find_syncopations(input, &chain->config.meter, &chain->meter.syncopations) != STANDARD_METER_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_METER;
    }
    chain->have_syncopations = true;
    if (standard_meter_find_hemiolas(input, &chain->config.meter, &chain->meter.hemiolas) != STANDARD_METER_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_METER;
    }
    chain->have_hemiolas = true;
    if (standard_meter_find_hypermeter(input, &chain->config.meter, &chain->meter.hypermeter) != STANDARD_METER_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_METER;
    }
    chain->have_hypermeter = true;
    if (standard_meter_find_densities(input, &chain->config.meter, &chain->meter.densities) != STANDARD_METER_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_METER;
    }
    chain->have_densities = true;
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus texture_stage(AnalysisChain *chain) {
    if (chain_build_texture_input(chain->notes, &chain->pitches, &chain->texture_input) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_TEXTURE;
    }
    chain->have_texture_input = true;
    if (standard_texture_analyze(&chain->texture_input.input, &chain->config.texture, &chain->texture) !=
        STANDARD_TEXTURE_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_TEXTURE;
    }
    chain->have_texture = true;
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus voice_leading_stage(AnalysisChain *chain) {
    if (chain_build_voice_grid(chain->notes, &chain->pitches, &chain->segs, chain->count, &chain->grid) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_VOICE_LEADING;
    }
    chain->have_grid = true;
    chain->voice_leading.voice_count = chain->grid.grid.voice_count;
    if (standard_counterpoint_grid_prepare(&chain->grid.grid, &chain->prepared_grid) != STANDARD_COUNTERPOINT_GRID_OK) {
        return ANALYSIS_CHAIN_ERR_VOICE_LEADING;
    }
    chain->have_prepared_grid = true;
    if (standard_counterpoint_classify_motions(&chain->prepared_grid, &chain->voice_leading.motions) !=
        STANDARD_COUNTERPOINT_MOTION_OK) {
        return ANALYSIS_CHAIN_ERR_VOICE_LEADING;
    }
    chain->have_motions = true;
    if (standard_counterpoint_check_perfects(&chain->prepared_grid, &chain->voice_leading.perfects) !=
        STANDARD_COUNTERPOINT_PERFECTS_OK) {
        return ANALYSIS_CHAIN_ERR_VOICE_LEADING;
    }
    chain->have_perfects = true;
    if (harmony_analysis_check_resolutions(chain->pass.prepared, chain->count, &chain->grid.grid, &chain->config.resolutions,
                                           &chain->voice_leading.resolutions) != HARMONY_RESOLUTION_OK) {
        return ANALYSIS_CHAIN_ERR_VOICE_LEADING;
    }
    chain->have_resolutions = true;
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus melody_stage(AnalysisChain *chain) {
    int home = chain->home;
    if (home < 0 ||
        chain_build_melody_line(&chain->segs, chain->count, chain->lines.soprano, chain->pass.chords,
                                common_practice_key_index_tonic(home), common_practice_key_index_mode(home),
                                &chain->melody.line) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_melody_line = true;
    const StandardCounterpointMelodyLine *line = &chain->melody.line.line;
    if (line->count == 0) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    StandardCounterpointMelodySpan span = {"line", 0, line->count - 1};
    if (standard_counterpoint_melody_intervals(line, &chain->config.melody, &chain->melody.intervals) !=
        STANDARD_COUNTERPOINT_MELODY_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_intervals = true;
    if (standard_counterpoint_melody_contour_summaries(line, &chain->melody.intervals, &span, 1, &chain->melody.summaries) !=
        STANDARD_COUNTERPOINT_MELODY_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_summaries = true;
    if (standard_counterpoint_melody_climaxes(line, &chain->melody.intervals, &span, 1, &chain->melody.climaxes) !=
        STANDARD_COUNTERPOINT_MELODY_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_climaxes = true;
    if (standard_counterpoint_melody_leap_recoveries(line, &chain->melody.intervals, &chain->melody.recoveries) !=
        STANDARD_COUNTERPOINT_MELODY_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_recoveries = true;
    if (standard_counterpoint_melody_nct_counts(line, &span, 1, &chain->melody.counts) != STANDARD_COUNTERPOINT_MELODY_OK) {
        return ANALYSIS_CHAIN_ERR_MELODY;
    }
    chain->have_counts = true;
    return ANALYSIS_CHAIN_OK;
}

static AnalysisChainStatus schema_stage(AnalysisChain *chain) {
    if (chain->home < 0 ||
        chain_build_galant_segments(&chain->segs, chain->count, chain->notes->map, &chain->lines, chain->home,
                                    chain->pass.tonal_inputs, chain->pass.romans, &chain->galant) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_SCHEMAS;
    }
    chain->have_galant = true;
    if (galant_schema_match(&chain->galant.list, &chain->config.schemas, &chain->schemas) != GALANT_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_SCHEMAS;
    }
    chain->have_schemas = true;
    if (scale_sequence_detect(&chain->galant.list, &chain->config.sequences, &chain->sequences) != GALANT_ANALYSIS_OK) {
        return ANALYSIS_CHAIN_ERR_SCHEMAS;
    }
    chain->have_sequences = true;
    return ANALYSIS_CHAIN_OK;
}

/* The segments that start before the end of the scope's last measure. */
static bool scope_count(AnalysisChain *chain) {
    chain->count = chain->segs.count;
    if (chain->config.last_measure <= 0) {
        return chain->count > 0;
    }
    TimeSpan *last = NULL;
    if (meter_map_measure_span(chain->notes->map, chain->config.last_measure, &last) != METER_MAP_OK) {
        return false;
    }
    double end = time_double(time_span_end(last));
    time_span_destroy(last);
    size_t count = 0;
    while (count < chain->segs.count && chain->segs.starts[count] < end - 1e-12) {
        count++;
    }
    chain->count = count;
    return count > 0;
}

static AnalysisChainStatus prepare(AnalysisChain *chain) {
    if (wants(chain, ANALYSIS_CHAIN_STAGE_OUTER_VOICES | ANALYSIS_CHAIN_STAGE_TEXTURE | ANALYSIS_CHAIN_STAGE_VOICE_LEADING)) {
        if (chain_build_pitches(chain->notes, &chain->pitches) != CHAIN_INPUT_OK) {
            return ANALYSIS_CHAIN_ERR_NOTES;
        }
        chain->have_pitches = true;
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_SEGMENTS)) {
        if (chain_build_segments(chain->notes, &chain->config.segmentation, &chain->segs) != CHAIN_INPUT_OK) {
            return ANALYSIS_CHAIN_ERR_SEGMENTS;
        }
        chain->have_segs = true;
        if (chain_build_members(chain->notes, &chain->segs) != CHAIN_INPUT_OK) {
            return ANALYSIS_CHAIN_ERR_SEGMENTS;
        }
        if (!scope_count(chain)) {
            return ANALYSIS_CHAIN_ERR_NOTES;
        }
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_OUTER_VOICES)) {
        AdaptiveSegmentationConfig voice_config = chain->config.segmentation;
        voice_config.merge_unchanged_harmony = false;
        if (chain_build_segments(chain->notes, &voice_config, &chain->voice_segs) != CHAIN_INPUT_OK) {
            return ANALYSIS_CHAIN_ERR_OUTER_VOICES;
        }
        chain->have_voice_segs = true;
        if (outer_voice_extract(&chain->voice_segs.sonorities, chain->pitches.events, chain->pitches.values, chain->notes->count,
                                chain->notes->map, standard_meter_segmentation_beat_grid, NULL, &chain->config.outer_voices,
                                &chain->lines) != OUTER_VOICE_OK) {
            return ANALYSIS_CHAIN_ERR_OUTER_VOICES;
        }
        chain->have_lines = true;
    }
    if (wants(chain, ANALYSIS_CHAIN_STAGE_KEYS) &&
        chain_build_key_segments(&chain->segs, chain->count, &chain->config.weighting, &chain->key_segments) != CHAIN_INPUT_OK) {
        return ANALYSIS_CHAIN_ERR_KEYS;
    }
    return ANALYSIS_CHAIN_OK;
}

static bool valid_config(const AnalysisChainConfig *config) {
    return config->stages != 0 && (config->stages & ~ANALYSIS_CHAIN_STAGE_ALL) == 0 && config->home_key_index < COMMON_PRACTICE_KEY_COUNT &&
           config->first_measure >= 0 && config->last_measure >= 0 && standard_form_config_valid(&config->form) &&
           standard_meter_config_valid(&config->meter);
}

/* The phrase ends of the first pass as the boundaries of the second. */
static bool phrase_boundaries(AnalysisChain *chain) {
    const StandardFormPhrases *phrases = &chain->pass.phrases;
    if (phrases->count <= 1) {
        return true;
    }
    chain->boundaries = calloc(phrases->count, sizeof(MusicalTime *));
    if (chain->boundaries == NULL) {
        return false;
    }
    for (size_t k = 0; k + 1 < phrases->count; k++) {
        const MusicalTime *end = time_span_end(phrases->items[k].span);
        if (musical_time_create(musical_time_numerator(end), musical_time_denominator(end), &chain->boundaries[chain->boundary_count]) !=
            MUSICAL_TIME_OK) {
            return false;
        }
        chain->boundary_count++;
    }
    return true;
}

AnalysisChainStatus analysis_chain_run(const AnalysisNoteSet *notes, const AnalysisKeyEvent *key_events, size_t key_event_count,
                                       const AnalysisChainConfig *config, AnalysisChain **out) {
    if (notes == NULL || out == NULL || (key_event_count > 0 && key_events == NULL)) {
        return ANALYSIS_CHAIN_ERR_NULL_ARGUMENT;
    }
    AnalysisChainConfig defaults = analysis_chain_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (!valid_config(config)) {
        return ANALYSIS_CHAIN_ERR_BAD_CONFIG;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return ANALYSIS_CHAIN_ERR_NOTES;
    }
    AnalysisChain *chain = calloc(1, sizeof(AnalysisChain));
    if (chain == NULL) {
        return ANALYSIS_CHAIN_ERR_ALLOC;
    }
    chain->config = *config;
    chain->stages = stage_closure(config->stages);
    chain->notes = notes;
    chain->key_events = key_events;
    chain->key_event_count = key_event_count;
    chain->home = config->home_key_index;

    AnalysisChainStatus status = prepare(chain);
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_KEYS)) {
        status = run_pass(chain, NULL, 0);
        /* The keys depend on the phrases, which depend on the cadences, which depend on the keys: the chords of the first pass are
           evidence for the keys and its phrase ends their boundaries, and everything from the keys on runs once more. */
        bool from_phrases = wants(chain, ANALYSIS_CHAIN_STAGE_FORM);
        if (status == ANALYSIS_CHAIN_OK && chain->config.refine_keys && (from_phrases || chain->config.refine_with_given_boundaries)) {
            for (size_t s = 0; s < chain->count; s++) {
                chain->key_segments[s].chord = common_practice_key_chord_of(&chain->pass.chords[s]);
            }
            if (from_phrases && !phrase_boundaries(chain)) {
                status = ANALYSIS_CHAIN_ERR_FORM;
            } else {
                /* the boundaries are the phrase ends of the first pass, or, with no form, the caller's own */
                const MusicalTime *const *boundaries = from_phrases ? (const MusicalTime *const *)chain->boundaries
                                                                    : chain->config.key_regions.phrase_boundaries;
                size_t boundary_count = from_phrases ? chain->boundary_count : chain->config.key_regions.phrase_boundary_count;
                pass_destroy(chain);
                status = run_pass(chain, boundaries, boundary_count);
            }
        }
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_FORM)) {
        status = motive_stage(chain);
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_METER)) {
        status = meter_stage(chain);
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_TEXTURE)) {
        status = texture_stage(chain);
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_VOICE_LEADING)) {
        status = voice_leading_stage(chain);
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_MELODY)) {
        status = melody_stage(chain);
    }
    if (status == ANALYSIS_CHAIN_OK && wants(chain, ANALYSIS_CHAIN_STAGE_SCHEMAS)) {
        status = schema_stage(chain);
    }
    if (status != ANALYSIS_CHAIN_OK) {
        analysis_chain_destroy(chain);
        return status;
    }
    *out = chain;
    return ANALYSIS_CHAIN_OK;
}

/* --- accessors --- */

size_t analysis_chain_segment_count(const AnalysisChain *chain) {
    return chain != NULL ? chain->count : 0;
}

const ChainSegments *analysis_chain_segments(const AnalysisChain *chain) {
    return chain != NULL && chain->have_segs ? &chain->segs : NULL;
}

int analysis_chain_home_key(const AnalysisChain *chain) {
    return chain != NULL ? chain->home : -1;
}

const CommonPracticeKeyRegions *analysis_chain_key_regions(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_regions ? &chain->pass.regions : NULL;
}

const CommonPracticeWeightedResult *analysis_chain_chords(const AnalysisChain *chain) {
    return chain != NULL ? chain->pass.chords : NULL;
}

const StandardTonalityChordInput *analysis_chain_chord_inputs(const AnalysisChain *chain) {
    return chain != NULL ? chain->pass.tonal_inputs : NULL;
}

const StandardTonalityRomanResult *analysis_chain_numerals(const AnalysisChain *chain) {
    return chain != NULL ? chain->pass.romans : NULL;
}

const ChainSegments *analysis_chain_voice_segments(const AnalysisChain *chain) {
    return chain != NULL && chain->have_voice_segs ? &chain->voice_segs : NULL;
}

const OuterVoiceLines *analysis_chain_outer_voices(const AnalysisChain *chain) {
    return chain != NULL && chain->have_lines ? &chain->lines : NULL;
}

const HarmonyCadences *analysis_chain_cadences(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_cadences ? &chain->pass.cadences : NULL;
}

const StandardFormPhrases *analysis_chain_phrases(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_phrases ? &chain->pass.phrases : NULL;
}

const StandardFormSegmentation *analysis_chain_ideas(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_ideas ? &chain->pass.ideas : NULL;
}

const StandardFormThemes *analysis_chain_themes(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_themes ? &chain->pass.themes : NULL;
}

const StandardFormExpansions *analysis_chain_expansions(const AnalysisChain *chain) {
    return chain != NULL && chain->pass.have_expansions ? &chain->pass.expansions : NULL;
}

const StandardFormMotives *analysis_chain_motives(const AnalysisChain *chain) {
    return chain != NULL && chain->have_motives ? &chain->motives : NULL;
}

const AnalysisChainMeter *analysis_chain_meter(const AnalysisChain *chain) {
    return chain != NULL && chain->have_densities ? &chain->meter : NULL;
}

const StandardTextureAnalysis *analysis_chain_texture(const AnalysisChain *chain) {
    return chain != NULL && chain->have_texture ? &chain->texture : NULL;
}

const AnalysisChainVoiceLeading *analysis_chain_voice_leading(const AnalysisChain *chain) {
    return chain != NULL && chain->have_resolutions ? &chain->voice_leading : NULL;
}

const AnalysisChainMelody *analysis_chain_melody(const AnalysisChain *chain) {
    return chain != NULL && chain->have_counts ? &chain->melody : NULL;
}

const GalantSchemaMatches *analysis_chain_schemas(const AnalysisChain *chain) {
    return chain != NULL && chain->have_schemas ? &chain->schemas : NULL;
}

const ScaleSequenceMatches *analysis_chain_sequences(const AnalysisChain *chain) {
    return chain != NULL && chain->have_sequences ? &chain->sequences : NULL;
}
