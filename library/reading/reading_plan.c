#include "reading/reading_plan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/time/meter_map.h"
#include "model/time/time_signature.h"
#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "realization/performer_part.h"

typedef struct {
    NodeId node;
    Rational start;
    Rational end;
} Owner;

typedef struct {
    Plan *plan;
    NodeId form;
    Rational end;
    Owner *sections;
    size_t section_count;
} Builder;

static RealizationResult refuse(ReadingPlanError *error, const char *message) {
    if (error) {
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return REALIZATION_ERR_BAD_VALUE;
}

static RealizationResult from_plan(PlanResult result) {
    return result == PLAN_OK ? REALIZATION_OK : result == PLAN_ERR_ALLOC ? REALIZATION_ERR_ALLOC : REALIZATION_ERR_BAD_VALUE;
}

static Rational rational_max(Rational a, Rational b) {
    return rational_cmp(a, b) >= 0 ? a : b;
}

static Rational rational_min(Rational a, Rational b) {
    return rational_cmp(a, b) <= 0 ? a : b;
}

static bool length_of(Rational start, Rational end, Rational *out) {
    return rational_sub(end, start, out) == RATIONAL_OK;
}

/* The end of the last performer note of the movement; false when it has none. */
static bool last_note_end(const Movement *movement, Rational *out) {
    bool any = false;
    Rational latest = {0, 1};
    for (size_t p = 0; p < movement_part_count(movement); p++) {
        const PerformerPart *part = movement_part_at(movement, p);
        for (size_t n = 0; n < performer_part_note_count(part); n++) {
            const PerformerNote *note = performer_part_note_at(part, n);
            Rational end;
            if (rational_add(note->onset.value, note->duration.value, &end) != RATIONAL_OK) {
                return false;
            }
            latest = any ? rational_max(latest, end) : end;
            any = true;
        }
    }
    *out = latest;
    return any;
}

/* The measure that starts at or holds `time`: its number and [start, end). */
static bool measure_of(const MeterMap *map, Rational time, int *index, Rational *start, Rational *end) {
    TimeSpan span;
    MusicalTime at = musical_time_of(time);
    if (meter_map_measure_index_at(map, &at, index) != METER_MAP_OK || meter_map_measure_span(map, *index, &span) != METER_MAP_OK) {
        return false;
    }
    *start = span.start.value;
    *end = span.end.value;
    return true;
}

/* The next barline at or after `time`. */
static bool barline_at_or_after(const MeterMap *map, Rational time, Rational *out) {
    int index;
    Rational start;
    Rational end;
    if (!measure_of(map, time, &index, &start, &end)) {
        return false;
    }
    *out = rational_cmp(start, time) == 0 ? time : end;
    return true;
}

/* The last bar that starts before `end`: [bar_start, bar_end). */
static bool last_bar_before(const MeterMap *map, Rational end, Rational *bar_start, Rational *bar_end) {
    int index;
    if (!measure_of(map, end, &index, bar_start, bar_end)) {
        return false;
    }
    if (rational_cmp(*bar_start, end) != 0) {
        return true;
    }
    /* `end` is a barline: the bar that ends there is the one before it */
    TimeSpan span;
    if (meter_map_measure_span(map, index - 1, &span) != METER_MAP_OK) {
        return false;
    }
    *bar_start = span.start.value;
    *bar_end = span.end.value;
    return true;
}

static PlanResult add_meters(Builder *builder, const MeterMap *map) {
    for (size_t i = 0; i < meter_map_region_count(map); i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        Rational start = meter_map_region_start(region).value;
        MusicalTime region_end;
        Rational end = meter_map_region_end(region, &region_end) ? rational_min(region_end.value, builder->end) : builder->end;
        if (rational_cmp(start, end) >= 0) {
            continue;
        }
        PlanLanePayload payload;
        memset(&payload, 0, sizeof(payload));
        const TimeSignature *signature = meter_map_region_time_signature(region);
        payload.meter.beats_per_bar = (int)time_signature_top_number(signature);
        payload.meter.beat_unit = (int)time_signature_bottom_number(signature);
        PlanSpan span = {start, {0, 1}};
        LaneSpanId id;
        PlanResult result = length_of(start, end, &span.length) ? PLAN_OK : PLAN_ERR_BAD_TIME;
        if (result == PLAN_OK) {
            result = plan_add_lane_span(builder->plan, builder->form, PLAN_LANE_METER, 0, span, &payload, &id);
        }
        if (result == PLAN_OK) {
            result = plan_lane_span_set_state(builder->plan, id, PLAN_STATE_GIVEN);
        }
        if (result != PLAN_OK) {
            return result;
        }
    }
    return PLAN_OK;
}

static bool is_letter(const char *label) {
    return label[0] >= 'A' && label[0] <= 'Z' && label[1] == '\0';
}

/* A name no section holds yet: the letter, else the letter and a number from 2. */
static void unique_name(const Builder *builder, const Plan *plan, char letter, char out[SECTION_ID_MAX]) {
    (void)builder;
    snprintf(out, SECTION_ID_MAX, "%c", letter);
    for (int n = 2; plan_section_named(plan, out) != PLAN_NODE_NONE; n++) {
        snprintf(out, SECTION_ID_MAX, "%c%d", letter, n);
    }
}

static const ReadingCadence *last_cadence_in(const Reading *reading, Rational from, Rational to) {
    const ReadingCadence *found = NULL;
    for (size_t i = 0; i < reading_cadence_count(reading); i++) {
        const ReadingCadence *cadence = reading_cadence_at(reading, i);
        if (rational_cmp(cadence->arrival, from) >= 0 && rational_cmp(cadence->arrival, to) < 0) {
            found = cadence;
        }
    }
    return found;
}

static PlanResult add_section(Builder *builder, const Reading *reading, const MeterMap *map, const char *label, size_t position,
                              Rational start, Rational end) {
    char letter = is_letter(label) ? label[0] : (char)('A' + (position % 26));
    SectionDescription description;
    section_description_init(&description);
    char name[SECTION_ID_MAX];
    unique_name(builder, builder->plan, letter, name);
    snprintf(description.id, sizeof(description.id), "%s", name);
    snprintf(description.letter, sizeof(description.letter), "%c", letter);
    Rational bar_start;
    Rational bar_end;
    if (last_bar_before(map, end, &bar_start, &bar_end)) {
        const ReadingCadence *cadence = last_cadence_in(reading, rational_max(bar_start, start), end);
        if (cadence) {
            description.has_cadence = true;
            description.cadence = cadence->type;
        }
    }
    PlanSpan span = {start, {0, 1}};
    NodeId node;
    PlanResult result = length_of(start, end, &span.length) ? PLAN_OK : PLAN_ERR_BAD_TIME;
    if (result == PLAN_OK) {
        result = plan_add_node(builder->plan, builder->form, PLAN_NODE_SECTION, span, name, &node);
    }
    if (result == PLAN_OK) {
        result = plan_node_set_section(builder->plan, node, &description);
    }
    if (result != PLAN_OK) {
        return result;
    }
    Owner *grown = realloc(builder->sections, (builder->section_count + 1) * sizeof(*grown));
    if (!grown) {
        return PLAN_ERR_ALLOC;
    }
    builder->sections = grown;
    builder->sections[builder->section_count++] = (Owner){node, start, end};
    return PLAN_OK;
}

static PlanResult add_sections(Builder *builder, const Reading *reading, const MeterMap *map) {
    Rational cursor = {0, 1};
    size_t position = 0;
    for (size_t i = 0; i < reading_group_count(reading); i++) {
        const ReadingGroup *group = reading_group_at(reading, i);
        if (group->kind != READING_GROUP_SECTION || group->parent != READING_NO_PARENT) {
            continue;
        }
        Rational start = rational_max(group->start, cursor);
        Rational end = rational_min(group->end, builder->end);
        if (rational_cmp(start, end) >= 0) {
            continue;
        }
        PlanResult result = add_section(builder, reading, map, group->label, position++, start, end);
        if (result != PLAN_OK) {
            return result;
        }
        cursor = end;
    }
    if (builder->section_count == 0) {
        return add_section(builder, reading, map, "A", 0, (Rational){0, 1}, builder->end);
    }
    return PLAN_OK;
}

/* The owner of the stretch that starts at `at`: the section that holds it, else the form (with the end of the free stretch). */
static Owner owner_at(const Builder *builder, Rational at) {
    Rational free_end = builder->end;
    for (size_t i = 0; i < builder->section_count; i++) {
        const Owner *section = &builder->sections[i];
        if (rational_cmp(at, section->start) >= 0 && rational_cmp(at, section->end) < 0) {
            return *section;
        }
        if (rational_cmp(section->start, at) > 0) {
            free_end = rational_min(free_end, section->start);
        }
    }
    return (Owner){builder->form, at, free_end};
}

/* Indexes of claims sorted by start (a stable order for equal starts). */
typedef struct {
    Rational start;
    size_t index;
} Ordered;

static int compare_ordered(const void *a, const void *b) {
    const Ordered *x = a;
    const Ordered *y = b;
    int by_start = rational_cmp(x->start, y->start);
    return by_start != 0 ? by_start : (x->index > y->index) - (x->index < y->index);
}

/* Adds a Given lane span for [start, end) cut at the owners' bounds and above `cursor`; the cursor moves to the end of what was added. */
static PlanResult add_cut(Builder *builder, PlanLaneKind lane, Rational start, Rational end, const PlanLanePayload *payload, Rational *cursor,
                          size_t *added) {
    start = rational_max(start, *cursor);
    end = rational_min(end, builder->end);
    while (rational_cmp(start, end) < 0) {
        Owner owner = owner_at(builder, start);
        Rational piece_end = rational_min(end, owner.end);
        PlanSpan span = {start, {0, 1}};
        LaneSpanId id;
        PlanResult result = length_of(start, piece_end, &span.length) ? PLAN_OK : PLAN_ERR_BAD_TIME;
        if (result == PLAN_OK) {
            result = plan_add_lane_span(builder->plan, owner.node, lane, 0, span, payload, &id);
        }
        if (result == PLAN_OK) {
            result = plan_lane_span_set_state(builder->plan, id, PLAN_STATE_GIVEN);
        }
        if (result != PLAN_OK) {
            return result;
        }
        (*added)++;
        start = piece_end;
    }
    *cursor = rational_max(*cursor, end);
    return PLAN_OK;
}

static PlanResult add_regions(Builder *builder, const Reading *reading, size_t *added) {
    size_t count = reading_region_count(reading);
    Ordered *order = calloc(count, sizeof(*order));
    if (!order) {
        return PLAN_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        order[i] = (Ordered){reading_region_at(reading, i)->start, i};
    }
    qsort(order, count, sizeof(*order), compare_ordered);
    Rational cursor = {0, 1};
    PlanResult result = PLAN_OK;
    for (size_t i = 0; i < count && result == PLAN_OK; i++) {
        const ReadingRegion *region = reading_region_at(reading, order[i].index);
        if (!region->material) {
            continue;
        }
        PlanLanePayload payload;
        memset(&payload, 0, sizeof(payload));
        payload.region = plan_key_of_pitch_class(region->center, region->material);
        result = add_cut(builder, PLAN_LANE_REGION, region->start, region->end, &payload, &cursor, added);
    }
    free(order);
    return result;
}

static PlanResult add_harmony(Builder *builder, const Reading *reading) {
    size_t count = reading_chord_count(reading);
    Ordered *order = calloc(count, sizeof(*order));
    if (!order) {
        return PLAN_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        order[i] = (Ordered){reading_chord_at(reading, i)->start, i};
    }
    qsort(order, count, sizeof(*order), compare_ordered);
    Rational cursor = {0, 1};
    size_t added = 0;
    PlanResult result = PLAN_OK;
    for (size_t i = 0; i < count && result == PLAN_OK; i++) {
        const ReadingChord *chord = reading_chord_at(reading, order[i].index);
        if (chord->numeral[0] == '\0') {
            continue;
        }
        PlanLanePayload payload;
        memset(&payload, 0, sizeof(payload));
        snprintf(payload.harmony.roman_numeral, sizeof(payload.harmony.roman_numeral), "%s", chord->numeral);
        /* a chord belongs to the section that holds its start: it is cut at that section's end, not carried into the next one */
        Rational start = rational_max(chord->start, cursor);
        Owner owner = owner_at(builder, start);
        Rational end = rational_min(chord->end, owner.end);
        result = add_cut(builder, PLAN_LANE_HARMONY, start, end, &payload, &cursor, &added);
    }
    free(order);
    return result;
}

RealizationResult reading_to_plan(const Reading *reading, const Movement *movement, const KnowledgeSnapshot *knowledge, Plan **out,
                                  ReadingPlanError *error) {
    (void)knowledge; /* the materials of the claims are the snapshot's, borrowed by the reading */
    if (!reading || !movement || !out) {
        return REALIZATION_ERR_NULL;
    }
    if (reading_region_count(reading) == 0) {
        return refuse(error, "the reading names no region");
    }
    const ReadingTarget *target = reading_target(reading);
    if (target->digest[0] == '\0') {
        return refuse(error, "the reading names no realization version");
    }
    const MeterMap *map = movement_meter_map(movement);
    Rational last;
    Rational end;
    if (!map || !last_note_end(movement, &last) || !barline_at_or_after(map, last, &end) || rational_cmp(end, (Rational){0, 1}) <= 0) {
        return refuse(error, "the movement has no notes");
    }
    Builder builder = {plan_create(), PLAN_NODE_NONE, end, NULL, 0};
    if (!builder.plan) {
        return REALIZATION_ERR_ALLOC;
    }
    PlanSpan all = {{0, 1}, end};
    size_t regions = 0;
    PlanDerivation derivation;
    memset(&derivation, 0, sizeof(derivation));
    snprintf(derivation.reading, sizeof(derivation.reading), "%s", reading_source(reading)->name);
    snprintf(derivation.realization, sizeof(derivation.realization), "%s", target->digest);
    const char *stage = "the form";
    PlanResult result = plan_add_node(builder.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, "form", &builder.form);
    if (result == PLAN_OK) {
        stage = "the meters";
        result = add_meters(&builder, map);
    }
    if (result == PLAN_OK) {
        stage = "the sections";
        result = add_sections(&builder, reading, map);
    }
    if (result == PLAN_OK) {
        stage = "the keys";
        result = add_regions(&builder, reading, &regions);
    }
    if (result == PLAN_OK && regions == 0) {
        free(builder.sections);
        plan_destroy(builder.plan);
        return refuse(error, "the reading names no region");
    }
    if (result == PLAN_OK) {
        stage = "the chords";
        result = add_harmony(&builder, reading);
    }
    if (result == PLAN_OK) {
        stage = "the source";
        result = plan_set_derived_from(builder.plan, &derivation);
    }
    free(builder.sections);
    if (result != PLAN_OK) {
        plan_destroy(builder.plan);
        if (error) {
            snprintf(error->message, sizeof(error->message), "the reading cannot be made into a plan: %s (plan result %d)", stage,
                     (int)result);
        }
        return from_plan(result);
    }
    *out = builder.plan;
    return REALIZATION_OK;
}
