#include <ursatz_composer/lanes.h>

#include <stdlib.h>
#include <string.h>

#include "plan/plan_internal.h"

static bool is_note_lane(PlanLaneKind lane) {
    return lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY || lane == PLAN_LANE_INNER_VOICE;
}

void plan_lanes_free(PiecePlan *plan) {
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        constraint_list_free(&plan->lane_spans[i].constraints);
        if (!plan->lane_spans[i].removed && is_note_lane(plan->lane_spans[i].lane)) {
            free((void *)plan->lane_spans[i].payload.notes.notes);
        }
    }
    free(plan->lane_spans);
    plan->lane_spans = NULL;
    plan->lane_span_count = 0;
    plan->lane_span_capacity = 0;
    plan->removed_lane_span_count = 0;
}

LaneSpan *plan_lane_span_at(const PiecePlan *plan, LaneSpanId id) {
    if (!plan || id == PLAN_LANE_SPAN_NONE || id > plan->lane_span_count || plan->lane_spans[id - 1].removed) {
        return NULL;
    }
    return &plan->lane_spans[id - 1];
}

LaneSpanId plan_lane_span_rank(const PiecePlan *plan, LaneSpanId id) {
    if (!plan_lane_span_at(plan, id) || plan->removed_lane_span_count == 0) {
        return id;
    }
    LaneSpanId rank = 0;
    for (LaneSpanId other = 1; other <= id; other++) {
        rank += !plan->lane_spans[other - 1].removed;
    }
    return rank;
}

PlanResult plan_lane_span_remove_owned(PiecePlan *plan, NodeId owner) {
    for (size_t i = 0; plan && i < plan->lane_span_count; i++) {
        LaneSpan *entry = &plan->lane_spans[i];
        if (entry->removed || entry->owner != owner) {
            continue;
        }
        if (is_note_lane(entry->lane)) {
            free((void *)entry->payload.notes.notes);
        }
        constraint_list_free(&entry->constraints);
        memset(&entry->payload, 0, sizeof(entry->payload));
        entry->removed = true;
        plan->removed_lane_span_count++;
    }
    return PLAN_OK;
}

static bool spans_overlap(PlanSpan a, PlanSpan b) {
    PlanTime a_end;
    PlanTime b_end;
    if (plan_span_end(a, &a_end) != PLAN_OK || plan_span_end(b, &b_end) != PLAN_OK) {
        return false;
    }
    return plan_time_compare(a.start, b_end) < 0 && plan_time_compare(b.start, a_end) < 0;
}

static bool note_fits(const PlanNote *note, PlanTime span_length) {
    PlanTime zero = {0, 1};
    PlanTime note_end;
    if (note->offset.den == 0 || note->duration.den == 0 || note->duration.num <= 0) {
        return false;
    }
    if (plan_time_compare(note->offset, zero) < 0) {
        return false;
    }
    if (plan_time_add(note->offset, note->duration, &note_end) != PLAN_OK) {
        return false;
    }
    return plan_time_compare(note_end, span_length) <= 0;
}

bool lane_payload_is_valid(PlanLaneKind lane, PlanSpan span, const PlanLanePayload *payload) {
    switch (lane) {
    case PLAN_LANE_METER:
        return payload->meter.beats_per_bar > 0 && payload->meter.beat_unit > 0 &&
               payload->meter.bars_per_hyperbar >= 0;
    case PLAN_LANE_KEY:
        return payload->key.tonic_pitch_class >= 0 && payload->key.tonic_pitch_class <= 11 &&
               (payload->key.mode == PLAN_MODE_MAJOR || payload->key.mode == PLAN_MODE_MINOR);
    case PLAN_LANE_HARMONY: {
        size_t length = strnlen(payload->harmony.roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
        return length > 0 && length < PLAN_ROMAN_NUMERAL_MAX;
    }
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        /* An inner voice may be silent over its span: no notes. */
        if (payload->notes.note_count == 0 ? lane != PLAN_LANE_INNER_VOICE : !payload->notes.notes) {
            return false;
        }
        for (size_t i = 0; i < payload->notes.note_count; i++) {
            if (!note_fits(&payload->notes.notes[i], span.length)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

PlanResult plan_add_lane_span(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span,
                              const PlanLanePayload *payload, LaneSpanId *out_id) {
    if (!plan || !payload || !out_id) {
        return PLAN_ERR_NULL;
    }
    const PlanNode *owner_node = plan_node_at(plan, owner);
    if (!owner_node) {
        return PLAN_ERR_NOT_FOUND;
    }
    if (span.start.den == 0 || span.length.den == 0 || span.length.num <= 0 ||
        !plan_span_contains(owner_node->span, span)) {
        return PLAN_ERR_BAD_SPAN;
    }
    if (voice < 0 || (lane != PLAN_LANE_INNER_VOICE && voice != 0)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    if (!lane_payload_is_valid(lane, span, payload)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        const LaneSpan *existing = &plan->lane_spans[i];
        if (!existing->removed && existing->lane == lane && existing->voice == voice && spans_overlap(existing->span, span)) {
            return PLAN_ERR_LANE_OVERLAP;
        }
    }

    PlanLanePayload stored = *payload;
    if (is_note_lane(lane)) {
        size_t bytes = payload->notes.note_count * sizeof(PlanNote);
        PlanNote *copy = bytes ? malloc(bytes) : NULL;
        if (bytes && !copy) {
            return PLAN_ERR_ALLOC;
        }
        if (bytes) {
            memcpy(copy, payload->notes.notes, bytes);
        }
        stored.notes.notes = copy;
    }

    if (plan->lane_span_count == plan->lane_span_capacity) {
        size_t capacity = plan->lane_span_capacity ? plan->lane_span_capacity * 2 : 16;
        LaneSpan *spans = realloc(plan->lane_spans, capacity * sizeof(LaneSpan));
        if (!spans) {
            if (is_note_lane(lane)) {
                free((void *)stored.notes.notes);
            }
            return PLAN_ERR_ALLOC;
        }
        plan->lane_spans = spans;
        plan->lane_span_capacity = capacity;
    }

    LaneSpan *entry = &plan->lane_spans[plan->lane_span_count++];
    entry->lane = lane;
    entry->voice = voice;
    entry->owner = owner;
    entry->span = span;
    entry->payload = stored;
    entry->state = PLAN_STATE_CONSTRAINED;
    entry->removed = false;
    memset(&entry->constraints, 0, sizeof(entry->constraints));
    *out_id = plan->lane_span_count;
    return PLAN_OK;
}

PlanResult plan_lane_span_resize(PiecePlan *plan, LaneSpanId id, PlanTime new_length) {
    LaneSpan *entry = plan_lane_span_at(plan, id);
    if (!entry) {
        return plan ? PLAN_ERR_NOT_FOUND : PLAN_ERR_NULL;
    }
    if (entry->state == PLAN_STATE_GIVEN) {
        return PLAN_ERR_GIVEN_LOCKED;
    }
    if (new_length.den == 0 || new_length.num <= 0 || plan_time_compare(new_length, entry->span.length) > 0) {
        return PLAN_ERR_BAD_SPAN;
    }
    PlanSpan shorter = {entry->span.start, new_length};
    if (!lane_payload_is_valid(entry->lane, shorter, &entry->payload)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    entry->span = shorter;
    return PLAN_OK;
}

bool plan_has_lane_span(const PiecePlan *plan, LaneSpanId id) {
    return plan_lane_span_at(plan, id) != NULL;
}

size_t plan_lane_span_count(const PiecePlan *plan) {
    return plan ? plan->lane_span_count : 0;
}

PlanLaneKind plan_lane_span_lane(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->lane : PLAN_LANE_METER;
}

int plan_lane_span_voice(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->voice : 0;
}

NodeId plan_lane_span_owner(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? entry->owner : PLAN_NODE_NONE;
}

PlanSpan plan_lane_span_span(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    PlanSpan empty = {{0, 1}, {0, 1}};
    return entry ? entry->span : empty;
}

const PlanLanePayload *plan_lane_span_payload(const PiecePlan *plan, LaneSpanId id) {
    const LaneSpan *entry = plan_lane_span_at(plan, id);
    return entry ? &entry->payload : NULL;
}

static bool matches_lane(const LaneSpan *entry, PlanLaneKind lane, int voice, PlanSpan range) {
    return !entry->removed && entry->lane == lane && entry->voice == voice && spans_overlap(entry->span, range);
}

/* Orders spans by start, then by id. */
static bool comes_before(const PiecePlan *plan, LaneSpanId a, LaneSpanId b) {
    int by_start = plan_time_compare(plan->lane_spans[a - 1].span.start, plan->lane_spans[b - 1].span.start);
    return by_start < 0 || (by_start == 0 && a < b);
}

size_t plan_lane_find(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan range, LaneSpanId *out_ids,
                      size_t capacity) {
    if (!plan) {
        return 0;
    }
    size_t matches = 0;
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        if (matches_lane(&plan->lane_spans[i], lane, voice, range)) {
            matches++;
        }
    }
    if (!out_ids) {
        return matches;
    }

    /* Each pass picks the earliest match that follows the one before it. */
    size_t wanted = matches < capacity ? matches : capacity;
    LaneSpanId previous = PLAN_LANE_SPAN_NONE;
    for (size_t k = 0; k < wanted; k++) {
        LaneSpanId best = PLAN_LANE_SPAN_NONE;
        for (size_t i = 0; i < plan->lane_span_count; i++) {
            LaneSpanId id = i + 1;
            if (!matches_lane(&plan->lane_spans[i], lane, voice, range)) {
                continue;
            }
            if (previous != PLAN_LANE_SPAN_NONE && !comes_before(plan, previous, id)) {
                continue;
            }
            if (best == PLAN_LANE_SPAN_NONE || comes_before(plan, id, best)) {
                best = id;
            }
        }
        out_ids[k] = best;
        previous = best;
    }
    return matches;
}

size_t plan_node_lane_spans(const PiecePlan *plan, NodeId owner, LaneSpanId *out_ids, size_t capacity) {
    if (!plan) {
        return 0;
    }
    size_t matches = 0;
    for (size_t i = 0; i < plan->lane_span_count; i++) {
        if (plan->lane_spans[i].removed || plan->lane_spans[i].owner != owner) {
            continue;
        }
        if (out_ids && matches < capacity) {
            out_ids[matches] = i + 1;
        }
        matches++;
    }
    return matches;
}
