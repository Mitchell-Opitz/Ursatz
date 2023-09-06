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
    assert(windows.count == 3 && windows.first_measure == 0 && windows.last_measure == 5);
    for (size_t w = 0; w < windows.count; w++) {
        assert(windows.items[w].first_measure == (int)w && windows.items[w].last_measure == (int)w + 3);
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
    assert(windows.count == 1 && windows.items[0].first_measure == 0 && windows.items[0].last_measure == 1);
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
        assert(region->source == COMMON_PRACTICE_REGION_SOURCE_PROFILE);
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 0, 7);
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 0, 15);
    assert(regions.region_count == 2);
    assert(regions.regions[0].key_index == 0 && regions.regions[1].key_index == 7);
    assert(strcmp(key_label(regions.regions[1].key), "G major") == 0);
    assert(regions.regions[1].first_measure >= 7 && regions.regions[1].first_measure <= 9);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_MODULATION && !regions.changes[0].is_return);
    assert(strcmp(entity_id_value(interpretation_target_id(key_interpretation(regions.regions[1].key))),
                  "key-region-2") == 0);
    common_practice_key_regions_destroy(&regions);

    /* a penalty above the correlation range allows no change; zero follows every window */
    CommonPracticeKeyRegionsConfig stiff = common_practice_key_regions_config_default();
    stiff.change_penalty = 10.0;
    assert(key_regions_estimate(piece.items, piece.count, map, &stiff, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
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
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert_gap_free(&regions, 0, 21);
    assert(regions.region_count == 3);
    assert(regions.regions[0].key_index == 0 && regions.regions[1].key_index == 7 && regions.regions[2].key_index == 0);
    assert(regions.changes[0].extent == KEY_CHANGE_EXTENT_TONICIZATION && !regions.changes[0].is_return);
    assert(regions.changes[1].extent == KEY_CHANGE_EXTENT_TONICIZATION && regions.changes[1].is_return);
    common_practice_key_regions_destroy(&regions);

    /* the same excursion, no longer "short" under the default limit of 4 measures, is a weak modulation */
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &a) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &b) == COMMON_PRACTICE_KEY_REGIONS_OK);
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, NULL) == COMMON_PRACTICE_KEY_REGIONS_ERR_NULL_ARGUMENT);
    assert(key_regions_estimate(piece.items, 0, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_NO_SEGMENTS);

    CommonPracticeKeyRegionsConfig config = common_practice_key_regions_config_default();
    config.change_penalty = -0.1;
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config.change_penalty = NAN;
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.tonicization_max_measures = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.context_id_prefix = "";
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config.context_id_prefix = NULL;
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG);
    config = common_practice_key_regions_config_default();
    config.context_id_prefix = "piece-7";
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
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
    }
    segment->members = members;
    segment->member_count = 4;
    segment->id = id;
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2 && regions.regions[1].key_index == 7);
    assert(regions.changes[0].has_pivot && regions.changes[0].pivot_segment == 7);
    common_practice_key_regions_destroy(&regions);

    /* a search distance of 0 reaches only segments at the boundary, and the chord is a measure before it */
    CommonPracticeKeyRegionsConfig none = common_practice_key_regions_config_default();
    none.pivot_search_measures = 0;
    assert(key_regions_estimate(piece.items, piece.count, map, &none, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 2);
    assert(regions.regions[1].first_measure == 8 ? !regions.changes[0].has_pivot : true);
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
    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
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
    assert(key_regions_estimate(piece.items, piece.count, map, &config, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3 && regions.changes[0].extent == KEY_CHANGE_EXTENT_TONICIZATION);
    assert(!regions.changes[0].has_pivot);
    common_practice_key_regions_destroy(&regions);

    assert(key_regions_estimate(piece.items, piece.count, map, NULL, &regions) == COMMON_PRACTICE_KEY_REGIONS_OK);
    assert(regions.region_count == 3 && regions.changes[0].extent == KEY_CHANGE_EXTENT_WEAK_MODULATION);
    assert(regions.changes[0].has_pivot && regions.changes[0].pivot_segment == 7);
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
    test_a_direct_modulation_has_no_pivot();
    test_a_tonicization_has_no_pivot_but_a_weak_modulation_does();
    printf("test_key_regions: ok\n");
    return 0;
}
