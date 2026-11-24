#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/piece_plan_keys.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_modal_theme.h"
#include "support/test_support.h"
#include "ursatz_test.h"
#include "util/util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The modal sweep (R-725, R-732, gate 2 of P69): twenty themes from seed 500, a sentence and a period in each of the five church modes on C,
 * made as `listen --what themes --count 20 --seed 500` makes them, held to what the Composer writes in a mode: every chord of the harmony
 * lane in the mode's collection, at most 0.020 of the melody notes outside it (chromatic embellishment), a chord with a characteristic step,
 * the chord before the final one holding one (a modal cadence), and a characteristic step on a strong beat in at least three quarters of the
 * themes. `composed` and `strong` are pinned from the run of P69 B11 (modal_props.py over batches.sh, same numbers up to its reading of the
 * MusicXML); a smoke (seed 500 alone) runs in the fast set.
 */

#define MAX_CHORDS 64
#define THEMES 20
#define FIRST_SEED 500

typedef struct {
    const char *into;
    const char *word;
    size_t composed; /* of THEMES, pinned */
    size_t strong;   /* of those composed, a characteristic step on a strong beat, pinned */
} Batch;

static const Batch BATCHES[] = {
    {"sentence", "dorian", 16, 13},  {"sentence", "mixolydian", 15, 14}, {"sentence", "lydian", 17, 16}, {"sentence", "phrygian", 15, 13},
    {"sentence", "locrian", 17, 17}, {"period", "dorian", 14, 12},       {"period", "mixolydian", 13, 12}, {"period", "lydian", 17, 16},
    {"period", "phrygian", 16, 16},  {"period", "locrian", 19, 19},
};
#define BATCH_COUNT (sizeof(BATCHES) / sizeof(BATCHES[0]))

typedef struct {
    size_t composed;
    size_t chords;
    size_t out_harm;
    size_t notes;
    size_t out_mel;
    size_t strong;
    size_t col_harm;
    size_t cad_color;
} Measures;

static unsigned chord_mask(const Plan *plan, LaneSpanId id, const SpelledKey *key, const HarmonyKind *kind) {
    HarmonyChord chord;
    int root;
    int intervals[COMPOSER_CHORD_TONES_MAX];
    size_t count;
    assert(harmony_roman_parse_in(kind, plan_lane_span_payload(plan, id)->harmony.roman_numeral, &chord));
    assert(composer_chord_tones(test_composer_chords(), key, &chord, &root, intervals, &count));
    unsigned mask = 0;
    for (size_t k = 0; k < count; k++) {
        mask |= 1u << util_pitch_class(root + intervals[k]);
    }
    return mask;
}

static unsigned collection_mask(const PitchMaterial *material) {
    unsigned mask = 0;
    for (int pc = 0; pc < 12; pc++) {
        mask |= pitch_material_contains(material, pc) ? 1u << pc : 0;
    }
    return mask;
}

/* The measures of one theme, added to the batch's. */
static void measure(const Plan *plan, const char *word, Measures *out) {
    SpelledKey key = test_key(0, word);
    HarmonyKind kind = harmony_kind(test_composer_chords(), &key);
    unsigned collection = collection_mask(test_material(word));
    unsigned colors = composer_key_color_mask(&key);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, MAX_CHORDS);
    assert(count >= 2);
    unsigned masks[MAX_CHORDS];
    bool color_in_chord = false;
    for (size_t i = 0; i < count; i++) {
        masks[i] = chord_mask(plan, ids[i], &key, &kind);
        out->chords++;
        out->out_harm += (masks[i] & ~collection) != 0;
        color_in_chord = color_in_chord || (masks[i] & colors) != 0;
    }
    out->col_harm += color_in_chord;
    /* The chord before the final one that is another chord than it. */
    size_t before = count - 1;
    while (before > 0 && masks[before - 1] == masks[count - 1]) {
        before--;
    }
    out->cad_color += before > 0 && (masks[before - 1] & colors) != 0;
    LaneSpanId melody[256];
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, plan_root(plan)), melody, 256);
    bool strong = false;
    for (size_t i = 0; i < spans; i++) {
        PlanSpan span = plan_lane_span_span(plan, melody[i]);
        const PlanNotes notes = plan_lane_span_payload(plan, melody[i])->notes;
        for (size_t n = 0; n < notes.note_count; n++) {
            Rational onset;
            assert(rational_add(span.start, notes.notes[n].offset, &onset) == RATIONAL_OK);
            unsigned pc = (unsigned)util_pitch_class(notes.notes[n].midi);
            out->notes++;
            out->out_mel += ((collection >> pc) & 1u) == 0;
            strong = strong || ((onset.num * 2) % (long)onset.den == 0 && ((colors >> pc) & 1u) != 0);
        }
    }
    out->strong += strong;
}

static Measures run(const Batch *batch, unsigned themes) {
    Measures total;
    memset(&total, 0, sizeof(total));
    for (unsigned seed = FIRST_SEED; seed < FIRST_SEED + themes; seed++) {
        Plan *plan = NULL;
        if (!compose_theme_in(batch->word, batch->into, seed, NULL, &plan)) {
            continue;
        }
        total.composed++;
        measure(plan, batch->word, &total);
        plan_destroy(plan);
    }
    return total;
}

/*
 * test_modal_sweep <mode>: the 40 themes of one church mode (a sentence batch and a period batch), a slow test for each mode (a theme takes
 * seconds, so the sweep is split to stay within one test's time). test_modal_sweep smoke: seed 500 alone in the sentence in lydian and the
 * period in phrygian, in the fast set.
 */
int main(int argc, char **argv) {
    const char *argument = argc > 1 ? argv[1] : "";
    bool smoke = strcmp(argument, "smoke") == 0;
    size_t ran = 0;
    for (size_t b = 0; b < BATCH_COUNT; b++) {
        const Batch *batch = &BATCHES[b];
        bool chosen = smoke ? (strcmp(batch->word, "lydian") == 0 && strcmp(batch->into, "sentence") == 0) ||
                                  (strcmp(batch->word, "phrygian") == 0 && strcmp(batch->into, "period") == 0)
                            : strcmp(batch->word, argument) == 0;
        if (!chosen) {
            continue;
        }
        ran++;
        Measures m = run(batch, smoke ? 1 : THEMES);
        fprintf(stderr, "%s-%s composed %zu out_harm %zu/%zu out_mel %zu/%zu col_harm %zu cad_color %zu strong %zu\n", batch->into, batch->word, m.composed,
                m.out_harm, m.chords, m.out_mel, m.notes, m.col_harm, m.cad_color, m.strong);
        assert(m.out_harm == 0);                                    /* every chord in the collection */
        assert(m.out_mel * 1000 <= m.notes * (smoke ? 30 : 20));    /* at most 0.020 of the notes outside it (0.030 of one theme: the smoke) */
        assert(m.col_harm == m.composed);                           /* a chord holds a characteristic step in every theme */
        assert(m.cad_color == m.composed);                          /* and the chord before the final one does */
        if (smoke) {
            assert(m.composed == 1);
        } else {
            assert(m.composed >= 10 && m.strong * 4 >= m.composed * 3);
            assert(m.composed == batch->composed && m.strong == batch->strong);
        }
    }
    assert(ran > 0);
    printf("test_modal_sweep %s passed\n", argument);
    return 0;
}
