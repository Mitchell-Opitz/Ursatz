#include <ursatz_composer/form_return.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "material/form_return_internal.h"
#include "util/util.h"
#include "structure/plan_util.h"
#include <plan/plan_constraint.h>
#include <model/pitch/pitch_arithmetic.h>

/* The most key spans a section is read across: a section holds at most two keys, with a few to spare. */
#define MAX_KEY_PIECES 8

/* ------------------------------------------------------------- the copy */

static PlanConstraint *copy_constraints(size_t count, const PlanConstraint *(*at)(const Plan *, size_t, size_t),
                                        const Plan *plan, size_t id) {
    PlanConstraint *out = count ? calloc(count, sizeof(PlanConstraint)) : NULL;
    for (size_t i = 0; out && i < count; i++) {
        out[i] = *at(plan, id, i);
    }
    return out;
}

static const PlanConstraint *node_constraint_at(const Plan *plan, size_t id, size_t index) {
    return plan_node_constraint_at(plan, (NodeId)id, index);
}

static const PlanConstraint *span_constraint_at(const Plan *plan, size_t id, size_t index) {
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

static size_t subtree_size(const Plan *plan, NodeId id) {
    size_t size = 1;
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        size += subtree_size(plan, plan_node_child_at(plan, id, i));
    }
    return size;
}

/* Reads the children of id (and theirs) into copy->nodes after the ones there, their node ids into ids. */
static bool collect_nodes(const Plan *plan, NodeId id, size_t parent, ReturnCopy *copy, NodeId *ids) {
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
        node->name = util_copy_string(plan_node_name(plan, child));
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
static bool collect_spans(const Plan *plan, NodeId owner, size_t index, ReturnCopy *copy) {
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

bool return_copy_read(const Plan *plan, NodeId section, ReturnCopy *out) {
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
        node->name = util_copy_string(from->nodes[i].name);
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

static bool same_times(Rational a, Rational b);

static bool move_span(PlanSpan *span, Rational delta) {
    return rational_add(span->start, delta, &span->start) == RATIONAL_OK;
}

bool return_copy_move(ReturnCopy *copy, Rational from, Rational to, int transpose) {
    Rational delta;
    if (rational_sub(to, from, &delta) != RATIONAL_OK) {
        return false;
    }
    for (size_t i = 0; i < copy->node_count; i++) {
        ReturnNode *node = &copy->nodes[i];
        if (!move_span(&node->span, delta)) {
            return false;
        }
        for (size_t c = 0; c < node->constraint_count; c++) {
            if (node->constraints[c].kind == PLAN_CONSTRAINT_REGION) {
                SpelledKey *key = &node->constraints[c].params.region;
                *key = plan_key_of_pitch_class(util_pitch_class(spelled_key_tonic_pitch_class(key) + transpose), key->material);
            }
        }
    }
    for (size_t i = 0; i < copy->span_count; i++) {
        ReturnSpan *span = &copy->spans[i];
        if (!move_span(&span->span, delta)) {
            return false;
        }
        for (size_t n = 0; n < span->note_count; n++) {
            span->notes[n].midi += transpose;
        }
        for (size_t c = 0; c < span->constraint_count; c++) {
            if (span->constraints[c].kind == PLAN_CONSTRAINT_REGION) {
                SpelledKey *key = &span->constraints[c].params.region;
                *key = plan_key_of_pitch_class(util_pitch_class(spelled_key_tonic_pitch_class(key) + transpose), key->material);
            }
        }
    }
    return true;
}

/* The pitch moved from the other mode of the tonic into `to` (R-499). */
int return_map_pitch(int tonic, const PitchMaterial *to, int pitch) {
    int degree = util_pitch_class(pitch - tonic);
    if (composer_material_is_minor(to)) {
        return degree == 4 || degree == 9 ? pitch - 1 : pitch;
    }
    return degree == 3 || degree == 8 || degree == 10 ? pitch + 1 : pitch;
}

static void map_key(PlanConstraint *constraint, const PitchMaterial *to) {
    if (constraint->kind == PLAN_CONSTRAINT_REGION) {
        SpelledKey *key = &constraint->params.region;
        *key = plan_key_of_pitch_class(spelled_key_tonic_pitch_class(key), to);
    }
}

bool return_copy_change_mode(ReturnCopy *copy, SpelledKey home, const PitchMaterial *to) {
    int tonic = spelled_key_tonic_pitch_class(&home);
    for (size_t i = 0; i < copy->node_count; i++) {
        for (size_t c = 0; c < copy->nodes[i].constraint_count; c++) {
            map_key(&copy->nodes[i].constraints[c], to);
        }
    }
    for (size_t i = 0; i < copy->span_count; i++) {
        ReturnSpan *span = &copy->spans[i];
        for (size_t c = 0; c < span->constraint_count; c++) {
            map_key(&span->constraints[c], to);
            /* The minor phrase model has one predominant: a half-cadence phrase of four bars deals a chord over two bars. */
            if (composer_material_is_minor(to) && span->constraints[c].kind == PLAN_CONSTRAINT_HARMONY_SHAPE) {
                span->constraints[c].params.harmony_shape.split_at_bars = false;
            }
        }
        if (span->lane == PLAN_LANE_MELODY || span->lane == PLAN_LANE_INNER_VOICE) {
            for (size_t n = 0; n < span->note_count; n++) {
                span->notes[n].midi = return_map_pitch(tonic, to, span->notes[n].midi);
                span->notes[n].spelled = false;
            }
        }
    }
    return true;
}

int return_mode_pitch(const ReturnPayload *payload, int pitch) {
    return payload->mode_change ? return_map_pitch(spelled_key_tonic_pitch_class(&payload->key), payload->to_material, pitch) : pitch;
}

PlanResult return_copy_write(Plan *plan, NodeId section, const ReturnCopy *copy, PlanNoteOperation operation) {
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
        PlanNote *written = NULL;
        if (span->lane == PLAN_LANE_HARMONY) {
            payload.harmony = span->harmony;
        } else {
            written = span->note_count ? calloc(span->note_count, sizeof(PlanNote)) : NULL;
            if (span->note_count && !written) {
                result = PLAN_ERR_ALLOC;
                break;
            }
            for (size_t n = 0; n < span->note_count; n++) {
                written[n] = span->notes[n];
                PlanNoteId from = written[n].id;
                written[n].id = PLAN_NOTE_NONE;
                written[n].source = PLAN_NOTE_NONE;
                written[n].operation = PLAN_NOTE_FROM_NONE;
                if (operation == PLAN_NOTE_FROM_RETURN && from != PLAN_NOTE_NONE) {
                    written[n].source = from;
                    written[n].operation = PLAN_NOTE_FROM_RETURN;
                }
            }
            payload.notes.notes = written;
            payload.notes.note_count = span->note_count;
        }
        result = plan_add_lane_span(plan, owner, span->lane, span->voice, span->span, &payload, &id);
        free(written);
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

static bool find_return(const Plan *plan, NodeId section, ReturnRef *out) {
    const SectionDescription *description = plan_section_or_empty(plan, section);
    NodeId source = description->returns[0] != '\0' ? plan_section_named(plan, description->returns) : PLAN_NODE_NONE;
    if (source == PLAN_NODE_NONE) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->section = source;
    out->variation = description->variation;
    out->setting = (PlanAccompanimentPattern)description->setting;
    out->across_modes = description->mode == SECTION_MODE_HOME;
    return true;
}

bool return_meter_over(const Plan *plan, PlanSpan span, PlanMeter *out) {
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
    Rational from;
    Rational to;
    SpelledKey key;
} KeyPiece;

static bool key_pieces(const Plan *plan, PlanSpan span, KeyPiece *out, size_t *count) {
    LaneSpanId ids[MAX_KEY_PIECES];
    Rational end;
    size_t found = plan_lane_find(plan, PLAN_LANE_REGION, 0, span, ids, MAX_KEY_PIECES);
    if (found == 0 || found > MAX_KEY_PIECES || plan_span_end(span, &end) != PLAN_OK) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan key_span = plan_lane_span_span(plan, ids[i]);
        Rational key_end;
        Rational from = key_span.start;
        Rational to;
        if (plan_span_end(key_span, &key_end) != PLAN_OK) {
            return false;
        }
        to = rational_cmp(key_end, end) < 0 ? key_end : end;
        from = rational_cmp(from, span.start) > 0 ? from : span.start;
        if (rational_sub(from, span.start, &out[i].from) != RATIONAL_OK || rational_sub(to, span.start, &out[i].to) != RATIONAL_OK) {
            return false;
        }
        out[i].key = plan_lane_span_payload(plan, ids[i])->region;
    }
    *count = found;
    return true;
}

/* The key difference, in semitones from -5 to 6, a section returned in another is transposed by; false when the shapes differ. */
bool return_key_shift(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                      int *shift) {
    KeyPiece a[MAX_KEY_PIECES];
    KeyPiece b[MAX_KEY_PIECES];
    size_t na;
    size_t nb;
    int difference = -1;
    if (!key_pieces(source_plan, source, a, &na) || !key_pieces(target_plan, target, b, &nb) || na != nb) {
        return false;
    }
    for (size_t i = 0; i < na; i++) {
        int step = util_pitch_class(spelled_key_tonic_pitch_class(&b[i].key) - spelled_key_tonic_pitch_class(&a[i].key));
        if (rational_cmp(a[i].from, b[i].from) != 0 || rational_cmp(a[i].to, b[i].to) != 0 ||
            composer_key_is_minor(&a[i].key) != composer_key_is_minor(&b[i].key) || (difference >= 0 && difference != step)) {
            return false;
        }
        difference = step;
    }
    *shift = pitch_class_step(0, difference, true);
    return true;
}

bool return_mode_shift_by(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                          int *shift, const PitchMaterial **to_material) {
    KeyPiece a[MAX_KEY_PIECES];
    KeyPiece b[MAX_KEY_PIECES];
    size_t na;
    size_t nb;
    int difference = -1;
    if (!key_pieces(source_plan, source, a, &na) || !key_pieces(target_plan, target, b, &nb) || na != nb) {
        return false;
    }
    for (size_t i = 0; i < na; i++) {
        int step = util_pitch_class(spelled_key_tonic_pitch_class(&b[i].key) - spelled_key_tonic_pitch_class(&a[i].key));
        if (rational_cmp(a[i].from, b[i].from) != 0 || rational_cmp(a[i].to, b[i].to) != 0 ||
            composer_key_is_minor(&a[i].key) == composer_key_is_minor(&b[i].key) || composer_key_is_minor(&b[i].key) != composer_key_is_minor(&b[0].key) || (difference >= 0 && difference != step)) {
            return false;
        }
        difference = step;
    }
    *shift = pitch_class_step(0, difference, true);
    *to_material = b[0].key.material;
    return true;
}

/* The copy across modes on one tonic (a figurate return, R-499): the shift must be 0. */
bool return_mode_shift(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                       const PitchMaterial **to_material) {
    int shift;
    return return_mode_shift_by(source_plan, source, target_plan, target, &shift, to_material) && shift == 0;
}

/* The variations the operator makes: all of them (figurate keeps the melody here; compose_run writes its figure, R-497). */
static bool variation_offered(SectionReturnVariation variation) {
    return variation == SECTION_RETURN_EXACT || variation == SECTION_RETURN_ORNAMENT ||
           variation == SECTION_RETURN_REHARMONIZE || variation == SECTION_RETURN_REREGISTER || variation == SECTION_RETURN_VARIED ||
           variation == SECTION_RETURN_FIGURATE || variation == SECTION_RETURN_EMBELLISH || variation == SECTION_RETURN_SETTING;
}

/* Whether the section's RETURN names another section of the piece that is as long, in the same meter and in keys of the same shape. */
static bool return_geometry(const Plan *plan, NodeId section, ReturnRef *ret) {
    PlanMeter source_meter;
    PlanMeter target_meter;
    int shift;
    if (!plan_has_node(plan, section) || plan_node_kind(plan, section) != PLAN_NODE_SECTION || !find_return(plan, section, ret)) {
        return false;
    }
    if (ret->section == section || !plan_has_node(plan, ret->section) ||
        plan_node_kind(plan, ret->section) != PLAN_NODE_SECTION ||
        plan_section_has_role(plan, ret->section, SECTION_ROLE_MATERIAL)) {
        return false;
    }
    PlanSpan from = plan_node_span(plan, ret->section);
    PlanSpan to = plan_node_span(plan, section);
    const PitchMaterial *to_material = NULL;
    return rational_cmp(from.length, to.length) == 0 && return_meter_over(plan, from, &source_meter) &&
           return_meter_over(plan, to, &target_meter) && source_meter.beats_per_bar == target_meter.beats_per_bar &&
           source_meter.beat_unit == target_meter.beat_unit &&
           (return_key_shift(plan, from, plan, to, &shift) ||
            (ret->variation == SECTION_RETURN_FIGURATE && return_mode_shift(plan, from, plan, to, &to_material)) ||
            /* A reharmonized return in the home mode (R-524): moved by the tonic step and mapped into the target's mode. */
            (ret->variation == SECTION_RETURN_REHARMONIZE && ret->across_modes &&
             return_mode_shift_by(plan, from, plan, to, &shift, &to_material)));
}

bool return_target(const Plan *plan, NodeId section, ReturnRef *ret) {
    return return_geometry(plan, section, ret);
}

bool form_return_copies(const Plan *plan, NodeId section) {
    ReturnRef ret;
    return return_geometry(plan, section, &ret);
}

bool form_return_mode_change(const Plan *plan, NodeId section, const PitchMaterial **to) {
    ReturnRef ret;
    int shift;
    const PitchMaterial *to_material = NULL;
    if (!return_geometry(plan, section, &ret)) {
        return false;
    }
    PlanSpan from = plan_node_span(plan, ret.section);
    PlanSpan target = plan_node_span(plan, section);
    if (return_key_shift(plan, from, plan, target, &shift) || !return_mode_shift_by(plan, from, plan, target, &shift, &to_material)) {
        return false;
    }
    *to = to_material;
    return true;
}

bool form_return_source(const Plan *plan, NodeId section, NodeId *source, SectionReturnVariation *variation) {
    ReturnRef ret;
    if (!plan_has_node(plan, section) || plan_node_kind(plan, section) != PLAN_NODE_SECTION ||
        plan_node_state(plan, section) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, section) != 0 ||
        !plan_section_or_empty(plan, section)->has_pairing || !return_geometry(plan, section, &ret) ||
        !variation_offered(ret.variation)) {
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

bool form_return_handles(const Plan *plan, NodeId section) {
    return form_return_source(plan, section, NULL, NULL);
}

/* Every lane span under the node, and the node's descendants', is written; the lanes seen are bits of *seen. */
static bool lanes_written(const Plan *plan, NodeId node, unsigned *seen) {
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
bool return_source_ready(const Plan *plan, NodeId source) {
    unsigned seen = 0;
    unsigned needed = (1u << PLAN_LANE_HARMONY) | (1u << PLAN_LANE_BASS) | (1u << PLAN_LANE_MELODY);
    return plan_node_state(plan, source) != PLAN_STATE_CONSTRAINED && lanes_written(plan, source, &seen) &&
           (seen & needed) == needed;
}

/* Whether a time is a whole number of cells. */
static bool on_cell(Rational time, Rational cell, size_t *index) {
    long numerator = time.num * (long)cell.den;
    long denominator = (long)time.den * cell.num;
    if (denominator <= 0 || numerator < 0 || numerator % denominator != 0) {
        return false;
    }
    *index = (size_t)(numerator / denominator);
    return true;
}

bool form_return_melody_fits(const Plan *plan, NodeId section) {
    PlanSpan span = plan_node_span(plan, section);
    KeyPiece pieces[MAX_KEY_PIECES];
    size_t piece_count;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t chord_count;
    PhraseMetre metre;
    if (!key_pieces(plan, span, pieces, &piece_count)) {
        return false;
    }
    if (piece_count > 1) {
        return true;
    }
    SpelledKey key = pieces[0].key;
    if (!harmony_read_lane(plan, span, harmony_key_mode(&key), chords, &chord_count, &metre)) {
        return false;
    }
    /* The cell each chord starts on. */
    size_t starts[HARMONY_MAX_CHORDS + 1];
    starts[0] = 0;
    for (size_t c = 0; c < chord_count; c++) {
        starts[c + 1] = starts[c] + chords[c].cells;
    }
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
    if (spans && !ids) {
        return false;
    }
    plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, spans);
    bool fits = true;
    for (size_t s = 0; fits && s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; fits && n < notes->note_count; n++) {
            Rational onset;
            Rational from_start;
            size_t cell;
            if (rational_add(lane_span.start, notes->notes[n].offset, &onset) != RATIONAL_OK ||
                rational_sub(onset, span.start, &from_start) != RATIONAL_OK) {
                fits = false;
            } else if (on_cell(from_start, metre.cell_length, &cell)) {
                size_t c = 0;
                while (c + 1 < chord_count && starts[c + 1] <= cell) {
                    c++;
                }
                unsigned mask;
                unsigned seventh;
                int pitch_class = util_pitch_class(notes->notes[n].midi);
                fits = cell < starts[chord_count] && melody_chord_mask(&key, &chords[c], &mask, &seventh) &&
                       ((mask >> pitch_class) & 1u);
            }
        }
    }
    free(ids);
    return fits;
}

/* A chord of a section's harmony lane: where it starts in the section, how long it is, and its numeral. */
typedef struct {
    Rational from;
    Rational length;
    char roman[PLAN_ROMAN_NUMERAL_MAX];
} ChordAt;

static ChordAt *read_chords(const Plan *plan, NodeId section, size_t *count) {
    PlanSpan span = plan_node_span(plan, section);
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, NULL, 0);
    LaneSpanId *ids = found ? calloc(found, sizeof(LaneSpanId)) : NULL;
    ChordAt *chords = found ? calloc(found, sizeof(ChordAt)) : NULL;
    *count = 0;
    if (!found || !ids || !chords) {
        free(ids);
        free(chords);
        return NULL;
    }
    plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, found);
    for (size_t i = 0; i < found; i++) {
        PlanSpan chord = plan_lane_span_span(plan, ids[i]);
        if (rational_sub(chord.start, span.start, &chords[i].from) != RATIONAL_OK) {
            free(ids);
            free(chords);
            return NULL;
        }
        chords[i].length = chord.length;
        memcpy(chords[i].roman, plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
    }
    free(ids);
    *count = found;
    return chords;
}

/* Whether the source has a chord of the same numeral at the same place and length. */
static bool chord_in_source(const ChordAt *chord, const ChordAt *source, size_t source_count) {
    for (size_t j = 0; j < source_count; j++) {
        if (same_times(chord->from, source[j].from) && same_times(chord->length, source[j].length) &&
            strcmp(chord->roman, source[j].roman) == 0) {
            return true;
        }
    }
    return false;
}

bool form_return_chords_changed(const Plan *plan, NodeId section, size_t *changed, size_t *total) {
    ReturnRef ret;
    size_t count;
    size_t source_count;
    if (!plan_has_node(plan, section) || !find_return(plan, section, &ret) || !plan_has_node(plan, ret.section)) {
        return false;
    }
    ChordAt *chords = read_chords(plan, section, &count);
    ChordAt *source = read_chords(plan, ret.section, &source_count);
    bool ok = chords && source;
    size_t different = 0;
    for (size_t i = 0; ok && i < count; i++) {
        different += !chord_in_source(&chords[i], source, source_count);
    }
    if (ok) {
        *changed = different;
        *total = count;
    }
    free(chords);
    free(source);
    return ok;
}

bool form_return_cadence_changed(const Plan *plan, NodeId section, bool *changed) {
    ReturnRef ret;
    size_t count;
    size_t source_count;
    if (!plan_has_node(plan, section) || !find_return(plan, section, &ret) || !plan_has_node(plan, ret.section)) {
        return false;
    }
    Rational length = plan_node_span(plan, section).length;
    ChordAt *chords = read_chords(plan, section, &count);
    ChordAt *source = read_chords(plan, ret.section, &source_count);
    bool ok = chords && source;
    bool different = false;
    for (size_t i = 0; ok && i < count; i++) {
        Rational share;
        ok = rational_div(chords[i].from, length, &share) == RATIONAL_OK;
        /* A chord that starts in the last quarter of the section. */
        if (ok && rational_cmp(share, (Rational){3, 4}) >= 0) {
            different = different || !chord_in_source(&chords[i], source, source_count);
        }
    }
    if (ok) {
        *changed = different;
    }
    free(chords);
    free(source);
    return ok;
}

bool form_return_bar_changed(const Plan *plan, NodeId section, unsigned bar, bool *changed) {
    ReturnRef ret;
    PlanMeter meter;
    size_t count;
    size_t source_count;
    if (bar < 1 || !plan_has_node(plan, section) || !find_return(plan, section, &ret) || !plan_has_node(plan, ret.section) ||
        !return_meter_over(plan, plan_node_span(plan, section), &meter)) {
        return false;
    }
    Rational from;
    Rational to;
    Rational length;
    plan_meter_bar_length(&meter, &length);
    if (rational_mul_int(length, (long long)bar - 1, &from) != RATIONAL_OK || rational_add(from, length, &to) != RATIONAL_OK) {
        return false;
    }
    ChordAt *chords = read_chords(plan, section, &count);
    ChordAt *source = read_chords(plan, ret.section, &source_count);
    bool ok = chords && source;
    bool different = false;
    for (size_t i = 0; ok && i < count; i++) {
        /* A chord that starts inside the bar. */
        if (rational_cmp(chords[i].from, from) >= 0 && rational_cmp(chords[i].from, to) < 0) {
            different = different || !chord_in_source(&chords[i], source, source_count);
        }
    }
    if (ok) {
        *changed = different;
    }
    free(chords);
    free(source);
    return ok;
}

/* The numerals of the chords that start in the last quarter of the section, space separated. */
static bool cadence_numerals(const Plan *plan, NodeId section, char *out, size_t size) {
    size_t count;
    ChordAt *chords = read_chords(plan, section, &count);
    Rational length = plan_node_span(plan, section).length;
    size_t used = 0;
    bool ok = chords != NULL && size > 0;
    out[0] = '\0';
    for (size_t i = 0; ok && i < count; i++) {
        Rational share;
        ok = rational_div(chords[i].from, length, &share) == RATIONAL_OK;
        if (ok && rational_cmp(share, (Rational){3, 4}) >= 0) {
            int wrote = snprintf(out + used, size - used, "%s%s", used ? " " : "", chords[i].roman);
            ok = wrote > 0 && (size_t)wrote < size - used;
            used += ok ? (size_t)wrote : 0;
        }
    }
    free(chords);
    return ok;
}

bool form_return_cadence_numerals(const Plan *plan, NodeId section, char *from, char *to, size_t size) {
    ReturnRef ret;
    return plan_has_node(plan, section) && find_return(plan, section, &ret) && plan_has_node(plan, ret.section) &&
           cadence_numerals(plan, ret.section, from, size) && cadence_numerals(plan, section, to, size);
}

bool form_return_edge_numerals(const Plan *plan, NodeId section, char *first, char *last, size_t size) {
    size_t count;
    ChordAt *chords = plan_has_node(plan, section) && size >= PLAN_ROMAN_NUMERAL_MAX ? read_chords(plan, section, &count) : NULL;
    if (!chords || count == 0) {
        free(chords);
        return false;
    }
    snprintf(first, size, "%s", chords[0].roman);
    snprintf(last, size, "%s", chords[count - 1].roman);
    free(chords);
    return true;
}

/* ------------------------------------------------------------- the operator */

static bool return_applies(const Plan *plan, FillTarget target, void *context) {
    NodeId source;
    (void)context;
    return target.kind == FILL_TARGET_NODE && form_return_source(plan, target.id, &source, NULL) &&
           return_source_ready(plan, source);
}

/* Builds the payload of a return of the target: the returned section moved onto it, before the variation. */
static ReturnPayload *make_payload(const Plan *plan, NodeId target, NodeId source, SectionReturnVariation variation) {
    KeyPiece pieces[MAX_KEY_PIECES];
    size_t count;
    ReturnPayload *payload = calloc(1, sizeof(*payload));
    if (!payload) {
        return NULL;
    }
    PlanSpan from = plan_node_span(plan, source);
    payload->magic = RETURN_PAYLOAD_MAGIC;
    payload->variation = variation;
    ReturnRef ret = {0};
    if (find_return(plan, target, &ret)) {
        payload->setting = ret.setting;
    }
    payload->from_start = from.start;
    payload->span = plan_node_span(plan, target);
    const PitchMaterial *to_material = NULL;
    if (!key_pieces(plan, payload->span, pieces, &count) || !return_meter_over(plan, payload->span, &payload->meter)) {
        free(payload);
        return NULL;
    }
    if (!return_key_shift(plan, from, plan, payload->span, &payload->transpose)) {
        /* A figurate return in the other mode of the same tonic (R-499): moved by 0, mapped, always reharmonized. A
         * reharmonized return with across_modes (R-524): moved by the tonic step, then mapped into the target's mode. */
        if (variation == SECTION_RETURN_FIGURATE && return_mode_shift(plan, from, plan, payload->span, &to_material)) {
            payload->transpose = 0;
        } else if (variation != SECTION_RETURN_REHARMONIZE || !ret.across_modes ||
                   !return_mode_shift_by(plan, from, plan, payload->span, &payload->transpose, &to_material)) {
            free(payload);
            return NULL;
        }
        payload->mode_change = true;
        payload->to_material = to_material;
    }
    payload->key = pieces[0].key;
    if (variation == SECTION_RETURN_FIGURATE) {
        const SectionDescription *described = plan_section_or_empty(plan, target);
        payload->reharmonize = payload->reharmonize || (described->has_figure && described->figure.reharmonize);
        payload->reharmonize = payload->reharmonize || payload->mode_change;
    }
    if (!return_copy_read(plan, source, &payload->source) || !return_copy_clone(&payload->source, &payload->result) ||
        !return_copy_move(&payload->result, from.start, payload->span.start, payload->transpose) ||
        (payload->mode_change && !return_copy_change_mode(&payload->result, payload->key, payload->to_material))) {
        return_payload_destroy(payload);
        return NULL;
    }
    return payload;
}

/* How many ornaments of the melody are proposed. */
#define ORNAMENT_CANDIDATES 4

static bool add_payload(FillCandidateList *out, const char *label, ReturnPayload *payload) {
    return fill_candidates_add(out, label, payload, return_payload_destroy);
}

static bool return_generate(const Plan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    const MelodyFillParams *range = context;
    MelodyFillParams defaults = melody_fill_params_default();
    NodeId source;
    SectionReturnVariation variation;
    if (target.kind != FILL_TARGET_NODE || !form_return_source(plan, target.id, &source, &variation)) {
        return false;
    }
    if (!range) {
        range = &defaults;
    }
    if (variation == SECTION_RETURN_ORNAMENT || variation == SECTION_RETURN_VARIED || variation == SECTION_RETURN_EMBELLISH) {
        ReturnPayload *kept[ORNAMENT_CANDIDATES];
        size_t kept_count = 0;
        for (size_t draw = 0; draw < 2 * ORNAMENT_CANDIDATES && kept_count < ORNAMENT_CANDIDATES; draw++) {
            ReturnPayload *payload = make_payload(plan, target.id, source, variation);
            if (!payload) {
                return false;
            }
            bool repeat = !return_vary(payload, rng);
            for (size_t i = 0; !repeat && i < kept_count; i++) {
                repeat = return_same_melody(kept[i], payload);
            }
            if (repeat) {
                return_payload_destroy(payload);
                continue;
            }
            kept[kept_count++] = payload;
            if (!add_payload(out, variation == SECTION_RETURN_VARIED ? "varied" : (variation == SECTION_RETURN_EMBELLISH ? "embellish" : "ornament"), payload)) {
                return false;
            }
        }
        return true;
    }
    /* A re-registration is proposed up and down; the one that leaves the range is not made. */
    int octaves[2] = {12, -12};
    for (size_t i = 0; i < (variation == SECTION_RETURN_REREGISTER ? 2u : 1u); i++) {
        ReturnPayload *payload = make_payload(plan, target.id, source, variation);
        if (!payload) {
            return false;
        }
        payload->octave = variation == SECTION_RETURN_REREGISTER ? octaves[i] : 0;
        payload->range_low = range->range_low;
        payload->range_high = range->range_high;
        if (!return_vary(payload, rng)) {
            return_payload_destroy(payload);
            continue;
        }
        const char *label = variation == SECTION_RETURN_EXACT       ? "exact"
                            : variation == SECTION_RETURN_REHARMONIZE ? "reharmonize"
                            : variation == SECTION_RETURN_FIGURATE    ? "figurate"
                            : variation == SECTION_RETURN_SETTING     ? "setting"
                            : i == 0                              ? "octave up"
                                                                  : "octave down";
        if (!add_payload(out, label, payload)) {
            return false;
        }
    }
    return true;
}

static PlanResult return_apply(Plan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const ReturnPayload *payload = candidate ? candidate->payload : NULL;
    (void)context;
    if (!payload || payload->magic != RETURN_PAYLOAD_MAGIC || target.kind != FILL_TARGET_NODE ||
        !form_return_handles(plan, target.id)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    return return_copy_write(plan, target.id, &payload->result, PLAN_NOTE_FROM_RETURN);
}

FillResult form_return_register(FillOperatorRegistry *registry, const MelodyFillParams *melody) {
    FillOperator operator_;
    if (!registry) {
        return FILL_ERR_NULL;
    }
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = FORM_RETURN_OPERATOR_NAME;
    operator_.generator.supported_inputs = "Plan theme section with a RETURN to a realized section";
    operator_.generator.supported_outputs = "Plan copy of the returned section's nodes and lane spans";
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

static bool same_times(Rational a, Rational b) {
    return rational_cmp(a, b) == 0;
}

static bool same_span(PlanSpan a, PlanSpan b) {
    return same_times(a.start, b.start) && same_times(a.length, b.length);
}

bool return_same_notes(const ReturnSpan *a, const ReturnSpan *b, int shift, const ReturnPayload *map_or_null) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t n = 0; n < a->note_count; n++) {
        int pitch = a->notes[n].midi + shift;
        if ((map_or_null ? return_mode_pitch(map_or_null, pitch) : pitch) != b->notes[n].midi ||
            !same_times(a->notes[n].offset, b->notes[n].offset) || !same_times(a->notes[n].duration, b->notes[n].duration)) {
            return false;
        }
    }
    return true;
}

/* The index of the next span at or after i that the comparison counts; the count when there is none. */
static size_t next_counted(const ReturnCopy *copy, size_t i, const ReturnAllow *allow) {
    while (i < copy->span_count && allow && (allow->ignored & (1u << (unsigned)copy->spans[i].lane))) {
        i++;
    }
    return i;
}

bool return_result_matches(const ReturnPayload *payload, const ReturnAllow *allow) {
    ReturnCopy moved;
    bool same = false;
    if (!return_copy_clone(&payload->source, &moved)) {
        return false;
    }
    if (return_copy_move(&moved, payload->from_start, payload->span.start, payload->transpose) &&
        moved.node_count == payload->result.node_count && (allow || moved.span_count == payload->result.span_count)) {
        same = true;
        for (size_t i = 0; same && i < moved.node_count; i++) {
            const ReturnNode *a = &moved.nodes[i];
            const ReturnNode *b = &payload->result.nodes[i];
            same = a->kind == b->kind && a->parent == b->parent && same_span(a->span, b->span) &&
                   strcmp(a->name, b->name) == 0 && a->state == b->state && a->constraint_count == b->constraint_count;
        }
        /* The spans are paired in order, those of an ignored lane left out on both sides. */
        size_t i = next_counted(&moved, 0, allow);
        size_t j = next_counted(&payload->result, 0, allow);
        for (; same && i < moved.span_count && j < payload->result.span_count;
             i = next_counted(&moved, i + 1, allow), j = next_counted(&payload->result, j + 1, allow)) {
            const ReturnSpan *a = &moved.spans[i];
            const ReturnSpan *b = &payload->result.spans[j];
            unsigned bit = 1u << (unsigned)a->lane;
            same = a->lane == b->lane && a->voice == b->voice && a->owner == b->owner && same_span(a->span, b->span) &&
                   a->state == b->state && a->constraint_count == b->constraint_count &&
                   strcmp(a->harmony.roman_numeral, b->harmony.roman_numeral) == 0;
            if (same && !(allow && (allow->skip_notes & bit))) {
                same = return_same_notes(a, b, allow && (allow->shifted & bit) ? allow->shift : 0, NULL);
            }
        }
        same = same && i >= moved.span_count && j >= payload->result.span_count;
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

bool form_return_checker(const Plan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, FORM_RETURN_OPERATOR_NAME) != 0) {
        return fill_default_checker(plan, target, operator_, candidate, context, out);
    }
    const ReturnPayload *payload = candidate->payload;
    ReturnRef ret;
    if (!payload || payload->magic != RETURN_PAYLOAD_MAGIC || target.kind != FILL_TARGET_NODE) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!find_return(plan, target.id, &ret) || ret.variation != payload->variation) {
        reject(out, "variation is not the one asked for");
        return true;
    }
    if (payload->variation == SECTION_RETURN_EXACT) {
        if (!return_result_matches(payload, NULL)) {
            reject(out, "exact return is not the returned section");
            return true;
        }
        out->score_numerator = 1;
        return true;
    }
    return_vary_check(payload, out);
    return true;
}
