#include <stdlib.h>
#include <string.h>

#include <theory/counterpoint/melodic_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "piece/piece_internal.h"
#include "util/position.h"
#include "util/util.h"

/*
 * The connective (R-486): the last bar of a held section that leads into the next ends with a short stepwise line into
 * the next section's first note. The downbeat note of the bar is kept and shortened to one beat, the rest of the bar
 * takes eighth notes in the scale of the key the section ends in, the last of them one scale step from the next
 * section's first note and on the side the downbeat note is on. No random number: the line is the one the rule gives, or
 * none.
 */

/* The most notes of a line (eighths in a bar of 4/4 with room to spare). */
#define CONNECTIVE_MAX_NOTES 12
/* The furthest, in semitones, the downbeat note may move to the first note of the line. */
#define CONNECTIVE_LEAP 7
/* How far, in semitones, a line note may lie outside the range the section's other melody notes use. */
#define CONNECTIVE_RANGE_SLACK 2
/* The most melody spans read over a section. */
#define CONNECTIVE_MAX_SPANS 64

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
/* Harmonic minor: the raised seventh. */
static const int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 11};

static bool in_scale(int tonic, bool minor, int pitch) {
    int pitch_class = ((pitch - tonic) % 12 + 12) % 12;
    const int *scale = minor ? MINOR_SCALE : MAJOR_SCALE;
    for (int i = 0; i < 7; i++) {
        if (scale[i] == pitch_class) {
            return true;
        }
    }
    return false;
}

/* The next pitch of the scale strictly above (direction 1) or below (-1) the pitch. */
static int scale_step(int tonic, bool minor, int pitch, int direction) {
    int next = pitch;
    do {
        next += direction;
    } while (!in_scale(tonic, minor, next));
    return next;
}

static Rational tick_before(Rational time) {
    Rational tick = {1, 4096};
    return position_sub(time, tick);
}

static LaneSpanId span_at(const PiecePlan *plan, PlanLaneKind lane, Rational at) {
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    PlanSpan range = {at, {1, 4096}};
    return plan_lane_find(plan, lane, 0, range, &id, 1) > 0 ? id : PLAN_LANE_SPAN_NONE;
}

/* The section that starts where this one ends; PLAN_NODE_NONE when there is none (material is not a section of the piece). */
static NodeId next_section(const PiecePlan *plan, NodeId section) {
    PlanSpan span = plan_node_span(plan, section);
    Rational end = position_add(span.start, span.length);
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (id != section && plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
            !plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_MATERIAL) &&
            !plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM) &&
            rational_cmp(plan_node_span(plan, id).start, end) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* The pitch of the first melody note of the section that sounds from its start; false when there is none. */
static bool first_note(const PiecePlan *plan, NodeId section, int *out) {
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId id = span_at(plan, PLAN_LANE_MELODY, span.start);
    if (id == PLAN_LANE_SPAN_NONE) {
        return false;
    }
    Rational origin = plan_lane_span_span(plan, id).start;
    const PlanNotes *notes = &plan_lane_span_payload(plan, id)->notes;
    bool found = false;
    Rational best = {0, 1};
    for (size_t i = 0; i < notes->note_count; i++) {
        Rational onset = position_add(origin, notes->notes[i].offset);
        if (rational_cmp(onset, span.start) >= 0 && (!found || rational_cmp(onset, best) < 0)) {
            best = onset;
            *out = notes->notes[i].midi;
            found = true;
        }
    }
    return found;
}

/* The lowest and highest melody pitch of the section outside its last bar; false when it has none. */
static bool melody_range(const PiecePlan *plan, NodeId section, Rational bar_start, int *low, int *high) {
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId ids[CONNECTIVE_MAX_SPANS];
    size_t count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, CONNECTIVE_MAX_SPANS);
    bool found = false;
    for (size_t s = 0; s < count && s < CONNECTIVE_MAX_SPANS; s++) {
        Rational origin = plan_lane_span_span(plan, ids[s]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            Rational onset = position_add(origin, notes->notes[i].offset);
            if (rational_cmp(onset, span.start) < 0 || rational_cmp(onset, bar_start) >= 0) {
                continue;
            }
            int midi = notes->notes[i].midi;
            *low = !found || midi < *low ? midi : *low;
            *high = !found || midi > *high ? midi : *high;
            found = true;
        }
    }
    return found;
}

/* The constraint's written flag, set: the constraint is replaced, as a constraint is not changed in place. */
static PieceResult mark_written(PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        if (plan_node_constraint_at(plan, section, i)->kind == PLAN_CONSTRAINT_CONNECTIVE) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_CONNECTIVE;
            constraint.params.connective.written = true;
            PieceResult result = piece_result_of_plan(plan_node_remove_constraint(plan, section, i));
            return result == PIECE_OK ? piece_add_constraint(plan, section, &constraint) : result;
        }
    }
    return PIECE_OK;
}

PieceResult piece_connective_write(PiecePlan *plan, NodeId section) {
    if (!plan || !plan_has_node(plan, section) || !plan_node_has_constraint(plan, section, PLAN_CONSTRAINT_CONNECTIVE)) {
        return PIECE_ERR_NULL;
    }
    PlanSpan span = plan_node_span(plan, section);
    Rational end = position_add(span.start, span.length);
    Rational last = tick_before(end);
    NodeId next = next_section(plan, section);
    LaneSpanId meter_span = span_at(plan, PLAN_LANE_METER, last);
    LaneSpanId key_span = span_at(plan, PLAN_LANE_KEY, last);
    int target;
    if (next == PLAN_NODE_NONE || meter_span == PLAN_LANE_SPAN_NONE || key_span == PLAN_LANE_SPAN_NONE ||
        !first_note(plan, next, &target)) {
        return PIECE_OK;
    }
    /* The last bar, and the beat and eighths in it. */
    PlanMeter meter = plan_lane_span_payload(plan, meter_span)->meter;
    if (meter.beats_per_bar < 1 || meter.beat_unit < 1) {
        return PIECE_OK;
    }
    Rational unit = {1, meter.beat_unit};
    Rational bar = position_mul(unit, meter.beats_per_bar);
    Rational beat = meter.beat_unit == 8 && meter.beats_per_bar % 3 == 0 && meter.beats_per_bar >= 6 ? (Rational){3, 8} : unit;
    Rational eighth = {1, 8};
    Rational bar_start = position_sub(end, bar);
    Rational rest = position_sub(bar, beat);
    if (rational_cmp(bar_start, span.start) < 0 || !position_is_positive(rest) || !position_is_multiple(rest, eighth)) {
        return PIECE_OK;
    }
    long long count = position_floor_units(rest, eighth);
    if (count < 1 || count > CONNECTIVE_MAX_NOTES) {
        return PIECE_OK;
    }
    /* The melody span the last bar is in, and the note on its downbeat. */
    LaneSpanId melody = span_at(plan, PLAN_LANE_MELODY, bar_start);
    if (melody == PLAN_LANE_SPAN_NONE) {
        return PIECE_OK;
    }
    PlanSpan melody_span = plan_lane_span_span(plan, melody);
    if (rational_cmp(position_add(melody_span.start, melody_span.length), end) < 0) {
        return PIECE_OK;
    }
    const PlanNotes *old = &plan_lane_span_payload(plan, melody)->notes;
    size_t downbeat = old->note_count;
    for (size_t i = 0; i < old->note_count; i++) {
        if (rational_cmp(position_add(melody_span.start, old->notes[i].offset), bar_start) == 0) {
            downbeat = i;
        }
    }
    if (downbeat == old->note_count) {
        return PIECE_OK;
    }
    int from = old->notes[downbeat].midi;
    /* The line, written backwards from its last note: one scale step from the target, on the side of the downbeat note. */
    AnalysisKey key = plan_lane_span_payload(plan, key_span)->key;
    int tonic = analysis_key_tonic_pitch_class(&key);
    bool minor = key.mode == KEY_STRUCTURE_MODE_MINOR;
    int side = from > target ? 1 : -1;
    int line[CONNECTIVE_MAX_NOTES];
    line[count - 1] = scale_step(tonic, minor, target, side);
    for (long long i = count - 2; i >= 0; i--) {
        line[i] = scale_step(tonic, minor, line[i + 1], side);
    }
    /* The melodic rules: the move into the line and the step into the target (the line itself is the scale, the augmented second of harmonic minor included). */
    int move = line[0] - from;
    if (util_absolute(move) > CONNECTIVE_LEAP || util_absolute(move) == 6 ||
        !standard_counterpoint_melodic_semitones_allowed(move) || !standard_counterpoint_is_step(target - line[count - 1])) {
        return PIECE_OK;
    }
    int low;
    int high;
    if (!melody_range(plan, section, bar_start, &low, &high)) {
        return PIECE_OK;
    }
    for (long long i = 0; i < count; i++) {
        if (line[i] < low - CONNECTIVE_RANGE_SLACK || line[i] > high + CONNECTIVE_RANGE_SLACK) {
            return PIECE_OK;
        }
    }
    /* The span's notes before the bar, the downbeat note as one beat, the line, then the notes after the section. */
    PlanNote *notes = calloc(old->note_count + (size_t)count + 1, sizeof(PlanNote));
    if (!notes) {
        return PIECE_ERR_ALLOC;
    }
    size_t written = 0;
    for (size_t i = 0; i < old->note_count; i++) {
        Rational onset = position_add(melody_span.start, old->notes[i].offset);
        if (rational_cmp(onset, bar_start) < 0) {
            notes[written++] = old->notes[i];
        }
    }
    notes[written++] = plan_note(from, position_sub(bar_start, melody_span.start), beat);
    for (long long i = 0; i < count; i++) {
        Rational offset = position_add(position_sub(bar_start, melody_span.start), position_add(beat, position_mul(eighth, i)));
        notes[written++] = plan_note(line[i], offset, eighth);
    }
    for (size_t i = 0; i < old->note_count; i++) {
        Rational onset = position_add(melody_span.start, old->notes[i].offset);
        if (rational_cmp(onset, end) >= 0) {
            notes[written++] = old->notes[i];
        }
    }
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = notes;
    payload.notes.note_count = written;
    PieceResult result = piece_result_of_plan(plan_lane_span_write(plan, melody, &payload));
    free(notes);
    /* A span the plan refuses to rewrite (Given) keeps the bar as it was and the constraint false. */
    if (result != PIECE_OK) {
        return result == PIECE_ERR_ALLOC ? result : PIECE_OK;
    }
    return mark_written(plan, section);
}
