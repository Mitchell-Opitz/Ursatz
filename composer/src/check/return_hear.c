/*
 * A framework rule check on the plan's own planned material (boundary rule 7, R-257): the thematic return relation between two named sections. Its framework
 * inputs are built from the plan, not from a render, so it stays a framework call. What a listener finds in the
 * written music is heard through the Analyzer once per run (check/hearing.h).
 */
#include <ursatz_composer/return_hear.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/form/thematic_return_detection.h>
#include <model/core/value.h>
#include <model/identity/entity_id.h>
#include <model/pitch/pitch_identity.h>
#include <analysis/notes/contour.h>
#include <analysis/notes/rhythmic_pattern.h>
#include <model/time/duration.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/form_hear.h>
#include <plan/plan_lanes.h>

#include "check/form_internal.h"
#include "structure/plan_util.h"

/* The most themes read across a plan: nine sections, each with a few themes. */
#define MAX_THEMES 64
#define ID_SIZE 32

void return_heard_destroy(ReturnHeard *heard) {
    if (heard) {
        free(heard->pairs);
        memset(heard, 0, sizeof(*heard));
    }
}

const ReturnHeardPair *return_heard_find(const ReturnHeard *heard, NodeId earlier, NodeId later) {
    for (size_t i = 0; heard && i < heard->count; i++) {
        if (heard->pairs[i].earlier == earlier && heard->pairs[i].later == later) {
            return &heard->pairs[i];
        }
    }
    return NULL;
}

/* One theme heard, as the detection takes it. */
typedef struct {
    NodeId section;
    EntityID *id;
    EntityID **event_ids;
    size_t note_count;
    Contour *contour;
    RhythmicPattern *rhythm;
    MotifWindow window;
    bool usable;
    /* A section with a FIGURE and the section it returns, each heard on its skeleton: the figured section. Else PLAN_NODE_NONE. */
    NodeId reduced_for;
} Theme;

static void theme_destroy(Theme *theme) {
    if (theme->contour) {
        contour_destroy(theme->contour);
    }
    if (theme->rhythm) {
        rhythmic_pattern_destroy(theme->rhythm);
    }
    for (size_t i = 0; theme->event_ids && i < theme->note_count; i++) {
        if (theme->event_ids[i]) {
            entity_id_destroy(theme->event_ids[i]);
        }
    }
    free(theme->event_ids);
    if (theme->id) {
        entity_id_destroy(theme->id);
    }
    memset(theme, 0, sizeof(*theme));
}

/* Every melody note that starts inside range, in order: its pitch and its duration. False on allocation failure. */
static bool read_notes(const Plan *plan, PlanSpan range, int **pitches, Rational **durations, size_t *count) {
    Rational end;
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, range, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
    size_t capacity = 0;
    *pitches = NULL;
    *durations = NULL;
    *count = 0;
    if ((spans && !ids) || plan_span_end(range, &end) != PLAN_OK) {
        free(ids);
        return false;
    }
    plan_lane_find(plan, PLAN_LANE_MELODY, 0, range, ids, spans);
    for (size_t s = 0; s < spans; s++) {
        capacity += plan_lane_span_payload(plan, ids[s])->notes.note_count;
    }
    *pitches = capacity ? calloc(capacity, sizeof(int)) : NULL;
    *durations = capacity ? calloc(capacity, sizeof(Rational)) : NULL;
    if (capacity && (!*pitches || !*durations)) {
        free(ids);
        free(*pitches);
        free(*durations);
        *pitches = NULL;
        *durations = NULL;
        return false;
    }
    for (size_t s = 0; s < spans; s++) {
        PlanSpan span = plan_lane_span_span(plan, ids[s]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational onset;
            if (rational_add(span.start, notes->notes[n].offset, &onset) != RATIONAL_OK) {
                continue;
            }
            if (rational_cmp(onset, range.start) >= 0 && rational_cmp(onset, end) < 0) {
                (*pitches)[*count] = notes->notes[n].midi;
                (*durations)[*count] = notes->notes[n].duration;
                (*count)++;
            }
        }
    }
    free(ids);
    return true;
}

/* The Contour and RhythmicPattern of the notes into the theme, which then owns them. False when they cannot be made. */
static bool make_window(Theme *theme, const int *pitches, const Rational *durations, size_t count) {
    PitchIdentity **pitch_ids = calloc(count, sizeof(PitchIdentity *));
    ContourDirection *directions = calloc(count - 1, sizeof(ContourDirection));
    Duration *duration_values = calloc(count, sizeof(Duration));
    bool ok = pitch_ids && directions && duration_values;
    size_t made_pitches = 0;
    for (size_t i = 0; ok && i < count; i++) {
        char token[ID_SIZE];
        int length = snprintf(token, sizeof(token), "p%d-%zu", pitches[i], i);
        ok = pitch_identity_create(token, (size_t)length, &pitch_ids[i]) == PITCH_IDENTITY_OK;
        made_pitches += ok;
        if (ok && i > 0) {
            directions[i - 1] = pitches[i] > pitches[i - 1]   ? CONTOUR_DIRECTION_UP
                                : pitches[i] < pitches[i - 1] ? CONTOUR_DIRECTION_DOWN
                                                              : CONTOUR_DIRECTION_SAME;
        }
        ok = ok && duration_of(durations[i], &duration_values[i]) == DURATION_OK;
    }
    /* The Contour owns the elements it is given and copies the arrays; the pattern copies the durations. */
    if (ok && contour_create(pitch_ids, count, directions, count - 1, &theme->contour) == CONTOUR_OK) {
        made_pitches = 0;
    } else {
        ok = false;
    }
    if (!ok || rhythmic_pattern_create(duration_values, count, &theme->rhythm) != RHYTHMIC_PATTERN_OK) {
        ok = false;
    }
    for (size_t i = 0; pitch_ids && i < made_pitches; i++) {
        pitch_identity_destroy(pitch_ids[i]);
    }
    free(pitch_ids);
    free(directions);
    free(duration_values);
    if (!ok) {
        return false;
    }
    theme->event_ids = calloc(count, sizeof(EntityID *));
    theme->note_count = count;
    for (size_t i = 0; theme->event_ids && ok && i < count; i++) {
        char value[ID_SIZE];
        snprintf(value, sizeof(value), "note-%p-%zu", (void *)theme, i);
        ok = entity_id_create(value, &theme->event_ids[i]) == ENTITY_ID_OK;
    }
    if (!ok || !theme->event_ids) {
        return false;
    }
    theme->window.contour = theme->contour;
    theme->window.rhythm = theme->rhythm;
    theme->window.event_ids = (const EntityID *const *)theme->event_ids;
    theme->window.event_id_count = count;
    return true;
}

/* The bar of the meter under a section, in whole notes. */
static bool bar_length(const Plan *plan, PlanSpan span, Rational *bar) {
    LaneSpanId id;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, &id, 1) < 1) {
        return false;
    }
    const PlanMeter *meter = &plan_lane_span_payload(plan, id)->meter;
    return meter->beat_unit > 0 && plan_meter_bar_length(meter, bar) == PLAN_OK;
}

/* Adds the themes heard in a section to themes; form_hear_in counts bars from the start of the plan. */
static ReturnHearResult read_section(ComposerHearing *hearing, const Plan *plan, NodeId section, Theme *themes, size_t *count) {
    FormHeard heard;
    PlanSpan span = plan_node_span(plan, section);
    Rational bar;
    FormHearResult result = form_hear_in(hearing, plan, span, &heard);
    if (result == FORM_HEAR_ERR_NOT_WRITTEN) {
        return RETURN_HEAR_ERR_NOT_WRITTEN;
    }
    if (result == FORM_HEAR_ERR_ALLOC) {
        return RETURN_HEAR_ERR_ALLOC;
    }
    if (result != FORM_HEAR_OK || !bar_length(plan, span, &bar)) {
        form_heard_destroy(&heard);
        return RETURN_HEAR_ERR_ANALYSIS;
    }
    ReturnHearResult status = RETURN_HEAR_OK;
    for (size_t t = 0; status == RETURN_HEAR_OK && t < heard.count && *count < MAX_THEMES; t++) {
        PlanSpan range;
        int *pitches;
        Rational *durations;
        size_t notes;
        if (rational_make(bar.num * (long)heard.themes[t].first_bar, bar.den, &range.start) != RATIONAL_OK ||
            rational_make(bar.num * (long)heard.themes[t].bars, bar.den, &range.length) != RATIONAL_OK) {
            status = RETURN_HEAR_ERR_ANALYSIS;
            break;
        }
        if (!read_notes(plan, range, &pitches, &durations, &notes)) {
            status = RETURN_HEAR_ERR_ALLOC;
            break;
        }
        Theme *theme = &themes[*count];
        char value[ID_SIZE];
        snprintf(value, sizeof(value), "theme-occurrence-%zu", *count);
        theme->section = section;
        if (entity_id_create(value, &theme->id) != ENTITY_ID_OK) {
            status = RETURN_HEAR_ERR_ALLOC;
        } else if (notes >= THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH) {
            theme->usable = make_window(theme, pitches, durations, notes);
            status = theme->usable ? RETURN_HEAR_OK : RETURN_HEAR_ERR_ALLOC;
        }
        (*count)++;
        free(pitches);
        free(durations);
    }
    form_heard_destroy(&heard);
    return status;
}

/* The section a section with a FIGURE returns (R-497); false for a section with no FIGURE or no RETURN. */
static bool figured_return(const Plan *plan, NodeId section, NodeId *source) {
    const SectionDescription *described = plan_section_or_empty(plan, section);
    *source = described->returns[0] != '\0' ? plan_section_named(plan, described->returns) : PLAN_NODE_NONE;
    return described->has_figure && *source != PLAN_NODE_NONE;
}

/* A melody note: where it starts and ends, from the start of the plan, and its pitch. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
} HeardNote;

/*
 * The melody of a section reduced to one note per cell of the meter: the note sounding at the cell's start, lasting the
 * cell (a cell with no note sounding is left out). The cells are the beats the figure is made on (form_figure.h), from the
 * start of the section: a quarter, a dotted quarter in 6/8. False on allocation failure or a meter without cells.
 */
static bool read_skeleton(const Plan *plan, NodeId section, int **pitches, Rational **durations, size_t *count) {
    PlanSpan span = plan_node_span(plan, section);
    PlanMeter meter;
    Rational cell;
    Rational end;
    LaneSpanId meter_id;
    *pitches = NULL;
    *durations = NULL;
    *count = 0;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, &meter_id, 1) < 1) {
        return false;
    }
    meter = plan_lane_span_payload(plan, meter_id)->meter;
    bool compound = meter.beat_unit == 8 && meter.beats_per_bar == 6;
    if ((!compound && meter.beat_unit != 4) || rational_make(compound ? 3 : 1, compound ? 8 : 4, &cell) != RATIONAL_OK ||
        plan_span_end(span, &end) != PLAN_OK) {
        return false;
    }
    size_t cells = (size_t)(compound ? 2 : (meter.beats_per_bar > 0 ? meter.beats_per_bar : 1));
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
    size_t total = 0;
    if (spans && !ids) {
        return false;
    }
    plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, spans);
    for (size_t s = 0; s < spans; s++) {
        total += plan_lane_span_payload(plan, ids[s])->notes.note_count;
    }
    HeardNote *notes = total ? calloc(total, sizeof(HeardNote)) : NULL;
    size_t used = 0;
    for (size_t s = 0; notes && s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        const PlanNotes *held = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; n < held->note_count; n++) {
            HeardNote *note = &notes[used];
            if (rational_add(lane_span.start, held->notes[n].offset, &note->onset) == RATIONAL_OK &&
                rational_add(note->onset, held->notes[n].duration, &note->end) == RATIONAL_OK) {
                note->pitch = held->notes[n].midi;
                used++;
            }
        }
    }
    free(ids);
    size_t capacity = (size_t)(span.length.num * (long long)cells / (span.length.den ? span.length.den : 1)) + cells + 1;
    *pitches = calloc(capacity, sizeof(int));
    *durations = calloc(capacity, sizeof(Rational));
    bool ok = (total == 0 || notes) && *pitches && *durations;
    Rational start = span.start;
    for (size_t k = 0; ok; k++) {
        if (rational_cmp(start, end) >= 0) {
            break;
        }
        for (size_t n = 0; n < used && *count < capacity; n++) {
            if (rational_cmp(notes[n].onset, start) <= 0 && rational_cmp(start, notes[n].end) < 0) {
                (*pitches)[*count] = notes[n].pitch;
                (*durations)[*count] = cell;
                (*count)++;
                break;
            }
        }
        ok = rational_add(start, cell, &start) == RATIONAL_OK;
    }
    free(notes);
    if (!ok) {
        free(*pitches);
        free(*durations);
        *pitches = NULL;
        *durations = NULL;
        *count = 0;
    }
    return ok;
}

/* Adds the skeleton of a section to themes, heard for the figured section; the skeleton is one window of the whole section. */
static ReturnHearResult add_skeleton(const Plan *plan, NodeId section, NodeId figured, Theme *themes, size_t *count) {
    int *pitches;
    Rational *durations;
    size_t notes;
    char value[ID_SIZE];
    if (*count >= MAX_THEMES) {
        return RETURN_HEAR_OK;
    }
    if (!read_skeleton(plan, section, &pitches, &durations, &notes)) {
        return RETURN_HEAR_ERR_ANALYSIS;
    }
    Theme *theme = &themes[*count];
    ReturnHearResult status = RETURN_HEAR_OK;
    snprintf(value, sizeof(value), "theme-occurrence-%zu", *count);
    theme->section = section;
    theme->reduced_for = figured;
    if (entity_id_create(value, &theme->id) != ENTITY_ID_OK) {
        status = RETURN_HEAR_ERR_ALLOC;
    } else if (notes >= THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH) {
        theme->usable = make_window(theme, pitches, durations, notes);
        status = theme->usable ? RETURN_HEAR_OK : RETURN_HEAR_ERR_ALLOC;
    }
    (*count)++;
    free(pitches);
    free(durations);
    return status;
}

/* Keeps the best similarity of the themes of a pair of sections. */
static bool add_pair(ReturnHeard *out, size_t *capacity, const ReturnHeardPair *pair) {
    for (size_t i = 0; i < out->count; i++) {
        if (out->pairs[i].earlier == pair->earlier && out->pairs[i].later == pair->later) {
            if (pair->similarity > out->pairs[i].similarity) {
                out->pairs[i] = *pair;
            }
            return true;
        }
    }
    if (out->count == *capacity) {
        size_t grown = *capacity ? *capacity * 2 : 8;
        ReturnHeardPair *pairs = realloc(out->pairs, grown * sizeof(ReturnHeardPair));
        if (!pairs) {
            return false;
        }
        out->pairs = pairs;
        *capacity = grown;
    }
    out->pairs[out->count++] = *pair;
    return true;
}

ReturnHearResult return_hear_in(ComposerHearing *hearing, const Plan *plan, ReturnHeard *out) {
    if (!hearing || !plan || !out) {
        return RETURN_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    Theme *themes = calloc(MAX_THEMES, sizeof(Theme));
    ThematicReturnDetectionEntry *entries = calloc(MAX_THEMES, sizeof(ThematicReturnDetectionEntry));
    size_t *entry_theme = calloc(MAX_THEMES, sizeof(size_t));
    size_t theme_count = 0;
    size_t entry_count = 0;
    ThematicReturnPair *found = NULL;
    size_t found_count = 0;
    ReturnHearResult status = themes && entries && entry_theme ? RETURN_HEAR_OK : RETURN_HEAR_ERR_ALLOC;

    for (NodeId id = 1; status == RETURN_HEAR_OK && id <= plan_node_count(plan); id++) {
        if (plan_node_kind(plan, id) == PLAN_NODE_SECTION && !plan_section_has_role(plan, id, SECTION_ROLE_MATERIAL) &&
            !plan_section_has_role(plan, id, SECTION_ROLE_SEAM)) {
            status = read_section(hearing, plan, id, themes, &theme_count);
        }
    }
    /* A section with a FIGURE is heard against the section it returns on their skeletons (R-497). */
    for (NodeId id = 1; status == RETURN_HEAR_OK && id <= plan_node_count(plan); id++) {
        NodeId source;
        if (plan_node_kind(plan, id) == PLAN_NODE_SECTION && figured_return(plan, id, &source)) {
            status = add_skeleton(plan, source, id, themes, &theme_count);
            status = status == RETURN_HEAR_OK ? add_skeleton(plan, id, id, themes, &theme_count) : status;
        }
    }
    for (size_t t = 0; status == RETURN_HEAR_OK && t < theme_count; t++) {
        if (themes[t].usable) {
            entries[entry_count].theme_occurrence_id = themes[t].id;
            entries[entry_count].window = &themes[t].window;
            entry_theme[entry_count++] = t;
        }
    }
    if (status == RETURN_HEAR_OK) {
        ThematicReturnDetectionResult built =
            thematic_return_detection_build_pairs_configured(entries, entry_count, NULL, &found, &found_count);
        status = built == THEMATIC_RETURN_DETECTION_OK
                     ? RETURN_HEAR_OK
                     : (built == THEMATIC_RETURN_DETECTION_ERR_ALLOC ? RETURN_HEAR_ERR_ALLOC : RETURN_HEAR_ERR_ANALYSIS);
    }
    size_t capacity = 0;
    for (size_t r = 0; status == RETURN_HEAR_OK && r < found_count; r++) {
        size_t later = entry_theme[found[r].later];
        size_t earlier = entry_theme[found[r].earlier];
        ReturnHeardPair pair = {themes[earlier].section, themes[later].section, found[r].contour_score,
                                found[r].rhythm_score, found[r].similarity};
        /* A skeleton is heard only against the skeleton of its own figured section; the figured section's pair with the
           section it returns is the skeletons', not the notes'. */
        NodeId source;
        bool replaced = themes[earlier].reduced_for == PLAN_NODE_NONE && themes[later].reduced_for == PLAN_NODE_NONE &&
                        figured_return(plan, pair.later, &source) && source == pair.earlier;
        if (themes[earlier].reduced_for != themes[later].reduced_for || replaced) {
            continue;
        }
        if (pair.earlier != pair.later && !add_pair(out, &capacity, &pair)) {
            status = RETURN_HEAR_ERR_ALLOC;
        }
    }
    free(found);
    for (size_t t = 0; themes && t < theme_count; t++) {
        theme_destroy(&themes[t]);
    }
    free(themes);
    free(entries);
    free(entry_theme);
    if (status != RETURN_HEAR_OK) {
        return_heard_destroy(out);
    }
    return status;
}
