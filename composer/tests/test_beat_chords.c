#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <harmony/harmony_internal.h>
#include <plan/plan_lanes.h>
#include <plan/plan_state.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/piece.h>
#include "support/test_composer.h"
#include "support/test_support.h"
#include "support/test_knowledge.h"
#include "ursatz_test.h"

/*
 * Given chords on beats and half beats, up to four a bar (R-761, A20261115-84), run as processes of the `theme` command:
 *
 *   measured   the six requests P70 measured (a sentence of eight bars, one bar of given chords: three in 4/4 on beats and on half
 *              beats, two in 3/4, one in 6/8) each compose on seeds 1 to 3 on their own seed, with every given chord in the plan at
 *              its onset
 *   grid       a span without a Given chord off the accent cells keeps them; one with a Given chord on a beat is read on beats,
 *              one on a half beat on the divisions (4/4 and 3/4), and in 6/8 the eighths
 */

typedef struct {
    const char *meter;
    const char *degrees;
    int bar;
    const char *text;
    size_t count;
    const char *numerals[4];
    long starts[4]; /* in eighths of a whole note from the start of the theme */
} Case;

static const char *const MOTIF_44 = "3:q 2:e 1:e 2:q 3:q | 5:h 4:q 3:q";
static const char *const MOTIF_34 = "1:q 2:e 3:e 4:q | 5:h.";
static const char *const MOTIF_68 = "5:q 4:e 3:q 2:e | 1:q. 3:q.";

static const Case CASES[] = {
    {"4/4", MOTIF_44, 4, "IV I@1 II@2 V@3", 4, {"IV", "I", "ii", "V"}, {32, 34, 36, 38}},
    {"4/4", MOTIF_44, 5, "VI II@1 V@2 I@3", 4, {"vi", "ii", "V", "I"}, {40, 42, 44, 46}},
    {"4/4", MOTIF_44, 5, "VI II@3/2 V@2 I@3", 4, {"vi", "ii", "V", "I"}, {40, 43, 44, 46}},
    {"3/4", MOTIF_34, 4, "IV V@1 I@2", 3, {"IV", "V", "I"}, {24, 26, 28}},
    {"3/4", MOTIF_34, 4, "IV V@3/2 I@2", 3, {"IV", "V", "I"}, {24, 27, 28}},
    {"6/8", MOTIF_68, 4, "IV V@2 I@4", 3, {"IV", "V", "I"}, {24, 26, 28}},
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))
#define SEEDS 3

static void write_request(const Case *test, const char *path) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fprintf(file,
            "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"into\":\"sentence\",\"key\":\"C\",\"material\":\"major\","
            "\"meter\":\"%s\",\"bars\":8,\"cadence\":\"PAC\",\"development\":\"balanced\",\"motives\":[{\"id\":\"A\",\"form\":\"full\","
            "\"degrees\":\"%s\",\"pickup\":\"0\",\"harmony\":null,\"features\":[],\"role\":\"melodic\"}],\"slots\":{\"basic\":\"A\"},"
            "\"chords\":[{\"bar\":%d,\"text\":\"%s\"}],\"accompaniment\":{\"pattern\":\"block\"}}\n",
            test->meter, test->degrees, test->bar, test->text);
    fclose(file);
}

/* The Given harmony spans of the theme in the plan file: numerals and starts in eighths of a whole note. */
static size_t given_harmony(const char *plan_path, char numerals[][32], long *eighths, size_t capacity) {
    Plan *plan = NULL;
    PlanJsonError error;
    assert(test_read_plan_file(plan_path, &plan, &error) == PLAN_OK);
    NodeId theme = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (name && strcmp(name, "theme") == 0) {
            theme = id;
        }
    }
    assert(theme != PLAN_NODE_NONE);
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, theme), spans, 64);
    assert(found > 0 && found <= 64);
    size_t given = 0;
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_state(plan, spans[i]) != PLAN_STATE_GIVEN) {
            continue;
        }
        assert(given < capacity);
        snprintf(numerals[given], 32, "%s", plan_lane_span_payload(plan, spans[i])->harmony.roman_numeral);
        Rational start = plan_lane_span_span(plan, spans[i]).start;
        assert((start.num * 8) % (long)start.den == 0);
        eighths[given++] = start.num * 8 / (long)start.den;
    }
    plan_destroy(plan);
    return given;
}

static void test_measured(void) {
    /* Every request and seed runs at once: the processes are independent and the test stays short. */
    char line[16384];
    size_t used = 0;
    for (size_t c = 0; c < CASE_COUNT; c++) {
        char path[64];
        snprintf(path, sizeof(path), "case%zu.json", c);
        write_request(&CASES[c], path);
        for (int seed = 1; seed <= SEEDS; seed++) {
            used += (size_t)snprintf(line + used, sizeof(line) - used,
                                     "(\"%s\" theme case%zu.json --seed %d --format json --out case%zu_%d_plan.json >case%zu_%d.out "
                                     "2>case%zu_%d.err; echo $? >case%zu_%d.rc) & ",
                                     COMPOSER_CLI_PATH, c, seed, c, seed, c, seed, c, seed, c, seed);
        }
    }
    snprintf(line + used, sizeof(line) - used, "wait");
    assert(system(line) == 0);
    for (size_t c = 0; c < CASE_COUNT; c++) {
        for (int seed = 1; seed <= SEEDS; seed++) {
            char name[64];
            snprintf(name, sizeof(name), "case%zu_%d.rc", c, seed);
            char *status = test_must_read_file(name, NULL);
            if (atoi(status) != 0) {
                snprintf(name, sizeof(name), "case%zu_%d.err", c, seed);
                char *err = test_must_read_file(name, NULL);
                fprintf(stderr, "%s '%s' bar %d seed %d: %s\n", CASES[c].meter, CASES[c].text, CASES[c].bar, seed, err);
                free(err);
            }
            assert(atoi(status) == 0);
            free(status);
            /* on its own seed: the first theme of the result is the seed asked for */
            snprintf(name, sizeof(name), "case%zu_%d.out", c, seed);
            char *out = test_must_read_file(name, NULL);
            char wanted[48];
            snprintf(wanted, sizeof(wanted), "\"themes\":[{\"seed\":%d,", seed);
            assert(strstr(out, wanted) != NULL);
            free(out);
            char numerals[16][32];
            long eighths[16];
            snprintf(name, sizeof(name), "case%zu_%d_plan.json", c, seed);
            size_t count = given_harmony(name, numerals, eighths, 16);
            assert(count == CASES[c].count);
            for (size_t i = 0; i < count; i++) {
                assert(strcmp(numerals[i], CASES[c].numerals[i]) == 0 && eighths[i] == CASES[c].starts[i]);
            }
        }
    }
}

static Rational time_of(long numerator, unsigned long denominator) {
    Rational time;
    assert(rational_make(numerator, denominator, &time) == RATIONAL_OK);
    return time;
}

/* A plan of the meter with section A's node. */
static Plan *new_plan(int beats, int unit, NodeId *section) {
    PieceRequest request;
    Plan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.form = "rounded-binary";
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = beats;
    request.meter.beat_unit = unit;
    request.seed = 1;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    NodeId root = plan_root(plan);
    *section = PLAN_NODE_NONE;
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        const SectionDescription *description = plan_node_section(plan, node);
        if (description && strcmp(description->letter, "A") == 0) {
            *section = node;
        }
    }
    assert(*section != PLAN_NODE_NONE);
    return plan;
}

static size_t cells_after(int beats, int unit, const GivenChord *chords, size_t count) {
    NodeId section;
    Plan *plan = new_plan(beats, unit, &section);
    if (count > 0) {
        assert(plan_give_chords(plan, section, chords, count) == GIVEN_OK);
    }
    PhraseMetre metre;
    assert(harmony_read_metre(plan, plan_node_span(plan, section), &metre));
    size_t cells = metre.cells_per_bar;
    assert(rational_cmp(metre.cell_length, time_of((long)beats, (unsigned long)unit * (unsigned long)cells)) == 0);
    plan_destroy(plan);
    return cells;
}

static void test_grid(void) {
    /* no Given chord: the accent cells (half bars in 4/4, a bar in 3/4, two cells in 6/8) */
    size_t accent_44 = cells_after(4, 4, NULL, 0);
    size_t accent_34 = cells_after(3, 4, NULL, 0);
    size_t accent_68 = cells_after(6, 8, NULL, 0);
    assert(accent_44 == 2 && accent_34 == 1 && accent_68 == 2);
    /* a chord on the half bar and one on a bar line stay on the accent cells */
    const GivenChord on_cells_44[2] = {{"I", time_of(0, 1), time_of(1, 2)}, {"V", time_of(1, 2), time_of(1, 2)}};
    assert(cells_after(4, 4, on_cells_44, 2) == accent_44);
    /* a chord on beat 2 (4/4): beats; on the "and" of beat 1: the divisions */
    const GivenChord on_beat_44[2] = {{"I", time_of(0, 1), time_of(1, 4)}, {"V", time_of(1, 4), time_of(3, 4)}};
    assert(cells_after(4, 4, on_beat_44, 2) == 4);
    const GivenChord on_half_44[2] = {{"I", time_of(0, 1), time_of(3, 8)}, {"V", time_of(3, 8), time_of(5, 8)}};
    assert(cells_after(4, 4, on_half_44, 2) == 8);
    /* 3/4: beats are three cells, half beats six */
    const GivenChord on_beat_34[2] = {{"I", time_of(0, 1), time_of(1, 4)}, {"V", time_of(1, 4), time_of(1, 2)}};
    assert(cells_after(3, 4, on_beat_34, 2) == 3);
    const GivenChord on_half_34[2] = {{"I", time_of(0, 1), time_of(3, 8)}, {"V", time_of(3, 8), time_of(3, 8)}};
    assert(cells_after(3, 4, on_half_34, 2) == 6);
    /* 6/8: a chord on an eighth reads six cells */
    const GivenChord on_eighth_68[2] = {{"I", time_of(0, 1), time_of(1, 4)}, {"V", time_of(1, 4), time_of(1, 2)}};
    assert(cells_after(6, 8, on_eighth_68, 2) == 6);
}

int main(void) {
    test_measured();
    test_grid();
    puts("test_beat_chords: ok");
    return 0;
}
