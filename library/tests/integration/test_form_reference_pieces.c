/*
 * The phrase form of the five reference pieces (Bach 114, Mozart 545, Clementi 36, Field 1, Field 5) against
 * tests/fixtures/annotations/<piece>_phrase_form.txt: DRAFT hand analyses, not yet checked against the scores (see their headers),
 * read from the pitch content of the files and not from this code's output. What they list is scored; what they do not list is
 * only reported (the adapter's phrases and themes that match no annotation).
 *
 * A phrase boundary is FOUND when the adapter has a phrase whose last measure (before any extension) is within one measure of the
 * annotated one. A theme is RIGHT when the adapter has a theme of the annotated form whose first and last measures are each within
 * one measure of the annotated ones; an annotated expansion is FOUND when the adapter has a length change of the same kind whose
 * measures and extent agree within one.
 *
 * Two runs per piece, over the measures the annotation says it read:
 *   1. THE FORM ADAPTER ALONE. The cadences are the annotation's phrase ends (the arrival is the whole measure, uncertainty 0, the
 *      strength rank standard-harmony's); the soprano, the meter and the segmentation are the real ones from the L2 pipeline.
 *      Asserted over all five pieces: at least 80% of the annotated boundaries found, at least 70% of the annotated themes
 *      right, every annotated expansion of 2 or more measures found. Because the boundaries here come from the same annotation, the
 *      boundary figure shows the arithmetic of snapping and absorption, not the finding of cadences; the theme and expansion figures
 *      are the adapter's own.
 *   2. THE WHOLE CHAIN. The cadences are the cadence adapter's (standard-harmony), from the weighted chord identification, the key
 *      regions and the Roman numerals, as in test_cadences_reference_pieces.c (whose helpers this file reuses). The earlier stages
 *      miss much of the harmony, so this run is reported with its numbers and misses and asserted only at a floor set at what it
 *      measures, so that a regression shows; it is not a quality target.
 *
 * Nothing is tuned to the fixtures: every default is the library's.
 */

#define main cadence_pieces_unused_main
#include "test_cadences_reference_pieces.c"
#undef main

#include "analysis_expansion.h"
#include "analysis_ideas.h"
#include "analysis_phrase.h"
#include "analysis_theme.h"
#include "form_kernel_bridge.h"
#include "hand_cadences.h"
#include "analysis_input.h"
#include "harmony_cadence_rules.h"
#include "phrase_level_form.h"

#define FORM_TARGET_BOUNDARIES 0.80
#define FORM_TARGET_THEMES 0.70
/* The floors of the whole-chain run, under what it measured when this was written (boundaries 57.9%, themes 0%; see the PR). */
#define CHAIN_FLOOR_BOUNDARIES 0.50
#define CHAIN_FLOOR_THEMES 0.0

/* --- the annotation --- */

typedef struct {
    int measure;
    CadenceType type;
} AnnotatedEnd;

typedef struct {
    PhraseLevelForm form;
    int first;
    int last;
} AnnotatedTheme;

typedef struct {
    StandardFormExpansionKind kind;
    int first;
    int last;
    int measures;
} AnnotatedExpansion;

typedef struct {
    int range_first;
    int range_last;
    AnnotatedEnd ends[32];
    size_t end_count;
    AnnotatedTheme themes[16];
    size_t theme_count;
    AnnotatedExpansion expansions[8];
    size_t expansion_count;
} FormAnnotation;

static CadenceType cadence_of_text(const char *text) {
    if (strcmp(text, "pac") == 0) {
        return CADENCE_TYPE_PERFECT_AUTHENTIC;
    }
    if (strcmp(text, "iac") == 0) {
        return CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    }
    assert(strcmp(text, "hc") == 0);
    return CADENCE_TYPE_HALF;
}

static PhraseLevelForm form_of_text(const char *text) {
    for (int f = 0; f < (int)PHRASE_LEVEL_FORM_COUNT; f++) {
        if (strcmp(text, phrase_level_form_name((PhraseLevelForm)f)) == 0) {
            return (PhraseLevelForm)f;
        }
    }
    assert(!"unknown form in the annotation");
    return PHRASE_LEVEL_FORM_UNIQUE;
}

static FormAnnotation load_form_annotation(const char *piece_prefix) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_phrase_form.txt", URSATZ_ANNOTATION_DIR, piece_prefix);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    FormAnnotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char kind[24];
        char word[24];
        if (sscanf(line, "%23s", kind) != 1) {
            continue;
        }
        if (strcmp(kind, "range") == 0) {
            assert(sscanf(line, "range %d %d", &a.range_first, &a.range_last) == 2);
        } else if (strcmp(kind, "phrase-end") == 0) {
            AnnotatedEnd *e = &a.ends[a.end_count++];
            assert(sscanf(line, "phrase-end %d %23s", &e->measure, word) == 2);
            e->type = cadence_of_text(word);
        } else if (strcmp(kind, "theme") == 0) {
            AnnotatedTheme *t = &a.themes[a.theme_count++];
            assert(sscanf(line, "theme %23s %d %d", word, &t->first, &t->last) == 3);
            t->form = form_of_text(word);
        } else if (strcmp(kind, "expansion") == 0) {
            AnnotatedExpansion *e = &a.expansions[a.expansion_count++];
            assert(sscanf(line, "expansion %23s %d %d %d", word, &e->first, &e->last, &e->measures) == 4);
            e->kind = strcmp(word, "internal") == 0 ? STANDARD_FORM_EXPANSION_INTERNAL
                                                    : (strcmp(word, "external") == 0 ? STANDARD_FORM_EXPANSION_EXTERNAL
                                                                                     : STANDARD_FORM_EXPANSION_CONTRACTION);
        }
    }
    free(text);
    assert(a.range_first > 0 && a.range_last >= a.range_first);
    return a;
}

/* --- one analysis --- */

typedef struct {
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
    StandardFormExpansions expansions;
} FormAnalysis;

static void form_analyze(const StandardFormInput *input, FormAnalysis *out) {
    assert(standard_form_segment_phrases(input, NULL, form_kernel_detect, NULL, &out->phrases) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(input, &out->phrases, NULL, &out->segmentation) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(input, &out->phrases, &out->segmentation, NULL, &out->themes) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_detect_expansions(input, &out->phrases, &out->segmentation, &out->themes, NULL,
                                           &out->expansions) == STANDARD_FORM_ANALYSIS_OK);
}

static void form_analysis_destroy(FormAnalysis *a) {
    standard_form_expansions_destroy(&a->expansions);
    standard_form_themes_destroy(&a->themes);
    standard_form_segmentation_destroy(&a->segmentation);
    standard_form_phrases_destroy(&a->phrases);
}

typedef struct {
    size_t ends;
    size_t ends_found;
    size_t themes;
    size_t themes_right;
    size_t expansions;
    size_t expansions_found;
    size_t long_expansions; /* annotated expansions of 2 or more measures */
    size_t long_expansions_found;
} FormScore;

static void form_score_add(FormScore *total, const FormScore *s) {
    total->ends += s->ends;
    total->ends_found += s->ends_found;
    total->themes += s->themes;
    total->themes_right += s->themes_right;
    total->expansions += s->expansions;
    total->expansions_found += s->expansions_found;
    total->long_expansions += s->long_expansions;
    total->long_expansions_found += s->long_expansions_found;
}

static int abs_int(int v) {
    return v < 0 ? -v : v;
}

static FormScore form_evaluate(const char *label, const FormAnnotation *a, const FormAnalysis *f) {
    FormScore score;
    memset(&score, 0, sizeof(score));
    printf("  %s: %zu phrase(s), %zu theme(s), %zu length change(s) reported\n", label, f->phrases.count, f->themes.count,
           f->expansions.count);
    for (size_t i = 0; i < f->phrases.count; i++) {
        const StandardFormPhraseClaim *c = &f->phrases.items[i].claim;
        printf("    phrase %zu: measures %d-%d (proper %d-%d, %d long)%s%s\n", i, c->first_measure, c->last_measure,
               c->first_measure, c->end_measure, c->length_measures,
               c->cadence_type >= 0 ? ", cadence " : ", no cadence",
               c->cadence_type >= 0 ? cadence_type_name((CadenceType)c->cadence_type) : "");
    }
    for (size_t i = 0; i < a->end_count; i++) {
        score.ends++;
        bool found = false;
        for (size_t k = 0; k < f->phrases.count; k++) {
            found = found || abs_int(f->phrases.items[k].claim.end_measure - a->ends[i].measure) <= 1;
        }
        score.ends_found += found ? 1 : 0;
        if (!found) {
            printf("    boundary at measure %d: not found\n", a->ends[i].measure);
        }
    }
    for (size_t i = 0; i < a->theme_count; i++) {
        score.themes++;
        bool right = false;
        const StandardFormThemeClaim *located = NULL;
        for (size_t k = 0; k < f->themes.count; k++) {
            const StandardFormThemeClaim *c = &f->themes.items[k].claim;
            if (abs_int(c->first_measure - a->themes[i].first) <= 1 && abs_int(c->last_measure - a->themes[i].last) <= 1) {
                located = c;
                right = right || c->form == (int)a->themes[i].form;
            }
        }
        score.themes_right += right ? 1 : 0;
        if (!right) {
            printf("    theme %s at measures %d-%d: %s\n", phrase_level_form_name(a->themes[i].form), a->themes[i].first,
                   a->themes[i].last,
                   located != NULL ? phrase_level_form_name((PhraseLevelForm)located->form) : "not found");
        }
    }
    for (size_t k = 0; k < f->themes.count; k++) {
        const StandardFormThemeClaim *c = &f->themes.items[k].claim;
        bool annotated = false;
        for (size_t i = 0; i < a->theme_count; i++) {
            annotated = annotated || (abs_int(c->first_measure - a->themes[i].first) <= 1 &&
                                      abs_int(c->last_measure - a->themes[i].last) <= 1);
        }
        if (!annotated) {
            printf("    reported, not annotated: %s at measures %d-%d\n", phrase_level_form_name((PhraseLevelForm)c->form),
                   c->first_measure, c->last_measure);
        }
    }
    for (size_t i = 0; i < a->expansion_count; i++) {
        score.expansions++;
        bool long_one = a->expansions[i].measures >= 2;
        score.long_expansions += long_one ? 1 : 0;
        bool found = false;
        for (size_t k = 0; k < f->expansions.count; k++) {
            const StandardFormExpansionClaim *c = &f->expansions.items[k].claim;
            found = found || (c->kind == (int)a->expansions[i].kind && abs_int(c->measures_changed - a->expansions[i].measures) <= 1 &&
                              abs_int(c->first_measure - a->expansions[i].first) <= 1 &&
                              abs_int(c->last_measure - a->expansions[i].last) <= 1);
        }
        score.expansions_found += found ? 1 : 0;
        score.long_expansions_found += (found && long_one) ? 1 : 0;
        if (!found) {
            printf("    expansion of %d measure(s) at %d-%d: not found\n", a->expansions[i].measures, a->expansions[i].first,
                   a->expansions[i].last);
        }
    }
    printf("    boundaries %zu of %zu, themes right %zu of %zu, expansions found %zu of %zu\n", score.ends_found, score.ends,
           score.themes_right, score.themes, score.expansions_found, score.expansions);
    return score;
}

/* --- the two runs of a piece --- */

/* The annotated phrase ends as hand cadences, each arriving in its whole measure. */
static size_t hand_ends(const FormAnnotation *a, HandCadence *out) {
    for (size_t i = 0; i < a->end_count; i++) {
        out[i].measure = a->ends[i].measure;
        out[i].type = a->ends[i].type;
    }
    return a->end_count;
}

/* The cadences of the whole chain for a piece: copied from run_whole_chain of test_cadences_reference_pieces.c, which returns only
   a score; this keeps the cadences. */
static HarmonyCadences chain_cadences(const Piece *piece, const WeightedPipeline *p, const MeterMap *map,
                                      const OuterVoiceLines *lines, const Annotation *a, HarmonySegment **segments_out,
                                      Inputs *inputs_out) {
    size_t count = scope_count(p, a);
    CommonPracticeWeightedResults chords = {0};
    assert(common_practice_identify_weighted_sequence(p->segments, count, a->home, COMMON_PRACTICE_KEY_MAJOR, NULL, &chords) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    CommonPracticeKeySegment *key_segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(key_segments != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p->sonorities.items[s].sonority);
        key_segments[s].start = time_span_start(window);
        key_segments[s].end = time_span_end(window);
        key_segments[s].source = &p->segments[s];
        assert(common_practice_weight_segment(p->segments[s].members, p->segments[s].member_count, &weighting,
                                              &key_segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(key_segments, count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    StandardTonalityKeyRegionInput *region_inputs =
        (StandardTonalityKeyRegionInput *)calloc(regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    StandardTonalitySegmentInput *segment_inputs =
        (StandardTonalitySegmentInput *)calloc(count, sizeof(StandardTonalitySegmentInput));
    assert(region_inputs != NULL && segment_inputs != NULL);
    for (size_t i = 0; i < regions.region_count; i++) {
        region_inputs[i].key = regions.regions[i].key;
        region_inputs[i].start = time_span_start(regions.regions[i].span);
        region_inputs[i].uncertainty = regions.regions[i].uncertainty;
    }
    for (size_t s = 0; s < count; s++) {
        segment_inputs[s].segment_id = p->segment_ids[s];
        segment_inputs[s].start = key_segments[s].start;
        segment_inputs[s].chord = chords.items[s].matched ? chords.items[s].best_chord : NULL;
        segment_inputs[s].weights = key_segments[s].weights.pitch_class;
    }
    StandardTonalityChordInput *tonal_inputs = NULL;
    size_t tonal_count = 0;
    assert(standard_tonality_build_chord_inputs(segment_inputs, count, region_inputs, regions.region_count, &tonal_inputs,
                                                &tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *romans = NULL;
    assert(standard_tonality_build_roman_numerals(tonal_inputs, tonal_count, &romans) == STANDARD_TONALITY_ROMAN_OK);

    *inputs_out = make_inputs(p, map, a, count);
    Inputs *in = inputs_out;
    for (size_t s = 0; s < count; s++) {
        HarmonySegmentInput *input = &in->inputs[s];
        if (!romans[s].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &romans[s].claim;
        int intervals[4];
        size_t interval_count = 0;
        claim_intervals(claim, intervals, &interval_count);
        set_intervals(input, intervals, interval_count);
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = tonal_inputs[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = romans[s].uncertainty;
        input->bass_degree = degree_of(&lines->bass[s], tonal_inputs[s].tonic_pitch_class);
        input->soprano_degree = degree_of(&lines->soprano[s], tonal_inputs[s].tonic_pitch_class);
    }
    assert(harmony_analysis_prepare(in->inputs, count, segments_out) == HARMONY_ANALYSIS_OK);
    HarmonyCadences found = {0};
    assert(harmony_analysis_find_cadences(*segments_out, count, NULL, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    (void)piece;

    standard_tonality_roman_results_destroy(romans, tonal_count);
    standard_tonality_chord_inputs_destroy(tonal_inputs);
    free(segment_inputs);
    free(region_inputs);
    common_practice_key_regions_destroy(&regions);
    free(key_segments);
    common_practice_weighted_results_destroy(&chords);
    return found;
}

typedef struct {
    const char *prefix;
    Piece piece;
} FormPiece;

static const FormPiece FORM_PIECES[5] = {{"bach114", {"Bach 114", "Bach 114.mid", "bach114_cadences.txt", true}},
                                         {"mozart545", {"Mozart 545", "Mozart 545.mid", "mozart545_cadences.txt", false}},
                                         {"clementi36", {"Clementi 36", "Clementi 36.mid", "clementi36_cadences.txt", false}},
                                         {"field1", {"Field 1", "Field 1.mid", "field1_cadences.txt", false}},
                                         {"field5", {"Field 5", "Field 5.mid", "field5_cadences.txt", false}}};

static void run_piece(const FormPiece *fp, FormScore *alone_total, FormScore *chain_total) {
    FormAnnotation annotation = load_form_annotation(fp->prefix);
    Annotation cadence_annotation = load_annotation(fp->piece.fixture);
    MeterMap *own = fp->piece.score_three_four ? three_four_map() : NULL;
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, fp->piece.midi, own);
    const MeterMap *map = own != NULL ? own : p.meter.meter_map;
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.voice_sonorities, p.events, p.pitch_values, p.note_count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);
    printf("%s, measures %d-%d (draft hand analysis):\n", fp->piece.name, annotation.range_first, annotation.range_last);

    {
        HandCadence ends[32];
        HarmonyCadences hand = hand_cadences_make(analysis_chain_segments(p.chain), map, ends, hand_ends(&annotation, ends));
        ChainFormInput build;
        assert(chain_build_form_input(&p.set, analysis_chain_segments(p.chain), &lines, annotation.range_first, annotation.range_last,
                                      &hand, &build) == CHAIN_INPUT_OK);
        FormAnalysis analysis;
        memset(&analysis, 0, sizeof(analysis));
        form_analyze(&build.input, &analysis);
        FormScore s = form_evaluate("the form adapter alone, on the annotated cadences", &annotation, &analysis);
        form_score_add(alone_total, &s);
        form_analysis_destroy(&analysis);
        chain_form_input_destroy(&build);
        hand_cadences_destroy(&hand);
    }
    {
        HarmonySegment *segments = NULL;
        Inputs inputs;
        HarmonyCadences found = chain_cadences(&fp->piece, &p, map, &lines, &cadence_annotation, &segments, &inputs);
        ChainFormInput build;
        assert(chain_build_form_input(&p.set, analysis_chain_segments(p.chain), &lines, annotation.range_first, annotation.range_last,
                                      &found, &build) == CHAIN_INPUT_OK);
        FormAnalysis analysis;
        memset(&analysis, 0, sizeof(analysis));
        form_analyze(&build.input, &analysis);
        printf("  (the cadence adapter reported %zu cadence(s) in the stretches it read)\n", found.count);
        FormScore s = form_evaluate("the whole chain, on the cadence adapter's cadences", &annotation, &analysis);
        form_score_add(chain_total, &s);
        form_analysis_destroy(&analysis);
        chain_form_input_destroy(&build);
        harmony_cadences_destroy(&found);
        harmony_analysis_destroy(segments);
        destroy_inputs(&inputs);
    }
    outer_voice_lines_destroy(&lines);
    pipeline_destroy(&p);
    if (own != NULL) {
        meter_map_destroy(own);
    }
}

static double ratio(size_t a, size_t b) {
    return b == 0 ? 1.0 : (double)a / (double)b;
}

#ifndef URSATZ_FORM_PIECES_AS_LIBRARY /* defined by a test that includes this file for its helpers (test_rhythm_field5.c) */
int main(void) {
    FormScore alone;
    FormScore chain;
    memset(&alone, 0, sizeof(alone));
    memset(&chain, 0, sizeof(chain));
    for (size_t i = 0; i < 5; i++) {
        run_piece(&FORM_PIECES[i], &alone, &chain);
    }
    printf("All five pieces, the form adapter alone: boundaries %zu of %zu (%.1f%%), themes right %zu of %zu (%.1f%%), "
           "expansions of 2+ measures found %zu of %zu\n",
           alone.ends_found, alone.ends, 100.0 * ratio(alone.ends_found, alone.ends), alone.themes_right, alone.themes,
           100.0 * ratio(alone.themes_right, alone.themes), alone.long_expansions_found, alone.long_expansions);
    printf("All five pieces, the whole chain: boundaries %zu of %zu (%.1f%%), themes right %zu of %zu (%.1f%%), "
           "expansions of 2+ measures found %zu of %zu\n",
           chain.ends_found, chain.ends, 100.0 * ratio(chain.ends_found, chain.ends), chain.themes_right, chain.themes,
           100.0 * ratio(chain.themes_right, chain.themes), chain.long_expansions_found, chain.long_expansions);
    assert(alone.ends > 0 && alone.themes > 0);
    assert(ratio(alone.ends_found, alone.ends) >= FORM_TARGET_BOUNDARIES);
    assert(ratio(alone.themes_right, alone.themes) >= FORM_TARGET_THEMES);
    assert(alone.long_expansions_found == alone.long_expansions);
    assert(ratio(chain.ends_found, chain.ends) >= CHAIN_FLOOR_BOUNDARIES);
    assert(ratio(chain.themes_right, chain.themes) >= CHAIN_FLOOR_THEMES);
    if (ratio(chain.ends_found, chain.ends) < FORM_TARGET_BOUNDARIES || ratio(chain.themes_right, chain.themes) < FORM_TARGET_THEMES) {
        printf("  NOTE: the whole chain is below the targets (floors asserted; see the PR)\n");
    }
    printf("test_form_reference_pieces: ok\n");
    return 0;
}
#endif
