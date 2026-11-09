#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "standard-form/phrase_level_form.h"
#include "semantics/analysis_key.h"
#include "ursatz_chain/analysis_chain.h"
#include "weighted_pipeline.h"

/*
 * The chain's phrases against Michelle's readings of the Clementi and Mozart reference pieces (R-393; the readings are
 * tests/fixtures/annotations/{clementi36,mozart545}_v1_expected.txt and *_phrase_form.txt). The chain is analysis_chain_run to the form
 * stage, every default the library's (phrases first, R-389 to R-391), with the file's key signature events and C major as the home key
 * (both pieces are in C major).
 *
 * Asserted for Clementi 36:
 *   - the phrase grid units are 1-8, 8-15 (elided: it starts on the last measure of the unit before), 16-23, 24-31 and 32-38;
 *   - the cadences claimed are exactly a half cadence at measure 4 beat 1, an imperfect authentic cadence at measure 8 beat 1, a half
 *     cadence at measure 23 beat 1 with the perfect authentic reading as its alternative at measure 24 beat 1, and a half cadence at
 *     measure 27 beat 1;
 *   - the theme of measures 1 to 8 is a period with parts 1-4 and 5-8.
 * Asserted for Mozart 545: the phrases are measures 1-12 (half cadence at measure 12 beat 1) and 13-28 (perfect authentic cadence at
 * measure 28 beat 1), no phrase of one measure starts in measures 1 to 28, and exactly one cadence arrives in measures 26 to 28.
 *
 * Reported, not asserted, with the cause (R-393): Clementi's perfect authentic cadence at measure 15 (measure 14 reads vii6 in C, the key,
 * P42), at measure 31 (the V of measure 30 beat 4 is merged into the I6/4 span: chord spans) and at measure 38 (measure 37 reads iii6: the
 * span seventh penalty, R-376), and the type of measure 8, which Michelle reads as a perfect authentic cadence (the beat-level soprano
 * reads the passing A4, R-395). URSATZ_CORPUS_DIR comes from CMake.
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

/* The measure a time in whole notes lies in, and whether it is the measure's first beat. */
static int measure_of(const MeterMap *map, double at, bool *beat_one) {
    for (int measure = 1; measure < 400; measure++) {
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

static void check_clementi(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&CLEMENTI, &p, &key_events);

    printf("%s, phrases first\n", CLEMENTI.name);
    const StandardFormPhraseGrid *grid = analysis_chain_phrase_grid(chain);
    assert(grid != NULL);
    static const int UNITS[5][3] = {{1, 8, 0}, {8, 15, 1}, {16, 23, 0}, {24, 31, 0}, {32, 38, 0}};
    printf("  grid units:");
    for (size_t i = 0; i < grid->count; i++) {
        printf(" %d-%d%s", grid->units[i].first_measure, grid->units[i].last_measure, grid->units[i].elided ? " (elided)" : "");
    }
    printf("\n");
    CHECK(grid->count == 5, "Clementi: %zu grid units, expected 5", grid->count);
    for (size_t i = 0; i < grid->count && i < 5; i++) {
        CHECK(grid->units[i].first_measure == UNITS[i][0] && grid->units[i].last_measure == UNITS[i][1] &&
                  grid->units[i].elided == (UNITS[i][2] == 1),
              "Clementi: grid unit %zu is %d-%d, expected %d-%d", i, grid->units[i].first_measure, grid->units[i].last_measure, UNITS[i][0],
              UNITS[i][1]);
    }

    Claimed claimed[MAX_CADENCES];
    size_t count = claimed_of(chain, p.set.map, claimed);
    printf("  cadences claimed:");
    for (size_t i = 0; i < count; i++) {
        printf(" %s m%d%s", type_name(claimed[i].type), claimed[i].measure, claimed[i].beat_one ? " b1" : "");
        if (claimed[i].alternative_measure > 0) {
            printf(" (or %s m%d%s)", type_name(claimed[i].alternative_type), claimed[i].alternative_measure,
                   claimed[i].alternative_beat_one ? " b1" : "");
        }
    }
    printf("\n");
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

    /* reported, not asserted (R-393) */
    printf("  reported: PAC m15 %s (m14 reads vii6 in C: the key, P42); PAC m31 %s (m30 beat 4's V is merged into the I6/4 span: chord "
           "spans); PAC m38 %s (m37 reads iii6: the span seventh penalty, R-376); m8 is %s (annotated PAC; the beat-level soprano reads "
           "the passing A4, R-395)\n",
           has_claimed(claimed, count, 15, CADENCE_TYPE_PERFECT_AUTHENTIC) ? "claimed" : "not claimed",
           has_claimed(claimed, count, 31, CADENCE_TYPE_PERFECT_AUTHENTIC) ? "claimed" : "not claimed",
           has_claimed(claimed, count, 38, CADENCE_TYPE_PERFECT_AUTHENTIC) ? "claimed" : "not claimed",
           has_claimed(claimed, count, 8, CADENCE_TYPE_PERFECT_AUTHENTIC) ? "PAC" : "not PAC");

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

static void check_mozart(void) {
    WeightedPipeline p;
    AnalysisKeyEvent *key_events = NULL;
    AnalysisChain *chain = run_chain(&MOZART, &p, &key_events);

    printf("%s, phrases first\n", MOZART.name);
    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    assert(phrases != NULL);
    Claimed claimed[MAX_CADENCES];
    size_t count = claimed_of(chain, p.set.map, claimed);
    printf("  phrases:");
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhraseClaim *c = &phrases->items[i].claim;
        printf(" m%d-%d (%s)", c->first_measure, c->end_measure, c->cadence_type >= 0 ? type_name(c->cadence_type) : "no cadence");
    }
    printf("\n  cadences claimed:");
    for (size_t i = 0; i < count; i++) {
        printf(" %s m%d%s", type_name(claimed[i].type), claimed[i].measure, claimed[i].beat_one ? " b1" : "");
    }
    printf("\n");

    bool first = false;
    bool second = false;
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhraseClaim *c = &phrases->items[i].claim;
        first = first || (c->first_measure == 1 && c->end_measure == 12 && c->cadence_type == CADENCE_TYPE_HALF);
        second = second || (c->first_measure == 13 && c->end_measure == 28 && c->cadence_type == CADENCE_TYPE_PERFECT_AUTHENTIC);
        CHECK(!(c->first_measure <= 28 && c->length_measures == 1), "Mozart: a phrase of one measure at measure %d", c->first_measure);
    }
    CHECK(first, "Mozart: no phrase of measures 1-12 ending on a half cadence");
    CHECK(second, "Mozart: no phrase of measures 13-28 ending on a perfect authentic cadence");
    CHECK(has_claimed(claimed, count, 12, CADENCE_TYPE_HALF), "Mozart: no half cadence at measure 12 beat 1");
    CHECK(has_claimed(claimed, count, 28, CADENCE_TYPE_PERFECT_AUTHENTIC), "Mozart: no perfect authentic cadence at measure 28 beat 1");
    size_t arriving = 0;
    for (size_t i = 0; i < count; i++) {
        arriving += claimed[i].measure >= 26 && claimed[i].measure <= 28 ? 1 : 0;
    }
    CHECK(arriving == 1, "Mozart: %zu cadences arrive in measures 26 to 28, expected 1", arriving);

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

int main(void) {
    check_clementi();
    check_mozart();
    if (failures > 0) {
        printf("test_reference_phrases: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_reference_phrases: ok\n");
    return 0;
}
