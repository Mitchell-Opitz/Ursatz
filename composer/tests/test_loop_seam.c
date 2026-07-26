#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/piece_structure.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/seam_hear.h>

/*
 * The loop seam (R-171).
 *
 *   rules   the four seam rules on hand-made plans: V to I and I to IV held, IV to I not; parallel octaves across the
 *           seam; a leap of a ninth; a leading tone that goes down to 6; and the plans that are not seams
 *   sweep   the B3 structure (intro 2 bars, body 8 bars and 6 beats, turnaround 1 bar, 4/4, C major), seeds 1 to 20:
 *           the seam section equals the body's first bar, nothing renders past the loop's end, the held loop end is
 *           written and still held, and the seam rules are reported. The share of runs that hold all four rules is
 *           measured and asserted against the baseline recorded below.
 */

#define SEEDS 20
/* The runs of the seeds above that hold all four rules, as measured when the seam was added (docs/loop-seam.md). */
#define BASELINE_ALL_HELD 11
/* Of the 20 seeds, those that must complete: the melody of a phrase that ends on V is hard to write (B4). */
#define BASELINE_COMPLETED 18

/* ------------------------------------------------------------- the rules */

static PlanTime whole(long num, unsigned long den) {
    PlanTime time;
    assert(plan_time_make(num, den, &time) == PLAN_OK);
    return time;
}

/* A lane span of notes, evenly spaced over a bar that starts at `bar`. */
static void add_notes(PiecePlan *plan, NodeId owner, PlanLaneKind lane, long bar, const int *pitches, size_t count) {
    PlanNote notes[8];
    PlanLanePayload payload;
    LaneSpanId id;
    assert(count > 0 && count <= 8);
    for (size_t i = 0; i < count; i++) {
        notes[i].midi_pitch = pitches[i];
        notes[i].offset = whole((long)i, (unsigned long)count);
        notes[i].duration = whole(1, (unsigned long)count);
    }
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = notes;
    payload.notes.note_count = count;
    assert(plan_add_lane_span(plan, owner, lane, 0, (PlanSpan){whole(bar, 1), whole(1, 1)}, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
}

static void add_chord(PiecePlan *plan, NodeId owner, long bar, const char *roman) {
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    snprintf(payload.harmony.roman_numeral, sizeof(payload.harmony.roman_numeral), "%s", roman);
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, (PlanSpan){whole(bar, 1), whole(1, 1)}, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
}

typedef struct {
    const char *before;
    const char *after;
    int bass_before;
    int bass_after;
    const int *melody_before; /* the notes of the bar before the seam */
    size_t melody_before_count;
    const int *melody_after; /* the notes of the seam's bar */
    size_t melody_after_count;
} SeamCase;

/* A plan of a bar before the seam and the seam, with a chord, a bass note and melody in each. */
static SeamHeard hear_case(const SeamCase *c) {
    PiecePlan *plan = plan_create();
    NodeId form;
    NodeId body;
    NodeId seam;
    PlanLanePayload payload;
    LaneSpanId id;
    PlanConstraint mark = {PLAN_CONSTRAINT_SEAM, {0}};
    SeamHeard heard;
    assert(plan);
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, (PlanSpan){whole(0, 1), whole(3, 1)}, "form", &form) ==
           PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, (PlanSpan){whole(0, 1), whole(1, 1)}, "body", &body) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, (PlanSpan){whole(1, 1), whole(1, 1)}, "seam", &seam) == PLAN_OK);
    assert(plan_node_add_constraint(plan, seam, &mark) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_METER, 0, (PlanSpan){whole(0, 1), whole(3, 1)}, &payload, &id) ==
           PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key.tonic_pitch_class = 0;
    payload.key.mode = PLAN_MODE_MAJOR;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, (PlanSpan){whole(0, 1), whole(3, 1)}, &payload, &id) ==
           PLAN_OK);
    add_chord(plan, body, 0, c->before);
    add_chord(plan, seam, 1, c->after);
    add_notes(plan, body, PLAN_LANE_BASS, 0, &c->bass_before, 1);
    add_notes(plan, seam, PLAN_LANE_BASS, 1, &c->bass_after, 1);
    add_notes(plan, body, PLAN_LANE_MELODY, 0, c->melody_before, c->melody_before_count);
    add_notes(plan, seam, PLAN_LANE_MELODY, 1, c->melody_after, c->melody_after_count);
    assert(seam_hear(plan, body, &heard) == SEAM_HEAR_ERR_NOT_SEAM);
    assert(seam_hear(plan, seam, &heard) == SEAM_HEAR_OK);
    plan_destroy(plan);
    return heard;
}

/* The melody 67, 71 to 72, 76 over a bass G to C: nothing breaks a rule but what the chords decide. */
static SeamCase calm(const char *before, const char *after) {
    static const int BEFORE[] = {67, 71};
    static const int AFTER[] = {72, 76};
    SeamCase c = {before, after, 43, 48, BEFORE, 2, AFTER, 2};
    return c;
}

static void test_rules(void) {
    /* The progression: V to I and I to IV are held, IV to I is not; a repeated tonic is. */
    SeamCase c = calm("V", "I");
    SeamHeard heard = hear_case(&c);
    assert(heard.progression && heard.no_parallels && heard.step && heard.leading_tone && heard.held == SEAM_RULE_COUNT);
    c = calm("I", "IV");
    assert(hear_case(&c).progression);
    c = calm("IV", "I");
    heard = hear_case(&c);
    assert(!heard.progression && heard.held == SEAM_RULE_COUNT - 1);
    c = calm("I", "I");
    assert(hear_case(&c).progression);
    c = calm("ii", "V");
    assert(hear_case(&c).progression);
    c = calm("V", "IV");
    assert(!hear_case(&c).progression);

    /* Parallel octaves across the seam: bass C to D and melody C to D. */
    static const int OCTAVE_BEFORE[] = {60};
    static const int OCTAVE_AFTER[] = {62};
    SeamCase parallel = {"I", "ii", 48, 50, OCTAVE_BEFORE, 1, OCTAVE_AFTER, 1};
    assert(!hear_case(&parallel).no_parallels);
    /* Parallel fifths: bass C to D, melody G to A. */
    static const int FIFTH_BEFORE[] = {67};
    static const int FIFTH_AFTER[] = {69};
    SeamCase fifths = {"I", "ii", 48, 50, FIFTH_BEFORE, 1, FIFTH_AFTER, 1};
    assert(!hear_case(&fifths).no_parallels);
    /* Thirds above the bass are not perfect. */
    static const int THIRD_BEFORE[] = {64};
    static const int THIRD_AFTER[] = {65};
    SeamCase thirds = {"I", "ii", 48, 50, THIRD_BEFORE, 1, THIRD_AFTER, 1};
    assert(hear_case(&thirds).no_parallels);
    /* The same octave kept by one voice standing still is not parallel motion. */
    SeamCase still = {"I", "I", 48, 48, OCTAVE_BEFORE, 1, OCTAVE_BEFORE, 1};
    assert(hear_case(&still).no_parallels);

    /* A leap of a ninth is too far; a fourth is not; a sixth is held when a step the other way follows. */
    static const int NINTH_BEFORE[] = {60};
    static const int NINTH_AFTER[] = {74};
    SeamCase ninth = {"I", "IV", 48, 53, NINTH_BEFORE, 1, NINTH_AFTER, 1};
    assert(!hear_case(&ninth).step);
    static const int FOURTH_AFTER[] = {65};
    SeamCase fourth = {"I", "IV", 48, 53, NINTH_BEFORE, 1, FOURTH_AFTER, 1};
    assert(hear_case(&fourth).step);
    static const int SIXTH_AFTER_STEPPED[] = {69, 67};
    SeamCase sixth = {"I", "IV", 48, 53, NINTH_BEFORE, 1, SIXTH_AFTER_STEPPED, 2};
    assert(hear_case(&sixth).step);
    static const int SIXTH_AFTER_LEAP[] = {69, 72};
    SeamCase unanswered = {"I", "IV", 48, 53, NINTH_BEFORE, 1, SIXTH_AFTER_LEAP, 2};
    assert(!hear_case(&unanswered).step);

    /* The leading tone goes up to the tonic, not down to 6; a melody that is not on degree 7 is not asked. */
    static const int LEADING_BEFORE[] = {71};
    static const int DOWN_AFTER[] = {69};
    static const int UP_AFTER[] = {72};
    SeamCase down = {"V", "I", 43, 48, LEADING_BEFORE, 1, DOWN_AFTER, 1};
    assert(!hear_case(&down).leading_tone);
    SeamCase up = {"V", "I", 43, 48, LEADING_BEFORE, 1, UP_AFTER, 1};
    assert(hear_case(&up).leading_tone);
    static const int OTHER_BEFORE[] = {67};
    static const int OTHER_AFTER[] = {65};
    SeamCase other = {"V", "I", 43, 48, OTHER_BEFORE, 1, OTHER_AFTER, 1};
    assert(hear_case(&other).leading_tone);

    /* A plan with no chord before the seam is not heard. */
    PiecePlan *plan = plan_create();
    NodeId form;
    NodeId seam;
    PlanConstraint mark = {PLAN_CONSTRAINT_SEAM, {0}};
    PlanLanePayload payload;
    LaneSpanId id;
    assert(plan);
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, (PlanSpan){whole(0, 1), whole(2, 1)}, "form", &form) ==
           PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, (PlanSpan){whole(1, 1), whole(1, 1)}, "seam", &seam) == PLAN_OK);
    assert(plan_node_add_constraint(plan, seam, &mark) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key.mode = PLAN_MODE_MAJOR;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, (PlanSpan){whole(0, 1), whole(2, 1)}, &payload, &id) ==
           PLAN_OK);
    assert(seam_hear(plan, seam, &heard) == SEAM_HEAR_ERR_NOT_WRITTEN);
    assert(seam_hear(NULL, seam, &heard) == SEAM_HEAR_ERR_NULL);
    plan_destroy(plan);
}

/* ------------------------------------------------------------- the sweep */

typedef struct {
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 2];
    PieceStructure structure;
} Fixture;

static PieceSection *add_section(Fixture *fixture, const char *id, const char *letter, PiecePart part, size_t beats,
                                 PieceFillKind fill) {
    PieceSection *section = &fixture->sections[fixture->structure.section_count++];
    memset(section, 0, sizeof(*section));
    snprintf(section->id, sizeof(section->id), "%s", id);
    snprintf(section->letter, sizeof(section->letter), "%s", letter);
    section->part = part;
    section->beats = beats;
    section->pickup = (PlanTime){0, 1};
    section->fill = fill;
    return section;
}

/* The intro 2 bars, the body 8 bars motif sentence plus 6 beats empty, the turnaround 1 bar (all 4/4). */
static void standard_structure(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->structure.key.tonic_pitch_class = 0;
    fixture->structure.key.mode = PLAN_MODE_MAJOR;
    fixture->structure.meter.beats_per_bar = 4;
    fixture->structure.meter.beat_unit = 4;
    fixture->structure.sections = fixture->sections;
    add_section(fixture, "intro", "I", PIECE_PART_INTRO, 8, PIECE_FILL_EMPTY);
    PieceSection *a = add_section(fixture, "a", "A", PIECE_PART_BODY, 32, PIECE_FILL_MOTIF);
    a->has_into = true;
    a->into = PLAN_ARCHETYPE_SENTENCE;
    add_section(fixture, "b", "B", PIECE_PART_BODY, 6, PIECE_FILL_EMPTY);
    add_section(fixture, "t", "T", PIECE_PART_TURNAROUND, 4, PIECE_FILL_EMPTY);
}

static const PlanConstraint *node_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        if (plan_node_constraint_at(plan, id, i)->kind == kind) {
            return plan_node_constraint_at(plan, id, i);
        }
    }
    return NULL;
}

/* The first or the last section of a part of the piece structure. */
static NodeId part_section(const PiecePlan *plan, int part, bool last) {
    NodeId found = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const PlanConstraint *constraint = node_constraint(plan, id, PLAN_CONSTRAINT_PIECE_PART);
        if (constraint && constraint->params.piece_part.part == part && (last || found == PLAN_NODE_NONE)) {
            found = id;
        }
    }
    return found;
}

/* What sounds in a lane inside a window: the onset from the window's start, and the pitch (a hash of the numeral for a
   chord). */
typedef struct {
    PlanTime onset;
    int pitch;
} Sound;

#define MAX_SOUNDS 64

static PlanTime minus(PlanTime a, PlanTime b) {
    PlanTime out;
    assert(plan_time_make(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den, &out) == PLAN_OK);
    return out;
}

static size_t sounds(const PiecePlan *plan, PlanLaneKind lane, PlanSpan window, Sound *out) {
    LaneSpanId ids[32];
    size_t found = plan_lane_find(plan, lane, 0, window, ids, 32);
    size_t count = 0;
    PlanTime window_end;
    assert(found <= 32 && plan_span_end(window, &window_end) == PLAN_OK);
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            continue;
        }
        if (lane == PLAN_LANE_HARMONY) {
            PlanTime start = plan_time_compare(span.start, window.start) > 0 ? span.start : window.start;
            const char *numeral = payload->harmony.roman_numeral;
            int hash = 0;
            for (size_t c = 0; numeral[c]; c++) {
                hash = hash * 31 + numeral[c];
            }
            assert(count < MAX_SOUNDS);
            out[count].onset = minus(start, window.start);
            out[count++].pitch = hash;
            continue;
        }
        for (size_t n = 0; n < payload->notes.note_count; n++) {
            PlanTime onset;
            assert(plan_time_add(span.start, payload->notes.notes[n].offset, &onset) == PLAN_OK);
            if (plan_time_compare(onset, window.start) < 0 || plan_time_compare(onset, window_end) >= 0) {
                continue;
            }
            assert(count < MAX_SOUNDS);
            out[count].onset = minus(onset, window.start);
            out[count++].pitch = payload->notes.notes[n].midi_pitch;
        }
    }
    return count;
}

static bool lane_written(const PiecePlan *plan, PlanLaneKind lane, PlanSpan window) {
    LaneSpanId ids[128];
    size_t found = plan_lane_find(plan, lane, 0, window, ids, 128);
    if (found == 0 || found > 128) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
    }
    return true;
}

/* The seam is the body's first bar, lane by lane, Given. */
static void check_copy(const PiecePlan *plan, NodeId seam) {
    static const PlanLaneKind LANES[3] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    PlanSpan seam_span = plan_node_span(plan, seam);
    PlanSpan first = {plan_node_span(plan, part_section(plan, PIECE_PART_BODY, false)).start, seam_span.length};
    for (size_t lane = 0; lane < 3; lane++) {
        Sound body[MAX_SOUNDS];
        Sound copy[MAX_SOUNDS];
        size_t body_count = sounds(plan, LANES[lane], first, body);
        size_t copy_count = sounds(plan, LANES[lane], seam_span, copy);
        assert(body_count > 0 && body_count == copy_count);
        for (size_t i = 0; i < body_count; i++) {
            assert(plan_time_compare(body[i].onset, copy[i].onset) == 0 && body[i].pitch == copy[i].pitch);
        }
    }
    LaneSpanId owned[64];
    size_t count = plan_node_lane_spans(plan, seam, owned, 64);
    assert(count > 0 && count <= 64);
    for (size_t i = 0; i < count; i++) {
        assert(plan_lane_span_state(plan, owned[i]) == PLAN_STATE_GIVEN);
    }
}

/* Nothing sounds past the loop's end: the piece ends there, the seam is skipped and has no marker. */
static void check_render(const PiecePlan *plan, NodeId seam, PlanTime loop_end) {
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    assert(plan_time_compare(rendered.piece_end, loop_end) == 0);
    for (size_t i = 0; i < rendered.count; i++) {
        PlanSpan span = plan_lane_span_span(plan, rendered.spans[i]);
        assert(plan_lane_span_owner(plan, rendered.spans[i]) != seam && plan_time_compare(span.start, loop_end) < 0);
    }
    size_t skipped = 0;
    for (size_t i = 0; i < rendered.skipped_count; i++) {
        skipped += rendered.skipped[i].reason == RENDER_SKIP_SEAM;
    }
    assert(skipped > 0);
    for (size_t i = 0; i < rendered.section_count; i++) {
        assert(strcmp(rendered.sections[i].label, "seam") != 0);
    }
    rendered_plan_destroy(&rendered);
}

static void test_sweep(void) {
    size_t completed = 0;
    size_t all_held = 0;
    size_t held_by_rule[SEAM_RULE_COUNT] = {0};
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Fixture fixture;
        PiecePlan *plan;
        ComposeConfig config;
        ComposeOutcome outcome;
        standard_structure(&fixture);
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        if (compose_run(plan, &config, &outcome) != PIECE_OK) {
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
            continue;
        }
        completed++;
        NodeId seam = PLAN_NODE_NONE;
        for (NodeId id = 1; id <= plan_node_count(plan); id++) {
            if (node_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
                seam = id;
            }
        }
        NodeId loop_end = part_section(plan, PIECE_PART_TURNAROUND, true);
        assert(seam != PLAN_NODE_NONE && loop_end != PLAN_NODE_NONE);
        PlanSpan end_span = plan_node_span(plan, loop_end);
        PlanTime end;
        assert(plan_span_end(end_span, &end) == PLAN_OK);
        /* The seam follows the loop's end, one bar long, and is the first bar of the body. */
        assert(plan_time_compare(plan_node_span(plan, seam).start, end) == 0);
        assert(plan_time_compare(plan_node_span(plan, seam).length, whole(1, 1)) == 0);
        check_copy(plan, seam);
        check_render(plan, seam, end);
        /* The held loop end is written, and is still held. */
        assert(node_constraint(plan, loop_end, PLAN_CONSTRAINT_HOLD));
        assert(plan_node_state(plan, loop_end) != PLAN_STATE_CONSTRAINED);
        assert(lane_written(plan, PLAN_LANE_HARMONY, end_span) && lane_written(plan, PLAN_LANE_BASS, end_span) &&
               lane_written(plan, PLAN_LANE_MELODY, end_span));
        assert(outcome.records[COMPOSE_STAGE_SEAM] != NULL);
        /* The seam rules are reported, and are the ones the finished plan has. */
        SeamHeard heard;
        assert(outcome.seam_planned && seam_hear(plan, seam, &heard) == SEAM_HEAR_OK);
        assert(heard.held == (size_t)heard.progression + (size_t)heard.no_parallels + (size_t)heard.step +
                                 (size_t)heard.leading_tone);
        assert(outcome.seam.held == heard.held && outcome.seam.progression == heard.progression);
        assert(outcome.score.numerator <= outcome.score.denominator && outcome.score.denominator >= SEAM_RULE_COUNT);
        held_by_rule[0] += heard.progression;
        held_by_rule[1] += heard.no_parallels;
        held_by_rule[2] += heard.step;
        held_by_rule[3] += heard.leading_tone;
        all_held += heard.held == SEAM_RULE_COUNT;
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("composed %zu of %d; all four seam rules held in %zu (progression %zu, no parallels %zu, step %zu, leading "
           "tone %zu)\n",
           completed, SEEDS, all_held, held_by_rule[0], held_by_rule[1], held_by_rule[2], held_by_rule[3]);
    assert(completed >= BASELINE_COMPLETED);
    assert(all_held >= BASELINE_ALL_HELD);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "rules") == 0) {
        test_rules();
    } else if (strcmp(mode, "sweep") == 0) {
        test_sweep();
    } else {
        fprintf(stderr, "usage: test_loop_seam rules|sweep\n");
        return 2;
    }
    printf("test_loop_seam %s: ok\n", mode);
    return 0;
}
