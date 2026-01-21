#include <ursatz_composer/style.h>

#include "style_internal.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/composer_api.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/render.h>

/* The key span the plan starts in. */
static bool starting_key(const PiecePlan *plan, PlanKey *out) {
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    LaneSpanId first;
    size_t count = plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, &first, 1);
    if (count == 0 || plan_time_compare(plan_lane_span_span(plan, first).start, (PlanTime){0, 1}) > 0) {
        return false;
    }
    *out = plan_lane_span_payload(plan, first)->key;
    return true;
}

ComposerMeasureResult composer_style_measure(const PiecePlan *plan, const ComposerStyle *style, StyleComparison *out) {
    if (!plan || !style || !out) {
        return COMPOSER_MEASURE_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    PlanKey key;
    if (!starting_key(plan, &key)) {
        return COMPOSER_MEASURE_ERR_PLAN;
    }
    RenderedPlan rendered;
    PlanRenderResult drawn = plan_render(plan, &rendered);
    if (drawn != PLAN_RENDER_OK) {
        return drawn == PLAN_RENDER_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_PLAN;
    }
    if (rendered.count == 0) {
        rendered_plan_destroy(&rendered);
        return COMPOSER_MEASURE_ERR_NOT_WRITTEN;
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
    ComposerMeasureResult result = COMPOSER_MEASURE_OK;
    if (analyzed != ANALYZER_FRAGMENT_OK) {
        result = analyzed == ANALYZER_FRAGMENT_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_ANALYSIS;
    } else {
        AnalyzerFragmentResult compared = analyzer_fragment_style_distance_without(
            &analysis, composer_style_model(style), composer_style_motive_categories, COMPOSER_STYLE_MOTIVE_CATEGORY_COUNT, out);
        if (compared != ANALYZER_FRAGMENT_OK) {
            memset(out, 0, sizeof(*out));
            result = compared == ANALYZER_FRAGMENT_ERR_ALLOC ? COMPOSER_MEASURE_ERR_ALLOC : COMPOSER_MEASURE_ERR_ANALYSIS;
        }
        analyzer_fragment_analysis_destroy(&analysis);
    }
    rendered_plan_destroy(&rendered);
    return result;
}
