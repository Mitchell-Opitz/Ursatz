#include <stdio.h>
#include <string.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive_analysis.h>

#include "grow/grow_internal.h"
#include "style/style_internal.h"
#include "util/plan_util.h"

/*
 * An idea's fidelity to its source, read from the plan (R-354). The idea's melody span names its motive node (the span's
 * MOTIVE_REF); that node's own MOTIVE_REF says what it is made from. The realized notes are the span's notes inside the
 * node's length, in the key and meter of the lanes over the span. Pure reads: nothing is written to the plan.
 */

/*
 * The least fidelity an idea may have, by development setting and role (R-355). R-355 starts balanced at presentation 3/4,
 * continuation 1/2, cadential 3/10, melody 1/10 higher and theme 1/10 lower; the compose rates (R-317) lowered them in steps
 * of 1/20, as far as the melody rules' own bends need (a clash with the chords moves a note, R-328, and the segment of a
 * fragment is two or three notes): presentation 11/20 and continuation 7/20 in every setting, cadential melody 7/20. Contrast
 * and none are 0: a contrasting idea is meant to stray; it is scored and shown, never refused.
 */
static const MotiveScore FIDELITY_MINIMUM[GROW_DEVELOP_COUNT][GROW_ROLE_CONTRAST + 1] = {
    [GROW_DEVELOP_BALANCED] = {[GROW_ROLE_NONE] = {0, 1},
                               [GROW_ROLE_PRESENTATION] = {11, 20},
                               [GROW_ROLE_CONTINUATION] = {7, 20},
                               [GROW_ROLE_CADENTIAL] = {3, 10},
                               [GROW_ROLE_CONTRAST] = {0, 1}},
    [GROW_DEVELOP_MELODY] = {[GROW_ROLE_NONE] = {0, 1},
                             [GROW_ROLE_PRESENTATION] = {11, 20},
                             [GROW_ROLE_CONTINUATION] = {7, 20},
                             [GROW_ROLE_CADENTIAL] = {7, 20},
                             [GROW_ROLE_CONTRAST] = {0, 1}},
    [GROW_DEVELOP_THEME] = {[GROW_ROLE_NONE] = {0, 1},
                            [GROW_ROLE_PRESENTATION] = {11, 20},
                            [GROW_ROLE_CONTINUATION] = {7, 20},
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

/* The notes as a motive in the meter and key of the lanes over the span; its length is the end of its last note. */
static bool realized_motive(const PiecePlan *plan, LaneSpanId melody, const PlanNote *notes, size_t count, Motive *out) {
    PlanMeter meter;
    AnalysisKey key;
    Rational length;
    if (count == 0 || count > MOTIVE_MAX_NOTES || !read_meter_key(plan, plan_lane_span_span(plan, melody), &meter, &key)) {
        return false;
    }
    PlanNotes written = {notes, count};
    if (!notes_end(&written, count, &length)) {
        return false;
    }
    motive_init(out, meter);
    motive_set_key(out, &key, MOTIVE_SKETCH_KEY_GIVEN);
    motive_set_lane_notes(out, notes, count);
    out->length = length;
    return true;
}

/* The length of the idea's motive node: the end of its last note, the pickup part of it (R-319). */
static bool node_length_of(const PiecePlan *plan, LaneSpanId melody, Rational *length) {
    const PlanConstraint *asked = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    LaneSpanId node_span;
    if (!asked || !plan_has_node(plan, asked->params.motive.motive) ||
        !plan_melody_span_of(plan, asked->params.motive.motive, &node_span)) {
        return false;
    }
    const PlanNotes *carried = &plan_lane_span_payload(plan, node_span)->notes;
    return notes_end(carried, carried->note_count, length);
}

/* How many of the notes, in order, start before the idea's motive node ends. */
static size_t notes_inside(const PlanNote *notes, size_t count, Rational node_length) {
    size_t inside = 0;
    while (inside < count && inside < MOTIVE_MAX_NOTES && rational_cmp(notes[inside].offset, node_length) < 0) {
        inside++;
    }
    return inside;
}

bool grow_idea_realized(const PiecePlan *plan, LaneSpanId melody, Motive *out) {
    Rational node_length;
    if (!plan || !out || !plan_has_lane_span(plan, melody) || plan_lane_span_lane(plan, melody) != PLAN_LANE_MELODY ||
        !node_length_of(plan, melody, &node_length)) {
        return false;
    }
    const PlanNotes *written = &plan_lane_span_payload(plan, melody)->notes;
    return realized_motive(plan, melody, written->notes, notes_inside(written->notes, written->note_count, node_length), out);
}

bool grow_idea_fidelity(const PiecePlan *plan, LaneSpanId melody, const Motive *basic, MotiveFidelity *out) {
    Motive source;
    Motive realized;
    PlanMotiveRef ref;
    bool from_segment = false;
    return out && grow_idea_fidelity_source(plan, melody, basic, &source, &ref, &from_segment) &&
           grow_idea_realized(plan, melody, &realized) && motive_fidelity(&source, &realized, &ref, from_segment, out);
}

/* --- the checker ------------------------------------------------------------ */

static const char *role_name(GrowIdeaRole role) {
    switch (role) {
    case GROW_ROLE_PRESENTATION:
        return "presentation";
    case GROW_ROLE_CONTINUATION:
        return "continuation";
    case GROW_ROLE_CADENTIAL:
        return "cadential";
    case GROW_ROLE_CONTRAST:
        return "contrast";
    case GROW_ROLE_NONE:
        break;
    }
    return "none";
}

static unsigned long long hash_value(unsigned long long hash, long long value) {
    return (hash ^ (unsigned long long)value) * 1099511628211ull;
}

/*
 * The key of a measurement: the span and the notes' shape with the octave taken out (every pitch less the octave of the
 * first), since the fidelity of the same notes an octave higher or lower is the same (R-354 drops whole octaves).
 */
static unsigned long long measurement_key(LaneSpanId span, const PlanNote *notes, size_t count) {
    unsigned long long hash = hash_value(14695981039346656037ull, (long long)span);
    int octave = count > 0 ? (notes[0].midi >= 0 ? notes[0].midi / 12 : -((11 - notes[0].midi) / 12)) * 12 : 0;
    for (size_t i = 0; i < count; i++) {
        hash = hash_value(hash, notes[i].midi - octave);
        hash = hash_value(hash, notes[i].offset.num);
        hash = hash_value(hash, notes[i].offset.den);
        hash = hash_value(hash, notes[i].duration.num);
        hash = hash_value(hash, notes[i].duration.den);
    }
    return hash;
}

static bool cache_find(const GrowFidelityCache *cache, unsigned long long key, MotiveFidelity *out, bool *departs) {
    size_t used = cache ? (cache->count < GROW_FIDELITY_CACHE ? cache->count : GROW_FIDELITY_CACHE) : 0;
    for (size_t i = 0; i < used; i++) {
        if (cache->entries[i].key == key) {
            *out = cache->entries[i].fidelity;
            *departs = cache->entries[i].departs;
            return true;
        }
    }
    return false;
}

static void cache_add(GrowFidelityCache *cache, unsigned long long key, const MotiveFidelity *fidelity, bool departs) {
    if (cache) {
        size_t slot = cache->count % GROW_FIDELITY_CACHE;
        cache->entries[slot].key = key;
        cache->entries[slot].fidelity = *fidelity;
        cache->entries[slot].departs = departs;
        cache->count++;
    }
}

/*
 * The recognizer's search depth for ranking: one step. Most intended relations are heard in one step; a contrasting idea
 * two operations from its source needs two, but a search of two steps costs about half a second for a candidate it does not
 * find (R-307). The agreement a search cut short misses counts as none; the score is the same at any depth, and the heard
 * text shown for an idea is searched to two steps (motive_fidelity).
 */
#define RANKING_DEPTH 1u

/*
 * Whether the ref itself makes the idea differ from its source: a liquidation, a fragment by notes, an inversion, any relation
 * but exact, a transposition, a dominant form and a segment's own fragment. Such an idea's score against the source is low
 * by what it was asked to be, so it is ranked but never refused.
 */
bool grow_ref_departs(const PlanMotiveRef *ref, bool from_segment) {
    if (ref->steps > 0) {
        return true;
    }
    if (!ref->has_step || plan_motive_ref_restates(ref)) {
        return false;
    }
    switch (ref->step.kind) {
    case MOTIVE_RELATION_EXACT:
    case MOTIVE_RELATION_TRANSPOSE:
    case MOTIVE_RELATION_DOMINANT_FORM:
        return false;
    case MOTIVE_RELATION_FRAGMENT:
        return !from_segment;
    default:
        return true;
    }
}

/* The idea's fidelity for these realized notes: from the cache, else measured. *departs says its ref makes the idea differ from its source. */
static bool measure(const PiecePlan *plan, LaneSpanId span, const PlanNote *notes, size_t count, GrowIdeaRole role,
                    const GrowFidelityContext *context, MotiveFidelity *out, bool *departs) {
    unsigned long long key = measurement_key(span, notes, count);
    if (cache_find(context->cache, key, out, departs)) {
        return true;
    }
    Motive source;
    Motive realized;
    PlanMotiveRef ref;
    bool from_segment = false;
    if (!grow_idea_fidelity_source(plan, span, context->basic, &source, &ref, &from_segment) ||
        !realized_motive(plan, span, notes, count, &realized)) {
        return false;
    }
    bool measured;
    if (role == GROW_ROLE_CADENTIAL) {
        /* Only the score: a cadential idea's residue is liquidated in more steps than the search reaches (R-355). */
        MotiveRecognizability recognizability;
        measured = motive_recognizability(&source, &realized, NULL, &recognizability);
        if (measured) {
            memset(out, 0, sizeof(*out));
            out->score = recognizability.total;
            snprintf(out->heard, sizeof(out->heard), "not recognized");
        }
    } else {
        measured = motive_fidelity_depth(&source, &realized, &ref, from_segment, RANKING_DEPTH, out);
    }
    *departs = grow_ref_departs(&ref, from_segment);
    if (measured) {
        cache_add(context->cache, key, out, *departs);
    }
    return measured;
}

/* A fraction in lowest terms and below 2^32 on both sides, as the fill loop compares scores in 64 bits (style_check.c does the same). */
static void set_reduced(unsigned long *numerator_out, unsigned long *denominator_out, Wide numerator, Wide denominator) {
    Wide divisor = wide_gcd(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    while (numerator >= ((Wide)1 << 32) || denominator >= ((Wide)1 << 32)) {
        numerator >>= 1;
        denominator >>= 1;
    }
    *numerator_out = (unsigned long)numerator;
    *denominator_out = denominator ? (unsigned long)denominator : 1ul;
}

bool grow_fidelity_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out) {
    const GrowFidelityContext *fidelity_context = context;
    if (!fidelity_context || !fidelity_context->inner || !plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (!fidelity_context->inner(plan, target, operator_, candidate, fidelity_context->inner_context, out)) {
        return false;
    }
    if (out->hard_violation || strcmp(operator_->generator.name, MELODY_FILL_OPERATOR_NAME) != 0 ||
        target.kind != FILL_TARGET_LANE_SPAN) {
        return true;
    }
    PlanNote notes[MOTIVE_MAX_NOTES];
    size_t count = 0;
    LaneSpanId span;
    /* Of the notes that carry a motive, those inside the idea's motive node are the idea's realized notes, as in the plan (an
       idea over several bars can carry more motive notes than its node holds). */
    Rational node_length;
    if (!melody_candidate_realized(candidate, &span, notes, &count, MOTIVE_MAX_NOTES) || span != (LaneSpanId)target.id ||
        !plan_has_lane_span(plan, span) || !plan_lane_span_find_constraint(plan, span, PLAN_CONSTRAINT_MOTIVE_REF) ||
        !node_length_of(plan, span, &node_length)) {
        return true;
    }
    count = notes_inside(notes, count, node_length);
    if (count == 0) {
        return true;
    }
    NodeId owner = plan_lane_span_owner(plan, span);
    GrowIdeaRole role = plan_node_kind(plan, owner) == PLAN_NODE_IDEA ? grow_idea_role(plan_node_name(plan, owner))
                                                                       : GROW_ROLE_NONE;
    MotiveFidelity measured;
    bool departs = false;
    if (role == GROW_ROLE_NONE || !measure(plan, span, notes, count, role, fidelity_context, &measured, &departs)) {
        return true;
    }
    MotiveScore minimum = fidelity_context->minimum ? *fidelity_context->minimum
                                                    : grow_fidelity_minimum(fidelity_context->development, role);
    if (!departs && measured.score.numerator * minimum.denominator < minimum.numerator * measured.score.denominator) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "motive fidelity %lu/%lu under %lu/%lu (%s)", measured.score.numerator,
                 measured.score.denominator, minimum.numerator, minimum.denominator, role_name(role));
        return true;
    }
    unsigned long quantized = measured.score.numerator * 100 / measured.score.denominator;
    Wide held = out->score_numerator;
    Wide total = out->score_denominator;
    Wide scale = 201 * (total + 1);
    set_reduced(&out->score_numerator, &out->score_denominator, ((measured.agrees ? 100u : 0u) + (Wide)quantized) * (total + 1) + held,
                scale);
    /* The style's part stays a part of the score: it is scaled as the held share is, held/total over 201 (total + 1). */
    if (out->style_denominator > 0) {
        set_reduced(&out->style_numerator, &out->style_denominator, (Wide)out->style_numerator * total,
                    (Wide)out->style_denominator * scale);
    }
    return true;
}
