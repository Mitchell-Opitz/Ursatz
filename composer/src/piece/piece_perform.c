#include <ursatz_composer/piece_perform.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <plan/plan_constraint.h>
#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/given.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <plan/plan_state.h>

#include "piece/piece_internal.h"
#include "util/plan_util.h"

/*
 * The performance of a composed piece (R-485): the sections of the composed plan in the order they are played. See
 * piece_perform.h.
 */

/* The most key lane spans read over one section. */
#define PERFORM_MAX_KEY_SPANS 8

/* The sections of the composed plan in order, the material left out. */
static size_t composed_sections(const PiecePlan *plan, NodeId *out, size_t capacity) {
    NodeId root = plan_root(plan);
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (plan_node_kind(plan, child) == PLAN_NODE_SECTION && !plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            if (count < capacity) {
                out[count] = child;
            }
            count++;
        }
    }
    return count;
}

/* The meter of the composed plan over its first bar. */
static bool plan_meter(const PiecePlan *plan, PlanMeter *meter) {
    LaneSpanId id;
    PlanSpan first = {{0, 1}, {1, 64}};
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, first, &id, 1) < 1) {
        return false;
    }
    *meter = plan_lane_span_payload(plan, id)->meter;
    return true;
}

/* The key lane over the source section, cut to it and moved to start, added to the performed plan under its form. */
static PieceResult copy_keys(PiecePlan *performed, NodeId form, const PiecePlan *plan, PlanSpan source, Rational start) {
    LaneSpanId ids[PERFORM_MAX_KEY_SPANS];
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, source, ids, PERFORM_MAX_KEY_SPANS);
    Rational source_end;
    if (found == 0 || found > PERFORM_MAX_KEY_SPANS || plan_span_end(source, &source_end) != PLAN_OK) {
        return PIECE_ERR_BAD_REQUEST;
    }
    for (size_t i = 0; i < found; i++) {
        PlanSpan key_span = plan_lane_span_span(plan, ids[i]);
        Rational key_end;
        Rational from;
        Rational to;
        PlanSpan moved;
        PlanLanePayload payload;
        LaneSpanId id;
        if (plan_span_end(key_span, &key_end) != PLAN_OK) {
            return PIECE_ERR_BAD_REQUEST;
        }
        from = rational_cmp(key_span.start, source.start) > 0 ? key_span.start : source.start;
        to = rational_cmp(key_end, source_end) < 0 ? key_end : source_end;
        Rational offset;
        if (rational_sub(from, source.start, &offset) != RATIONAL_OK || rational_add(start, offset, &moved.start) != RATIONAL_OK ||
            rational_sub(to, from, &moved.length) != RATIONAL_OK) {
            return PIECE_ERR_BAD_REQUEST;
        }
        memset(&payload, 0, sizeof(payload));
        payload.key = plan_lane_span_payload(plan, ids[i])->key;
        PieceResult result = piece_result_of_plan(plan_add_lane_span(performed, form, PLAN_LANE_KEY, 0, moved, &payload, &id));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(performed, id, PLAN_STATE_GIVEN)) : result;
        if (result != PIECE_OK) {
            return result;
        }
    }
    return PIECE_OK;
}

/* The constraints a performed section holds: the source's letter, cadences and key. */
static PieceResult copy_constraints(PiecePlan *performed, NodeId node, const PiecePlan *plan, NodeId source) {
    const SectionDescription *from = plan_section_or_empty(plan, source);
    SectionDescription description = *plan_node_section(performed, node);
    snprintf(description.letter, sizeof(description.letter), "%s", from->letter);
    description.has_cadence = from->has_cadence;
    description.cadence = from->cadence;
    description.has_first_cadence = from->has_first_cadence;
    description.first_cadence = from->first_cadence;
    PieceResult result = piece_result_of_plan(plan_node_set_section(performed, node, &description));
    for (size_t i = 0; result == PIECE_OK && i < plan_node_constraint_count(plan, source); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, source, i);
        if (constraint->kind == PLAN_CONSTRAINT_KEY) {
            result = piece_add_constraint(performed, node, constraint);
        }
    }
    return result;
}

/* Whether the section at `index` of a part's size is the first or the last of a repeated group, and whether it is in one. */
static bool in_repeat(const FormSpec *form, size_t index, size_t *first, size_t *count) {
    for (size_t i = 0; i < form->repeat_count; i++) {
        if (index >= form->repeats[i].first && index < form->repeats[i].first + form->repeats[i].count) {
            *first = form->repeats[i].first;
            *count = form->repeats[i].count;
            return true;
        }
    }
    return false;
}

/* The sections of the performance in order: (part, source index, repeat). */
typedef struct {
    size_t part;
    size_t index;
    bool repeat;
} Step;

static bool order_steps(const PiecePerformPart *parts, size_t part_count, Step *steps, size_t *count) {
    *count = 0;
    for (size_t p = 0; p < part_count; p++) {
        const FormSpecSize *size = parts[p].size;
        size_t index = 0;
        while (index < size->section_count) {
            size_t first = 0;
            size_t span = 0;
            size_t times = 1;
            size_t length = 1;
            if (parts[p].repeats && in_repeat(parts[p].form, index, &first, &span)) {
                times = 2;
                length = span;
            }
            for (size_t time = 0; time < times; time++) {
                for (size_t j = 0; j < length; j++) {
                    if (*count == PIECE_PERFORMED_MAX) {
                        return false;
                    }
                    steps[(*count)++] = (Step){p, index + j, time == 1};
                }
            }
            index += length;
        }
    }
    return true;
}

void piece_performance_destroy(PiecePerformance *performance) {
    if (performance) {
        plan_destroy(performance->plan);
        memset(performance, 0, sizeof(*performance));
    }
}

void piece_parts_destroy(PieceParts *parts) {
    if (!parts) {
        return;
    }
    for (size_t i = 0; i < FORM_SPEC_MAX_PARTS_OF; i++) {
        plan_destroy(parts->plans[i]);
        compose_outcome_destroy(&parts->outcomes[i]);
        form_spec_destroy(parts->forms[i]);
    }
    memset(parts, 0, sizeof(*parts));
}

PieceResult piece_compose_parts(const FormSpec *form, const PieceRequest *request, const ComposeConfig *config,
                                PieceParts *out) {
    if (!form || !request || !config || !out) {
        return PIECE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (form->part_count == 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    const FormSpecSize *size = form_spec_size(form, request->bars, request->count);
    if (!size) {
        return PIECE_ERR_BAD_SIZE;
    }
    /* The keys first, so that a part's key does not depend on how another part composed. */
    FillRng rng;
    fill_rng_seed(&rng, request->seed);
    for (size_t i = 0; i < form->part_count; i++) {
        const FormPart *part = &form->parts[i];
        if (part->again != FORM_SPEC_NO_RETURN) {
            out->keys[i] = out->keys[part->again];
        } else if (part->key == FORM_SPEC_HOME) {
            out->keys[i] = request->key;
        } else if (!piece_region_key(config->knowledge, request->key, &form->regions[part->key], &rng, &out->keys[i])) {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    out->count = form->part_count;
    PieceResult result = PIECE_OK;
    for (size_t i = 0; result == PIECE_OK && i < form->part_count; i++) {
        const FormPart *part = &form->parts[i];
        if (part->again != FORM_SPEC_NO_RETURN) {
            continue;
        }
        PieceRequest made = *request;
        ComposeConfig composing = *config;
        made.form = part->form;
        made.bars = part->bars ? part->bars : size->bars;
        made.count = 0;
        made.key = out->keys[i];
        made.seed = request->seed + PIECE_PART_SEED_STRIDE * (unsigned long)i;
        composing.seed = made.seed;
        if (part->new_motive) {
            made.motive = NULL;
            made.source = NULL;
            composing.basic = NULL;
        }
        if (form_spec_builtin(part->form, &out->forms[i]) != FORM_SPEC_OK) {
            result = PIECE_ERR_BAD_REQUEST;
            break;
        }
        result = piece_build_plan(&made, &out->plans[i]);
        if (result == PIECE_OK) {
            result = compose_run(out->plans[i], &composing, &out->outcomes[i]);
        }
    }
    if (result != PIECE_OK) {
        piece_parts_destroy(out);
    }
    return result;
}

static PieceResult perform(const PiecePerformPart *parts, size_t part_count, const FormSpec *record_form, const char *plan_name,
                           PiecePerformance *out) {
    if (!parts || part_count == 0 || !out) {
        return PIECE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    PlanMeter meter;
    Step steps[PIECE_PERFORMED_MAX];
    size_t step_count;
    NodeId sources[PIECE_PERFORMED_MAX][FORM_SPEC_MAX_SECTIONS];
    for (size_t p = 0; p < part_count; p++) {
        PlanMeter part_meter;
        if (!parts[p].form || !parts[p].size || !parts[p].plan || p >= PIECE_PERFORMED_MAX ||
            composed_sections(parts[p].plan, sources[p], FORM_SPEC_MAX_SECTIONS) != parts[p].size->section_count ||
            !plan_meter(parts[p].plan, &part_meter)) {
            return PIECE_ERR_BAD_REQUEST;
        }
        if (p == 0) {
            meter = part_meter;
        } else if (part_meter.beats_per_bar != meter.beats_per_bar || part_meter.beat_unit != meter.beat_unit) {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    if (!order_steps(parts, part_count, steps, &step_count)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    /* The length of the performance: the sections' lengths added. */
    PlanSpan whole = {{0, 1}, {0, 1}};
    for (size_t i = 0; i < step_count; i++) {
        PlanSpan span = plan_node_span(parts[steps[i].part].plan, sources[steps[i].part][steps[i].index]);
        if (rational_add(whole.length, span.length, &whole.length) != RATIONAL_OK) {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    PiecePlan *performed = plan_create();
    if (!performed) {
        return PIECE_ERR_ALLOC;
    }
    NodeId form_id = PLAN_NODE_NONE;
    PlanLanePayload payload;
    LaneSpanId lane;
    PieceResult result = piece_result_of_plan(
        plan_add_node(performed, PLAN_NODE_NONE, PLAN_NODE_FORM, whole, plan_name, &form_id));
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = meter;
        result = piece_result_of_plan(plan_add_lane_span(performed, form_id, PLAN_LANE_METER, 0, whole, &payload, &lane));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(performed, lane, PLAN_STATE_GIVEN)) : result;
    }
    Rational start = {0, 1};
    for (size_t i = 0; result == PIECE_OK && i < step_count; i++) {
        const PiecePerformPart *part = &parts[steps[i].part];
        NodeId source = sources[steps[i].part][steps[i].index];
        const SectionDescription *spec = &part->size->sections[steps[i].index];
        PiecePerformedSection *section = &out->sections[i];
        PlanSpan source_span = plan_node_span(part->plan, source);
        PlanSpan span = {start, source_span.length};
        const char *prefix = steps[i].part > 0 && part->name ? part->name : NULL;
        snprintf(section->name, sizeof(section->name), "%s%s%s%s", prefix ? prefix : "", prefix ? " " : "", spec->name,
                 steps[i].repeat ? " (repeat)" : "");
        section->part = steps[i].part;
        section->source = source;
        section->start = start;
        section->repeat = steps[i].repeat;
        result = piece_result_of_plan(plan_add_node(performed, form_id, PLAN_NODE_SECTION, span, section->name, &section->node));
        result = result == PIECE_OK ? copy_constraints(performed, section->node, part->plan, source) : result;
        result = result == PIECE_OK ? copy_keys(performed, form_id, part->plan, source_span, start) : result;
        if (result == PIECE_OK) {
            GivenResult given = plan_give_section(performed, section->node, part->plan, source);
            result = given == GIVEN_OK ? PIECE_OK : PIECE_ERR_BAD_REQUEST;
        }
        if (result == PIECE_OK) {
            out->section_count = i + 1;
            result = piece_result_of_plan(plan_result_of_rational(rational_add(start, source_span.length, &start)));
        }
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(performed, form_id, PLAN_STATE_GIVEN));
    }
    result = result == PIECE_OK && record_form ? piece_set_form_record(performed, record_form) : result;
    if (result != PIECE_OK) {
        plan_destroy(performed);
        memset(out, 0, sizeof(*out));
        return result;
    }
    out->plan = performed;
    return PIECE_OK;
}

PieceResult piece_perform(const PiecePerformPart *parts, size_t part_count, PiecePerformance *out) {
    const FormSpec *form = parts && part_count > 0 ? parts[0].form : NULL;
    return perform(parts, part_count, form, form ? form->plan_name : NULL, out);
}

size_t piece_parts_source(const FormSpec *form, size_t index) {
    return form->parts[index].again == FORM_SPEC_NO_RETURN ? index : form->parts[index].again;
}

PieceResult piece_perform_compound(const FormSpec *form, size_t bars, size_t count, const PieceParts *parts,
                                   PiecePerformance *out) {
    if (!form || !parts || !out) {
        return PIECE_ERR_NULL;
    }
    const FormSpecSize *size = form_spec_size(form, bars, count);
    PiecePerformPart performed[FORM_SPEC_MAX_PARTS_OF];
    if (!size || form->part_count == 0 || parts->count != form->part_count) {
        return PIECE_ERR_BAD_REQUEST;
    }
    for (size_t i = 0; i < form->part_count; i++) {
        size_t source = piece_parts_source(form, i);
        if (!parts->forms[source] || !parts->plans[source]) {
            return PIECE_ERR_BAD_REQUEST;
        }
        performed[i].form = parts->forms[source];
        performed[i].size =
            form_spec_size(parts->forms[source], form->parts[source].bars ? form->parts[source].bars : size->bars, 0);
        performed[i].plan = parts->plans[source];
        performed[i].repeats = form->parts[i].repeats;
        performed[i].name = form->parts[i].name;
    }
    return perform(performed, form->part_count, form, form->plan_name, out);
}
