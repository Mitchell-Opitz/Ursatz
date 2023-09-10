#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_key_signature.h"
#include "key_identification.h"
#include "key_regions.h"
#include "weighted_pipeline.h"

/*
 * Local key regions on Clementi, Sonatina Op. 36 No. 1 (first movement) and Mozart, K. 545 (first movement),
 * against tests/fixtures/annotations/clementi36_key_regions.txt and mozart545_key_regions.txt: DRAFT hand
 * readings, not yet checked against the scores (see their headers), written before the code was run. For each
 * piece the test
 *   - finds the key regions of the annotated measures from the adaptive sonority segments, the weighted pitch
 *     classes and the file's key signature events (read as midi_key_signature_scan_all returns them),
 *   - asserts that every annotated MODULATION boundary has an estimated key change within 2 measures,
 *   - asserts the share of scored measures that carry the annotated key, and prints it, with the misses and the
 *     estimated regions, changes and pivots,
 *   - asserts that the whole-piece estimator (key_identification.h, unchanged) still gives its answers.
 * The defaults are not tuned to these files. URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR come from CMake.
 */

typedef struct {
    int first;
    int last;
    int key_index;
} Region;

typedef struct {
    int measure;
    bool modulation;
} Change;

typedef struct {
    Region regions[32];
    size_t region_count;
    int unscored[8][2];
    size_t unscored_count;
    Change changes[16];
    size_t change_count;
    int scope_first;
    int scope_last;
} Annotation;

static int tonic_of(const char *name) {
    static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    for (int pc = 0; pc < 12; pc++) {
        if (strcmp(name, NAMES[pc]) == 0) {
            return pc;
        }
    }
    assert(!"unknown tonic name");
    return -1;
}

static Annotation load_annotation(const char *file) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_ANNOTATION_DIR, file);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Annotation a;
    memset(&a, 0, sizeof(a));
    a.scope_first = 0;
    a.scope_last = 0;
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[16];
        char tonic[8];
        char mode[16];
        int x = 0;
        int y = 0;
        if (sscanf(line, "region %d %d %7s %15s", &x, &y, tonic, mode) == 4) {
            assert(a.region_count < 32);
            a.regions[a.region_count].first = x;
            a.regions[a.region_count].last = y;
            a.regions[a.region_count].key_index =
                common_practice_key_index_of(tonic_of(tonic), strcmp(mode, "minor") == 0 ? COMMON_PRACTICE_KEY_MINOR
                                                                                           : COMMON_PRACTICE_KEY_MAJOR);
            a.region_count++;
        } else if (sscanf(line, "unscored %d %d", &x, &y) == 2) {
            assert(a.unscored_count < 8);
            a.unscored[a.unscored_count][0] = x;
            a.unscored[a.unscored_count][1] = y;
            a.unscored_count++;
        } else if (sscanf(line, "change %d %15s", &x, word) == 2) {
            assert(a.change_count < 16);
            a.changes[a.change_count].measure = x;
            a.changes[a.change_count].modulation = strcmp(word, "modulation") == 0;
            a.change_count++;
        } else if (sscanf(line, "scope %d %d", &x, &y) == 2) {
            a.scope_first = x;
            a.scope_last = y;
        } else {
            assert(!"unrecognised annotation line");
        }
    }
    free(text);
    assert(a.region_count > 0);
    if (a.scope_last == 0) {
        a.scope_first = a.regions[0].first;
        a.scope_last = a.regions[a.region_count - 1].last;
    }
    return a;
}

static int expected_key(const Annotation *a, int measure) {
    for (size_t i = 0; i < a->unscored_count; i++) {
        if (measure >= a->unscored[i][0] && measure <= a->unscored[i][1]) {
            return -1;
        }
    }
    for (size_t i = 0; i < a->region_count; i++) {
        if (measure >= a->regions[i].first && measure <= a->regions[i].last) {
            return a->regions[i].key_index;
        }
    }
    return -1;
}

static int estimated_key(const CommonPracticeKeyRegions *regions, int measure_number) {
    int index = measure_number - 1;
    for (size_t i = 0; i < regions->region_count; i++) {
        if (index >= regions->regions[i].first_measure && index <= regions->regions[i].last_measure) {
            return regions->regions[i].key_index;
        }
    }
    return -1;
}

static const char *key_name(int key_index, char out[16]) {
    snprintf(out, 16, "%s %s", common_practice_key_index_tonic_name(key_index),
             common_practice_key_index_mode(key_index) == COMMON_PRACTICE_KEY_MINOR ? "minor" : "major");
    return out;
}

static void report_piece(const char *label, const char *file, const char *annotation_file, double scope_notes_end,
                         bool assert_results) {
    Annotation annotation = load_annotation(annotation_file);
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, file, NULL);

    /* the segments of the annotated scope */
    size_t count = 0;
    while (count < p.segment_count && p.starts[count] < scope_notes_end) {
        count++;
    }
    CommonPracticeKeySegment *segments = (CommonPracticeKeySegment *)calloc(count, sizeof(CommonPracticeKeySegment));
    assert(segments != NULL);
    CommonPracticeWeightedConfig weighting = common_practice_weighted_config_default();
    for (size_t s = 0; s < count; s++) {
        const TimeSpan *window = sonority_time_window(p.sonorities.items[s].sonority);
        segments[s].start = time_span_start(window);
        segments[s].end = time_span_end(window);
        segments[s].source = &p.segments[s];
        assert(common_practice_weight_segment(p.segments[s].members, p.segments[s].member_count, &weighting,
                                              &segments[s].weights) == COMMON_PRACTICE_WEIGHTED_OK);
    }

    /* the file's key signature events, as the importer's scan returns them */
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
    size_t size = 0;
    unsigned char *data = pipeline_read_file(path, &size);
    MidiKeySignatureEvents scanned = {NULL, 0};
    assert(midi_key_signature_scan_all(data, size, &scanned) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    free(data);
    CommonPracticeKeySignatureEvent *events =
        (CommonPracticeKeySignatureEvent *)calloc(scanned.count + 1, sizeof(CommonPracticeKeySignatureEvent));
    assert(events != NULL);
    for (size_t i = 0; i < scanned.count; i++) {
        events[i].position = scanned.events[i].position;
        events[i].sharps_flats = scanned.events[i].sharps_flats;
    }

    printf("%s (%s), measures %d-%d, %zu segments, %zu key signature event(s):\n", label, file,
           annotation.scope_first, annotation.scope_last, count, scanned.count);
    for (int with_events = 1; with_events >= 0; with_events--) {
        CommonPracticeKeyRegions regions = {0};
        assert(key_regions_estimate(segments, count, p.meter.meter_map, with_events ? events : NULL,
                                    with_events ? scanned.count : 0, NULL, &regions) ==
               COMMON_PRACTICE_KEY_REGIONS_OK);
        printf(" %s the key signature events:\n", with_events ? "with" : "without");
        for (size_t i = 0; i < regions.region_count; i++) {
            char name[16];
            const CommonPracticeKeyRegion *r = &regions.regions[i];
            printf("  measures %d-%d  %-8s uncertainty %.2f%s", r->first_measure + 1, r->last_measure + 1,
                   key_name(r->key_index, name), r->uncertainty,
                   r->source == COMMON_PRACTICE_REGION_SOURCE_KEY_SIGNATURE ? "  (key signature)" : "");
            if (i > 0) {
                const CommonPracticeKeyChange *c = &regions.changes[i - 1];
                printf("  <- %s%s%s", key_change_extent_name(c->extent), c->is_return ? " (return)" : "",
                       c->has_pivot ? ", pivot found" : (key_change_extent_is_modulation(c->extent) ? ", direct" : ""));
            }
            printf("\n");
        }

        size_t scored = 0;
        size_t right = 0;
        int run_start = 0;
        int run_expected = -2;
        for (int m = annotation.scope_first; m <= annotation.scope_last; m++) {
            int want = expected_key(&annotation, m);
            if (want < 0) {
                continue;
            }
            scored++;
            int got = estimated_key(&regions, m);
            right += got == want ? 1 : 0;
            if (got != want) {
                char a[16];
                char b[16];
                printf("   miss at measure %d: annotated %s, estimated %s\n", m, key_name(want, a),
                       got >= 0 ? key_name(got, b) : "(none)");
            }
            (void)run_start;
            (void)run_expected;
        }
        printf("  %zu of %zu scored measures carry the annotated key (%.1f%%)\n", right, scored,
               100.0 * (double)right / (double)scored);

        size_t found = 0;
        size_t asked = 0;
        for (size_t c = 0; c < annotation.change_count; c++) {
            if (!annotation.changes[c].modulation) {
                continue;
            }
            asked++;
            bool near = false;
            int nearest = 1000;
            for (size_t i = 1; i < regions.region_count; i++) {
                int boundary = regions.regions[i].first_measure + 1;
                int distance = abs(boundary - annotation.changes[c].measure);
                nearest = distance < nearest ? distance : nearest;
                near = near || distance <= 2;
            }
            found += near ? 1 : 0;
            if (!near) {
                printf("   modulation at measure %d not found within 2 measures (nearest change %d away)\n",
                       annotation.changes[c].measure, nearest);
            }
        }
        printf("  %zu of %zu annotated modulation boundaries found within 2 measures\n\n", found, asked);

        if (with_events && assert_results) {
            assert(found == asked);
            assert((double)right / (double)scored >= 0.80);
        }
        common_practice_key_regions_destroy(&regions);
    }

    midi_key_signature_events_destroy(&scanned);
    free(events);
    free(segments);
    pipeline_destroy(&p);
}

/* The whole-piece estimator is unchanged: its answers for these pieces' pitch classes (every pitch class that
   sounds, tonic-relative). */
static void check_whole_piece_estimator(void) {
    CommonPracticeKeyMode mode;
    /* Clementi 36: C D D# E F F# G A B */
    int clementi_in_c[9] = {0, 2, 3, 4, 5, 6, 7, 9, 11};
    assert(common_practice_identify_key_mode(clementi_in_c, 9, &mode) && mode == COMMON_PRACTICE_KEY_MAJOR);
    int clementi_in_e[9] = {8, 10, 11, 0, 1, 2, 3, 5, 7}; /* relative to E */
    assert(common_practice_identify_key_mode(clementi_in_e, 9, &mode) && mode == COMMON_PRACTICE_KEY_MINOR);
    int clementi_in_a[9] = {3, 5, 6, 7, 8, 9, 10, 0, 2}; /* relative to A */
    assert(common_practice_identify_key_mode(clementi_in_a, 9, &mode) && mode == COMMON_PRACTICE_KEY_MINOR);
    int clementi_in_d[9] = {10, 0, 1, 2, 3, 4, 5, 7, 9}; /* relative to D: no key matches */
    assert(!common_practice_identify_key_mode(clementi_in_d, 9, &mode));
    /* Mozart 545: all twelve pitch classes sound, so the major collection matches first on any tonic */
    int all_twelve[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    assert(common_practice_identify_key_mode(all_twelve, 12, &mode) && mode == COMMON_PRACTICE_KEY_MAJOR);
}

int main(void) {
    check_whole_piece_estimator();
    /* Clementi 36: 38 measures of 4/4, one whole note each */
    report_piece("Clementi, Sonatina Op. 36 No. 1, first movement", "Clementi 36.mid", "clementi36_key_regions.txt",
                 38.0, true);
    /* Mozart 545: the first movement is the first 146 measures of 4/4 */
    report_piece("Mozart, K. 545, first movement", "Mozart 545.mid", "mozart545_key_regions.txt", 146.0, true);
    printf("test_key_regions_corpus: ok\n");
    return 0;
}
