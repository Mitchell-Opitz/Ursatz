#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applied_chords.h"
#include "chord_input.h"
#include "key_regions.h"
#include "roman_numeral_adapter.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "weighted_pipeline.h"

/*
 * Roman numerals for Bach, Minuet in G (BWV Anh. 114), against tests/fixtures/annotations/
 * bach114_roman_numerals.txt: a DRAFT hand reading, not yet checked against the score (see its header),
 * written from the pitch content of the file and not from this code's output.
 *
 * Two tests. A segment is right when its numeral matches the annotation on degree, inversion and applied
 * target.
 *
 * 1. THE ADAPTER ALONE. The chord and key of each annotated segment come from the annotation (the chord a
 *    diatonic identifier would give: the annotated root with the key's own quality, and the annotated bass; the
 *    key is G major), the weights are the segment's real pitch-class weights, and only the adapter runs.
 *    Asserted: at least 85% of the annotated segments match, and every annotated applied chord is found.
 * 2. THE WHOLE CHAIN. Adaptive segments in the score's 3/4, weights, weighted chord identification (G major,
 *    whole piece: the vocabulary is diatonic), local key regions, then the adapter. The chord and key stages
 *    are earlier work and miss most of the marks (ornamented segments read as other chords; measures 19-22 and
 *    27-32 read as D major). Reported with the misses, and asserted only at a floor just under the measured
 *    level, so a regression shows; it is not a quality target.
 *
 * Nothing is tuned to the fixture: every default is the library's. URSATZ_CORPUS_DIR and
 * URSATZ_ANNOTATION_DIR come from CMake.
 */

#define TONIC_G 7
#define MATCH_TARGET 0.85
#define CHAIN_FLOOR 0.30 /* the whole chain scored 34.6% when this was written */

typedef struct {
    double at;       /* whole notes */
    bool end;
    int degree;      /* 1..7 */
    int inversion;   /* 0..3 */
    int applied_target; /* 0 for none, else 1..7 */
    bool seventh;
    char text[24];
} Mark;

typedef struct {
    Mark marks[96];
    size_t count;
} Marks;

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

/* "V6/5/V", "vio6", "V/V", "I6/4", "V7": the degree, the inversion and the applied target. */
static void parse_numeral(const char *text, Mark *mark) {
    size_t n = 0;
    while (text[n] == 'I' || text[n] == 'V' || text[n] == 'i' || text[n] == 'v') {
        n++;
    }
    mark->degree = roman_degree(text, n);
    assert(mark->degree > 0);
    const char *rest = text + n;
    if (*rest == 'o') {
        rest++;
    }
    char figure[16] = {0};
    size_t f = 0;
    while (*rest != '\0' && (isdigit((unsigned char)*rest) || *rest == '/')) {
        assert(f + 1 < sizeof(figure));
        figure[f++] = *rest++;
    }
    mark->applied_target = 0;
    if (f > 0 && figure[f - 1] == '/') {
        figure[--f] = '\0';
        assert(*rest != '\0');
        size_t m = 0;
        while (rest[m] != '\0') {
            m++;
        }
        mark->applied_target = roman_degree(rest, m);
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

static Marks load_marks(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/bach114_roman_numerals.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Marks marks;
    memset(&marks, 0, sizeof(marks));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        int measure = 0;
        double beat = 0.0;
        char numeral[24] = "";
        assert(sscanf(line, "%d %lf %23s", &measure, &beat, numeral) == 3);
        assert(marks.count < 96 && measure >= 1 && beat >= 1.0);
        Mark *mark = &marks.marks[marks.count++];
        mark->at = ((double)(measure - 1) * 3.0 + (beat - 1.0)) / 4.0;
        snprintf(mark->text, sizeof(mark->text), "%s", numeral);
        mark->end = strcmp(numeral, "END") == 0;
        if (!mark->end) {
            parse_numeral(numeral, mark);
        }
    }
    free(text);
    assert(marks.count > 10);
    return marks;
}

/* The mark in force at a time, or NULL outside the marked stretches. */
static const Mark *mark_at(const Marks *marks, double t) {
    const Mark *found = NULL;
    for (size_t i = 0; i < marks->count; i++) {
        if (marks->marks[i].at <= t + 1e-9) {
            found = &marks->marks[i];
        }
    }
    return found != NULL && !found->end ? found : NULL;
}

static MeterMap *three_four_map(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *three_four = NULL;
    assert(time_signature_create(3, quarter, &three_four) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {three_four, zero};
    EntityID *id = NULL;
    assert(entity_id_create("bach-114-score-meter", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    return map;
}

static void print_where(double t) {
    long measure = (long)(t / 0.75 + 1e-9) + 1;
    double beat = 1.0 + (t - (double)(measure - 1) * 0.75) * 4.0;
    printf("measure %ld beat %.2f", measure, beat);
}

/* The scale: semitones above the tonic of degrees 1..7 in major, and the key's own triad quality. */
static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MAJOR_TRIAD_THIRD[7] = {4, 3, 3, 4, 4, 3, 3}; /* the third above each degree's root */
static const int MAJOR_TRIAD_FIFTH[7] = {7, 7, 7, 7, 7, 7, 6};

static int wrap12(int v) {
    return ((v % 12) + 12) % 12;
}

/* The adapter input an ideal chord identifier would give for an annotated chord: the root and the key's own
   triad on it (an applied chord arrives as its diatonic neighbour, the way the identifier gives it), the
   annotated bass, in G major. */
static void input_from_mark(const Mark *mark, StandardTonalityChordInput *input) {
    int root;
    int intervals[4];
    size_t count;
    int chord_root;
    int chord_third_fifth[2];
    if (mark->applied_target != 0) {
        int target_root = wrap12(TONIC_G + MAJOR_SCALE[mark->applied_target - 1]);
        root = wrap12(target_root + (mark->degree == 7 ? 11 : 7));
        if (mark->degree == 7) {
            int dim[3] = {0, 3, 6};
            memcpy(intervals, dim, sizeof(dim));
            count = 3;
        } else {
            int dom[4] = {0, 4, 7, 10};
            memcpy(intervals, dom, sizeof(dom));
            count = mark->seventh ? 4 : 3;
        }
        /* the diatonic chord on that root: the degree of the root in G major */
        int offset = wrap12(root - TONIC_G);
        int degree_index = 0;
        for (int d = 0; d < 7; d++) {
            if (MAJOR_SCALE[d] == offset) {
                degree_index = d;
            }
        }
        chord_root = root;
        chord_third_fifth[0] = MAJOR_TRIAD_THIRD[degree_index];
        chord_third_fifth[1] = MAJOR_TRIAD_FIFTH[degree_index];
    } else {
        root = wrap12(TONIC_G + MAJOR_SCALE[mark->degree - 1]);
        chord_root = root;
        chord_third_fifth[0] = MAJOR_TRIAD_THIRD[mark->degree - 1];
        chord_third_fifth[1] = MAJOR_TRIAD_FIFTH[mark->degree - 1];
        intervals[0] = 0;
        intervals[1] = chord_third_fifth[0];
        intervals[2] = chord_third_fifth[1];
        count = 3;
        if (mark->seventh) {
            intervals[3] = mark->degree == 5 ? 10 : (MAJOR_TRIAD_THIRD[mark->degree - 1] == 4 ? 11 : 10);
            if (mark->degree == 7) {
                intervals[3] = 10;
            }
            count = 4;
        }
    }
    /* The chord the identifier gives is the diatonic triad (or seventh) on the root; an applied chord arrives
       as that triad, with the same bass. */
    int read_intervals[4] = {0, chord_third_fifth[0], chord_third_fifth[1], 0};
    size_t read_count = 3;
    if (mark->applied_target == 0) {
        memcpy(read_intervals, intervals, sizeof(read_intervals));
        read_count = count;
    }
    int member = mark->inversion;
    int bass_interval = member < (int)count ? intervals[member] : 0;
    memset(input, 0, sizeof(*input));
    input->has_numeral = true;
    input->root_pitch_class = chord_root;
    input->bass_pitch_class = wrap12(root + bass_interval);
    input->interval_count = read_count;
    for (size_t i = 0; i < read_count; i++) {
        input->intervals[i] = read_intervals[i];
    }
    input->tonic_pitch_class = TONIC_G;
    input->mode = STANDARD_TONALITY_ROMAN_KEY_MAJOR;
}

static bool segment_matches(const StandardTonalityRomanResult *result, const Mark *mark) {
    if (!result->has_numeral) {
        return false;
    }
    const StandardTonalityRomanClaim *claim = &result->claim;
    bool right = claim->inversion == mark->inversion;
    if (mark->applied_target != 0) {
        return right && claim->applied && claim->applied_target_degree == mark->applied_target;
    }
    return right && !claim->applied && claim->degree == mark->degree;
}

static void test_adapter_alone_matches_the_hand_analysis(void) {
    Marks marks = load_marks();
    MeterMap *score_meter = three_four_map();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Bach 114.mid", score_meter);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();

    StandardTonalityChordInput *inputs =
        (StandardTonalityChordInput *)calloc(p.segment_count, sizeof(StandardTonalityChordInput));
    const Mark **segment_marks = (const Mark **)calloc(p.segment_count, sizeof(const Mark *));
    assert(inputs != NULL && segment_marks != NULL);
    for (size_t s = 0; s < p.segment_count; s++) {
        segment_marks[s] = mark_at(&marks, p.starts[s]);
        inputs[s].segment_id = p.segment_ids[s];
        if (segment_marks[s] != NULL) {
            input_from_mark(segment_marks[s], &inputs[s]);
            inputs[s].segment_id = p.segment_ids[s];
            CommonPracticeWeights weights;
            assert(common_practice_weight_segment(p.segments[s].members, p.segments[s].member_count, &weighting,
                                                  &weights) == COMMON_PRACTICE_WEIGHTED_OK);
            inputs[s].has_weights = true;
            memcpy(inputs[s].weights, weights.pitch_class, sizeof(inputs[s].weights));
        }
    }
    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals(inputs, p.segment_count, &results) == STANDARD_TONALITY_ROMAN_OK);

    size_t annotated = 0;
    size_t matched = 0;
    size_t applied_marks = 0;
    size_t applied_found = 0;
    const Mark *previous_applied = NULL;
    bool seen = false;
    printf("Bach 114, the adapter alone on the annotated chords (draft hand marks, G major):\n");
    for (size_t s = 0; s < p.segment_count; s++) {
        const Mark *mark = segment_marks[s];
        if (mark == NULL) {
            continue;
        }
        annotated++;
        bool right = segment_matches(&results[s], mark);
        if (mark->applied_target != 0) {
            if (mark != previous_applied) {
                applied_marks++;
                seen = false;
                previous_applied = mark;
            }
            if (results[s].has_numeral && results[s].claim.applied &&
                results[s].claim.applied_target_degree == mark->applied_target && !seen) {
                applied_found++;
                seen = true;
            }
        }
        if (right) {
            matched++;
        } else {
            printf("  miss: ");
            print_where(p.starts[s]);
            printf(": annotated %s, got %s\n", mark->text, results[s].has_numeral ? results[s].claim.label : "(no numeral)");
        }
    }
    double rate = annotated == 0 ? 0.0 : (double)matched / (double)annotated;
    printf("  %zu of %zu annotated segments match (%.1f%%); %zu of %zu annotated applied chords found\n", matched,
           annotated, rate * 100.0, applied_found, applied_marks);
    assert(annotated > 0 && applied_marks > 0);
    assert(rate >= MATCH_TARGET);
    assert(applied_found == applied_marks);

    standard_tonality_roman_results_destroy(results, p.segment_count);
    free(segment_marks);
    free(inputs);
    pipeline_destroy(&p);
    meter_map_destroy(score_meter);
}

static void test_whole_chain_against_the_hand_analysis(void) {
    Marks marks = load_marks();
    MeterMap *score_meter = three_four_map();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Bach 114.mid", score_meter);

    CommonPracticeWeightedResults chords = {0};
    assert(common_practice_identify_weighted_sequence(p.segments, p.segment_count, TONIC_G,
                                                      COMMON_PRACTICE_KEY_MAJOR, NULL, &chords) ==
           COMMON_PRACTICE_WEIGHTED_OK);

    /* weights and key segments */
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    CommonPracticeKeySegment *key_segments =
        (CommonPracticeKeySegment *)calloc(p.segment_count, sizeof(CommonPracticeKeySegment));
    assert(key_segments != NULL);
    for (size_t s = 0; s < p.segment_count; s++) {
        const TimeSpan *window = sonority_time_window(p.sonorities.items[s].sonority);
        key_segments[s].start = time_span_start(window);
        key_segments[s].end = time_span_end(window);
        key_segments[s].source = &p.segments[s];
        assert(common_practice_weight_segment(p.segments[s].members, p.segments[s].member_count, &weighting,
                                              &key_segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(key_segments, p.segment_count, score_meter, NULL, 0, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);

    printf("Bach 114, the whole chain: Roman numerals in the local key (draft hand marks, G major):\n");
    for (size_t i = 0; i < regions.region_count; i++) {
        printf("  key region: measures %d-%d %s uncertainty %.2f\n", regions.regions[i].first_measure,
               regions.regions[i].last_measure, key_label(regions.regions[i].key), regions.regions[i].uncertainty);
    }

    /* the adapter */
    StandardTonalityKeyRegionInput *region_inputs =
        (StandardTonalityKeyRegionInput *)calloc(regions.region_count, sizeof(StandardTonalityKeyRegionInput));
    StandardTonalitySegmentInput *segment_inputs =
        (StandardTonalitySegmentInput *)calloc(p.segment_count, sizeof(StandardTonalitySegmentInput));
    assert(region_inputs != NULL && segment_inputs != NULL);
    for (size_t i = 0; i < regions.region_count; i++) {
        region_inputs[i].key = regions.regions[i].key;
        region_inputs[i].start = time_span_start(regions.regions[i].span);
        region_inputs[i].uncertainty = regions.regions[i].uncertainty;
    }
    for (size_t s = 0; s < p.segment_count; s++) {
        segment_inputs[s].segment_id = p.segment_ids[s];
        segment_inputs[s].start = key_segments[s].start;
        segment_inputs[s].chord = chords.items[s].matched ? chords.items[s].best_chord : NULL;
        segment_inputs[s].weights = key_segments[s].weights.pitch_class;
    }
    StandardTonalityChordInput *inputs = NULL;
    size_t input_count = 0;
    assert(standard_tonality_build_chord_inputs(segment_inputs, p.segment_count, region_inputs,
                                                regions.region_count, &inputs, &input_count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals(inputs, input_count, &results) == STANDARD_TONALITY_ROMAN_OK);

    size_t annotated = 0;
    size_t matched = 0;
    size_t applied_marks = 0;
    size_t applied_found = 0;
    const Mark *previous_applied = NULL;
    bool applied_seen_for_mark = false;
    for (size_t s = 0; s < p.segment_count; s++) {
        const Mark *mark = mark_at(&marks, p.starts[s]);
        if (mark == NULL) {
            continue;
        }
        const StandardTonalityRomanResult *result = &results[s];
        annotated++;
        bool right = false;
        if (result->has_numeral) {
            const StandardTonalityRomanClaim *claim = &result->claim;
            right = claim->inversion == mark->inversion;
            if (mark->applied_target != 0) {
                right = right && claim->applied && claim->applied_target_degree == mark->applied_target;
            } else {
                right = right && !claim->applied && claim->degree == mark->degree;
            }
        }
        if (mark->applied_target != 0) {
            if (mark != previous_applied) {
                applied_marks++;
                applied_seen_for_mark = false;
                previous_applied = mark;
            }
            if (result->has_numeral && result->claim.applied && result->claim.applied_target_degree == mark->applied_target &&
                !applied_seen_for_mark) {
                applied_found++;
                applied_seen_for_mark = true;
            }
        }
        if (right) {
            matched++;
        } else {
            printf("  miss: ");
            print_where(p.starts[s]);
            printf(": annotated %s, got %s\n", mark->text, result->has_numeral ? result->claim.label : "(no numeral)");
        }
    }
    double rate = annotated == 0 ? 0.0 : (double)matched / (double)annotated;
    printf("  %zu of %zu annotated segments match (%.1f%%); %zu of %zu annotated applied chords found\n", matched,
           annotated, rate * 100.0, applied_found, applied_marks);

    /* Reported against the 85% target, asserted only at the floor: the chord and key stages are earlier work. */
    assert(annotated > 0 && applied_marks > 0);
    assert(rate >= CHAIN_FLOOR);
    if (rate < MATCH_TARGET) {
        printf("  NOTE: %.1f%% is below the %.0f%% target (floor %.0f%% asserted; see the PR)\n", rate * 100.0,
               MATCH_TARGET * 100.0, CHAIN_FLOOR * 100.0);
    }
    (void)applied_found;

    standard_tonality_roman_results_destroy(results, input_count);
    standard_tonality_chord_inputs_destroy(inputs);
    free(segment_inputs);
    free(region_inputs);
    common_practice_key_regions_destroy(&regions);
    free(key_segments);
    common_practice_weighted_results_destroy(&chords);
    pipeline_destroy(&p);
    meter_map_destroy(score_meter);
}

int main(void) {
    test_adapter_alone_matches_the_hand_analysis();
    test_whole_chain_against_the_hand_analysis();
    printf("test_bach114_roman_numerals: ok\n");
    return 0;
}
