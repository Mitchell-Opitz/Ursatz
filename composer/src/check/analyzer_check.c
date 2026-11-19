#include <ursatz_composer/analyzer_check.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/identity/entity_id.h>
#include <model/time/meter_arithmetic.h>
#include <model/time/meter_map.h>
#include <theory/harmony/harmony_cadence_rules.h>
#include <theory/tonality/common_practice_materials.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/harmony_phrase.h>
#include <plan/plan_state.h>

#include "check/hearing_result.h"
#include "util/plan_util.h"
#include "util/util.h"

#define MAX_CADENCES 2

void analyzer_check_destroy(AnalyzerCheck *check) {
    if (!check) {
        return;
    }
    free(check->items);
    memset(check, 0, sizeof(*check));
}

/* ------------------------------------------------------------- the plan */

/* Whether a lane span the section holds and a run needs is still open. */
static bool section_open(const PiecePlan *plan, PlanSpan section) {
    Rational end;
    if (plan_span_end(section, &end) != PLAN_OK) {
        return true;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id)) {
            continue;
        }
        PlanLaneKind lane = plan_lane_span_lane(plan, id);
        PlanSpan span = plan_lane_span_span(plan, id);
        if ((lane == PLAN_LANE_MELODY || lane == PLAN_LANE_BASS || lane == PLAN_LANE_HARMONY) &&
            rational_cmp(span.start, section.start) >= 0 && rational_cmp(span.start, end) < 0 &&
            plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED) {
            return true;
        }
    }
    return false;
}

/* The meter and the key of the lane span that is in force at the section's start. */
static bool lane_at_start(const PiecePlan *plan, PlanLaneKind lane, PlanSpan section, LaneSpanId *out) {
    size_t count = plan_lane_find(plan, lane, 0, section, NULL, 0);
    LaneSpanId *ids = count ? calloc(count, sizeof(LaneSpanId)) : NULL;
    if (!ids) {
        return false;
    }
    plan_lane_find(plan, lane, 0, section, ids, count);
    bool found = rational_cmp(plan_lane_span_span(plan, ids[0]).start, section.start) <= 0;
    *out = ids[0];
    free(ids);
    return found;
}

/* ----------------------------------------------------------- the items */

typedef struct {
    AnalyzerCheckItem *items;
    size_t count;
    size_t capacity;
    bool failed;
} Items;

static AnalyzerCheckItem *add_item(Items *items, AnalyzerCheckKind kind, size_t bar, const char *planned,
                                   const char *heard, bool agreed) {
    if (items->count == items->capacity) {
        size_t grown = items->capacity ? items->capacity * 2 : 8;
        AnalyzerCheckItem *more = realloc(items->items, grown * sizeof(AnalyzerCheckItem));
        if (!more) {
            items->failed = true;
            return NULL;
        }
        items->items = more;
        items->capacity = grown;
    }
    AnalyzerCheckItem *item = &items->items[items->count++];
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->bar = bar;
    snprintf(item->planned, sizeof(item->planned), "%s", planned);
    snprintf(item->heard, sizeof(item->heard), "%s", heard);
    item->agreed = agreed;
    return item;
}

static bool authentic_name(const char *name) {
    return strcmp(name, "perfect-authentic") == 0 || strcmp(name, "imperfect-authentic") == 0 ||
           strcmp(name, "authentic-unspecified") == 0;
}

/* Whether the cadence the Analyzer heard is the one planned: any authentic one for a planned PAC or IAC. */
static bool cadence_agrees(CadenceType planned, const char *heard) {
    const char *name = cadence_type_name(planned);
    if (planned == CADENCE_TYPE_PERFECT_AUTHENTIC || planned == CADENCE_TYPE_IMPERFECT_AUTHENTIC) {
        return authentic_name(heard);
    }
    return strcmp(name, heard) == 0;
}

/* The number of bars a length in whole notes holds. */
static size_t bars_in(long numerator, unsigned long denominator, const PlanMeter *meter) {
    if (numerator < 0) {
        return 0;
    }
    return (size_t)(((unsigned long)numerator * (unsigned long)meter->beat_unit) /
                    (denominator * (unsigned long)meter->beats_per_bar));
}

/* The bar (0-based from the fragment's start) a time in whole notes falls in, read through a meter map of the plan's
   meter (0 before the start). SIZE_MAX when the meter or the time is not one a map holds. */
static size_t bar_of(Rational at, const PlanMeter *meter) {
    if (rational_is_negative(at)) {
        return 0;
    }
    size_t bar = (size_t)-1;
    TimeSignature *signature = NULL;
    EntityID *id = NULL;
    MeterMap *map = NULL;
    if (plan_meter_signature(*meter, &signature) && entity_id_create("analyzer-check-meter", &id) == ENTITY_ID_OK) {
        MeterMapChangePoint start = {signature, musical_time_of(rational_ratio(0, 1))};
        NotatedPosition position;
        if (meter_map_create(id, &start, 1, &map) != METER_MAP_OK) {
            entity_id_destroy(id);
        } else {
            if (meter_map_position_of(map, musical_time_of(at), &position) == METER_MAP_OK) {
                bar = (size_t)(position.measure - meter_map_first_measure_number(map));
            }
            meter_map_destroy(map);
        }
    }
    time_signature_destroy(signature);
    return bar;
}

/* The bar (0-based from the section's start) the chord of a report entry starts in; SIZE_MAX before the section. */
static size_t entry_bar(const ChainEntry *entry, Rational origin, const PlanMeter *meter) {
    Rational from_origin;
    if (rational_sub(entry->time, origin, &from_origin) != RATIONAL_OK || rational_is_negative(from_origin)) {
        return (size_t)-1;
    }
    return bar_of(from_origin, meter);
}

static void compare_cadences(const PiecePlan *plan, NodeId section, size_t bars, Rational origin, const PlanMeter *meter,
                             const ChainResult *report, Items *items) {
    CadenceType planned[MAX_CADENCES];
    size_t count = 0;
    const SectionDescription *described = plan_section_or_empty(plan, section);
    if (described->has_cadence) {
        planned[count++] = described->cadence;
    }
    if (described->has_first_cadence && count < MAX_CADENCES) {
        planned[count++] = described->first_cadence;
    }
    /* The first cadence closes the section; the second, when there is one, closes its antecedent halfway. */
    for (size_t i = 0; i < count; i++) {
        size_t bar = i == 0 ? bars - 1 : bars / 2 - 1;
        const char *heard = "none";
        bool agreed = false;
        for (size_t c = 0; c < report->cadence_count; c++) {
            const AnalysisCadence *cadence = &report->cadences[c];
            if (cadence->arrival_first >= report->entry_count) {
                continue;
            }
            const ChainEntry *entry = &report->entries[cadence->arrival_first];
            if (entry_bar(entry, origin, meter) != bar) {
                continue;
            }
            bool matches = cadence_agrees(planned[i], cadence->type);
            if (matches || !agreed) {
                heard = cadence->type;
                agreed = matches;
            }
        }
        add_item(items, ANALYZER_CHECK_CADENCE, bar, cadence_type_name(planned[i]), heard,
                 agreed);
    }
}

static void compare_key(const ChainResult *report, SpelledKey key, Items *items) {
    char planned[32];
    char heard[32] = "none";
    bool agreed = false;
    util_key_name(spelled_key_tonic_pitch_class(&key), key.material, planned, sizeof(planned));
    if (report->key_region_count > 0) {
        const AnalysisKeyRegion *region = &report->key_regions[0];
        CommonPracticeKeyMode mode;
        if (common_practice_key_mode_of(region->material, &mode)) {
            util_key_name(region->tonic_pitch_class, region->material, heard, sizeof(heard));
            agreed = region->tonic_pitch_class == spelled_key_tonic_pitch_class(&key) &&
                     (mode == COMMON_PRACTICE_KEY_MAJOR) == (!composer_key_is_minor(&key));
        }
    }
    add_item(items, ANALYZER_CHECK_KEY, 0, planned, heard, agreed);
}

/* ------------------------------------------------------------- the check */

static AnalyzerCheckResult result_of(HearingResult status) {
    switch (status) {
    case HEARING_OK:
        return ANALYZER_CHECK_OK;
    case HEARING_ERR_NULL:
        return ANALYZER_CHECK_ERR_NULL;
    case HEARING_ERR_ALLOC:
        return ANALYZER_CHECK_ERR_ALLOC;
    case HEARING_ERR_NOT_WRITTEN:
        return ANALYZER_CHECK_ERR_NOT_WRITTEN;
    case HEARING_ERR_PLAN:
        return ANALYZER_CHECK_ERR_PLAN;
    default:
        return ANALYZER_CHECK_ERR_ANALYSIS;
    }
}

/* The hard faults the plan's own notes take part in, added to the items; first_bar is the heard span's first bar. */
static bool add_violations(ComposerHearing *hearing, const AnalysisChain *chain, size_t first_bar, Items *items,
                           bool *contradicts) {
    const PiecePlan *plan = hearing_plan(hearing);
    const RenderedPlan *rendered = hearing_rendered(hearing);
    ChainVoiceFault *violations = NULL;
    size_t violation_count = 0;
    if (!chain_result_voice_faults(chain, &violations, &violation_count)) {
        return false;
    }
    for (size_t i = 0; i < violation_count; i++) {
        bool writes = false;
        for (size_t n = 0; n < violations[i].note_id_count && !writes; n++) {
            size_t event = hearing_source_event(hearing, chain, violations[i].note_ids[n]);
            writes = event < rendered->count && plan_lane_span_state(plan, rendered->spans[event]) != PLAN_STATE_GIVEN;
        }
        if (violations[i].hard && writes) {
            size_t bar = violations[i].measure > 0 ? (size_t)violations[i].measure - 1 : 0;
            add_item(items, ANALYZER_CHECK_VIOLATION, bar >= first_bar ? bar - first_bar : 0, "none", violations[i].rule,
                     false);
            *contradicts = true;
        }
    }
    chain_voice_faults_free(violations, violation_count);
    return true;
}

AnalyzerCheckResult analyzer_check_section(const PiecePlan *plan, const KnowledgeSnapshot *knowledge, NodeId section,
                                           AnalyzerCheck *out) {
    if (!out) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    ComposerHearing *hearing = NULL;
    if (!plan) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    AnalyzerCheckResult made = result_of(hearing_create(plan, knowledge, &hearing));
    if (made == ANALYZER_CHECK_OK) {
        made = analyzer_check_section_in(hearing, section, out);
    }
    hearing_destroy(hearing);
    return made;
}

AnalyzerCheckResult analyzer_check_section_in(ComposerHearing *hearing, NodeId section, AnalyzerCheck *out) {
    if (!out) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    const PiecePlan *plan = hearing_plan(hearing);
    if (!plan || !plan_has_node(plan, section)) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    PlanSpan span = plan_node_span(plan, section);
    if (section_open(plan, span)) {
        return ANALYZER_CHECK_ERR_NOT_WRITTEN;
    }
    LaneSpanId meter_span;
    LaneSpanId key_span;
    if (!lane_at_start(plan, PLAN_LANE_METER, span, &meter_span) || !lane_at_start(plan, PLAN_LANE_KEY, span, &key_span)) {
        return ANALYZER_CHECK_ERR_PLAN;
    }
    PlanMeter meter = plan_lane_span_payload(plan, meter_span)->meter;
    SpelledKey key = plan_lane_span_payload(plan, key_span)->key;
    unsigned long bar_units = (unsigned long)meter.beats_per_bar;
    if (meter.beats_per_bar <= 0 || meter.beat_unit <= 0 || span.length.num <= 0 ||
        ((unsigned long)span.length.num * (unsigned long)meter.beat_unit) % (span.length.den * bar_units) != 0) {
        return ANALYZER_CHECK_ERR_PLAN;
    }
    size_t bars = bars_in(span.length.num, span.length.den, &meter);

    const AnalysisChain *heard = NULL;
    AnalyzerCheckResult hear_status = result_of(hearing_section(hearing, span, &heard));
    if (hear_status != ANALYZER_CHECK_OK) {
        return hear_status;
    }
    const ChainResult *report = analysis_chain_result(heard);
    Items items;
    memset(&items, 0, sizeof(items));
    compare_cadences(plan, section, bars, span.start, &meter, report, &items);
    compare_key(report, key, &items);
    out->planned = items.count;
    for (size_t i = 0; i < items.count; i++) {
        out->agreed += items.items[i].agreed;
    }
    out->contradicts = out->agreed != out->planned;
    bool contradicts = false;
    if (!add_violations(hearing, heard, bar_of(span.start, &meter), &items, &contradicts)) {
        items.failed = true;
    }
    out->contradicts = out->contradicts || contradicts;
    if (items.failed) {
        free(items.items);
        memset(out, 0, sizeof(*out));
        return ANALYZER_CHECK_ERR_ALLOC;
    }
    out->items = items.items;
    out->item_count = items.count;
    return ANALYZER_CHECK_OK;
}

/* ------------------------------------------------------------- the whole piece */

/* |a - b| at most one beat of the meter. */
static bool within_a_beat(Rational a, Rational b, const PlanMeter *meter) {
    Rational difference;
    if (rational_sub(a, b, &difference) != RATIONAL_OK) {
        return false;
    }
    if (rational_is_negative(difference) && rational_neg(difference, &difference) != RATIONAL_OK) {
        return false;
    }
    return rational_cmp(difference, rational_ratio(1, meter->beat_unit)) <= 0;
}

/* The section a node is, as the structure plan builds them: one with a piece part, which is not the seam. */
static bool is_part_section(const PiecePlan *plan, NodeId id) {
    return plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION &&
           plan_section_part_of(plan, id, NULL) &&
           !plan_section_has_role(plan, id, SECTION_ROLE_SEAM);
}

/* Where the Analyzer heard a cadence end: the start of the chord after its arrival, else the end of the fragment. */
static Rational cadence_end(const ChainResult *report, const AnalysisCadence *cadence, Rational fragment_end) {
    Rational out = fragment_end;
    if (cadence->arrival_last + 1 < report->entry_count) {
        const ChainEntry *next = &report->entries[cadence->arrival_last + 1];
        out = next->time;
    }
    return out;
}

/*
 * Whether a cadence the Analyzer heard belongs to the end of a section: it ends within one beat of it (the chord of its
 * arrival gives way to the next), or its arrival begins in the section's last bar. A tonic that arrives in the last bar
 * and carries on into the next section ends a bar or more after it, and is the section's cadence all the same.
 */
static bool cadence_at(const ChainResult *report, const AnalysisCadence *cadence, Rational section_end, Rational fragment_end,
                       const PlanMeter *meter) {
    if (within_a_beat(cadence_end(report, cadence, fragment_end), section_end, meter)) {
        return true;
    }
    if (cadence->arrival_first >= report->entry_count) {
        return false;
    }
    const ChainEntry *arrival = &report->entries[cadence->arrival_first];
    Rational start = arrival->time;
    Rational last_bar;
    Rational bar = {meter->beats_per_bar, (unsigned long)meter->beat_unit};
    if (rational_sub(section_end, bar, &last_bar) != RATIONAL_OK) {
        return false;
    }
    return rational_cmp(start, last_bar) >= 0 && rational_cmp(start, section_end) <= 0;
}

static void compare_piece_cadences(const PiecePlan *plan, const PlanMeter *meter, const ChainResult *report,
                                   Rational fragment_end, Items *items) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!is_part_section(plan, id)) {
            continue;
        }
        CadenceType planned_cadence;
        const bool planned = plan_node_cadence(plan, id, &planned_cadence);
        Rational end;
        PlanSpan span = plan_node_span(plan, id);
        if (!planned || plan_span_end(span, &end) != PLAN_OK) {
            continue;
        }
        Rational last_beat;
        Rational one_beat = {1, (unsigned long)meter->beat_unit};
        size_t bar = rational_sub(end, one_beat, &last_beat) == RATIONAL_OK && last_beat.num > 0
                         ? bar_of(last_beat, meter)
                         : 0;
        const char *heard = "none";
        bool agreed = false;
        for (size_t c = 0; c < report->cadence_count; c++) {
            const AnalysisCadence *cadence = &report->cadences[c];
            if (!cadence_at(report, cadence, end, fragment_end, meter)) {
                continue;
            }
            bool matches = cadence_agrees(planned_cadence, cadence->type);
            if (matches || !agreed) {
                heard = cadence->type;
                agreed = matches;
            }
        }
        /* A half cadence leads on: into the next section's tonic the Analyzer hears the dominant arrive and resolve, an
           authentic cadence that starts the next section; at the piece's end it has nothing to resolve into and is not
           heard at all. Neither is held against the plan. */
        bool at_end = rational_cmp(end, fragment_end) >= 0;
        /* The last cadence of the piece has no chord after it: when the Analyzer hears none there, none is held against it. */
        if (at_end && !agreed && strcmp(heard, "none") == 0) {
            heard = "none (the piece ends)";
            agreed = true;
        }
        if (planned_cadence == CADENCE_TYPE_HALF && !agreed) {
            bool last = at_end;
            bool resolved = false;
            for (size_t c = 0; c < report->cadence_count && !resolved; c++) {
                const AnalysisCadence *cadence = &report->cadences[c];
                resolved = cadence->arrival_first < report->entry_count &&
                           within_a_beat(report->entries[cadence->arrival_first].time, end, meter);
            }
            if (last || resolved) {
                heard = last ? "none (the piece ends)" : "resolved into the next section";
                agreed = true;
            }
        }
        add_item(items, ANALYZER_CHECK_CADENCE, bar, cadence_type_name(planned_cadence), heard,
                 agreed);
    }
}

AnalyzerCheckResult analyzer_check_piece_in(ComposerHearing *hearing, AnalyzerCheck *out) {
    if (!out) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    const PiecePlan *plan = hearing_plan(hearing);
    if (!plan) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    const RenderedPlan *rendered = hearing_rendered(hearing);
    PlanSpan whole = {{0, 1}, rendered->piece_end};
    if (rendered->count == 0 || section_open(plan, whole)) {
        return ANALYZER_CHECK_ERR_NOT_WRITTEN;
    }
    LaneSpanId meter_span;
    LaneSpanId key_span;
    if (!lane_at_start(plan, PLAN_LANE_METER, whole, &meter_span) || !lane_at_start(plan, PLAN_LANE_KEY, whole, &key_span)) {
        return ANALYZER_CHECK_ERR_PLAN;
    }
    PlanMeter meter = plan_lane_span_payload(plan, meter_span)->meter;
    SpelledKey key = plan_lane_span_payload(plan, key_span)->key;
    if (meter.beats_per_bar <= 0 || meter.beat_unit <= 0) {
        return ANALYZER_CHECK_ERR_PLAN;
    }
    const AnalysisChain *heard = NULL;
    AnalyzerCheckResult hear_status = result_of(hearing_piece(hearing, &heard));
    if (hear_status != ANALYZER_CHECK_OK) {
        return hear_status;
    }
    const ChainResult *report = analysis_chain_result(heard);
    Items items;
    memset(&items, 0, sizeof(items));
    compare_piece_cadences(plan, &meter, report, rendered->piece_end, &items);
    compare_key(report, key, &items);
    out->planned = items.count;
    for (size_t i = 0; i < items.count; i++) {
        out->agreed += items.items[i].agreed;
    }
    out->contradicts = out->agreed != out->planned;
    bool contradicts = false;
    if (!add_violations(hearing, heard, 0, &items, &contradicts)) {
        items.failed = true;
    }
    if (items.failed) {
        free(items.items);
        memset(out, 0, sizeof(*out));
        return ANALYZER_CHECK_ERR_ALLOC;
    }
    out->items = items.items;
    out->item_count = items.count;
    return ANALYZER_CHECK_OK;
}
