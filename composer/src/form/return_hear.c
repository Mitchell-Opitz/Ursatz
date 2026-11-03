/*
 * A framework rule check on the plan's own planned material (boundary rule 7, R-257): the thematic return relation between two named sections. Its framework
 * inputs are built from the plan, not from a render, so it stays a framework call. What a listener finds in the
 * written music is heard through the Analyzer once per run (check/hearing.h).
 */
#include <ursatz_composer/return_hear.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/thematic_return_detection.h>
#include <core/value.h>
#include <identity/entity_id.h>
#include <pitch/pitch_identity.h>
#include <relationship/relationship.h>
#include <semantics/contour.h>
#include <semantics/rhythmic_pattern.h>
#include <time/duration.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/lanes.h>

#include "form/form_internal.h"

/* The most themes read across a plan: nine sections, each with a few themes. */
#define MAX_THEMES 64
#define ID_SIZE 32

const char *return_hear_result_name(ReturnHearResult result) {
    switch (result) {
    case RETURN_HEAR_OK:
        return "ok";
    case RETURN_HEAR_ERR_NULL:
        return "null argument";
    case RETURN_HEAR_ERR_ALLOC:
        return "allocation failed";
    case RETURN_HEAR_ERR_NOT_WRITTEN:
        return "not written";
    case RETURN_HEAR_ERR_ANALYSIS:
        return "analysis failed";
    }
    return "unknown";
}

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
static bool read_notes(const PiecePlan *plan, PlanSpan range, int **pitches, Rational **durations, size_t *count) {
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
    Duration **duration_objects = calloc(count, sizeof(Duration *));
    bool ok = pitch_ids && directions && duration_objects;
    size_t made_pitches = 0;
    size_t made_durations = 0;
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
        ok = ok && duration_create(durations[i].num, durations[i].den, &duration_objects[i]) == DURATION_OK;
        made_durations += ok;
    }
    /* The Contour and the RhythmicPattern own the elements they are given and copy the arrays. */
    if (ok && contour_create(pitch_ids, count, directions, count - 1, &theme->contour) == CONTOUR_OK) {
        made_pitches = 0;
    } else {
        ok = false;
    }
    if (ok && rhythmic_pattern_create(duration_objects, count, &theme->rhythm) == RHYTHMIC_PATTERN_OK) {
        made_durations = 0;
    } else {
        ok = false;
    }
    for (size_t i = 0; pitch_ids && i < made_pitches; i++) {
        pitch_identity_destroy(pitch_ids[i]);
    }
    for (size_t i = 0; duration_objects && i < made_durations; i++) {
        duration_destroy(duration_objects[i]);
    }
    free(pitch_ids);
    free(directions);
    free(duration_objects);
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
static bool bar_length(const PiecePlan *plan, PlanSpan span, Rational *bar) {
    LaneSpanId id;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, &id, 1) < 1) {
        return false;
    }
    const PlanMeter *meter = &plan_lane_span_payload(plan, id)->meter;
    return meter->beat_unit > 0 && rational_make(meter->beats_per_bar, (long long)meter->beat_unit, bar) == RATIONAL_OK;
}

/* Adds the themes heard in a section to themes; form_hear counts bars from the start of the plan. */
static ReturnHearResult read_section(ComposerHearing *hearing, const PiecePlan *plan, NodeId section, Theme *themes, size_t *count) {
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

/* The index of the theme whose id is the entity's. */
static bool theme_of(const Theme *themes, size_t count, const EntityID *id, size_t *out) {
    for (size_t i = 0; i < count; i++) {
        if (themes[i].id && entity_id_equals(themes[i].id, id)) {
            *out = i;
            return true;
        }
    }
    return false;
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

ReturnHearResult return_hear(const PiecePlan *plan, ReturnHeard *out) {
    ComposerHearing *hearing = NULL;
    if (!plan || !out) {
        return RETURN_HEAR_ERR_NULL;
    }
    HearingResult made = hearing_create(plan, &hearing);
    if (made != HEARING_OK) {
        memset(out, 0, sizeof(*out));
        return made == HEARING_ERR_ALLOC ? RETURN_HEAR_ERR_ALLOC : RETURN_HEAR_ERR_ANALYSIS;
    }
    ReturnHearResult status = return_hear_in(hearing, plan, out);
    hearing_destroy(hearing);
    return status;
}

ReturnHearResult return_hear_in(ComposerHearing *hearing, const PiecePlan *plan, ReturnHeard *out) {
    if (!hearing || !plan || !out) {
        return RETURN_HEAR_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    Theme *themes = calloc(MAX_THEMES, sizeof(Theme));
    ThematicReturnDetectionEntry *entries = calloc(MAX_THEMES, sizeof(ThematicReturnDetectionEntry));
    size_t *entry_theme = calloc(MAX_THEMES, sizeof(size_t));
    size_t theme_count = 0;
    size_t entry_count = 0;
    Relationship **relationships = NULL;
    size_t relationship_count = 0;
    ReturnHearResult status = themes && entries && entry_theme ? RETURN_HEAR_OK : RETURN_HEAR_ERR_ALLOC;

    for (NodeId id = 1; status == RETURN_HEAR_OK && id <= plan_node_count(plan); id++) {
        if (plan_node_kind(plan, id) == PLAN_NODE_SECTION && !plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_MATERIAL) &&
            !plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
            status = read_section(hearing, plan, id, themes, &theme_count);
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
        ThematicReturnDetectionRelationshipResult built = thematic_return_detection_build_relationships_configured(
            entries, entry_count, NULL, &relationships, &relationship_count);
        status = built == THEMATIC_RETURN_DETECTION_REL_OK
                     ? RETURN_HEAR_OK
                     : (built == THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC ? RETURN_HEAR_ERR_ALLOC
                                                                           : RETURN_HEAR_ERR_ANALYSIS);
    }
    size_t capacity = 0;
    for (size_t r = 0; status == RETURN_HEAR_OK && r < relationship_count; r++) {
        size_t later;
        size_t earlier;
        const Value *qualifiers = relationship_qualifiers(relationships[r]);
        size_t size = 0;
        const ThematicReturnSimilarity *evidence = qualifiers ? value_content(qualifiers, &size) : NULL;
        if (!theme_of(themes, theme_count, relationship_source_id(relationships[r]), &later) ||
            !theme_of(themes, theme_count, relationship_target_id(relationships[r]), &earlier) || !evidence ||
            size != sizeof(*evidence)) {
            status = RETURN_HEAR_ERR_ANALYSIS;
            break;
        }
        ReturnHeardPair pair = {themes[earlier].section, themes[later].section, evidence->contour_score,
                                evidence->rhythm_score, evidence->similarity};
        if (pair.earlier != pair.later && !add_pair(out, &capacity, &pair)) {
            status = RETURN_HEAR_ERR_ALLOC;
        }
    }
    if (relationships) {
        thematic_return_detection_destroy_relationships(relationships, relationship_count);
    }
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
