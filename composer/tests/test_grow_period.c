#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-form/hybrid_rules.h>
#include <standard-form/idea_label.h>
#include <standard-form/period_rules.h>
#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <standard-form/phrase_role.h>
#include <standard-form/sentence_rules.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

/*
 * Growing a theme from each of the nine pairings of a beginning and an ending, in four keys and meters, 8 and 4 bars,
 * seeds 1 to 10. Each run must complete; the plan it leaves is read back here and held to standard-form's rule
 * functions, worked out from the nodes and the lanes and not from the operator; the same seed gives the same plan and
 * the same records.
 *
 * With two arguments, a beginning and an ending (0 to 2 each, the order of PlanThemeBeginning and PlanThemeEnding),
 * it runs that pairing alone; with none, all nine.
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
#define MAX_NODES 16

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

static GrowRequest make_request(const Scenario *scenario, const Motive *motive, size_t bars, PlanThemeBeginning beginning,
                                PlanThemeEnding ending) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key.tonic_pitch_class = scenario->tonic;
    request.key.mode = scenario->mode;
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = scenario->unit;
    request.bars = bars;
    request.pairing.beginning = beginning;
    request.pairing.ending = ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
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

/* The lane spans of a lane over a span, in time order, tiling it and all written. */
static size_t tiling(const PiecePlan *plan, PlanLaneKind lane, PlanSpan within, LaneSpanId *out, size_t capacity) {
    size_t count = plan_lane_find(plan, lane, 0, within, out, capacity);
    assert(count > 0 && count <= capacity);
    long at = ticks(within.start);
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(plan, out[i]);
        assert(ticks(span.start) == at);
        assert(plan_lane_span_state(plan, out[i]) != PLAN_STATE_CONSTRAINED);
        at += ticks(span.length);
    }
    assert(at == ticks(within.start) + ticks(within.length));
    return count;
}

static PhraseBeginning beginning_of(PlanThemeBeginning beginning) {
    switch (beginning) {
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return PHRASE_BEGINNING_ANTECEDENT;
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        return PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return PHRASE_BEGINNING_PRESENTATION;
    }
    assert(0);
    return PHRASE_BEGINNING_COUNT;
}

static PhraseEnding ending_of(PlanThemeEnding ending) {
    switch (ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return PHRASE_ENDING_CONTINUATION;
    case PLAN_THEME_ENDING_CONSEQUENT:
        return PHRASE_ENDING_CONSEQUENT;
    case PLAN_THEME_ENDING_CADENTIAL:
        return PHRASE_ENDING_CADENTIAL;
    }
    assert(0);
    return PHRASE_ENDING_COUNT;
}

/* --------------------------------------------------- the nodes a pairing makes */

typedef struct {
    const char *name;
    PlanNodeKind kind;
    long start; /* in eighths of the theme */
    long units;
} Expected;

static size_t expected_nodes(PlanThemeBeginning beginning, PlanThemeEnding ending, Expected *out) {
    size_t count = 0;
    bool antecedent = beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    out[count++] = antecedent ? (Expected){"antecedent", PLAN_NODE_PHRASE, 0, 4}
                              : (Expected){"phrase", PLAN_NODE_PHRASE, 0, 8};
    if (beginning == PLAN_THEME_BEGINNING_PRESENTATION) {
        out[count++] = (Expected){"presentation", PLAN_NODE_SUBPHRASE, 0, 4};
    } else if (beginning == PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA) {
        out[count++] = (Expected){"compound basic idea", PLAN_NODE_SUBPHRASE, 0, 4};
    }
    out[count++] = (Expected){"basic idea", PLAN_NODE_IDEA, 0, 2};
    out[count++] = beginning == PLAN_THEME_BEGINNING_PRESENTATION ? (Expected){"repetition", PLAN_NODE_IDEA, 2, 2}
                                                                  : (Expected){"contrasting idea", PLAN_NODE_IDEA, 2, 2};
    if (antecedent) {
        out[count++] = ending == PLAN_THEME_ENDING_CONSEQUENT ? (Expected){"consequent", PLAN_NODE_PHRASE, 4, 4}
                                                              : (Expected){"second phrase", PLAN_NODE_PHRASE, 4, 4};
    }
    switch (ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        out[count++] = (Expected){"continuation", PLAN_NODE_SUBPHRASE, 4, 4};
        out[count++] = (Expected){"fragment 1", PLAN_NODE_IDEA, 4, 1};
        out[count++] = (Expected){"fragment 2", PLAN_NODE_IDEA, 5, 1};
        out[count++] = (Expected){"cadential", PLAN_NODE_IDEA, 6, 2};
        break;
    case PLAN_THEME_ENDING_CONSEQUENT:
        if (!antecedent) {
            out[count++] = (Expected){"consequent", PLAN_NODE_SUBPHRASE, 4, 4};
        }
        out[count++] = (Expected){"consequent basic idea", PLAN_NODE_IDEA, 4, 2};
        out[count++] = (Expected){"consequent contrasting idea", PLAN_NODE_IDEA, 6, 2};
        break;
    case PLAN_THEME_ENDING_CADENTIAL:
        out[count++] = (Expected){"cadential", PLAN_NODE_SUBPHRASE, 4, 4};
        out[count++] = (Expected){"cadential 1", PLAN_NODE_IDEA, 4, 2};
        out[count++] = (Expected){"cadential 2", PLAN_NODE_IDEA, 6, 2};
        break;
    }
    assert(count <= MAX_NODES);
    return count;
}

static PlanThemePairing theme_pairing(const PiecePlan *plan, NodeId theme) {
    PlanThemePairing pairing = {PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION};
    bool found = false;
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            pairing = constraint->params.theme_pairing;
            found = true;
        }
    }
    assert(found);
    return pairing;
}

static bool node_cadence(const PiecePlan *plan, NodeId node, CadenceType *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            *out = constraint->params.cadence;
            return true;
        }
    }
    return false;
}

static int direction(int semitones) {
    return semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
}

static bool same_contour(const PlanNotes *a, const PlanNotes *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 1; i < a->note_count; i++) {
        if (direction(a->notes[i].midi_pitch - a->notes[i - 1].midi_pitch) !=
            direction(b->notes[i].midi_pitch - b->notes[i - 1].midi_pitch)) {
            return false;
        }
    }
    return true;
}

static bool same_pitches(const PlanNotes *a, const PlanNotes *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi_pitch != b->notes[i].midi_pitch) {
            return false;
        }
    }
    return true;
}

static const PlanNotes *motive_notes(const PiecePlan *plan, const char *name) {
    NodeId node = find_node(plan, name);
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(node != PLAN_NODE_NONE && plan_node_lane_spans(plan, node, &span, 1) == 1);
    assert(plan_lane_span_state(plan, span) != PLAN_STATE_CONSTRAINED);
    return &plan_lane_span_payload(plan, span)->notes;
}

/* The length of a motive node's melody, in ticks. */
static long motive_length(const PiecePlan *plan, const char *name) {
    NodeId node = find_node(plan, name);
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(node != PLAN_NODE_NONE && plan_node_lane_spans(plan, node, &span, 1) == 1);
    return ticks(plan_lane_span_span(plan, span).length);
}

/* ------------------------------------------------------------------ checks */

static void check_structure(const PiecePlan *plan, const Scenario *scenario, size_t bars, PlanThemeBeginning beginning,
                            PlanThemeEnding ending) {
    Rational bar;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    long bar_ticks = ticks(bar);
    NodeId theme = find_node(plan, "theme");
    assert(theme != PLAN_NODE_NONE && plan_node_state(plan, theme) == PLAN_STATE_REALIZED);
    assert(ticks(plan_node_span(plan, theme).length) == (long)bars * bar_ticks);

    /* The nodes, in eighths of the theme. */
    Expected nodes[MAX_NODES];
    size_t count = expected_nodes(beginning, ending, nodes);
    for (size_t i = 0; i < count; i++) {
        NodeId node = find_node(plan, nodes[i].name);
        assert(node != PLAN_NODE_NONE && plan_node_kind(plan, node) == nodes[i].kind);
        assert(plan_node_state(plan, node) == PLAN_STATE_REALIZED);
        PlanSpan span = plan_node_span(plan, node);
        assert(ticks(span.start) * 8 == nodes[i].start * (long)bars * bar_ticks);
        assert(ticks(span.length) * 8 == nodes[i].units * (long)bars * bar_ticks);
    }
    size_t phrases = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_PHRASE && name && strcmp(name, "material") != 0) {
            phrases++;
        }
    }
    /* A theme with an antecedent has two phrases; any other has one. */
    assert(phrases == (beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? 2u : 1u));

    /* The pairing is the one asked for, and the form is the one the pairing makes. */
    PlanThemePairing pairing = theme_pairing(plan, theme);
    assert(pairing.beginning == beginning && pairing.ending == ending);
    PhraseLevelForm form;
    assert(standard_form_pairing_form(beginning_of(beginning), ending_of(ending), &form));
    PlanArchetype archetype = PLAN_ARCHETYPE_SENTENCE;
    for (size_t i = 0; i < plan_node_constraint_count(plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE) {
            archetype = constraint->params.archetype;
        }
    }
    assert((form == PHRASE_LEVEL_FORM_SENTENCE) == (archetype == PLAN_ARCHETYPE_SENTENCE));
    assert((form == PHRASE_LEVEL_FORM_PERIOD) == (archetype == PLAN_ARCHETYPE_PERIOD));
    assert((form == PHRASE_LEVEL_FORM_HYBRID) == (archetype == PLAN_ARCHETYPE_HYBRID));
    if (form == PHRASE_LEVEL_FORM_HYBRID) {
        assert(standard_form_pairing_is_hybrid(beginning_of(beginning), ending_of(ending)));
    }

    long basic = ticks(plan_node_span(plan, find_node(plan, "basic idea")).length);
    assert(standard_form_basic_idea_length_valid((size_t)(basic / bar_ticks)));

    /* The motives: m0 is the source; a contrasting idea does not read as a repetition of it. */
    const PlanNotes *m0 = motive_notes(plan, "m0");
    if (beginning != PLAN_THEME_BEGINNING_PRESENTATION) {
        const PlanNotes *contrast = motive_notes(plan, "m1");
        assert(!standard_form_idea_reads_as_repetition(same_contour(m0, contrast)));
    }
    if (ending == PLAN_THEME_ENDING_CONTINUATION) {
        for (const char *const *fragment = (const char *const[]){"fragment 1", "fragment 2", NULL}; *fragment;
             fragment++) {
            long length = ticks(plan_node_span(plan, find_node(plan, *fragment)).length);
            assert(standard_form_is_fragmentation((size_t)basic, (size_t)length));
        }
        /* The fragments of the notes too: each is shorter than the motive it was made from. */
        for (const char *const *name = (const char *const[]){"m2", "m3", NULL}; *name; name++) {
            assert(standard_form_is_fragmentation((size_t)motive_length(plan, "m0"), (size_t)motive_length(plan, *name)));
        }
    }
    if (ending == PLAN_THEME_ENDING_CONSEQUENT) {
        /* The consequent restates the basic idea in its motive: the same notes, in a transposition if the restated
           family chose one, so the same contour. */
        assert(same_contour(m0, motive_notes(plan, "m2")));
        if (beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
            /* ... and ends with a contrasting idea that is not the antecedent's. */
            assert(!same_pitches(motive_notes(plan, "m1"), motive_notes(plan, "m3")));
        }
        assert(!standard_form_idea_reads_as_repetition(same_contour(m0, motive_notes(plan, "m3"))));
    }
    if (ending == PLAN_THEME_ENDING_CADENTIAL) {
        /* Two ideas liquidated from m0, the second at least as much as the first. */
        const PlanNotes *first = motive_notes(plan, "m2");
        const PlanNotes *second = motive_notes(plan, "m3");
        assert(first->note_count < m0->note_count && second->note_count <= first->note_count);
    }

    /* The period's rules, on the cadences the two phrases carry. */
    if (form == PHRASE_LEVEL_FORM_PERIOD) {
        CadenceType weak;
        CadenceType strong;
        assert(node_cadence(plan, find_node(plan, "antecedent"), &weak));
        assert(node_cadence(plan, find_node(plan, "consequent"), &strong));
        CadenceType first = weak;
        CadenceType second = strong;
        assert(standard_form_phrase_roles_in_order(PHRASE_ROLE_ANTECEDENT, PHRASE_ROLE_CONSEQUENT));
        assert(standard_form_period_phrase_ideas_valid(IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_CONTRASTING_IDEA));
        assert(standard_form_period_cadences_valid(first, second));
        assert(harmony_cadence_strength(first) < harmony_cadence_strength(second));
        assert(standard_form_antecedent_cadence_usual(first) && standard_form_consequent_cadence_usual(second));
        assert(standard_form_antecedent_length_usual(bars / 2) == (bars == 8));
        assert(standard_form_consequent_length_usual(bars / 2, bars / 2));
    }
    if (beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        CadenceType weak;
        CadenceType strong;
        NodeId second = find_node(plan, ending == PLAN_THEME_ENDING_CONSEQUENT ? "consequent" : "second phrase");
        assert(node_cadence(plan, find_node(plan, "antecedent"), &weak) && node_cadence(plan, second, &strong));
        assert(harmony_cadence_strength(weak) <
               harmony_cadence_strength(strong));
    }
}

/* The chord a harmony span writes, and whether its roman numeral names a dominant. */
static bool is_dominant(const PiecePlan *plan, LaneSpanId span) {
    const char *roman = plan_lane_span_payload(plan, span)->harmony.roman_numeral;
    return roman[0] == 'V' && (roman[1] == '\0' || roman[1] == '7');
}

static bool is_tonic(const PiecePlan *plan, LaneSpanId span) {
    const char *roman = plan_lane_span_payload(plan, span)->harmony.roman_numeral;
    return (strcmp(roman, "I") == 0 || strcmp(roman, "i") == 0);
}

static LaneSpanId last_in(const LaneSpanId *ids, size_t count, const PiecePlan *plan, PlanSpan within) {
    LaneSpanId last = PLAN_LANE_SPAN_NONE;
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        if (ticks(span.start) < ticks(within.start) + ticks(within.length)) {
            last = ids[i];
        }
    }
    return last;
}

static void check_lanes(const PiecePlan *plan, const Scenario *scenario, size_t bars, PlanThemeBeginning beginning,
                        PlanThemeEnding ending) {
    LaneSpanId ids[96];
    NodeId theme_node = find_node(plan, "theme");
    PlanSpan theme = plan_node_span(plan, theme_node);
    Rational bar;
    assert(rational_make(scenario->beats, (long long)scenario->unit, &bar) == RATIONAL_OK);
    long bar_ticks = ticks(bar);
    size_t cells_per_bar = scenario->unit == 8 || (scenario->beats == 4 && scenario->unit == 4) ? 2 : 1;
    long cell_ticks = bar_ticks / (long)cells_per_bar;
    long half = (long)bars * bar_ticks / 2;
    bool antecedent = beginning == PLAN_THEME_BEGINNING_ANTECEDENT;

    /* Harmony: chords on cells, tiling the theme, starting on the tonic. */
    size_t chords = tiling(plan, PLAN_LANE_HARMONY, theme, ids, 96);
    assert(is_tonic(plan, ids[0]));
    for (size_t i = 0; i < chords; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        assert(ticks(span.start) % cell_ticks == 0 && ticks(span.length) % cell_ticks == 0);
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED);
    }
    /* The cadence is on the last chord of the theme. */
    CadenceType final_cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    assert(node_cadence(plan, find_node(plan, antecedent ? (ending == PLAN_THEME_ENDING_CONSEQUENT ? "consequent" : "second phrase") : "phrase"),
                        &final_cadence));
    assert(final_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    const PlanConstraint *cadence = plan_lane_span_constraint_count(plan, ids[chords - 1]) > 0
                                        ? plan_lane_span_constraint_at(plan, ids[chords - 1], 0)
                                        : NULL;
    assert(cadence && cadence->kind == PLAN_CONSTRAINT_CADENCE && cadence->params.cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(is_tonic(plan, ids[chords - 1]));
    if (antecedent) {
        /* The antecedent's last chord is its weak cadence's: a dominant for a half cadence. */
        PlanSpan first_half = {theme.start, {half, TICKS}};
        Rational first_length;
        assert(rational_make(half, TICKS, &first_length) == RATIONAL_OK);
        first_half.length = first_length;
        LaneSpanId last = last_in(ids, chords, plan, first_half);
        CadenceType weak = CADENCE_TYPE_HALF;
        assert(node_cadence(plan, find_node(plan, "antecedent"), &weak));
        assert(weak == CADENCE_TYPE_HALF);
        assert(is_dominant(plan, last));
        assert(plan_lane_span_constraint_count(plan, last) > 0 &&
               plan_lane_span_constraint_at(plan, last, 0)->kind == PLAN_CONSTRAINT_CADENCE &&
               plan_lane_span_constraint_at(plan, last, 0)->params.cadence == CADENCE_TYPE_HALF);
        /* No chord crosses the antecedent's end. */
        for (size_t i = 0; i < chords; i++) {
            PlanSpan span = plan_lane_span_span(plan, ids[i]);
            assert(!(ticks(span.start) < half && ticks(span.start) + ticks(span.length) > half));
        }
    }

    /* Bass: a span for each chord, written. */
    size_t basses = tiling(plan, PLAN_LANE_BASS, theme, ids, 96);
    assert(basses == chords);

    /* Melody: one span for each idea, tiling the theme, each written. */
    Expected nodes[MAX_NODES];
    size_t count = expected_nodes(beginning, ending, nodes);
    const char *idea_names[8];
    size_t ideas = 0;
    for (size_t i = 0; i < count; i++) {
        if (nodes[i].kind == PLAN_NODE_IDEA) {
            idea_names[ideas++] = nodes[i].name;
        }
    }
    size_t melodies = tiling(plan, PLAN_LANE_MELODY, theme, ids, 96);
    assert(melodies == ideas);
    for (size_t i = 0; i < melodies; i++) {
        assert(plan_lane_span_owner(plan, ids[i]) == find_node(plan, idea_names[i]));
    }
    /* The cadence arrives on the tonic. */
    const PlanNotes *last = &plan_lane_span_payload(plan, ids[melodies - 1])->notes;
    int tonic_pc = (last->notes[last->note_count - 1].midi_pitch % 12 + 12) % 12;
    assert(tonic_pc == scenario->tonic);

    /* The range, and one highest note in each phrase of the theme's melody. */
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        const char *name = plan_node_name(plan, node);
        if (plan_node_kind(plan, node) != PLAN_NODE_PHRASE || !name || strcmp(name, "material") == 0) {
            continue;
        }
        PlanSpan phrase = plan_node_span(plan, node);
        LaneSpanId inside[16];
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, phrase, inside, 16);
        assert(found > 0 && found <= 16);
        int top = 0;
        size_t at_top = 0;
        for (size_t i = 0; i < found; i++) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, inside[i])->notes;
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
    assert(material >= 2);
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

static void run_scenario(const Scenario *scenario, size_t bars, unsigned long seed, PlanThemeBeginning beginning,
                         PlanThemeEnding ending) {
    Motive motive = make_motive(scenario);
    GrowRequest request = make_request(scenario, &motive, bars, beginning, ending);
    GrowConfig config;
    char *plans[2];
    char *records[2];
    /* The same seed is run twice for two of the seeds, to be sure it gives the same plan and records. */
    int rounds = seed == 1 || seed == 10 ? 2 : 1;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    for (int round = 0; round < rounds; round++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        GrowResult result = grow_run(plan, &config, &outcome);
        if (result != GROW_OK) {
            fprintf(stderr, "%s, %zu bars, pairing %d+%d, seed %lu: %s at %s\n", scenario->name, bars, (int)beginning,
                    (int)ending, seed, grow_result_name(result), grow_stage_name(outcome.stuck_stage));
        }
        assert(result == GROW_OK && !outcome.stuck);
        if (round == 0) {
            check_structure(plan, scenario, bars, beginning, ending);
            check_lanes(plan, scenario, bars, beginning, ending);
            check_render(plan, bars, scenario);
            assert(outcome.score.denominator >= 1 && outcome.score.numerator <= outcome.score.denominator);
            assert(outcome.requested_beginning == (int)beginning_of(beginning));
            assert(outcome.requested_ending == (int)ending_of(ending));
        }
        plans[round] = plan_to_json(plan, NULL);
        records[round] = records_text(&outcome);
        assert(plans[round]);
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    if (rounds == 2) {
        assert(strcmp(plans[0], plans[1]) == 0);
        assert(strcmp(records[0], records[1]) == 0);
    }
    for (int round = 0; round < rounds; round++) {
        free(plans[round]);
        free(records[round]);
    }
}

static void test_pairing(PlanThemeBeginning beginning, PlanThemeEnding ending) {
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (size_t bars = 8; bars >= 4; bars -= 4) {
            for (unsigned long seed = 1; seed <= 10; seed++) {
                run_scenario(&SCENARIOS[s], bars, seed, beginning, ending);
            }
        }
    }
}

/* The antecedent's weak cadence: half by default, imperfect authentic when asked; the melody ends on the third. */
static void test_first_cadence(void) {
    const Scenario *scenario = &SCENARIOS[0];
    Motive motive = make_motive(scenario);
    GrowRequest request = make_request(scenario, &motive, 8, PLAN_THEME_BEGINNING_ANTECEDENT,
                                       PLAN_THEME_ENDING_CONSEQUENT);
    request.first_cadence = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    for (unsigned long seed = 1; seed <= 3; seed++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        assert(grow_run(plan, &config, &outcome) == GROW_OK);
        CadenceType weak = CADENCE_TYPE_HALF;
        assert(node_cadence(plan, find_node(plan, "antecedent"), &weak) && weak == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
        /* The antecedent's melody ends on the third of the key, the tone of an imperfect authentic cadence. */
        LaneSpanId melodies[8];
        PlanSpan antecedent = plan_node_span(plan, find_node(plan, "antecedent"));
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, antecedent, melodies, 8);
        assert(found == 2);
        const PlanNotes *notes = &plan_lane_span_payload(plan, melodies[1])->notes;
        assert((notes->notes[notes->note_count - 1].midi_pitch % 12 + 12) % 12 == (scenario->tonic + 4) % 12);
        /* Its harmony ends on the tonic, not the dominant. */
        LaneSpanId chords[64];
        size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, antecedent, chords, 64);
        assert(count > 0 && is_tonic(plan, chords[count - 1]));
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
}

static void test_bad_requests(void) {
    const Scenario *scenario = &SCENARIOS[0];
    Motive motive = make_motive(scenario);
    PiecePlan *plan = NULL;
    GrowRequest period = make_request(scenario, &motive, 8, PLAN_THEME_BEGINNING_ANTECEDENT,
                                      PLAN_THEME_ENDING_CONSEQUENT);
    GrowRequest changed = period;
    assert(grow_build_plan(&period, &plan) == GROW_OK);
    plan_destroy(plan);
    /* Equal cadences are not a period, and nothing is weaker than a half cadence. */
    changed.first_cadence = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    changed.cadence = CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST && plan == NULL);
    changed = period;
    changed.cadence = CADENCE_TYPE_HALF;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    /* An antecedent ends in a half or an imperfect authentic cadence, and no other. */
    changed = period;
    changed.first_cadence = CADENCE_TYPE_DECEPTIVE;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    changed.first_cadence = CADENCE_TYPE_PLAGAL;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
    /* The weak cadence is read only for an antecedent: a presentation ignores it. */
    changed = make_request(scenario, &motive, 8, PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONSEQUENT);
    changed.first_cadence = CADENCE_TYPE_DECEPTIVE;
    assert(grow_build_plan(&changed, &plan) == GROW_OK);
    plan_destroy(plan);
    /* A pairing that is none of the nine. */
    changed = period;
    changed.pairing.ending = (PlanThemeEnding)9;
    assert(grow_build_plan(&changed, &plan) == GROW_ERR_BAD_REQUEST);
}

int main(int argc, char **argv) {
    if (argc == 3) {
        PlanThemeBeginning beginning = (PlanThemeBeginning)atoi(argv[1]);
        PlanThemeEnding ending = (PlanThemeEnding)atoi(argv[2]);
        assert(beginning >= PLAN_THEME_BEGINNING_PRESENTATION && beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA);
        assert(ending >= PLAN_THEME_ENDING_CONTINUATION && ending <= PLAN_THEME_ENDING_CADENTIAL);
        test_pairing(beginning, ending);
        if (beginning == PLAN_THEME_BEGINNING_ANTECEDENT && ending == PLAN_THEME_ENDING_CONSEQUENT) {
            test_first_cadence();
            test_bad_requests();
        }
        printf("test_grow_period %d %d: ok\n", (int)beginning, (int)ending);
        return 0;
    }
    for (int beginning = PLAN_THEME_BEGINNING_PRESENTATION; beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA;
         beginning++) {
        for (int ending = PLAN_THEME_ENDING_CONTINUATION; ending <= PLAN_THEME_ENDING_CADENTIAL; ending++) {
            test_pairing((PlanThemeBeginning)beginning, (PlanThemeEnding)ending);
        }
    }
    test_first_cadence();
    test_bad_requests();
    puts("test_grow_period: ok");
    return 0;
}
