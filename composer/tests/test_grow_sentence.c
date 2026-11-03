#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/hybrid_rules.h>
#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <semantics/subphrase_role.h>
#include <standard-form/sentence_rules.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/variant_registry.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

/*
 * Growing a sentence from a motive in four keys and meters, 8 and 4 bars, seeds 1 to 10. Each run must complete;
 * the plan it leaves is read back here and held to standard-form's rule functions, worked out from the nodes and the
 * lanes and not from the operator; the same seed gives the same plan and the same records.
 */

typedef struct {
    const char *name;
    int tonic;
    PlanKeyMode mode;
    int beats;
    int unit;
    const char *motive;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, PLAN_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"D minor 3/4", 2, PLAN_MODE_MINOR, 3, 4, "F4:q A4:e G4:e E4:q"},
    {"G major 6/8", 7, PLAN_MODE_MAJOR, 6, 8, "B4:q A4:e G4:e F#4:e D4:e"},
    {"F major 2/4", 5, PLAN_MODE_MAJOR, 2, 4, "A4:q C5:e Bb4:e"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

#define TICKS 128L

static Motive make_motive(const Scenario *scenario) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(scenario->motive, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static GrowRequest make_request(const Scenario *scenario, const Motive *motive, size_t bars) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key.tonic_pitch_class = scenario->tonic;
    request.key.mode = scenario->mode;
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = scenario->unit;
    request.bars = bars;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = PLAN_CADENCE_PAC;
    return request;
}

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static long ticks(Rational time) {
    assert((time.num * TICKS) % (long)time.den == 0);
    return time.num * TICKS / (long)time.den;
}

/* The lane spans of a lane over the theme, in time order, tiling it and all written. */
static size_t tiling(const PiecePlan *plan, PlanLaneKind lane, PlanSpan theme, LaneSpanId *out, size_t capacity) {
    size_t count = plan_lane_find(plan, lane, 0, theme, out, capacity);
    assert(count > 0 && count <= capacity);
    long at = ticks(theme.start);
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(plan, out[i]);
        assert(ticks(span.start) == at);
        assert(plan_lane_span_state(plan, out[i]) != PLAN_STATE_CONSTRAINED);
        at += ticks(span.length);
    }
    assert(at == ticks(theme.start) + ticks(theme.length));
    return count;
}

static void check_structure(const PiecePlan *plan, const Scenario *scenario, size_t bars) {
    Rational bar;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    long bar_ticks = ticks(bar);
    NodeId theme = find_node(plan, "theme");
    assert(theme != PLAN_NODE_NONE && plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    assert(ticks(plan_node_span(plan, theme).length) == (long)bars * bar_ticks);

    /* The nodes, in units of a bar over eight bars: an eighth of the theme each. */
    static const char *const NAMES[] = {"phrase", "presentation", "basic idea", "repetition",
                                        "continuation", "fragment 1", "fragment 2", "cadential"};
    static const long STARTS[] = {0, 0, 0, 2, 4, 4, 5, 6};
    static const long LENGTHS[] = {8, 4, 2, 2, 4, 1, 1, 2};
    for (size_t i = 0; i < 8; i++) {
        NodeId node = find_node(plan, NAMES[i]);
        assert(node != PLAN_NODE_NONE && plan_node_state(plan, node) == PLAN_STATE_REALIZED);
        PlanSpan span = plan_node_span(plan, node);
        assert(ticks(span.start) * 8 == STARTS[i] * (long)bars * bar_ticks);
        assert(ticks(span.length) * 8 == LENGTHS[i] * (long)bars * bar_ticks);
    }
    assert(plan_node_kind(plan, find_node(plan, "phrase")) == PLAN_NODE_PHRASE);
    assert(plan_node_kind(plan, find_node(plan, "presentation")) == PLAN_NODE_SUBPHRASE);
    assert(plan_node_kind(plan, find_node(plan, "continuation")) == PLAN_NODE_SUBPHRASE);
    assert(plan_node_kind(plan, find_node(plan, "cadential")) == PLAN_NODE_IDEA);

    /* standard-form's hard rules, asked of what is in the plan. */
    PlanThemePairing pairing = {PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT};
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            pairing = constraint->params.theme_pairing;
        }
    }
    assert(pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION && pairing.ending == PLAN_THEME_ENDING_CONTINUATION);
    PhraseLevelForm form;
    assert(standard_form_pairing_form(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CONTINUATION, &form));
    assert(form == PHRASE_LEVEL_FORM_SENTENCE);
    assert(standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_PRESENTATION, SUBPHRASE_ROLE_CONTINUATION));
    assert(standard_form_phrase_is_sentential(true, true));
    long basic = ticks(plan_node_span(plan, find_node(plan, "basic idea")).length);
    assert(standard_form_basic_idea_length_valid((size_t)(basic / bar_ticks)));
    for (const char *const *fragment = (const char *const[]){"fragment 1", "fragment 2", NULL}; *fragment; fragment++) {
        long length = ticks(plan_node_span(plan, find_node(plan, *fragment)).length);
        assert(standard_form_is_fragmentation((size_t)basic, (size_t)length));
    }
    /* The fragments of the notes too: each is shorter than the motive it was made from. */
    NodeId m0 = find_node(plan, "m0");
    LaneSpanId m0_span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, m0, &m0_span, 1) == 1);
    long m0_length = ticks(plan_lane_span_span(plan, m0_span).length);
    for (const char *const *name = (const char *const[]){"m2", "m3", NULL}; *name; name++) {
        LaneSpanId made = PLAN_LANE_SPAN_NONE;
        assert(plan_node_lane_spans(plan, find_node(plan, *name), &made, 1) == 1);
        assert(plan_lane_span_state(plan, made) == PLAN_STATE_REALIZED);
        assert(standard_form_is_fragmentation((size_t)m0_length, (size_t)ticks(plan_lane_span_span(plan, made).length)));
    }
}

static void check_lanes(const PiecePlan *plan, const Scenario *scenario, size_t bars) {
    LaneSpanId ids[64];
    NodeId theme_node = find_node(plan, "theme");
    PlanSpan theme = plan_node_span(plan, theme_node);
    Rational bar;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    long bar_ticks = ticks(bar);
    size_t cells_per_bar = scenario->unit == 8 || (scenario->beats == 4 && scenario->unit == 4) ? 2 : 1;
    long cell_ticks = bar_ticks / (long)cells_per_bar;

    /* Harmony: chords on cells, tiling the theme; the presentation starts on the tonic, the continuation does not
       cross a barline and its chords are no longer than the presentation's would allow. */
    size_t chords = tiling(plan, PLAN_LANE_HARMONY, theme, ids, 64);
    const char *first = plan_lane_span_payload(plan, ids[0])->harmony.roman_numeral;
    assert(strcmp(first, "I") == 0 || strcmp(first, "i") == 0);
    long half = (long)bars * bar_ticks / 2;
    for (size_t i = 0; i < chords; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        long start = ticks(span.start);
        long length = ticks(span.length);
        assert(start % cell_ticks == 0 && length % cell_ticks == 0);
        if (start >= half) {
            assert(start % bar_ticks + length <= bar_ticks);
        }
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED);
    }
    /* The cadence is on the last chord. */
    const PlanConstraint *cadence = plan_lane_span_constraint_count(plan, ids[chords - 1]) > 0
                                        ? plan_lane_span_constraint_at(plan, ids[chords - 1], 0)
                                        : NULL;
    assert(cadence && cadence->kind == PLAN_CONSTRAINT_CADENCE && cadence->params.cadence == PLAN_CADENCE_PAC);

    /* Bass: a span for each chord, written. */
    size_t basses = tiling(plan, PLAN_LANE_BASS, theme, ids, 64);
    assert(basses == chords);

    /* Melody: one span for each idea, tiling the theme, each written. */
    size_t melodies = tiling(plan, PLAN_LANE_MELODY, theme, ids, 64);
    assert(melodies == 5);
    for (size_t i = 0; i < melodies; i++) {
        assert(plan_lane_span_owner(plan, ids[i]) == find_node(plan,
               (const char *const[]){"basic idea", "repetition", "fragment 1", "fragment 2", "cadential"}[i]));
    }
    /* The cadence arrives on the tonic. */
    const PlanNotes *last = &plan_lane_span_payload(plan, ids[melodies - 1])->notes;
    int tonic_pc = (last->notes[last->note_count - 1].midi_pitch % 12 + 12) % 12;
    assert(tonic_pc == scenario->tonic);

    /* One highest note in the theme's melody. */
    int top = 0;
    size_t at_top = 0;
    for (size_t i = 0; i < melodies; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(notes->notes[n].midi_pitch >= 60 && notes->notes[n].midi_pitch <= 79);
            if (notes->notes[n].midi_pitch > top) {
                top = notes->notes[n].midi_pitch;
                at_top = 1;
            } else if (notes->notes[n].midi_pitch == top) {
                at_top++;
            }
        }
    }
    assert(at_top == 1);
}

/* The material is not part of the piece: it is skipped, and the piece ends with the theme. */
static void check_render(const PiecePlan *plan, size_t bars, const Scenario *scenario) {
    RenderedPlan rendered;
    Rational bar;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(rendered.count > 0);
    assert(ticks(rendered.piece_end) == (long)bars * ticks(bar));
    size_t material = 0;
    for (size_t i = 0; i < rendered.skipped_count; i++) {
        material += rendered.skipped[i].reason == RENDER_SKIP_MATERIAL;
        assert(rendered.skipped[i].reason != RENDER_SKIP_NOT_REALIZED);
    }
    assert(material >= 5);
    assert(rendered.part_count >= 2);
    rendered_plan_destroy(&rendered);
}

static char *records_text(const GrowOutcome *outcome) {
    size_t size = 1;
    char *all = calloc(1, 1);
    for (int stage = 0; stage < GROW_STAGE_COUNT; stage++) {
        char *json = fill_record_to_json(outcome->records[stage]);
        assert(json);
        size += strlen(json);
        all = realloc(all, size);
        strcat(all, json);
        free(json);
    }
    return all;
}

static void run_scenario(const Scenario *scenario, size_t bars, unsigned long seed) {
    Motive motive = make_motive(scenario);
    GrowRequest request = make_request(scenario, &motive, bars);
    GrowConfig config;
    char *plans[2];
    char *records[2];
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    for (int round = 0; round < 2; round++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        GrowResult result = grow_run(plan, &config, &outcome);
        if (result != GROW_OK) {
            fprintf(stderr, "%s, %zu bars, seed %lu: %s at %s\n", scenario->name, bars, seed, grow_result_name(result),
                    grow_stage_name(outcome.stuck_stage));
        }
        assert(result == GROW_OK && !outcome.stuck);
        if (round == 0) {
            check_structure(plan, scenario, bars);
            check_lanes(plan, scenario, bars);
            check_render(plan, bars, scenario);
            assert(outcome.score.denominator >= 1 && outcome.score.numerator <= outcome.score.denominator);
        }
        plans[round] = plan_to_json(plan, NULL);
        records[round] = records_text(&outcome);
        assert(plans[round]);
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(strcmp(plans[0], plans[1]) == 0);
    assert(strcmp(records[0], records[1]) == 0);
    for (int round = 0; round < 2; round++) {
        free(plans[round]);
        free(records[round]);
    }
}

static void test_sentences(void) {
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (size_t bars = 8; bars >= 4; bars -= 4) {
            for (unsigned long seed = 1; seed <= 10; seed++) {
                run_scenario(&SCENARIOS[s], bars, seed);
            }
        }
    }
}

/* Different seeds give different sentences. */
static void test_seeds_vary(void) {
    char *seen[10];
    size_t distinct = 0;
    Motive motive = make_motive(&SCENARIOS[0]);
    GrowRequest request = make_request(&SCENARIOS[0], &motive, 8);
    for (unsigned long seed = 1; seed <= 10; seed++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        assert(grow_run(plan, &config, &outcome) == GROW_OK);
        char *json = plan_to_json(plan, NULL);
        bool known = false;
        for (size_t i = 0; i < distinct; i++) {
            known = known || strcmp(seen[i], json) == 0;
        }
        if (known) {
            free(json);
        } else {
            seen[distinct++] = json;
        }
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(distinct > 5);
    for (size_t i = 0; i < distinct; i++) {
        free(seen[i]);
    }
}

static void test_bad_requests(void) {
    Motive motive = make_motive(&SCENARIOS[0]);
    GrowRequest request = make_request(&SCENARIOS[0], &motive, 8);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(NULL, &plan) == GROW_ERR_NULL);
    assert(grow_build_plan(&request, NULL) == GROW_ERR_NULL);

    GrowRequest changed = request;
    changed.bars = 6;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST && plan == NULL);
    changed = request;
    changed.meter.beats_per_bar = 5;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    /* An antecedent ends in a cadence weaker than the theme's, and no cadence is weaker than a half cadence. */
    changed = request;
    changed.pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
    changed.pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
    changed.cadence = PLAN_CADENCE_HC;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    changed.pairing.beginning = (PlanThemeBeginning)7;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    changed = request;
    changed.cadence = PLAN_CADENCE_DC;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);

    /* A motive that does not fit in a bar. */
    Motive long_motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = request.meter;
    assert(motive_from_notes_text("E4:q G4:q F4:q D4:q C4:q", &options, &long_motive, NULL) == MOTIVE_OK);
    changed = request;
    changed.motive = &long_motive;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_MOTIVE_TOO_LONG && plan == NULL);

    /* A motive of one note has nothing to fragment. */
    Motive single;
    assert(motive_from_notes_text("E4:q", &options, &single, NULL) == MOTIVE_OK);
    changed.motive = &single;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);

    /* grow_run wants an open theme. */
    GrowConfig config;
    GrowOutcome outcome;
    memset(&config, 0, sizeof(config));
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    grow_outcome_destroy(&outcome);
    assert(grow_run(plan, &config, &outcome) == GROW_ERR_BAD_REQUEST);
    assert(grow_run(NULL, &config, &outcome) == GROW_ERR_NULL);
    plan_destroy(plan);
}

/* What grow_build_plan makes, before anything runs. */
static void test_built_plan(void) {
    Motive motive = make_motive(&SCENARIOS[1]);
    GrowRequest request = make_request(&SCENARIOS[1], &motive, 8);
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    NodeId theme = find_node(plan, "theme");
    NodeId material = find_node(plan, "material");
    assert(theme != PLAN_NODE_NONE && material != PLAN_NODE_NONE);
    assert(plan_node_state(plan, theme) == PLAN_STATE_CONSTRAINED && plan_node_child_count(plan, theme) == 0);
    bool archetype = false;
    bool pairing = false;
    bool cadence = false;
    bool key = false;
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        PlanConstraintKind kind = plan_node_constraint_at(plan, theme, i)->kind;
        archetype = archetype || kind == PLAN_CONSTRAINT_ARCHETYPE;
        pairing = pairing || kind == PLAN_CONSTRAINT_THEME_PAIRING;
        cadence = cadence || kind == PLAN_CONSTRAINT_CADENCE;
        key = key || kind == PLAN_CONSTRAINT_KEY;
    }
    assert(archetype && pairing && cadence && key);
    assert(plan_node_constraint_count(plan, material) == 1 &&
           plan_node_constraint_at(plan, material, 0)->kind == PLAN_CONSTRAINT_MATERIAL);
    NodeId m0 = find_node(plan, "m0");
    assert(m0 != PLAN_NODE_NONE && plan_node_state(plan, m0) == PLAN_STATE_GIVEN);
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, m0, &span, 1) == 1 && plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN);

    /* The plan survives its own JSON. */
    char *json = plan_to_json(plan, NULL);
    PiecePlan *again = NULL;
    assert(json && plan_from_json(json, &again, NULL) == PLAN_OK);
    char *second = plan_to_json(again, NULL);
    assert(second && strcmp(json, second) == 0);
    free(json);
    free(second);
    plan_destroy(again);
    plan_destroy(plan);
}

/* The pitches and the length of the motive written on the motive node `name`. */
static Motive written_motive(const PiecePlan *plan, const char *name, const Motive *template_motive) {
    Motive motive = *template_motive;
    motive.key.tonic_pitch_class = SCENARIOS[0].tonic;
    motive.key.mode = SCENARIOS[0].mode;
    motive.key_source = MOTIVE_KEY_GIVEN;
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, find_node(plan, name), &span, 1) == 1);
    const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
    motive.note_count = notes->note_count;
    memcpy(motive.notes, notes->notes, notes->note_count * sizeof(PlanNote));
    motive.length = plan_lane_span_span(plan, span).length;
    motive.harmony_count = 0;
    motive.feature_count = 0;
    return motive;
}

static bool same_notes(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi_pitch != b->notes[i].midi_pitch || rational_cmp(a->notes[i].offset, b->notes[i].offset) != 0 ||
            rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/*
 * Motive-forms (B8): in a sentence the repetition answers in the dominant form where its chords go to V (bars 3 and 4),
 * the two fragments are the basic idea liquidated in units that halve, and the same seed gives the same theme.
 */
static void test_motive_forms(void) {
    Motive motive = make_motive(&SCENARIOS[0]);
    GrowRequest request = make_request(&SCENARIOS[0], &motive, 8);
    size_t dominant = 0;
    size_t halved = 0;
    for (unsigned long seed = 1; seed <= 10; seed++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.basic = &motive;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        assert(grow_run(plan, &config, &outcome) == GROW_OK);

        /* The repetition's chords: when the first one is the dominant, its motive is the basic idea's dominant form. */
        NodeId repetition = find_node(plan, "repetition");
        PlanSpan span = plan_node_span(plan, repetition);
        LaneSpanId chord = PLAN_LANE_SPAN_NONE;
        assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, &chord, 1) >= 1);
        const char *numeral = plan_lane_span_payload(plan, chord)->harmony.roman_numeral;
        Motive m0 = written_motive(plan, "m0", &motive);
        Motive m1 = written_motive(plan, "m1", &motive);
        if (numeral[0] == 'V' && numeral[1] != 'I') {
            VariantChain chain;
            Motive expected;
            memset(&chain, 0, sizeof(chain));
            assert(variant_step_init(&chain.steps[0], "dominant-form"));
            chain.step_count = 1;
            assert(variant_apply_chain(&m0, &chain, 1, &expected, NULL) == VARIANT_OK);
            /* A form below C#4 is taken up an octave. */
            int lowest = 127;
            for (size_t n = 0; n < expected.note_count; n++) {
                lowest = expected.notes[n].midi_pitch < lowest ? expected.notes[n].midi_pitch : lowest;
            }
            if (lowest < 58) {
                for (size_t n = 0; n < expected.note_count; n++) {
                    expected.notes[n].midi_pitch += 12;
                }
            }
            if (same_notes(&m1, &expected)) {
                dominant++;
            }
        }

        /* The fragments: each half the unit of the one before (the basic idea, a bar here, then half, then a quarter). */
        Motive m2 = written_motive(plan, "m2", &motive);
        Motive m3 = written_motive(plan, "m3", &motive);
        assert(ticks(m3.length) <= ticks(m2.length) && ticks(m2.length) < ticks(m0.length));
        if (ticks(m2.length) * 2 == ticks(m0.length) && ticks(m3.length) * 4 == ticks(m0.length)) {
            halved++;
        }

        /* The same seed gives the same theme, motives and records. */
        PiecePlan *again = NULL;
        GrowOutcome second;
        assert(grow_build_plan(&request, &again) == GROW_OK && grow_run(again, &config, &second) == GROW_OK);
        char *first_json = plan_to_json(plan, NULL);
        char *second_json = plan_to_json(again, NULL);
        char *first_records = records_text(&outcome);
        char *second_records = records_text(&second);
        assert(strcmp(first_json, second_json) == 0 && strcmp(first_records, second_records) == 0);
        /* The motives stage names the chain, the connection and the derivation of each form. */
        assert(strstr(first_records, "motive-form") != NULL && strstr(first_records, "%: ") != NULL);
        free(first_json);
        free(second_json);
        free(first_records);
        free(second_records);
        grow_outcome_destroy(&outcome);
        grow_outcome_destroy(&second);
        plan_destroy(plan);
        plan_destroy(again);
    }
    assert(dominant > 0);
    assert(halved > 0);
}

int main(void) {
    test_built_plan();
    test_bad_requests();
    test_sentences();
    test_seeds_vary();
    test_motive_forms();
    puts("test_grow_sentence: ok");
    return 0;
}
