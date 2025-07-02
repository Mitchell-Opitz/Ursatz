#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/variations.h>

/*
 * Variations and harmonizations (variations.h, R-088). Each mode is one ctest entry:
 *
 *   distinct       a rounded binary of 24 bars in C major 4/4 filled with seed 3: 4 variations of section B differ
 *                  pairwise, differ from the plan, leave section A as it was and the input plan untouched, and the same
 *                  seed gives the same variations again (quick)
 *   refused        a locked section gives REGENERATE_ERR_LOCKED, a count of 0 or 17, a target that is not there and
 *                  the Given meter span are refused, nothing is kept (quick)
 *   harmonize      a ternary form with an 8-bar melody given in B: 4 harmonizations keep every melody note and differ in
 *                  their harmony; a section with no given melody and a node that is no section are refused (quick)
 *   sweep_smoke    the sweep on seeds 1 and 2 (quick)
 *   sweep          seeds 1 to 20 of the variations of B, 4 asked for each: how many were kept (slow)
 *
 * The sweep asserts a baseline (R-075): it fails only when fewer variations are kept than the number recorded.
 */

#define MELODY                                                                                                         \
    "C5:h E5:h G5:q E5:q C5:h E5:h G5:h G5:q F5:q E5:h E5:h C5:h A4:q B4:q C5:h D5:h B4:h C5:w"

/* Of 20 seeds, 4 asked for each. */
#define SWEEP_BASELINE 80
#define SWEEP_SMOKE_BASELINE 8

static PieceRequest request(FormTemplateKind form) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form;
    request.bars = form == FORM_TEMPLATE_ROUNDED_BINARY ? 24 : 0;
    request.key.tonic_pitch_class = 0;
    request.key.mode = PLAN_MODE_MAJOR;
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    return request;
}

static ComposeConfig config_of(unsigned long seed) {
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    return config;
}

static FillTarget node_target(NodeId id) {
    FillTarget target = {FILL_TARGET_NODE, id};
    return target;
}

static char *json_of(const PiecePlan *plan) {
    char *text = plan_to_json(plan, NULL);
    assert(text);
    return text;
}

static NodeId section(const PiecePlan *plan, size_t index) {
    return plan_node_child_at(plan, plan_root(plan), index);
}

/* A filled rounded binary. */
static PiecePlan *filled(unsigned long seed) {
    PiecePlan *plan = NULL;
    PieceRequest asked = request(FORM_TEMPLATE_ROUNDED_BINARY);
    ComposeConfig config = config_of(seed);
    ComposeOutcome outcome;
    assert(piece_build_plan(&asked, &plan) == PIECE_OK);
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    compose_outcome_destroy(&outcome);
    return plan;
}

/* The lane spans of a lane over a section as text: where they are, their state and what they hold. */
static char *lane_text(const PiecePlan *plan, NodeId node, PlanLaneKind lane) {
    size_t capacity = 1 << 16;
    char *text = calloc(capacity, 1);
    size_t used = 0;
    PlanSpan span = plan_node_span(plan, node);
    for (int voice = 0; voice <= 3; voice++) {
        LaneSpanId ids[256];
        size_t found = plan_lane_find(plan, lane, voice, span, ids, 256);
        assert(found <= 256);
        for (size_t i = 0; i < found; i++) {
            PlanSpan at = plan_lane_span_span(plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
            used += (size_t)snprintf(text + used, capacity - used, "v%d %ld/%lu+%ld/%lu %d:", voice, at.start.num,
                                     at.start.den, at.length.num, at.length.den, (int)plan_lane_span_state(plan, ids[i]));
            if (lane == PLAN_LANE_HARMONY) {
                used += (size_t)snprintf(text + used, capacity - used, "%s;", payload->harmony.roman_numeral);
            }
            for (size_t n = 0; lane != PLAN_LANE_HARMONY && n < payload->notes.note_count; n++) {
                used += (size_t)snprintf(text + used, capacity - used, "%d@%ld/%lu,", payload->notes.notes[n].midi_pitch,
                                         payload->notes.notes[n].offset.num, payload->notes.notes[n].offset.den);
            }
            assert(used < capacity - 256);
        }
    }
    return text;
}

static char *section_text(const PiecePlan *plan, NodeId node) {
    PlanLaneKind lanes[4] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY, PLAN_LANE_INNER_VOICE};
    char *text = calloc(4 * (1 << 16) + 8, 1);
    for (int i = 0; i < 4; i++) {
        char *part = lane_text(plan, node, lanes[i]);
        strcat(text, part);
        strcat(text, "|");
        free(part);
    }
    return text;
}

/* --- distinct ------------------------------------------------------------------------ */

static void distinct(void) {
    PiecePlan *plan = filled(3);
    NodeId a = section(plan, 0);
    NodeId b = section(plan, 1);
    char *before = json_of(plan);
    char *a_before = section_text(plan, a);
    FillTarget target = node_target(b);
    ComposeConfig config = config_of(3);
    PlanVariations first;
    PlanVariations again;
    assert(plan_variations(plan, &target, 1, &config, 4, &first) == REGENERATE_OK);
    assert(first.count == 4);
    assert(first.seeds_tried >= 4 && first.seeds_tried <= 16);
    uint64_t original = plan_signature(plan);
    assert(original != 0);
    for (size_t i = 0; i < first.count; i++) {
        char *json = json_of(first.items[i].plan);
        char *a_text = section_text(first.items[i].plan, a);
        assert(first.items[i].signature != original);
        assert(first.items[i].signature == plan_signature(first.items[i].plan));
        assert(first.items[i].seed >= 3 && first.items[i].seed < 3 + first.seeds_tried);
        assert(strcmp(json, before) != 0);
        assert(strcmp(a_text, a_before) == 0); /* section A, lane by lane */
        for (size_t j = 0; j < i; j++) {
            char *other = json_of(first.items[j].plan);
            assert(first.items[i].signature != first.items[j].signature);
            assert(strcmp(json, other) != 0);
            free(other);
        }
        free(a_text);
        free(json);
    }
    /* The input plan is never changed. */
    char *after = json_of(plan);
    assert(strcmp(before, after) == 0);
    free(after);

    /* The same plan, target and seed give the same variations. */
    assert(plan_variations(plan, &target, 1, &config, 4, &again) == REGENERATE_OK);
    assert(again.count == first.count && again.seeds_tried == first.seeds_tried);
    for (size_t i = 0; i < first.count; i++) {
        char *x = json_of(first.items[i].plan);
        char *y = json_of(again.items[i].plan);
        assert(first.items[i].seed == again.items[i].seed);
        assert(strcmp(x, y) == 0);
        free(x);
        free(y);
    }
    plan_variations_destroy(&first);
    plan_variations_destroy(&again);
    assert(first.count == 0 && first.items == NULL);
    free(before);
    free(a_before);
    plan_destroy(plan);
}

/* --- refused ------------------------------------------------------------------------- */

static void refused(void) {
    PiecePlan *plan = filled(3);
    NodeId a = section(plan, 0);
    NodeId b = section(plan, 1);
    FillTarget target = node_target(b);
    ComposeConfig config = config_of(3);
    PlanVariations variations;
    char *before = json_of(plan);

    assert(plan_variations(plan, &target, 1, &config, 0, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(variations.count == 0);
    assert(plan_variations(plan, &target, 1, &config, 17, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_variations(plan, &target, 0, &config, 4, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_variations(NULL, &target, 1, &config, 4, &variations) == REGENERATE_ERR_NULL);
    FillTarget missing = {FILL_TARGET_LANE_SPAN, plan_lane_span_count(plan) + 5};
    assert(plan_variations(plan, &missing, 1, &config, 4, &variations) == REGENERATE_ERR_NOT_FOUND);
    FillTarget meter = {FILL_TARGET_LANE_SPAN, 1}; /* the meter span is Given */
    assert(plan_variations(plan, &meter, 1, &config, 4, &variations) == REGENERATE_ERR_LOCKED);
    assert(variations.count == 0 && variations.items == NULL);

    /* A locked section; another section can still be varied. */
    assert(plan_lock(plan, node_target(a)) == PLAN_OK);
    FillTarget locked = node_target(a);
    assert(plan_variations(plan, &locked, 1, &config, 4, &variations) == REGENERATE_ERR_LOCKED);
    assert(variations.count == 0 && variations.items == NULL);
    assert(plan_variations(plan, &target, 1, &config, 2, &variations) == REGENERATE_OK && variations.count == 2);
    plan_variations_destroy(&variations);

    /* Locking changed the input, nothing else did. */
    char *locked_json = json_of(plan);
    assert(strcmp(before, locked_json) != 0);
    free(locked_json);
    free(before);
    plan_destroy(plan);
}

/* --- harmonize ----------------------------------------------------------------------- */

static PiecePlan *with_given_melody(unsigned long seed) {
    PieceRequest asked = request(FORM_TEMPLATE_TERNARY);
    asked.given_melodies = 1;
    asked.seed = seed;
    PiecePlan *plan = NULL;
    MotiveOptions options;
    Motive melody;
    MotiveError error;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    assert(motive_from_notes_text(MELODY, &options, &melody, &error) == MOTIVE_OK);
    assert(piece_build_plan(&asked, &plan) == PIECE_OK);
    assert(plan_give_melody(plan, section(plan, 1), 0, melody.notes, melody.note_count) == GIVEN_OK);
    ComposeConfig config = config_of(seed);
    ComposeOutcome outcome;
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    compose_outcome_destroy(&outcome);
    return plan;
}

static void harmonize(void) {
    PiecePlan *plan = with_given_melody(1);
    NodeId a = section(plan, 0);
    NodeId b = section(plan, 1);
    ComposeConfig config = config_of(1);
    PlanVariations variations;
    char *melody_before = lane_text(plan, b, PLAN_LANE_MELODY);
    char *harmony_before = lane_text(plan, b, PLAN_LANE_HARMONY);
    char *before = json_of(plan);

    assert(plan_harmonizations(plan, b, &config, 4, &variations) == REGENERATE_OK);
    assert(variations.count == 4);
    for (size_t i = 0; i < variations.count; i++) {
        char *melody = lane_text(variations.items[i].plan, b, PLAN_LANE_MELODY);
        char *harmony = lane_text(variations.items[i].plan, b, PLAN_LANE_HARMONY);
        assert(strcmp(melody, melody_before) == 0); /* every melody note kept */
        assert(strcmp(harmony, harmony_before) != 0);
        for (size_t j = 0; j < i; j++) {
            char *other = lane_text(variations.items[j].plan, b, PLAN_LANE_HARMONY);
            assert(strcmp(harmony, other) != 0);
            free(other);
        }
        free(melody);
        free(harmony);
    }
    plan_variations_destroy(&variations);
    char *after = json_of(plan);
    assert(strcmp(before, after) == 0);
    free(after);

    /* A section whose melody is not given, the form, and a node that is not there. */
    assert(plan_harmonizations(plan, a, &config, 4, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(variations.count == 0 && variations.items == NULL);
    assert(plan_harmonizations(plan, plan_root(plan), &config, 4, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_harmonizations(plan, 99999, &config, 4, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_harmonizations(plan, b, &config, 0, &variations) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_harmonizations(NULL, b, &config, 4, &variations) == REGENERATE_ERR_NULL);
    free(melody_before);
    free(harmony_before);
    free(before);
    plan_destroy(plan);
}

/* --- the sweep ----------------------------------------------------------------------- */

static void sweep(unsigned long last, size_t baseline) {
    PiecePlan *plan = filled(3);
    FillTarget target = node_target(section(plan, 1));
    size_t kept = 0;
    for (unsigned long seed = 1; seed <= last; seed++) {
        ComposeConfig config = config_of(seed);
        PlanVariations variations;
        assert(plan_variations(plan, &target, 1, &config, 4, &variations) == REGENERATE_OK);
        kept += variations.count;
        plan_variations_destroy(&variations);
    }
    printf("kept %zu of %lu (baseline %zu)\n", kept, last * 4, baseline);
    assert(kept >= baseline);
    plan_destroy(plan);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "distinct") == 0) {
        distinct();
    } else if (strcmp(mode, "refused") == 0) {
        refused();
    } else if (strcmp(mode, "harmonize") == 0) {
        harmonize();
    } else if (strcmp(mode, "sweep_smoke") == 0) {
        sweep(2, SWEEP_SMOKE_BASELINE);
    } else if (strcmp(mode, "sweep") == 0) {
        sweep(20, SWEEP_BASELINE);
    } else {
        fprintf(stderr, "usage: test_variations distinct|refused|harmonize|sweep_smoke|sweep\n");
        return 2;
    }
    return 0;
}
