#include <string.h>

#include <harmonic_function.h>
#include <harmony_functions.h>
#include <harmony_related_key.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

/* The most bars a phrase can span, and the most barlines searched before a phrase; guards for the loops below. */
#define MAX_PHRASE_BARS 1024
#define MAX_BARLINES 1000000

static PlanResult time_scaled(PlanTime time, size_t count, PlanTime *out) {
    return plan_time_make(time.num * (long)count, time.den, out);
}

static bool span_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind, PlanConstraint *out) {
    size_t count = plan_lane_span_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            *out = *constraint;
            return true;
        }
    }
    return false;
}

static bool node_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind, PlanConstraint *out) {
    size_t count = plan_node_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            *out = *constraint;
            return true;
        }
    }
    return false;
}

bool harmony_find_constraint(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanConstraintKind kind,
                             PlanConstraint *out) {
    return span_constraint(plan, id, kind, out) || node_constraint(plan, owner, kind, out);
}

bool harmony_span_has_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind) {
    PlanConstraint unused;
    return span_constraint(plan, id, kind, &unused);
}

bool harmony_find_key(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKey *out) {
    LaneSpanId keys[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, span, keys, 2);
    if (found == 1) {
        if (!plan_span_contains(plan_lane_span_span(plan, keys[0]), span)) {
            return false;
        }
        *out = plan_lane_span_payload(plan, keys[0])->key;
        return true;
    }
    PlanConstraint constraint;
    if (found == 0 && harmony_find_constraint(plan, id, owner, PLAN_CONSTRAINT_KEY, &constraint)) {
        *out = constraint.params.key;
        return true;
    }
    return false;
}

/* The most key spans looked through before a point of the piece. */
#define MAX_KEY_SPANS 32

bool harmony_key_before(const PiecePlan *plan, PlanTime start, PlanKey *out) {
    LaneSpanId spans[MAX_KEY_SPANS];
    PlanSpan before = {{0, 1}, {0, 1}};
    if (start.num <= 0 || plan_time_make(start.num, start.den, &before.length) != PLAN_OK) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, before, spans, MAX_KEY_SPANS);
    if (found == 0 || found > MAX_KEY_SPANS) {
        return false;
    }
    PlanTime end;
    if (plan_span_end(plan_lane_span_span(plan, spans[found - 1]), &end) != PLAN_OK ||
        plan_time_compare(end, start) != 0) {
        return false;
    }
    *out = plan_lane_span_payload(plan, spans[found - 1])->key;
    return true;
}

/* The closely related key of `from` that is the key `to`. */
static bool related_key_of(PlanKey from, PlanKey to, HarmonyMode *home, HarmonyRelatedKey *out) {
    *home = from.mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
    int semitone = ((to.tonic_pitch_class - from.tonic_pitch_class) % 12 + 12) % 12;
    HarmonyMode mode = to.mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
    for (int i = 0; i < harmony_related_key_count(*home); i++) {
        HarmonyRelatedKey key;
        if (harmony_related_key_at(*home, i, &key) && key.mode == mode &&
            harmony_related_key_semitone(*home, &key) == semitone) {
            *out = key;
            return true;
        }
    }
    return false;
}

/* A chord on the degree in the key, a triad in root position. */
static HarmonyChord triad_on(int degree) {
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = degree;
    return chord;
}

size_t harmony_pivot_degrees(PlanKey from, PlanKey to, int degrees[HARMONY_PIVOT_MAX]) {
    HarmonyMode home;
    HarmonyRelatedKey related;
    HarmonyPivot pivots[HARMONY_PIVOT_MAX];
    int ranks[HARMONY_PIVOT_MAX];
    if (!related_key_of(from, to, &home, &related)) {
        return 0;
    }
    int found = harmony_pivot_find(home, &related, pivots);
    size_t count = 0;
    for (int i = 0; i < found; i++) {
        HarmonyPivotRank rank;
        HarmonyChord chord = triad_on(pivots[i].new_degree);
        HarmonicFunction function;
        /* A phrase cannot open on a dominant, a diminished triad (vii in major, ii in minor) or the minor v. */
        bool diminished = to.mode == PLAN_MODE_MINOR ? pivots[i].new_degree == 2 : pivots[i].new_degree == 7;
        bool minor_v = to.mode == PLAN_MODE_MINOR && (pivots[i].new_degree == 5 || pivots[i].new_degree == 7);
        if (!harmony_pivot_rank(home, &related, &pivots[i], &rank) || rank == HARMONY_PIVOT_POOR || diminished ||
            minor_v ||
            !harmony_function_of(related.mode, &chord, &function) || function == HARMONIC_FUNCTION_DOMINANT) {
            continue;
        }
        /* Inserted by rank, the earlier of two equal ones first. */
        size_t at = count;
        while (at > 0 && ranks[at - 1] > (int)rank) {
            degrees[at] = degrees[at - 1];
            ranks[at] = ranks[at - 1];
            at--;
        }
        degrees[at] = pivots[i].new_degree;
        ranks[at] = (int)rank;
        count++;
    }
    return count;
}

bool harmony_pivot_chord_valid(PlanKey from, PlanKey to, const HarmonyChord *chord) {
    HarmonyMode home;
    HarmonyRelatedKey related;
    HarmonyPivot pivots[HARMONY_PIVOT_MAX];
    if (!chord || chord->inversion != 0 || chord->seventh || !related_key_of(from, to, &home, &related)) {
        return false;
    }
    int found = harmony_pivot_find(home, &related, pivots);
    for (int i = 0; i < found; i++) {
        if (pivots[i].new_degree == chord->degree && harmony_pivot_valid(home, &related, &pivots[i])) {
            return true;
        }
    }
    return false;
}

bool harmony_find_mode(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKeyMode *out) {
    PlanKey key;
    if (!harmony_find_key(plan, id, owner, span, &key)) {
        return false;
    }
    *out = key.mode;
    return true;
}

/* a minus b, which must not be later than a. */
static bool time_since(PlanTime a, PlanTime b, PlanTime *out) {
    return plan_time_make(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den, out) == PLAN_OK && out->num >= 0;
}

/* Whether the time is a whole number of beats of the meter (a beat is 1/beat_unit of a whole note). */
static bool on_beat(PlanTime time, int beat_unit) {
    return time.den != 0 && (time.num * (long)beat_unit) % (long)time.den == 0;
}

/* The whole times `unit` goes into `time`; unit must be positive. */
static size_t times_in(PlanTime time, PlanTime unit) {
    return (size_t)((time.num * (long)unit.den) / ((long)time.den * unit.num));
}

bool harmony_read_metre(const PiecePlan *plan, PlanSpan span, PhraseMetre *out) {
    PhraseMetre metre;
    LaneSpanId meters[2];
    if (!plan || !out || plan_lane_find(plan, PLAN_LANE_METER, 0, span, meters, 2) != 1) {
        return false;
    }
    PlanSpan meter_span = plan_lane_span_span(plan, meters[0]);
    if (!plan_span_contains(meter_span, span)) {
        return false;
    }
    memset(&metre, 0, sizeof(metre));
    metre.meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (plan_time_make(metre.meter.beats_per_bar, (unsigned long)metre.meter.beat_unit, &metre.bar_length) !=
            PLAN_OK ||
        !harmony_rhythm_cells_per_bar(&metre.meter, &metre.cells_per_bar) ||
        plan_time_make(metre.bar_length.num, metre.bar_length.den * metre.cells_per_bar, &metre.cell_length) !=
            PLAN_OK) {
        return false;
    }

    /* The span starts and ends on a beat of the meter span: its start is `offset` after the last barline at or before it. */
    PlanTime barline = meter_span.start;
    for (size_t bar = 0;; bar++) {
        PlanTime next;
        if (bar >= MAX_BARLINES || plan_time_add(barline, metre.bar_length, &next) != PLAN_OK) {
            return false;
        }
        if (plan_time_compare(next, span.start) > 0) {
            break;
        }
        barline = next;
    }
    PlanTime offset;
    PlanTime end;
    if (span.length.num <= 0 || !time_since(span.start, barline, &offset) || !on_beat(offset, metre.meter.beat_unit) ||
        !on_beat(span.length, metre.meter.beat_unit) || plan_time_add(offset, span.length, &end) != PLAN_OK) {
        return false;
    }
    /* The bars the span touches: those it ends in, counting a barline it ends on as the end of the bar before. */
    PlanTime bars_length = {0, 1};
    metre.bars = 0;
    while (plan_time_compare(bars_length, end) < 0) {
        if (metre.bars >= MAX_PHRASE_BARS || plan_time_add(bars_length, metre.bar_length, &bars_length) != PLAN_OK) {
            return false;
        }
        metre.bars++;
    }

    /* A span that starts inside a cell has a lead; what is left is whole cells and a tail. */
    PlanTime into_cell;
    PlanTime whole_cells;
    PlanTime remaining = span.length;
    size_t cells_before = times_in(offset, metre.cell_length);
    if (plan_time_make(metre.cell_length.num * (long)cells_before, metre.cell_length.den, &whole_cells) != PLAN_OK ||
        !time_since(offset, whole_cells, &into_cell)) {
        return false;
    }
    metre.lead = (PlanTime){0, 1};
    if (into_cell.num > 0) {
        PlanTime to_boundary;
        if (!time_since(metre.cell_length, into_cell, &to_boundary)) {
            return false;
        }
        metre.lead = plan_time_compare(to_boundary, span.length) < 0 ? to_boundary : span.length;
        if (!time_since(span.length, metre.lead, &remaining)) {
            return false;
        }
    }
    size_t whole = times_in(remaining, metre.cell_length);
    PlanTime whole_length;
    metre.tail = (PlanTime){0, 1};
    if (plan_time_make(metre.cell_length.num * (long)whole, metre.cell_length.den, &whole_length) != PLAN_OK ||
        !time_since(remaining, whole_length, &metre.tail)) {
        return false;
    }
    metre.cells = (metre.lead.num > 0 ? 1 : 0) + whole + (metre.tail.num > 0 ? 1 : 0);
    if (metre.cells == 0 || metre.cells > MAX_PHRASE_BARS * metre.cells_per_bar + 2) {
        return false;
    }
    metre.start = span.start;
    metre.offset = offset;
    *out = metre;
    return true;
}

bool harmony_metre_cell_offset(const PhraseMetre *metre, size_t cell, PlanTime *out) {
    size_t lead_cells = metre->lead.num > 0 ? 1 : 0;
    size_t tail_cells = metre->tail.num > 0 ? 1 : 0;
    if (cell > metre->cells) {
        return false;
    }
    if (cell == 0) {
        *out = (PlanTime){0, 1};
        return true;
    }
    /* Past the lead, the cells run on from it; the end of a span with a tail is the tail's length on. */
    size_t whole = cell == metre->cells && tail_cells ? cell - 1 - lead_cells : cell - lead_cells;
    PlanTime run;
    PlanTime sum;
    if (plan_time_make(metre->cell_length.num * (long)whole, metre->cell_length.den, &run) != PLAN_OK ||
        plan_time_add(metre->lead, run, &sum) != PLAN_OK) {
        return false;
    }
    if (cell == metre->cells && tail_cells) {
        return plan_time_add(sum, metre->tail, out) == PLAN_OK;
    }
    *out = sum;
    return true;
}

PlanResult harmony_metre_cells_span(const PhraseMetre *metre, size_t first, size_t cells, PlanSpan *out) {
    PlanTime from;
    PlanTime to;
    if (!harmony_metre_cell_offset(metre, first, &from) || !harmony_metre_cell_offset(metre, first + cells, &to) ||
        !time_since(to, from, &out->length)) {
        return PLAN_ERR_BAD_INDEX;
    }
    return plan_time_add(metre->start, from, &out->start);
}

bool harmony_metre_cell_position(const PhraseMetre *metre, size_t cell, size_t *bar, PlanTime *in_bar) {
    PlanTime offset;
    PlanTime from_barline;
    PlanTime whole_bars;
    if (!harmony_metre_cell_offset(metre, cell, &offset) ||
        plan_time_add(metre->offset, offset, &from_barline) != PLAN_OK) {
        return false;
    }
    size_t bars = times_in(from_barline, metre->bar_length);
    if (plan_time_make(metre->bar_length.num * (long)bars, metre->bar_length.den, &whole_bars) != PLAN_OK ||
        !time_since(from_barline, whole_bars, in_bar)) {
        return false;
    }
    *bar = bars;
    return true;
}

bool harmony_metre_cell_on_downbeat(const PhraseMetre *metre, size_t cell) {
    size_t bar;
    PlanTime in_bar;
    return harmony_metre_cell_position(metre, cell, &bar, &in_bar) && in_bar.num == 0;
}

bool harmony_metre_cell_is_part(const PhraseMetre *metre, size_t cell) {
    return (metre->lead.num > 0 && cell == 0) || (metre->tail.num > 0 && cell + 1 == metre->cells);
}

BeatStrength harmony_metre_cell_strength(const PhraseMetre *metre, size_t cell) {
    if (harmony_metre_cell_on_downbeat(metre, cell)) {
        return BEAT_STRENGTH_STRONG;
    }
    return harmony_metre_cell_is_part(metre, cell) ? BEAT_STRENGTH_WEAK : BEAT_STRENGTH_MEDIUM;
}

bool harmony_metre_find_cells(const PhraseMetre *metre, PlanSpan span, size_t *first, size_t *count) {
    PlanTime from;
    PlanTime to;
    PlanTime end;
    if (!time_since(span.start, metre->start, &from) || plan_span_end(span, &end) != PLAN_OK ||
        !time_since(end, metre->start, &to)) {
        return false;
    }
    bool found_first = false;
    for (size_t cell = 0; cell <= metre->cells; cell++) {
        PlanTime at;
        if (!harmony_metre_cell_offset(metre, cell, &at)) {
            return false;
        }
        int against_from = plan_time_compare(at, from);
        if (against_from == 0) {
            *first = cell;
            found_first = true;
        }
        int against_to = plan_time_compare(at, to);
        if (against_to == 0) {
            if (!found_first || cell <= *first) {
                return false;
            }
            *count = cell - *first;
            return true;
        }
        if (against_to > 0) {
            return false;
        }
    }
    return false;
}

size_t harmony_metre_last_cell(const PhraseMetre *metre) {
    /* The last downbeat the span holds starts the last bar portion; a span with none in it starts the last chord on the
       first cell boundary after the lead. */
    size_t first = metre->lead.num > 0 ? 1 : 0;
    if (first >= metre->cells) {
        return metre->cells - 1;
    }
    for (size_t cell = metre->cells; cell-- > first;) {
        if (harmony_metre_cell_on_downbeat(metre, cell)) {
            return cell;
        }
    }
    return first;
}

static bool read_request(const PiecePlan *plan, LaneSpanId target, bool prolongation, bool around_given,
                         PhraseRequest *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY) {
        return false;
    }
    PhraseRequest request;
    PhraseMetre metre;
    memset(&request, 0, sizeof(request));
    request.target = target;
    request.owner = plan_lane_span_owner(plan, target);
    request.span = plan_lane_span_span(plan, target);
    if (!harmony_read_metre(plan, request.span, &metre) || harmony_span_has_constraint(plan, target, PLAN_CONSTRAINT_EMBELLISH)) {
        return false;
    }
    request.meter = metre.meter;
    request.bar_length = metre.bar_length;
    request.bars = metre.bars;
    request.cells_per_bar = metre.cells_per_bar;
    request.cell_length = metre.cell_length;
    request.cells = metre.cells;
    request.lead = metre.lead;
    request.tail = metre.tail;
    request.metre = metre;
    /* A Given bass over the span means the harmony is to be found from the bass (harmony_octave.h), unless this operator
       works around Given material: it writes chords that stand on the bass. */
    LaneSpanId basses[HARMONY_MAX_CHORDS];
    size_t bass_count = plan_lane_find(plan, PLAN_LANE_BASS, 0, request.span, basses, HARMONY_MAX_CHORDS);
    for (size_t i = 0; !around_given && i < bass_count && i < HARMONY_MAX_CHORDS; i++) {
        if (plan_lane_span_state(plan, basses[i]) == PLAN_STATE_GIVEN) {
            return false;
        }
    }

    PlanConstraint constraint;
    request.has_shape = harmony_find_constraint(plan, target, request.owner, PLAN_CONSTRAINT_HARMONY_SHAPE, &constraint);
    if (request.has_shape) {
        request.shape = constraint.params.harmony_shape;
    }
    /* A prolongation is the prolongation operator's, and nothing else is. */
    if (prolongation != (request.has_shape && request.shape.kind == PLAN_HARMONY_SHAPE_PROLONGATION)) {
        return false;
    }
    if (prolongation) {
        request.cadence = PLAN_CADENCE_HC;
    } else {
        if (!harmony_find_constraint(plan, target, request.owner, PLAN_CONSTRAINT_CADENCE, &constraint)) {
            return false;
        }
        request.cadence = constraint.params.cadence;
    }
    if (!harmony_find_key(plan, target, request.owner, request.span, &request.key)) {
        return false;
    }
    request.mode = request.key.mode;
    if (request.has_shape && request.shape.starts_on == PLAN_HARMONY_ANCHOR_PIVOT) {
        /* A pivot opens a phrase model on a key change; a prolongation does not open one. */
        request.pivot = true;
        if (prolongation || !harmony_key_before(plan, request.span.start, &request.from_key)) {
            return false;
        }
        request.pivot_count = harmony_pivot_degrees(request.from_key, request.key, request.pivot_degrees);
        if (request.pivot_count == 0) {
            return false;
        }
    }
    request.around_given = around_given;
    if (around_given) {
        harmony_given_read(plan, &request);
        /* Given material is read and heard on whole cells: not around a span with part cells. */
        if (request.has_given && (request.lead.num > 0 || request.tail.num > 0)) {
            return false;
        }
    }
    *out = request;
    return true;
}

bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    return read_request(plan, target, false, false, out);
}

bool harmony_prolong_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    return read_request(plan, target, true, false, out);
}

bool harmony_phrase_read_request_given(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    return read_request(plan, target, false, true, out);
}

bool harmony_prolong_read_request_given(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    return read_request(plan, target, true, true, out);
}

PlanResult harmony_request_cells_span(const PhraseRequest *request, size_t first_cell, size_t cells, PlanSpan *out) {
    return harmony_metre_cells_span(&request->metre, first_cell, cells, out);
}

PlanResult harmony_cells_span(PlanTime start, PlanTime cell_length, size_t first_cell, size_t cells, PlanSpan *out) {
    PlanTime offset;
    PlanResult result = time_scaled(cell_length, first_cell, &offset);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_time_add(start, offset, &out->start);
    if (result != PLAN_OK) {
        return result;
    }
    return time_scaled(cell_length, cells, &out->length);
}

bool harmony_count_cells_or_zero(PlanTime cell_length, PlanTime length, size_t *cells) {
    if (length.num == 0) {
        *cells = 0;
        return true;
    }
    return length.num > 0 && harmony_count_cells(cell_length, length, cells);
}

bool harmony_read_cells(const PiecePlan *plan, PlanSpan span, CellMetre *out) {
    LaneSpanId meters[2];
    CellMetre metre;
    if (!plan || !out || plan_lane_find(plan, PLAN_LANE_METER, 0, span, meters, 2) != 1) {
        return false;
    }
    PlanSpan meter_span = plan_lane_span_span(plan, meters[0]);
    if (!plan_span_contains(meter_span, span)) {
        return false;
    }
    metre.meter = plan_lane_span_payload(plan, meters[0])->meter;
    PlanTime before = {-meter_span.start.num, meter_span.start.den};
    PlanTime offset;
    if (plan_time_make(metre.meter.beats_per_bar, (unsigned long)metre.meter.beat_unit, &metre.bar_length) != PLAN_OK ||
        !harmony_rhythm_cells_per_bar(&metre.meter, &metre.cells_per_bar) ||
        plan_time_make(metre.bar_length.num, metre.bar_length.den * metre.cells_per_bar, &metre.cell_length) !=
            PLAN_OK ||
        plan_time_add(span.start, before, &offset) != PLAN_OK ||
        !harmony_count_cells_or_zero(metre.cell_length, offset, &metre.first_cell) ||
        !harmony_count_cells(metre.cell_length, span.length, &metre.cells)) {
        return false;
    }
    *out = metre;
    return true;
}

bool harmony_count_cells(PlanTime cell_length, PlanTime length, size_t *cells) {
    PlanTime sum = {0, 1};
    size_t count = 0;
    while (plan_time_compare(sum, length) < 0) {
        if (count >= MAX_PHRASE_BARS * 4 || plan_time_add(sum, cell_length, &sum) != PLAN_OK) {
            return false;
        }
        count++;
    }
    *cells = count;
    return count > 0 && plan_time_compare(sum, length) == 0;
}
