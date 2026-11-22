#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/events/note_event.h>
#include <model/pitch/pitch_numeric_value.h>
#include <model/time/meter_arithmetic.h>
#include <model/time/temporal_anchor.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/critic.h>

/* The melody line of a candidate (R-719): the notes of the melody lane's voice 0, and the pairs of ideas the critic judges the contrast of. */

void critic_line_destroy(CriticLine *line) {
    if (line == NULL) {
        return;
    }
    free(line->notes);
    memset(line, 0, sizeof(*line));
}

static bool melody_part(const RenderedPlan *rendered, size_t event) {
    const RenderedPart *part = &rendered->parts[rendered->part_index[event]];
    return part->lane == PLAN_LANE_MELODY && part->voice == 0;
}

/* The run of the line's notes that start inside the span: the notes are in time order, so it is contiguous. */
static void note_run(const CriticLine *line, PlanSpan span, size_t *first, size_t *count) {
    Rational end;
    *first = 0;
    *count = 0;
    if (plan_span_end(span, &end) != PLAN_OK) {
        return;
    }
    for (size_t i = 0; i < line->count; i++) {
        bool inside = rational_cmp(line->notes[i].onset, span.start) >= 0 && rational_cmp(line->notes[i].onset, end) < 0;
        if (inside && *count == 0) {
            *first = i;
        }
        if (inside) {
            (*count)++;
        }
    }
}

/* The idea node of this name that starts at or before `before` and nearest to it, else the first of the name; PLAN_NODE_NONE for none. */
static NodeId idea_named(const Plan *plan, const char *name, Rational before) {
    NodeId found = PLAN_NODE_NONE;
    NodeId first = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *node_name = plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_IDEA ? plan_node_name(plan, id) : NULL;
        if (node_name == NULL || strcmp(node_name, name) != 0) {
            continue;
        }
        first = first == PLAN_NODE_NONE ? id : first;
        if (rational_cmp(plan_node_span(plan, id).start, before) <= 0) {
            found = id;
        }
    }
    return found != PLAN_NODE_NONE ? found : first;
}

static bool add_pair(const Plan *plan, CriticLine *line, NodeId idea, NodeId against) {
    if (idea == PLAN_NODE_NONE || against == PLAN_NODE_NONE || idea == against || line->pair_count == CRITIC_PAIR_MAX) {
        return false;
    }
    CriticIdeaPair *pair = &line->pairs[line->pair_count];
    PlanSpan idea_span = plan_node_span(plan, idea);
    PlanSpan against_span = plan_node_span(plan, against);
    snprintf(pair->idea, sizeof(pair->idea), "%s", plan_node_name(plan, idea));
    snprintf(pair->against, sizeof(pair->against), "%s", plan_node_name(plan, against));
    note_run(line, idea_span, &pair->idea_first, &pair->idea_count);
    note_run(line, against_span, &pair->against_first, &pair->against_count);
    pair->idea_start = idea_span.start;
    pair->against_start = against_span.start;
    pair->idea_length = idea_span.length;
    pair->against_length = against_span.length;
    line->pair_count++;
    return true;
}

/* A contrast constraint names the idea it contrasts against; with none in the plan, a period's ideas are paired by the names grow gives them. */
static void find_pairs(const Plan *plan, CriticLine *line) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_IDEA) {
            continue;
        }
        const PlanConstraint *constraint = plan_node_find_constraint(plan, id, PLAN_CONSTRAINT_CONTRAST);
        if (constraint != NULL) {
            add_pair(plan, line, id, idea_named(plan, constraint->params.contrast.against, plan_node_span(plan, id).start));
        }
    }
    if (line->pair_count > 0) {
        return;
    }
    static const char *const NAMES[][2] = {{"contrasting idea", "basic idea"}, {"consequent contrasting idea", "consequent basic idea"}};
    for (size_t i = 0; i < 2; i++) {
        NodeId idea = idea_named(plan, NAMES[i][0], plan_node_span(plan, plan_root(plan)).start);
        if (idea != PLAN_NODE_NONE) {
            add_pair(plan, line, idea, idea_named(plan, NAMES[i][1], plan_node_span(plan, idea).start));
        }
    }
}

CriticResult critic_line_from_render(const Plan *plan, const RenderedPlan *rendered, CriticLine *out) {
    if (plan == NULL || rendered == NULL || out == NULL) {
        return CRITIC_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (rendered->meter_map == NULL) {
        return CRITIC_ERR_RENDER;
    }
    size_t count = 0;
    for (size_t i = 0; i < rendered->count; i++) {
        count += melody_part(rendered, i) ? 1 : 0;
    }
    if (count == 0) {
        return CRITIC_ERR_EMPTY;
    }
    out->notes = calloc(count, sizeof(CriticNote));
    if (out->notes == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    int first_counted = meter_map_first_counted_measure(rendered->meter_map);
    for (size_t i = 0; i < rendered->count; i++) {
        if (!melody_part(rendered, i)) {
            continue;
        }
        CriticNote *note = &out->notes[out->count];
        const NoteEvent *event = rendered->events[i];
        int measure = 0;
        Rational start;
        Rational end;
        note->onset = temporal_anchor_position(note_event_temporal_anchor(event))->value;
        note->duration = note_event_duration(event)->value;
        note->pitch = (int)pitch_numeric_value_position_numerator(rendered->pitch_values[i]);
        if (meter_map_measure_of(rendered->meter_map, note->onset, &measure) != METER_MAP_OK ||
            meter_map_measure_bounds(rendered->meter_map, measure, &start, &end) != METER_MAP_OK ||
            rational_sub(note->onset, start, &note->position) != RATIONAL_OK) {
            critic_line_destroy(out);
            return CRITIC_ERR_RENDER;
        }
        note->bar = measure + 1 - first_counted;
        out->count++;
    }
    int measure = 0;
    if (meter_map_measure_of(rendered->meter_map, out->notes[0].onset, &measure) != METER_MAP_OK ||
        meter_map_beat_length(rendered->meter_map, measure, &out->beat) != METER_MAP_OK) {
        critic_line_destroy(out);
        return CRITIC_ERR_RENDER;
    }
    find_pairs(plan, out);
    return CRITIC_OK;
}

/* A motive's own timeline: 0 is the start of its pickup, if it has one, and its first barline is at the pickup's length (motive.h). */
CriticResult critic_line_from_motive(const Motive *motive, CriticLine *out) {
    PlanMeter meter;
    if (motive == NULL || out == NULL) {
        return CRITIC_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (motive->note_count == 0 || motive->bar.den <= 0 || motive->bar.num <= 0 || !motive_meter_of(motive, &meter)) {
        return motive->note_count == 0 ? CRITIC_ERR_EMPTY : CRITIC_ERR_RENDER;
    }
    Rational pickup = motive->pickup.den > 0 && motive->pickup.num > 0 ? motive->pickup : rational_ratio(0, 1);
    bool compound = meter.beat_unit == 8 && meter.beats_per_bar >= 6 && meter.beats_per_bar % 3 == 0;
    out->beat = compound ? rational_ratio(3, 8) : rational_ratio(1, meter.beat_unit);
    out->notes = calloc(motive->note_count, sizeof(CriticNote));
    if (out->notes == NULL) {
        return CRITIC_ERR_ALLOC;
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        CriticNote *note = &out->notes[i];
        note->onset = motive->notes[i].onset;
        note->duration = motive->notes[i].duration;
        note->pitch = motive_pitch(motive, i);
        if (note->pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) { /* a rhythm motive has no melody to judge */
            critic_line_destroy(out);
            return CRITIC_ERR_EMPTY;
        }
        if (rational_cmp(note->onset, pickup) < 0) {
            note->bar = 0;
            note->position = note->onset;
            continue;
        }
        Rational from_barline;
        Rational bars;
        Rational start;
        long long whole = 0;
        if (rational_sub(note->onset, pickup, &from_barline) != RATIONAL_OK || rational_div(from_barline, motive->bar, &bars) != RATIONAL_OK ||
            rational_floor(bars, &whole) != RATIONAL_OK || rational_mul_int(motive->bar, whole, &start) != RATIONAL_OK ||
            rational_sub(from_barline, start, &note->position) != RATIONAL_OK || whole >= 1000000) {
            critic_line_destroy(out);
            return CRITIC_ERR_OVERFLOW;
        }
        note->bar = (int)whole + 1;
    }
    out->count = motive->note_count;
    return CRITIC_OK;
}
