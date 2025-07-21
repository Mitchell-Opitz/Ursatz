#include <ursatz_composer/motive_render.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>
#include "motive/motive_internal.h"

static bool add_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out) {
    return plan_add_node(plan, parent, kind, span, NULL, out) == PLAN_OK &&
           plan_node_set_state(plan, *out, PLAN_STATE_GIVEN) == PLAN_OK;
}

static bool add_given_span(PiecePlan *plan, NodeId owner, PlanLaneKind lane, PlanSpan span, const PlanLanePayload *payload) {
    LaneSpanId id;
    return plan_add_lane_span(plan, owner, lane, 0, span, payload, &id) == PLAN_OK &&
           plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
}

PiecePlan *motive_render_plan(const Motive *motive) {
    if (!motive || motive->note_count == 0) {
        return NULL;
    }
    PiecePlan *plan = plan_create();
    if (!plan) {
        return NULL;
    }
    PlanSpan all = {{0, 1}, motive->length};
    NodeId form;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    bool ok = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, &form) &&
              add_node(plan, form, PLAN_NODE_SECTION, all, &section) &&
              add_node(plan, section, PLAN_NODE_PHRASE, all, &phrase) &&
              add_node(plan, phrase, PLAN_NODE_IDEA, all, &idea) &&
              motive_attach(plan, idea, (PlanTime){0, 1}, motive, "motive", NULL, NULL) == PLAN_OK;

    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    bool has_pickup = plan_time_compare(motive->pickup, (PlanTime){0, 1}) > 0;
    if (ok && has_pickup && plan_time_compare(motive->pickup, motive->length) < 0) {
        PlanSpan lead = {{0, 1}, motive->pickup};
        PlanTime rest_length = motive_time_minus(motive->length, motive->pickup);
        payload.meter.beats_per_bar = (int)motive->pickup.num;
        payload.meter.beat_unit = (int)motive->pickup.den;
        ok = add_given_span(plan, idea, PLAN_LANE_METER, lead, &payload);
        PlanSpan rest = {motive->pickup, rest_length};
        payload.meter = motive->meter;
        ok = ok && add_given_span(plan, idea, PLAN_LANE_METER, rest, &payload);
    } else if (ok) {
        payload.meter = motive->meter;
        if (has_pickup) {
            payload.meter.beats_per_bar = (int)motive->pickup.num;
            payload.meter.beat_unit = (int)motive->pickup.den;
        }
        ok = add_given_span(plan, idea, PLAN_LANE_METER, all, &payload);
    }
    if (ok && motive->key_source != MOTIVE_KEY_UNSPECIFIED) {
        memset(&payload, 0, sizeof(payload));
        payload.key = motive->key;
        ok = add_given_span(plan, idea, PLAN_LANE_KEY, all, &payload);
    }
    if (!ok) {
        plan_destroy(plan);
        return NULL;
    }
    return plan;
}

static PlanExportResult render_plan_for(const Motive *motive, PiecePlan **out, PlanExportError *error) {
    *out = motive_render_plan(motive);
    if (!*out) {
        if (error) {
            snprintf(error->message, sizeof(error->message), "the motive cannot be placed in a plan");
        }
        return motive ? PLAN_EXPORT_ERR_NOT_REPRESENTABLE : PLAN_EXPORT_ERR_NULL;
    }
    return PLAN_EXPORT_OK;
}

PlanExportResult motive_export_midi(const Motive *motive, const PlanExportOptions *options, const char *path,
                                    PlanExportError *error) {
    PiecePlan *plan;
    PlanExportResult result = render_plan_for(motive, &plan, error);
    if (result == PLAN_EXPORT_OK) {
        result = plan_export_midi(plan, options, path, error);
        plan_destroy(plan);
    }
    return result;
}

PlanExportResult motive_export_musicxml(const Motive *motive, const PlanExportOptions *options, const char *path,
                                        PlanExportError *error) {
    PiecePlan *plan;
    PlanExportResult result = render_plan_for(motive, &plan, error);
    if (result == PLAN_EXPORT_OK) {
        result = plan_export_musicxml(plan, options, path, error);
        plan_destroy(plan);
    }
    return result;
}
