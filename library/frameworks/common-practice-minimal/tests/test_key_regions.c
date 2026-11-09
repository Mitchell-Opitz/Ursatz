#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "key_regions.h"
#include "time/note_value.h"
#include "time/time_signature.h"

/*
 * Key regions, step 1: windowed key scoring. Segments are built straight from pitch-class weights (one whole
 * note per measure in 4/4), so each test says exactly which pitch classes sound where.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static MeterMap *four_four(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(4, quarter, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {signature, zero};
    MeterMap *map = NULL;
    assert(meter_map_create(make_id("map"), &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    return map;
}

typedef struct {
    CommonPracticeKeySegment items[64];
    MusicalTime *times[130];
    size_t count;
    size_t time_count;
} Piece;

static MusicalTime *time_at(Piece *piece, long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    piece->times[piece->time_count++] = time;
    return time;
}

/* A one-measure segment at `measure` whose pitch classes weigh `weights`. */
static void add_measure(Piece *piece, long measure, const double weights[12]) {
    CommonPracticeKeySegment *segment = &piece->items[piece->count++];
    memset(segment, 0, sizeof(*segment));
    segment->start = time_at(piece, measure, 1);
    segment->end = time_at(piece, measure + 1, 1);
    for (int pc = 0; pc < 12; pc++) {
        segment->weights.pitch_class[pc] = weights[pc];
        segment->weights.total += weights[pc];
    }
    segment->weights.bass_pitch = -1;
    segment->weights.bass_pitch_class = -1;
}

static void destroy_piece(Piece *piece) {
    for (size_t i = 0; i < piece->time_count; i++) {
        musical_time_destroy(piece->times[i]);
    }
}

/* A major scale's weights on a tonic, tonic/third/fifth heavier. */
static void major_weights(int tonic, double out[12]) {
    static const int SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
    static const double WEIGHT[7] = {3.0, 1.0, 2.0, 1.2, 2.5, 1.0, 0.8};
    memset(out, 0, 12 * sizeof(double));
    for (int i = 0; i < 7; i++) {
        out[(tonic + SCALE[i]) % 12] = WEIGHT[i];
    }
}

static int best_key(const CommonPracticeKeyWindow *window) {
    int best = 0;
    for (int k = 1; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        if (window->correlation[k] > window->correlation[best]) {
            best = k;
        }
    }
    return best;
}

static void test_a_scale_scores_its_own_key_highest(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double c[12];
    major_weights(0, c);
    for (long m = 0; m < 6; m++) {
        add_measure(&piece, m, c);
    }
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 3 && windows.first_measure == 1 && windows.last_measure == 6);
    for (size_t w = 0; w < windows.count; w++) {
        assert(windows.items[w].first_measure == (int)w + 1 && windows.items[w].last_measure == (int)w + 4);
        assert(best_key(&windows.items[w]) == 0);
        assert(windows.items[w].correlation[0] > 0.8);
        assert(windows.items[w].correlation[0] > windows.items[w].correlation[21]); /* over A minor */
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            assert(windows.items[w].correlation[k] >= -1.0 - 1e-12 && windows.items[w].correlation[k] <= 1.0 + 1e-12);
        }
    }
    common_practice_key_windows_destroy(&windows);
    common_practice_key_windows_destroy(&windows);
    common_practice_key_windows_destroy(NULL);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_minor_distribution_scores_the_minor_key(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    /* A minor with the raised seventh (G#): A 4, B 1, C 2.5, D 1.5, E 3, F 1.5, G# 1 */
    double a_minor[12] = {0};
    a_minor[9] = 4.0;
    a_minor[11] = 1.0;
    a_minor[0] = 2.5;
    a_minor[2] = 1.5;
    a_minor[4] = 3.0;
    a_minor[5] = 1.5;
    a_minor[8] = 1.0;
    add_measure(&piece, 0, a_minor);
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 1);
    assert(best_key(&windows.items[0]) == common_practice_key_index_of(9, COMMON_PRACTICE_KEY_MINOR));
    common_practice_key_windows_destroy(&windows);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_the_windows_follow_a_change_of_key(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double c[12];
    double g[12];
    major_weights(0, c);
    major_weights(7, g);
    for (long m = 0; m < 4; m++) {
        add_measure(&piece, m, c);
    }
    for (long m = 4; m < 8; m++) {
        add_measure(&piece, m, g);
    }
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 5);
    assert(best_key(&windows.items[0]) == 0);
    assert(best_key(&windows.items[4]) == 7);
    common_practice_key_windows_destroy(&windows);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_short_piece_gets_one_window(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double c[12];
    major_weights(0, c);
    add_measure(&piece, 0, c);
    add_measure(&piece, 1, c);
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 1 && windows.items[0].first_measure == 1 && windows.items[0].last_measure == 2);
    common_practice_key_windows_destroy(&windows);

    CommonPracticeKeyRegionsConfig one = common_practice_key_regions_config_default();
    one.window_measures = 1;
    assert(key_window_scores(piece.items, piece.count, map, &one, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 2);
    common_practice_key_windows_destroy(&windows);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_silent_window_scores_zero(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double none[12] = {0};
    add_measure(&piece, 0, none);
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        assert(windows.items[0].correlation[k] == 0.0);
    }
    common_practice_key_windows_destroy(&windows);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_custom_profile_set_is_used(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double only_g[12] = {0};
    only_g[7] = 1.0;
    add_measure(&piece, 0, only_g);
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    memset(&config.profiles, 0, sizeof(config.profiles));
    config.profiles.major[0] = 1.0; /* only the tonic counts */
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, &config, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(best_key(&windows.items[0]) == 7);
    assert(fabs(windows.items[0].correlation[7] - 1.0) < 1e-12);
    common_practice_key_windows_destroy(&windows);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_bad_input(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double c[12];
    major_weights(0, c);
    add_measure(&piece, 0, c);
    add_measure(&piece, 1, c);
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, NULL) == COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(key_window_scores(piece.items, piece.count, NULL, NULL, &windows) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(key_window_scores(NULL, 2, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(key_window_scores(piece.items, 0, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS);

    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.window_measures = 0;
    assert(key_window_scores(piece.items, piece.count, map, &config, &windows) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.profiles.minor[3] = NAN;
    assert(key_window_scores(piece.items, piece.count, map, &config, &windows) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);

    CommonPracticeKeySegment swapped[2] = {piece.items[1], piece.items[0]};
    assert(key_window_scores(swapped, 2, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT);
    CommonPracticeKeySegment backwards = piece.items[0];
    backwards.end = backwards.start;
    assert(key_window_scores(&backwards, 1, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT);
    CommonPracticeKeySegment negative = piece.items[0];
    negative.weights.pitch_class[2] = -1.0;
    assert(key_window_scores(&negative, 1, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT);
    CommonPracticeKeySegment missing = piece.items[0];
    missing.start = NULL;
    assert(key_window_scores(&missing, 1, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT);
    assert(windows.items == NULL);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_key_index_helpers(void) {
    assert(common_practice_key_index_tonic(0) == 0 && common_practice_key_index_mode(0) == COMMON_PRACTICE_KEY_MAJOR);
    assert(common_practice_key_index_tonic(21) == 9 && common_practice_key_index_mode(21) == COMMON_PRACTICE_KEY_MINOR);
    assert(common_practice_key_index_tonic(24) == -1 && common_practice_key_index_tonic(-1) == -1);
    assert(common_practice_key_index_of(7, COMMON_PRACTICE_KEY_MAJOR) == 7);
    assert(common_practice_key_index_of(7, COMMON_PRACTICE_KEY_MINOR) == 19);
    assert(common_practice_key_index_of(12, COMMON_PRACTICE_KEY_MAJOR) == -1);
    assert(strcmp(common_practice_key_index_tonic_name(18), "F#") == 0);
    assert(strcmp(common_practice_key_index_tonic_name(3), "Eb") == 0);
    assert(common_practice_key_index_tonic_name(30) == NULL);
    CommonPracticeKeyProfiles kk = common_practice_krumhansl_kessler_profiles();
    assert(kk.major[0] == 6.35 && kk.minor[3] == 5.38);
}

/* --- regions --- */

static void add_run(Piece *piece, long *measure, int tonic, long count) {
    double weights[12];
    major_weights(tonic, weights);
    for (long i = 0; i < count; i++) {
        add_measure(piece, (*measure)++, weights);
    }
}

static bool time_is(const MusicalTime *time, long numerator, unsigned long denominator) {
    return musical_time_numerator(time) * (long)denominator == numerator * (long)musical_time_denominator(time);
}

static void assert_gap_free(const CommonPracticeKeyRegions *regions, int first, int last) {
    assert(regions->region_count >= 1);
    assert(regions->regions[0].first_measure == first);
    assert(regions->regions[regions->region_count - 1].last_measure == last);
    assert(regions->change_count == regions->region_count - 1);
    for (size_t i = 0; i < regions->region_count; i++) {
        const CommonPracticeKeyRegion *region = &regions->regions[i];
        assert(region->first_measure <= region->last_measure);
        assert(region->uncertainty >= 0.0 && region->uncertainty <= 1.0);
        assert(region->source == COMMON_PRACTICE_REGION_SOURCE_PROFILE ||
               region->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
        assert(region->key != NULL && region->span != NULL);
        if (i > 0) {
            assert(region->first_measure == regions->regions[i - 1].last_measure + 1);
            assert(regions->changes[i - 1].from_region == i - 1 && regions->changes[i - 1].to_region == i);
            assert(!regions->changes[i - 1].cadence_confirmed);
            assert(regions->regions[i].key_index != regions->regions[i - 1].key_index);
            /* the spans meet at a barline */
            assert(musical_time_equals(time_span_end(regions->regions[i - 1].span), time_span_start(region->span)));
        }
    }
}

static void test_a_single_key_piece_is_one_region(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 8);
    assert(regions.region_count == 1 && regions.change_count == 0);
    assert(regions.regions[0].key_index == 0);
    assert(strcmp(key_label(regions.regions[0].key), "C major") == 0);
    assert(time_is(time_span_start(regions.regions[0].span), 0, 1) && time_is(time_span_end(regions.regions[0].span), 8, 1));
    assert(strcmp(entity_id_value(interpretation_target_id(key_interpretation(regions.regions[0].key))),
                  "key-region-1") == 0);
    common_practice_key_regions_destroy(&regions);
    common_practice_key_regions_destroy(&regions);
    common_practice_key_regions_destroy(NULL);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_modulation_from_c_to_g_is_two_regions(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 16);
    assert(regions.region_count == 2);
    assert(regions.regions[0].key_index == 0 && regions.regions[1].key_index == 7);
    assert(strcmp(key_label(regions.regions[1].key), "G major") == 0);
    assert(regions.regions[1].first_measure >= 8 && regions.regions[1].first_measure <= 10);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_MODULATION && !regions.changes[0].is_return);
    assert(strcmp(entity_id_value(interpretation_target_id(key_interpretation(regions.regions[1].key))),
                  "key-region-2") == 0);
    common_practice_key_regions_destroy(&regions);

    /* a penalty above the correlation range allows no change; zero follows every window */
    CommonPracticeKeyRegionsConfig stiff = common_practice_key_regions_config_default();
    stiff.change_penalty = 10.0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &stiff, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_brief_excursion_that_returns_is_a_tonicization(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 6);
    add_run(&piece, &measure, 0, 8);
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.tonicization_max_measures = 8;
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 22);
    assert(regions.region_count == 3);
    assert(regions.regions[0].key_index == 0 && regions.regions[1].key_index == 7 && regions.regions[2].key_index == 0);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_TONICIZATION && !regions.changes[0].is_return);
    assert(regions.changes[1].extent == KEY_CHANGE_EXTENT_TONICIZATION && regions.changes[1].is_return);
    common_practice_key_regions_destroy(&regions);

    /* the same excursion, no longer "short" under the default limit of 4 measures, is a weak modulation */
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_WEAK_MODULATION);
    assert(regions.changes[1].extent == KEY_CHANGE_EXTENT_WEAK_MODULATION && regions.changes[1].is_return);
    assert(key_change_extent_is_modulation(regions.changes[0].extent));
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_the_same_input_gives_the_same_regions(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 6);
    add_run(&piece, &measure, 7, 6);
    add_run(&piece, &measure, 2, 6);
    CommonPracticeKeyRegions a = {0};
    CommonPracticeKeyRegions b = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &a) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &b) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(a.region_count == b.region_count && a.change_count == b.change_count);
    for (size_t i = 0; i < a.region_count; i++) {
        assert(a.regions[i].key_index == b.regions[i].key_index);
        assert(a.regions[i].first_measure == b.regions[i].first_measure);
        assert(a.regions[i].last_measure == b.regions[i].last_measure);
        assert(a.regions[i].uncertainty == b.regions[i].uncertainty);
    }
    for (size_t i = 0; i < a.change_count; i++) {
        assert(a.changes[i].extent == b.changes[i].extent && a.changes[i].is_return == b.changes[i].is_return);
    }
    common_practice_key_regions_destroy(&a);
    common_practice_key_regions_destroy(&b);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_region_input_errors(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 4);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, NULL) == COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(key_regions_estimate(piece.items, 0, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS);

    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.change_penalty = -0.1;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config.change_penalty = NAN;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.tonicization_max_measures = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.context_id_prefix = "";
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config.context_id_prefix = NULL;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.context_id_prefix = "piece-7";
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(strcmp(entity_id_value(interpretation_target_id(key_interpretation(regions.regions[0].key))), "piece-7-1") == 0);
    common_practice_key_regions_destroy(&regions);
    assert(regions.regions == NULL);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

/* --- pivot chords --- */

static void make_chord(CommonPracticeMember members[4], CommonPracticeSegment *segment, const int pitches[4],
                       const char *id) {
    for (int i = 0; i < 4; i++) {
        members[i].note_id = NULL;
        members[i].pitch = pitches[i];
        members[i].held = false;
        members[i].duration = 1.0;
        members[i].strength = BEAT_STRENGTH_STRONG;
        members[i].voice_id = NULL;
        members[i].onset = 0.0;
        members[i].resolves_in_segment = false;
    }
    segment->members = members;
    segment->member_count = 4;
    segment->id = id;
    segment->beat = 0;
    segment->has_bass = false;
    segment->bass_pitch = 0;
}

/* C x8 (the last measure's segment is a C major chord), then a run in `tonic` x8. */
static void build_modulation(Piece *piece, const CommonPracticeSegment *chord, int tonic) {
    long measure = 0;
    add_run(piece, &measure, 0, 8);
    piece->items[7].source = chord;
    add_run(piece, &measure, tonic, 8);
}

static void test_a_chord_in_both_keys_is_the_pivot(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    CommonPracticeMember members[4];
    CommonPracticeSegment chord;
    const int c_major[4] = {48, 60, 64, 67}; /* I in C, IV in G */
    make_chord(members, &chord, c_major, "chord-c");
    build_modulation(&piece, &chord, 7);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2 && regions.regions[1].key_index == 7);
    assert(regions.changes[0].has_pivot && regions.changes[0].pivot_segment == 7);
    common_practice_key_regions_destroy(&regions);

    /* a search distance of 0 reaches only segments at the boundary, and the chord is a measure before it */
    CommonPracticeKeyRegionsConfig none = common_practice_key_regions_config_default();
    none.pivot_search_measures = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &none, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert(regions.regions[1].first_measure == 9 ? !regions.changes[0].has_pivot : true);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

/* A minor, minor weights: tonic, third and fifth heavier; the raised seventh G# sounds, not G. */
static void minor_weights(int tonic, double out[12]) {
    static const int SCALE[7] = {0, 2, 3, 5, 7, 8, 11};
    static const double WEIGHT[7] = {3.0, 1.0, 2.0, 1.2, 2.5, 1.0, 0.8};
    memset(out, 0, 12 * sizeof(double));
    for (int i = 0; i < 7; i++) {
        out[(tonic + SCALE[i]) % 12] = WEIGHT[i];
    }
}

/* A minor x8 (the last measure's segment is the chord), then a run in E major x8. */
static void build_minor_modulation(Piece *piece, const CommonPracticeSegment *chord) {
    long measure = 0;
    double weights[12];
    minor_weights(9, weights);
    for (int i = 0; i < 8; i++) {
        add_measure(piece, measure++, weights);
    }
    piece->items[7].source = chord;
    add_run(piece, &measure, 4, 8);
}

static void test_a_pivot_chord_in_a_minor_old_key_is_read_in_minor(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    CommonPracticeMember members[4];
    CommonPracticeSegment chord;
    /* E G# B E: the V of A minor, which is also the I of E major. Read in A minor's own vocabulary it is the
       raised-leading-tone dominant; read in the relative major C (the old stand-in) it was an iii with no third
       that C major does not contain, and no pivot. */
    const int e_major[4] = {40, 56, 59, 64};
    make_chord(members, &chord, e_major, "chord-e");
    build_minor_modulation(&piece, &chord);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert(regions.regions[0].key_index == 21 && regions.regions[1].key_index == 4);
    assert(regions.changes[0].has_pivot && regions.changes[0].pivot_segment == 7);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_direct_modulation_has_no_pivot(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    CommonPracticeMember members[4];
    CommonPracticeSegment chord;
    const int c_major[4] = {48, 60, 64, 67};
    make_chord(members, &chord, c_major, "chord-c");
    build_modulation(&piece, &chord, 6); /* F# major shares no note of C major's triad */
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2 && regions.regions[1].key_index == 6);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_MODULATION);
    assert(!regions.changes[0].has_pivot);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_tonicization_has_no_pivot_but_a_weak_modulation_does(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    CommonPracticeMember members[4];
    CommonPracticeSegment chord;
    const int c_major[4] = {48, 60, 64, 67};
    make_chord(members, &chord, c_major, "chord-c");
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    piece.items[7].source = &chord;
    add_run(&piece, &measure, 7, 6);
    add_run(&piece, &measure, 0, 8);

    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.tonicization_max_measures = 8;
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3 && regions.changes[0].extent == KEY_CHANGE_EXTENT_TONICIZATION);
    assert(!regions.changes[0].has_pivot);
    common_practice_key_regions_destroy(&regions);

    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3 && regions.changes[0].extent == KEY_CHANGE_EXTENT_WEAK_MODULATION);
    assert(regions.changes[0].has_pivot && regions.changes[0].pivot_segment == 7);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

/* --- key signature events --- */

static CommonPracticeKeySignatureEvent make_event(Piece *piece, long numerator, unsigned long denominator, int sharps) {
    CommonPracticeKeySignatureEvent event = {time_at(piece, numerator, denominator), sharps};
    return event;
}

static const CommonPracticeKeyRegion *region_starting_at(const CommonPracticeKeyRegions *regions, int measure) {
    for (size_t i = 0; i < regions->region_count; i++) {
        if (regions->regions[i].first_measure == measure) {
            return &regions->regions[i];
        }
    }
    return NULL;
}

static void test_a_signature_change_forces_a_boundary_against_the_profile(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8); /* the notes say G major throughout the second half */
    CommonPracticeKeyRegions regions = {0};

    /* without the event the boundary is wherever the path puts it, and the key is G */
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2 && regions.regions[1].key_index == 7);
    assert(regions.regions[1].source == COMMON_PRACTICE_REGION_SOURCE_PROFILE);
    common_practice_key_regions_destroy(&regions);

    /* one flat at measure 8 (F major / D minor) forces a boundary there, though G major fits the notes */
    CommonPracticeKeySignatureEvent one_flat = make_event(&piece, 8, 1, -1);
    assert(key_regions_estimate(piece.items, piece.count, map, &one_flat, 1, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 16);
    const CommonPracticeKeyRegion *forced = region_starting_at(&regions, 9);
    assert(forced != NULL);
    assert(forced->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    assert(forced->key_index == 5 || forced->key_index == 14);
    assert(regions.regions[0].last_measure == 8 && regions.regions[0].key_index == 0);
    assert(regions.regions[0].source == COMMON_PRACTICE_REGION_SOURCE_PROFILE);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_signature_at_the_start_restricts_only_the_first_region(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 7, 12); /* G major notes */
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1 && regions.regions[0].key_index == 7);
    common_practice_key_regions_destroy(&regions);

    CommonPracticeKeySignatureEvent none = make_event(&piece, 0, 1, 0); /* C major / A minor */
    assert(key_regions_estimate(piece.items, piece.count, map, &none, 1, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 12);
    assert(regions.region_count >= 2);
    assert(regions.regions[0].key_index == 0 || regions.regions[0].key_index == 21);
    assert(regions.regions[0].source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    assert(regions.regions[regions.region_count - 1].key_index == 7); /* free again after the first region */
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_repeated_and_late_events_are_not_boundaries(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 16);
    CommonPracticeKeySignatureEvent events[3] = {make_event(&piece, 0, 1, 0), make_event(&piece, 8, 1, 0),
                                                 make_event(&piece, 100, 1, 1)};
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, events, 3, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1 && regions.regions[0].key_index == 0);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_events_snap_to_the_nearest_barline(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions regions = {0};
    CommonPracticeKeySignatureEvent early = make_event(&piece, 33, 4, -1); /* 8 + 1/4 whole notes */
    assert(key_regions_estimate(piece.items, piece.count, map, &early, 1, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(region_starting_at(&regions, 9) != NULL &&
           region_starting_at(&regions, 9)->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    common_practice_key_regions_destroy(&regions);
    CommonPracticeKeySignatureEvent late = make_event(&piece, 35, 4, -1); /* 8 + 3/4 */
    assert(key_regions_estimate(piece.items, piece.count, map, &late, 1, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(region_starting_at(&regions, 10) != NULL &&
           region_starting_at(&regions, 10)->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_bad_events(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    CommonPracticeKeyRegions regions = {0};
    CommonPracticeKeySignatureEvent bad = make_event(&piece, 4, 1, 8);
    assert(key_regions_estimate(piece.items, piece.count, map, &bad, 1, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT);
    bad = make_event(&piece, 4, 1, -8);
    assert(key_regions_estimate(piece.items, piece.count, map, &bad, 1, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT);
    CommonPracticeKeySignatureEvent no_position = {NULL, 0};
    assert(key_regions_estimate(piece.items, piece.count, map, &no_position, 1, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT);
    CommonPracticeKeySignatureEvent swapped[2] = {make_event(&piece, 6, 1, 1), make_event(&piece, 2, 1, 2)};
    assert(key_regions_estimate(piece.items, piece.count, map, swapped, 2, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 2, NULL, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(regions.regions == NULL);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

/* --- functional evidence, phrase boundaries and the held home key --- */

static unsigned triad_mask(int root, int third, int fifth) {
    return (1u << (root % 12)) | (1u << ((root + third) % 12)) | (1u << ((root + fifth) % 12));
}

static void set_chord(Piece *piece, size_t segment, int root, unsigned mask) {
    piece->items[segment].chord.present = true;
    piece->items[segment].chord.root_pitch_class = root;
    piece->items[segment].chord.pitch_classes = mask;
}

static void test_functional_evidence_favours_the_key_a_dominant_resolves_into(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    double flat[12];
    for (int pc = 0; pc < 12; pc++) {
        flat[pc] = 1.0;
    }
    flat[0] = 1.1;
    for (long m = 0; m < 4; m++) {
        add_measure(&piece, m, flat);
    }
    /* G major then C major, twice: a dominant to tonic motion in C, and a leading tone resolving up */
    set_chord(&piece, 0, 7, triad_mask(7, 4, 7));
    set_chord(&piece, 1, 0, triad_mask(0, 4, 7));
    set_chord(&piece, 2, 7, triad_mask(7, 4, 7));
    set_chord(&piece, 3, 0, triad_mask(0, 4, 7));
    CommonPracticeKeyWindows windows = {0};
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.count == 1);
    const CommonPracticeKeyWindow *window = &windows.items[0];
    assert(window->evidence[0] > window->evidence[7] && window->evidence[0] > window->evidence[5]);
    assert(window->evidence[0] > window->evidence[common_practice_key_index_of(0, COMMON_PRACTICE_KEY_MINOR)]); /* not C minor */
    assert(window->evidence[0] > 0.0 && window->evidence[0] <= 0.8 + 1e-12); /* weight 0.8, evidence per chord <= 1 */
    common_practice_key_windows_destroy(&windows);

    /* no chords, no evidence */
    for (size_t i = 0; i < piece.count; i++) {
        memset(&piece.items[i].chord, 0, sizeof(piece.items[i].chord));
    }
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        assert(windows.items[0].evidence[k] == 0.0);
    }
    common_practice_key_windows_destroy(&windows);

    /* a dominant into a minor tonic is evidence for the minor key, not its parallel major */
    int a_minor = common_practice_key_index_of(9, COMMON_PRACTICE_KEY_MINOR);
    int a_major = common_practice_key_index_of(9, COMMON_PRACTICE_KEY_MAJOR);
    memset(&piece.items[2].chord, 0, sizeof(piece.items[2].chord));
    memset(&piece.items[3].chord, 0, sizeof(piece.items[3].chord));
    set_chord(&piece, 0, 4, triad_mask(4, 4, 7)); /* E G# B */
    set_chord(&piece, 1, 9, triad_mask(9, 3, 7)); /* A C E */
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.items[0].evidence[a_minor] > 0.0 && windows.items[0].evidence[a_major] == 0.0);
    common_practice_key_windows_destroy(&windows);

    /* the raised seventh after a minor chord counts for the minor key only; after nothing minor it does not */
    set_chord(&piece, 0, 9, triad_mask(9, 3, 7)); /* A C E */
    set_chord(&piece, 1, 4, triad_mask(4, 4, 7)); /* E G# B */
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.items[0].evidence[a_minor] > 0.0 && windows.items[0].evidence[a_major] == 0.0);
    common_practice_key_windows_destroy(&windows);
    set_chord(&piece, 0, 2, triad_mask(2, 4, 7)); /* D F# A: no minor colour for A minor */
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(windows.items[0].evidence[a_minor] == 0.0);
    common_practice_key_windows_destroy(&windows);

    /* a zero weight turns the evidence off */
    CommonPracticeKeyRegionsConfig off = common_practice_key_regions_config_default();
    off.evidence_weight = 0.0;
    assert(key_window_scores(piece.items, piece.count, map, &off, &windows) == COMMON_PRACTICE_KEY_REGIONS_OK);
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        assert(windows.items[0].evidence[k] == 0.0);
    }
    common_practice_key_windows_destroy(&windows);
    off.evidence_weight = -0.1;
    assert(key_window_scores(piece.items, piece.count, map, &off, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    piece.items[0].chord.root_pitch_class = 12;
    assert(key_window_scores(piece.items, piece.count, map, NULL, &windows) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_SEGMENT);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_edges_snap_to_a_nearby_phrase_boundary(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions plain = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &plain) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(plain.region_count == 2);
    int edge = plain.regions[1].first_measure;
    common_practice_key_regions_destroy(&plain);

    /* a boundary one measure from the edge, whichever side it is on, takes the edge */
    for (int target = edge - 1; target <= edge + 1; target++) {
        MusicalTime *boundary = time_at(&piece, target - 1, 1); /* the start of measure `target` */
        const MusicalTime *list[1] = {boundary};
        CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
        config.phrase_boundaries = list;
        config.phrase_boundary_count = 1;
        CommonPracticeKeyRegions snapped = {0};
        assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &snapped) == COMMON_PRACTICE_KEY_REGIONS_OK);
        assert_gap_free(&snapped, 1, 16);
        assert(snapped.region_count == 2 && snapped.regions[1].first_measure == target);
        assert(snapped.regions[0].key_index == 0 && snapped.regions[1].key_index == 7);
        common_practice_key_regions_destroy(&snapped);
    }

    /* a boundary beyond the reach is ignored, and reach 0 keeps every edge */
    MusicalTime *far = time_at(&piece, edge + 2, 1);
    const MusicalTime *far_list[1] = {far};
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.phrase_boundaries = far_list;
    config.phrase_boundary_count = 1;
    CommonPracticeKeyRegions kept = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(kept.regions[1].first_measure == edge);
    common_practice_key_regions_destroy(&kept);
    config.snap_reach_measures = 3;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(kept.regions[1].first_measure == edge + 3);
    common_practice_key_regions_destroy(&kept);
    config.snap_reach_measures = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(kept.regions[1].first_measure == edge);
    common_practice_key_regions_destroy(&kept);

    /* a boundary at the start of the piece, or past its end, moves nothing */
    MusicalTime *before = time_at(&piece, 0, 1);
    MusicalTime *after = time_at(&piece, 40, 1);
    const MusicalTime *outside[2] = {before, after};
    config = common_practice_key_regions_config_default();
    config.phrase_boundaries = outside;
    config.phrase_boundary_count = 2;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(kept.regions[1].first_measure == edge);
    common_practice_key_regions_destroy(&kept);

    /* a bad list */
    config.phrase_boundaries = NULL;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    const MusicalTime *missing[1] = {NULL};
    config.phrase_boundaries = missing;
    config.phrase_boundary_count = 1;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &kept) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_EVENT);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_an_edge_at_a_key_signature_does_not_snap(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeySignatureEvent events[2] = {make_event(&piece, 0, 1, 0), make_event(&piece, 8, 1, 1)};
    const MusicalTime *list[1] = {time_at(&piece, 9, 1)};
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.phrase_boundaries = list;
    config.phrase_boundary_count = 1;
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, events, 2, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2 && regions.regions[1].first_measure == 9);
    assert(regions.regions[1].source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

/* Weights of the diatonic collection of C major leaning on G: G major's tones with F for F#. */
static void g_leaning_diatonic_weights(double out[12]) {
    memset(out, 0, 12 * sizeof(double));
    out[7] = 3.0;
    out[2] = 2.5;
    out[11] = 2.0;
    out[0] = 1.2;
    out[9] = 1.0;
    out[4] = 1.0;
    out[5] = 0.5;
}

static void test_a_passage_of_iv_v_and_ii_keeps_the_home_key(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    double passage[12];
    g_leaning_diatonic_weights(passage);
    for (int i = 0; i < 6; i++) {
        add_measure(&piece, measure++, passage);
    }
    add_run(&piece, &measure, 0, 8);

    /* without chord data the profile follows the passage into the dominant key */
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count > 1);
    common_practice_key_regions_destroy(&regions);

    /* chords on V, ii and IV, all diatonic to C: the home key holds */
    for (size_t i = 8; i < 14; i++) {
        int roots[3] = {7, 2, 5};
        int root = roots[i % 3];
        set_chord(&piece, i, root, triad_mask(root, root == 7 || root == 5 ? 4 : 3, 7));
    }
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 22);
    assert(regions.region_count == 1 && regions.regions[0].key_index == 0);
    common_practice_key_regions_destroy(&regions);

    /* a chord off IV, V and ii (vi) lets the switch stand */
    set_chord(&piece, 10, 9, triad_mask(9, 3, 7));
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count > 1);
    common_practice_key_regions_destroy(&regions);

    /* so does the new key's leading tone (F# in the weights, beyond the tolerance) */
    set_chord(&piece, 10, 7, triad_mask(7, 4, 7));
    for (size_t i = 8; i < 14; i++) {
        piece.items[i].weights.pitch_class[6] = 1.0;
        piece.items[i].weights.total += 1.0;
    }
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count > 1);
    common_practice_key_regions_destroy(&regions);

    /* a tolerance wide enough to forgive that pitch holds the key again */
    CommonPracticeKeyRegionsConfig lax = common_practice_key_regions_config_default();
    lax.hold_tolerance = 0.5;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &lax, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1);
    common_practice_key_regions_destroy(&regions);
    lax.hold_tolerance = -1.0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &lax, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_the_chord_of_a_result_is_plain_data(void) {
    CommonPracticeWeightedResult none;
    memset(&none, 0, sizeof(none));
    assert(!common_practice_key_chord_of(&none).present);
    assert(!common_practice_key_chord_of(NULL).present);
}

/* --- keys on phrases --- */

#define C_BIT (1u << 0)
#define G_BIT (1u << 7)

static CommonPracticeKeyPhrase phrase_of(int first, int last, unsigned cadence_keys, unsigned arrival_keys, bool closes) {
    CommonPracticeKeyPhrase phrase = {first, last, cadence_keys, arrival_keys, closes};
    return phrase;
}

static CommonPracticeKeyRegionsConfig phrase_config(const CommonPracticeKeyPhrase *phrases, size_t count) {
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.phrases = phrases;
    config.phrase_count = count;
    return config;
}

static void assert_region(const CommonPracticeKeyRegions *regions, size_t index, int first, int last, int key) {
    assert(index < regions->region_count);
    assert(regions->regions[index].first_measure == first && regions->regions[index].last_measure == last);
    assert(regions->regions[index].key_index == key);
}

static void test_no_phrases_gives_the_window_regions(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions plain = {0};
    CommonPracticeKeyRegions same = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, NULL, &plain) == COMMON_PRACTICE_KEY_REGIONS_OK);
    /* a phrase list with a count of 0 and a repeat list with a count of 0 change nothing */
    CommonPracticeKeyPhrase unused = phrase_of(1, 16, G_BIT, 0, false);
    CommonPracticeKeyRepeat unused_repeat = {1, 9, 8};
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    assert(config.phrases == NULL && config.phrase_count == 0 && config.repeats == NULL && config.repeat_count == 0);
    config.phrases = &unused;
    config.repeats = &unused_repeat;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &same) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(plain.region_count == 2 && same.region_count == plain.region_count);
    for (size_t i = 0; i < plain.region_count; i++) {
        assert(plain.regions[i].first_measure == same.regions[i].first_measure);
        assert(plain.regions[i].last_measure == same.regions[i].last_measure);
        assert(plain.regions[i].key_index == same.regions[i].key_index);
    }
    assert(!plain.changes[0].at_arrival && !same.changes[0].at_arrival);
    common_practice_key_regions_destroy(&plain);
    common_practice_key_regions_destroy(&same);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_key_changes_only_when_a_phrase_cadences_in_it(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 16);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions regions = {0};

    CommonPracticeKeyPhrase confirmed[3] = {phrase_of(1, 8, C_BIT, 0, false), phrase_of(9, 16, C_BIT, 0, false),
                                            phrase_of(17, 24, G_BIT, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(confirmed, 3);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 24);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 16, 0);
    assert_region(&regions, 1, 17, 24, 7);
    assert(!regions.changes[0].at_arrival);
    common_practice_key_regions_destroy(&regions);

    /* the same music without a cadence in G: the G phrase is a tonicization inside C */
    confirmed[2].cadence_keys = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1);
    assert_region(&regions, 0, 1, 24, 0);
    common_practice_key_regions_destroy(&regions);

    /* a phrase list that leaves gaps and runs past the piece is closed up and clipped */
    CommonPracticeKeyPhrase loose[3] = {phrase_of(3, 8, C_BIT, 0, false), phrase_of(10, 16, C_BIT, 0, false),
                                        phrase_of(20, 40, G_BIT, 0, false)};
    config = phrase_config(loose, 3);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 24);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 16, 0); /* each phrase starts after the one before: 3-8 from 1, 10-16 from 9, 20-40 from 17 to 24 */
    assert_region(&regions, 1, 17, 24, 7);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_modulation_and_its_return_each_need_a_cadence(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    add_run(&piece, &measure, 0, 8);
    CommonPracticeKeyPhrase phrases[3] = {phrase_of(1, 8, C_BIT, 0, false), phrase_of(9, 16, G_BIT, 0, false),
                                          phrase_of(17, 24, C_BIT, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(phrases, 3);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 24);
    assert(regions.region_count == 3);
    assert_region(&regions, 0, 1, 8, 0);
    assert_region(&regions, 1, 9, 16, 7);
    assert_region(&regions, 2, 17, 24, 0);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_WEAK_MODULATION && regions.changes[1].is_return);
    common_practice_key_regions_destroy(&regions);

    /* without a cadence in G the excursion cannot start, and the return has nothing to return from */
    phrases[1].cadence_keys = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1);
    assert_region(&regions, 0, 1, 24, 0);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_change_belongs_at_the_elided_arrival(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions regions = {0};

    /* the C phrase ends on an arrival in G and the next phrase is G: the change is at the arrival measure */
    CommonPracticeKeyPhrase arrival[2] = {phrase_of(1, 8, C_BIT, G_BIT, false), phrase_of(9, 16, G_BIT, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(arrival, 2);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 16);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 7, 0);
    assert_region(&regions, 1, 8, 16, 7);
    assert(regions.changes[0].at_arrival);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);

    /* the modulating phrase has G music and its cadence arrives in G: it stays C, the next phrase is G */
    Piece five = {{{0}}, {0}, 0, 0};
    measure = 0;
    add_run(&five, &measure, 0, 12);
    add_run(&five, &measure, 7, 8);
    for (size_t i = 8; i < 12; i++) { /* the last C phrase moves V to I, which a G reading of it does not have */
        set_chord(&five, i, i % 2 == 0 ? 7 : 0, i % 2 == 0 ? triad_mask(7, 4, 7) : triad_mask(0, 4, 7));
    }
    CommonPracticeKeyPhrase modulating[5] = {phrase_of(1, 4, C_BIT, 0, false), phrase_of(5, 8, C_BIT, 0, false),
                                             phrase_of(9, 12, C_BIT, 0, false), phrase_of(13, 16, 0, G_BIT, false),
                                             phrase_of(17, 20, G_BIT, 0, false)};
    config = phrase_config(modulating, 5);
    assert(key_regions_estimate(five.items, five.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 20);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 15, 0);
    assert_region(&regions, 1, 16, 20, 7);
    assert(regions.changes[0].at_arrival);
    common_practice_key_regions_destroy(&regions);

    /* the same phrases without the arrival and with the cadence in the modulating phrase: it takes the new key whole */
    modulating[3].arrival_keys = 0;
    modulating[3].cadence_keys = G_BIT;
    assert(key_regions_estimate(five.items, five.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 12, 0);
    assert_region(&regions, 1, 13, 20, 7);
    assert(!regions.changes[0].at_arrival);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&five);
    meter_map_destroy(map);
}

static void test_a_closing_cadence_frees_the_next_phrase(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 16);
    add_run(&piece, &measure, 7, 8);
    CommonPracticeKeyRegions regions = {0};
    CommonPracticeKeyPhrase closing[3] = {phrase_of(1, 8, C_BIT, 0, false), phrase_of(9, 16, C_BIT, 0, true),
                                          phrase_of(17, 24, 0, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(closing, 3);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert_region(&regions, 0, 1, 16, 0);
    assert_region(&regions, 1, 17, 24, 7);
    common_practice_key_regions_destroy(&regions);

    closing[1].closes = false; /* the same music and cadences, but no closing cadence: nothing confirms G */
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1);
    assert_region(&regions, 0, 1, 24, 0);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_signature_restricts_the_first_phrase(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 16); /* C major notes under an F major signature */
    CommonPracticeKeySignatureEvent one_flat = make_event(&piece, 0, 1, -1);
    CommonPracticeKeyPhrase phrases[2] = {phrase_of(1, 3, 0, 0, false), phrase_of(4, 16, C_BIT, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(phrases, 2);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, &one_flat, 1, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 16);
    assert(regions.region_count == 2);
    assert(regions.regions[0].key_index == 5 || regions.regions[0].key_index == 14); /* F major or D minor, not C */
    assert(regions.regions[0].last_measure == 3 && regions.regions[0].source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    assert_region(&regions, 1, 4, 16, 0);
    common_practice_key_regions_destroy(&regions);

    /* a signature that starts in the middle of a phrase splits it: the part before keeps no cadence data */
    CommonPracticeKeyPhrase whole[1] = {phrase_of(1, 16, C_BIT, 0, false)};
    config = phrase_config(whole, 1);
    CommonPracticeKeySignatureEvent two_sharps = make_event(&piece, 8, 1, 2);
    assert(key_regions_estimate(piece.items, piece.count, map, &two_sharps, 1, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 16);
    assert(regions.region_count == 2);
    assert(regions.regions[1].first_measure == 9 && regions.regions[1].source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    assert(regions.regions[1].key_index == 2 || regions.regions[1].key_index == 23); /* D major or B minor */
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_repeat_copies_its_keys(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    double flat[12];
    for (int pc = 0; pc < 12; pc++) {
        flat[pc] = 1.0;
    }
    add_run(&piece, &measure, 0, 8);
    add_run(&piece, &measure, 7, 8);
    for (int i = 0; i < 8; i++) {
        add_measure(&piece, measure++, flat); /* music that says nothing about the key */
    }
    CommonPracticeKeyPhrase phrases[3] = {phrase_of(1, 8, C_BIT, 0, false), phrase_of(9, 16, G_BIT, 0, false),
                                          phrase_of(17, 24, 0, 0, false)};
    CommonPracticeKeyRegionsConfig config = phrase_config(phrases, 3);
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert_region(&regions, 1, 9, 24, 7);
    common_practice_key_regions_destroy(&regions);

    CommonPracticeKeyRepeat repeat = {1, 17, 8};
    config.repeats = &repeat;
    config.repeat_count = 1;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 24);
    assert(regions.region_count == 3);
    assert_region(&regions, 0, 1, 8, 0);
    assert_region(&regions, 1, 9, 16, 7);
    assert_region(&regions, 2, 17, 24, 0);
    common_practice_key_regions_destroy(&regions);

    /* a repeat partly outside the piece copies what is inside it */
    repeat.later_first = 21;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3);
    assert_region(&regions, 1, 9, 20, 7);
    assert_region(&regions, 2, 21, 24, 0);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_a_repeat_does_not_copy_the_first_region_of_a_signature_span(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    double flat[12];
    for (int pc = 0; pc < 12; pc++) {
        flat[pc] = 1.0;
    }
    add_run(&piece, &measure, 0, 16); /* C major notes under an F major signature */
    for (int i = 0; i < 8; i++) {
        add_measure(&piece, measure++, flat);
    }
    CommonPracticeKeySignatureEvent one_flat = make_event(&piece, 0, 1, -1);
    CommonPracticeKeyPhrase phrases[3] = {phrase_of(1, 8, 0, 0, false), phrase_of(9, 16, C_BIT, 0, false),
                                          phrase_of(17, 24, 0, 0, false)};
    CommonPracticeKeyRepeat repeat = {1, 17, 8};
    CommonPracticeKeyRegionsConfig config = phrase_config(phrases, 3);
    config.repeats = &repeat;
    config.repeat_count = 1;
    CommonPracticeKeyRegions regions = {0};
    assert(key_regions_estimate(piece.items, piece.count, map, &one_flat, 1, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 1, 24);
    assert(regions.region_count == 2);
    assert(regions.regions[0].last_measure == 8 && regions.regions[0].source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE);
    assert_region(&regions, 1, 9, 24, 0); /* measures 17 to 24 keep C: the signature's first region is not copied */
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

static void test_cadence_keys_read_the_arrival(void) {
    CommonPracticeKeyChord c_major = {true, 0, triad_mask(0, 4, 7)};
    CommonPracticeKeyChord a_minor = {true, 9, triad_mask(9, 3, 7)};
    CommonPracticeKeyChord g_major = {true, 7, triad_mask(7, 4, 7)};
    CommonPracticeKeyChord g_seventh = {true, 7, triad_mask(7, 4, 7) | (1u << 5)};
    CommonPracticeKeyChord b_diminished = {true, 11, triad_mask(11, 3, 6)};
    CommonPracticeKeyChord both_thirds = {true, 0, triad_mask(0, 4, 7) | (1u << 3)};
    CommonPracticeKeyChord bare_fifth = {true, 0, (1u << 0) | (1u << 7)};
    CommonPracticeKeyChord absent = {false, 0, 0};

    /* a perfect authentic cadence arrives on the tonic triad: its key, in the mode of its third */
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &c_major) == 1u << 0);
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &a_minor) == 1u << 21);
    /* an arrival that does not settle its third, or has none, confirms nothing */
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &both_thirds) == 0);
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &bare_fifth) == 0);
    /* a half cadence arrives on a major dominant: the major and minor keys a fifth below */
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_HALF, &g_major) == ((1u << 0) | (1u << 12)));
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_HALF, &g_seventh) == ((1u << 0) | (1u << 12)));
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_HALF, &a_minor) == 0);
    /* a diminished arrival confirms nothing, for either kind */
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_HALF, &b_diminished) == 0);
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &b_diminished) == 0);
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, &absent) == 0);
    assert(common_practice_cadence_keys(COMMON_PRACTICE_CADENCE_AUTHENTIC, NULL) == 0);
    assert(common_practice_cadence_keys((CommonPracticeCadenceKind)7, &c_major) == 0);
}

static void test_bad_phrase_configs(void) {
    MeterMap *map = four_four();
    Piece piece = {{{0}}, {0}, 0, 0};
    long measure = 0;
    add_run(&piece, &measure, 0, 8);
    CommonPracticeKeyRegions regions = {0};
    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();

    config.phrase_count = 1; /* a count with no list */
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.repeat_count = 1;
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);

    CommonPracticeKeyPhrase out_of_order[2] = {phrase_of(5, 8, 0, 0, false), phrase_of(1, 4, 0, 0, false)};
    config = phrase_config(out_of_order, 2);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    CommonPracticeKeyPhrase overlapping[2] = {phrase_of(1, 5, 0, 0, false), phrase_of(5, 8, 0, 0, false)};
    config = phrase_config(overlapping, 2);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    CommonPracticeKeyPhrase backwards[1] = {phrase_of(6, 2, 0, 0, false)};
    config = phrase_config(backwards, 1);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) ==
           COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    assert(regions.regions == NULL && regions.region_count == 0);

    /* phrases wholly outside the piece leave it as one phrase */
    CommonPracticeKeyPhrase outside[1] = {phrase_of(20, 30, G_BIT, 0, false)};
    config = phrase_config(outside, 1);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, 0, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 1 && regions.regions[0].key_index == 0);
    common_practice_key_regions_destroy(&regions);
    destroy_piece(&piece);
    meter_map_destroy(map);
}

int main(void) {
    test_a_scale_scores_its_own_key_highest();
    test_a_minor_distribution_scores_the_minor_key();
    test_the_windows_follow_a_change_of_key();
    test_a_short_piece_gets_one_window();
    test_a_silent_window_scores_zero();
    test_a_custom_profile_set_is_used();
    test_bad_input();
    test_key_index_helpers();
    test_a_single_key_piece_is_one_region();
    test_a_modulation_from_c_to_g_is_two_regions();
    test_a_brief_excursion_that_returns_is_a_tonicization();
    test_the_same_input_gives_the_same_regions();
    test_region_input_errors();
    test_a_chord_in_both_keys_is_the_pivot();
    test_a_pivot_chord_in_a_minor_old_key_is_read_in_minor();
    test_a_direct_modulation_has_no_pivot();
    test_a_tonicization_has_no_pivot_but_a_weak_modulation_does();
    test_a_signature_change_forces_a_boundary_against_the_profile();
    test_a_signature_at_the_start_restricts_only_the_first_region();
    test_repeated_and_late_events_are_not_boundaries();
    test_events_snap_to_the_nearest_barline();
    test_bad_events();
    test_functional_evidence_favours_the_key_a_dominant_resolves_into();
    test_edges_snap_to_a_nearby_phrase_boundary();
    test_an_edge_at_a_key_signature_does_not_snap();
    test_a_passage_of_iv_v_and_ii_keeps_the_home_key();
    test_the_chord_of_a_result_is_plain_data();
    test_no_phrases_gives_the_window_regions();
    test_a_key_changes_only_when_a_phrase_cadences_in_it();
    test_a_modulation_and_its_return_each_need_a_cadence();
    test_a_change_belongs_at_the_elided_arrival();
    test_a_closing_cadence_frees_the_next_phrase();
    test_a_signature_restricts_the_first_phrase();
    test_a_repeat_copies_its_keys();
    test_a_repeat_does_not_copy_the_first_region_of_a_signature_span();
    test_cadence_keys_read_the_arrival();
    test_bad_phrase_configs();
    printf("test_key_regions: ok\n");
    return 0;
}
