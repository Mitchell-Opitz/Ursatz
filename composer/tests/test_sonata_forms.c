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
 * The sonatina forms (P53 B2, R-523), one test per form:
 *
 *   test_sonata_forms sonatina | sonatina-tonic-major
 *
 * Eight rows of key and meter (C major, A minor 4/4 and 3/4, G major and D minor 2/4, C major and D minor 6/8), seeds 1 to
 * 10, each seed tried again from the next seeds, up to 16 times, as the piece entry does (PIECE_SEED_TRIES): at least 9 of
 * 10 seeds compose in each row. In every composed run:
 *   - B, C1 and C2 are in the subordinate key throughout (V major in a major piece, III in a minor one);
 *   - E is in the elaboration key throughout (IV in major, VI in minor; both major), keeps every melody note of B moved by the
 *     key difference at its onset and has more notes (embellish, R-516);
 *   - P is in the development key throughout (vi in major, iv in minor) and S in the sequence key (ii in major, v in minor);
 *     S's melody is P's moved by the key difference;
 *   - R starts in the home key; A1 and T1 are in the home key throughout;
 *   - B1 and C3 of `sonatina` are in the home key throughout (R-524): their melodies have the onsets and durations of B's
 *     and C1's, each pitch moved by the tonic step and, in a minor piece, mapped into the minor (return_map_pitch), their
 *     RETURN has across_modes; in `sonatina-tonic-major` they are in the tonic major (R-517) with the melody moved by the
 *     key difference;
 *   - C4 starts in the home key; K ends on the tonic.
 * The counts of each row, with the first seed alone and with the tries, and the seconds a seed are printed; docs/sonata.md
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

/* Where each part is in the sections, in order. */
enum { A = 0, T, B, C1, C2, E, P, S, R, A1, T1, B1, C3, C4, K, SECTION_COUNT };

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

/* One key throughout, the given one. */
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

/* The melody of `section` against `source`: the same onsets and durations, each pitch the source's moved by `shift` and, when
   `to` is the minor, lowered on the degrees 4 and 9 semitones above the tonic as a copy into the minor is (R-524). */
static void check_copy(const PiecePlan *plan, NodeId source, NodeId section, int shift, bool minor_map, int tonic) {
    static Event x[MAX_EVENTS];
    static Event y[MAX_EVENTS];
    size_t xn = events(plan, source, PLAN_LANE_MELODY, 0, x);
    size_t yn = events(plan, section, PLAN_LANE_MELODY, 0, y);
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    assert(xn > 0 && xn == yn);
    for (size_t i = 0; i < xn; i++) {
        int pitch = x[i].pitch + shift;
        if (minor_map) {
            const int degree = ((pitch - tonic) % 12 + 12) % 12;
            pitch -= degree == 4 || degree == 9 ? 1 : 0; /* the major third and sixth of the source, lowered in the minor */
        }
        assert(equal(x[i].at, y[i].at) && equal(x[i].length, y[i].length) && y[i].pitch == pitch);
    }
}

/* E keeps every melody note of B at its onset, moved by the key difference, and has more notes. */
static void check_embellished(const PiecePlan *plan, NodeId source, NodeId section, int shift) {
    static Event a[MAX_EVENTS];
    static Event b[MAX_EVENTS];
    size_t an = events(plan, source, PLAN_LANE_MELODY, 0, a);
    size_t bn = events(plan, section, PLAN_LANE_MELODY, 0, b);
    assert(plan_node_state(plan, section) == PLAN_STATE_REALIZED);
    for (size_t i = 0; i < an; i++) {
        bool found = false;
        for (size_t j = 0; j < bn && !found; j++) {
            found = equal(a[i].at, b[j].at) && a[i].pitch + shift == b[j].pitch;
        }
        assert(found);
    }
    assert(bn > an);
}

/* Everything the branch says of a composed piece. */
static void check_piece(const char *form, const PiecePlan *plan, const Row *row) {
    NodeId s[SECTION_COUNT];
    assert(sections_of(plan, s) == SECTION_COUNT);
    const KeyLane lane = read_lane(plan);
    const bool minor = row->mode == KEY_STRUCTURE_MODE_MINOR;
    const bool major_recap = strcmp(form, "sonatina-tonic-major") == 0;
    const int region = minor ? row->tonic + 3 : row->tonic + 7;
    const int elaboration = minor ? row->tonic + 8 : row->tonic + 5;
    /* B, C1 and C2 are in the subordinate key throughout. */
    check_one_key(plan, &lane, s[B], region, KEY_STRUCTURE_MODE_MAJOR);
    check_one_key(plan, &lane, s[C1], region, KEY_STRUCTURE_MODE_MAJOR);
    check_one_key(plan, &lane, s[C2], region, KEY_STRUCTURE_MODE_MAJOR);
    const AnalysisKey at_b = key_at(&lane, plan_node_span(plan, s[B]).start);
    /* E: the elaboration key, B embellished and moved. */
    check_one_key(plan, &lane, s[E], elaboration, KEY_STRUCTURE_MODE_MAJOR);
    const PlanConstraint *back = return_of(plan, s[E]);
    assert(back && back->params.section_return.variation == PLAN_RETURN_EMBELLISH && back->params.section_return.section == s[B]);
    check_embellished(plan, s[B], s[E], shift_between(at_b, key_at(&lane, plan_node_span(plan, s[E]).start)));
    /* P and S: the development and sequence keys; S's melody is P's moved. */
    check_one_key(plan, &lane, s[P], row->tonic + (minor ? 5 : 9), KEY_STRUCTURE_MODE_MINOR);
    check_one_key(plan, &lane, s[S], row->tonic + (minor ? 7 : 2), KEY_STRUCTURE_MODE_MINOR);
    check_copy(plan, s[P], s[S], shift_between(key_at(&lane, plan_node_span(plan, s[P]).start), key_at(&lane, plan_node_span(plan, s[S]).start)),
               false, 0);
    /* R starts home; A1 and T1 are home throughout. */
    assert(is_key(key_at(&lane, plan_node_span(plan, s[R]).start), row->tonic, row->mode));
    check_one_key(plan, &lane, s[A1], row->tonic, row->mode);
    check_one_key(plan, &lane, s[T1], row->tonic, row->mode);
    /* B1 and C3: the home key (the home mode, R-524) or the tonic major (R-517); the melody of B and of C1 moved. */
    const NodeId sources[2] = {s[B], s[C1]};
    const NodeId copies[2] = {s[B1], s[C3]};
    for (size_t i = 0; i < 2; i++) {
        const PlanConstraint *again = return_of(plan, copies[i]);
        assert(again && again->params.section_return.section == sources[i]);
        if (major_recap) {
            assert(again->params.section_return.variation == PLAN_RETURN_EXACT && !again->params.section_return.across_modes);
            check_one_key(plan, &lane, copies[i], row->tonic, KEY_STRUCTURE_MODE_MAJOR);
        } else {
            assert(again->params.section_return.variation == PLAN_RETURN_REHARMONIZE && again->params.section_return.across_modes);
            check_one_key(plan, &lane, copies[i], row->tonic, row->mode);
        }
        const int shift = shift_between(key_at(&lane, plan_node_span(plan, sources[i]).start),
                                        key_at(&lane, plan_node_span(plan, copies[i]).start));
        check_copy(plan, sources[i], copies[i], shift, !major_recap && minor, row->tonic);
    }
    /* C4 starts in the home key; K ends on the tonic. */
    assert(is_key(key_at(&lane, plan_node_span(plan, s[C4]).start), row->tonic, row->mode));
    check_ends_on_tonic(plan, s[K], row);
}

/* One try: the plan of the form in the row from the seed, composed; true when it completes (and then it is checked). */
static bool compose_try(const char *form, const Row *row, unsigned long seed) {
    PieceRequest request;
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    ComposeConfig config;
    memset(&request, 0, sizeof(request));
    request.form = form;
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
        check_piece(form, plan, row);
    } else {
        assert(ran == PIECE_ERR_STUCK);
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return ran == PIECE_OK;
}

static int test_form(const char *form) {
    int failed = 0;
    for (size_t r = 0; r < ROW_COUNT; r++) {
        size_t first = 0;
        size_t composed = 0;
        size_t tries = 0;
        clock_t started = clock();
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            bool done = false;
            for (unsigned long extra = 0; extra < SEED_TRIES && !done; extra++) {
                done = compose_try(form, &ROWS[r], seed + extra);
                tries++;
                first += done && extra == 0;
            }
            composed += done;
        }
        double seconds = (double)(clock() - started) / CLOCKS_PER_SEC / (double)SEEDS;
        printf("%s %s: %zu of %d seeds compose (%zu on the first seed alone, %zu tries), %.1f s a seed\n", form, ROWS[r].name,
               composed, SEEDS, first, tries, seconds);
        if (composed < COMPOSED_LEAST) {
            fprintf(stderr, "%s %s: at least %d of %d seeds should compose\n", form, ROWS[r].name, COMPOSED_LEAST, SEEDS);
            failed = 1;
        }
    }
    return failed;
}

int main(int argc, char **argv) {
    if (argc == 2 && (strcmp(argv[1], "sonatina") == 0 || strcmp(argv[1], "sonatina-tonic-major") == 0)) {
        return test_form(argv[1]);
    }
    fprintf(stderr, "usage: test_sonata_forms sonatina|sonatina-tonic-major\n");
    return 2;
}
