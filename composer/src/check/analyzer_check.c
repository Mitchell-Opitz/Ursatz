#include <ursatz_composer/analyzer_check.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <harmony_cadence_rules.h>

#include <ursatz_analyzer/composer_api.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#define MAX_CADENCES 2

const char *analyzer_check_result_name(AnalyzerCheckResult result) {
    static const char *const NAMES[] = {"ok", "null", "alloc", "not-written", "plan", "analysis"};
    return (size_t)result < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[result] : "unknown";
}

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
    PlanTime end;
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
            plan_time_compare(span.start, section.start) >= 0 && plan_time_compare(span.start, end) < 0 &&
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
    bool found = plan_time_compare(plan_lane_span_span(plan, ids[0]).start, section.start) <= 0;
    *out = ids[0];
    free(ids);
    return found;
}

static void key_name(char *out, size_t size, int tonic_pitch_class, bool minor) {
    static const char *const NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    snprintf(out, size, "%s %s", NAMES[tonic_pitch_class % 12], minor ? "minor" : "major");
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
static bool cadence_agrees(PlanCadenceType planned, const char *heard) {
    const char *name = cadence_type_name(harmony_cadence_type_of(planned));
    if (planned == PLAN_CADENCE_PAC || planned == PLAN_CADENCE_IAC) {
        return authentic_name(heard);
    }
    return strcmp(name, heard) == 0;
}

/* The bar (0-based from the fragment's start) a time in whole notes falls in. */
static size_t bar_of(long numerator, unsigned long denominator, const PlanMeter *meter) {
    if (numerator < 0) {
        return 0;
    }
    return (size_t)(((unsigned long)numerator * (unsigned long)meter->beat_unit) /
                    (denominator * (unsigned long)meter->beats_per_bar));
}

static void compare_cadences(const PiecePlan *plan, NodeId section, size_t bars, const PlanMeter *meter,
                             const AnalysisReport *report, Items *items) {
    PlanCadenceType planned[MAX_CADENCES];
    size_t count = 0;
    for (size_t i = 0; i < plan_node_constraint_count(plan, section) && count < MAX_CADENCES; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            planned[count++] = constraint->params.cadence;
        }
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
            const AnalysisReportEntry *entry = &report->entries[cadence->arrival_first];
            if (bar_of(entry->time_numerator, entry->time_denominator, meter) != bar) {
                continue;
            }
            bool matches = cadence_agrees(planned[i], cadence->type);
            if (matches || !agreed) {
                heard = cadence->type;
                agreed = matches;
            }
        }
        add_item(items, ANALYZER_CHECK_CADENCE, bar, cadence_type_name(harmony_cadence_type_of(planned[i])), heard,
                 agreed);
    }
}

static void compare_key(const AnalysisReport *report, PlanKey key, Items *items) {
    char planned[32];
    char heard[32] = "none";
    bool agreed = false;
    key_name(planned, sizeof(planned), key.tonic_pitch_class, key.mode == PLAN_MODE_MINOR);
    if (report->key_region_count > 0) {
        const AnalysisKeyRegion *region = &report->key_regions[0];
        key_name(heard, sizeof(heard), region->tonic_pitch_class, !region->is_major);
        agreed = region->tonic_pitch_class == key.tonic_pitch_class && region->is_major == (key.mode == PLAN_MODE_MAJOR);
    }
    add_item(items, ANALYZER_CHECK_KEY, 0, planned, heard, agreed);
}

/* Whether a hard fault involves a note the plan wrote: a note in a span that is not Given. */
static bool writes_note(const PiecePlan *plan, const RenderedPlan *rendered, const AnalyzerViolation *violation) {
    for (size_t n = 0; n < violation->note_id_count; n++) {
        size_t id = violation->note_ids[n];
        if (id < rendered->count && plan_lane_span_state(plan, rendered->spans[id]) != PLAN_STATE_GIVEN) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------- the check */

AnalyzerCheckResult analyzer_check_section(const PiecePlan *plan, NodeId section, AnalyzerCheck *out) {
    if (!out) {
        return ANALYZER_CHECK_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
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
    PlanKey key = plan_lane_span_payload(plan, key_span)->key;
    unsigned long bar_units = (unsigned long)meter.beats_per_bar;
    if (meter.beats_per_bar <= 0 || meter.beat_unit <= 0 || span.length.num <= 0 ||
        ((unsigned long)span.length.num * (unsigned long)meter.beat_unit) % (span.length.den * bar_units) != 0) {
        return ANALYZER_CHECK_ERR_PLAN;
    }
    size_t bars = bar_of(span.length.num, span.length.den, &meter);

    RenderedPlan rendered;
    PlanRenderResult drawn = plan_render_range(plan, span, &rendered);
    if (drawn != PLAN_RENDER_OK) {
        return drawn == PLAN_RENDER_ERR_ALLOC ? ANALYZER_CHECK_ERR_ALLOC : ANALYZER_CHECK_ERR_PLAN;
    }
    if (rendered.count == 0) {
        rendered_plan_destroy(&rendered);
        return ANALYZER_CHECK_ERR_NOT_WRITTEN;
    }
    AnalyzerFragment fragment;
    memset(&fragment, 0, sizeof(fragment));
    fragment.events = (const NoteEvent *const *)rendered.events;
    fragment.count = rendered.count;
    fragment.meter = rendered.meter_map;
    fragment.has_key = true;
    fragment.key_tonic_pitch_class = key.tonic_pitch_class;
    fragment.key_minor = key.mode == PLAN_MODE_MINOR;
    fragment.streams = rendered.part_index;

    AnalyzerFragmentAnalysis analysis;
    AnalyzerFragmentResult analyzed = analyzer_analyze_fragment(&fragment, &analysis);
    if (analyzed != ANALYZER_FRAGMENT_OK) {
        rendered_plan_destroy(&rendered);
        return analyzed == ANALYZER_FRAGMENT_ERR_ALLOC ? ANALYZER_CHECK_ERR_ALLOC : ANALYZER_CHECK_ERR_ANALYSIS;
    }
    const AnalysisReport *report = analyzer_fragment_report(&analysis);
    Items items;
    memset(&items, 0, sizeof(items));
    compare_cadences(plan, section, bars, &meter, report, &items);
    compare_key(report, key, &items);
    out->planned = items.count;
    for (size_t i = 0; i < items.count; i++) {
        out->agreed += items.items[i].agreed;
    }
    out->contradicts = out->agreed != out->planned;

    AnalyzerViolation *violations = NULL;
    size_t violation_count = 0;
    if (analyzer_fragment_violations(&analysis, &violations, &violation_count) != ANALYZER_FRAGMENT_OK) {
        items.failed = true;
    }
    for (size_t i = 0; i < violation_count; i++) {
        if (violations[i].hard && writes_note(plan, &rendered, &violations[i])) {
            add_item(&items, ANALYZER_CHECK_VIOLATION, violations[i].measure > 0 ? (size_t)violations[i].measure - 1 : 0, "none",
                     violations[i].rule, false);
            out->contradicts = true;
        }
    }
    analyzer_violations_free(violations, violation_count);
    analyzer_fragment_analysis_destroy(&analysis);
    rendered_plan_destroy(&rendered);
    if (items.failed) {
        free(items.items);
        memset(out, 0, sizeof(*out));
        return ANALYZER_CHECK_ERR_ALLOC;
    }
    out->items = items.items;
    out->item_count = items.count;
    return ANALYZER_CHECK_OK;
}
