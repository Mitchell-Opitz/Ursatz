#include <string.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive_analysis.h>

#include "grow/grow_internal.h"
#include "util/plan_util.h"

/*
 * An idea's fidelity to its source, read from the plan (R-354). The idea's melody span names its motive node (the span's
 * MOTIVE_REF); that node's own MOTIVE_REF says what it is made from. The realized notes are the span's notes inside the
 * node's length, in the key and meter of the lanes over the span. Pure reads: nothing is written to the plan.
 */

/*
 * The least fidelity an idea may have, by development setting and role (R-355). Balanced: presentation 3/4, continuation 1/2,
 * cadential 3/10, contrast and none 0 (a contrasting idea is meant to stray; it is scored and shown, never refused). The
 * melody setting keeps the ideas closer, 1/10 higher; the theme setting lets them stray, 1/10 lower.
 */
static const MotiveScore FIDELITY_MINIMUM[GROW_DEVELOP_COUNT][GROW_ROLE_CONTRAST + 1] = {
    [GROW_DEVELOP_BALANCED] = {[GROW_ROLE_NONE] = {0, 1},
                               [GROW_ROLE_PRESENTATION] = {3, 4},
                               [GROW_ROLE_CONTINUATION] = {1, 2},
                               [GROW_ROLE_CADENTIAL] = {3, 10},
                               [GROW_ROLE_CONTRAST] = {0, 1}},
    [GROW_DEVELOP_MELODY] = {[GROW_ROLE_NONE] = {0, 1},
                             [GROW_ROLE_PRESENTATION] = {17, 20},
                             [GROW_ROLE_CONTINUATION] = {3, 5},
                             [GROW_ROLE_CADENTIAL] = {2, 5},
                             [GROW_ROLE_CONTRAST] = {0, 1}},
    [GROW_DEVELOP_THEME] = {[GROW_ROLE_NONE] = {0, 1},
                            [GROW_ROLE_PRESENTATION] = {13, 20},
                            [GROW_ROLE_CONTINUATION] = {2, 5},
                            [GROW_ROLE_CADENTIAL] = {1, 5},
                            [GROW_ROLE_CONTRAST] = {0, 1}},
};

MotiveScore grow_fidelity_minimum(GrowDevelopment development, GrowIdeaRole role) {
    MotiveScore none = {0, 1};
    if ((int)development < 0 || development >= GROW_DEVELOP_COUNT || (int)role < GROW_ROLE_NONE || role > GROW_ROLE_CONTRAST) {
        return none;
    }
    return FIDELITY_MINIMUM[development][role];
}

GrowIdeaRole grow_idea_role(const char *idea_name) {
    static const struct {
        const char *name;
        GrowIdeaRole role;
    } ROLES[] = {{"basic idea", GROW_ROLE_PRESENTATION},
                 {"repetition", GROW_ROLE_PRESENTATION},
                 {"consequent basic idea", GROW_ROLE_PRESENTATION},
                 {"fragment 1", GROW_ROLE_CONTINUATION},
                 {"fragment 2", GROW_ROLE_CONTINUATION},
                 {"cadential", GROW_ROLE_CADENTIAL},
                 {"cadential 1", GROW_ROLE_CADENTIAL},
                 {"cadential 2", GROW_ROLE_CADENTIAL},
                 {"contrasting idea", GROW_ROLE_CONTRAST},
                 {"consequent contrasting idea", GROW_ROLE_CONTRAST}};
    for (size_t i = 0; idea_name && i < sizeof(ROLES) / sizeof(ROLES[0]); i++) {
        if (strcmp(idea_name, ROLES[i].name) == 0) {
            return ROLES[i].role;
        }
    }
    return GROW_ROLE_NONE;
}

/* The meter and key of the lanes over the span. */
static bool read_meter_key(const PiecePlan *plan, PlanSpan span, PlanMeter *meter, AnalysisKey *key) {
    LaneSpanId id;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, &id, 1) < 1) {
        return false;
    }
    *meter = plan_lane_span_payload(plan, id)->meter;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, &id, 1) < 1) {
        return false;
    }
    *key = plan_lane_span_payload(plan, id)->key;
    return true;
}

/* The end of the last note: the length of a motive's notes. */
static bool notes_end(const PlanNotes *notes, size_t count, Rational *out) {
    return count > 0 && rational_add(notes->notes[count - 1].offset, notes->notes[count - 1].duration, out) == RATIONAL_OK;
}

/*
 * The theme's motive m0 as the plan holds it, with the compose run's basic motive's features and pickup when m0 is its
 * first notes (grow_theme.c's draw_segments does the same), so its segments are the run's.
 */
static bool read_m0(const PiecePlan *plan, const Motive *basic, const PlanMeter *meter, const AnalysisKey *key, Motive *m0) {
    NodeId node = grow_find_source(plan);
    if (node == PLAN_NODE_NONE || !grow_read_motive(plan, node, meter, key, m0)) {
        return false;
    }
    bool same = basic && basic->note_count >= m0->note_count;
    for (size_t i = 0; same && i < m0->note_count; i++) {
        same = motive_pitch(basic, i) == motive_pitch(m0, i) && rational_cmp(basic->notes[i].onset, m0->notes[i].onset) == 0 &&
               rational_cmp(basic->notes[i].duration, m0->notes[i].duration) == 0;
    }
    if (same) {
        m0->pickup = basic->pickup;
        m0->feature_count = 0;
        for (size_t f = 0; f < basic->feature_count; f++) {
            if (basic->features[f].last < m0->note_count) {
                m0->features[m0->feature_count++] = basic->features[f];
            }
        }
    }
    return true;
}

/* The segment of m0 the label names, as a motive of its own. */
static bool read_segment(const Motive *m0, const char *label, Motive *out) {
    MotiveAnalysis analysis;
    if (!motive_analyze(m0, &analysis)) {
        return false;
    }
    for (size_t s = 0; s < analysis.segment_count && s < MOTIVE_MAX_FEATURES; s++) {
        if (strcmp(analysis.segments[s].label, label) == 0) {
            return grow_segment_motive(m0, &analysis.segments[s], out);
        }
    }
    return false;
}

bool grow_idea_fidelity_source(const PiecePlan *plan, LaneSpanId melody, const Motive *basic, Motive *source,
                               PlanMotiveRef *ref, bool *from_segment) {
    if (!plan || !source || !ref || !from_segment || !plan_has_lane_span(plan, melody)) {
        return false;
    }
    const PlanConstraint *asked = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    PlanMeter meter;
    AnalysisKey key;
    if (!asked || !plan_has_node(plan, asked->params.motive.motive) ||
        !read_meter_key(plan, plan_lane_span_span(plan, melody), &meter, &key)) {
        return false;
    }
    /* The node the span asks for says what it is made from; m0 and a motive given for an idea are not made from anything. */
    NodeId node = asked->params.motive.motive;
    const PlanConstraint *made = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_MOTIVE_REF);
    NodeId source_node = node;
    *ref = asked->params.motive;
    if (made && plan_has_node(plan, made->params.motive.motive)) {
        *ref = made->params.motive;
        source_node = ref->motive;
    }
    *from_segment = false;
    if (ref->segment[0] != '\0') {
        Motive m0;
        *from_segment = true;
        return read_m0(plan, basic, &meter, &key, &m0) && read_segment(&m0, ref->segment, source);
    }
    if (ref->steps > 0) {
        /* A liquidation by segments: made from m0 whole (its node holds the residue, not the source). */
        return read_m0(plan, basic, &meter, &key, source);
    }
    return grow_read_motive(plan, source_node, &meter, &key, source);
}

bool grow_idea_realized(const PiecePlan *plan, LaneSpanId melody, Motive *out) {
    if (!plan || !out || !plan_has_lane_span(plan, melody) || plan_lane_span_lane(plan, melody) != PLAN_LANE_MELODY) {
        return false;
    }
    const PlanConstraint *asked = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    LaneSpanId node_span;
    PlanMeter meter;
    AnalysisKey key;
    if (!asked || !plan_has_node(plan, asked->params.motive.motive) ||
        !plan_melody_span_of(plan, asked->params.motive.motive, &node_span) ||
        !read_meter_key(plan, plan_lane_span_span(plan, melody), &meter, &key)) {
        return false;
    }
    /* The idea's motive node is as long as the end of its last note; the pickup is part of that length (R-319). */
    const PlanNotes *carried = &plan_lane_span_payload(plan, node_span)->notes;
    Rational node_length;
    if (!notes_end(carried, carried->note_count, &node_length)) {
        return false;
    }
    const PlanNotes *written = &plan_lane_span_payload(plan, melody)->notes;
    size_t inside = 0;
    while (inside < written->note_count && inside < MOTIVE_MAX_NOTES &&
           rational_cmp(written->notes[inside].offset, node_length) < 0) {
        inside++;
    }
    Rational length;
    if (inside == 0 || !notes_end(written, inside, &length)) {
        return false;
    }
    motive_init(out, meter);
    motive_set_key(out, &key, MOTIVE_SKETCH_KEY_GIVEN);
    motive_set_lane_notes(out, written->notes, inside);
    out->length = length;
    return true;
}

bool grow_idea_fidelity(const PiecePlan *plan, LaneSpanId melody, const Motive *basic, MotiveFidelity *out) {
    Motive source;
    Motive realized;
    PlanMotiveRef ref;
    bool from_segment = false;
    return out && grow_idea_fidelity_source(plan, melody, basic, &source, &ref, &from_segment) &&
           grow_idea_realized(plan, melody, &realized) && motive_fidelity(&source, &realized, &ref, from_segment, out);
}
