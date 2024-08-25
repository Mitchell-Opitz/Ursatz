#include <ursatz_composer/piece_plan_keys.h>

#include <string.h>

#include <harmony_chord.h>
#include <harmony_related_key.h>

#include <ursatz_composer/constraint.h>
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
