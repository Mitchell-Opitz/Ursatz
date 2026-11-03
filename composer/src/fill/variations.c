#include <ursatz_composer/variations.h>

#include <stdlib.h>
#include <string.h>

#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>
#include "util/plan_util.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static uint64_t hash_value(uint64_t hash, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        hash ^= (value >> (8 * i)) & 0xFFu;
        hash *= FNV_PRIME;
    }
    return hash;
}

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b) {
        unsigned long t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1;
}

/* A fraction as its reduced numerator and denominator, into the hash. */
static uint64_t hash_fraction(uint64_t hash, long numerator, unsigned long denominator) {
    unsigned long magnitude = numerator < 0 ? 0UL - (unsigned long)numerator : (unsigned long)numerator;
    unsigned long divisor = gcd(magnitude, denominator);
    hash = hash_value(hash, (uint64_t)(numerator / (long)divisor));
    return hash_value(hash, denominator / divisor);
}

typedef struct {
    PlanSpan span;
    LaneSpanId id;
} HarmonySpan;

/* By start, then length, then numeral: the same chords in the same places sort the same whatever their ids. */
static const PiecePlan *sort_plan;

static int compare_harmony(const void *left, const void *right) {
    const HarmonySpan *a = left;
    const HarmonySpan *b = right;
    int order = plan_time_compare(a->span.start, b->span.start);
    if (order == 0) {
        order = plan_time_compare(a->span.length, b->span.length);
    }
    return order != 0 ? order
                      : strcmp(plan_lane_span_payload(sort_plan, a->id)->harmony.roman_numeral,
                               plan_lane_span_payload(sort_plan, b->id)->harmony.roman_numeral);
}

/* The harmony lane of the plan in time order: where each span is and its numeral. */
static uint64_t hash_harmony(uint64_t hash, const PiecePlan *plan) {
    size_t count = 0;
    HarmonySpan *spans = calloc(plan_lane_span_count(plan) + 1, sizeof(HarmonySpan));
    if (!spans) {
        return 0;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == PLAN_LANE_HARMONY) {
            spans[count++] = (HarmonySpan){plan_lane_span_span(plan, id), id};
        }
    }
    sort_plan = plan;
    qsort(spans, count, sizeof(HarmonySpan), compare_harmony);
    for (size_t i = 0; i < count; i++) {
        hash = hash_fraction(hash, spans[i].span.start.num, spans[i].span.start.den);
        hash = hash_fraction(hash, spans[i].span.length.num, spans[i].span.length.den);
        for (const char *c = plan_lane_span_payload(plan, spans[i].id)->harmony.roman_numeral; *c; c++) {
            hash = hash_value(hash, (unsigned char)*c);
        }
    }
    free(spans);
    return hash;
}

static uint64_t signature_of(const PiecePlan *plan, bool harmony_only) {
    if (plan && harmony_only) {
        return hash_harmony(FNV_OFFSET, plan);
    }
    RenderedPlan rendered;
    if (!plan || plan_render(plan, &rendered) != PLAN_RENDER_OK) {
        return 0;
    }
    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < rendered.count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(rendered.events[i]));
        const Duration *duration = note_event_duration(rendered.events[i]);
        hash = hash_value(hash, rendered.part_index[i]);
        hash = hash_fraction(hash, musical_time_numerator(onset), musical_time_denominator(onset));
        hash = hash_fraction(hash, duration_numerator(duration), duration_denominator(duration));
        hash = hash_value(hash, (uint64_t)pitch_numeric_value_position_numerator(rendered.pitch_values[i]));
    }
    rendered_plan_destroy(&rendered);
    return hash;
}

uint64_t plan_signature(const PiecePlan *plan) {
    return signature_of(plan, false);
}

/* A plan file numbers its nodes and lane spans from 1 with no gaps; a copy through JSON then keeps every id. */
static bool ids_are_stable(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id)) {
            return false;
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id)) {
            return false;
        }
    }
    return true;
}

static PiecePlan *copy_plan(const char *text) {
    PiecePlan *copy = NULL;
    PlanJsonError error;
    return plan_from_json(text, &copy, &error) == PLAN_OK ? copy : NULL;
}

void plan_variations_destroy(PlanVariations *variations) {
    if (!variations) {
        return;
    }
    for (size_t i = 0; i < variations->count; i++) {
        plan_destroy(variations->items[i].plan);
        compose_outcome_destroy(&variations->items[i].outcome);
    }
    free(variations->items);
    memset(variations, 0, sizeof(*variations));
}

static RegenerateResult vary(const PiecePlan *plan, const FillTarget *targets, size_t target_count,
                             const ComposeConfig *config, size_t count, bool with_harmony, PlanVariations *out) {
    if (!plan || !targets || !config || !out) {
        return REGENERATE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (count < 1 || count > PLAN_VARIATIONS_MAX || target_count == 0 || !ids_are_stable(plan)) {
        return REGENERATE_ERR_BAD_TARGET;
    }

    /* The errors that no seed can mend, before any is run. */
    FillTarget *scope = NULL;
    size_t scope_count = 0;
    RegenerateResult scoped = plan_regenerate_scope_all(plan, targets, target_count, &scope, &scope_count);
    free(scope);
    if (scoped != REGENERATE_OK) {
        return scoped;
    }

    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    if (!text) {
        return REGENERATE_ERR_ALLOC;
    }
    out->items = calloc(count, sizeof(PlanVariation));
    if (!out->items) {
        free(text);
        return REGENERATE_ERR_ALLOC;
    }
    uint64_t original = signature_of(plan, with_harmony);
    RegenerateResult result = REGENERATE_OK;
    size_t seeds = 4 * count;
    for (size_t k = 0; k < seeds && out->count < count && result == REGENERATE_OK; k++) {
        out->seeds_tried = k + 1;
        PiecePlan *copy = copy_plan(text);
        if (!copy) {
            result = REGENERATE_ERR_ALLOC;
            break;
        }
        ComposeConfig run = *config;
        run.seed = (config->seed + k) & 0xFFFFFFFFUL;
        ComposeOutcome outcome;
        memset(&outcome, 0, sizeof(outcome));
        RegenerateResult ran = plan_regenerate_all(copy, targets, target_count, &run, &outcome);
        bool keep = false;
        uint64_t signature = 0;
        if (ran == REGENERATE_OK) {
            signature = signature_of(copy, with_harmony);
            keep = signature != original;
            for (size_t i = 0; keep && i < out->count; i++) {
                keep = out->items[i].signature != signature;
            }
        } else if (ran != REGENERATE_ERR_STUCK) {
            result = ran;
        }
        if (keep) {
            out->items[out->count++] = (PlanVariation){copy, run.seed, signature, outcome};
        } else {
            plan_destroy(copy);
            compose_outcome_destroy(&outcome);
        }
    }
    free(text);
    if (result != REGENERATE_OK) {
        plan_variations_destroy(out);
    }
    return result;
}

RegenerateResult plan_variations(const PiecePlan *plan, const FillTarget *targets, size_t target_count,
                                 const ComposeConfig *config, size_t count, PlanVariations *out) {
    return vary(plan, targets, target_count, config, count, false, out);
}

RegenerateResult plan_harmonizations(const PiecePlan *plan, NodeId section, const ComposeConfig *config, size_t count,
                                     PlanVariations *out) {
    if (!plan || !config || !out) {
        return REGENERATE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan_has_node(plan, section) || plan_section_of(plan, section) != section) {
        return REGENERATE_ERR_BAD_TARGET;
    }
    FillTarget *targets = calloc(plan_lane_span_count(plan) + 1, sizeof(FillTarget));
    if (!targets) {
        return REGENERATE_ERR_ALLOC;
    }
    size_t target_count = 0;
    size_t melody_count = 0;
    bool melody_given = true;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id) || !plan_in_subtree(plan, plan_lane_span_owner(plan, id), section)) {
            continue;
        }
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        if (lane == PLAN_LANE_MELODY) {
            melody_count++;
            melody_given = melody_given && plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN;
        } else if (lane == PLAN_LANE_HARMONY && plan_lane_span_state(plan, id) != PLAN_STATE_GIVEN) {
            targets[target_count++] = (FillTarget){FILL_TARGET_LANE_SPAN, id};
        }
    }
    RegenerateResult result = melody_count > 0 && melody_given && target_count > 0
                                  ? vary(plan, targets, target_count, config, count, true, out)
                                  : REGENERATE_ERR_BAD_TARGET;
    free(targets);
    return result;
}
