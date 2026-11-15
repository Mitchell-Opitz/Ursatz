#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>

/*
 * The Andante and rondo forms (P52 B5 and B6, R-520), one test per form:
 *
 *   test_rondo_forms andante-aba | andante-abab | small-rondo-ababa | small-rondo-abaca | large-rondo | sonata-rondo
 *
 * Eight rows of key and meter (C major, A minor 4/4 and 3/4, G major and D minor 2/4, C major and D minor 6/8), seeds 1 to
 * 10, each seed tried again from the next seeds, up to 16 times, as the piece entry does (PIECE_SEED_TRIES): at least 9 of
 * 10 seeds compose in each row. In every composed run:
 *   - B is in the subordinate region's key from its first bar (V major in a major piece, III in a minor one);
 *   - R and R1 start in the home key;
 *   - A1 keeps every melody note of A at its place and pitch and has more notes (embellish, R-516);
 *   - B1 (ABAB, ABABA) is in the tonic major (home in a major piece, the parallel major in a minor one, R-517); its
 *     melody is B's moved by the key difference; the section after it starts in the home key;
 *   - the section set as a `setting` (A2 in ABABA and ABACA, A3 in the large and sonata rondos) has A's harmony, bass and
 *     melody, and an inner voice 1 with notes where A's has none (R-518);
 *   - A2 of the large and sonata rondos is A reharmonized: it keeps A's melody;
 *   - C (ABACA, large rondo) is in one of the second region's keys throughout (vi or IV in major, VI or iv in minor);
 *   - P of the sonata rondo is in the development key throughout (vi in major, iv in minor) and S in the sequence key (ii in
 *     major, v in minor), S's melody P's moved by the key difference;
 *   - R2 starts in the home key;
 *   - K ends on the tonic.
 * The counts of each row, with the first seed alone and with the tries, and the seconds a seed are printed; docs/rondo.md
 * records them. The tests are slow (CMake LABELS slow).
 */

#define SEEDS 10
#define SEED_TRIES 16
#define COMPOSED_LEAST 9
#define MAX_EVENTS 512
#define NONE ((size_t)-1)

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
} Row;

static const Row ROWS[] = {
    {"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4}, {"A minor 4/4", 9, KEY_STRUCTURE_MODE_MINOR, 4, 4},
    {"C major 3/4", 0, KEY_STRUCTURE_MODE_MAJOR, 3, 4}, {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4},
    {"G major 2/4", 7, KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {"D minor 2/4", 2, KEY_STRUCTURE_MODE_MINOR, 2, 4},
    {"C major 6/8", 0, KEY_STRUCTURE_MODE_MAJOR, 6, 8}, {"D minor 6/8", 2, KEY_STRUCTURE_MODE_MINOR, 6, 8},
};
#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

/* Where each part is in a form's sections, in order; NONE where the form has none. `set` is the section returned as a
   `setting` (A2 in the small rondos, A3 in the large and sonata), `reh` the one returned reharmonized (A2 in those). */
typedef struct {
    const char *name;
    size_t count;
    size_t a, b, r, a1, b1, r1, r2, set, reh, c, p, s, k;
} Layout;

static const Layout LAYOUTS[] = {
    {"andante-aba", 6, 0, 2, 3, 4, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, 5},
    {"andante-abab", 8, 0, 2, 3, 4, 6, NONE, NONE, NONE, NONE, NONE, NONE, NONE, 7},
    {"small-rondo-ababa", 10, 0, 2, 3, 4, 6, 7, NONE, 8, NONE, NONE, NONE, NONE, 9},
    {"small-rondo-abaca", 9, 0, 2, 3, 4, NONE, 6, NONE, 7, NONE, 5, NONE, NONE, 8},
    {"large-rondo", 13, 0, 2, 3, 4, 9, 6, 10, 11, 7, 5, NONE, NONE, 12},
    {"sonata-rondo", 14, 0, 2, 3, 4, 10, 7, 11, 12, 8, NONE, 5, 6, 13},
};

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

/* The chords, or the notes, of a lane over a section, with their time from the section's start. */
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

static size_t sections_of(const PiecePlan *plan, NodeId *out) {
    NodeId form = plan_root(plan);
    size_t count = plan_node_child_count(plan, form) - 1;
    for (size_t i = 0; i < count; i++) {
        out[i] = plan_node_child_at(plan, form, i);
    }
    return count;
}

/* The key lane. */
typedef struct {
    PlanSpan span[32];
    AnalysisKey key[32];
    size_t count;
} KeyLane;

static KeyLane read_lane(const PiecePlan *plan) {
    KeyLane lane;
    LaneSpanId ids[32];
    PlanSpan all = {{0, 1}, {100000, 1}};
    lane.count = plan_lane_find(plan, PLAN_LANE_KEY, 0, all, ids, 32);
    assert(lane.count >= 1 && lane.count <= 32);
    for (size_t i = 0; i < lane.count; i++) {
        lane.span[i] = plan_lane_span_span(plan, ids[i]);
        lane.key[i] = plan_lane_span_payload(plan, ids[i])->key;
    }
    return lane;
}

static AnalysisKey key_at(const KeyLane *lane, Rational time) {
    for (size_t i = 0; i < lane->count; i++) {
        Rational end;
        assert(plan_span_end(lane->span[i], &end) == PLAN_OK);
        if (rational_cmp(time, end) < 0) {
            return lane->key[i];
        }
    }
    assert(!"a time inside the form");
    return lane->key[0];
}

static bool is_key(AnalysisKey key, int tonic, KeyStructureMode mode) {
    return analysis_key_tonic_pitch_class(&key) == ((tonic % 12) + 12) % 12 && key.mode == mode;
}

/* The key difference in semitones from -5 to 6 a section returned in another key is moved by. */
static int shift_between(AnalysisKey from, AnalysisKey to) {
    int step = ((analysis_key_tonic_pitch_class(&to) - analysis_key_tonic_pitch_class(&from)) % 12 + 12) % 12;
    return step > 6 ? step - 12 : step;
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

/* A1 keeps every melody note of A at its place and pitch and has more notes. */
static void check_embellished(const PiecePlan *plan, NodeId source, NodeId section) {
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
}

/* A2 has A's harmony, bass and melody, and an inner voice 1 with notes where A has none. */
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

/* B1's melody is B's, moved by the key difference. */
static void check_moved(const PiecePlan *plan, NodeId b, NodeId b1, int shift) {
    static Event x[MAX_EVENTS];
    static Event y[MAX_EVENTS];
    size_t xn = events(plan, b, PLAN_LANE_MELODY, 0, x);
    size_t yn = events(plan, b1, PLAN_LANE_MELODY, 0, y);
    assert(xn > 0 && xn == yn);
    for (size_t i = 0; i < xn; i++) {
        assert(equal(x[i].at, y[i].at) && equal(x[i].length, y[i].length) && x[i].pitch + shift == y[i].pitch);
    }
}

/* C is in one key throughout, one of the second region's: vi or IV in major, VI or iv in minor. */
static void check_second(const PiecePlan *plan, const KeyLane *lane, NodeId section, const Row *row) {
    PlanSpan span = plan_node_span(plan, section);
    Rational end;
    assert(plan_span_end(span, &end) == PLAN_OK);
    const bool major = row->mode == KEY_STRUCTURE_MODE_MAJOR;
    const AnalysisKey first = key_at(lane, span.start);
    assert(is_key(first, row->tonic + (major ? 9 : 8), major ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR) ||
           is_key(first, row->tonic + 5, row->mode));
    for (size_t i = 0; i < lane->count; i++) {
        Rational lane_end;
        assert(plan_span_end(lane->span[i], &lane_end) == PLAN_OK);
        if (rational_cmp(lane->span[i].start, end) < 0 && rational_cmp(lane_end, span.start) > 0) {
            assert(is_key(lane->key[i], analysis_key_tonic_pitch_class(&first), first.mode));
        }
    }
}

/* P or S: one key throughout, the given one. */
static void check_one_key(const PiecePlan *plan, const KeyLane *lane, NodeId section, int tonic, KeyStructureMode mode) {
    PlanSpan span = plan_node_span(plan, section);
    Rational end;
    assert(plan_span_end(span, &end) == PLAN_OK);
    assert(is_key(key_at(lane, span.start), tonic, mode));
    for (size_t i = 0; i < lane->count; i++) {
        Rational lane_end;
        assert(plan_span_end(lane->span[i], &lane_end) == PLAN_OK);
        if (rational_cmp(lane->span[i].start, end) < 0 && rational_cmp(lane_end, span.start) > 0) {
            assert(is_key(lane->key[i], tonic, mode));
        }
    }
}

/* K ends on the tonic: its last bass note and its last chord. */
static void check_ends_on_tonic(const PiecePlan *plan, NodeId section, const Row *row) {
    static Event bass[MAX_EVENTS];
    static Event chords[MAX_EVENTS];
    size_t bn = events(plan, section, PLAN_LANE_BASS, 0, bass);
    size_t cn = events(plan, section, PLAN_LANE_HARMONY, 0, chords);
    assert(bn > 0 && cn > 0);
    assert((bass[bn - 1].pitch % 12 + 12) % 12 == row->tonic % 12);
    const char *roman = chords[cn - 1].roman;
    assert((roman[0] == 'I' || roman[0] == 'i') && (roman[1] == '\0' || roman[1] == ' ' || roman[1] == '7' || roman[1] == '6'));
}

/* Everything the branch says of a composed piece. */
static void check_piece(const PiecePlan *plan, const Layout *layout, const Row *row) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(sections_of(plan, sections) == layout->count);
    const KeyLane lane = read_lane(plan);
    const bool minor = row->mode == KEY_STRUCTURE_MODE_MINOR;
    const int region = minor ? row->tonic + 3 : row->tonic + 7;
    /* B is in the region from its first bar, V major or III. */
    const AnalysisKey at_b = key_at(&lane, plan_node_span(plan, sections[layout->b]).start);
    assert(is_key(at_b, region, KEY_STRUCTURE_MODE_MAJOR));
    /* R and R1 start in the home key. */
    assert(is_key(key_at(&lane, plan_node_span(plan, sections[layout->r]).start), row->tonic, row->mode));
    if (layout->r1 != NONE) {
        assert(is_key(key_at(&lane, plan_node_span(plan, sections[layout->r1]).start), row->tonic, row->mode));
    }
    if (layout->r2 != NONE) {
        assert(is_key(key_at(&lane, plan_node_span(plan, sections[layout->r2]).start), row->tonic, row->mode));
    }
    /* A1 keeps every melody note of A and has more. */
    const PlanConstraint *back = return_of(plan, sections[layout->a1]);
    assert(back && back->params.section_return.variation == PLAN_RETURN_EMBELLISH &&
           back->params.section_return.section == sections[layout->a]);
    check_embellished(plan, sections[layout->a], sections[layout->a1]);
    /* B1 is in the tonic major, its melody is B's moved by the key difference, and the section after it is home. */
    if (layout->b1 != NONE) {
        const AnalysisKey at_b1 = key_at(&lane, plan_node_span(plan, sections[layout->b1]).start);
        assert(is_key(at_b1, row->tonic, KEY_STRUCTURE_MODE_MAJOR));
        check_moved(plan, sections[layout->b], sections[layout->b1], shift_between(at_b, at_b1));
        assert(is_key(key_at(&lane, plan_node_span(plan, sections[layout->b1 + 1]).start), row->tonic, row->mode));
    }
    /* A2 of the large and sonata rondos is A reharmonized: its melody is A's. */
    if (layout->reh != NONE) {
        const PlanConstraint *again = return_of(plan, sections[layout->reh]);
        assert(again && again->params.section_return.variation == PLAN_RETURN_REHARMONIZE &&
               again->params.section_return.section == sections[layout->a]);
        assert(plan_node_state(plan, sections[layout->reh]) == PLAN_STATE_REALIZED);
        check_moved(plan, sections[layout->a], sections[layout->reh],
                    shift_between(key_at(&lane, plan_node_span(plan, sections[layout->a]).start),
                                  key_at(&lane, plan_node_span(plan, sections[layout->reh]).start)));
    }
    /* The section set in a new accompaniment is A set. */
    if (layout->set != NONE) {
        const PlanConstraint *setting = return_of(plan, sections[layout->set]);
        assert(setting && setting->params.section_return.variation == PLAN_RETURN_SETTING &&
               setting->params.section_return.setting == PLAN_ACCOMPANIMENT_ARPEGGIATED &&
               setting->params.section_return.section == sections[layout->a]);
        check_setting(plan, sections[layout->a], sections[layout->set]);
    }
    if (layout->c != NONE) {
        check_second(plan, &lane, sections[layout->c], row);
    }
    /* The sonata rondo's P is in the development key and S in the sequence key; S's melody is P's moved. */
    if (layout->p != NONE) {
        const bool major = row->mode == KEY_STRUCTURE_MODE_MAJOR;
        check_one_key(plan, &lane, sections[layout->p], row->tonic + (major ? 9 : 5), KEY_STRUCTURE_MODE_MINOR);
        check_one_key(plan, &lane, sections[layout->s], row->tonic + (major ? 2 : 7), KEY_STRUCTURE_MODE_MINOR);
        check_moved(plan, sections[layout->p], sections[layout->s],
                    shift_between(key_at(&lane, plan_node_span(plan, sections[layout->p]).start),
                                  key_at(&lane, plan_node_span(plan, sections[layout->s]).start)));
    }
    check_ends_on_tonic(plan, sections[layout->k], row);
}

/* One try: the plan of the form in the row from the seed, composed; true when it completes (and then it is checked). */
static bool compose_try(const Layout *layout, const Row *row, unsigned long seed) {
    PieceRequest request;
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    ComposeConfig config;
    memset(&request, 0, sizeof(request));
    request.form = layout->name;
    request.key = plan_key_of_pitch_class(row->tonic, row->mode);
    request.meter.beats_per_bar = (unsigned)row->beats;
    request.meter.beat_unit = (unsigned)row->unit;
    request.modulate = true;
    request.seed = seed;
    assert(piece_build_plan(&request, &plan) == PIECE_OK && plan);
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    PieceResult ran = compose_run(plan, &config, &outcome);
    if (ran == PIECE_OK) {
        check_piece(plan, layout, row);
    } else {
        assert(ran == PIECE_ERR_STUCK);
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return ran == PIECE_OK;
}

static int test_form(const Layout *layout) {
    int failed = 0;
    for (size_t r = 0; r < ROW_COUNT; r++) {
        size_t first = 0;
        size_t composed = 0;
        size_t tries = 0;
        clock_t started = clock();
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            bool done = false;
            for (unsigned long extra = 0; extra < SEED_TRIES && !done; extra++) {
                done = compose_try(layout, &ROWS[r], seed + extra);
                tries++;
                first += done && extra == 0;
            }
            composed += done;
        }
        double seconds = (double)(clock() - started) / CLOCKS_PER_SEC / (double)SEEDS;
        printf("%s %s: %zu of %d seeds compose (%zu on the first seed alone, %zu tries), %.1f s a seed\n", layout->name,
               ROWS[r].name, composed, SEEDS, first, tries, seconds);
        if (composed < COMPOSED_LEAST) {
            fprintf(stderr, "%s %s: at least %d of %d seeds should compose\n", layout->name, ROWS[r].name, COMPOSED_LEAST,
                    SEEDS);
            failed = 1;
        }
    }
    return failed;
}

int main(int argc, char **argv) {
    for (size_t i = 0; argc == 2 && i < sizeof(LAYOUTS) / sizeof(LAYOUTS[0]); i++) {
        if (strcmp(argv[1], LAYOUTS[i].name) == 0) {
            return test_form(&LAYOUTS[i]);
        }
    }
    fprintf(stderr, "usage: test_rondo_forms andante-aba|andante-abab|small-rondo-ababa|small-rondo-abaca|large-rondo|sonata-rondo\n");
    return 2;
}
