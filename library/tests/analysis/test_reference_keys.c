#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics/analysis_key.h"
#include "standard-form/phrase_level_form.h"
#include "ursatz_chain/analysis_chain.h"
#include "weighted_pipeline.h"

/*
 * The chain's keys against Michelle's readings of the Clementi and Mozart reference pieces (R-401): the keys are placed on the phrases of the
 * phrase-first pass and everything is read once more in them (R-397 to R-399). The chain is analysis_chain_run to the form stage, every
 * default the library's, with the file's key signature events and C major as the home key. The readings are
 * tests/fixtures/annotations/{clementi36,mozart545}_key_regions.txt and *_v1_expected.txt.
 *
 * Asserted for Clementi 36: the key regions are exactly C major 1-7, G major 8-15, C minor 16-23 and C major 24-38; the grid units, the
 * cadences claimed and the period of measures 1 to 8 are those test_reference_phrases asserts.
 * Asserted for Mozart 545:
 *   - the key regions begin C major 1-12, G major 13-28, C major 29-40, G major 41-56; the key is G minor at measures 57 and 102, F major
 *     over 70-77 and 115-122, C major over 79-101 and 124-146;
 *   - the key by measure, the numeral labels and the claimed cadences of measures 29 to 56 are those of 1 to 28 (moved by 28), and of 102 to
 *     146 those of 57 to 101 (moved by 45);
 *   - the key by measure agrees with mozart545_key_regions.txt in at least 124 of the 146 measures (84.9%; 106 of 146, 72.6%, before).
 * Reported, not asserted, with the causes (R-401): measures 78 and 123 read C major (annotated F major: the phrase 78-81 ends with the
 * imperfect authentic cadence in C at measure 81); the development after its opening (D minor 60-63 and F major 64-69, annotated G minor:
 * the sequence's D minor phrase ends with the authentic cadence at measure 63, the unit 63-70 with the perfect authentic cadence in F); and
 * Clementi's perfect authentic cadence at measure 15 (measure 14 reads iii6 in G, not V7: the span seventh penalty, R-376).
 * URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR come from CMake.
 */

#define MAX_CADENCES 32
#define MAX_MEASURES 400

typedef struct {
    const char *name;
    const char *midi;
} Piece;

static const Piece CLEMENTI = {"Clementi 36", "Clementi 36.mid"};
static const Piece MOZART = {"Mozart 545", "Mozart 545.mid"};

static int failures = 0;

#define CHECK(condition, ...)                \
    do {                                     \
        if (!(condition)) {                  \
            printf("  FAIL " __VA_ARGS__);   \
            printf("\n");                    \
            failures++;                      \
        }                                    \
    } while (0)

static double to_double(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

/* The measure a time in whole notes lies in, and whether it is the measure's first beat. */
static int measure_of(const MeterMap *map, double at, bool *beat_one) {
    for (int measure = 1; measure < MAX_MEASURES; measure++) {
        TimeSpan *span = NULL;
        if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
            break;
        }
        double from = to_double(time_span_start(span));
        double to = to_double(time_span_end(span));
        time_span_destroy(span);
        if (from - 1e-9 <= at && at < to - 1e-9) {
            if (beat_one != NULL) {
                *beat_one = fabs(at - from) < 1e-9;
            }
            return measure;
        }
    }
    return -1;
}

typedef struct {
    int measure;
    bool beat_one;
    int type;
    int alternative_measure; /* 0 with none */
    bool alternative_beat_one;
    int alternative_type;
} Claimed;

static AnalysisChain *run_chain(const Piece *piece, WeightedPipeline *p, AnalysisKeyEvent **key_events_out) {
    *p = pipeline_run(URSATZ_CORPUS_DIR, piece->midi, NULL);
    size_t key_event_count = p->import.key_signatures.count;
    AnalysisKeyEvent *key_events = (AnalysisKeyEvent *)calloc(key_event_count + 1, sizeof(AnalysisKeyEvent));
    assert(key_events != NULL);
    for (size_t i = 0; i < key_event_count; i++) {
        const MidiKeySignatureEvent *event = &p->import.key_signatures.events[i];
        key_events[i].time = event->position;
        assert(analysis_key_from_fifths(event->sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &key_events[i].key));
    }
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    config.first_measure = 1;
    config.home_key_index = common_practice_key_index_of(0, COMMON_PRACTICE_KEY_MAJOR);
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run(&p->set, key_events, key_event_count, &config, &chain) == ANALYSIS_CHAIN_OK);
    *key_events_out = key_events;
    return chain;
}

static size_t claimed_of(const AnalysisChain *chain, const MeterMap *map, Claimed *out) {
    const ChainSegments *segments = analysis_chain_segments(chain);
    const HarmonyCadences *cadences = analysis_chain_cadences(chain);
    assert(cadences->count <= MAX_CADENCES);
    for (size_t i = 0; i < cadences->count; i++) {
        const HarmonyCadenceResult *c = &cadences->items[i];
        memset(&out[i], 0, sizeof(out[i]));
        out[i].measure = measure_of(map, segments->starts[c->arrival_first], &out[i].beat_one);
        out[i].type = c->type;
        if (c->alternative.present && c->alternative.reading == HARMONY_CADENCE_READING_CADENCE) {
            out[i].alternative_measure = measure_of(map, segments->starts[c->alternative.arrival_first], &out[i].alternative_beat_one);
            out[i].alternative_type = c->alternative.type;
        }
    }
    return cadences->count;
}

static const char *type_name(int type) {
    switch (type) {
    case CADENCE_TYPE_PERFECT_AUTHENTIC:
        return "PAC";
    case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
        return "IAC";
    case CADENCE_TYPE_AUTHENTIC_UNSPECIFIED:
        return "authentic";
    case CADENCE_TYPE_HALF:
        return "HC";
    default:
        return "other";
    }
}

static bool has_claimed(const Claimed *claimed, size_t count, int measure, int type) {
    for (size_t i = 0; i < count; i++) {
        if (claimed[i].measure == measure && claimed[i].beat_one && claimed[i].type == type) {
            return true;
        }
    }
    return false;
}

/* The key of every measure, -1 where no region holds it. */
static void keys_by_measure(const CommonPracticeKeyRegions *regions, int *keys) {
    for (int m = 0; m < MAX_MEASURES; m++) {
        keys[m] = -1;
        for (size_t i = 0; i < regions->region_count; i++) {
            if (m >= regions->regions[i].first_measure && m <= regions->regions[i].last_measure) {
                keys[m] = regions->regions[i].key_index;
            }
        }
    }
}

static const char *key_name(int key_index, char out[16]) {
    if (key_index < 0) {
        snprintf(out, 16, "(none)");
    } else {
        snprintf(out, 16, "%s %s", common_practice_key_index_tonic_name(key_index),
                 common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR ? "minor" : "major");
    }
    return out;
}

static int tonic_of(const char *name) {
    static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    for (int i = 0; i < 12; i++) {
        if (strcmp(name, NAMES[i]) == 0) {
            return i;
        }
    }
    assert(!"unknown tonic");
    return 0;
}

/* The annotated key of every measure (-1 for none) from a key regions file: the `region <first> <last> <tonic> <major|minor>` lines. */
static void annotated_keys(const char *file, int *keys) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_ANNOTATION_DIR, file);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    assert(text != NULL);
    for (int m = 0; m < MAX_MEASURES; m++) {
        keys[m] = -1;
    }
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        char tonic[8];
        char mode[16];
        int first = 0;
        int last = 0;
        if (sscanf(line, "region %d %d %7s %15s", &first, &last, tonic, mode) == 4) {
            int key = common_practice_key_index_of(tonic_of(tonic), strcmp(mode, "minor") == 0 ? COMMON_PRACTICE_KEY_MINOR
                                                                                                : COMMON_PRACTICE_KEY_MAJOR);
            for (int m = first; m <= last && m < MAX_MEASURES; m++) {
                keys[m] = key;
            }
        }
    }
    free(text);
}

static void check_region(const CommonPracticeKeyRegions *regions, size_t index, const char *piece, int first, int last, int tonic,
                         CommonPracticeKeyMode mode) {
    if (index >= regions->region_count) {
        CHECK(false, "%s: no key region %zu", piece, index + 1);
        return;
    }
    const CommonPracticeKeyRegion *r = &regions->regions[index];
    CHECK(r->first_measure == first && r->last_measure == last && r->key_index == common_practice_key_index_of(tonic, mode),
          "%s: key region %zu is %d-%d, key %d, expected %d-%d, key %d", piece, index + 1, r->first_measure, r->last_measure, r->key_index,
          first, last, common_practice_key_index_of(tonic, mode));
}

static void check_clementi(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&CLEMENTI, &p, &key_events);
    printf("%s, keys from phrases\n", CLEMENTI.name);

    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    assert(regions != NULL);
    printf("  key regions:");
    for (size_t i = 0; i < regions->region_count; i++) {
        char name[16];
        printf(" %d-%d %s", regions->regions[i].first_measure, regions->regions[i].last_measure, key_name(regions->regions[i].key_index, name));
    }
    printf("\n");
    CHECK(regions->region_count == 4, "Clementi: %zu key regions, expected 4", regions->region_count);
    check_region(regions, 0, "Clementi", 1, 7, 0, COMMON_PRACTICE_KEY_MAJOR);
    check_region(regions, 1, "Clementi", 8, 15, 7, COMMON_PRACTICE_KEY_MAJOR);
    check_region(regions, 2, "Clementi", 16, 23, 0, COMMON_PRACTICE_KEY_MINOR);
    check_region(regions, 3, "Clementi", 24, 38, 0, COMMON_PRACTICE_KEY_MAJOR);

    /* the grid, cadences and period of test_reference_phrases, unchanged */
    const StandardFormPhraseGrid *grid = analysis_chain_phrase_grid(chain);
    assert(grid != NULL);
    static const int UNITS[5][3] = {{1, 8, 0}, {8, 15, 1}, {16, 23, 0}, {24, 31, 0}, {32, 38, 0}};
    CHECK(grid->count == 5, "Clementi: %zu grid units, expected 5", grid->count);
    for (size_t i = 0; i < grid->count && i < 5; i++) {
        CHECK(grid->units[i].first_measure == UNITS[i][0] && grid->units[i].last_measure == UNITS[i][1] &&
                  grid->units[i].elided == (UNITS[i][2] == 1),
              "Clementi: grid unit %zu is %d-%d, expected %d-%d", i, grid->units[i].first_measure, grid->units[i].last_measure, UNITS[i][0],
              UNITS[i][1]);
    }
    Claimed claimed[MAX_CADENCES];
    size_t count = claimed_of(chain, p.set.map, claimed);
    CHECK(count == 4, "Clementi: %zu cadences claimed, expected 4", count);
    CHECK(has_claimed(claimed, count, 4, CADENCE_TYPE_HALF), "Clementi: no half cadence at measure 4 beat 1");
    CHECK(has_claimed(claimed, count, 8, CADENCE_TYPE_IMPERFECT_AUTHENTIC), "Clementi: no imperfect authentic cadence at measure 8 beat 1");
    CHECK(has_claimed(claimed, count, 23, CADENCE_TYPE_HALF), "Clementi: no half cadence at measure 23 beat 1");
    CHECK(has_claimed(claimed, count, 27, CADENCE_TYPE_HALF), "Clementi: no half cadence at measure 27 beat 1");
    bool alternative = false;
    for (size_t i = 0; i < count; i++) {
        if (claimed[i].measure == 23 && claimed[i].alternative_measure == 24 && claimed[i].alternative_beat_one &&
            claimed[i].alternative_type == CADENCE_TYPE_PERFECT_AUTHENTIC) {
            alternative = true;
        }
    }
    CHECK(alternative, "Clementi: the half cadence at measure 23 has no perfect authentic alternative at measure 24 beat 1");
    const StandardFormThemes *themes = analysis_chain_themes(chain);
    bool period = false;
    for (size_t i = 0; i < themes->count; i++) {
        const StandardFormThemeClaim *t = &themes->items[i].claim;
        if (t->form == PHRASE_LEVEL_FORM_PERIOD && t->first_measure == 1 && t->last_measure == 8 && t->first_part_first_measure == 1 &&
            t->first_part_last_measure == 4 && t->second_part_first_measure == 5 && t->second_part_last_measure == 8) {
            period = true;
        }
    }
    CHECK(period, "Clementi: no period of measures 1-8 with parts 1-4 and 5-8");

    /* reported, not asserted (R-401) */
    printf("  reported: PAC m15 %s (m14 reads iii6 in G, not V7: the span seventh penalty, R-376)\n",
           has_claimed(claimed, count, 15, CADENCE_TYPE_PERFECT_AUTHENTIC) ? "claimed" : "not claimed");

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

/* Each segment's numeral label ("" for none) and measure; returns the segment count. */
static size_t labels_of(const AnalysisChain *chain, const MeterMap *map, int *measures, const char **labels) {
    const ChainSegments *segments = analysis_chain_segments(chain);
    const StandardTonalityRomanResult *romans = analysis_chain_numerals(chain);
    size_t count = analysis_chain_segment_count(chain);
    assert(segments != NULL && romans != NULL);
    for (size_t i = 0; i < count; i++) {
        measures[i] = measure_of(map, segments->starts[i], NULL);
        labels[i] = romans[i].has_numeral ? romans[i].claim.label : "";
    }
    return count;
}

/* The labels of the segments starting in measures first to last are those of the segments starting in the measures `shift` earlier. */
static void check_same_numerals(const char *piece, const int *measures, const char **labels, size_t count, int first, int last, int shift) {
    size_t a = 0;
    size_t b = 0;
    size_t compared = 0;
    size_t differing = 0;
    int first_difference = 0;
    for (;;) {
        while (a < count && (measures[a] < first || measures[a] > last)) {
            a++;
        }
        while (b < count && (measures[b] < first - shift || measures[b] > last - shift)) {
            b++;
        }
        if (a >= count || b >= count) {
            break;
        }
        if (measures[a] - shift != measures[b] || strcmp(labels[a], labels[b]) != 0) {
            if (differing == 0) {
                first_difference = measures[a];
            }
            differing++;
        }
        compared++;
        a++;
        b++;
    }
    CHECK(differing == 0 && compared > 0 && a >= count && b >= count, "%s: numerals of measures %d-%d differ from those %d measures earlier (%zu of %zu, first at "
          "measure %d)", piece, first, last, shift, differing, compared, first_difference);
}

static void check_same_cadences(const char *piece, const Claimed *claimed, size_t count, int first, int last, int shift) {
    size_t later = 0;
    size_t earlier = 0;
    for (size_t i = 0; i < count; i++) {
        if (claimed[i].measure >= first && claimed[i].measure <= last) {
            later++;
            CHECK(has_claimed(claimed, count, claimed[i].measure - shift, claimed[i].type), "%s: %s at measure %d has no match at measure %d",
                  piece, type_name(claimed[i].type), claimed[i].measure, claimed[i].measure - shift);
        }
        if (claimed[i].measure >= first - shift && claimed[i].measure <= last - shift) {
            earlier++;
        }
    }
    CHECK(later == earlier, "%s: %zu cadences in measures %d-%d, %zu in the measures %d earlier", piece, later, first, last, earlier, shift);
}

static void check_mozart(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&MOZART, &p, &key_events);
    printf("%s, keys from phrases\n", MOZART.name);

    const CommonPracticeKeyRegions *regions = analysis_chain_key_regions(chain);
    assert(regions != NULL);
    int keys[MAX_MEASURES];
    keys_by_measure(regions, keys);
    printf("  key regions:");
    for (size_t i = 0; i < regions->region_count; i++) {
        char name[16];
        printf(" %d-%d %s", regions->regions[i].first_measure, regions->regions[i].last_measure, key_name(regions->regions[i].key_index, name));
    }
    printf("\n");
    check_region(regions, 0, "Mozart", 1, 12, 0, COMMON_PRACTICE_KEY_MAJOR);
    check_region(regions, 1, "Mozart", 13, 28, 7, COMMON_PRACTICE_KEY_MAJOR);
    check_region(regions, 2, "Mozart", 29, 40, 0, COMMON_PRACTICE_KEY_MAJOR);
    check_region(regions, 3, "Mozart", 41, 56, 7, COMMON_PRACTICE_KEY_MAJOR);
    const int g_minor = common_practice_key_index_of(7, COMMON_PRACTICE_KEY_MINOR);
    const int f_major = common_practice_key_index_of(5, COMMON_PRACTICE_KEY_MAJOR);
    const int c_major = common_practice_key_index_of(0, COMMON_PRACTICE_KEY_MAJOR);
    CHECK(keys[57] == g_minor, "Mozart: measure 57 is not G minor");
    CHECK(keys[102] == g_minor, "Mozart: measure 102 is not G minor");
    for (int m = 70; m <= 77; m++) {
        CHECK(keys[m] == f_major, "Mozart: measure %d is not F major", m);
    }
    for (int m = 115; m <= 122; m++) {
        CHECK(keys[m] == f_major, "Mozart: measure %d is not F major", m);
    }
    for (int m = 79; m <= 101; m++) {
        CHECK(keys[m] == c_major, "Mozart: measure %d is not C major", m);
    }
    for (int m = 124; m <= 146; m++) {
        CHECK(keys[m] == c_major, "Mozart: measure %d is not C major", m);
    }

    /* the repeats read the same: keys, numerals and cadences */
    for (int m = 1; m <= 28; m++) {
        CHECK(keys[m + 28] == keys[m], "Mozart: measure %d reads a different key from measure %d", m + 28, m);
    }
    for (int m = 57; m <= 101; m++) {
        CHECK(keys[m + 45] == keys[m], "Mozart: measure %d reads a different key from measure %d", m + 45, m);
    }
    int *measures = calloc(analysis_chain_segment_count(chain) + 1, sizeof(int));
    const char **labels = calloc(analysis_chain_segment_count(chain) + 1, sizeof(char *));
    assert(measures != NULL && labels != NULL);
    size_t segment_count = labels_of(chain, p.set.map, measures, labels);
    check_same_numerals("Mozart", measures, labels, segment_count, 29, 56, 28);
    check_same_numerals("Mozart", measures, labels, segment_count, 102, 146, 45);
    free(measures);
    free(labels);
    Claimed claimed[MAX_CADENCES];
    size_t count = claimed_of(chain, p.set.map, claimed);
    check_same_cadences("Mozart", claimed, count, 29, 56, 28);
    check_same_cadences("Mozart", claimed, count, 102, 146, 45);

    /* the key by measure against the annotation */
    int annotated[MAX_MEASURES];
    annotated_keys("mozart545_key_regions.txt", annotated);
    int agree = 0;
    for (int m = 1; m <= 146; m++) {
        agree += annotated[m] >= 0 && keys[m] == annotated[m] ? 1 : 0;
    }
    printf("  key by measure: %d of 146 agree with the annotation (%.1f%%)\n", agree, 100.0 * agree / 146.0);
    CHECK(agree >= 124, "Mozart: %d of 146 measures agree with the annotation, expected at least 124", agree);

    /* reported, not asserted (R-401) */
    char got[16];
    char want[16];
    printf("  reported: m78 reads %s and m123 reads %s (annotated F major: the phrase 78-81 ends with the IAC in C at m81)\n",
           key_name(keys[78], got), key_name(keys[123], want));
    printf("  reported: development keys m60-m69:");
    for (int m = 60; m <= 69; m++) {
        printf(" %s", key_name(keys[m], got));
    }
    printf(" (annotated G minor: the sequence's D minor phrase ends with the AC m63, the unit 63-70 with the PAC in F)\n");

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

int main(void) {
    check_clementi();
    check_mozart();
    if (failures > 0) {
        printf("test_reference_keys: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_reference_keys: ok\n");
    return 0;
}
