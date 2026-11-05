#include <ursatz_composer/form_hear.h>

#include <stdlib.h>
#include <string.h>

#include <standard-form/analysis/analysis_theme.h>
#include <standard-form/phrase_level_form.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/result_chain.h>
#include <ursatz_chain/analysis_chain.h>

#include <ursatz_composer/lanes.h>

#include "check/hearing_result.h"
#include "form/form_internal.h"
#include "harmony/harmony_internal.h"

const char *form_hear_result_name(FormHearResult result) {
    switch (result) {
    case FORM_HEAR_OK:
        return "ok";
    case FORM_HEAR_ERR_NULL:
        return "null argument";
    case FORM_HEAR_ERR_ALLOC:
        return "out of memory";
    case FORM_HEAR_ERR_NOT_WRITTEN:
        return "lane not written";
    case FORM_HEAR_ERR_METER:
        return "meter";
    case FORM_HEAR_ERR_ANALYSIS:
        return "analysis failed";
    }
    return "unknown";
}

const char *form_hear_form_name(int form) {
    return phrase_level_form_name((PhraseLevelForm)form);
}

void form_heard_destroy(FormHeard *heard) {
    if (!heard) {
        return;
    }
    free(heard->themes);
    free(heard->cadences);
    memset(heard, 0, sizeof(*heard));
}

static FormHearResult result_of(HearingResult status) {
    switch (status) {
    case HEARING_OK:
        return FORM_HEAR_OK;
    case HEARING_ERR_NULL:
        return FORM_HEAR_ERR_NULL;
    case HEARING_ERR_ALLOC:
        return FORM_HEAR_ERR_ALLOC;
    case HEARING_ERR_NOT_WRITTEN:
        return FORM_HEAR_ERR_NOT_WRITTEN;
    case HEARING_ERR_PLAN:
        return FORM_HEAR_ERR_METER;
    default:
        return FORM_HEAR_ERR_ANALYSIS;
    }
}

/* True when a span of the lane over range is Constrained. */
static bool lane_open(const PiecePlan *plan, PlanLaneKind lane, PlanSpan range) {
    size_t count = plan_lane_find(plan, lane, 0, range, NULL, 0);
    if (count == 0) {
        return false;
    }
    LaneSpanId *ids = malloc(count * sizeof(*ids));
    if (!ids) {
        return true;
    }
    plan_lane_find(plan, lane, 0, range, ids, count);
    bool open = false;
    for (size_t i = 0; i < count && !open; i++) {
        open = plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED;
    }
    free(ids);
    return open;
}

/* The cadences the Analyzer heard over the span, with where each arrives (the start of the chord it arrives on). */
static FormHearResult copy_cadences(const AnalyzerResult *result, FormHeard *out) {
    const HarmonyCadences *cadences = analysis_chain_cadences(analyzer_result_chain(result));
    const AnalysisReport *report = analyzer_result_report(result);
    if (!cadences || cadences->count == 0) {
        return FORM_HEAR_OK;
    }
    out->cadences = calloc(cadences->count, sizeof(*out->cadences));
    if (!out->cadences) {
        return FORM_HEAR_ERR_ALLOC;
    }
    for (size_t i = 0; i < cadences->count; i++) {
        size_t first = cadences->items[i].arrival_first;
        if (first >= report->entry_count ||
            rational_make(report->entries[first].time_numerator, report->entries[first].time_denominator,
                          &out->cadences[i].arrival) != RATIONAL_OK) {
            return FORM_HEAR_ERR_ANALYSIS;
        }
        out->cadences[i].type = cadences->items[i].type;
    }
    out->cadence_count = cadences->count;
    return FORM_HEAR_OK;
}

/* The Analyzer's themes as heard themes. */
static FormHearResult copy_themes(const AnalyzerResult *result, const MeterMap *meter_map, FormHeard *out) {
    const AnalysisChain *chain = analyzer_result_chain(result);
    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    const StandardFormThemes *themes = analysis_chain_themes(chain);
    out->phrase_count = phrases ? phrases->count : 0;
    if (!themes || themes->count == 0) {
        return FORM_HEAR_OK;
    }
    out->themes = calloc(themes->count, sizeof(*out->themes));
    if (!out->themes) {
        return FORM_HEAR_ERR_ALLOC;
    }
    int first_measure = meter_map_first_measure_number(meter_map);
    for (size_t i = 0; i < themes->count; i++) {
        const StandardFormThemeClaim *claim = &themes->items[i].claim;
        FormHeardTheme *theme = &out->themes[i];
        theme->form = claim->form;
        theme->beginning = claim->beginning;
        theme->ending = claim->ending;
        theme->commonality = claim->commonality;
        theme->first_bar = (size_t)(claim->first_measure - first_measure);
        theme->bars = (size_t)(claim->last_measure - claim->first_measure + 1);
        theme->phrase_count = (size_t)claim->phrase_count;
        theme->first_cadence = claim->first_cadence_type;
        theme->second_cadence = claim->second_cadence_type;
        theme->archetypal = claim->archetypal != 0;
        theme->has_fragmentation = claim->has_fragmentation != 0;
    }
    out->count = themes->count;
    return FORM_HEAR_OK;
}

FormHearResult form_hear_in(ComposerHearing *hearing, const PiecePlan *plan, PlanSpan span, FormHeard *out) {
    if (!out) {
        return FORM_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan || !hearing) {
        return FORM_HEAR_ERR_NULL;
    }
    if (lane_open(plan, PLAN_LANE_MELODY, span) || lane_open(plan, PLAN_LANE_BASS, span) ||
        lane_open(plan, PLAN_LANE_HARMONY, span)) {
        return FORM_HEAR_ERR_NOT_WRITTEN;
    }
    PhraseMetre metre;
    if (!harmony_read_metre(plan, span, &metre)) {
        return FORM_HEAR_ERR_METER;
    }
    const AnalyzerResult *result = NULL;
    FormHearResult status = result_of(hearing_section(hearing, span, &result));
    if (status == FORM_HEAR_OK) {
        status = copy_cadences(result, out);
    }
    if (status == FORM_HEAR_OK) {
        status = copy_themes(result, hearing_rendered(hearing)->meter_map, out);
    }
    if (status != FORM_HEAR_OK) {
        form_heard_destroy(out);
    }
    return status;
}

FormHearResult form_hear(const PiecePlan *plan, PlanSpan span, FormHeard *out) {
    return form_hear_with_sections(plan, span, NULL, 0, out);
}

/*
 * The Analyzer's hearing takes no section starts (AnalyzerHearingOptions holds only the stages, not
 * refine_with_given_boundaries), so the starts are not passed on: this is form_hear.
 */
FormHearResult form_hear_with_sections(const PiecePlan *plan, PlanSpan span, const Rational *section_starts,
                                       size_t section_count, FormHeard *out) {
    (void)section_starts;
    (void)section_count;
    if (!out) {
        return FORM_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    if (!plan) {
        return FORM_HEAR_ERR_NULL;
    }
    ComposerHearing *hearing = NULL;
    FormHearResult status = result_of(hearing_create(plan, &hearing));
    if (status == FORM_HEAR_OK) {
        status = form_hear_in(hearing, plan, span, out);
    }
    hearing_destroy(hearing);
    return status;
}
