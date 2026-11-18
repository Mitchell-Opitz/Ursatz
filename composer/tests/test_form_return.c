#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>
#include "ursatz_test.h"
#include "support/test_knowledge.h"

/*
 * Returning sections (form-return, form_return.h): rounded binary, ternary, layered and variations, C major 4/4 with an
 * invented motive, seeds 1 to 5. A section that returns an earlier one is given its material instead of being grown
 * afresh.
 *
 *   smoke        rounded binary 24 bars, seed 1: A' is A, shifted (quick)
 *   exact        rounded binary (also with an accompaniment) and layered: every return has the nodes and the
 *                harmony, bass, melody and inner voices of the section it returns, shifted by the time between them
 *   ornament     variations (3): variations 1 and 3 keep every note of the theme, add only passing and neighbor tones
 *                off the strong beats, one a beat, and keep its harmony and bass
 *   reharmonize  variations (3): variation 2 has the theme's melody and a harmony the melody fits, with the cadence;
 *                the share of its chords that differ from the theme's is printed and is not below its baseline
 *   varied       tests/fixtures/forms/varied-form.json (A, a pedal B, A returned varied), C major 4/4, seeds 1 to 10:
 *                at least 9 compose; the return keeps every note of A at its place, adds notes (at most one a bar), has a
 *                melody its chords fit, and in at least 7 runs a chord of its last 2 bars is not A's; the built-in ternary
 *                (24 bars, seeds 1 to 5) returns A varied the same way
 *   embellish    the stand-in form of R-516 (A, T, B, R, A1 returning A embellish, K; 44 bars, modulating), G major and
 *                D minor 2/4, seeds 1 to 10: at least 9 of 10 compose in each; A1 keeps every melody note of A at its
 *                place and pitch and has more notes; with A1 ornament the form composes at most 2 of 10 in G major
 *   setting      the stand-in form of R-516 with A1 returning A setting arpeggiated (R-518), C major and A minor in 4/4 and
 *                3/4, G major and D minor in 2/4, C major and D minor in 6/8, seeds 1 to 10: at least 9 of 10 compose in
 *                each; A1's harmony, bass and melody are A's and its inner voice 1 has notes where A's has none; with a
 *                request accompaniment of arpeggiated A1's return is set in alberti, and an alberti setting in 6/8 is
 *                built as oom-pah
 *   home         the stand-in of R-524 (andante-abab with B1 returning B reharmonized in the home mode; 56 bars,
 *                modulating), C major and A minor in 4/4, G major and D minor in 2/4, seeds 1 to 10, each seed tried
 *                again from the next seeds up to 16 times as the piece entry does (PIECE_SEED_TRIES): at least 9 of 10
 *                compose in each; B1's key lane is the home key throughout, its RETURN has across_modes, its melody has
 *                B's onsets and durations and B's pitches moved by the tonic step (in minor also mapped into the minor)
 *   reregister   rounded binary with A' re-registered, with and without an accompaniment: the melody and the inner
 *                voices are an octave from A's and inside the range; with the default range a run completes only when
 *                one octave fits, else it is stuck at the returns
 */

#define SEEDS 5
#define VARIED_SEEDS 10
#define VARIED_COMPOSED_LEAST 9
#define VARIED_CADENCE_LEAST 7
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
            out[count].pitch = payload->notes.notes[n].midi;
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

/* The return a section holds, as a small record in a slot that stays valid through the next three calls; NULL for a section that returns none. */
static const ReturnRef *return_of(const PiecePlan *plan, NodeId section) {
    static ReturnRef slots[4];
    static size_t next = 0;
    const SectionDescription *described = plan_section_or_empty(plan, section);
    NodeId source = described->returns[0] != '\0' ? plan_section_named(plan, described->returns) : PLAN_NODE_NONE;
    ReturnRef *out = &slots[next++ % 4];
    if (source == PLAN_NODE_NONE) {
        return NULL;
    }
    memset(out, 0, sizeof(*out));
    out->section = source;
    out->variation = described->variation;
    out->setting = (PlanAccompanimentPattern)described->setting;
    out->across_modes = described->mode == SECTION_MODE_HOME;
    return out;
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

static PieceRequest request_of(const char *kind, size_t bars, size_t count, bool accompanied) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = kind;
    request.bars = bars;
    request.count = count;
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
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
    config.knowledge = test_knowledge();
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
    const char *kind;
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
        {"rounded binary 24 bars", "rounded-binary", 24, 0, false},
        {"rounded binary 24 bars, accompanied", "rounded-binary", 24, 0, true},
        {"layered, 3 layers", "layered", 0, 3, false},
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
                NodeId sections[FORM_SPEC_MAX_SECTIONS];
                size_t count = sections_of(plan, sections);
                completed++;
                for (size_t i = 0; i < count; i++) {
                    const ReturnRef *back = return_of(plan, sections[i]);
                    if (back) {
                        assert(back->variation == SECTION_RETURN_EXACT);
                        check_exact(plan, back->section, sections[i]);
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
    PieceRequest request = request_of("variations", 0, 3, false);
    size_t completed = 0;
    size_t added = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeOutcome outcome;
        if (compose(&request, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = sections_of(plan, sections);
            assert(count == 4);
            completed++;
            for (size_t i = 1; i < count; i += 2) {
                const ReturnRef *back = return_of(plan, sections[i]);
                assert(back && back->variation == SECTION_RETURN_ORNAMENT);
                added += check_ornament(plan, back->section, sections[i]);
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
    PieceRequest request = request_of("variations", 0, 3, false);
    size_t completed = 0;
    size_t changed_total = 0;
    size_t chords_total = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeOutcome outcome;
        if (compose(&request, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = sections_of(plan, sections);
            const ReturnRef *back = return_of(plan, sections[2]);
            assert(count == 4 && back && back->variation == SECTION_RETURN_REHARMONIZE);
            NodeId source = back->section;
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

/* ------------------------------------------------------------ varied */

/* The notes the varied return adds to A's melody: every note of A is there at its place and pitch, at most one added a bar. */
static size_t check_varied(const PiecePlan *plan, NodeId source, NodeId section) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    size_t added = 0;
    long last_bar = -1;
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    assert(same_subtree(plan, source, section));
    for (size_t i = 0; i < an; i++) {
        bool found = false;
        for (size_t j = 0; j < bn && !found; j++) {
            found = equal(a[i].at, b[j].at) && a[i].pitch == b[j].pitch;
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
        long bar = b[j].at.num / (long)b[j].at.den; /* a bar of 4/4 is a whole note */
        assert(bar != last_bar);
        last_bar = bar;
        added++;
    }
    assert(bn == an + added && added > 0);
    /* Every strong-beat note is a tone of the chord now under it. */
    assert(form_return_melody_fits(plan, section));
    return added;
}

static int test_varied(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/forms/varied-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        return 2;
    }
    free(text);
    assert(form->sizes[0].section_count == 3 && form->sizes[0].sections[2].variation == SECTION_RETURN_VARIED);
    size_t composed = 0;
    size_t added = 0;
    size_t cadence = 0;
    for (unsigned long seed = 1; seed <= VARIED_SEEDS; seed++) {
        PieceRequest request = request_of(form->name, 0, 0, false);
        PiecePlan *plan = NULL;
        ComposeOutcome outcome;
        ComposeConfig config;
        assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.form = form;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = 0;
            NodeId root = plan_root(plan);
            for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
                NodeId child = plan_node_child_at(plan, root, i);
                if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
                    sections[count++] = child;
                }
            }
            const ReturnRef *back = return_of(plan, sections[2]);
            bool changed = false;
            assert(count == 3 && back && back->variation == SECTION_RETURN_VARIED);
            added += check_varied(plan, back->section, sections[2]);
            assert(form_return_cadence_changed(plan, sections[2], &changed));
            cadence += changed;
            composed++;
        } else {
            printf("  seed %lu: not composed\n", seed);
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    form_spec_destroy(form);
    printf("varied-form: %zu of %d seeds compose, %zu notes added, a chord of the last 2 bars differs in %zu runs\n",
           composed, VARIED_SEEDS, added, cadence);
    /* The built-in ternary returns A varied (R-445): every composed run keeps A's notes and adds some. */
    PieceRequest ternary = request_of("ternary", 24, 0, false);
    size_t ternary_composed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeOutcome outcome;
        if (compose(&ternary, seed, NULL, NULL, &plan, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            assert(sections_of(plan, sections) == 3);
            const ReturnRef *back = return_of(plan, sections[2]);
            assert(back && back->variation == SECTION_RETURN_VARIED);
            check_varied(plan, back->section, sections[2]);
            ternary_composed++;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("ternary 24 bars: %zu of %d seeds compose, A returned varied\n", ternary_composed, SEEDS);
    assert(ternary_composed > 0);
    if (composed < VARIED_COMPOSED_LEAST || cadence < VARIED_CADENCE_LEAST) {
        fprintf(stderr, "varied-form: at least %d composed and %d changed cadences expected\n", VARIED_COMPOSED_LEAST,
                VARIED_CADENCE_LEAST);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ embellish */

#define EMBELLISH_SEEDS 10
#define EMBELLISH_COMPOSED_LEAST 9
#define EMBELLISH_ORNAMENT_MOST 2

/* The stand-in form of R-516: A, T, B, R, A1 and K, 44 bars, with A1 returning A with the variation %s and the members %s. */
static const char EMBELLISH_FORM[] =
    "{\"schema\":\"ursatz-form\",\"version\":2,\"name\":\"embellish-form\",\"label\":\"Embellish form\",\"modulates\":true,"
    "\"regions\":[{\"name\":\"subordinate\",\"major\":[\"omt.relation.dominant\"],\"minor\":[\"omt.relation.mediant\"]}],"
    "\"sizes\":[{\"bars\":44,\"default\":true,\"sections\":["
    "{\"name\":\"A\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true},"
    "{\"name\":\"T\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"half\",\"keys\":{\"end\":\"subordinate\"}},"
    "{\"name\":\"B\",\"letter\":\"B\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"keys\":{\"start\":\"subordinate\",\"end\":\"subordinate\"},\"motive\":\"invert tonal=1\"},"
    "{\"name\":\"R\",\"letter\":\"R\",\"role\":\"retransition\",\"bars\":4,\"cadence\":\"half\"},"
    "{\"name\":\"A1\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true,\"returns\":\"A\",\"variation\":\"%s\"%s},"
    "{\"name\":\"K\",\"letter\":\"K\",\"role\":\"coda\",\"bars\":8,\"cadence\":\"perfect-authentic\"}]}]}";

typedef struct {
    const char *name;
    int pitch_class;
    HarmonyMode mode;
} EmbellishRow;

/* A1 keeps every melody note of A at its place and pitch and has more notes; returns the notes it adds. */
static size_t check_embellish(const PiecePlan *plan, NodeId source, NodeId section) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    for (size_t i = 0; i < an; i++) {
        bool found = false;
        for (size_t j = 0; j < bn && !found; j++) {
            found = equal(a[i].at, b[j].at) && a[i].pitch == b[j].pitch;
        }
        assert(found);
    }
    assert(bn > an);
    return bn - an;
}

/* How many of the seeds compose the form in a row; with check, the embellished returns are checked. */
static size_t embellish_row(const FormSpec *form, const EmbellishRow *row, bool check, size_t *added) {
    size_t composed = 0;
    for (unsigned long seed = 1; seed <= EMBELLISH_SEEDS; seed++) {
        PieceRequest request;
        PiecePlan *plan = NULL;
        ComposeOutcome outcome;
        ComposeConfig config;
        memset(&request, 0, sizeof(request));
        request.form = form->name;
        request.key = test_key_mode(row->pitch_class, row->mode);
        request.knowledge = test_knowledge();
        request.meter.beats_per_bar = 2;
        request.meter.beat_unit = 4;
        request.modulate = true;
        assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.form = form;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            composed++;
            if (check) {
                NodeId sections[FORM_SPEC_MAX_SECTIONS];
                size_t count = 0;
                NodeId root = plan_root(plan);
                for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
                    NodeId child = plan_node_child_at(plan, root, i);
                    if (!plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
                        sections[count++] = child;
                    }
                }
                const ReturnRef *back = return_of(plan, sections[4]);
                assert(count == 6 && back && back->variation == SECTION_RETURN_EMBELLISH);
                *added += check_embellish(plan, back->section, sections[4]);
            }
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    return composed;
}

static FormSpec *stand_in_form(const char *variation, const char *members) {
    char text[4096];
    FormSpec *form = NULL;
    FormSpecError error;
    snprintf(text, sizeof(text), EMBELLISH_FORM, variation, members);
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "stand-in form: %s: %s\n", error.path, error.message);
        exit(2);
    }
    return form;
}

static FormSpec *embellish_form(const char *variation) {
    return stand_in_form(variation, "");
}

static int test_embellish(void) {
    static const EmbellishRow ROWS[] = {
        {"G major 2/4", 7, HARMONY_MODE_MAJOR},
        {"D minor 2/4", 2, HARMONY_MODE_MINOR},
    };
    FormSpec *form = embellish_form("embellish");
    int failed = 0;
    size_t added = 0;
    for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); r++) {
        size_t composed = embellish_row(form, &ROWS[r], true, &added);
        printf("%s: %zu of %d seeds compose, A1 embellished\n", ROWS[r].name, composed, EMBELLISH_SEEDS);
        if (composed < EMBELLISH_COMPOSED_LEAST) {
            fprintf(stderr, "%s: at least %d of %d seeds should compose\n", ROWS[r].name, EMBELLISH_COMPOSED_LEAST,
                    EMBELLISH_SEEDS);
            failed = 1;
        }
    }
    form_spec_destroy(form);
    /* The same form with A1 ornament has no note to split in 2/4 (R-516): embellish is what lets it compose. */
    FormSpec *plain = embellish_form("ornament");
    size_t none = 0;
    size_t ornament = embellish_row(plain, &ROWS[0], false, &none);
    printf("G major 2/4 with A1 ornament: %zu of %d seeds compose, %zu notes added with embellish\n", ornament,
           EMBELLISH_SEEDS, added);
    form_spec_destroy(plain);
    if (ornament > EMBELLISH_ORNAMENT_MOST) {
        fprintf(stderr, "ornament composes %zu of %d, at most %d expected\n", ornament, EMBELLISH_SEEDS,
                EMBELLISH_ORNAMENT_MOST);
        failed = 1;
    }
    return failed;
}

/* ------------------------------------------------------------ setting */

#define SETTING_SEEDS 10
#define SETTING_COMPOSED_LEAST 9
#define SETTING_ACCOMPANIED_SEEDS 5

typedef struct {
    const char *name;
    int pitch_class;
    HarmonyMode mode;
    int beats;
    int unit;
} SettingRow;

static PieceRequest setting_request(const SettingRow *row) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = "embellish-form";
    request.key = test_key_mode(row->pitch_class, row->mode);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = (unsigned)row->beats;
    request.meter.beat_unit = (unsigned)row->unit;
    request.modulate = true;
    return request;
}

/* A1 has A's harmony, bass and melody, and an inner voice 1 with notes where A has none (the request has no accompaniment). */
static void check_setting(const PiecePlan *plan, NodeId source, NodeId section) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    static const PlanLaneKind LANES[] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    for (size_t l = 0; l < sizeof(LANES) / sizeof(LANES[0]); l++) {
        size_t an = events(plan, source, LANES[l], 0, a);
        size_t bn = events(plan, section, LANES[l], 0, b);
        assert(an > 0 && same_events(a, an, b, bn, 0));
    }
    assert(events(plan, source, PLAN_LANE_INNER_VOICE, 1, a) == 0);
    assert(events(plan, section, PLAN_LANE_INNER_VOICE, 1, b) > 0);
}

/* How many of the seeds compose the form in a row, A1's setting checked in each. */
static size_t setting_row(const FormSpec *form, const SettingRow *row) {
    size_t composed = 0;
    for (unsigned long seed = 1; seed <= SETTING_SEEDS; seed++) {
        PieceRequest request = setting_request(row);
        PiecePlan *plan = NULL;
        ComposeOutcome outcome;
        ComposeConfig config;
        assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.form = form;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            size_t count = sections_of(plan, sections);
            const ReturnRef *back = return_of(plan, sections[4]);
            assert(count == 6 && back && back->variation == SECTION_RETURN_SETTING &&
                   back->setting == PLAN_ACCOMPANIMENT_ARPEGGIATED);
            check_setting(plan, back->section, sections[4]);
            composed++;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    return composed;
}

/* The pattern A1's return is built with, for a form whose setting is `pattern` and a request accompaniment (or none). */
static PlanAccompanimentPattern built_setting(const char *pattern, const SettingRow *row, int accompaniment) {
    char members[64];
    snprintf(members, sizeof(members), ",\"setting\":\"%s\"", pattern);
    FormSpec *form = stand_in_form("setting", members);
    PieceRequest request = setting_request(row);
    PiecePlan *plan = NULL;
    if (accompaniment >= 0) {
        request.has_accompaniment = true;
        request.accompaniment.pattern = (PlanAccompanimentPattern)accompaniment;
        request.accompaniment.texture = -1;
    }
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    sections_of(plan, sections);
    const ReturnRef *back = return_of(plan, sections[4]);
    assert(back && back->variation == SECTION_RETURN_SETTING);
    PlanAccompanimentPattern out = back->setting;
    plan_destroy(plan);
    form_spec_destroy(form);
    return out;
}

static int test_setting(void) {
    static const SettingRow ROWS[] = {
        {"C major 4/4", 0, HARMONY_MODE_MAJOR, 4, 4}, {"A minor 4/4", 9, HARMONY_MODE_MINOR, 4, 4},
        {"C major 3/4", 0, HARMONY_MODE_MAJOR, 3, 4}, {"A minor 3/4", 9, HARMONY_MODE_MINOR, 3, 4},
        {"G major 2/4", 7, HARMONY_MODE_MAJOR, 2, 4}, {"D minor 2/4", 2, HARMONY_MODE_MINOR, 2, 4},
        {"C major 6/8", 0, HARMONY_MODE_MAJOR, 6, 8}, {"D minor 6/8", 2, HARMONY_MODE_MINOR, 6, 8},
    };
    FormSpec *form = stand_in_form("setting", ",\"setting\":\"arpeggiated\"");
    int failed = 0;
    for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); r++) {
        size_t composed = setting_row(form, &ROWS[r]);
        printf("%s: %zu of %d seeds compose, A1 set in arpeggiated\n", ROWS[r].name, composed, SETTING_SEEDS);
        if (composed < SETTING_COMPOSED_LEAST) {
            fprintf(stderr, "%s: at least %d of %d seeds should compose\n", ROWS[r].name, SETTING_COMPOSED_LEAST,
                    SETTING_SEEDS);
            failed = 1;
        }
    }
    form_spec_destroy(form);
    /* The piece's own pattern is not the setting: arpeggiated over an arpeggiated piece is set in the next, alberti. */
    assert(built_setting("arpeggiated", &ROWS[0], PLAN_ACCOMPANIMENT_ARPEGGIATED) == PLAN_ACCOMPANIMENT_ALBERTI);
    FormSpec *accompanied = stand_in_form("setting", ",\"setting\":\"arpeggiated\"");
    size_t alberti = 0;
    for (unsigned long seed = 1; seed <= SETTING_ACCOMPANIED_SEEDS; seed++) {
        PieceRequest request = setting_request(&ROWS[0]);
        PiecePlan *plan = NULL;
        ComposeOutcome outcome;
        ComposeConfig config;
        request.has_accompaniment = true;
        request.accompaniment.pattern = PLAN_ACCOMPANIMENT_ARPEGGIATED;
        request.accompaniment.texture = -1;
        assert(form_build_plan(accompanied, &request, &plan) == PIECE_OK && plan);
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.form = accompanied;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            sections_of(plan, sections);
            const ReturnRef *back = return_of(plan, sections[4]);
            assert(back && back->setting == PLAN_ACCOMPANIMENT_ALBERTI);
            assert(plan_node_state(plan, sections[4]) == PLAN_STATE_REALIZED);
            alberti++;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("C major 4/4 with an arpeggiated accompaniment: %zu of %d seeds compose, A1 set in alberti\n", alberti,
           SETTING_ACCOMPANIED_SEEDS);
    form_spec_destroy(accompanied);
    if (alberti == 0) {
        failed = 1;
    }
    /* Alberti has no compound meter: it is built as oom-pah in 6/8. */
    printf("C major 6/8, setting alberti: A1 is set in oom-pah\n");
    assert(built_setting("alberti", &ROWS[6], -1) == PLAN_ACCOMPANIMENT_OOM_PAH);
    return failed;
}

/* ------------------------------------------------------------ reregister */

static void make_reregister(PiecePlan *plan) {
    NodeId section = plan_node_child_at(plan, plan_root(plan), 2);
    SectionDescription described = *plan_node_section(plan, section);
    assert(described.returns[0] != '\0');
    described.variation = SECTION_RETURN_REREGISTER;
    assert(plan_node_set_section(plan, section, &described) == PLAN_OK);
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
            PieceRequest request = request_of("rounded-binary", 24, 0, accompanied);
            size_t completed = 0;
            size_t stuck_at_returns = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PiecePlan *plan;
                ComposeOutcome outcome;
                PieceResult ran = compose(&request, seed, ranges[r], make_reregister, &plan, &outcome);
                if (ran == PIECE_OK) {
                    NodeId sections[FORM_SPEC_MAX_SECTIONS];
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

/* ------------------------------------------------------------ home */

#define HOME_SEEDS 10
#define HOME_SEED_TRIES 16
#define HOME_COMPOSED_LEAST 9

/* The stand-in form of R-524: andante-abab (56 bars, modulating) with B1 returning B reharmonized in the home mode. */
static const char HOME_FORM[] =
    "{\"schema\":\"ursatz-form\",\"version\":2,\"name\":\"home-form\",\"label\":\"Home form\",\"closing\":true,"
    "\"modulates\":true,\"regions\":[{\"name\":\"subordinate\",\"major\":[\"omt.relation.dominant\"],\"minor\":[\"omt.relation.mediant\"]}],"
    "\"sizes\":[{\"bars\":56,\"default\":true,\"sections\":["
    "{\"name\":\"A\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true},"
    "{\"name\":\"T\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"half\",\"keys\":{\"end\":\"subordinate\"}},"
    "{\"name\":\"B\",\"letter\":\"B\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"keys\":{\"start\":\"subordinate\",\"end\":\"subordinate\"},\"motive\":\"invert tonal=1\"},"
    "{\"name\":\"R\",\"letter\":\"R\",\"role\":\"retransition\",\"bars\":4,\"cadence\":\"half\"},"
    "{\"name\":\"A1\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true,\"returns\":\"A\",\"variation\":\"embellish\"},"
    "{\"name\":\"T1\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"half\"},"
    "{\"name\":\"B1\",\"letter\":\"B\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"returns\":\"B\",\"variation\":\"reharmonize\",\"mode\":\"home\"},"
    "{\"name\":\"K\",\"letter\":\"K\",\"role\":\"coda\",\"bars\":8,\"cadence\":\"perfect-authentic\",\"closed\":true}]}]}";

typedef struct {
    const char *name;
    int pitch_class;
    HarmonyMode mode;
    int beats;
    int unit;
} HomeRow;

/* The keys of the key lane under a section; every one must be the home key. */
static size_t keys_under(const PiecePlan *plan, NodeId section, AnalysisKey *out, size_t capacity) {
    LaneSpanId ids[32];
    size_t count = plan_lane_find(plan, PLAN_LANE_KEY, 0, plan_node_span(plan, section), ids, 32);
    assert(count >= 1 && count <= capacity);
    for (size_t i = 0; i < count; i++) {
        out[i] = plan_lane_span_payload(plan, ids[i])->key;
    }
    return count;
}

/* B1 against B: the key lane is home, the return has across_modes, the melody is B's moved by the tonic step (R-524). */
static void check_home(const PiecePlan *plan, NodeId source, NodeId section, const HomeRow *row) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    AnalysisKey source_key;
    AnalysisKey keys[32];
    const ReturnRef *back = return_of(plan, section);
    assert(back && back->section == source &&
           back->variation == SECTION_RETURN_REHARMONIZE && back->across_modes);
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    assert(keys_under(plan, source, &source_key, 1) == 1);
    size_t key_count = keys_under(plan, section, keys, 32);
    for (size_t i = 0; i < key_count; i++) {
        assert(analysis_key_tonic_pitch_class(&keys[i]) == row->pitch_class && harmony_key_mode(&keys[i]) == row->mode);
    }
    int step = ((row->pitch_class - analysis_key_tonic_pitch_class(&source_key)) % 12 + 12) % 12;
    int shift = step > 6 ? step - 12 : step;
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    assert(an == bn && an > 0);
    for (size_t i = 0; i < an; i++) {
        int pitch = a[i].pitch + shift;
        if (row->mode == HARMONY_MODE_MINOR) {
            int degree = ((pitch - row->pitch_class) % 12 + 12) % 12;
            pitch -= degree == 4 || degree == 9 ? 1 : 0;
        }
        assert(equal(a[i].at, b[i].at) && equal(a[i].length, b[i].length) && b[i].pitch == pitch);
    }
}

static int test_home(void) {
    static const HomeRow ROWS[] = {
        {"C major 4/4", 0, HARMONY_MODE_MAJOR, 4, 4},
        {"A minor 4/4", 9, HARMONY_MODE_MINOR, 4, 4},
        {"G major 2/4", 7, HARMONY_MODE_MAJOR, 2, 4},
        {"D minor 2/4", 2, HARMONY_MODE_MINOR, 2, 4},
    };
    FormSpec *form = NULL;
    FormSpecError error;
    int failed = 0;
    if (form_spec_parse(HOME_FORM, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "home form: %s: %s\n", error.path, error.message);
        return 2;
    }
    for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); r++) {
        size_t composed = 0;
        size_t first = 0;
        for (unsigned long seed = 1; seed <= HOME_SEEDS; seed++) {
            bool done = false;
            for (unsigned long extra = 0; extra < HOME_SEED_TRIES && !done; extra++) {
                PieceRequest request;
                PiecePlan *plan = NULL;
                ComposeOutcome outcome;
                ComposeConfig config;
                memset(&request, 0, sizeof(request));
                request.form = form->name;
                request.key = test_key_mode(ROWS[r].pitch_class, ROWS[r].mode);
                request.knowledge = test_knowledge();
                request.meter.beats_per_bar = ROWS[r].beats;
                request.meter.beat_unit = ROWS[r].unit;
                request.modulate = true;
                assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
                memset(&config, 0, sizeof(config));
                config.knowledge = test_knowledge();
                config.seed = seed + extra;
                config.form = form;
                done = compose_run(plan, &config, &outcome) == PIECE_OK;
                if (done) {
                    NodeId sections[FORM_SPEC_MAX_SECTIONS];
                    size_t count = sections_of(plan, sections);
                    assert(count == 8);
                    check_home(plan, sections[2], sections[6], &ROWS[r]);
                    first += extra == 0;
                }
                compose_outcome_destroy(&outcome);
                plan_destroy(plan);
            }
            composed += done;
        }
        printf("%s: %zu of %d seeds compose (%zu on the first seed alone), B1 returned in the home mode\n", ROWS[r].name,
               composed, HOME_SEEDS, first);
        if (composed < HOME_COMPOSED_LEAST) {
            fprintf(stderr, "%s: at least %d of %d seeds should compose\n", ROWS[r].name, HOME_COMPOSED_LEAST, HOME_SEEDS);
            failed = 1;
        }
    }
    form_spec_destroy(form);
    return failed;
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
    if (strcmp(mode, "varied") == 0) {
        return test_varied();
    }
    if (strcmp(mode, "embellish") == 0) {
        return test_embellish();
    }
    if (strcmp(mode, "setting") == 0) {
        return test_setting();
    }
    if (strcmp(mode, "reregister") == 0) {
        return test_reregister();
    }
    if (strcmp(mode, "home") == 0) {
        return test_home();
    }
    fprintf(stderr, "usage: test_form_return smoke|exact|ornament|reharmonize|varied|embellish|setting|reregister|home\n");
    return 2;
}
