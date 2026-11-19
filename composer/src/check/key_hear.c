#include <ursatz_composer/key_hear.h>

#include <stdio.h>
#include <string.h>

#include <theory/harmony/harmony_chord.h>
#include <theory/harmony/harmony_key_change.h>
#include <theory/harmony/harmony_related_key.h>
#include <theory/tonality/region_relation.h>
#include <theory/tonality/tonal_key.h>
#include <theory/vocabulary/cadence_type.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/harmony_phrase.h>
#include <plan/plan_state.h>
#include "check/hearing.h"
#include "check/form_internal.h"
#include "structure/plan_util.h"
#include "util/util.h"

/* ------------------------------------------------------------- the check */

#define MAX_ARRIVALS 256

/* The key lane over the whole plan, in order. */
typedef struct {
    PlanSpan span[MAX_SEGMENTS];
    SpelledKey key[MAX_SEGMENTS];
    size_t count;
} KeyLane;

static bool read_key_lane(const Plan *plan, KeyLane *lane) {
    LaneSpanId ids[MAX_SEGMENTS];
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    lane->count = plan_lane_find(plan, PLAN_LANE_REGION, 0, everything, ids, MAX_SEGMENTS);
    if (lane->count == 0 || lane->count > MAX_SEGMENTS) {
        return false;
    }
    for (size_t i = 0; i < lane->count; i++) {
        lane->span[i] = plan_lane_span_span(plan, ids[i]);
        lane->key[i] = plan_lane_span_payload(plan, ids[i])->region;
    }
    return true;
}

/* Whether the key is the home key's or one of its closely related keys. */
static bool related_to_home(SpelledKey home, SpelledKey key) {
    HarmonyMode home_mode = harmony_key_mode(&home);
    HarmonyMode mode = harmony_key_mode(&key);
    if (spelled_key_equals(&home, &key)) {
        return true;
    }
    /* The home key's parallel key, on the same tonic (R-499). */
    if (spelled_key_tonic_pitch_class(&home) == spelled_key_tonic_pitch_class(&key)) {
        return true;
    }
    int semitone = util_pitch_class(spelled_key_tonic_pitch_class(&key) - spelled_key_tonic_pitch_class(&home));
    for (int i = 0; i < harmony_related_key_count(home_mode); i++) {
        HarmonyRelatedKey related;
        if (harmony_related_key_at(home_mode, i, &related) && related.mode == mode &&
            harmony_related_key_semitone(home_mode, &related) == semitone) {
            return harmony_key_is_closely_related(home_mode, &related);
        }
    }
    return false;
}

/* Adds the authentic arrivals heard in `span`. False when the span is not readable. */
static bool hear_arrivals(ComposerHearing *hearing, const Plan *plan, PlanSpan span, Rational *arrivals,
                          size_t *arrival_count) {
    FormHeard heard;
    if (form_hear_in(hearing, plan, span, &heard) != FORM_HEAR_OK) {
        return false;
    }
    for (size_t i = 0; i < heard.cadence_count && *arrival_count < MAX_ARRIVALS; i++) {
        int type = heard.cadences[i].type;
        if (type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
            type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) {
            arrivals[(*arrival_count)++] = heard.cadences[i].arrival;
        }
    }
    form_heard_destroy(&heard);
    return true;
}

/*
 * The arrival of every authentic cadence heard in the sections, in whole notes. Each section is heard whole; the hearing
 * gives the Analyzer the key at the span's start, so the part of a section after a key change inside it is heard again
 * from the change, in its new key. False when a section is not readable.
 */
static bool authentic_arrivals(ComposerHearing *hearing, const Plan *plan, const KeyLane *lane, const NodeId *sections,
                               size_t count, Rational *arrivals, size_t *arrival_count) {
    *arrival_count = 0;
    for (size_t s = 0; s < count; s++) {
        PlanSpan whole = plan_node_span(plan, sections[s]);
        Rational end;
        if (!hear_arrivals(hearing, plan, whole, arrivals, arrival_count) || plan_span_end(whole, &end) != PLAN_OK) {
            return false;
        }
        for (size_t k = 0; k < lane->count; k++) {
            Rational at = lane->span[k].start;
            Rational negative = {-at.num, at.den};
            PlanSpan part = {at, {0, 1}};
            if (rational_cmp(at, whole.start) > 0 && rational_cmp(at, end) < 0 &&
                rational_add(end, negative, &part.length) == RATIONAL_OK &&
                !hear_arrivals(hearing, plan, part, arrivals, arrival_count)) {
                return false;
            }
        }
    }
    return true;
}

static bool constraint_key(const Plan *plan, NodeId section, SpelledKey *out) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, section, PLAN_CONSTRAINT_REGION);
    if (constraint) {
        *out = constraint->params.region;
    }
    return constraint != NULL;
}

static size_t chords_between(const Plan *plan, Rational start, Rational end) {
    PlanSpan range = {start, {0, 1}};
    Rational negative = {-start.num, start.den};
    if (rational_cmp(end, start) <= 0 || rational_add(end, negative, &range.length) != RATIONAL_OK) {
        return 0;
    }
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
}

bool piece_check_keys_in(ComposerHearing *hearing, const Plan *plan, PieceKeyCheck *out) {
    if (!hearing || !plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    NodeId form = plan_root(plan);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t section_count = 0;
    Rational piece_end = {0, 1};
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            piece_end = plan_node_span(plan, child).start;
        } else if (plan_section_has_role(plan, child, SECTION_ROLE_SEAM)) {
            continue;
        } else if (section_count < FORM_SPEC_MAX_SECTIONS) {
            sections[section_count++] = child;
        }
    }
    KeyLane lane;
    Rational arrivals[MAX_ARRIVALS + 1];
    size_t arrival_count;
    if (section_count == 0 || piece_end.num == 0 || !read_key_lane(plan, &lane) ||
        !authentic_arrivals(hearing, plan, &lane, sections, section_count, arrivals, &arrival_count)) {
        return false;
    }
    SpelledKey home = lane.key[0];
    for (size_t k = 0; k < lane.count; k++) {
        if (!related_to_home(home, lane.key[k]) && out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "key span %zu is not closely related to the home key", k);
        }
    }
    for (size_t k = 1; k < lane.count; k++) {
        /* A change of mode on one tonic is neither counted nor classified (R-499). */
        if (spelled_key_equals(&lane.key[k], &lane.key[k - 1]) ||
            spelled_key_tonic_pitch_class(&lane.key[k]) == spelled_key_tonic_pitch_class(&lane.key[k - 1])) {
            continue;
        }
        out->changes++;
        Rational span_end;
        Rational region_end = piece_end;
        Rational cadence_end = {0, 1};
        bool planned = false;
        if (plan_span_end(lane.span[k], &span_end) != PLAN_OK) {
            return false;
        }
        region_end = rational_cmp(span_end, piece_end) < 0 ? span_end : piece_end;
        /* A section planned to end in the new key, inside the span: the last of them is where the cadence is. */
        for (size_t s = 0; s < section_count; s++) {
            SpelledKey ends_in;
            Rational end;
            if (!constraint_key(plan, sections[s], &ends_in) ||
                plan_span_end(plan_node_span(plan, sections[s]), &end) != PLAN_OK) {
                return false;
            }
            if (spelled_key_equals(&ends_in, &lane.key[k]) && rational_cmp(end, lane.span[k].start) > 0 &&
                rational_cmp(end, region_end) <= 0 && rational_cmp(end, cadence_end) > 0) {
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
            !harmony_key_change_confirm(length_extent, lane.span[k].start, region_end, arrivals,
                                        arrival_count, &extent)) {
            return false;
        }
        if (key_change_extent_is_modulation(extent)) {
            out->modulations++;
        } else if (out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "the key change at %lld/%lld is a %s, not a modulation",
                     lane.span[k].start.num, lane.span[k].start.den, key_change_extent_name(extent));
        }
    }
    return true;
}
