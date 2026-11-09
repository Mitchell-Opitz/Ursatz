#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantics/analysis_key.h"
#include "ursatz_chain/analysis_chain.h"
#include "weighted_pipeline.h"

/*
 * The two-voice check of the harmonic span rules (R-379, R-382, R-384). The Composer's exports are two-voice textures: a bass line under a
 * melody, with no inner voices to complete a triad. Read on harmonic spans they must keep every perfect authentic cadence and every theme
 * the chain finds without spans, and the chord before each cadence's arrival must have its bass on its root.
 *
 * Each fixture in tests/fixtures/two-voice (README.md names its source) is run through pipeline_run and analysis_chain_run to the form stage,
 * with the file's key signature and its mode as the key event and the home key, with harmonic_spans off and on. Asserted:
 *   - the values of R-379 without spans (the sentence has one theme and a perfect authentic cadence in measure 8; the period the same; the
 *     fill one theme and perfect authentic cadences in measures 8 and 16);
 *   - every perfect authentic cadence (by its arrival measure) and every theme (its form, first measure and length) of the run without spans
 *     is in the run with spans;
 *   - in the run with spans, the chord before each of those cadences' arrival has its bass on its root.
 * Both runs are printed.
 */

#define MAX_ITEMS 16

typedef struct {
    size_t pac_count;
    int pac_measure[MAX_ITEMS];
    bool bass_on_root[MAX_ITEMS]; /* the chord before the arrival */
    size_t theme_count;
    int theme_form[MAX_ITEMS];
    int theme_first[MAX_ITEMS];
    int theme_length[MAX_ITEMS];
} Run;

typedef struct {
    const char *file;
    size_t pacs;          /* the perfect authentic cadences without spans (R-379) */
    int pac_measures[2];
    size_t themes;
} Fixture;

static const Fixture FIXTURES[3] = {
    {"sentence-c-major.mid", 1, {8, 0}, 1},
    {"period-a-minor.mid", 1, {8, 0}, 1},
    {"fill.mid", 2, {8, 16}, 1},
};

static int measure_of(const MeterMap *map, double at) {
    for (int measure = 1; measure < 400; measure++) {
        TimeSpan *span = NULL;
        if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
            break;
        }
        const MusicalTime *start = time_span_start(span);
        const MusicalTime *end = time_span_end(span);
        double from = (double)musical_time_numerator(start) / (double)musical_time_denominator(start);
        double to = (double)musical_time_numerator(end) / (double)musical_time_denominator(end);
        time_span_destroy(span);
        if (from - 1e-9 <= at && at < to - 1e-9) {
            return measure;
        }
    }
    return -1;
}

static Run run_fixture(const char *file, bool spans) {
    WeightedPipeline p = pipeline_run(URSATZ_TWO_VOICE_DIR, file, NULL);
    size_t key_event_count = p.import.key_signatures.count;
    assert(key_event_count > 0);
    AnalysisKeyEvent *key_events = (AnalysisKeyEvent *)calloc(key_event_count, sizeof(AnalysisKeyEvent));
    assert(key_events != NULL);
    for (size_t i = 0; i < key_event_count; i++) {
        const MidiKeySignatureEvent *event = &p.import.key_signatures.events[i];
        KeyStructureMode mode = event->mode_byte == 1 ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
        key_events[i].time = event->position;
        assert(analysis_key_from_fifths(event->sharps_flats, mode, &key_events[i].key));
    }
    /* the home key: the first key signature's tonic and mode */
    const MidiKeySignatureEvent *first = &p.import.key_signatures.events[0];
    bool minor = first->mode_byte == 1;
    int major_tonic = ((first->sharps_flats * 7) % 12 + 12) % 12;
    int tonic = minor ? (major_tonic + 9) % 12 : major_tonic;

    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    config.first_measure = 1;
    config.home_key_index = common_practice_key_index_of(tonic, minor ? COMMON_PRACTICE_KEY_MINOR : COMMON_PRACTICE_KEY_MAJOR);
    config.harmonic_spans = spans;
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run(&p.set, key_events, key_event_count, &config, &chain) == ANALYSIS_CHAIN_OK);

    Run run;
    memset(&run, 0, sizeof(run));
    const ChainSegments *segments = analysis_chain_segments(chain);
    const StandardTonalityChordInput *inputs = analysis_chain_chord_inputs(chain);
    const HarmonyCadences *cadences = analysis_chain_cadences(chain);
    for (size_t i = 0; i < cadences->count; i++) {
        const HarmonyCadenceResult *cadence = &cadences->items[i];
        if (cadence->type != CADENCE_TYPE_PERFECT_AUTHENTIC) {
            continue;
        }
        assert(run.pac_count < MAX_ITEMS);
        size_t before = cadence->arrival_first > 0 ? cadence->arrival_first - 1 : 0;
        run.pac_measure[run.pac_count] = measure_of(p.set.map, segments->starts[cadence->arrival_first]);
        run.bass_on_root[run.pac_count] = cadence->arrival_first > 0 && inputs[before].bass_pitch_class == inputs[before].root_pitch_class;
        run.pac_count++;
    }
    const StandardFormThemes *themes = analysis_chain_themes(chain);
    for (size_t i = 0; i < themes->count; i++) {
        assert(run.theme_count < MAX_ITEMS);
        run.theme_form[run.theme_count] = themes->items[i].claim.form;
        run.theme_first[run.theme_count] = themes->items[i].claim.first_measure;
        run.theme_length[run.theme_count] = themes->items[i].claim.length_measures;
        run.theme_count++;
    }
    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
    return run;
}

static void print_run(const char *file, const char *label, const Run *run) {
    printf("  %s %s: %zu theme(s)", file, label, run->theme_count);
    for (size_t i = 0; i < run->theme_count; i++) {
        printf(" [form %d, m%d, %d measures]", run->theme_form[i], run->theme_first[i], run->theme_length[i]);
    }
    printf("; %zu perfect authentic cadence(s)", run->pac_count);
    for (size_t i = 0; i < run->pac_count; i++) {
        printf(" m%d (chord before has its bass on its root: %s)", run->pac_measure[i], run->bass_on_root[i] ? "yes" : "no");
    }
    printf("\n");
}

static void check_fixture(const Fixture *fixture) {
    Run without = run_fixture(fixture->file, false);
    Run with = run_fixture(fixture->file, true);
    print_run(fixture->file, "without spans", &without);
    print_run(fixture->file, "with spans   ", &with);

    /* the R-379 values of the run without spans */
    assert(without.theme_count == fixture->themes);
    assert(without.pac_count == fixture->pacs);
    for (size_t i = 0; i < fixture->pacs; i++) {
        assert(without.pac_measure[i] == fixture->pac_measures[i]);
    }

    /* nothing the run without spans finds is lost on spans */
    for (size_t i = 0; i < without.pac_count; i++) {
        bool found = false;
        for (size_t j = 0; j < with.pac_count; j++) {
            found = found || with.pac_measure[j] == without.pac_measure[i];
        }
        if (!found) {
            printf("  FAIL %s: the perfect authentic cadence of measure %d is lost on spans\n", fixture->file, without.pac_measure[i]);
        }
        assert(found);
    }
    for (size_t i = 0; i < without.theme_count; i++) {
        bool found = false;
        for (size_t j = 0; j < with.theme_count; j++) {
            found = found || (with.theme_form[j] == without.theme_form[i] && with.theme_first[j] == without.theme_first[i] &&
                              with.theme_length[j] == without.theme_length[i]);
        }
        if (!found) {
            printf("  FAIL %s: the theme (form %d, m%d, %d measures) is lost on spans\n", fixture->file, without.theme_form[i],
                   without.theme_first[i], without.theme_length[i]);
        }
        assert(found);
    }

    /* on spans, the chord before each cadence's arrival has its bass on its root */
    for (size_t j = 0; j < with.pac_count; j++) {
        for (size_t i = 0; i < without.pac_count; i++) {
            if (with.pac_measure[j] == without.pac_measure[i] && !with.bass_on_root[j]) {
                printf("  FAIL %s: the chord before the cadence of measure %d has its bass off its root\n", fixture->file,
                       with.pac_measure[j]);
            }
            assert(with.pac_measure[j] != without.pac_measure[i] || with.bass_on_root[j]);
        }
    }
}

int main(void) {
    printf("The Composer's two-voice exports, with and without harmonic spans\n");
    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); i++) {
        check_fixture(&FIXTURES[i]);
    }
    printf("test_two_voice_spans: ok\n");
    return 0;
}
