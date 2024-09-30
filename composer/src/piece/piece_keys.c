#include <ursatz_composer/piece_plan_keys.h>

#include <string.h>

#include <stdio.h>

#include <harmony_chord.h>
#include <harmony_key_change.h>
#include <harmony_related_key.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/plan_state.h>

/* A key lane span before it is added: its key and where it lies. */
typedef struct {
    PlanSpan span;
    PlanKey key;
} KeySegment;

#define MAX_SEGMENTS (2 * FORM_TEMPLATE_MAX_SECTIONS + 1)

static PieceResult from_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

static HarmonyMode mode_of(PlanKeyMode mode) {
    return mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

static bool same_key(PlanKey a, PlanKey b) {
    return a.tonic_pitch_class == b.tonic_pitch_class && a.mode == b.mode;
}

static const PlanConstraint *find_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            return constraint;
        }
    }
    return NULL;
}

/*
 * The closely related key of the home key on one of the degrees, drawn from the list. False when the draw is not a
 * key standard-harmony calls closely related.
 */
static bool pick_away(PlanKey home, const int *degrees, size_t count, FillRng *rng, PlanKey *out) {
    HarmonyMode home_mode = mode_of(home.mode);
    HarmonyRelatedKey related;
    int semitone;
    if (!harmony_related_key_of(home_mode, degrees[fill_rng_below(rng, count)], &related) ||
        !harmony_key_is_closely_related(home_mode, &related)) {
        return false;
    }
    semitone = harmony_related_key_semitone(home_mode, &related);
    if (semitone < 0) {
        return false;
    }
    out->tonic_pitch_class = (home.tonic_pitch_class + semitone) % 12;
    out->mode = related.mode == HARMONY_MODE_MINOR ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR;
    return true;
}

/* The key a binary or rounded binary A ends in: V in major, III in minor. */
static bool pick_dominant_side(PlanKey home, FillRng *rng, PlanKey *out) {
    static const int MAJOR[] = {5};
    static const int MINOR[] = {3};
    return home.mode == PLAN_MODE_MINOR ? pick_away(home, MINOR, 1, rng, out) : pick_away(home, MAJOR, 1, rng, out);
}

/* The key a ternary B ends in: IV, vi or V in major; III, iv or VI in minor. */
static bool pick_contrast(PlanKey home, FillRng *rng, PlanKey *out) {
    static const int MAJOR[] = {4, 6, 5};
    static const int MINOR[] = {3, 4, 6};
    return home.mode == PLAN_MODE_MINOR ? pick_away(home, MINOR, 3, rng, out) : pick_away(home, MAJOR, 3, rng, out);
}

/* The key of a variation in another key: the relative key, vi in major and III in minor. */
static bool pick_relative(PlanKey home, FillRng *rng, PlanKey *out) {
    static const int MAJOR[] = {6};
    static const int MINOR[] = {3};
    return home.mode == PLAN_MODE_MINOR ? pick_away(home, MINOR, 1, rng, out) : pick_away(home, MAJOR, 1, rng, out);
}

/* Draws the keys each section starts and ends in; every section is home when keys is left as set. */
static bool draw_keys(FormTemplateKind kind, size_t count, PlanKey home, FillRng *rng, PieceSectionKeys *keys,
                      bool *modulates) {
    PlanKey away = home;
    *modulates = false;
    for (size_t i = 0; i < count; i++) {
        keys[i].start = home;
        keys[i].end = home;
    }
    /* Two of three pieces modulate where the template can. */
    bool can = (kind == FORM_TEMPLATE_BINARY && count == 2) || (kind == FORM_TEMPLATE_ROUNDED_BINARY && count == 3) ||
               (kind == FORM_TEMPLATE_TERNARY && count == 3) || (kind == FORM_TEMPLATE_VARIATIONS && count >= 4);
    if (!rng || !can || fill_rng_below(rng, 3) == 0) {
        return true;
    }
    switch (kind) {
    case FORM_TEMPLATE_BINARY:
    case FORM_TEMPLATE_ROUNDED_BINARY:
        if (!pick_dominant_side(home, rng, &away)) {
            return false;
        }
        keys[0].end = away;
        keys[1].start = away;
        break;
    case FORM_TEMPLATE_TERNARY:
        if (!pick_contrast(home, rng, &away)) {
            return false;
        }
        keys[1].end = away;
        break;
    case FORM_TEMPLATE_VARIATIONS: {
        /* The middle variation: with n variations, number (n + 1) / 2, section index the same (the theme is 0). */
        size_t middle = count / 2;
        if (!pick_relative(home, rng, &away)) {
            return false;
        }
        keys[middle].start = away;
        keys[middle].end = away;
        break;
    }
    default:
        return true;
    }
    *modulates = true;
    return true;
}

/* Adds a segment, joined to the one before it when that is in the same key and ends where this one starts. */
static PieceResult push_segment(KeySegment *segments, size_t *count, PlanSpan span, PlanKey key) {
    if (*count > 0 && same_key(segments[*count - 1].key, key)) {
        PlanTime end;
        PieceResult result = from_plan(plan_span_end(segments[*count - 1].span, &end));
        if (result == PIECE_OK && plan_time_compare(end, span.start) == 0) {
            return from_plan(plan_time_add(segments[*count - 1].span.length, span.length,
                                           &segments[*count - 1].span.length));
        }
        return PIECE_ERR_BAD_REQUEST;
    }
    if (*count >= MAX_SEGMENTS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    segments[*count].span = span;
    segments[*count].key = key;
    (*count)++;
    return PIECE_OK;
}

/* A section whose key changes halfway has its first half in the key it starts in and its second in the key it ends in. */
static PieceResult section_segments(KeySegment *segments, size_t *count, PlanSpan span, const PieceSectionKeys *keys) {
    if (same_key(keys->start, keys->end)) {
        return push_segment(segments, count, span, keys->start);
    }
    PlanSpan first;
    PlanSpan second;
    first.start = span.start;
    PieceResult result = from_plan(plan_time_make(span.length.num, span.length.den * 2, &first.length));
    if (result != PIECE_OK) {
        return result;
    }
    second.length = first.length;
    result = from_plan(plan_time_add(span.start, first.length, &second.start));
    if (result == PIECE_OK) {
        result = push_segment(segments, count, first, keys->start);
    }
    return result == PIECE_OK ? push_segment(segments, count, second, keys->end) : result;
}

/* The KEY constraint of the section, replaced by the key it ends in. */
static PieceResult set_section_key(PiecePlan *plan, NodeId section, PlanKey key) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        if (plan_node_constraint_at(plan, section, i)->kind == PLAN_CONSTRAINT_KEY) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_KEY;
            constraint.params.key = key;
            PieceResult result = from_plan(plan_node_remove_constraint(plan, section, i));
            return result == PIECE_OK ? from_plan(plan_node_add_constraint(plan, section, &constraint)) : result;
        }
    }
    return PIECE_ERR_BAD_REQUEST;
}

PieceResult piece_plan_keys(PiecePlan *plan, FillRng *rng, PieceKeyPlan *out) {
    if (!plan || !out) {
        return PIECE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    NodeId form = plan_root(plan);
    FormTemplateKind kind;
    const char *name = form == PLAN_NODE_NONE ? NULL : plan_node_name(plan, form);
    if (!name || !form_template_from_name(name, &kind)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    NodeId material = PLAN_NODE_NONE;
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            material = child;
        } else if (count < FORM_TEMPLATE_MAX_SECTIONS) {
            sections[count++] = child;
        } else {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    const PlanConstraint *home_key = count > 0 ? find_constraint(plan, sections[0], PLAN_CONSTRAINT_KEY) : NULL;
    if (!home_key || material == PLAN_NODE_NONE) {
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanKey home = home_key->params.key;
    out->count = count;
    if (!draw_keys(kind, count, home, rng, out->sections, &out->modulates)) {
        return PIECE_ERR_BAD_REQUEST;
    }

    KeySegment segments[MAX_SEGMENTS];
    size_t segment_count = 0;
    PieceResult result = PIECE_OK;
    for (size_t i = 0; result == PIECE_OK && i < count; i++) {
        result = section_segments(segments, &segment_count, plan_node_span(plan, sections[i]), &out->sections[i]);
    }
    if (result == PIECE_OK) {
        result = push_segment(segments, &segment_count, plan_node_span(plan, material), home);
    }
    for (size_t i = 0; result == PIECE_OK && i < segment_count; i++) {
        PlanLanePayload payload;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        payload.key = segments[i].key;
        result = from_plan(plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, segments[i].span, &payload, &id));
        result = result == PIECE_OK ? from_plan(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN)) : result;
    }
    for (size_t i = 0; result == PIECE_OK && i < count; i++) {
        if (!same_key(out->sections[i].end, home)) {
            result = set_section_key(plan, sections[i], out->sections[i].end);
        }
    }
    return result;
}

/* ------------------------------------------------------------- the check */

#define MAX_ARRIVALS 256

/* The key lane over the whole plan, in order. */
typedef struct {
    PlanSpan span[MAX_SEGMENTS];
    PlanKey key[MAX_SEGMENTS];
    size_t count;
} KeyLane;

static bool read_key_lane(const PiecePlan *plan, KeyLane *lane) {
    LaneSpanId ids[MAX_SEGMENTS];
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    lane->count = plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, ids, MAX_SEGMENTS);
    if (lane->count == 0 || lane->count > MAX_SEGMENTS) {
        return false;
    }
    for (size_t i = 0; i < lane->count; i++) {
        lane->span[i] = plan_lane_span_span(plan, ids[i]);
        lane->key[i] = plan_lane_span_payload(plan, ids[i])->key;
    }
    return true;
}

/* Whether the key is the home key's or one of its closely related keys. */
static bool related_to_home(PlanKey home, PlanKey key) {
    HarmonyMode home_mode = mode_of(home.mode);
    HarmonyMode mode = mode_of(key.mode);
    if (same_key(home, key)) {
        return true;
    }
    int semitone = ((key.tonic_pitch_class - home.tonic_pitch_class) % 12 + 12) % 12;
    for (int i = 0; i < harmony_related_key_count(home_mode); i++) {
        HarmonyRelatedKey related;
        if (harmony_related_key_at(home_mode, i, &related) && related.mode == mode &&
            harmony_related_key_semitone(home_mode, &related) == semitone) {
            return harmony_key_is_closely_related(home_mode, &related);
        }
    }
    return false;
}

static double as_double(PlanTime time) {
    return (double)time.num / (double)time.den;
}

/* The arrival of every authentic cadence heard in the sections, in whole notes. False when a section is not readable. */
static bool authentic_arrivals(const PiecePlan *plan, const NodeId *sections, size_t count, double *arrivals,
                               size_t *arrival_count) {
    *arrival_count = 0;
    for (size_t s = 0; s < count; s++) {
        FormHeard heard;
        if (form_hear(plan, plan_node_span(plan, sections[s]), &heard) != FORM_HEAR_OK) {
            return false;
        }
        for (size_t i = 0; i < heard.cadence_count && *arrival_count < MAX_ARRIVALS; i++) {
            int type = heard.cadences[i].type;
            if (type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
                type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) {
                arrivals[(*arrival_count)++] = as_double(heard.cadences[i].arrival);
            }
        }
        form_heard_destroy(&heard);
    }
    return true;
}

static bool constraint_key(const PiecePlan *plan, NodeId section, PlanKey *out) {
    const PlanConstraint *constraint = find_constraint(plan, section, PLAN_CONSTRAINT_KEY);
    if (constraint) {
        *out = constraint->params.key;
    }
    return constraint != NULL;
}

static size_t chords_between(const PiecePlan *plan, PlanTime start, PlanTime end) {
    PlanSpan range = {start, {0, 1}};
    PlanTime negative = {-start.num, start.den};
    if (plan_time_compare(end, start) <= 0 || plan_time_add(end, negative, &range.length) != PLAN_OK) {
        return 0;
    }
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
}

bool piece_check_keys(const PiecePlan *plan, PieceKeyCheck *out) {
    if (!plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    NodeId form = plan_root(plan);
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t section_count = 0;
    PlanTime piece_end = {0, 1};
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            piece_end = plan_node_span(plan, child).start;
        } else if (section_count < FORM_TEMPLATE_MAX_SECTIONS) {
            sections[section_count++] = child;
        }
    }
    KeyLane lane;
    double arrivals[MAX_ARRIVALS + 1];
    size_t arrival_count;
    if (section_count == 0 || piece_end.num == 0 || !read_key_lane(plan, &lane) ||
        !authentic_arrivals(plan, sections, section_count, arrivals, &arrival_count)) {
        return false;
    }
    PlanKey home = lane.key[0];
    for (size_t k = 0; k < lane.count; k++) {
        if (!related_to_home(home, lane.key[k]) && out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "key span %zu is not closely related to the home key", k);
        }
    }
    for (size_t k = 1; k < lane.count; k++) {
        if (same_key(lane.key[k], lane.key[k - 1])) {
            continue;
        }
        out->changes++;
        PlanTime span_end;
        PlanTime region_end = piece_end;
        PlanTime cadence_end = {0, 1};
        bool planned = false;
        if (plan_span_end(lane.span[k], &span_end) != PLAN_OK) {
            return false;
        }
        region_end = plan_time_compare(span_end, piece_end) < 0 ? span_end : piece_end;
        /* A section planned to end in the new key, inside the span: the last of them is where the cadence is. */
        for (size_t s = 0; s < section_count; s++) {
            PlanKey ends_in;
            PlanTime end;
            if (!constraint_key(plan, sections[s], &ends_in) ||
                plan_span_end(plan_node_span(plan, sections[s]), &end) != PLAN_OK) {
                return false;
            }
            if (same_key(ends_in, lane.key[k]) && plan_time_compare(end, lane.span[k].start) > 0 &&
                plan_time_compare(end, region_end) <= 0 && plan_time_compare(end, cadence_end) > 0) {
                cadence_end = end;
                planned = true;
            }
        }
        if (!planned) {
            continue;
        }
        HarmonyKeyChange change;
        change.chords_in_new_key = (int)chords_between(plan, lane.span[k].start, region_end);
        change.cadence_in_new_key = true;
        change.continues_after_cadence = chords_between(plan, cadence_end, region_end) > 0;
        KeyChangeExtent length_extent;
        KeyChangeExtent extent;
        if (!harmony_key_change_classify(&change, &length_extent) ||
            !harmony_key_change_confirm(length_extent, as_double(lane.span[k].start), as_double(region_end), arrivals,
                                        arrival_count, &extent)) {
            return false;
        }
        if (key_change_extent_is_modulation(extent)) {
            out->modulations++;
        } else if (out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "the key change at %ld/%lu is a %s, not a modulation",
                     lane.span[k].start.num, lane.span[k].start.den, key_change_extent_name(extent));
        }
    }
    return true;
}
