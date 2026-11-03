#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "piece/piece_internal.h"

/*
 * The seam (R-171): the bar the loop leads into, a Given copy of the body's first bar. The copy is made through
 * plan_give_section, from a plan of one section that holds the written lane spans of that bar.
 */

#define SEAM_MAX_SPANS 128
/* The inner voices a texture writes. */
#define SEAM_INNER_VOICES 3

bool piece_is_structure_plan(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
            plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_PIECE_PART)) {
            return true;
        }
    }
    return false;
}

NodeId piece_seam_find(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
            plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool piece_seam_body_start(const PiecePlan *plan, Rational *out) {
    bool found = false;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            Rational start = plan_node_span(plan, id).start;
            if (constraint->kind == PLAN_CONSTRAINT_PIECE_PART && constraint->params.piece_part.part == 1 &&
                (!found || rational_cmp(start, *out) < 0)) {
                *out = start;
                found = true;
            }
        }
    }
    return found;
}

static Rational later(Rational a, Rational b) {
    return rational_cmp(a, b) >= 0 ? a : b;
}

static Rational earlier(Rational a, Rational b) {
    return rational_cmp(a, b) <= 0 ? a : b;
}

/* The part of a lane span inside the bar; false when none of it is. */
static bool clip_span(PlanSpan span, PlanSpan bar, PlanSpan *out) {
    Rational span_end;
    Rational bar_end;
    Rational end;
    if (plan_span_end(span, &span_end) != PLAN_OK || plan_span_end(bar, &bar_end) != PLAN_OK) {
        return false;
    }
    out->start = later(span.start, bar.start);
    end = earlier(span_end, bar_end);
    return rational_cmp(out->start, end) < 0 && rational_sub(end, out->start, &out->length) == RATIONAL_OK;
}

/* The notes of a span that sound inside the clipped span, cut to it and measured from its start. */
static size_t clip_notes(const PlanNotes *notes, PlanSpan from, PlanSpan to, PlanNote *out) {
    Rational to_end;
    size_t count = 0;
    if (plan_span_end(to, &to_end) != PLAN_OK) {
        return 0;
    }
    for (size_t i = 0; i < notes->note_count; i++) {
        Rational onset;
        Rational end;
        Rational start;
        PlanNote note = notes->notes[i];
        if (rational_add(from.start, note.offset, &onset) != RATIONAL_OK || rational_add(onset, note.duration, &end) != RATIONAL_OK ||
            rational_cmp(end, to.start) <= 0 || rational_cmp(onset, to_end) >= 0) {
            continue;
        }
        start = later(onset, to.start);
        end = earlier(end, to_end);
        if (rational_sub(start, to.start, &note.offset) != RATIONAL_OK || rational_sub(end, start, &note.duration) != RATIONAL_OK) {
            continue;
        }
        out[count++] = note;
    }
    return count;
}

/* Adds the written spans of a lane and voice that sound in the bar to the slice section. */
static bool add_lane(const PiecePlan *plan, PiecePlan *slice, NodeId section, PlanLaneKind lane, int voice, PlanSpan bar) {
    LaneSpanId ids[SEAM_MAX_SPANS];
    size_t found = plan_lane_find(plan, lane, voice, bar, ids, SEAM_MAX_SPANS);
    if (found > SEAM_MAX_SPANS) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan span;
        PlanLanePayload payload;
        LaneSpanId added;
        PlanNote *notes = NULL;
        const PlanLanePayload *source = plan_lane_span_payload(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED ||
            !clip_span(plan_lane_span_span(plan, ids[i]), bar, &span)) {
            continue;
        }
        memset(&payload, 0, sizeof(payload));
        if (lane == PLAN_LANE_HARMONY) {
            payload.harmony = source->harmony;
        } else {
            notes = calloc(source->notes.note_count + 1, sizeof(PlanNote));
            if (!notes) {
                return false;
            }
            payload.notes.notes = notes;
            payload.notes.note_count = clip_notes(&source->notes, plan_lane_span_span(plan, ids[i]), span, notes);
            if (payload.notes.note_count == 0) {
                free(notes);
                continue;
            }
        }
        bool ok = plan_add_lane_span(slice, section, lane, voice, span, &payload, &added) == PLAN_OK &&
                  plan_lane_span_set_state(slice, added, PLAN_STATE_REALIZED) == PLAN_OK;
        free(notes);
        if (!ok) {
            return false;
        }
    }
    return true;
}

/* The slice: a plan holding one section, the bar, with the meter, the key and the written lane spans over it. */
static PiecePlan *make_slice(const PiecePlan *plan, PlanSpan bar, NodeId *section) {
    PiecePlan *slice = plan_create();
    PlanSpan whole = {{0, 1}, {0, 1}};
    NodeId form;
    LaneSpanId added;
    LaneSpanId keys[4];
    LaneSpanId meters[2];
    PlanLanePayload payload;
    Rational bar_end;
    if (!slice) {
        return NULL;
    }
    bool ok = plan_span_end(bar, &bar_end) == PLAN_OK && rational_sub(bar_end, whole.start, &whole.length) == RATIONAL_OK &&
              plan_add_node(slice, PLAN_NODE_NONE, PLAN_NODE_FORM, whole, "slice", &form) == PLAN_OK &&
              plan_add_node(slice, form, PLAN_NODE_SECTION, bar, "slice", section) == PLAN_OK;
    size_t meter_count = plan_lane_find(plan, PLAN_LANE_METER, 0, bar, meters, 2);
    size_t key_count = plan_lane_find(plan, PLAN_LANE_KEY, 0, bar, keys, 4);
    ok = ok && meter_count == 1 && key_count >= 1 && key_count <= 4;
    if (ok) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = plan_lane_span_payload(plan, meters[0])->meter;
        ok = plan_add_lane_span(slice, form, PLAN_LANE_METER, 0, whole, &payload, &added) == PLAN_OK &&
             plan_lane_span_set_state(slice, added, PLAN_STATE_GIVEN) == PLAN_OK;
    }
    for (size_t i = 0; ok && i < key_count; i++) {
        PlanSpan span;
        ok = clip_span(plan_lane_span_span(plan, keys[i]), bar, &span);
        memset(&payload, 0, sizeof(payload));
        payload.key = plan_lane_span_payload(plan, keys[i])->key;
        ok = ok && plan_add_lane_span(slice, form, PLAN_LANE_KEY, 0, span, &payload, &added) == PLAN_OK &&
             plan_lane_span_set_state(slice, added, PLAN_STATE_GIVEN) == PLAN_OK;
    }
    ok = ok && add_lane(plan, slice, *section, PLAN_LANE_HARMONY, 0, bar) && add_lane(plan, slice, *section, PLAN_LANE_BASS, 0, bar) &&
         add_lane(plan, slice, *section, PLAN_LANE_MELODY, 0, bar);
    for (int voice = 1; ok && voice <= SEAM_INNER_VOICES; voice++) {
        ok = add_lane(plan, slice, *section, PLAN_LANE_INNER_VOICE, voice, bar);
    }
    ok = ok && plan_node_set_state(slice, *section, PLAN_STATE_REALIZED) == PLAN_OK;
    if (!ok) {
        plan_destroy(slice);
        return NULL;
    }
    return slice;
}

GivenResult piece_seam_give(PiecePlan *plan, NodeId seam, Rational body_start) {
    PlanSpan bar = {body_start, plan_node_span(plan, seam).length};
    NodeId section;
    PiecePlan *slice = make_slice(plan, bar, &section);
    if (!slice) {
        return GIVEN_ERR_PLAN;
    }
    GivenResult result = plan_give_section(plan, seam, slice, section);
    plan_destroy(slice);
    return result;
}
