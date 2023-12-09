#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_phrase.h"
#include "analysis_six_four.h"
#include "applied_chords.h"
#include "chord_input.h"
#include "key_regions.h"
#include "roman_numeral_adapter.h"
#include "weighted_pipeline.h"
#include "analysis/outer_voice.h"

/*
 * Phrase model labels for Mozart, Piano Sonata K. 545, first movement, exposition, against
 * tests/fixtures/annotations/mozart545_phrase_model.txt: a DRAFT hand reading, not yet checked against the score
 * (see its header), written from the pitch content of the file and not from this code's output. This test chains
 * the frameworks (standard-meter, common-practice-minimal, standard-tonality, standard-harmony); it lives here because
 * framework sources never include each other.
 *
 * Two tests. The phrase spans and their cadences are the annotated ones (phrases and cadences are not analysed yet).
 * A segment is right when its label (Tb, PD, D, Te, or none) is the annotated zone of the chord sounding at its
 * start.
 *
 * 1. THE HARMONY ADAPTER ALONE. The chord of each segment comes from the annotation (degree, quality, inversion, the
 *    applied target), the segment's time, beat strength and metrical position are the real ones from the L2 pipeline.
 *    Asserted: at least 80% of the annotated segments get the right label, and every annotated cadential 6/4 is found.
 * 2. THE WHOLE CHAIN. The L2 pipeline, weighted chord identification (C major, whole piece: the vocabulary is
 *    diatonic), local key regions, the standard-tonality adapter, and then this adapter, with the real outer voices.
 *    The earlier stages miss much of the harmony. Reported with the misses and asserted only at a floor under the
 *    measured level, so a regression shows; it is not a quality target.
 *
 * Nothing is tuned to the fixture: every default is the library's. URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR
 * come from CMake.
 */

#define LABEL_TARGET 0.80
#define CHAIN_FLOOR 0.20
#define EXPOSITION_END 28.0 /* whole notes: 28 measures of 4/4 */

typedef struct {
    int first_measure;
    int last_measure;
    int tonic; /* pitch class */
    CadenceType cadence;
} Phrase;

typedef struct {
    double at; /* whole notes */
    int phrase;
    int degree;
    bool seventh;
    bool diminished;
    int inversion;
    int applied_target;
    int zone; /* PhraseModelLabel, or -1 for none */
    char text[24];
} ChordMark;

typedef struct {
    Phrase phrases[8];
    size_t phrase_count;
    ChordMark chords[64];
    size_t chord_count;
    double cadential[8];
    size_t cadential_count;
} Annotation;

static int tonic_of(const char *name) {
    if (strcmp(name, "C") == 0) {
        return 0;
    }
    assert(strcmp(name, "G") == 0);
    return 7;
}

static int roman_degree(const char *letters, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    assert(length < sizeof(lower));
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    return -1;
}

/* "V4/3", "ii6", "viio/V", "I6/4": degree, seventh, diminished, inversion, applied target. */
static void parse_numeral(const char *text, ChordMark *mark) {
    size_t n = 0;
    while (text[n] == 'I' || text[n] == 'V' || text[n] == 'i' || text[n] == 'v') {
        n++;
    }
    mark->degree = roman_degree(text, n);
    assert(mark->degree > 0);
    const char *rest = text + n;
    mark->diminished = *rest == 'o';
    if (mark->diminished) {
        rest++;
    }
    char figure[16] = {0};
    size_t f = 0;
    while (*rest != '\0' && (isdigit((unsigned char)*rest) || *rest == '/')) {
        figure[f++] = *rest++;
        assert(f < sizeof(figure));
    }
    mark->applied_target = 0;
    if (f > 0 && figure[f - 1] == '/') {
        figure[--f] = '\0';
        mark->applied_target = roman_degree(rest, strlen(rest));
        assert(mark->applied_target > 0);
    } else {
        assert(*rest == '\0');
    }
    mark->seventh = strcmp(figure, "7") == 0 || strcmp(figure, "6/5") == 0 || strcmp(figure, "4/3") == 0 ||
                    strcmp(figure, "4/2") == 0;
    if (strcmp(figure, "") == 0 || strcmp(figure, "7") == 0) {
        mark->inversion = 0;
    } else if (strcmp(figure, "6") == 0 || strcmp(figure, "6/5") == 0) {
        mark->inversion = 1;
    } else if (strcmp(figure, "6/4") == 0 || strcmp(figure, "4/3") == 0) {
        mark->inversion = 2;
    } else if (strcmp(figure, "4/2") == 0) {
        mark->inversion = 3;
    } else {
        assert(!"unknown figure");
    }
}

static double time_of(int measure, double beat) {
    return ((double)(measure - 1) * 4.0 + (beat - 1.0)) / 4.0;
}

static Annotation load_annotation(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/mozart545_phrase_model.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Annotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[16] = "";
        sscanf(line, "%15s", word);
        if (strcmp(word, "phrase") == 0) {
            Phrase *p = &a.phrases[a.phrase_count++];
            char tonic[4] = "";
            char cadence[16] = "";
            assert(sscanf(line, "%*s %d %d %3s %15s", &p->first_measure, &p->last_measure, tonic, cadence) == 4);
            p->tonic = tonic_of(tonic);
            p->cadence = strcmp(cadence, "half") == 0 ? CADENCE_TYPE_HALF : CADENCE_TYPE_PERFECT_AUTHENTIC;
            assert(a.phrase_count <= 8);
        } else if (strcmp(word, "chord") == 0) {
            ChordMark *c = &a.chords[a.chord_count++];
            int measure = 0;
            double beat = 0.0;
            char numeral[24] = "";
            char zone[8] = "";
            assert(sscanf(line, "%*s %d %lf %23s %7s", &measure, &beat, numeral, zone) == 4);
            assert(a.chord_count <= 64 && a.phrase_count > 0);
            c->at = time_of(measure, beat);
            c->phrase = (int)a.phrase_count - 1;
            snprintf(c->text, sizeof(c->text), "%s", numeral);
            parse_numeral(numeral, c);
            c->zone = strcmp(zone, "Tb") == 0 ? PHRASE_MODEL_LABEL_TONIC_BEGINNING
                      : strcmp(zone, "PD") == 0 ? PHRASE_MODEL_LABEL_PREDOMINANT
                      : strcmp(zone, "D") == 0  ? PHRASE_MODEL_LABEL_DOMINANT
                      : strcmp(zone, "Te") == 0 ? PHRASE_MODEL_LABEL_TONIC_ENDING
                                                : -1;
        } else if (strcmp(word, "cadential64") == 0) {
            int measure = 0;
            double beat = 0.0;
            assert(sscanf(line, "%*s %d %lf", &measure, &beat) == 2);
            a.cadential[a.cadential_count++] = time_of(measure, beat);
        }
    }
    free(text);
    assert(a.phrase_count == 3 && a.chord_count > 10);
    return a;
}

/* The chord mark in force at a time, within its phrase, or NULL. */
static const ChordMark *mark_at(const Annotation *a, double t) {
    const ChordMark *found = NULL;
    for (size_t i = 0; i < a->chord_count; i++) {
        const Phrase *p = &a->phrases[a->chords[i].phrase];
        if (t + 1e-9 >= time_of(p->first_measure, 1.0) && t < time_of(p->last_measure + 1, 1.0) - 1e-9 &&
            a->chords[i].at <= t + 1e-9) {
            found = &a->chords[i];
        }
    }
    return found;
}

static const char *zone_name(int zone) {
    return zone < 0 ? "-" : phrase_model_label_abbreviation((PhraseModelLabel)zone);
}

static void print_where(double t) {
    long measure = (long)(t + 1e-9) + 1;
    printf("measure %ld beat %.2f", measure, 1.0 + (t - (double)(measure - 1)) * 4.0);
}

/* --- segment inputs --- */

typedef struct {
    HarmonySegmentInput *inputs;
    MetricalPosition **positions;
    size_t count;
} Inputs;

static Inputs make_inputs(const WeightedPipeline *p, const MeterMap *map, size_t count) {
    Inputs in;
    in.count = count;
    in.inputs = (HarmonySegmentInput *)calloc(count, sizeof(HarmonySegmentInput));
    in.positions = (MetricalPosition **)calloc(count, sizeof(MetricalPosition *));
    assert(in.inputs != NULL && in.positions != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p->sonorities.items[s].sonority);
        assert(metrical_position_from_time(map, time_span_start(window), &in.positions[s]) == METRICAL_POSITION_OK);
        HarmonySegmentInput *input = &in.inputs[s];
        input->segment_id = p->segment_ids[s];
        input->span = window;
        input->position = in.positions[s];
        input->beat_strength = pipeline_attack_strength(map, time_span_start(window));
        input->key_ref = "key";
        input->mode = HARMONY_MODE_MAJOR;
    }
    return in;
}

static void destroy_inputs(Inputs *in) {
    for (size_t s = 0; s < in->count; s++) {
        metrical_position_destroy(in->positions[s]);
    }
    free(in->positions);
    free(in->inputs);
}

static void set_intervals(HarmonySegmentInput *input, const int *intervals, size_t count) {
    memset(input->intervals, 0, sizeof(input->intervals));
    for (size_t i = 0; i < count; i++) {
        input->intervals[i] = intervals[i];
    }
    input->interval_count = count;
}

/* The adapter input for an annotated chord: the key's own triad on the degree (the identifier's reading, diatonic),
   the dominant seventh when annotated so, and for an applied chord the quality of its kind. */
static void input_from_mark(const ChordMark *mark, HarmonySegmentInput *input) {
    static const int MAJOR[3] = {0, 4, 7};
    static const int MINOR[3] = {0, 3, 7};
    static const int DIM[3] = {0, 3, 6};
    static const int DOMINANT7[4] = {0, 4, 7, 10};
    static const int QUALITY_OF_DEGREE[7] = {0, 1, 1, 0, 0, 1, 2}; /* 0 major, 1 minor, 2 diminished */
    input->inversion = mark->inversion;
    input->applied_to = mark->applied_target;
    if (mark->applied_target != 0) {
        input->degree = mark->diminished ? 7 : 5;
        if (mark->diminished) {
            set_intervals(input, DIM, 3);
        } else {
            set_intervals(input, mark->seventh ? DOMINANT7 : MAJOR, mark->seventh ? 4 : 3);
        }
        input->bass_degree = 0; /* chromatic bass */
        return;
    }
    input->degree = mark->degree;
    if (mark->seventh) {
        set_intervals(input, DOMINANT7, 4);
    } else {
        int quality = QUALITY_OF_DEGREE[mark->degree - 1];
        set_intervals(input, quality == 0 ? MAJOR : (quality == 1 ? MINOR : DIM), 3);
    }
    static const int MEMBER[4] = {0, 2, 4, 6};
    input->bass_degree = (mark->degree - 1 + MEMBER[mark->inversion]) % 7 + 1;
}

/* Segment indices of a phrase: those whose start lies inside its measures. */
static void span_of(const WeightedPipeline *p, size_t count, const Phrase *phrase, HarmonyPhraseSpan *span) {
    size_t first = count;
    size_t last = 0;
    for (size_t s = 0; s < count; s++) {
        if (p->starts[s] + 1e-9 >= time_of(phrase->first_measure, 1.0) &&
            p->starts[s] < time_of(phrase->last_measure + 1, 1.0) - 1e-9) {
            first = first < s ? first : s;
            last = s;
        }
    }
    assert(first < count);
    span->first_segment = first;
    span->last_segment = last;
    span->cadence = phrase->cadence;
}

typedef struct {
    size_t annotated;
    size_t right;
    size_t cadential_found;
} Score;

static Score evaluate(const Annotation *a, const WeightedPipeline *p, size_t count, const HarmonySegment *segments,
                      const HarmonyPhraseSpan *spans, bool print) {
    HarmonyPhraseModel model;
    assert(harmony_analysis_phrase_model_label(segments, count, spans, a->phrase_count, &model) ==
           HARMONY_PHRASE_MODEL_OK);
    Score score = {0, 0, 0};
    for (size_t s = 0; s < count; s++) {
        const ChordMark *mark = mark_at(a, p->starts[s]);
        if (mark == NULL) {
            continue;
        }
        score.annotated++;
        int got = model.segments[s].has_label ? (int)model.segments[s].label : -1;
        if (got == mark->zone) {
            score.right++;
        } else if (print) {
            printf("  miss: ");
            print_where(p->starts[s]);
            printf(": annotated %s (%s), got %s\n", zone_name(mark->zone), mark->text, zone_name(got));
        }
    }
    harmony_phrase_model_destroy(&model);

    HarmonySixFourResult *six_fours = NULL;
    assert(harmony_analysis_classify_six_fours(segments, count, &six_fours) == HARMONY_SIX_FOUR_OK);
    for (size_t c = 0; c < a->cadential_count; c++) {
        bool found = false;
        for (size_t s = 0; s < count; s++) {
            if (p->starts[s] <= a->cadential[c] + 1e-9 && a->cadential[c] < p->ends[s] - 1e-9) {
                found = six_fours[s].classified && six_fours[s].type == SIX_FOUR_TYPE_CADENTIAL;
            }
        }
        score.cadential_found += found ? 1 : 0;
        if (!found && print) {
            printf("  cadential 6/4 not found: ");
            print_where(a->cadential[c]);
            printf("\n");
        }
    }
    harmony_six_four_results_destroy(six_fours, count);
    return score;
}

static size_t scope_count(const WeightedPipeline *p) {
    size_t count = 0;
    while (count < p->segment_count && p->starts[count] < EXPOSITION_END - 1e-9) {
        count++;
    }
    return count;
}

static void test_adapter_alone(void) {
    Annotation a = load_annotation();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Mozart 545.mid", NULL);
    const MeterMap *map = p.meter.meter_map;
    size_t count = scope_count(&p);
    Inputs in = make_inputs(&p, map, count);
    for (size_t s = 0; s < count; s++) {
        const ChordMark *mark = mark_at(&a, p.starts[s]);
        if (mark != NULL) {
            input_from_mark(mark, &in.inputs[s]);
        } else {
            in.inputs[s].degree = 0; /* not a chord this framework reads */
            in.inputs[s].interval_count = 3;
        }
    }
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(in.inputs, count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyPhraseSpan spans[8];
    for (size_t i = 0; i < a.phrase_count; i++) {
        span_of(&p, count, &a.phrases[i], &spans[i]);
    }
    printf("Mozart 545 exposition, the harmony adapter alone on the annotated chords (draft hand marks):\n");
    Score score = evaluate(&a, &p, count, segments, spans, true);
    double rate = score.annotated == 0 ? 0.0 : (double)score.right / (double)score.annotated;
    printf("  %zu of %zu annotated segments get the right label (%.1f%%); %zu of %zu annotated cadential 6/4s found\n",
           score.right, score.annotated, rate * 100.0, score.cadential_found, a.cadential_count);
    assert(score.annotated > 0);
    assert(rate >= LABEL_TARGET);
    assert(score.cadential_found == a.cadential_count);

    harmony_analysis_destroy(segments);
    destroy_inputs(&in);
    pipeline_destroy(&p);
}

/* --- the whole chain --- */

static void intervals_of(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
    static const int TRIADS[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    static const int SEVENTHS[5][4] = {{0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    memset(out, 0, 4 * sizeof(int));
    if (claim->seventh) {
        memcpy(out, SEVENTHS[claim->quality], 4 * sizeof(int));
        *count = 4;
    } else {
        memcpy(out, TRIADS[claim->quality], 3 * sizeof(int));
        *count = 3;
    }
}

/* The degree (1..7) of a pitch position in a major key on `tonic`, or 0 for a chromatic note or an absent one. */
static int degree_of(const OuterVoiceEntry *entry, int tonic) {
    static const int DEGREE_OF_OFFSET[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};
    if (!entry->present) {
        return 0;
    }
    long position = pitch_numeric_value_position_numerator(entry->pitch);
    return DEGREE_OF_OFFSET[(((position - tonic) % 12) + 12) % 12];
}

static void test_whole_chain(void) {
    Annotation a = load_annotation();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Mozart 545.mid", NULL);
    const MeterMap *map = p.meter.meter_map;
    size_t count = scope_count(&p);

    CommonPracticeWeightedResults chords = {0};
    assert(common_practice_identify_weighted_sequence(p.segments, count, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &chords) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    CommonPracticeKeySegment *key_segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(key_segments != NULL);
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p.sonorities.items[s].sonority);
        key_segments[s].start = time_span_start(window);
        key_segments[s].end = time_span_end(window);
        key_segments[s].source = &p.segments[s];
        assert(common_practice_weight_segment(p.segments[s].members, p.segments[s].member_count, &weighting,
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
        segment_inputs[s].segment_id = p.segment_ids[s];
        segment_inputs[s].start = key_segments[s].start;
        segment_inputs[s].chord = chords.items[s].matched ? chords.items[s].best_chord : NULL;
        segment_inputs[s].weights = key_segments[s].weights.pitch_class;
    }
    StandardTonalityChordInput *tonal_inputs = NULL;
    size_t tonal_count = 0;
    assert(standard_tonality_build_chord_inputs(segment_inputs, count, region_inputs, regions.region_count,
                                                &tonal_inputs, &tonal_count) == STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *romans = NULL;
    assert(standard_tonality_build_roman_numerals(tonal_inputs, tonal_count, &romans) == STANDARD_TONALITY_ROMAN_OK);

    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.sonorities, p.events, p.pitch_values, p.note_count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);

    /* Fill the neutral input from standard-tonality's output: the caller's job, done here. */
    Inputs in = make_inputs(&p, map, count);
    for (size_t s = 0; s < count; s++) {
        HarmonySegmentInput *input = &in.inputs[s];
        if (!romans[s].has_numeral) {
            input->degree = 0;
            input->interval_count = 3;
            continue;
        }
        const StandardTonalityRomanClaim *claim = &romans[s].claim;
        int intervals[4];
        size_t interval_count = 0;
        intervals_of(claim, intervals, &interval_count);
        set_intervals(input, intervals, interval_count);
        input->degree = claim->degree;
        input->inversion = claim->inversion;
        input->applied_to = claim->applied ? claim->applied_target_degree : 0;
        input->mode = tonal_inputs[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
        input->uncertainty = romans[s].uncertainty;
        input->bass_degree = degree_of(&lines.bass[s], tonal_inputs[s].tonic_pitch_class);
        input->soprano_degree = degree_of(&lines.soprano[s], tonal_inputs[s].tonic_pitch_class);
    }
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(in.inputs, count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyPhraseSpan spans[8];
    for (size_t i = 0; i < a.phrase_count; i++) {
        span_of(&p, count, &a.phrases[i], &spans[i]);
    }

    printf("Mozart 545 exposition, the whole chain (draft hand marks):\n");
    Score score = evaluate(&a, &p, count, segments, spans, true);
    double rate = score.annotated == 0 ? 0.0 : (double)score.right / (double)score.annotated;
    printf("  %zu of %zu annotated segments get the right label (%.1f%%); %zu of %zu annotated cadential 6/4s found\n",
           score.right, score.annotated, rate * 100.0, score.cadential_found, a.cadential_count);
    assert(score.annotated > 0);
    assert(rate >= CHAIN_FLOOR);
    if (rate < LABEL_TARGET) {
        printf("  NOTE: %.1f%% is below the %.0f%% target (floor %.0f%% asserted; see the PR)\n", rate * 100.0,
               LABEL_TARGET * 100.0, CHAIN_FLOOR * 100.0);
    }

    harmony_analysis_destroy(segments);
    destroy_inputs(&in);
    outer_voice_lines_destroy(&lines);
    standard_tonality_roman_results_destroy(romans, tonal_count);
    standard_tonality_chord_inputs_destroy(tonal_inputs);
    free(segment_inputs);
    free(region_inputs);
    common_practice_key_regions_destroy(&regions);
    free(key_segments);
    common_practice_weighted_results_destroy(&chords);
    pipeline_destroy(&p);
}

int main(void) {
    test_adapter_alone();
    test_whole_chain();
    printf("test_mozart545_phrase_model: ok\n");
    return 0;
}
