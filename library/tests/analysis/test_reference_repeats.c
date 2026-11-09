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
 * The repeats and returns the chain finds, and the grid that follows them (R-400). The chain is analysis_chain_run to the form stage,
 * every default the library's, with the file's key signature events and C major as the home key.
 *
 * Asserted for Clementi 36: the returns are measures 1-4 to 24-27 (-12 semitones) and 8-12 to 31-35 (-7), no repeat.
 * Asserted for Mozart 545:
 *   - the repeats are measures 1-28 to 29-56 and 57-101 to 102-146, the return is 1-8 to 70-77 (+5);
 *   - the grid units of the second development are 101-108 (elided), 108-115 (elided), 115-122 (elided), 123-130 and 131-146;
 *   - its cadences and phrases in measures 102 to 146 are those of measures 57 to 101 moved by 45.
 * URSATZ_CORPUS_DIR comes from CMake.
 */

#define MAX_CADENCES 32

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

static int measure_of(const MeterMap *map, double at) {
    for (int measure = 1; measure < 400; measure++) {
        TimeSpan *span = NULL;
        if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
            break;
        }
        double from = to_double(time_span_start(span));
        double to = to_double(time_span_end(span));
        time_span_destroy(span);
        if (from - 1e-9 <= at && at < to - 1e-9) {
            return measure;
        }
    }
    return -1;
}

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

static bool has_item(const StandardFormRepeats *repeats, StandardFormRepeatKind kind, int ef, int el, int lf, int ll, int transposition) {
    for (size_t i = 0; i < repeats->count; i++) {
        const StandardFormRepeat *r = &repeats->items[i];
        if (r->kind == kind && r->earlier_first == ef && r->earlier_last == el && r->later_first == lf && r->later_last == ll &&
            r->transposition == transposition) {
            return true;
        }
    }
    return false;
}

static size_t kind_count(const StandardFormRepeats *repeats, StandardFormRepeatKind kind) {
    size_t n = 0;
    for (size_t i = 0; i < repeats->count; i++) {
        n += repeats->items[i].kind == kind ? 1 : 0;
    }
    return n;
}

static void check_clementi(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&CLEMENTI, &p, &key_events);
    printf("%s\n", CLEMENTI.name);
    const StandardFormRepeats *repeats = analysis_chain_repeats(chain);
    assert(repeats != NULL);
    CHECK(repeats->count == 2 && kind_count(repeats, STANDARD_FORM_REPEAT) == 0, "Clementi: %zu items, expected 2 returns", repeats->count);
    CHECK(has_item(repeats, STANDARD_FORM_RETURN, 1, 4, 24, 27, -12), "Clementi: return 1-4 to 24-27 (-12) missing");
    CHECK(has_item(repeats, STANDARD_FORM_RETURN, 8, 12, 31, 35, -7), "Clementi: return 8-12 to 31-35 (-7) missing");
    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

static void check_mozart(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&MOZART, &p, &key_events);
    printf("%s\n", MOZART.name);
    const StandardFormRepeats *repeats = analysis_chain_repeats(chain);
    assert(repeats != NULL);
    CHECK(repeats->count == 3 && kind_count(repeats, STANDARD_FORM_REPEAT) == 2, "Mozart: %zu items, expected 2 repeats and 1 return",
          repeats->count);
    CHECK(has_item(repeats, STANDARD_FORM_REPEAT, 1, 28, 29, 56, 0), "Mozart: repeat 1-28 to 29-56 missing");
    CHECK(has_item(repeats, STANDARD_FORM_REPEAT, 57, 101, 102, 146, 0), "Mozart: repeat 57-101 to 102-146 missing");
    CHECK(has_item(repeats, STANDARD_FORM_RETURN, 1, 8, 70, 77, 5), "Mozart: return 1-8 to 70-77 (+5) missing");

    const StandardFormPhraseGrid *grid = analysis_chain_phrase_grid(chain);
    assert(grid != NULL);
    static const int UNITS[5][3] = {{101, 108, 1}, {108, 115, 1}, {115, 122, 1}, {123, 130, 0}, {131, 146, 0}};
    size_t first = grid->count;
    for (size_t i = 0; i < grid->count; i++) {
        if (grid->units[i].last_measure == 108 && grid->units[i].first_measure == 101) {
            first = i;
        }
    }
    printf("  grid units:");
    for (size_t i = 0; i < grid->count; i++) {
        printf(" %d-%d%s", grid->units[i].first_measure, grid->units[i].last_measure, grid->units[i].elided ? "e" : "");
    }
    printf("\n");
    CHECK(first + 5 == grid->count, "Mozart: the grid does not end with the five units of the second development");
    for (size_t k = 0; first + k < grid->count && k < 5; k++) {
        const StandardFormGridUnit *u = &grid->units[first + k];
        CHECK(u->first_measure == UNITS[k][0] && u->last_measure == UNITS[k][1] && u->elided == (UNITS[k][2] == 1),
              "Mozart: development unit %zu is %d-%d, expected %d-%d", k, u->first_measure, u->last_measure, UNITS[k][0], UNITS[k][1]);
    }

    /* The cadences of 102 to 146 are those of 57 to 101 moved by 45. */
    const ChainSegments *segments = analysis_chain_segments(chain);
    const HarmonyCadences *cadences = analysis_chain_cadences(chain);
    assert(cadences->count <= MAX_CADENCES);
    int earlier[MAX_CADENCES][2];
    int later[MAX_CADENCES][2];
    size_t earlier_count = 0;
    size_t later_count = 0;
    for (size_t i = 0; i < cadences->count; i++) {
        const HarmonyCadenceResult *c = &cadences->items[i];
        int measure = measure_of(p.set.map, segments->starts[c->arrival_first]);
        if (measure >= 57 && measure <= 101) {
            earlier[earlier_count][0] = measure + 45;
            earlier[earlier_count++][1] = c->type;
        } else if (measure >= 102 && measure <= 146) {
            later[later_count][0] = measure;
            later[later_count++][1] = c->type;
        }
    }
    printf("  cadences 57-101: %zu, 102-146: %zu\n", earlier_count, later_count);
    CHECK(earlier_count == 6 && later_count == 6, "Mozart: %zu cadences in 57-101 and %zu in 102-146, expected 6 and 6", earlier_count,
          later_count);
    for (size_t i = 0; i < earlier_count && i < later_count; i++) {
        CHECK(earlier[i][0] == later[i][0] && earlier[i][1] == later[i][1], "Mozart: cadence %zu is m%d type %d, expected m%d type %d", i,
              later[i][0], later[i][1], earlier[i][0], earlier[i][1]);
    }

    /* The phrases of 102 to 146 are those of 57 to 101 moved by 45. */
    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    assert(phrases != NULL);
    int pe[MAX_CADENCES][2];
    int pl[MAX_CADENCES][2];
    size_t pe_count = 0;
    size_t pl_count = 0;
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhraseClaim *c = &phrases->items[i].claim;
        if (c->first_measure >= 57 && c->last_measure <= 101 && pe_count < MAX_CADENCES) {
            pe[pe_count][0] = c->first_measure + 45;
            pe[pe_count++][1] = c->last_measure + 45;
        } else if (c->first_measure >= 102 && pl_count < MAX_CADENCES) {
            pl[pl_count][0] = c->first_measure;
            pl[pl_count++][1] = c->last_measure;
        }
    }
    printf("  phrases 57-101: %zu, 102-146: %zu\n", pe_count, pl_count);
    CHECK(pe_count == 6 && pl_count == 6, "Mozart: %zu phrases in 57-101 and %zu in 102-146, expected 6 and 6", pe_count, pl_count);
    for (size_t i = 0; i < pe_count && i < pl_count; i++) {
        CHECK(pe[i][0] == pl[i][0] && pe[i][1] == pl[i][1], "Mozart: phrase %zu is %d-%d, expected %d-%d", i, pl[i][0], pl[i][1], pe[i][0],
              pe[i][1]);
    }
    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

int main(void) {
    check_clementi();
    check_mozart();
    if (failures > 0) {
        printf("test_reference_repeats: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_reference_repeats: ok\n");
    return 0;
}
