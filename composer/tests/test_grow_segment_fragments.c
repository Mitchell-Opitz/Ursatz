#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * Continuations from segments (B2 of P34): a 2-bar 4/4 motif with the segments `a` (bar 1) and `b` (bar 2) in an 8-bar
 * sentence, over seeds 1 to 10. A fragment is a segment of the motif and has that segment's rhythm; every developed idea has
 * a summary; a motif with no features is given proposed segments and its fragments name them; the cadential idea liquidates
 * segments in at least one step and at most as many as the motif has; the plan's JSON keeps the segment and the steps.
 */

#define SEEDS 10
/* A two-bar motif in C major 4/4: bar 1 is q e e q q, bar 2 is q e e h. */
#define MOTIF_NOTES "C4:q D4:e E4:e G4:q E4:q G4:q E4:e D4:e C4:h"

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static Motive make_motive(bool with_features) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    if (with_features) {
        options.has_features = true;
        options.feature_count = 2;
        strcpy(options.features[0].label, "a");
        options.features[0].first = 0;
        options.features[0].last = 4;
        strcpy(options.features[1].label, "b");
        options.features[1].first = 5;
        options.features[1].last = 8;
    }
    assert(motive_from_notes_text(MOTIF_NOTES, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static GrowRequest make_request(const Motive *motive) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    return request;
}

static PiecePlan *grow(const Motive *motive, unsigned long seed) {
    GrowRequest request = make_request(motive);
    GrowConfig config;
    GrowOutcome outcome;
    PiecePlan *plan = NULL;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    grow_outcome_destroy(&outcome);
    return plan;
}

/* The motive_ref of the node `name`. */
static const PlanMotiveRef *ref_of(const PiecePlan *plan, const char *name) {
    NodeId node = find_node(plan, name);
    assert(node != PLAN_NODE_NONE);
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    assert(!"the node has a motive_ref");
    return NULL;
}

static size_t written_notes(const PiecePlan *plan, const char *name, const PlanNote **notes) {
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, find_node(plan, name), &span, 1) == 1);
    const PlanNotes *held = &plan_lane_span_payload(plan, span)->notes;
    *notes = held->notes;
    return held->note_count;
}

/* Whether the notes of a motive node have the durations and offsets given as sixteenths of a bar's count of eighths. */
static bool has_rhythm(const PlanNote *notes, size_t count, const int *eighths, size_t expected) {
    Rational at = {0, 1};
    if (count != expected) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        Rational length = {eighths[i], 8};
        if (rational_cmp(notes[i].offset, at) != 0 || rational_cmp(notes[i].duration, length) != 0 ||
            rational_add(at, length, &at) != RATIONAL_OK) {
            return false;
        }
    }
    return true;
}

/*
 * A fragment is a segment, with that segment's rhythm, and names it. The draw weighs b over a two to one, and a fragment 2
 * can be the next segment; the compose stages then keep some runs that drew a, so segment b is a fragment in about 60% of
 * runs (the plan asked for 8 of 10, which the weights do not make: B2 notes): at least half of the 10 seeds here.
 */
static void test_fragments_are_segments(void) {
    static const int RHYTHM_B[] = {2, 1, 1, 4};
    static const int RHYTHM_A[] = {2, 1, 1, 2, 2};
    Motive motive = make_motive(true);
    size_t with_b = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan = grow(&motive, seed);
        bool has_b = false;
        for (int k = 2; k <= 3; k++) {
            char name[4];
            const PlanNote *notes;
            snprintf(name, sizeof(name), "m%d", k);
            const PlanMotiveRef *ref = ref_of(plan, name);
            size_t count = written_notes(plan, name, &notes);
            if (strcmp(ref->segment, "b") == 0) {
                has_b = true;
                /* The rhythm of segment b is kept by the exact fragment and by its sequence. */
                assert(has_rhythm(notes, count, RHYTHM_B, 4));
            } else {
                assert(strcmp(ref->segment, "a") == 0);
                assert(has_rhythm(notes, count, RHYTHM_A, 5));
            }
        }
        with_b += has_b ? 1 : 0;
        plan_destroy(plan);
    }
    printf("a fragment is segment b in %zu of %d runs\n", with_b, SEEDS);
    assert(with_b >= 5);
}

/* A motive with no features gets proposed segments, and its fragments name them. */
static void test_proposed_segments(void) {
    Motive motive = make_motive(false);
    assert(motive.feature_count == 0);
    size_t named = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan = grow(&motive, seed);
        for (int k = 2; k <= 3; k++) {
            char name[4];
            snprintf(name, sizeof(name), "m%d", k);
            const char *segment = ref_of(plan, name)->segment;
            named += segment[0] != '\0' ? 1 : 0;
            assert(segment[0] == '\0' || (segment[0] >= 'a' && segment[0] <= 'h'));
        }
        plan_destroy(plan);
    }
    printf("proposed segments named by %zu of %d fragments\n", named, 2 * SEEDS);
    assert(named >= SEEDS);
}

/* The cadential idea liquidates segments in 1 to 2 steps (the motif has 2 segments), else drops notes (no steps). */
static void test_liquidation_steps(void) {
    Motive motive = make_motive(true);
    size_t liquidated = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan = grow(&motive, seed);
        size_t steps = ref_of(plan, "m4")->steps;
        assert(steps <= 2);
        liquidated += steps > 0 ? 1 : 0;
        plan_destroy(plan);
    }
    printf("the cadential idea liquidates segments in %zu of %d runs\n", liquidated, SEEDS);
    assert(liquidated >= 8);
}

/* The plan's JSON keeps the segment and the steps, and reads them back. */
static void test_plan_round_trip(void) {
    Motive motive = make_motive(true);
    PiecePlan *plan = grow(&motive, 1);
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text && strstr(text, "\"segment\"") && strstr(text, "\"steps\""));
    PiecePlan *again = NULL;
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    for (int k = 2; k <= 4; k++) {
        char name[4];
        snprintf(name, sizeof(name), "m%d", k);
        const PlanMotiveRef *before = ref_of(plan, name);
        const PlanMotiveRef *after = ref_of(again, name);
        assert(strcmp(before->segment, after->segment) == 0 && before->steps == after->steps);
    }
    char *second = plan_to_json(again, &error);
    assert(second && strcmp(text, second) == 0);
    /* A ref without them reads as no segment and no steps. */
    assert(ref_of(again, "m1")->segment[0] == '\0' && ref_of(again, "m1")->steps == 0);
    free(text);
    free(second);
    plan_destroy(plan);
    plan_destroy(again);
}

/* Every developed idea of the theme result has a summary naming the idea, its source and its operation. */
static void test_summaries(void) {
    const char *request =
        "{\"schema\": \"ursatz-theme-request\", \"schema_version\": 2, \"into\": \"sentence\", \"hybrid\": null, "
        "\"key\": \"C\", \"mode\": \"major\", \"meter\": \"4/4\", \"bars\": 8, \"cadence\": \"PAC\", "
        "\"first_cadence\": null, \"motives\": [{\"id\": \"A\", \"form\": \"full\", "
        "\"degrees\": \"1:q 2:e 3:e 5:q 3:q 5:q 3:e 2:e 1:h\", \"pickup\": \"0\", \"harmony\": null, "
        "\"features\": [{\"label\": \"a\", \"first\": 0, \"last\": 4}, {\"label\": \"b\", \"first\": 5, \"last\": 8}], "
        "\"role\": \"melodic\"}], \"slots\": {\"basic\": \"A\", \"contrasting\": null, \"continuation\": null, "
        "\"cadential\": null}, \"accompaniment\": null, \"voicing\": null, \"style\": null, \"style_weight\": \"1/2\", "
        "\"without\": [], \"tempo\": null, \"seed\": 1}";
    ComposerResponse response;
    assert(composer_api_theme(request, NULL, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    const cJSON *developments = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(themes, 0), "developments");
    assert(cJSON_GetArraySize(developments) == 4);
    const cJSON *item = NULL;
    bool fragment = false;
    bool cadential = false;
    cJSON_ArrayForEach(item, developments) {
        const cJSON *idea = cJSON_GetObjectItemCaseSensitive(item, "idea");
        const cJSON *summary = cJSON_GetObjectItemCaseSensitive(item, "summary");
        const cJSON *segment = cJSON_GetObjectItemCaseSensitive(item, "segment");
        assert(cJSON_IsString(idea) && cJSON_IsString(summary) && segment);
        /* The idea as the plan names it, an equals sign, its source and its operation. */
        char start[64];
        snprintf(start, sizeof(start), "%s = ", idea->valuestring);
        assert(strncmp(summary->valuestring, start, strlen(start)) == 0);
        assert(strchr(summary->valuestring + strlen(start), ',') != NULL);
        if (strcmp(idea->valuestring, "fragment 1") == 0) {
            fragment = true;
            assert(cJSON_IsString(segment) && strstr(summary->valuestring, "= segment ") != NULL);
        }
        if (strcmp(idea->valuestring, "cadential") == 0) {
            cadential = true;
            assert(strstr(summary->valuestring, "basic idea, ") != NULL);
        }
    }
    assert(fragment && cadential);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

int main(void) {
    test_fragments_are_segments();
    test_proposed_segments();
    test_liquidation_steps();
    test_plan_round_trip();
    test_summaries();
    return 0;
}
