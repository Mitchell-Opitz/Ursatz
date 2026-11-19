#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "support/test_support.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The liquidation section (B2 of P49, R-494). tests/fixtures/forms/liquidation-form.json in C major 3/4, A minor 3/4,
 * C major 2/4, C major 6/8 and (B4 of P51, R-505) C major 4/4, seeds 1 to 10, an invented motive: every composed run has five ideas of 1, 1, 1, 1/2 and
 * 1/2 bars, V (or V7) in the bass throughout, a last chord V or V7, a last melody note that is a tone of V, a liquidated
 * idea with fewer notes than the basic idea, a stepwise scale line and an ascending broken chord; at least 9 of 10 compose
 * in each row. Then the plan round-trips through plan JSON, and the applies rule is false off its cases.
 */

#define SEEDS 10
#define COMPOSED_LEAST 9

typedef struct {
    const char *name;
    int tonic;
    HarmonyMode mode;
    int beats;
    int unit;
} Row;

static const Row ROWS[] = {{"C major 3/4", 0, HARMONY_MODE_MAJOR, 3, 4},
                           {"A minor 3/4", 9, HARMONY_MODE_MINOR, 3, 4},
                           {"C major 2/4", 0, HARMONY_MODE_MAJOR, 2, 4},
                           {"C major 6/8", 0, HARMONY_MODE_MAJOR, 6, 8},
                           {"C major 4/4", 0, HARMONY_MODE_MAJOR, 4, 4}};

static FormSpec *load_form(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/liquidation-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    assert(text);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        exit(2);
    }
    free(text);
    return form;
}

static PiecePlan *compose(const FormSpec *form, const Row *row, unsigned long seed, ComposeOutcome *outcome,
                          PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key_mode(row->tonic, row->mode);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = (unsigned)row->beats;
    request.meter.beat_unit = (unsigned)row->unit;
    request.seed = seed;
    PiecePlan *plan = NULL;
    PieceResult built = form_build_plan(form, &request, &plan);
    if (built != PIECE_OK) {
        fprintf(stderr, "form_build_plan: %s\n", piece_result_name(built));
        exit(2);
    }
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

typedef struct {
    int midi;
    Rational onset;
} Heard;

static size_t piece_sections(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            sections[count++] = child;
        }
    }
    return count;
}

static Rational add_time(Rational a, Rational b) {
    Rational out;
    assert(rational_add(a, b, &out) == RATIONAL_OK);
    return out;
}

/* The notes of the melody that start in [from, to), in onset order. */
static size_t melody_notes(const PiecePlan *plan, Rational from, Rational to, Heard *out, size_t cap) {
    LaneSpanId ids[16];
    PlanSpan range = {from, {0, 1}};
    assert(rational_sub(to, from, &range.length) == RATIONAL_OK);
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, range, ids, 16);
    size_t count = 0;
    for (size_t s = 0; s < found && s < 16; s++) {
        Rational origin = plan_lane_span_span(plan, ids[s]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            Rational onset = add_time(origin, notes->notes[i].offset);
            if (rational_cmp(onset, from) >= 0 && rational_cmp(onset, to) < 0 && count < cap) {
                out[count].midi = notes->notes[i].midi;
                out[count].onset = onset;
                count++;
            }
        }
    }
    for (size_t i = 1; i < count; i++) {
        for (size_t j = i; j > 0 && rational_cmp(out[j].onset, out[j - 1].onset) < 0; j--) {
            Heard swap = out[j];
            out[j] = out[j - 1];
            out[j - 1] = swap;
        }
    }
    return count;
}

/* The notes of the motive an idea's melody asks for: the idea's melody span names a motive node, whose own melody holds them. */
static size_t motive_notes_of(const PiecePlan *plan, NodeId idea) {
    LaneSpanId spans[8];
    size_t found = plan_node_lane_spans(plan, idea, spans, 8);
    for (size_t i = 0; i < found && i < 8; i++) {
        if (plan_lane_span_lane(plan, spans[i]) != PLAN_LANE_MELODY) {
            continue;
        }
        for (size_t c = 0; c < plan_lane_span_constraint_count(plan, spans[i]); c++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, spans[i], c);
            if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF) {
                continue;
            }
            LaneSpanId own[8];
            size_t owned = plan_node_lane_spans(plan, constraint->params.motive.motive, own, 8);
            for (size_t k = 0; k < owned && k < 8; k++) {
                if (plan_lane_span_lane(plan, own[k]) == PLAN_LANE_MELODY) {
                    return plan_lane_span_payload(plan, own[k])->notes.note_count;
                }
            }
        }
    }
    return 0;
}

static bool is_dominant_numeral(const char *numeral) {
    return strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0 || strcmp(numeral, "V6") == 0 ||
           strcmp(numeral, "V65") == 0 || strcmp(numeral, "V6/5") == 0 || strcmp(numeral, "V4/3") == 0 ||
           strcmp(numeral, "V43") == 0 || strcmp(numeral, "V4/2") == 0 || strcmp(numeral, "V42") == 0;
}

/* One composed run: the checks of the header comment. */
static void check_run(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    NodeId l = sections[2];
    assert(plan_section_has_role(plan, l, SECTION_ROLE_LIQUIDATION));
    assert(!plan_section_or_empty(plan, l)->has_pairing &&
           !plan_section_or_empty(plan, l)->has_pairing);
    PlanSpan span = plan_node_span(plan, l);
    Rational end = add_time(span.start, span.length);
    Rational bar = {row->beats, row->unit};
    Rational half = {row->beats, row->unit * 2};

    /* One phrase, five ideas: 1, 1, 1, 1/2, 1/2 bars, named as the motives stage scores them. */
    assert(plan_node_child_count(plan, l) == 1);
    NodeId phrase = plan_node_child_at(plan, l, 0);
    assert(plan_node_child_count(plan, phrase) == 5);
    static const char *const NAMES[5] = {"basic idea", "repetition", "fragment 1", "fragment 2", "cadential"};
    Heard heard[5][16];
    size_t counts[5];
    Rational at = span.start;
    for (size_t i = 0; i < 5; i++) {
        NodeId idea = plan_node_child_at(plan, phrase, i);
        PlanSpan s = plan_node_span(plan, idea);
        Rational want = i < 3 ? bar : half;
        assert(strcmp(plan_node_name(plan, idea), NAMES[i]) == 0);
        assert(rational_cmp(s.start, at) == 0 && rational_cmp(s.length, want) == 0);
        at = add_time(at, want);
        counts[i] = melody_notes(plan, s.start, add_time(s.start, s.length), heard[i], 16);
        assert(counts[i] > 0);
    }
    assert(rational_cmp(at, end) == 0);

    /* V throughout: every chord of the section, and the bass on the dominant's pitch class. */
    int dominant = (row->tonic + 7) % 12;
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 16);
    assert(found > 0);
    for (size_t i = 0; i < found && i < 16; i++) {
        const char *numeral = plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral;
        if (!is_dominant_numeral(numeral)) {
            fprintf(stderr, "%s seed %lu: chord %s in the liquidation\n", row->name, seed, numeral);
        }
        assert(is_dominant_numeral(numeral));
    }
    const char *last = plan_lane_span_payload(plan, ids[found - 1])->harmony.roman_numeral;
    assert(strcmp(last, "V") == 0 || strcmp(last, "V7") == 0);
    found = plan_lane_find(plan, PLAN_LANE_BASS, 0, span, ids, 16);
    assert(found > 0);
    for (size_t i = 0; i < found && i < 16; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(((notes->notes[n].midi % 12) + 12) % 12 == dominant);
        }
    }

    /* The last melody note is a tone of V: degree 5, 7 or 2. */
    int tone = ((heard[4][counts[4] - 1].midi - row->tonic) % 12 + 12) % 12;
    assert(tone == 7 || tone == 11 || tone == 2);

    /* Fragment 1 has fewer notes than m0: the motives the ideas ask for (the melody fills the rest of the bar). */
    size_t basic_notes = motive_notes_of(plan, plan_node_child_at(plan, phrase, 0));
    size_t fragment_notes = motive_notes_of(plan, plan_node_child_at(plan, phrase, 2));
    if (!(fragment_notes > 0 && fragment_notes < basic_notes)) {
        fprintf(stderr, "%s seed %lu: fragment 1 asks for %zu notes, m0 %zu\n", row->name, seed, fragment_notes, basic_notes);
    }
    assert(fragment_notes > 0 && fragment_notes < basic_notes);
    /* The scale line is stepwise (1 or 2 semitones) and the broken chord ascends. */
    for (size_t i = 1; i < counts[3]; i++) {
        int step = abs(heard[3][i].midi - heard[3][i - 1].midi);
        if (step != 1 && step != 2) {
            fprintf(stderr, "%s seed %lu: scale line", row->name, seed);
            for (size_t n = 0; n < counts[3]; n++) {
                fprintf(stderr, " %d", heard[3][n].midi);
            }
            fprintf(stderr, "\n");
        }
        assert(step == 1 || step == 2);
    }
    for (size_t i = 1; i < counts[4]; i++) {
        assert(heard[4][i].midi > heard[4][i - 1].midi);
    }
}

static int rows_composed(const FormSpec *form) {
    int status = 0;
    for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); r++) {
        int composed = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            ComposeOutcome outcome;
            PieceResult ran;
            PiecePlan *plan = compose(form, &ROWS[r], seed, &outcome, &ran);
            if (ran == PIECE_OK) {
                check_run(plan, &ROWS[r], seed);
                composed++;
            } else {
                printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("liquidation-form in %s: %d of %d seeds compose\n", ROWS[r].name, composed, SEEDS);
        if (composed < COMPOSED_LEAST) {
            fprintf(stderr, "liquidation-form in %s: %d composed, at least %d expected\n", ROWS[r].name, composed,
                    COMPOSED_LEAST);
            status = 1;
        }
    }
    return status;
}

/* The plan of a built liquidation form (before composing, then composed) round-trips through plan JSON byte for byte. */
static void test_round_trip(const FormSpec *form) {
    ComposeOutcome outcome;
    PieceResult ran;
    PiecePlan *plan = compose(form, &ROWS[0], 1, &outcome, &ran);
    PlanJsonError error;
    char *text = plan_to_json(plan, test_plan_producer(), &error);
    assert(text);
    assert(strstr(text, "\"liquidation\""));
    PiecePlan *parsed = NULL;
    assert(plan_from_json(test_knowledge(), text, &parsed, &error) == PLAN_OK && parsed);
    char *again = plan_to_json(parsed, test_plan_producer(), &error);
    assert(again && strcmp(text, again) == 0);
    free(text);
    free(again);
    plan_destroy(parsed);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
}

static PiecePlan *plan_of(const FormSpec *form, const Motive *motive, bool modulate, PieceResult *result) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 3;
    request.meter.beat_unit = 4;
    request.seed = 1;
    request.motive = motive;
    request.modulate = modulate;
    PiecePlan *plan = NULL;
    *result = form_build_plan(form, &request, &plan);
    return plan;
}

/*
 * grow_liquidate_applies is true on the form's L and false off its cases: a section that is no liquidation, an L whose key
 * lane leaves home inside it, and (the form builder's own refusal) a motive of 2 bars, which no 4-bar section holds.
 */
static void test_applies(const FormSpec *form) {
    PieceResult result;
    PiecePlan *plan = plan_of(form, NULL, false, &result);
    assert(result == PIECE_OK);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    FillTarget target = {FILL_TARGET_NODE, sections[2]};
    FillTarget other = {FILL_TARGET_NODE, sections[1]};
    assert(grow_liquidate_applies(plan, target));
    assert(!grow_liquidate_applies(plan, other));
    plan_destroy(plan);

    /* A key lane that leaves home inside L (a key span of its own after the first): not a liquidation to grow. */
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/liquidation-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    assert(text);
    const char *label = "\"label\": \"Liquidation form\",";
    const char *section = "\"role\": \"liquidation\", \"bars\": 4, \"cadence\": \"half\"";
    char *at = strstr(text, label);
    char *in = strstr(text, section);
    assert(at && in);
    char edited[4096];
    snprintf(edited, sizeof(edited),
             "%.*s \"regions\": [{\"name\": \"away\", \"major\": [\"omt.relation.dominant\"], \"minor\": [\"omt.relation.dominant\"]}],%.*s%s, \"keys\": {\"end\": \"away\"}%s",
             (int)(at + strlen(label) - text), text, (int)(in - (at + strlen(label))), at + strlen(label), section,
             in + strlen(section));
    FormSpec *keyed = NULL;
    FormSpecError error;
    assert(form_spec_parse(edited, &keyed, &error) == FORM_SPEC_OK);
    plan = plan_of(keyed, NULL, true, &result);
    assert(result == PIECE_OK);
    assert(piece_sections(plan, sections) == 4);
    FillTarget moving = {FILL_TARGET_NODE, sections[2]};
    assert(!grow_liquidate_applies(plan, moving));
    plan_destroy(plan);
    form_spec_destroy(keyed);
    free(text);

    /* A motive of 2 bars fits no 4-bar section: the plan is not built. */
    snprintf(path, sizeof(path), "%s/rates/two-bar-3-4.json", COMPOSER_FIXTURE_DIR);
    char *motive_text = test_must_read_file(path, NULL);
    Motive motive;
    assert(motive_text && motive_from_json(motive_text, test_motive_options(), &motive, NULL) == MOTIVE_OK);
    free(motive_text);
    plan = plan_of(form, &motive, false, &result);
    assert(result == PIECE_ERR_MOTIVE_TOO_LONG);
    plan_destroy(plan);
}

int main(void) {
    FormSpec *form = load_form();
    assert(strcmp(form->name, "liquidation-form") == 0 && form->sizes[0].section_count == 4 &&
           form->sizes[0].sections[2].role == SECTION_ROLE_LIQUIDATION && form->sizes[0].sections[2].bars == 4);
    int status = rows_composed(form);
    test_round_trip(form);
    test_applies(form);
    form_spec_destroy(form);
    return status;
}
