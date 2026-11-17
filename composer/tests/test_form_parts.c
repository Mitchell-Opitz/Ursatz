#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "support/test_support.h"
#include "util/plan_util.h"
#include "support/test_composer.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The parts of larger forms (P51, R-503 to R-509), one case per branch. Each composes a 32-bar form built here in the eight
 * rows of key and meter, seeds 1 to 10, and at least 9 of 10 compose in each row.
 *
 * transition (B2, R-504): A theme 8 PAC closed, T transition 8 HC keys end `subordinate`, S subordinate 8 antecedent and
 * consequent PAC in `subordinate`, C codetta 8 presentation and continuation PAC in `subordinate`; region major [5] minor [3].
 * In every composed run T carries the transition function, T's second half is in the region's key (its key lane span starts
 * at T's half), T's last chord is that key's V or V7, and T's continuation is liquidated: its `fragment 2` idea is made by
 * liquidation steps on a segment (1 step or more) and its `cadential` idea by at least as many; the codetta's `fragment 2` has
 * none, so only the transition liquidates. The liquidation is read from the plan, not counted in notes: a residue is regrown to
 * fill its bar (grow-continuation.md), so its span often holds as many notes as fragment 1 or more (R-512).
 *
 * independent (B3, R-504): the same form with T `independent`. In every composed run T's `basic idea` melody differs from A's,
 * the material holds two invented motives (m0 and `independent T`, each of two notes or more), T's motive reference names the
 * second, and the composed plan round-trips through plan JSON byte for byte, as the plan built before it does.
 *
 * retransition (B4, R-505): tests/fixtures/forms/parts-of-forms.json with the coda K replaced by a codetta of 8 (so no coda
 * is grown yet), 48 bars. In every composed run R carries the retransition function and no archetype, is in the home key,
 * its bass is the dominant throughout and its last chord V or V7, its `basic idea` asks for a motive with fewer notes than
 * m0, and its `cadential` melody keeps the intervals of the main theme head (the leading notes of m0) it asks for.
 *
 * coda (B5, R-507): tests/fixtures/forms/parts-of-forms.json as it is, 48 bars. In every composed run K carries the coda
 * function, has three phrases of 4, 2 and 2 bars, each ends on V or V7 in root position to a root-position tonic, the last
 * melody note is the tonic, and the plan round-trips through plan JSON. `grow_coda_applies` is true on K and false for a coda
 * of 4 bars, for a coda that follows another region (a start change) and for a source of 2 bars.
 *
 * With the argument `smoke` each case composes seed 1 in the first row (C major 4/4) alone, which must compose, and every
 * check of a composed piece runs on it, with no rate over the seeds or rows; the checks of `grow_coda_applies` run as they do
 * (B11 of P56 registers it as test_form_parts_smoke).
 */

#define SEEDS 10
#define COMPOSED_LEAST 9

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
} Row;

static const Row ROWS[] = {{"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4}, {"A minor 4/4", 9, KEY_STRUCTURE_MODE_MINOR, 4, 4},
                           {"C major 3/4", 0, KEY_STRUCTURE_MODE_MAJOR, 3, 4}, {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4},
                           {"G major 2/4", 7, KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {"D minor 2/4", 2, KEY_STRUCTURE_MODE_MINOR, 2, 4},
                           {"C major 6/8", 0, KEY_STRUCTURE_MODE_MAJOR, 6, 8}, {"D minor 6/8", 2, KEY_STRUCTURE_MODE_MINOR, 6, 8}};
#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

/* The 32-bar form; the transition's line is the member that differs between cases. */
static const char *const FORM_HEAD =
    "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"form-parts\", \"label\": \"Form parts\", \"modulates\": true,"
    " \"regions\": [{\"name\": \"subordinate\", \"major\": [5], \"minor\": [3]}],"
    " \"sizes\": [{\"bars\": 32, \"default\": true, \"sections\": ["
    "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"closed\": true},";
static const char *const FORM_TAIL =
    ",{\"name\": \"S\", \"letter\": \"S\", \"role\": \"subordinate\", \"bars\": 8, \"beginning\": \"antecedent\","
    " \"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"keys\": {\"start\": \"subordinate\","
    " \"end\": \"subordinate\"}},"
    "{\"name\": \"C\", \"letter\": \"C\", \"role\": \"codetta\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"keys\": {\"start\": \"subordinate\","
    " \"end\": \"subordinate\"}}]}]}";
static const char *const INDEPENDENT =
    "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"transition\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"half\", \"independent\": true, \"keys\": {\"end\": \"subordinate\"}}";
static const char *const TRANSITION =
    "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"transition\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"half\", \"keys\": {\"end\": \"subordinate\"}}";

/* The fixture's text with the coda K replaced by a codetta of 8 (a retransition's case has no coda yet). */
static char *fixture_without_coda(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/parts-of-forms.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    assert(text);
    static const char *const CODA = "\"role\": \"coda\", \"bars\": 8, \"cadence\": \"perfect-authentic\"";
    static const char *const CODETTA = "\"role\": \"codetta\", \"bars\": 8, \"beginning\": \"presentation\", "
                                       "\"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"closed\": true";
    char *at = strstr(text, CODA);
    assert(at);
    size_t size = strlen(text) + strlen(CODETTA) + 1;
    char *out = malloc(size);
    assert(out);
    snprintf(out, size, "%.*s%s%s", (int)(at - text), text, CODETTA, at + strlen(CODA));
    free(text);
    return out;
}

static FormSpec *parse_form(const char *text) {
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "form: %s: %s\n", error.path, error.message);
        exit(2);
    }
    return form;
}

static FormSpec *load_form(const char *transition) {
    size_t size = strlen(FORM_HEAD) + strlen(transition) + strlen(FORM_TAIL) + 1;
    char *text = malloc(size);
    assert(text);
    snprintf(text, size, "%s%s%s", FORM_HEAD, transition, FORM_TAIL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "form: %s: %s\n", error.path, error.message);
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
    request.key = plan_key_of_pitch_class(row->tonic, row->mode);
    request.meter.beats_per_bar = (unsigned)row->beats;
    request.meter.beat_unit = (unsigned)row->unit;
    request.seed = seed;
    request.modulate = true;
    PiecePlan *plan = NULL;
    PieceResult built = form_build_plan(form, &request, &plan);
    if (built != PIECE_OK) {
        fprintf(stderr, "form_build_plan: %s\n", piece_result_name(built));
        exit(2);
    }
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

static Rational add_time(Rational a, Rational b) {
    Rational out;
    assert(rational_add(a, b, &out) == RATIONAL_OK);
    return out;
}

/* The sections of the piece, in order (the material nodes are not sections). */
static size_t piece_sections(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            sections[count++] = child;
        }
    }
    return count;
}

/* The motive reference of the motive node an idea's melody span names (the idea's own made motive), or NULL. */
static const PlanMotiveRef *idea_motive_ref(const PiecePlan *plan, NodeId idea) {
    LaneSpanId melody;
    if (idea == PLAN_NODE_NONE || !plan_melody_span_of(plan, idea, &melody)) {
        return NULL;
    }
    const PlanConstraint *span_ref = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    if (!span_ref) {
        return NULL;
    }
    const PlanConstraint *node_ref = plan_node_find_constraint(plan, span_ref->params.motive.motive, PLAN_CONSTRAINT_MOTIVE_REF);
    return node_ref ? &node_ref->params.motive : NULL;
}

/* The liquidation steps an idea is made with, 0 for an idea that is not liquidated. */
static size_t idea_steps(const PiecePlan *plan, NodeId idea) {
    const PlanMotiveRef *ref = idea_motive_ref(plan, idea);
    return ref ? ref->steps : 0;
}

/* The tonic pitch class of the region the subordinate material is in: the fifth in major, the third (III) in minor. */
static int region_tonic(const Row *row) {
    return (row->tonic + (row->mode == KEY_STRUCTURE_MODE_MAJOR ? 7 : 3)) % 12;
}

/* One composed run of the transition case. */
static void check_transition(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    NodeId t = sections[1];
    PlanSectionFunction function;
    assert(plan_section_function_of(plan, t, &function) && function.function == PLAN_FUNCTION_TRANSITION);
    PlanSpan span = plan_node_span(plan, t);
    Rational bar = {row->beats, row->unit};
    Rational half = {0, 1};
    for (int i = 0; i < 4; i++) {
        half = add_time(half, bar);
    }
    Rational middle = add_time(span.start, half);
    Rational end = add_time(span.start, span.length);

    /* The second half is in the region's key: a key span starts at the half. */
    LaneSpanId keys[8];
    PlanSpan second = {middle, half};
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, second, keys, 8);
    assert(found > 0);
    bool starts_at_half = false;
    for (size_t i = 0; i < found && i < 8; i++) {
        PlanSpan k = plan_lane_span_span(plan, keys[i]);
        if (rational_cmp(k.start, middle) == 0) {
            starts_at_half = true;
            if (analysis_key_tonic_pitch_class(&plan_lane_span_payload(plan, keys[i])->key) != region_tonic(row)) {
                fprintf(stderr, "%s seed %lu: T's second half is not in the region\n", row->name, seed);
                assert(false);
            }
        }
    }
    assert(starts_at_half);

    /* The last chord is the region key's V or V7 (the roman numeral is in that key). */
    LaneSpanId chords[16];
    found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, chords, 16);
    assert(found > 0);
    LaneSpanId last = chords[0];
    for (size_t i = 0; i < found && i < 16; i++) {
        if (rational_cmp(plan_lane_span_span(plan, chords[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = chords[i];
        }
    }
    const char *numeral = plan_lane_span_payload(plan, last)->harmony.roman_numeral;
    assert(rational_cmp(plan_lane_span_span(plan, last).start, end) < 0);
    if (strcmp(numeral, "V") != 0 && strcmp(numeral, "V7") != 0) {
        fprintf(stderr, "%s seed %lu: T ends on %s\n", row->name, seed, numeral);
    }
    assert(strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0);

    /* The liquidation, read from the plan: fragment 2 is a liquidation by steps on a segment, the cadential idea at least as far. */
    NodeId fragment_2 = test_find_named(plan, t, "fragment 2");
    const PlanMotiveRef *residue = idea_motive_ref(plan, fragment_2);
    if (!residue || residue->steps < 1 || residue->segment[0] == '\0' ||
        idea_steps(plan, test_find_named(plan, t, "cadential")) < residue->steps) {
        fprintf(stderr, "%s seed %lu: T's continuation is not liquidated\n", row->name, seed);
    }
    assert(residue && residue->steps >= 1 && residue->segment[0] != '\0');
    assert(idea_steps(plan, test_find_named(plan, t, "cadential")) >= residue->steps);
    /* Only the transition liquidates: the codetta's continuation is the form's usual one. */
    assert(idea_steps(plan, test_find_named(plan, sections[3], "fragment 2")) == 0);
}

/* The pitches of an idea's melody span, in order; the count, 0 when the idea has none. */
static size_t idea_pitches(const PiecePlan *plan, NodeId idea, int *out, size_t room) {
    LaneSpanId melody;
    if (idea == PLAN_NODE_NONE || !plan_melody_span_of(plan, idea, &melody)) {
        return 0;
    }
    const PlanLanePayload *payload = plan_lane_span_payload(plan, melody);
    size_t count = payload->notes.note_count < room ? payload->notes.note_count : room;
    for (size_t i = 0; i < count; i++) {
        out[i] = payload->notes.notes[i].midi;
    }
    return count;
}

/* The motive node of the material with this name, or PLAN_NODE_NONE, and its invented notes (2 or more). */
static bool invented_motive(const PiecePlan *plan, const char *name, NodeId *node) {
    *node = test_find_named(plan, plan_root(plan), name);
    LaneSpanId melody;
    return *node != PLAN_NODE_NONE && plan_node_kind(plan, *node) == PLAN_NODE_MOTIVE && plan_melody_span_of(plan, *node, &melody) &&
           plan_lane_span_payload(plan, melody)->notes.note_count >= 2;
}

static void check_round_trip(const PiecePlan *plan, const Row *row, unsigned long seed) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text);
    PiecePlan *again = NULL;
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    char *second = plan_to_json(again, &error);
    assert(second);
    if (strcmp(text, second) != 0) {
        fprintf(stderr, "%s seed %lu: the plan does not round-trip\n", row->name, seed);
        assert(false);
    }
    free(second);
    free(text);
    plan_destroy(again);
}

/* One composed run of the independent case. */
static void check_independent(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    NodeId own;
    NodeId m0;
    assert(invented_motive(plan, "m0", &m0) && invented_motive(plan, "independent T", &own));

    /* T's section asks for the motive of its own, as it is. */
    const PlanConstraint *ref = plan_node_find_constraint(plan, sections[1], PLAN_CONSTRAINT_MOTIVE_REF);
    assert(ref && ref->params.motive.motive == own && !ref->params.motive.has_step);
    assert(!plan_node_find_constraint(plan, sections[0], PLAN_CONSTRAINT_MOTIVE_REF));

    /* T's basic idea is not A's. */
    int a[64];
    int t[64];
    size_t a_count = idea_pitches(plan, test_find_named(plan, sections[0], "basic idea"), a, 64);
    size_t t_count = idea_pitches(plan, test_find_named(plan, sections[1], "basic idea"), t, 64);
    bool same = a_count == t_count;
    for (size_t i = 0; same && i < a_count; i++) {
        same = a[i] == t[i];
    }
    if (a_count == 0 || t_count == 0 || same) {
        fprintf(stderr, "%s seed %lu: T's basic idea is A's (%zu and %zu notes)\n", row->name, seed, a_count, t_count);
    }
    assert(a_count > 0 && t_count > 0 && !same);
    check_round_trip(plan, row, seed);
}

/* The motive node an idea's melody span asks for (the made motive node the idea's span names), or PLAN_NODE_NONE. */
static NodeId asked_motive(const PiecePlan *plan, NodeId idea) {
    LaneSpanId melody;
    if (idea == PLAN_NODE_NONE || !plan_melody_span_of(plan, idea, &melody)) {
        return PLAN_NODE_NONE;
    }
    const PlanConstraint *span_ref = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    return span_ref ? span_ref->params.motive.motive : PLAN_NODE_NONE;
}

/* The notes of a motive node's own melody span, 0 when it has none. */
static size_t node_pitches(const PiecePlan *plan, NodeId node, int *out, size_t room) {
    LaneSpanId melody;
    if (node == PLAN_NODE_NONE || !plan_melody_span_of(plan, node, &melody)) {
        return 0;
    }
    const PlanLanePayload *payload = plan_lane_span_payload(plan, melody);
    size_t count = payload->notes.note_count < room ? payload->notes.note_count : room;
    for (size_t i = 0; i < count; i++) {
        out[i] = payload->notes.notes[i].midi;
    }
    return count;
}

static bool is_dominant(const char *numeral) {
    return strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0 || strcmp(numeral, "V6") == 0 ||
           strcmp(numeral, "V65") == 0 || strcmp(numeral, "V6/5") == 0 || strcmp(numeral, "V4/3") == 0 ||
           strcmp(numeral, "V43") == 0 || strcmp(numeral, "V4/2") == 0 || strcmp(numeral, "V42") == 0;
}

/* One composed run of the retransition case: R is sections[4] of A, T, S, C, R, A return, K. */
static void check_retransition(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 7);
    NodeId r = sections[4];
    PlanSectionFunction function;
    assert(plan_section_function_of(plan, r, &function) && function.function == PLAN_FUNCTION_RETRANSITION);
    assert(!plan_node_find_constraint(plan, r, PLAN_CONSTRAINT_ARCHETYPE));
    PlanSpan span = plan_node_span(plan, r);

    /* In the home key, from its first bar: every key span that touches R is the home key's. */
    LaneSpanId ids[32];
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, span, ids, 32);
    assert(found > 0 && found <= 32);
    for (size_t i = 0; i < found; i++) {
        if (analysis_key_tonic_pitch_class(&plan_lane_span_payload(plan, ids[i])->key) != row->tonic) {
            fprintf(stderr, "%s seed %lu: R is not in the home key\n", row->name, seed);
            assert(false);
        }
    }

    /* The bass is the dominant throughout and the last chord is V or V7. */
    int dominant = (row->tonic + 7) % 12;
    found = plan_lane_find(plan, PLAN_LANE_BASS, 0, span, ids, 32);
    assert(found > 0 && found <= 32);
    for (size_t i = 0; i < found; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            if (((notes->notes[n].midi % 12) + 12) % 12 != dominant) {
                fprintf(stderr, "%s seed %lu: R's bass leaves the dominant\n", row->name, seed);
                assert(false);
            }
        }
    }
    found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 32);
    assert(found > 0 && found <= 32);
    LaneSpanId last = ids[0];
    for (size_t i = 0; i < found; i++) {
        assert(is_dominant(plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral));
        if (rational_cmp(plan_lane_span_span(plan, ids[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = ids[i];
        }
    }
    const char *numeral = plan_lane_span_payload(plan, last)->harmony.roman_numeral;
    assert(strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0);

    /* The basic idea asks for a residue with fewer notes than m0. */
    NodeId m0;
    assert(invented_motive(plan, "m0", &m0));
    int notes[64];
    size_t m0_count = node_pitches(plan, m0, notes, 64);
    int residue[64];
    NodeId basic = test_find_named(plan, r, "basic idea");
    size_t residue_count = node_pitches(plan, asked_motive(plan, basic), residue, 64);
    if (residue_count == 0 || residue_count >= m0_count) {
        fprintf(stderr, "%s seed %lu: R's basic idea asks for %zu notes, m0 has %zu\n", row->name, seed, residue_count, m0_count);
    }
    assert(residue_count > 0 && residue_count < m0_count);

    /* The cadential idea is the main theme head: the head motive it asks for is m0's first notes, and the melody keeps its
       intervals. */
    NodeId cadential = test_find_named(plan, r, "cadential");
    int head[64];
    size_t head_count = node_pitches(plan, asked_motive(plan, cadential), head, 64);
    int heard[64];
    size_t heard_count = idea_pitches(plan, cadential, heard, 64);
    assert(head_count >= 2 && head_count <= 3 && head_count <= m0_count && heard_count >= head_count);
    for (size_t i = 1; i < head_count; i++) {
        if (head[i] - head[i - 1] != notes[i] - notes[i - 1] || heard[i] - heard[i - 1] != notes[i] - notes[i - 1]) {
            fprintf(stderr, "%s seed %lu: R's cadential idea leaves m0's head (interval %zu)\n", row->name, seed, i);
            assert(false);
        }
    }
}

/* The harmony spans over [span], in time order. */
static size_t harmony_in(const PiecePlan *plan, PlanSpan span, LaneSpanId *out, size_t room) {
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, out, room);
    assert(found <= room);
    for (size_t i = 1; i < found; i++) {
        for (size_t j = i; j > 0 && rational_cmp(plan_lane_span_span(plan, out[j]).start, plan_lane_span_span(plan, out[j - 1]).start) < 0; j--) {
            LaneSpanId swap = out[j];
            out[j] = out[j - 1];
            out[j - 1] = swap;
        }
    }
    return found;
}

/* A numeral in root position: no figure after the letters (V, V7, I, i; not V6 or V65). */
static bool root_position(const char *numeral, bool seventh_allowed) {
    size_t letters = 0;
    while (numeral[letters] == 'I' || numeral[letters] == 'V' || numeral[letters] == 'i' || numeral[letters] == 'v') {
        letters++;
    }
    return letters > 0 && (numeral[letters] == '\0' || (seventh_allowed && strcmp(numeral + letters, "7") == 0));
}

/* The lowest pitch class of the bass over `span`'s last chord: the bass span that starts latest within it. */
static int bass_pitch_class_at_end(const PiecePlan *plan, PlanSpan span) {
    LaneSpanId ids[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, span, ids, 64);
    assert(found > 0 && found <= 64);
    LaneSpanId last = ids[0];
    for (size_t i = 1; i < found; i++) {
        if (rational_cmp(plan_lane_span_span(plan, ids[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = ids[i];
        }
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, last)->notes;
    assert(notes->note_count > 0);
    return ((notes->notes[notes->note_count - 1].midi % 12) + 12) % 12;
}

/* One composed run of the coda case: K is sections[6] of A, T, S, C, R, A return, K. */
static void check_coda(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 7);
    NodeId k = sections[6];
    PlanSectionFunction function;
    assert(plan_section_function_of(plan, k, &function) && function.function == PLAN_FUNCTION_CODA);
    assert(!plan_node_find_constraint(plan, k, PLAN_CONSTRAINT_ARCHETYPE));

    /* Three phrases of 4, 2 and 2 bars. */
    PlanSpan whole = plan_node_span(plan, k);
    Rational bar;
    assert(rational_div(whole.length, (Rational){8, 1}, &bar) == RATIONAL_OK);
    static const long BARS[3] = {4, 2, 2};
    assert(plan_node_child_count(plan, k) == 3);
    Rational expected = whole.start;
    for (size_t p = 0; p < 3; p++) {
        NodeId phrase = plan_node_child_at(plan, k, p);
        PlanSpan span = plan_node_span(plan, phrase);
        Rational length;
        assert(rational_mul_int(bar, BARS[p], &length) == RATIONAL_OK);
        if (rational_cmp(span.start, expected) != 0 || rational_cmp(span.length, length) != 0) {
            fprintf(stderr, "%s seed %lu: K's phrase %zu is not %ld bars from bar %zu\n", row->name, seed, p + 1, BARS[p], p);
            assert(false);
        }
        expected = add_time(span.start, span.length);

        /* Each ends on V or V7 to I, all in root position, with the bass on the dominant and then the tonic. */
        LaneSpanId chords[64];
        size_t count = harmony_in(plan, span, chords, 64);
        assert(count >= 2);
        const char *last = plan_lane_span_payload(plan, chords[count - 1])->harmony.roman_numeral;
        const char *before = plan_lane_span_payload(plan, chords[count - 2])->harmony.roman_numeral;
        if (!root_position(last, false) || (last[0] != 'I' && last[0] != 'i') || !root_position(before, true) || before[0] != 'V') {
            fprintf(stderr, "%s seed %lu: K's phrase %zu ends %s to %s\n", row->name, seed, p + 1, before, last);
            assert(false);
        }
        assert(bass_pitch_class_at_end(plan, span) == row->tonic);
    }

    /* The last melody note is the tonic. */
    LaneSpanId ids[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, whole, ids, 64);
    assert(found > 0 && found <= 64);
    LaneSpanId last = ids[0];
    for (size_t i = 1; i < found; i++) {
        if (rational_cmp(plan_lane_span_span(plan, ids[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = ids[i];
        }
    }
    const PlanNotes *notes = &plan_lane_span_payload(plan, last)->notes;
    assert(notes->note_count > 0);
    if (((notes->notes[notes->note_count - 1].midi % 12) + 12) % 12 != row->tonic) {
        fprintf(stderr, "%s seed %lu: K's last melody note is not the tonic\n", row->name, seed);
        assert(false);
    }
    check_round_trip(plan, row, seed);
}

static PiecePlan *plan_only(const FormSpec *form, const Motive *motive, PieceResult *result) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = motive ? 3 : 4;
    request.meter.beat_unit = 4;
    request.seed = 1;
    request.motive = motive;
    request.modulate = true;
    PiecePlan *plan = NULL;
    *result = form_build_plan(form, &request, &plan);
    return plan;
}

static const char *const CODA_AFTER_REGION =
    "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"coda-after-region\", \"label\": \"Coda after a region\","
    " \"modulates\": true, \"regions\": [{\"name\": \"subordinate\", \"major\": [5], \"minor\": [3]}],"
    " \"sizes\": [{\"bars\": 32, \"default\": true, \"sections\": ["
    "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"closed\": true},"
    "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"transition\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"half\", \"keys\": {\"end\": \"subordinate\"}},"
    "{\"name\": \"S\", \"letter\": \"S\", \"role\": \"subordinate\", \"bars\": 8, \"beginning\": \"antecedent\","
    " \"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"keys\": {\"start\": \"subordinate\","
    " \"end\": \"subordinate\"}},"
    "{\"name\": \"K\", \"letter\": \"K\", \"role\": \"coda\", \"bars\": 8, \"cadence\": \"perfect-authentic\"}]}]}";

/* A theme and a coda, every section 8 bars: a motive of 2 bars fits it. */
static const char *const THEME_AND_CODA =
    "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"theme-and-coda\", \"label\": \"Theme and coda\","
    " \"sizes\": [{\"bars\": 16, \"default\": true, \"sections\": ["
    "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"closed\": true},"
    "{\"name\": \"K\", \"letter\": \"K\", \"role\": \"coda\", \"bars\": 8, \"cadence\": \"perfect-authentic\"}]}]}";

/*
 * grow_coda_applies is true on the fixture's K and false off its cases: a section that is no coda, a coda of 4 bars (the
 * same section on a node of half the length; the form reader refuses it, so the plan is edited), a coda after another
 * region (its key lane then starts a change) and a source of 2 bars.
 */
static void test_applies(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/parts-of-forms.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    assert(text);
    FormSpec *form = parse_form(text);
    free(text);
    PieceResult result;
    PiecePlan *plan = plan_only(form, NULL, &result);
    assert(result == PIECE_OK);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 7);
    FillTarget coda = {FILL_TARGET_NODE, sections[6]};
    assert(grow_coda_applies(plan, coda));
    FillTarget other = {FILL_TARGET_NODE, sections[1]};
    assert(!grow_coda_applies(plan, other));
    FillTarget retransition = {FILL_TARGET_NODE, sections[4]};
    assert(!grow_coda_applies(plan, retransition));

    /* The same section as a node of 8 bars (the control) and of 4. */
    PlanSpan span = plan_node_span(plan, sections[6]);
    PlanConstraint constraints[16];
    size_t count = plan_node_constraint_count(plan, sections[6]);
    assert(count > 0 && count <= 16);
    for (size_t i = 0; i < count; i++) {
        constraints[i] = *plan_node_constraint_at(plan, sections[6], i);
    }
    assert(plan_remove_subtree(plan, sections[6]) == PLAN_OK);
    for (int bars = 8; bars >= 4; bars -= 4) {
        PlanSpan edited = span;
        Rational length = {0, 1};
        assert(rational_mul_int(span.length, bars, &length) == RATIONAL_OK);
        assert(rational_div(length, (Rational){8, 1}, &edited.length) == RATIONAL_OK);
        NodeId node;
        assert(plan_add_node(plan, plan_root(plan), PLAN_NODE_SECTION, edited, "K", &node) == PLAN_OK);
        for (size_t i = 0; i < count; i++) {
            assert(plan_node_add_constraint(plan, node, &constraints[i]) == PLAN_OK);
        }
        assert(plan_node_set_state(plan, node, PLAN_STATE_CONSTRAINED) == PLAN_OK);
        FillTarget edited_target = {FILL_TARGET_NODE, node};
        if (bars == 8) {
            assert(grow_coda_applies(plan, edited_target));
            assert(plan_remove_subtree(plan, node) == PLAN_OK);
        } else {
            assert(!grow_coda_applies(plan, edited_target));
        }
    }
    plan_destroy(plan);

    /* A coda after the subordinate region opens its home key from there: a start change. */
    FormSpec *after = parse_form(CODA_AFTER_REGION);
    plan = plan_only(after, NULL, &result);
    assert(result == PIECE_OK);
    assert(piece_sections(plan, sections) == 4);
    FillTarget moving = {FILL_TARGET_NODE, sections[3]};
    assert(!grow_coda_applies(plan, moving));
    plan_destroy(plan);
    form_spec_destroy(after);

    /* A source of 2 bars (3/4): the plan is built, the coda is not grown. */
    snprintf(path, sizeof(path), "%s/rates/two-bar-3-4.json", COMPOSER_FIXTURE_DIR);
    char *motive_text = test_must_read_file(path, NULL);
    Motive motive;
    assert(motive_text && motive_from_json(motive_text, NULL, &motive, NULL) == MOTIVE_OK);
    free(motive_text);
    FormSpec *plain = parse_form(THEME_AND_CODA);
    plan = plan_only(plain, NULL, &result);
    assert(result == PIECE_OK && piece_sections(plan, sections) == 2);
    FillTarget plain_coda = {FILL_TARGET_NODE, sections[1]};
    assert(grow_coda_applies(plan, plain_coda));
    plan_destroy(plan);
    plan = plan_only(plain, &motive, &result);
    assert(result == PIECE_OK);
    assert(piece_sections(plan, sections) == 2);
    FillTarget long_source = {FILL_TARGET_NODE, sections[1]};
    assert(!grow_coda_applies(plan, long_source));
    plan_destroy(plan);
    form_spec_destroy(plain);
    form_spec_destroy(form);
}

static int run_rows(const char *label, const FormSpec *form, void (*check)(const PiecePlan *, const Row *, unsigned long),
                    bool smoke) {
    int status = 0;
    const int seeds = smoke ? 1 : SEEDS;
    for (size_t r = 0; r < (smoke ? 1 : ROW_COUNT); r++) {
        int composed = 0;
        for (unsigned long seed = 1; seed <= (unsigned long)seeds; seed++) {
            ComposeOutcome outcome;
            PieceResult ran;
            PiecePlan *plan = compose(form, &ROWS[r], seed, &outcome, &ran);
            if (ran == PIECE_OK) {
                check(plan, &ROWS[r], seed);
                composed++;
            } else {
                printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("%s in %s: %d of %d seeds compose\n", label, ROWS[r].name, composed, seeds);
        if (smoke) {
            /* Seed 1 alone has no rate to hold, but it must compose, so that the checks of a composed piece ran. */
            if (composed < 1) {
                fprintf(stderr, "%s in %s: seed 1 should compose\n", label, ROWS[r].name);
                status = 1;
            }
        } else if (composed < COMPOSED_LEAST) {
            fprintf(stderr, "%s in %s: %d composed, at least %d expected\n", label, ROWS[r].name, composed, COMPOSED_LEAST);
            status = 1;
        }
    }
    return status;
}

int main(int argc, char **argv) {
    const bool smoke = argc == 2 && strcmp(argv[1], "smoke") == 0;
    if (argc > 2 || (argc == 2 && !smoke)) {
        fprintf(stderr, "usage: test_form_parts [smoke]\n");
        return 2;
    }
    FormSpec *form = load_form(TRANSITION);
    int status = run_rows("transition", form, check_transition, smoke);
    form_spec_destroy(form);
    form = load_form(INDEPENDENT);
    status |= run_rows("independent", form, check_independent, smoke);
    form_spec_destroy(form);
    char *fixture = fixture_without_coda();
    form = parse_form(fixture);
    free(fixture);
    status |= run_rows("retransition", form, check_retransition, smoke);
    form_spec_destroy(form);
    char *with_coda = NULL;
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/forms/parts-of-forms.json", COMPOSER_FIXTURE_DIR);
        with_coda = test_must_read_file(path, NULL);
        assert(with_coda);
    }
    form = parse_form(with_coda);
    free(with_coda);
    status |= run_rows("coda", form, check_coda, smoke);
    form_spec_destroy(form);
    test_applies();
    if (status == 0) {
        printf(smoke ? "test_form_parts smoke: ok\n" : "test_form_parts: ok\n");
    }
    return status;
}
