#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include "check/hearing.h"
#include "support/test_support.h"

/*
 * Locking and regenerating (regenerate.h, R-072): a rounded binary of 24 bars in C major 4/4 with an invented motive,
 * seeds 1 to 5. A run that is stuck is tried again with the next seed, up to 8, as the commands do; the seeds that needed
 * it are printed (a section with written melody after it has to join it, so regenerating A is stuck for some seeds).
 *
 *   remove       plan_remove_subtree: the descendants and their lane spans go, ids are not reused, a Given part refuses
 *                it, a plan file renumbers from 1 (quick)
 *   lock         plan_lock on a section, a node and a span; what is still open refuses (quick)
 *   dependents   plan_dependents of a section, a harmony, a bass and a melody span, a motive (quick)
 *   section      lock A, regenerate B: A's payloads are byte-identical, B's harmony, bass and melody are written again,
 *                A' (a return of A) is unchanged
 *   returns      regenerate A: A' is written again and is A's return; with A' locked it stays and A is written again
 *   lanes        regenerate one harmony span, one bass span, one melody span and one idea: A and A' are byte-identical,
 *                nothing of B that comes before the target is written again, B is written again
 *   locked       Given material, a locked section or span, a section with Given in it, and the targets that cannot be
 *                regenerated are refused and change nothing; a Given span a regenerated span depends on is untouched
 *   scope        regenerate B: only B is heard and handed to the Analyzer (A and A' are not open, so not in the outcome, their
 *                notes byte-equal); a full compose_run hands the Analyzer more spans
 *   scope-config ComposeConfig.scope = B on a fresh plan: A and A' are reported out of scope, add nothing to the heard
 *                score, and the plan written is the unscoped run's
 *   determinism  the same plan, target and seed give the same plan and the same records
 *   cli          the lock and regenerate commands, run as processes, with their exit codes
 */

#define SEEDS 5
#define SEED_TRIES 8
#define OUT "regenerate_cli_out.txt"
#define ERR "regenerate_cli_err.txt"

typedef struct {
    PiecePlan *plan;
    NodeId a;
    NodeId b;
    NodeId a_return;
} Piece;

typedef enum {
    TARGET_A,
    TARGET_B,
    TARGET_HARMONY_SPAN,
    TARGET_BASS_SPAN,
    TARGET_MELODY_SPAN,
    TARGET_IDEA
} TargetKind;

static PieceRequest request(void) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = FORM_TEMPLATE_ROUNDED_BINARY;
    request.bars = 24;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    return request;
}

static ComposeConfig config_of(unsigned long seed) {
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    return config;
}

static FillTarget node_target(NodeId id) {
    FillTarget target = {FILL_TARGET_NODE, id};
    return target;
}

static FillTarget span_target(LaneSpanId id) {
    FillTarget target = {FILL_TARGET_LANE_SPAN, id};
    return target;
}

/* Finds A, B and A' in a plan: the first three children of the form. */
static void bind(Piece *piece) {
    NodeId form = plan_root(piece->plan);
    piece->a = plan_node_child_at(piece->plan, form, 0);
    piece->b = plan_node_child_at(piece->plan, form, 1);
    piece->a_return = plan_node_child_at(piece->plan, form, 2);
    assert(strcmp(plan_node_name(piece->plan, piece->a), "A") == 0);
    assert(strcmp(plan_node_name(piece->plan, piece->b), "B") == 0);
    assert(strcmp(plan_node_name(piece->plan, piece->a_return), "A'") == 0);
}

/* A filled piece. */
static Piece fill(unsigned long seed) {
    Piece piece;
    PieceRequest asked = request();
    ComposeConfig config = config_of(seed);
    ComposeOutcome outcome;
    memset(&piece, 0, sizeof(piece));
    assert(piece_build_plan(&asked, &piece.plan) == PIECE_OK);
    assert(compose_run(piece.plan, &config, &outcome) == PIECE_OK);
    compose_outcome_destroy(&outcome);
    bind(&piece);
    return piece;
}

static char *json_of(const PiecePlan *plan) {
    char *text = plan_to_json(plan, NULL);
    assert(text);
    return text;
}

/*
 * The lane spans of a lane (any voice) over a section as text: where they are from the section's start, their state and
 * what they hold. Only the spans that end no later than limit (a time from the start of the plan) when limit is given: a
 * span that crosses it holds a regenerated chord, so its melody is written again.
 */
static char *lane_text_before(const PiecePlan *plan, NodeId section, PlanLaneKind lane, const Rational *limit) {
    size_t capacity = 1 << 16;
    char *text = calloc(capacity, 1);
    size_t used = 0;
    PlanSpan span = plan_node_span(plan, section);
    for (int voice = 0; voice <= 3; voice++) {
        LaneSpanId ids[256];
        size_t found = plan_lane_find(plan, lane, voice, span, ids, 256);
        assert(found <= 256);
        for (size_t i = 0; i < found; i++) {
            PlanSpan at = plan_lane_span_span(plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
            Rational from;
            Rational end;
            assert(plan_span_end(at, &end) == PLAN_OK);
            if (limit && rational_cmp(end, *limit) > 0) {
                continue;
            }
            assert(rational_sub(at.start, span.start, &from) == RATIONAL_OK);
            used += (size_t)snprintf(text + used, capacity - used, "v%d %lld/%lld+%lld/%lld %d:", voice, from.num, from.den,
                                     at.length.num, at.length.den, (int)plan_lane_span_state(plan, ids[i]));
            if (lane == PLAN_LANE_HARMONY) {
                used += (size_t)snprintf(text + used, capacity - used, "%s;", payload->harmony.roman_numeral);
            }
            for (size_t n = 0; lane != PLAN_LANE_HARMONY && n < payload->notes.note_count; n++) {
                used += (size_t)snprintf(text + used, capacity - used, "%d@%lld/%lld,", payload->notes.notes[n].midi,
                                         payload->notes.notes[n].offset.num, payload->notes.notes[n].offset.den);
            }
            assert(used < capacity - 256);
        }
    }
    return text;
}

static char *lane_text(const PiecePlan *plan, NodeId section, PlanLaneKind lane) {
    return lane_text_before(plan, section, lane, NULL);
}

/* Every lane of a section as text. */
static char *section_text(const PiecePlan *plan, NodeId section) {
    PlanLaneKind lanes[4] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY, PLAN_LANE_INNER_VOICE};
    char *parts[4];
    size_t total = 1;
    for (int i = 0; i < 4; i++) {
        parts[i] = lane_text(plan, section, lanes[i]);
        total += strlen(parts[i]) + 2;
    }
    char *text = calloc(total, 1);
    for (int i = 0; i < 4; i++) {
        strcat(text, parts[i]);
        strcat(text, "|");
        free(parts[i]);
    }
    return text;
}

/* The index-th lane span (voice 0) of a lane inside a section. */
static LaneSpanId span_in(const PiecePlan *plan, NodeId section, PlanLaneKind lane, size_t index) {
    LaneSpanId ids[256];
    size_t found = plan_lane_find(plan, lane, 0, plan_node_span(plan, section), ids, 256);
    assert(index < found);
    return ids[index];
}

static size_t spans_in(const PiecePlan *plan, NodeId section, PlanLaneKind lane) {
    return plan_lane_find(plan, lane, 0, plan_node_span(plan, section), NULL, 0);
}

/* The target of a kind in a piece, and the time it starts at. */
static FillTarget pick(const Piece *piece, TargetKind kind, Rational *from) {
    const PiecePlan *plan = piece->plan;
    PlanLaneKind lane = kind == TARGET_HARMONY_SPAN ? PLAN_LANE_HARMONY : kind == TARGET_BASS_SPAN ? PLAN_LANE_BASS : PLAN_LANE_MELODY;
    switch (kind) {
    case TARGET_A:
        *from = plan_node_span(plan, piece->a).start;
        return node_target(piece->a);
    case TARGET_B:
        *from = plan_node_span(plan, piece->b).start;
        return node_target(piece->b);
    case TARGET_IDEA: {
        /* The continuation's second fragment: an idea in the middle of B. */
        NodeId phrase = plan_node_child_at(plan, piece->b, 0);
        NodeId continuation = plan_node_child_at(plan, phrase, 1);
        NodeId idea = plan_node_child_at(plan, continuation, 1);
        *from = plan_node_span(plan, idea).start;
        return node_target(idea);
    }
    default: {
        LaneSpanId id = span_in(plan, piece->b, lane, spans_in(plan, piece->b, lane) / 2);
        *from = plan_lane_span_span(plan, id).start;
        return span_target(id);
    }
    }
}

/*
 * The time the regeneration of a target writes again from: the target's own start, or an earlier one when the closure
 * the target is written again with reaches back (the chords of a phrase model are written together, and the melody
 * over them with them).
 */
static Rational rewritten_from(const PiecePlan *plan, FillTarget target, Rational from) {
    FillTarget *scope = NULL;
    size_t count = 0;
    assert(plan_regenerate_scope(plan, target, &scope, &count) == REGENERATE_OK);
    for (size_t i = 0; i < count; i++) {
        if (scope[i].kind == FILL_TARGET_LANE_SPAN) {
            Rational at = plan_lane_span_span(plan, (LaneSpanId)scope[i].id).start;
            from = rational_cmp(at, from) < 0 ? at : from;
        }
    }
    free(scope);
    return from;
}

/*
 * Regenerates a target of the piece, from seed; a run that is stuck is started again from the piece as it was, with the
 * next seed, up to SEED_TRIES. The piece's plan is replaced by the result (and A, B and A' found again). Returns the
 * result of the last run; *used is the seed it ran with.
 */
static RegenerateResult regenerate_retry(Piece *piece, TargetKind kind, unsigned long seed, ComposeOutcome *outcome,
                                         unsigned long *used) {
    char *snapshot = json_of(piece->plan);
    RegenerateResult result = REGENERATE_ERR_STUCK;
    for (unsigned long attempt = 0; attempt < SEED_TRIES && result == REGENERATE_ERR_STUCK; attempt++) {
        PiecePlan *fresh = NULL;
        Rational from;
        Piece trial;
        ComposeConfig config = config_of(seed + attempt);
        assert(plan_from_json(snapshot, &fresh, NULL) == PLAN_OK);
        trial.plan = fresh;
        bind(&trial);
        compose_outcome_destroy(outcome);
        result = plan_regenerate(fresh, pick(&trial, kind, &from), &config, outcome);
        plan_destroy(piece->plan);
        piece->plan = fresh;
        *used = seed + attempt;
    }
    free(snapshot);
    bind(piece);
    return result;
}

/* ------------------------------------------------------------ remove */

static int test_remove(void) {
    Piece piece = fill(1);
    PiecePlan *plan = piece.plan;
    size_t nodes = plan_node_count(plan);
    size_t spans = plan_lane_span_count(plan);
    NodeId phrase = plan_node_child_at(plan, piece.b, 0);

    /* A Given lane span under B refuses the removal, and nothing changes. */
    LaneSpanId given = span_in(plan, piece.b, PLAN_LANE_MELODY, 0);
    assert(plan_lane_span_set_state(plan, given, PLAN_STATE_GIVEN) == PLAN_OK);
    char *before = json_of(plan);
    assert(plan_remove_subtree(plan, piece.b) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_remove_subtree(plan, 9999) == PLAN_ERR_NOT_FOUND);
    assert(plan_remove_subtree(NULL, piece.b) == PLAN_ERR_NULL);
    assert(plan_node_child_count(plan, piece.b) == 1 && plan_has_node(plan, phrase));
    char *after = json_of(plan);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);

    /* So does a Given node. */
    assert(plan_lane_span_set_state(plan, given, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    plan_destroy(plan);
    piece = fill(1);
    plan = piece.plan;
    phrase = plan_node_child_at(plan, piece.b, 0);
    NodeId continuation = plan_node_child_at(plan, phrase, 1);
    assert(plan_node_set_state(plan, continuation, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_remove_subtree(plan, piece.b) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_has_node(plan, continuation));
    plan_destroy(plan);

    /* Without them the removal goes through: no child, no span of theirs, and the ids stay unused. */
    piece = fill(1);
    plan = piece.plan;
    phrase = plan_node_child_at(plan, piece.b, 0);
    size_t under_b = 0;
    for (LaneSpanId id = 1; id <= spans; id++) {
        for (NodeId up = plan_lane_span_owner(plan, id); up != PLAN_NODE_NONE; up = plan_node_parent(plan, up)) {
            if (up == piece.b && plan_lane_span_owner(plan, id) != piece.b) {
                under_b++;
                break;
            }
        }
    }
    assert(under_b > 0);
    assert(plan_remove_subtree(plan, piece.b) == PLAN_OK);
    assert(plan_node_child_count(plan, piece.b) == 0);
    assert(!plan_has_node(plan, phrase) && plan_has_node(plan, piece.b));
    assert(plan_node_count(plan) == nodes && plan_lane_span_count(plan) == spans);
    size_t alive = 0;
    for (LaneSpanId id = 1; id <= spans; id++) {
        alive += plan_has_lane_span(plan, id);
    }
    assert(alive == spans - under_b);
    assert(plan_node_kind(plan, phrase) == PLAN_NODE_FORM && plan_node_parent(plan, phrase) == PLAN_NODE_NONE);

    /* A node added after takes a new id. */
    NodeId fresh;
    assert(plan_add_node(plan, piece.b, PLAN_NODE_PHRASE, plan_node_span(plan, piece.b), "again", &fresh) == PLAN_OK);
    assert(fresh == nodes + 1);

    /* The file renumbers from 1 and reads back to the same file. */
    char *text = json_of(plan);
    PiecePlan *again = NULL;
    assert(plan_from_json(text, &again, NULL) == PLAN_OK);
    size_t live_nodes = 0;
    size_t live_spans = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        live_nodes += plan_has_node(plan, id);
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        live_spans += plan_has_lane_span(plan, id);
    }
    assert(plan_node_count(again) == live_nodes && plan_lane_span_count(again) == live_spans);
    char *second = json_of(again);
    assert(strcmp(text, second) == 0);
    free(text);
    free(second);
    plan_destroy(again);
    plan_destroy(plan);
    printf("remove ok\n");
    return 0;
}

/* ------------------------------------------------------------ lock */

static size_t count_given(const PiecePlan *plan, NodeId section, size_t *nodes, size_t *open) {
    size_t spans = 0;
    *nodes = 0;
    *open = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        for (NodeId up = id; plan_has_node(plan, id) && up != PLAN_NODE_NONE; up = plan_node_parent(plan, up)) {
            if (up == section) {
                *nodes += plan_node_state(plan, id) == PLAN_STATE_GIVEN;
                *open += plan_node_state(plan, id) != PLAN_STATE_GIVEN;
                break;
            }
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        for (NodeId up = plan_lane_span_owner(plan, id); plan_has_lane_span(plan, id) && up != PLAN_NODE_NONE;
             up = plan_node_parent(plan, up)) {
            if (up == section) {
                spans += plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN;
                *open += plan_lane_span_state(plan, id) != PLAN_STATE_GIVEN;
                break;
            }
        }
    }
    return spans;
}

static int test_lock(void) {
    Piece piece = fill(1);
    PiecePlan *plan = piece.plan;
    size_t nodes;
    size_t open;
    size_t spans = count_given(plan, piece.b, &nodes, &open);
    assert(spans == 0 && nodes == 0 && open > 0);

    /* A section: it, every node under it and every lane span they own. */
    assert(plan_lock(plan, node_target(piece.b)) == PLAN_OK);
    spans = count_given(plan, piece.b, &nodes, &open);
    assert(open == 0 && nodes > 1 && spans > 0);
    assert(plan_node_state(plan, piece.b) == PLAN_STATE_GIVEN);
    /* A is untouched. */
    spans = count_given(plan, piece.a, &nodes, &open);
    assert(spans == 0 && nodes == 0);
    /* Locking again changes nothing; nothing leaves Given. */
    assert(plan_lock(plan, node_target(piece.b)) == PLAN_OK);
    assert(plan_node_set_state(plan, piece.b, PLAN_STATE_CONSTRAINED) == PLAN_ERR_GIVEN_LOCKED);

    /* A node under a section, and a lane span alone. */
    NodeId phrase = plan_node_child_at(plan, piece.a, 0);
    NodeId presentation = plan_node_child_at(plan, phrase, 0);
    assert(plan_lock(plan, node_target(presentation)) == PLAN_OK);
    assert(plan_node_state(plan, presentation) == PLAN_STATE_GIVEN && plan_node_state(plan, phrase) == PLAN_STATE_REALIZED);
    assert(plan_node_state(plan, plan_node_child_at(plan, presentation, 0)) == PLAN_STATE_GIVEN);
    LaneSpanId span = span_in(plan, piece.a_return, PLAN_LANE_BASS, 0);
    assert(plan_lane_span_state(plan, span) == PLAN_STATE_REALIZED);
    assert(plan_lock(plan, span_target(span)) == PLAN_OK);
    assert(plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_state(plan, span_in(plan, piece.a_return, PLAN_LANE_BASS, 1)) == PLAN_STATE_REALIZED);
    assert(plan_lock(plan, node_target(9999)) == PLAN_ERR_NOT_FOUND);
    assert(plan_lock(plan, span_target(99999)) == PLAN_ERR_NOT_FOUND);
    assert(plan_lock(NULL, node_target(piece.b)) == PLAN_ERR_NULL);
    plan_destroy(plan);

    /* What is still open is refused, and nothing changes: an unfilled plan has no written section. */
    PiecePlan *unfilled = NULL;
    PieceRequest asked = request();
    assert(piece_build_plan(&asked, &unfilled) == PIECE_OK);
    NodeId first = plan_node_child_at(unfilled, plan_root(unfilled), 0);
    char *before = json_of(unfilled);
    assert(plan_lock(unfilled, node_target(first)) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_node_state(unfilled, first) == PLAN_STATE_CONSTRAINED);
    NodeId material = plan_node_child_at(unfilled, plan_root(unfilled), plan_node_child_count(unfilled, plan_root(unfilled)) - 1);
    LaneSpanId open_span = PLAN_LANE_SPAN_NONE;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(unfilled); id++) {
        if (plan_lane_span_state(unfilled, id) == PLAN_STATE_CONSTRAINED) {
            open_span = id;
        }
    }
    assert(open_span != PLAN_LANE_SPAN_NONE);
    assert(plan_lock(unfilled, span_target(open_span)) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_lock(unfilled, node_target(material)) == PLAN_ERR_BAD_PAYLOAD);
    char *after = json_of(unfilled);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);
    plan_destroy(unfilled);
    printf("lock ok\n");
    return 0;
}

/* ------------------------------------------------------------ dependents */

static bool has_target(const FillTarget *list, size_t count, FillTarget target) {
    for (size_t i = 0; i < count; i++) {
        if (fill_target_equals(list[i], target)) {
            return true;
        }
    }
    return false;
}

/* Whether a target is in B: a node under it, or a lane span one owns. */
static bool in_section(const PiecePlan *plan, FillTarget target, NodeId section) {
    NodeId node = target.kind == FILL_TARGET_NODE ? (NodeId)target.id : plan_lane_span_owner(plan, (LaneSpanId)target.id);
    for (; node != PLAN_NODE_NONE; node = plan_node_parent(plan, node)) {
        if (node == section) {
            return true;
        }
    }
    return false;
}

static size_t overlapping(const PiecePlan *plan, const FillTarget *list, size_t count, PlanLaneKind lane, PlanSpan span) {
    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        if (list[i].kind == FILL_TARGET_LANE_SPAN && plan_lane_span_lane(plan, (LaneSpanId)list[i].id) == lane) {
            PlanSpan other = plan_lane_span_span(plan, (LaneSpanId)list[i].id);
            Rational end;
            Rational other_end;
            plan_span_end(span, &end);
            plan_span_end(other, &other_end);
            found += rational_cmp(other.start, end) < 0 && rational_cmp(span.start, other_end) < 0;
        }
    }
    return found;
}

static int test_dependents(void) {
    Piece piece = fill(1);
    PiecePlan *plan = piece.plan;
    FillTarget *list = NULL;
    size_t count = 0;

    /* A section: its whole subtree and lanes; A has A' as a dependent too, B has none. */
    assert(plan_dependents(plan, node_target(piece.b), &list, &count) == PLAN_OK);
    size_t subtree_nodes = 0;
    size_t subtree_spans = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        subtree_nodes += id != piece.b && in_section(plan, node_target(id), piece.b);
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        subtree_spans += in_section(plan, span_target(id), piece.b);
    }
    size_t nodes = 0;
    size_t spans = 0;
    for (size_t i = 0; i < count; i++) {
        assert(in_section(plan, list[i], piece.b) && !fill_target_equals(list[i], node_target(piece.b)));
        nodes += list[i].kind == FILL_TARGET_NODE;
        spans += list[i].kind == FILL_TARGET_LANE_SPAN;
        assert(i == 0 || list[i - 1].kind != FILL_TARGET_LANE_SPAN || list[i].kind == FILL_TARGET_LANE_SPAN);
    }
    assert(nodes == subtree_nodes && spans == subtree_spans && nodes > 0 && spans > 0);
    free(list);

    assert(plan_dependents(plan, node_target(piece.a), &list, &count) == PLAN_OK);
    assert(has_target(list, count, node_target(piece.a_return)));
    assert(has_target(list, count, node_target(plan_node_child_at(plan, piece.a_return, 0))));
    assert(!has_target(list, count, node_target(piece.b)) && !has_target(list, count, node_target(piece.a)));
    free(list);

    /* A harmony span: the bass and melody over its time, and the next chord of the section. */
    size_t harmony_count = spans_in(plan, piece.b, PLAN_LANE_HARMONY);
    assert(harmony_count > 3);
    LaneSpanId harmony = span_in(plan, piece.b, PLAN_LANE_HARMONY, 1);
    PlanSpan span = plan_lane_span_span(plan, harmony);
    assert(plan_dependents(plan, span_target(harmony), &list, &count) == PLAN_OK);
    assert(overlapping(plan, list, count, PLAN_LANE_BASS, span) >= 1);
    assert(overlapping(plan, list, count, PLAN_LANE_MELODY, span) >= 1);
    assert(has_target(list, count, span_target(span_in(plan, piece.b, PLAN_LANE_HARMONY, 2))));
    assert(!has_target(list, count, span_target(harmony)));
    size_t bass_count = 0;
    for (size_t i = 0; i < count; i++) {
        bass_count += list[i].kind == FILL_TARGET_LANE_SPAN && plan_lane_span_lane(plan, (LaneSpanId)list[i].id) == PLAN_LANE_BASS;
        assert(list[i].kind == FILL_TARGET_LANE_SPAN && in_section(plan, list[i], piece.b));
    }
    assert(bass_count >= 1);
    free(list);

    /* A Given dependent is left out. */
    LaneSpanId bass = span_in(plan, piece.b, PLAN_LANE_BASS, 1);
    assert(plan_lock(plan, span_target(bass)) == PLAN_OK);
    assert(plan_dependents(plan, span_target(harmony), &list, &count) == PLAN_OK);
    assert(!has_target(list, count, span_target(bass)));
    free(list);

    /* A bass span: the melody over its time, no chord. */
    bass = span_in(plan, piece.b, PLAN_LANE_BASS, 2);
    span = plan_lane_span_span(plan, bass);
    assert(plan_dependents(plan, span_target(bass), &list, &count) == PLAN_OK);
    assert(overlapping(plan, list, count, PLAN_LANE_MELODY, span) >= 1);
    for (size_t i = 0; i < count; i++) {
        assert(plan_lane_span_lane(plan, (LaneSpanId)list[i].id) != PLAN_LANE_HARMONY);
        assert(plan_lane_span_lane(plan, (LaneSpanId)list[i].id) != PLAN_LANE_BASS);
    }
    free(list);

    /* A melody span: the next idea of the section, and nothing across the section's end. */
    LaneSpanId melody = span_in(plan, piece.b, PLAN_LANE_MELODY, 1);
    assert(plan_dependents(plan, span_target(melody), &list, &count) == PLAN_OK);
    assert(count == 1 && has_target(list, count, span_target(span_in(plan, piece.b, PLAN_LANE_MELODY, 2))));
    free(list);
    LaneSpanId last = span_in(plan, piece.b, PLAN_LANE_MELODY, spans_in(plan, piece.b, PLAN_LANE_MELODY) - 1);
    assert(plan_dependents(plan, span_target(last), &list, &count) == PLAN_OK);
    assert(count == 0 && list == NULL);

    /* A motive: the melody spans that name it. */
    LaneSpanId named = span_in(plan, piece.b, PLAN_LANE_MELODY, 1);
    NodeId motive = PLAN_NODE_NONE;
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, named); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, named, i);
        motive = constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF ? constraint->params.motive.motive : motive;
    }
    assert(motive != PLAN_NODE_NONE && plan_node_kind(plan, motive) == PLAN_NODE_MOTIVE);
    assert(plan_dependents(plan, node_target(motive), &list, &count) == PLAN_OK);
    assert(has_target(list, count, span_target(named)));
    for (size_t i = 0; i < count; i++) {
        assert(list[i].kind == FILL_TARGET_LANE_SPAN && plan_lane_span_lane(plan, (LaneSpanId)list[i].id) == PLAN_LANE_MELODY);
    }
    free(list);

    /* An inner voice, meter and key span have none; an unknown target is not found. */
    assert(plan_dependents(plan, node_target(9999), &list, &count) == PLAN_ERR_NOT_FOUND);
    assert(plan_dependents(plan, span_target(1), &list, &count) == PLAN_OK);
    plan_destroy(plan);
    printf("dependents ok\n");
    return 0;
}

/* ------------------------------------------------------------ section */

static int test_section(void) {
    unsigned changed_harmony = 0;
    unsigned changed_bass = 0;
    unsigned changed_melody = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = fill(seed);
        assert(plan_lock(piece.plan, node_target(piece.a)) == PLAN_OK);
        char *a_before = section_text(piece.plan, piece.a);
        char *return_before = section_text(piece.plan, piece.a_return);
        char *harmony_before = lane_text(piece.plan, piece.b, PLAN_LANE_HARMONY);
        char *bass_before = lane_text(piece.plan, piece.b, PLAN_LANE_BASS);
        char *melody_before = lane_text(piece.plan, piece.b, PLAN_LANE_MELODY);
        ComposeOutcome outcome;
        unsigned long used;
        memset(&outcome, 0, sizeof(outcome));
        assert(regenerate_retry(&piece, TARGET_B, seed + 100, &outcome, &used) == REGENERATE_OK);
        char *a_after = section_text(piece.plan, piece.a);
        char *return_after = section_text(piece.plan, piece.a_return);
        char *harmony_after = lane_text(piece.plan, piece.b, PLAN_LANE_HARMONY);
        char *bass_after = lane_text(piece.plan, piece.b, PLAN_LANE_BASS);
        char *melody_after = lane_text(piece.plan, piece.b, PLAN_LANE_MELODY);
        assert(strcmp(a_before, a_after) == 0);
        assert(strcmp(return_before, return_after) == 0);
        assert(plan_node_state(piece.plan, piece.a) == PLAN_STATE_GIVEN);
        assert(plan_node_state(piece.plan, piece.b) == PLAN_STATE_REALIZED);
        assert(outcome.section_count == 1);
        changed_harmony += strcmp(harmony_before, harmony_after) != 0;
        changed_bass += strcmp(bass_before, bass_after) != 0;
        changed_melody += strcmp(melody_before, melody_after) != 0;
        free(a_before);
        free(a_after);
        free(return_before);
        free(return_after);
        free(harmony_before);
        free(harmony_after);
        free(bass_before);
        free(bass_after);
        free(melody_before);
        free(melody_after);
        compose_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    printf("B written again: harmony %u, bass %u, melody %u of %d\n", changed_harmony, changed_bass, changed_melody, SEEDS);
    assert(changed_harmony >= 3 && changed_bass >= 3 && changed_melody >= 3);
    printf("section ok\n");
    return 0;
}

/* ------------------------------------------------------------ scope */

static int test_scope(void) {
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t calls = hearing_analyzer_calls();
        Piece piece = fill(seed);
        size_t full_calls = hearing_analyzer_calls() - calls;
        assert(full_calls > 0);

        /* Regenerating B hears B only (A and A' are not open, so they are not in the outcome) and leaves their notes
           byte-equal. */
        char *a_before = section_text(piece.plan, piece.a);
        char *return_before = section_text(piece.plan, piece.a_return);
        ComposeOutcome outcome;
        unsigned long used;
        memset(&outcome, 0, sizeof(outcome));
        calls = hearing_analyzer_calls();
        assert(regenerate_retry(&piece, TARGET_B, seed + 100, &outcome, &used) == REGENERATE_OK);
        size_t regenerate_calls = hearing_analyzer_calls() - calls;
        char *a_after = section_text(piece.plan, piece.a);
        char *return_after = section_text(piece.plan, piece.a_return);
        assert(strcmp(a_before, a_after) == 0);
        assert(strcmp(return_before, return_after) == 0);
        assert(outcome.section_count == 1);
        assert(outcome.sections[0].section == piece.b && outcome.sections[0].in_scope);
        assert(outcome.sections[0].heard_result == FORM_HEAR_OK);
        printf("seed %lu: Analyzer spans, full run %zu, regenerating B %zu\n", seed, full_calls, regenerate_calls);
        assert(regenerate_calls < full_calls);
        free(a_before);
        free(a_after);
        free(return_before);
        free(return_after);
        compose_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    printf("scope regenerate ok\n");
    return 0;
}

/* A scope given to compose_run: only B is heard, the sections out of scope are reported so and add nothing to the heard
   score, and the written music is the one an unscoped run writes. */
static int test_scope_config(void) {
    unsigned checks_made = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PieceRequest asked = request();
        ComposeConfig config = config_of(seed);
        ComposeOutcome all;
        ComposeOutcome scoped;
        Piece unscoped;
        Piece only_b;
        memset(&unscoped, 0, sizeof(unscoped));
        memset(&only_b, 0, sizeof(only_b));
        assert(piece_build_plan(&asked, &unscoped.plan) == PIECE_OK);
        assert(piece_build_plan(&asked, &only_b.plan) == PIECE_OK);
        bind(&unscoped);
        bind(&only_b);
        assert(compose_run(unscoped.plan, &config, &all) == PIECE_OK);

        NodeId scope[1] = {only_b.b};
        config.scope = scope;
        config.scope_count = 1;
        assert(compose_run(only_b.plan, &config, &scoped) == PIECE_OK);

        assert(scoped.section_count == 3 && all.section_count == 3);
        unsigned long expected = 0;
        for (size_t i = 0; i < 3; i++) {
            assert(all.sections[i].in_scope && all.sections[i].heard_result == FORM_HEAR_OK);
            assert(scoped.sections[i].section == all.sections[i].section);
            if (scoped.sections[i].section == only_b.b) {
                assert(scoped.sections[i].in_scope && scoped.sections[i].heard_result == FORM_HEAR_OK);
                expected = scoped.sections[i].structure.denominator + (scoped.sections[i].requested_form >= 0 ? 1 : 0);
            } else {
                assert(!scoped.sections[i].in_scope);
                assert(scoped.sections[i].heard_result == FORM_HEAR_ERR_NOT_WRITTEN && scoped.sections[i].heard_form < 0);
                assert(!scoped.sections[i].heard_as_requested);
            }
        }
        /* The returns are read from the whole piece either way. */
        assert(scoped.returns_planned == all.returns_planned);
        assert(scoped.score.denominator == expected + scoped.returns_planned);

        /* Hearing less writes nothing else. */
        char *unscoped_json = json_of(unscoped.plan);
        char *scoped_json = json_of(only_b.plan);
        assert(strcmp(unscoped_json, scoped_json) == 0);
        free(unscoped_json);
        free(scoped_json);
        compose_outcome_destroy(&all);
        compose_outcome_destroy(&scoped);
        plan_destroy(unscoped.plan);
        plan_destroy(only_b.plan);

        /* With the Analyzer check, the sections out of scope are not checked. */
        Piece checked;
        ComposeOutcome checked_outcome;
        memset(&checked, 0, sizeof(checked));
        memset(&checked_outcome, 0, sizeof(checked_outcome));
        assert(piece_build_plan(&asked, &checked.plan) == PIECE_OK);
        bind(&checked);
        scope[0] = checked.b;
        config.analyzer_check = true;
        if (compose_run(checked.plan, &config, &checked_outcome) == PIECE_OK) {
            checks_made++;
            assert(checked_outcome.analyzer_check_count == 3);
            for (size_t i = 0; i < 3; i++) {
                if (checked_outcome.sections[i].section != checked.b) {
                    assert(checked_outcome.analyzer_checks[i].planned == 0 && checked_outcome.analyzer_checks[i].item_count == 0);
                }
            }
        }
        compose_outcome_destroy(&checked_outcome);
        plan_destroy(checked.plan);
    }
    printf("analyzer checks with a scope: %u of %d seeds\n", checks_made, SEEDS);
    assert(checks_made >= 1);
    printf("scope config ok\n");
    return 0;
}

/* ------------------------------------------------------------ returns */

static int test_returns(void) {
    unsigned changed = 0;
    unsigned retried = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = fill(seed);
        char *return_before = section_text(piece.plan, piece.a_return);
        char *b_before = section_text(piece.plan, piece.b);
        ComposeOutcome outcome;
        unsigned long used;
        memset(&outcome, 0, sizeof(outcome));
        /* Nothing locked: A' is written again, from the new A. */
        assert(regenerate_retry(&piece, TARGET_A, seed + 200, &outcome, &used) == REGENERATE_OK);
        retried += used != seed + 200;
        char *a_after = section_text(piece.plan, piece.a);
        char *return_after = section_text(piece.plan, piece.a_return);
        char *b_after = section_text(piece.plan, piece.b);
        assert(strcmp(a_after, return_after) == 0);
        assert(strcmp(b_before, b_after) == 0);
        assert(outcome.section_count == 2 && outcome.returns_planned == 1);
        changed += strcmp(return_before, return_after) != 0;
        free(return_before);
        free(b_before);

        /* A' locked: it stays, and A is written again. */
        assert(plan_lock(piece.plan, node_target(piece.a_return)) == PLAN_OK);
        char *locked_before = section_text(piece.plan, piece.a_return);
        char *a_old = section_text(piece.plan, piece.a);
        assert(regenerate_retry(&piece, TARGET_A, seed + 300, &outcome, &used) == REGENERATE_OK);
        retried += used != seed + 300;
        char *locked_after = section_text(piece.plan, piece.a_return);
        char *a_again = section_text(piece.plan, piece.a);
        assert(strcmp(locked_before, locked_after) == 0);
        assert(plan_node_state(piece.plan, piece.a_return) == PLAN_STATE_GIVEN);
        assert(plan_node_state(piece.plan, piece.a) == PLAN_STATE_REALIZED);
        assert(outcome.section_count == 1 && outcome.returns_planned == 0);
        assert(strcmp(a_old, a_again) != 0);
        compose_outcome_destroy(&outcome);
        free(a_after);
        free(return_after);
        free(b_after);
        free(locked_before);
        free(locked_after);
        free(a_old);
        free(a_again);
        plan_destroy(piece.plan);
    }
    printf("A' written again in %u of %d; %u runs needed a later seed\n", changed, SEEDS, retried);
    assert(changed >= 3);
    printf("returns ok\n");
    return 0;
}

/* ------------------------------------------------------------ lanes */

typedef struct {
    const char *name;
    TargetKind kind;
    PlanLaneKind lane;
} LaneCase;

static int test_lanes(void) {
    static const LaneCase CASES[] = {
        {"harmony span", TARGET_HARMONY_SPAN, PLAN_LANE_HARMONY},
        {"bass span", TARGET_BASS_SPAN, PLAN_LANE_BASS},
        {"melody span", TARGET_MELODY_SPAN, PLAN_LANE_MELODY},
        {"idea", TARGET_IDEA, PLAN_LANE_MELODY},
    };
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        unsigned rewritten = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Piece piece = fill(seed);
            Rational from;
            from = rewritten_from(piece.plan, pick(&piece, CASES[c].kind, &from), from);
            char *a_before = section_text(piece.plan, piece.a);
            char *return_before = section_text(piece.plan, piece.a_return);
            char *harmony_early = lane_text_before(piece.plan, piece.b, PLAN_LANE_HARMONY, &from);
            char *bass_early = lane_text_before(piece.plan, piece.b, PLAN_LANE_BASS, &from);
            char *melody_early = lane_text_before(piece.plan, piece.b, PLAN_LANE_MELODY, &from);
            char *b_before = section_text(piece.plan, piece.b);
            ComposeOutcome outcome;
            unsigned long used;
            memset(&outcome, 0, sizeof(outcome));
            assert(regenerate_retry(&piece, CASES[c].kind, seed + 400, &outcome, &used) == REGENERATE_OK);
            char *a_after = section_text(piece.plan, piece.a);
            char *return_after = section_text(piece.plan, piece.a_return);
            char *harmony_after = lane_text_before(piece.plan, piece.b, PLAN_LANE_HARMONY, &from);
            char *bass_after = lane_text_before(piece.plan, piece.b, PLAN_LANE_BASS, &from);
            char *melody_after = lane_text_before(piece.plan, piece.b, PLAN_LANE_MELODY, &from);
            char *b_after = section_text(piece.plan, piece.b);
            /* Nothing outside B is touched. */
            assert(strcmp(a_before, a_after) == 0);
            assert(strcmp(return_before, return_after) == 0);
            /* Nothing of B that comes before the target is written again: a harmony or bass span is written again with
               the chords or notes written from the same open span, which start no later, so only the melody is asked. */
            assert(strcmp(melody_early, melody_after) == 0);
            if (CASES[c].lane != PLAN_LANE_HARMONY && CASES[c].lane != PLAN_LANE_BASS) {
                assert(strcmp(harmony_early, harmony_after) == 0 && strcmp(bass_early, bass_after) == 0);
            }
            rewritten += strcmp(b_before, b_after) != 0;
            compose_outcome_destroy(&outcome);
            free(a_before);
            free(a_after);
            free(return_before);
            free(return_after);
            free(harmony_early);
            free(harmony_after);
            free(bass_early);
            free(bass_after);
            free(melody_early);
            free(melody_after);
            free(b_before);
            free(b_after);
            plan_destroy(piece.plan);
        }
        printf("%s: B written again in %u of %d\n", CASES[c].name, rewritten, SEEDS);
        assert(rewritten >= 1);
    }
    printf("lanes ok\n");
    return 0;
}

/* ------------------------------------------------------------ locked */

static int test_locked(void) {
    Piece piece = fill(1);
    PiecePlan *plan = piece.plan;
    ComposeConfig config = config_of(7);
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));

    /* A locked section, and a locked span: refused, and the plan is as it was. */
    assert(plan_lock(plan, node_target(piece.a)) == PLAN_OK);
    LaneSpanId locked_span = span_in(plan, piece.a_return, PLAN_LANE_HARMONY, 0);
    assert(plan_lock(plan, span_target(locked_span)) == PLAN_OK);
    char *before = json_of(plan);
    assert(plan_regenerate(plan, node_target(piece.a), &config, &outcome) == REGENERATE_ERR_LOCKED);
    assert(plan_regenerate(plan, span_target(locked_span), &config, &outcome) == REGENERATE_ERR_LOCKED);
    assert(plan_regenerate(plan, span_target(span_in(plan, piece.a, PLAN_LANE_BASS, 0)), &config, &outcome) == REGENERATE_ERR_LOCKED);

    /* What cannot be regenerated: the form's meter (Given, so locked), a motive, the material, a target that is not there. */
    LaneSpanId meter = PLAN_LANE_SPAN_NONE;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        meter = plan_lane_span_lane(plan, id) == PLAN_LANE_METER ? id : meter;
    }
    NodeId form = plan_root(plan);
    NodeId material = plan_node_child_at(plan, form, plan_node_child_count(plan, form) - 1);
    NodeId motive = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        motive = plan_node_kind(plan, id) == PLAN_NODE_MOTIVE ? id : motive;
    }
    assert(meter != PLAN_LANE_SPAN_NONE && motive != PLAN_NODE_NONE);
    assert(plan_regenerate(plan, node_target(material), &config, &outcome) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_regenerate(plan, node_target(motive), &config, &outcome) == REGENERATE_ERR_BAD_TARGET);
    assert(plan_regenerate(plan, span_target(meter), &config, &outcome) == REGENERATE_ERR_LOCKED);
    assert(plan_lane_span_set_state(plan, meter, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_regenerate(plan, node_target(9999), &config, &outcome) == REGENERATE_ERR_NOT_FOUND);
    assert(plan_regenerate(plan, span_target(99999), &config, &outcome) == REGENERATE_ERR_NOT_FOUND);
    assert(plan_regenerate(NULL, node_target(piece.b), &config, &outcome) == REGENERATE_ERR_NULL);
    char *after = json_of(plan);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);

    /* A section with a Given span among its ideas cannot be reopened whole. */
    LaneSpanId inside = span_in(plan, piece.b, PLAN_LANE_MELODY, 1);
    assert(plan_lock(plan, span_target(inside)) == PLAN_OK);
    before = json_of(plan);
    assert(plan_regenerate(plan, node_target(piece.b), &config, &outcome) == REGENERATE_ERR_LOCKED);
    after = json_of(plan);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);
    plan_destroy(plan);

    /* A Given span a regenerated span depends on is not changed: the run completes or is stuck, never changes it. */
    unsigned completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        piece = fill(seed);
        plan = piece.plan;
        Rational from;
        FillTarget target = pick(&piece, TARGET_HARMONY_SPAN, &from);
        PlanSpan span = plan_lane_span_span(plan, (LaneSpanId)target.id);
        LaneSpanId ids[64];
        size_t found = plan_lane_find(plan, PLAN_LANE_BASS, 0, span, ids, 64);
        assert(found >= 1);
        LaneSpanId bass = ids[0];
        assert(plan_lock(plan, span_target(bass)) == PLAN_OK);
        const PlanNotes *held = &plan_lane_span_payload(plan, bass)->notes;
        size_t note_count = held->note_count;
        PlanNote *notes = malloc(note_count * sizeof(PlanNote));
        assert(notes && note_count > 0);
        memcpy(notes, held->notes, note_count * sizeof(PlanNote));
        PlanSpan locked_at = plan_lane_span_span(plan, bass);
        config = config_of(seed + 500);
        RegenerateResult result = plan_regenerate(plan, target, &config, &outcome);
        assert(result == REGENERATE_OK || result == REGENERATE_ERR_STUCK);
        assert(plan_lane_span_state(plan, bass) == PLAN_STATE_GIVEN);
        assert(rational_cmp(plan_lane_span_span(plan, bass).start, locked_at.start) == 0);
        const PlanNotes *kept = &plan_lane_span_payload(plan, bass)->notes;
        assert(kept->note_count == note_count);
        for (size_t n = 0; n < note_count; n++) {
            assert(kept->notes[n].midi == notes[n].midi);
            assert(rational_cmp(kept->notes[n].offset, notes[n].offset) == 0);
            assert(rational_cmp(kept->notes[n].duration, notes[n].duration) == 0);
        }
        completed += result == REGENERATE_OK;
        free(notes);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("with a Given bass span: completed %u of %d\n", completed, SEEDS);
    printf("locked ok\n");
    return 0;
}

/* ------------------------------------------------------------ determinism */

static char *records_of(const ComposeOutcome *outcome) {
    size_t total = 1;
    char *parts[COMPOSE_STAGE_COUNT] = {0};
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        if (outcome->records[stage]) {
            parts[stage] = fill_record_to_json(outcome->records[stage]);
            assert(parts[stage]);
            total += strlen(parts[stage]) + 2;
        }
    }
    char *text = calloc(total, 1);
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        if (parts[stage]) {
            strcat(text, parts[stage]);
            strcat(text, "|");
            free(parts[stage]);
        }
    }
    return text;
}

static int test_determinism(void) {
    static const TargetKind KINDS[] = {TARGET_B, TARGET_A, TARGET_HARMONY_SPAN, TARGET_MELODY_SPAN, TARGET_IDEA};
    Piece piece = fill(1);
    char *snapshot = json_of(piece.plan);
    plan_destroy(piece.plan);
    for (size_t k = 0; k < sizeof(KINDS) / sizeof(KINDS[0]); k++) {
        char *plans[2];
        char *records[2];
        RegenerateResult results[2];
        for (int run = 0; run < 2; run++) {
            Piece again;
            Rational from;
            ComposeOutcome outcome;
            ComposeConfig config = config_of(9);
            memset(&outcome, 0, sizeof(outcome));
            assert(plan_from_json(snapshot, &again.plan, NULL) == PLAN_OK);
            bind(&again);
            results[run] = plan_regenerate(again.plan, pick(&again, KINDS[k], &from), &config, &outcome);
            assert(results[run] == REGENERATE_OK || results[run] == REGENERATE_ERR_STUCK);
            plans[run] = json_of(again.plan);
            records[run] = records_of(&outcome);
            compose_outcome_destroy(&outcome);
            plan_destroy(again.plan);
        }
        assert(results[0] == results[1]);
        assert(strcmp(plans[0], plans[1]) == 0);
        assert(strcmp(records[0], records[1]) == 0);
        assert(strlen(records[0]) > 0);
        free(plans[0]);
        free(plans[1]);
        free(records[0]);
        free(records[1]);
    }
    free(snapshot);
    printf("determinism ok\n");
    return 0;
}

/* ------------------------------------------------------------ cli */

static int run(const char *command, const char *arguments) {
    char line[4096];
    snprintf(line, sizeof(line), "\"%s\" %s %s >" OUT " 2>" ERR, COMPOSER_CLI_PATH, command, arguments);
    int status = system(line);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

static void expect_success(const char *command, const char *arguments, const char *message) {
    int status = run(command, arguments);
    char *err = test_read_file(ERR, NULL);
    char *out = test_read_file(OUT, NULL);
    if (status != 0 || strstr(out, message) == NULL) {
        fprintf(stderr, "'%s %s': expected 0 with '%s', got %d\nout: %s\nerr: %s\n", command, arguments, message, status, out, err);
    }
    assert(status == 0 && strstr(out, message) != NULL);
    free(err);
    free(out);
}

static void expect_failure(const char *command, const char *arguments, int status, const char *message) {
    int got = run(command, arguments);
    char *err = test_read_file(ERR, NULL);
    if (got != status || strstr(err, message) == NULL) {
        fprintf(stderr, "'%s %s': expected %d with '%s', got %d: %s\n", command, arguments, status, message, got, err);
    }
    assert(got == status && strstr(err, message) != NULL && strncmp(err, "Error: ", 7) == 0);
    free(err);
}

/* The text of a section of a plan file, by the child index in the form. */
static char *file_section_text(const char *path, size_t index) {
    PiecePlan *plan = NULL;
    assert(plan_read_json_file(path, &plan, NULL) == PLAN_OK);
    char *text = section_text(plan, plan_node_child_at(plan, plan_root(plan), index));
    plan_destroy(plan);
    return text;
}

static int test_cli(void) {
    remove("regenerate_cli_plan.json");
    expect_success("plan", "--form rounded-binary --key C --meter 4/4 --bars 24 --seed 1 --out regenerate_cli_plan.json", "sections");
    expect_success("fill", "regenerate_cli_plan.json --seed 1 --out regenerate_cli_filled.json", "wrote regenerate_cli_filled.json");

    /* lock */
    expect_success("lock", "regenerate_cli_filled.json --section A --out regenerate_cli_locked.json", "locked section A:");
    PiecePlan *locked = NULL;
    assert(plan_read_json_file("regenerate_cli_locked.json", &locked, NULL) == PLAN_OK);
    assert(plan_node_state(locked, plan_node_child_at(locked, plan_root(locked), 0)) == PLAN_STATE_GIVEN);
    assert(plan_node_state(locked, plan_node_child_at(locked, plan_root(locked), 1)) == PLAN_STATE_REALIZED);
    plan_destroy(locked);
    expect_success("lock", "regenerate_cli_filled.json --node 3 --lane bass --out regenerate_cli_bass.json", "the bass of section B");
    expect_failure("lock", "regenerate_cli_plan.json --section A --out regenerate_cli_x.json", 2, "not written yet");
    expect_failure("lock", "regenerate_cli_filled.json --section Z --out regenerate_cli_x.json", 2, "no section 'Z'");
    expect_failure("lock", "regenerate_cli_filled.json --node 9999 --out regenerate_cli_x.json", 2, "no node 9999");
    expect_failure("lock", "regenerate_cli_filled.json --out regenerate_cli_x.json", 1, "needs a node");
    expect_failure("lock", "regenerate_cli_filled.json --node 2 --section A --out regenerate_cli_x.json", 1, "not both");
    expect_failure("lock", "regenerate_cli_filled.json --node 2", 1, "needs --out");
    expect_failure("lock", "regenerate_cli_filled.json --node 2 --lane sax --out regenerate_cli_x.json", 1, "--lane must be");
    expect_failure("lock", "regenerate_cli_filled.json --node 2 --bogus --out regenerate_cli_x.json", 1, "unknown option");
    expect_failure("lock", "regenerate_cli_missing.json --node 2 --out regenerate_cli_x.json", 2, "cannot read plan");
    assert(!test_exists("regenerate_cli_x.json"));

    /* regenerate: A is locked, B is written again with the seed, and the files are written */
    remove("regenerate_cli_b.mid");
    remove("regenerate_cli_b.xml");
    expect_success("regenerate",
                   "regenerate_cli_locked.json --section B --seed 5 --out regenerate_cli_b.json --midi regenerate_cli_b.mid "
                   "--musicxml regenerate_cli_b.xml",
                   "reopened sections B");
    char *out = test_read_file(OUT, NULL);
    assert(strstr(out, "seed 5") != NULL && strstr(out, "sections (form and cadences heard)") != NULL);
    assert(strstr(out, "returns heard") != NULL && strstr(out, "wrote regenerate_cli_b.json") != NULL);
    free(out);
    assert(test_exists("regenerate_cli_b.mid") && test_exists("regenerate_cli_b.xml"));
    char *a_before = file_section_text("regenerate_cli_locked.json", 0);
    char *a_after = file_section_text("regenerate_cli_b.json", 0);
    char *b_before = file_section_text("regenerate_cli_locked.json", 1);
    char *b_after = file_section_text("regenerate_cli_b.json", 1);
    char *return_before = file_section_text("regenerate_cli_locked.json", 2);
    char *return_after = file_section_text("regenerate_cli_b.json", 2);
    assert(strcmp(a_before, a_after) == 0 && strcmp(return_before, return_after) == 0 && strcmp(b_before, b_after) != 0);
    free(a_before);
    free(a_after);
    free(b_before);
    free(b_after);
    free(return_before);
    free(return_after);

    /* the same plan, node and seed give the same bytes */
    expect_success("regenerate", "regenerate_cli_locked.json --section B --seed 5 --out regenerate_cli_b2.json", "seed 5");
    char *first = test_read_file("regenerate_cli_b.json", NULL);
    char *second = test_read_file("regenerate_cli_b2.json", NULL);
    assert(strcmp(first, second) == 0);
    free(first);
    free(second);

    /* a lane, by node id, as JSON */
    expect_success("regenerate", "regenerate_cli_locked.json --node 3 --lane harmony --seed 2 --out regenerate_cli_h.json --format json",
                   "{\"seed\":2,\"realized\":false,\"reopened\":{\"sections\":[],\"spans\":{\"harmony\":");
    expect_success("regenerate", "regenerate_cli_locked.json --section B --lane melody --seed 2 --out regenerate_cli_m.json",
                   "reopened lane spans: melody");

    /* refused: a locked section, and the usage errors */
    expect_failure("regenerate", "regenerate_cli_locked.json --section A --out regenerate_cli_x.json", 2, "Given");
    expect_failure("regenerate", "regenerate_cli_locked.json --section Z --out regenerate_cli_x.json", 2, "no section 'Z'");
    expect_failure("regenerate", "regenerate_cli_locked.json --node 9999 --out regenerate_cli_x.json", 2, "no node 9999");
    expect_failure("regenerate", "regenerate_cli_locked.json --section B", 1, "needs --out");
    expect_failure("regenerate", "regenerate_cli_locked.json --section B --seed x --out regenerate_cli_x.json", 1, "--seed must be");
    expect_failure("regenerate", "regenerate_cli_locked.json --section B --format xml --out regenerate_cli_x.json", 1, "--format must be");
    expect_failure("regenerate", "--section B --out regenerate_cli_x.json", 1, "needs a plan file");
    expect_failure("regenerate", "regenerate_cli_missing.json --section B --out regenerate_cli_x.json", 2, "cannot read plan");
    expect_failure("regenerate", "regenerate_cli_locked.json --section B --out regenerate_cli_x.json --lane sax", 1, "--lane must be");
    assert(!test_exists("regenerate_cli_x.json"));
    printf("cli ok\n");
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "remove") == 0) {
        return test_remove();
    }
    if (strcmp(mode, "lock") == 0) {
        return test_lock();
    }
    if (strcmp(mode, "dependents") == 0) {
        return test_dependents();
    }
    if (strcmp(mode, "section") == 0) {
        return test_section();
    }
    if (strcmp(mode, "scope") == 0) {
        return test_scope();
    }
    if (strcmp(mode, "scope-config") == 0) {
        return test_scope_config();
    }
    if (strcmp(mode, "returns") == 0) {
        return test_returns();
    }
    if (strcmp(mode, "lanes") == 0) {
        return test_lanes();
    }
    if (strcmp(mode, "locked") == 0) {
        return test_locked();
    }
    if (strcmp(mode, "determinism") == 0) {
        return test_determinism();
    }
    if (strcmp(mode, "cli") == 0) {
        return test_cli();
    }
    fprintf(stderr, "usage: test_regenerate remove|lock|dependents|section|scope|scope-config|returns|lanes|locked|determinism|cli\n");
    return 1;
}
