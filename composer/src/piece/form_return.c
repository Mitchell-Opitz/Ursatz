#include <ursatz_composer/form_return.h>

#include <stdlib.h>
#include <string.h>

#include "piece/form_return_internal.h"

/* The most key spans a section is read across: a section holds at most two keys, with a few to spare. */
#define MAX_KEY_PIECES 8

/* ------------------------------------------------------------- the copy */

static char *copy_string(const char *text) {
    size_t size = text ? strlen(text) + 1 : 1;
    char *out = malloc(size);
    if (out) {
        memcpy(out, text ? text : "", size);
    }
    return out;
}

static PlanConstraint *copy_constraints(size_t count, const PlanConstraint *(*at)(const PiecePlan *, size_t, size_t),
                                        const PiecePlan *plan, size_t id) {
    PlanConstraint *out = count ? calloc(count, sizeof(PlanConstraint)) : NULL;
    for (size_t i = 0; out && i < count; i++) {
        out[i] = *at(plan, id, i);
    }
    return out;
}

static const PlanConstraint *node_constraint_at(const PiecePlan *plan, size_t id, size_t index) {
    return plan_node_constraint_at(plan, (NodeId)id, index);
}

static const PlanConstraint *span_constraint_at(const PiecePlan *plan, size_t id, size_t index) {
    return plan_lane_span_constraint_at(plan, (LaneSpanId)id, index);
}

void return_copy_destroy(ReturnCopy *copy) {
    for (size_t i = 0; copy && i < copy->node_count; i++) {
        free(copy->nodes[i].name);
        free(copy->nodes[i].constraints);
    }
    for (size_t i = 0; copy && i < copy->span_count; i++) {
        free(copy->spans[i].notes);
        free(copy->spans[i].constraints);
    }
    if (copy) {
        free(copy->nodes);
        free(copy->spans);
        memset(copy, 0, sizeof(*copy));
    }
}

static size_t subtree_size(const PiecePlan *plan, NodeId id) {
    size_t size = 1;
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        size += subtree_size(plan, plan_node_child_at(plan, id, i));
    }
    return size;
}

/* Reads the children of id (and theirs) into copy->nodes after the ones there, their node ids into ids. */
static bool collect_nodes(const PiecePlan *plan, NodeId id, size_t parent, ReturnCopy *copy, NodeId *ids) {
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        NodeId child = plan_node_child_at(plan, id, i);
        size_t at = copy->node_count;
        ReturnNode *node = &copy->nodes[at];
        copy->node_count++;
        memset(node, 0, sizeof(*node));
        ids[at] = child;
        node->kind = plan_node_kind(plan, child);
        node->parent = parent;
        node->span = plan_node_span(plan, child);
        node->name = copy_string(plan_node_name(plan, child));
        node->state = plan_node_state(plan, child);
        node->constraint_count = plan_node_constraint_count(plan, child);
        node->constraints = copy_constraints(node->constraint_count, node_constraint_at, plan, child);
        if (!node->name || (node->constraint_count && !node->constraints)) {
            return false;
        }
        if (!collect_nodes(plan, child, at, copy, ids)) {
            return false;
        }
    }
    return true;
}

static bool is_written_lane(PlanLaneKind lane) {
    return lane == PLAN_LANE_HARMONY || lane == PLAN_LANE_BASS || lane == PLAN_LANE_MELODY ||
           lane == PLAN_LANE_INNER_VOICE;
}

/* Reads the written lane spans an owner holds, in the order they were added. */
static bool collect_spans(const PiecePlan *plan, NodeId owner, size_t index, ReturnCopy *copy) {
    size_t count = plan_node_lane_spans(plan, owner, NULL, 0);
    LaneSpanId *ids = count ? calloc(count, sizeof(LaneSpanId)) : NULL;
    if (count && !ids) {
        return false;
    }
    plan_node_lane_spans(plan, owner, ids, count);
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, ids[i]);
        if (!is_written_lane(lane)) {
            continue;
        }
        if (copy->span_count == copy->span_capacity) {
            size_t capacity = copy->span_capacity ? copy->span_capacity * 2 : 64;
            ReturnSpan *grown = realloc(copy->spans, capacity * sizeof(ReturnSpan));
            if (!grown) {
                ok = false;
                break;
            }
            copy->spans = grown;
            copy->span_capacity = capacity;
        }
        ReturnSpan *span = &copy->spans[copy->span_count];
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        memset(span, 0, sizeof(*span));
        copy->span_count++;
        span->lane = lane;
        span->voice = plan_lane_span_voice(plan, ids[i]);
        span->owner = index;
        span->span = plan_lane_span_span(plan, ids[i]);
        span->state = plan_lane_span_state(plan, ids[i]);
        if (lane == PLAN_LANE_HARMONY) {
            span->harmony = payload->harmony;
        } else {
            span->note_count = payload->notes.note_count;
            span->notes = span->note_count ? calloc(span->note_count, sizeof(PlanNote)) : NULL;
            if (span->note_count && !span->notes) {
                ok = false;
                break;
            }
            for (size_t n = 0; n < span->note_count; n++) {
                span->notes[n] = payload->notes.notes[n];
            }
        }
        span->constraint_count = plan_lane_span_constraint_count(plan, ids[i]);
        span->constraints = copy_constraints(span->constraint_count, span_constraint_at, plan, ids[i]);
        if (span->constraint_count && !span->constraints) {
            ok = false;
        }
    }
    free(ids);
    return ok;
}

bool return_copy_read(const PiecePlan *plan, NodeId section, ReturnCopy *out) {
    memset(out, 0, sizeof(*out));
    size_t size = subtree_size(plan, section) - 1;
    NodeId *ids = calloc(size + 1, sizeof(NodeId));
    out->nodes = size ? calloc(size, sizeof(ReturnNode)) : NULL;
    out->node_capacity = size;
    bool ok = ids && (size == 0 || out->nodes) && collect_nodes(plan, section, RETURN_SECTION, out, ids) &&
              collect_spans(plan, section, RETURN_SECTION, out);
    for (size_t i = 0; ok && i < out->node_count; i++) {
        ok = collect_spans(plan, ids[i], i, out);
    }
    free(ids);
    if (!ok) {
        return_copy_destroy(out);
    }
    return ok;
}

bool return_copy_clone(const ReturnCopy *from, ReturnCopy *out) {
    memset(out, 0, sizeof(*out));
    out->nodes = from->node_count ? calloc(from->node_count, sizeof(ReturnNode)) : NULL;
    out->spans = from->span_count ? calloc(from->span_count, sizeof(ReturnSpan)) : NULL;
    out->node_capacity = from->node_count;
    out->span_capacity = from->span_count;
    bool ok = (from->node_count == 0 || out->nodes) && (from->span_count == 0 || out->spans);
    for (size_t i = 0; ok && i < from->node_count; i++) {
        ReturnNode *node = &out->nodes[i];
        *node = from->nodes[i];
        out->node_count = i + 1;
        node->name = copy_string(from->nodes[i].name);
        node->constraints = from->nodes[i].constraint_count ? calloc(node->constraint_count, sizeof(PlanConstraint)) : NULL;
        ok = node->name && (node->constraint_count == 0 || node->constraints);
        if (ok && node->constraint_count) {
            memcpy(node->constraints, from->nodes[i].constraints, node->constraint_count * sizeof(PlanConstraint));
        }
    }
    for (size_t i = 0; ok && i < from->span_count; i++) {
        ReturnSpan *span = &out->spans[i];
        *span = from->spans[i];
        out->span_count = i + 1;
        span->notes = span->note_count ? calloc(span->note_count, sizeof(PlanNote)) : NULL;
        span->constraints = span->constraint_count ? calloc(span->constraint_count, sizeof(PlanConstraint)) : NULL;
        ok = (span->note_count == 0 || span->notes) && (span->constraint_count == 0 || span->constraints);
        if (ok && span->note_count) {
            memcpy(span->notes, from->spans[i].notes, span->note_count * sizeof(PlanNote));
        }
        if (ok && span->constraint_count) {
            memcpy(span->constraints, from->spans[i].constraints, span->constraint_count * sizeof(PlanConstraint));
        }
    }
    if (!ok) {
        /* A half-made span or node owns what it was given; clear what the copy of it did not get. */
        for (size_t i = out->node_count; i < from->node_count; i++) {
            out->nodes[i].name = NULL;
            out->nodes[i].constraints = NULL;
        }
        for (size_t i = out->span_count; i < from->span_count; i++) {
            out->spans[i].notes = NULL;
            out->spans[i].constraints = NULL;
        }
        out->node_count = from->node_count;
        out->span_count = from->span_count;
        return_copy_destroy(out);
    }
    return ok;
}

static bool time_sub(PlanTime a, PlanTime b, PlanTime *out) {
    return plan_time_make(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den, out) == PLAN_OK;
}

static bool move_span(PlanSpan *span, PlanTime delta) {
    return plan_time_add(span->start, delta, &span->start) == PLAN_OK;
}

bool return_copy_move(ReturnCopy *copy, PlanTime from, PlanTime to, int transpose) {
    PlanTime delta;
    if (!time_sub(to, from, &delta)) {
        return false;
    }
    for (size_t i = 0; i < copy->node_count; i++) {
        ReturnNode *node = &copy->nodes[i];
        if (!move_span(&node->span, delta)) {
            return false;
        }
        for (size_t c = 0; c < node->constraint_count; c++) {
            if (node->constraints[c].kind == PLAN_CONSTRAINT_KEY) {
                PlanKey *key = &node->constraints[c].params.key;
                key->tonic_pitch_class = ((key->tonic_pitch_class + transpose) % 12 + 12) % 12;
            }
        }
    }
    for (size_t i = 0; i < copy->span_count; i++) {
        ReturnSpan *span = &copy->spans[i];
        if (!move_span(&span->span, delta)) {
            return false;
        }
        for (size_t n = 0; n < span->note_count; n++) {
            span->notes[n].midi_pitch += transpose;
        }
        for (size_t c = 0; c < span->constraint_count; c++) {
            if (span->constraints[c].kind == PLAN_CONSTRAINT_KEY) {
                PlanKey *key = &span->constraints[c].params.key;
                key->tonic_pitch_class = ((key->tonic_pitch_class + transpose) % 12 + 12) % 12;
            }
        }
    }
    return true;
}

PlanResult return_copy_write(PiecePlan *plan, NodeId section, const ReturnCopy *copy) {
    NodeId *ids = calloc(copy->node_count + 1, sizeof(NodeId));
    if (!ids) {
        return PLAN_ERR_ALLOC;
    }
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < copy->node_count; i++) {
        const ReturnNode *node = &copy->nodes[i];
        NodeId parent = node->parent == RETURN_SECTION ? section : ids[node->parent];
        result = plan_add_node(plan, parent, node->kind, node->span, node->name, &ids[i]);
        for (size_t c = 0; result == PLAN_OK && c < node->constraint_count; c++) {
            result = plan_node_add_constraint(plan, ids[i], &node->constraints[c]);
        }
        if (result == PLAN_OK && node->state != PLAN_STATE_CONSTRAINED) {
            result = plan_node_set_state(plan, ids[i], node->state);
        }
    }
    for (size_t i = 0; result == PLAN_OK && i < copy->span_count; i++) {
        const ReturnSpan *span = &copy->spans[i];
        NodeId owner = span->owner == RETURN_SECTION ? section : ids[span->owner];
        PlanLanePayload payload;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        if (span->lane == PLAN_LANE_HARMONY) {
            payload.harmony = span->harmony;
        } else {
            payload.notes.notes = span->notes;
            payload.notes.note_count = span->note_count;
        }
        result = plan_add_lane_span(plan, owner, span->lane, span->voice, span->span, &payload, &id);
        for (size_t c = 0; result == PLAN_OK && c < span->constraint_count; c++) {
            result = plan_lane_span_add_constraint(plan, id, &span->constraints[c]);
        }
        if (result == PLAN_OK && span->state != PLAN_STATE_CONSTRAINED) {
            result = plan_lane_span_set_state(plan, id, span->state);
        }
    }
    free(ids);
    return result;
}

void return_payload_destroy(void *payload) {
    ReturnPayload *p = payload;
    if (p) {
        return_copy_destroy(&p->source);
        return_copy_destroy(&p->result);
        free(p);
    }
}

/* ------------------------------------------------------------- the target */

static bool has_constraint(const PiecePlan *plan, NodeId section, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        if (plan_node_constraint_at(plan, section, i)->kind == kind) {
            return true;
        }
    }
    return false;
}

static bool find_return(const PiecePlan *plan, NodeId section, PlanReturn *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_RETURN) {
            *out = constraint->params.section_return;
            return true;
        }
    }
    return false;
}

static bool meter_over(const PiecePlan *plan, PlanSpan span, PlanMeter *out) {
    LaneSpanId ids[2];
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, ids, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, ids[0]), span)) {
        return false;
    }
    *out = plan_lane_span_payload(plan, ids[0])->meter;
    return true;
}

/* A key under a section: from and to are measured from the section's start. */
typedef struct {
    PlanTime from;
    PlanTime to;
    PlanKey key;
} KeyPiece;

static bool key_pieces(const PiecePlan *plan, PlanSpan span, KeyPiece *out, size_t *count) {
    LaneSpanId ids[MAX_KEY_PIECES];
    PlanTime end;
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, span, ids, MAX_KEY_PIECES);
    if (found == 0 || found > MAX_KEY_PIECES || plan_span_end(span, &end) != PLAN_OK) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan key_span = plan_lane_span_span(plan, ids[i]);
        PlanTime key_end;
        PlanTime from = key_span.start;
        PlanTime to;
        if (plan_span_end(key_span, &key_end) != PLAN_OK) {
            return false;
        }
        to = plan_time_compare(key_end, end) < 0 ? key_end : end;
        from = plan_time_compare(from, span.start) > 0 ? from : span.start;
        if (!time_sub(from, span.start, &out[i].from) || !time_sub(to, span.start, &out[i].to)) {
            return false;
        }
        out[i].key = plan_lane_span_payload(plan, ids[i])->key;
    }
    *count = found;
    return true;
}

/* The key difference, in semitones from -5 to 6, a section returned in another is transposed by; false when the shapes differ. */
static bool key_shift(const PiecePlan *plan, PlanSpan source, PlanSpan target, int *shift) {
    KeyPiece a[MAX_KEY_PIECES];
    KeyPiece b[MAX_KEY_PIECES];
    size_t na;
    size_t nb;
    int difference = -1;
    if (!key_pieces(plan, source, a, &na) || !key_pieces(plan, target, b, &nb) || na != nb) {
        return false;
    }
    for (size_t i = 0; i < na; i++) {
        int step = ((b[i].key.tonic_pitch_class - a[i].key.tonic_pitch_class) % 12 + 12) % 12;
        if (plan_time_compare(a[i].from, b[i].from) != 0 || plan_time_compare(a[i].to, b[i].to) != 0 ||
            a[i].key.mode != b[i].key.mode || (difference >= 0 && difference != step)) {
            return false;
        }
        difference = step;
    }
    *shift = difference > 6 ? difference - 12 : difference;
    return true;
}

/* The variations the operator makes. */
static bool variation_offered(PlanReturnVariation variation) {
    return variation == PLAN_RETURN_EXACT;
}

bool form_return_source(const PiecePlan *plan, NodeId section, NodeId *source, PlanReturnVariation *variation) {
    PlanReturn ret;
    PlanMeter source_meter;
    PlanMeter target_meter;
    int shift;
    if (!plan_has_node(plan, section) || plan_node_kind(plan, section) != PLAN_NODE_SECTION ||
        plan_node_state(plan, section) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, section) != 0 ||
        !has_constraint(plan, section, PLAN_CONSTRAINT_ARCHETYPE) ||
        !has_constraint(plan, section, PLAN_CONSTRAINT_THEME_PAIRING) || !find_return(plan, section, &ret)) {
        return false;
    }
    if (ret.section == section || !plan_has_node(plan, ret.section) ||
        plan_node_kind(plan, ret.section) != PLAN_NODE_SECTION ||
        has_constraint(plan, ret.section, PLAN_CONSTRAINT_MATERIAL) || !variation_offered(ret.variation)) {
        return false;
    }
    PlanSpan from = plan_node_span(plan, ret.section);
    PlanSpan to = plan_node_span(plan, section);
    if (plan_time_compare(from.length, to.length) != 0 || !meter_over(plan, from, &source_meter) ||
        !meter_over(plan, to, &target_meter) || source_meter.beats_per_bar != target_meter.beats_per_bar ||
        source_meter.beat_unit != target_meter.beat_unit || !key_shift(plan, from, to, &shift)) {
        return false;
    }
    if (source) {
        *source = ret.section;
    }
    if (variation) {
        *variation = ret.variation;
    }
    return true;
}

bool form_return_handles(const PiecePlan *plan, NodeId section) {
    return form_return_source(plan, section, NULL, NULL);
}

/* Every lane span under the node, and the node's descendants', is written; the lanes seen are bits of *seen. */
static bool lanes_written(const PiecePlan *plan, NodeId node, unsigned *seen) {
    size_t count = plan_node_lane_spans(plan, node, NULL, 0);
    LaneSpanId *ids = count ? calloc(count, sizeof(LaneSpanId)) : NULL;
    bool written = count == 0 || ids;
    if (written) {
        plan_node_lane_spans(plan, node, ids, count);
    }
    for (size_t i = 0; written && i < count; i++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, ids[i]);
        if (is_written_lane(lane)) {
            written = plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED;
            *seen |= 1u << (unsigned)lane;
        }
    }
    free(ids);
    for (size_t i = 0; written && i < plan_node_child_count(plan, node); i++) {
        written = lanes_written(plan, plan_node_child_at(plan, node, i), seen);
    }
    return written;
}

/* The returned section is Realized and its harmony, bass and melody are all written. */
static bool source_ready(const PiecePlan *plan, NodeId source) {
    unsigned seen = 0;
    unsigned needed = (1u << PLAN_LANE_HARMONY) | (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_MELODY);
    return plan_node_state(plan, source) != PLAN_STATE_CONSTRAINED && lanes_written(plan, source, &seen) &&
           (seen & needed) == needed;
}

/* ------------------------------------------------------------- the operator */

static bool return_applies(const PiecePlan *plan, FillTarget target, void *context) {
    NodeId source;
    (void)context;
    return target.kind == FILL_TARGET_NODE && form_return_source(plan, target.id, &source, NULL) &&
           source_ready(plan, source);
}

/* Builds the payload of a return of the target: the returned section moved onto it, before the variation. */
static ReturnPayload *make_payload(const PiecePlan *plan, NodeId target, NodeId source, PlanReturnVariation variation) {
    KeyPiece pieces[MAX_KEY_PIECES];
    size_t count;
    ReturnPayload *payload = calloc(1, sizeof(*payload));
    if (!payload) {
        return NULL;
    }
    PlanSpan from = plan_node_span(plan, source);
    payload->magic = RETURN_PAYLOAD_MAGIC;
    payload->variation = variation;
    payload->span = plan_node_span(plan, target);
    if (!key_pieces(plan, payload->span, pieces, &count) || !meter_over(plan, payload->span, &payload->meter) ||
        !key_shift(plan, from, payload->span, &payload->transpose)) {
        free(payload);
        return NULL;
    }
    payload->key = pieces[0].key;
    if (!return_copy_read(plan, source, &payload->source) || !return_copy_clone(&payload->source, &payload->result) ||
        !return_copy_move(&payload->result, from.start, payload->span.start, payload->transpose)) {
        return_payload_destroy(payload);
        return NULL;
    }
    return payload;
}

static bool return_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    NodeId source;
    PlanReturnVariation variation;
    (void)rng;
    (void)context;
    if (target.kind != FILL_TARGET_NODE || !form_return_source(plan, target.id, &source, &variation)) {
        return false;
    }
    ReturnPayload *payload = make_payload(plan, target.id, source, variation);
    if (!payload) {
        return false;
    }
    return fill_candidates_add(out, "exact", payload, return_payload_destroy);
}

static PlanResult return_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const ReturnPayload *payload = candidate ? candidate->payload : NULL;
    (void)context;
    if (!payload || payload->magic != RETURN_PAYLOAD_MAGIC || target.kind != FILL_TARGET_NODE ||
        !form_return_handles(plan, target.id)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return return_copy_write(plan, target.id, &payload->result);
}

FillResult form_return_register(FillOperatorRegistry *registry, const MelodyFillParams *melody) {
    FillOperator operator_;
    if (!registry) {
        return FILL_ERR_NULL;
    }
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = FORM_RETURN_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan theme section with a RETURN to a realized section";
    operator_.generator.supported_outputs = "PiecePlan copy of the returned section's nodes and lane spans";
    operator_.generator.configuration_description = "the melody range a re-registered return keeps to";
    operator_.generator.constraints_description = "the returned section, shifted";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_GROW;
    operator_.applies = return_applies;
    operator_.generate = return_generate;
    operator_.apply = return_apply;
    operator_.context = (void *)melody;
    return fill_registry_register(registry, &operator_);
}

/* ------------------------------------------------------------- the checker */

static bool same_times(PlanTime a, PlanTime b) {
    return plan_time_compare(a, b) == 0;
}

static bool same_span(PlanSpan a, PlanSpan b) {
    return same_times(a.start, b.start) && same_times(a.length, b.length);
}

/* Whether the returned section moved onto the target by the payload's shift is what result holds. */
static bool result_is_moved_source(const ReturnPayload *payload, const PiecePlan *plan, NodeId target) {
    NodeId source;
    ReturnCopy moved;
    bool same = false;
    if (!form_return_source(plan, target, &source, NULL)) {
        return false;
    }
    PlanSpan from = plan_node_span(plan, source);
    if (!return_copy_clone(&payload->source, &moved)) {
        return false;
    }
    if (return_copy_move(&moved, from.start, payload->span.start, payload->transpose) &&
        moved.node_count == payload->result.node_count && moved.span_count == payload->result.span_count) {
        same = true;
        for (size_t i = 0; same && i < moved.node_count; i++) {
            const ReturnNode *a = &moved.nodes[i];
            const ReturnNode *b = &payload->result.nodes[i];
            same = a->kind == b->kind && a->parent == b->parent && same_span(a->span, b->span) &&
                   strcmp(a->name, b->name) == 0 && a->state == b->state && a->constraint_count == b->constraint_count;
        }
        for (size_t i = 0; same && i < moved.span_count; i++) {
            const ReturnSpan *a = &moved.spans[i];
            const ReturnSpan *b = &payload->result.spans[i];
            same = a->lane == b->lane && a->voice == b->voice && a->owner == b->owner && same_span(a->span, b->span) &&
                   a->state == b->state && a->constraint_count == b->constraint_count &&
                   a->note_count == b->note_count && strcmp(a->harmony.roman_numeral, b->harmony.roman_numeral) == 0;
            for (size_t n = 0; same && n < a->note_count; n++) {
                same = a->notes[n].midi_pitch == b->notes[n].midi_pitch &&
                       same_times(a->notes[n].offset, b->notes[n].offset) &&
                       same_times(a->notes[n].duration, b->notes[n].duration);
            }
        }
    }
    return_copy_destroy(&moved);
    return same;
}

static void reject(FillCheck *out, const char *reason) {
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    out->hard_violation = true;
    strncpy(out->reason, reason, sizeof(out->reason) - 1);
}

bool form_return_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, FORM_RETURN_OPERATOR_NAME) != 0) {
        return fill_default_checker(plan, target, operator_, candidate, context, out);
    }
    const ReturnPayload *payload = candidate->payload;
    PlanReturn ret;
    if (!payload || payload->magic != RETURN_PAYLOAD_MAGIC || target.kind != FILL_TARGET_NODE) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!find_return(plan, target.id, &ret) || ret.variation != payload->variation) {
        reject(out, "variation is not the one asked for");
        return true;
    }
    if (payload->variation == PLAN_RETURN_EXACT && !result_is_moved_source(payload, plan, target.id)) {
        reject(out, "exact return is not the returned section");
        return true;
    }
    out->score_numerator = 1;
    return true;
}
