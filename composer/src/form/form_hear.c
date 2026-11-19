#include <ursatz_composer/form_hear.h>

#include <stdlib.h>
#include <string.h>

#include <theory/form/hybrid_rules.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <model/time/time_span.h>
#include <reading/reading.h>

#include <plan/plan_lanes.h>

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
static bool lane_open(const Plan *plan, PlanLaneKind lane, PlanSpan range) {
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

/* The cadences the Analyzer heard over the span, with where each arrives (the start of the chord it arrives on): the reading's cadence claims. */
static FormHearResult copy_cadences(const Reading *reading, FormHeard *out) {
    size_t count = reading_cadence_count(reading);
    if (count == 0) {
        return FORM_HEAR_OK;
    }
    out->cadences = calloc(count, sizeof(*out->cadences));
    if (!out->cadences) {
        return FORM_HEAR_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        const ReadingCadence *claim = reading_cadence_at(reading, i);
        out->cadences[i].arrival = claim->arrival;
        out->cadences[i].type = claim->type;
    }
    out->cadence_count = count;
    return FORM_HEAR_OK;
}

/* A library name as the int the heard theme holds, -1 for none or an unknown name. */
static int form_value(bool (*from_name)(const char *, int *), const char *name) {
    int value = -1;
    return name[0] != '\0' && from_name(name, &value) ? value : -1;
}

static bool form_from_name(const char *name, int *out) {
    PhraseLevelForm value;
    bool ok = phrase_level_form_from_name(name, &value);
    *out = (int)value;
    return ok;
}

static bool beginning_from_name(const char *name, int *out) {
    PhraseBeginning value;
    bool ok = phrase_beginning_from_name(name, &value);
    *out = (int)value;
    return ok;
}

static bool ending_from_name(const char *name, int *out) {
    PhraseEnding value;
    bool ok = phrase_ending_from_name(name, &value);
    *out = (int)value;
    return ok;
}

static bool commonality_from_name(const char *name, int *out) {
    StandardFormCommonality value;
    bool ok = standard_form_commonality_from_name(name, &value);
    *out = (int)value;
    return ok;
}

/* The cadence a theme part ends in, -1 for a part with none (the reading holds CADENCE_TYPE_UNKNOWN for it). */
static int part_cadence(const ReadingGroup *group, size_t part) {
    return part < group->part_cadence_count && group->part_cadences[part] != CADENCE_TYPE_UNKNOWN ? (int)group->part_cadences[part] : -1;
}

/* The reading's theme groups as heard themes, and its phrase groups as the phrase count. */
static FormHearResult copy_themes(const Reading *reading, const MeterMap *meter_map, FormHeard *out) {
    size_t themes = 0;
    for (size_t g = 0; g < reading_group_count(reading); g++) {
        const ReadingGroup *group = reading_group_at(reading, g);
        themes += group->kind == READING_GROUP_THEME;
        out->phrase_count += group->kind == READING_GROUP_PHRASE;
    }
    if (themes == 0) {
        return FORM_HEAR_OK;
    }
    out->themes = calloc(themes, sizeof(*out->themes));
    if (!out->themes) {
        return FORM_HEAR_ERR_ALLOC;
    }
    int first_measure = meter_map_first_measure_number(meter_map);
    for (size_t g = 0; g < reading_group_count(reading); g++) {
        const ReadingGroup *group = reading_group_at(reading, g);
        if (group->kind != READING_GROUP_THEME) {
            continue;
        }
        FormHeardTheme *theme = &out->themes[out->count++];
        theme->form = form_value(form_from_name, group->form);
        theme->beginning = form_value(beginning_from_name, group->beginning);
        theme->ending = form_value(ending_from_name, group->ending);
        theme->commonality = form_value(commonality_from_name, group->commonality);
        theme->first_bar = (size_t)(group->first_measure - first_measure);
        theme->bars = (size_t)(group->last_measure - group->first_measure + 1);
        theme->phrase_count = group->phrase_count;
        theme->first_cadence = part_cadence(group, 0);
        theme->second_cadence = part_cadence(group, 1);
        theme->archetypal = group->archetypal;
        theme->has_fragmentation = group->fragmentation;
    }
    return FORM_HEAR_OK;
}

FormHearResult form_hear_in(ComposerHearing *hearing, const Plan *plan, PlanSpan span, FormHeard *out) {
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
    const Reading *result = NULL;
    FormHearResult status = result_of(hearing_section_reading(hearing, span, &result));
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

/* The key regions the Analyzer heard over the span, with where each starts: the reading's region claims. */
static FormHearResult copy_keys(const Reading *reading, FormHeardKey **out, size_t *count) {
    size_t regions = reading_region_count(reading);
    if (regions == 0) {
        return FORM_HEAR_OK;
    }
    FormHeardKey *keys = calloc(regions, sizeof(*keys));
    if (!keys) {
        return FORM_HEAR_ERR_ALLOC;
    }
    for (size_t i = 0; i < regions; i++) {
        const ReadingRegion *claim = reading_region_at(reading, i);
        keys[i].start = claim->start;
        keys[i].key = plan_key_of_pitch_class(claim->center, claim->material);
    }
    *out = keys;
    *count = regions;
    return FORM_HEAR_OK;
}

FormHearResult form_hear_keys(const Plan *plan, const KnowledgeSnapshot *knowledge, PlanSpan span, FormHeardKey **out,
                              size_t *count) {
    if (!out || !count) {
        return FORM_HEAR_ERR_NULL;
    }
    *out = NULL;
    *count = 0;
    if (!plan) {
        return FORM_HEAR_ERR_NULL;
    }
    ComposerHearing *hearing = NULL;
    FormHearResult status = result_of(hearing_create(plan, knowledge, &hearing));
    if (status != FORM_HEAR_OK) {
        return status;
    }
    if (lane_open(plan, PLAN_LANE_MELODY, span) || lane_open(plan, PLAN_LANE_BASS, span) ||
        lane_open(plan, PLAN_LANE_HARMONY, span)) {
        status = FORM_HEAR_ERR_NOT_WRITTEN;
    }
    PhraseMetre metre;
    if (status == FORM_HEAR_OK && !harmony_read_metre(plan, span, &metre)) {
        status = FORM_HEAR_ERR_METER;
    }
    const Reading *result = NULL;
    if (status == FORM_HEAR_OK) {
        status = result_of(hearing_section_reading(hearing, span, &result));
    }
    if (status == FORM_HEAR_OK) {
        status = copy_keys(result, out, count);
    }
    hearing_destroy(hearing);
    return status;
}
