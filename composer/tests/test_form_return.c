#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>

/*
 * Returning sections (form-return, form_return.h): rounded binary, ternary, layered and variations, C major 4/4 with an
 * invented motive, seeds 1 to 5. A section that returns an earlier one is given its material instead of being grown
 * afresh.
 *
 *   smoke        rounded binary 24 bars, seed 1: A' is A, shifted (quick)
 *   exact        rounded binary (also with an accompaniment), ternary and layered: every return has the nodes and the
 *                harmony, bass, melody and inner voices of the section it returns, shifted by the time between them
 *   ornament     variations (3): variations 1 and 3 keep every note of the theme, add only passing and neighbor tones
 *                off the strong beats, one a beat, and keep its harmony and bass
 *   reharmonize  variations (3): variation 2 has the theme's melody and a harmony the melody fits, with the cadence;
 *                the share of its chords that differ from the theme's is printed and is not below its baseline
 *   reregister   rounded binary with A' re-registered, with and without an accompaniment: the melody and the inner
 *                voices are an octave from A's and inside the range; with the default range a run completes only when
 *                one octave fits, else it is stuck at the returns
 */

#define SEEDS 5
#define MAX_EVENTS 512

/* One chord, or one note, of a lane over a section, with its time from the section's start. */
typedef struct {
    Rational at;
    Rational length;
    int pitch;
    char roman[PLAN_ROMAN_NUMERAL_MAX];
} Event;

static Rational minus(Rational a, Rational b) {
    Rational out;
    assert(rational_sub(a, b, &out) == RATIONAL_OK);
    return out;
}

static Rational plus(Rational a, Rational b) {
    Rational out;
    assert(rational_add(a, b, &out) == RATIONAL_OK);
    return out;
}

static bool equal(Rational a, Rational b) {
    return rational_cmp(a, b) == 0;
}

static size_t events(const PiecePlan *plan, NodeId section, PlanLaneKind lane, int voice, Event *out) {
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId ids[MAX_EVENTS];
    size_t spans = plan_lane_find(plan, lane, voice, span, ids, MAX_EVENTS);
    size_t count = 0;
    assert(spans <= MAX_EVENTS);
    for (size_t s = 0; s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[s]);
        Rational from = minus(lane_span.start, span.start);
        if (lane == PLAN_LANE_HARMONY) {
            assert(count < MAX_EVENTS);
            memset(&out[count], 0, sizeof(Event));
            out[count].at = from;
            out[count].length = lane_span.length;
            memcpy(out[count].roman, payload->harmony.roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
            count++;
            continue;
        }
        for (size_t n = 0; n < payload->notes.note_count; n++) {
            assert(count < MAX_EVENTS);
            memset(&out[count], 0, sizeof(Event));
            out[count].at = plus(from, payload->notes.notes[n].offset);
            out[count].length = payload->notes.notes[n].duration;
            out[count].pitch = payload->notes.notes[n].midi_pitch;
            count++;
        }
    }
    return count;
}

/* Whether two lists are the same events, the pitches of the second shift semitones from the first's. */
static bool same_events(const Event *a, size_t an, const Event *b, size_t bn, int shift) {
    if (an != bn) {
        return false;
    }
    for (size_t i = 0; i < an; i++) {
        if (!equal(a[i].at, b[i].at) || !equal(a[i].length, b[i].length) || a[i].pitch + shift != b[i].pitch ||
            strcmp(a[i].roman, b[i].roman) != 0) {
            return false;
        }
    }
    return true;
}

/* Whether two sections have the same nodes below them, at the same places in their sections. */
static bool same_subtree(const PiecePlan *plan, NodeId a, NodeId b) {
    if (plan_node_child_count(plan, a) != plan_node_child_count(plan, b)) {
        return false;
    }
    for (size_t i = 0; i < plan_node_child_count(plan, a); i++) {
        NodeId x = plan_node_child_at(plan, a, i);
        NodeId y = plan_node_child_at(plan, b, i);
        const char *xn = plan_node_name(plan, x);
        const char *yn = plan_node_name(plan, y);
        if (plan_node_kind(plan, x) != plan_node_kind(plan, y) || !xn || !yn || strcmp(xn, yn) != 0 ||
            !equal(minus(plan_node_span(plan, x).start, plan_node_span(plan, a).start),
                   minus(plan_node_span(plan, y).start, plan_node_span(plan, b).start)) ||
            !equal(plan_node_span(plan, x).length, plan_node_span(plan, y).length) ||
            plan_node_state(plan, x) != plan_node_state(plan, y) ||
            plan_node_constraint_count(plan, x) != plan_node_constraint_count(plan, y) || !same_subtree(plan, x, y)) {
            return false;
        }
    }
    return true;
}

static const PlanConstraint *return_of(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_RETURN) {
            return constraint;
        }
    }
    return NULL;
}

/* The sections of the piece, in order, without the material. */
static size_t sections_of(const PiecePlan *plan, NodeId *out) {
    NodeId form = plan_root(plan);
    size_t count = plan_node_child_count(plan, form) - 1;
    for (size_t i = 0; i < count; i++) {
        out[i] = plan_node_child_at(plan, form, i);
    }
    return count;
}

static PieceRequest request_of(FormTemplateKind kind, size_t bars, size_t count, bool accompanied) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = kind;
    request.bars = bars;
    request.count = count;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    if (accompanied) {
        request.has_accompaniment = true;
        request.accompaniment.pattern = PLAN_ACCOMPANIMENT_ANY;
        request.accompaniment.texture = -1;
    }
    return request;
}

static PieceResult compose(const PieceRequest *request, unsigned long seed, const MelodyFillParams *melody,
                           void (*edit)(PiecePlan *), PiecePlan **plan, ComposeOutcome *outcome) {
    ComposeConfig config;
    assert(piece_build_plan(request, plan) == PIECE_OK);
    if (edit) {
        edit(*plan);
    }
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.melody = melody;
    return compose_run(*plan, &config, outcome);
}

/* The lanes a return shares with the section it returns, and whether they are the same events. */
static bool lane_same(const PiecePlan *plan, NodeId source, NodeId section, PlanLaneKind lane, int voice, int shift) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    size_t an = events(plan, source, lane, voice, a);
    size_t bn = events(plan, section, lane, voice, b);
    return same_events(a, an, b, bn, shift);
}

/* ------------------------------------------------------------ exact */

typedef struct {
    const char *name;
    FormTemplateKind kind;
    size_t bars;
    size_t count;
    bool accompanied;
} Scenario;

static void check_exact(const PiecePlan *plan, NodeId source, NodeId section) {
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    assert(same_subtree(plan, source, section));
    assert(lane_same(plan, source, section, PLAN_LANE_HARMONY, 0, 0));
    assert(lane_same(plan, source, section, PLAN_LANE_BASS, 0, 0));
    assert(lane_same(plan, source, section, PLAN_LANE_MELODY, 0, 0));
    for (int voice = 1; voice <= 3; voice++) {
        assert(lane_same(plan, source, section, PLAN_LANE_INNER_VOICE, voice, 0));
    }
}

static int test_exact(bool smoke) {
    static const Scenario SCENARIOS[] = {
        {"rounded binary 24 bars", FORM_TEMPLATE_ROUNDED_BINARY, 24, 0, false},
        {"rounded binary 24 bars, accompanied", FORM_TEMPLATE_ROUNDED_BINARY, 24, 0, true},
        {"ternary 24 bars", FORM_TEMPLATE_TERNARY, 24, 0, false},
        {"layered, 3 layers", FORM_TEMPLATE_LAYERED, 0, 3, false},
    };
    size_t scenario_count = smoke ? 1 : sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);
    for (size_t s = 0; s < scenario_count; s++) {
        PieceRequest request = request_of(SCENARIOS[s].kind, SCENARIOS[s].bars, SCENARIOS[s].count, SCENARIOS[s].accompanied);
        size_t completed = 0;
        size_t returns = 0;
        for (unsigned long seed = 1; seed <= (smoke ? 1UL : SEEDS); seed++) {
            PiecePlan *plan;
            ComposeOutcome outcome;
            if (compose(&request, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
                NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
                size_t count = sections_of(plan, sections);
                completed++;
                for (size_t i = 0; i < count; i++) {
                    const PlanConstraint *back = return_of(plan, sections[i]);
                    if (back) {
                        assert(back->params.section_return.variation == PLAN_RETURN_EXACT);
                        check_exact(plan, back->params.section_return.section, sections[i]);
                        returns++;
                    }
                }
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("%s: %zu of %zu runs complete, %zu exact returns equal the section they return\n", SCENARIOS[s].name,
               completed, smoke ? 1UL : (size_t)SEEDS, returns);
        assert(completed > 0 && returns >= completed);
    }
    return 0;
}

/* ------------------------------------------------------------ ornament */

static bool strong(Rational at) {
    /* A cell of 4/4 is a half note. */
    return (at.num * 2) % (long)at.den == 0;
}

static long beat_of(Rational at) {
    return (at.num * 4) / (long)at.den;
}

static int step_between(int a, int b) {
    int move = b - a;
    return (move >= 1 && move <= 2) ? 1 : ((move <= -1 && move >= -2) ? -1 : 0);
}

static size_t check_ornament(const PiecePlan *plan, NodeId source, NodeId section) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    assert(same_subtree(plan, source, section));
    assert(lane_same(plan, source, section, PLAN_LANE_HARMONY, 0, 0));
    assert(lane_same(plan, source, section, PLAN_LANE_BASS, 0, 0));
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    size_t added = 0;
    long last_beat = -1;
    /* Every note of the theme is there, at its place and pitch, and the ones on a strong beat as long or shorter. */
    for (size_t i = 0; i < an; i++) {
        bool found = false;
        for (size_t j = 0; j < bn && !found; j++) {
            found = equal(a[i].at, b[j].at) && a[i].pitch == b[j].pitch && rational_cmp(b[j].length, a[i].length) <= 0;
        }
        assert(found);
    }
    for (size_t j = 0; j < bn; j++) {
        bool original = false;
        for (size_t i = 0; i < an && !original; i++) {
            original = equal(a[i].at, b[j].at);
        }
        if (original) {
            continue;
        }
        added++;
        assert(!strong(b[j].at) && j > 0 && j + 1 < bn);
        int from = step_between(b[j - 1].pitch, b[j].pitch);
        int to = step_between(b[j].pitch, b[j + 1].pitch);
        assert(from != 0 && to != 0 && (from == to || b[j - 1].pitch == b[j + 1].pitch));
        assert(beat_of(b[j].at) != last_beat);
        last_beat = beat_of(b[j].at);
    }
    assert(bn == an + added && added > 0);
    return added;
}

static int test_ornament(void) {
    PieceRequest request = request_of(FORM_TEMPLATE_VARIATIONS, 0, 3, false);
    size_t completed = 0;
    size_t added = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeOutcome outcome;
        if (compose(&request, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
            NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
            size_t count = sections_of(plan, sections);
            assert(count == 4);
            completed++;
            for (size_t i = 1; i < count; i += 2) {
                const PlanConstraint *back = return_of(plan, sections[i]);
                assert(back && back->params.section_return.variation == PLAN_RETURN_ORNAMENT);
                added += check_ornament(plan, back->params.section_return.section, sections[i]);
            }
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("variations: %zu of %zu runs complete, %zu notes added to the melody of the ornamented variations\n",
           completed, (size_t)SEEDS, added);
    assert(completed > 0 && added >= 2 * completed);
    return 0;
}

/* ------------------------------------------------------------ reharmonize */

/* The chords of variation 2 that differ from the theme's, over the runs: one in each, as the harmony operators have
   little to change. */
#define REHARMONIZE_CHANGED_BASELINE 5

static int test_reharmonize(void) {
    PieceRequest request = request_of(FORM_TEMPLATE_VARIATIONS, 0, 3, false);
    size_t completed = 0;
    size_t changed_total = 0;
    size_t chords_total = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeOutcome outcome;
        if (compose(&request, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
            NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
            size_t count = sections_of(plan, sections);
            const PlanConstraint *back = return_of(plan, sections[2]);
            assert(count == 4 && back && back->params.section_return.variation == PLAN_RETURN_REHARMONIZE);
            NodeId source = back->params.section_return.section;
            static Event a[MAX_EVENTS];
            static Event b[MAX_EVENTS];
            size_t changed;
            size_t total;
            completed++;
            assert(plan_node_state(plan, sections[2]) == PLAN_STATE_REALIZED);
            assert(same_subtree(plan, source, sections[2]));
            /* The melody is the theme's; the harmony and the bass under it are written, the melody fits them. */
            assert(lane_same(plan, source, sections[2], PLAN_LANE_MELODY, 0, 0));
            assert(form_return_melody_fits(plan, sections[2]));
            size_t an = events(plan, source, PLAN_LANE_HARMONY, 0, a);
            size_t bn = events(plan, sections[2], PLAN_LANE_HARMONY, 0, b);
            assert(an > 0 && bn > 0 && strcmp(a[an - 1].roman, b[bn - 1].roman) == 0);
            assert(equal(plus(b[bn - 1].at, b[bn - 1].length), plan_node_span(plan, sections[2]).length));
            assert(events(plan, sections[2], PLAN_LANE_BASS, 0, a) >= bn);
            assert(form_return_chords_changed(plan, sections[2], &changed, &total) && total == bn);
            changed_total += changed;
            chords_total += total;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("variations: %zu of %zu runs complete, %zu of %zu chords of variation 2 differ from the theme's "
           "(baseline %d)\n",
           completed, (size_t)SEEDS, changed_total, chords_total, REHARMONIZE_CHANGED_BASELINE);
    assert(completed > 0);
    return changed_total >= REHARMONIZE_CHANGED_BASELINE ? 0 : 1;
}

/* ------------------------------------------------------------ reregister */

static void make_reregister(PiecePlan *plan) {
    NodeId section = plan_node_child_at(plan, plan_root(plan), 2);
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        PlanConstraint constraint = *plan_node_constraint_at(plan, section, i);
        if (constraint.kind == PLAN_CONSTRAINT_RETURN) {
            constraint.params.section_return.variation = PLAN_RETURN_REREGISTER;
            assert(plan_node_remove_constraint(plan, section, i) == PLAN_OK);
            assert(plan_node_add_constraint(plan, section, &constraint) == PLAN_OK);
            return;
        }
    }
    assert(!"a return to edit");
}

/* The melody of the return is an octave from the theme's, all of it the same way, and inside the range. */
static int check_octave(const PiecePlan *plan, NodeId source, NodeId section, const MelodyFillParams *range, bool inner) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    int shift = 0;
    assert(an == bn && an > 0);
    for (size_t i = 0; i < an; i++) {
        shift = shift == 0 ? b[i].pitch - a[i].pitch : shift;
        assert(shift == 12 || shift == -12);
        assert(b[i].pitch - a[i].pitch == shift && equal(a[i].at, b[i].at) && equal(a[i].length, b[i].length));
        assert(b[i].pitch >= range->range_low && b[i].pitch <= range->range_high);
    }
    assert(lane_same(plan, source, section, PLAN_LANE_HARMONY, 0, 0));
    assert(lane_same(plan, source, section, PLAN_LANE_BASS, 0, 0));
    for (int voice = 1; inner && voice <= 3; voice++) {
        assert(lane_same(plan, source, section, PLAN_LANE_INNER_VOICE, voice, shift));
    }
    return shift;
}

static int test_reregister(void) {
    /* A range wide enough for an octave either way, and the default, which seldom has room for one. */
    MelodyFillParams wide = melody_fill_params_default();
    MelodyFillParams narrow = melody_fill_params_default();
    wide.range_low = 48;
    wide.range_high = 91;
    const MelodyFillParams *ranges[2] = {&wide, &narrow};
    const char *names[2] = {"range 48 to 91", "default range"};
    size_t moved_wide = 0;
    for (size_t r = 0; r < 2; r++) {
        for (int accompanied = 0; accompanied < 2; accompanied++) {
            PieceRequest request = request_of(FORM_TEMPLATE_ROUNDED_BINARY, 24, 0, accompanied);
            size_t completed = 0;
            size_t stuck_at_returns = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PiecePlan *plan;
                ComposeOutcome outcome;
                PieceResult ran = compose(&request, seed, ranges[r], make_reregister, &plan, &outcome);
                if (ran == PIECE_OK) {
                    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
                    sections_of(plan, sections);
                    check_octave(plan, sections[0], sections[2], ranges[r], accompanied);
                    completed++;
                } else {
                    /* Neither octave fits: the section is not returned. */
                    assert(ran == PIECE_ERR_STUCK);
                    stuck_at_returns += outcome.stuck_stage == COMPOSE_STAGE_RETURNS;
                }
                compose_outcome_destroy(&outcome);
                plan_destroy(plan);
            }
            printf("%s%s: %zu of %zu runs complete with A' an octave from A, %zu stuck at the returns\n", names[r],
                   accompanied ? ", accompanied" : "", completed, (size_t)SEEDS, stuck_at_returns);
            moved_wide += r == 0 ? completed : 0;
        }
    }
    assert(moved_wide > 0);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "smoke") == 0) {
        return test_exact(true);
    }
    if (strcmp(mode, "exact") == 0) {
        return test_exact(false);
    }
    if (strcmp(mode, "ornament") == 0) {
        return test_ornament();
    }
    if (strcmp(mode, "reharmonize") == 0) {
        return test_reharmonize();
    }
    if (strcmp(mode, "reregister") == 0) {
        return test_reregister();
    }
    fprintf(stderr, "usage: test_form_return smoke|exact|ornament|reharmonize|reregister\n");
    return 2;
}
