#include <ursatz_composer/piece_structure.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "piece/form_return_internal.h"
#include "piece/piece_internal.h"

static const char *const PART_NAMES[PIECE_PART_COUNT] = {"intro", "body", "turnaround", "ending"};
static const char *const FILL_NAMES[] = {"empty", "motif", "theme", "progression"};

/* The bars of the ending section piece_structure_default_ending adds. */
#define DEFAULT_ENDING_BARS 2

const char *piece_part_name(PiecePart part) {
    return part >= 0 && part < PIECE_PART_COUNT ? PART_NAMES[part] : "unknown";
}

bool piece_part_from_name(const char *name, PiecePart *out) {
    for (int part = 0; name && part < PIECE_PART_COUNT; part++) {
        if (strcmp(name, PART_NAMES[part]) == 0) {
            if (out) {
                *out = (PiecePart)part;
            }
            return true;
        }
    }
    return false;
}

const char *piece_fill_name(PieceFillKind fill) {
    return fill >= PIECE_FILL_EMPTY && fill <= PIECE_FILL_PROGRESSION ? FILL_NAMES[fill] : "unknown";
}

bool piece_fill_from_name(const char *name, PieceFillKind *out) {
    for (int fill = PIECE_FILL_EMPTY; name && fill <= PIECE_FILL_PROGRESSION; fill++) {
        if (strcmp(name, FILL_NAMES[fill]) == 0) {
            if (out) {
                *out = (PieceFillKind)fill;
            }
            return true;
        }
    }
    return false;
}

/* Whether the pickup is a multiple of 1/32 of a whole note, not negative and shorter than a bar. */
static bool pickup_valid(PlanTime pickup, PlanTime bar) {
    if (pickup.den == 0 || pickup.num < 0 || plan_time_compare(pickup, bar) >= 0) {
        return false;
    }
    return (pickup.num * (long)PIECE_PICKUP_GRID) % (long)pickup.den == 0;
}

PieceResult piece_structure_check(const PieceStructure *structure) {
    if (!structure || (structure->section_count > 0 && !structure->sections)) {
        return PIECE_ERR_NULL;
    }
    size_t cells;
    if (!grow_cells_per_bar(&structure->meter, &cells) || structure->key.tonic_pitch_class < 0 ||
        structure->key.tonic_pitch_class > 11 ||
        (structure->key.mode != PLAN_MODE_MAJOR && structure->key.mode != PLAN_MODE_MINOR) ||
        structure->meter.beats_per_bar <= 0 || structure->meter.beat_unit <= 0 ||
        structure->section_count > PIECE_STRUCTURE_MAX_SECTIONS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanTime bar = grow_bar_length(&structure->meter);
    size_t beats = 0;
    bool body = false;
    int last_part = PIECE_PART_INTRO;
    for (size_t i = 0; i < structure->section_count; i++) {
        const PieceSection *section = &structure->sections[i];
        if (section->id[0] == '\0' || strnlen(section->id, PIECE_SECTION_ID_MAX) == PIECE_SECTION_ID_MAX ||
            section->part < PIECE_PART_INTRO || section->part > PIECE_PART_ENDING || (int)section->part < last_part ||
            section->beats < 1 || !pickup_valid(section->pickup, bar) ||
            section->fill < PIECE_FILL_EMPTY || section->fill > PIECE_FILL_PROGRESSION) {
            return PIECE_ERR_BAD_REQUEST;
        }
        for (size_t other = 0; other < i; other++) {
            if (strcmp(structure->sections[other].id, section->id) == 0) {
                return PIECE_ERR_BAD_REQUEST;
            }
        }
        last_part = (int)section->part;
        body = body || section->part == PIECE_PART_BODY;
        beats += section->beats;
    }
    size_t per_bar = (size_t)structure->meter.beats_per_bar;
    if (!body || (beats + per_bar - 1) / per_bar > PIECE_STRUCTURE_MAX_BARS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

void piece_structure_default_cadences(PieceStructure *structure) {
    if (!structure || !structure->sections) {
        return;
    }
    size_t count = structure->section_count;
    /* The last section of each part that leads on: the intro into the body, the loop back to its start. */
    size_t last_intro = count;
    size_t last_loop = count;
    size_t last_ending = count;
    for (size_t i = 0; i < count; i++) {
        PiecePart part = structure->sections[i].part;
        if (part == PIECE_PART_INTRO) {
            last_intro = i;
        } else if (part == PIECE_PART_BODY || part == PIECE_PART_TURNAROUND) {
            last_loop = i;
        } else {
            last_ending = i;
        }
    }
    for (size_t i = 0; i < count; i++) {
        PieceSection *section = &structure->sections[i];
        if (section->has_cadence) {
            continue;
        }
        section->has_cadence = true;
        if (i == last_ending) {
            section->cadence = PLAN_CADENCE_PAC;
        } else if (i == last_loop || i == last_intro) {
            section->cadence = PLAN_CADENCE_HC;
        } else {
            section->cadence = PLAN_CADENCE_IAC;
        }
    }
}

bool piece_structure_default_ending(PieceStructure *structure, size_t capacity) {
    if (!structure || !structure->sections) {
        return false;
    }
    for (size_t i = 0; i < structure->section_count; i++) {
        if (structure->sections[i].part == PIECE_PART_ENDING) {
            return true;
        }
    }
    if (structure->section_count >= capacity || structure->meter.beats_per_bar <= 0) {
        return false;
    }
    PieceSection *section = &structure->sections[structure->section_count++];
    memset(section, 0, sizeof(*section));
    strcpy(section->id, "end");
    strcpy(section->letter, "E");
    section->part = PIECE_PART_ENDING;
    section->beats = DEFAULT_ENDING_BARS * (size_t)structure->meter.beats_per_bar;
    section->pickup = (PlanTime){0, 1};
    section->has_cadence = true;
    section->cadence = PLAN_CADENCE_PAC;
    section->fill = PIECE_FILL_EMPTY;
    return true;
}

/* ------------------------------------------------------------- the plan */

/* The weak cadence an antecedent ends in. */
#define ANTECEDENT_CADENCE PLAN_CADENCE_HC
/* The bars of material a section holds beside the bars of its own: its motive and the four its ideas are made from. */
#define MATERIAL_BARS_PER_SECTION 5

static PieceResult from_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

static PieceResult add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint) {
    return from_plan(plan_node_add_constraint(plan, id, constraint));
}

/* A length of `count` beats of the meter. */
static PlanTime beats_of(const PlanMeter *meter, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make((long)count, (unsigned long)meter->beat_unit, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The pairing a theme section is built from, by its archetype; false for a hybrid that is not one. */
static bool theme_pairing(const PieceSection *section, PlanThemePairing *out) {
    PlanArchetype into = section->has_into ? section->into : PLAN_ARCHETYPE_SENTENCE;
    switch (into) {
    case PLAN_ARCHETYPE_SENTENCE:
        out->beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        out->ending = PLAN_THEME_ENDING_CONTINUATION;
        return true;
    case PLAN_ARCHETYPE_PERIOD:
        out->beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
        out->ending = PLAN_THEME_ENDING_CONSEQUENT;
        return true;
    case PLAN_ARCHETYPE_HYBRID:
        *out = section->hybrid;
        return grow_pairing_archetype(out) == (int)PLAN_ARCHETYPE_HYBRID;
    }
    return false;
}

/*
 * Whether a motif or empty section is a theme section: 4 or 8 whole bars on a barline with no pickup, and a theme
 * grow-theme can build in the meter with the cadence asked for.
 */
static bool is_theme_section(const PieceStructure *structure, const PieceSection *section, size_t start_beats,
                             PlanThemePairing *pairing) {
    size_t per_bar = (size_t)structure->meter.beats_per_bar;
    if (section->pickup.num != 0 || start_beats % per_bar != 0 || section->beats % per_bar != 0) {
        return false;
    }
    size_t bars = section->beats / per_bar;
    return (bars == 4 || bars == 8) && theme_pairing(section, pairing) &&
           grow_theme_buildable(&structure->meter, bars, pairing, section->cadence, ANTECEDENT_CADENCE);
}

/* The key over the start of a span, when one key span holds it. */
static bool key_over(const PiecePlan *plan, PlanSpan span, PlanKey *out) {
    LaneSpanId ids[2];
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, ids, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, ids[0]), span)) {
        return false;
    }
    *out = plan_lane_span_payload(plan, ids[0])->key;
    return true;
}

/*
 * The m0 of the theme plan of a theme fill, in the structure's key: the pitches move by the key difference, as the
 * theme's lanes do (plan_give_section). False when the plan has no theme section, no m0, or is in another mode.
 */
static bool theme_motive(const PieceStructure *structure, const PieceSection *section, Motive *out) {
    const PiecePlan *theme_plan = section->theme_plan;
    NodeId theme = theme_plan ? grow_find_theme(theme_plan, false) : PLAN_NODE_NONE;
    NodeId source = theme_plan ? grow_find_source(theme_plan) : PLAN_NODE_NONE;
    PlanSpan span;
    PlanMeter meter;
    PlanKey key;
    if (theme == PLAN_NODE_NONE || source == PLAN_NODE_NONE) {
        return false;
    }
    span = plan_node_span(theme_plan, theme);
    if (!return_meter_over(theme_plan, span, &meter) || !key_over(theme_plan, span, &key) ||
        key.mode != structure->key.mode || !grow_read_motive(theme_plan, source, &meter, &key, out)) {
        return false;
    }
    int shift = ((structure->key.tonic_pitch_class - key.tonic_pitch_class) % 12 + 12) % 12;
    shift = shift > 6 ? shift - 12 : shift;
    for (size_t i = 0; i < out->note_count; i++) {
        out->notes[i].midi_pitch += shift;
        if (out->notes[i].midi_pitch < 0 || out->notes[i].midi_pitch > 127) {
            return false;
        }
    }
    out->key = structure->key;
    return true;
}

/* The basic motive (R-170): the structure's, else the first motif section's or theme fill's, else NULL to have one invented. */
static const Motive *basic_motive(const PieceStructure *structure, Motive *storage) {
    if (structure->motive) {
        return structure->motive;
    }
    for (size_t i = 0; i < structure->section_count; i++) {
        const PieceSection *section = &structure->sections[i];
        if (section->fill == PIECE_FILL_MOTIF && section->motive) {
            return section->motive;
        }
        if (section->fill == PIECE_FILL_THEME && theme_motive(structure, section, storage)) {
            return storage;
        }
    }
    return NULL;
}

/* The constraints a theme section has from the theme it is given: its archetype, pairing and cadences. */
static PieceResult add_theme_constraints(PiecePlan *plan, NodeId id, const PiecePlan *theme_plan, NodeId theme) {
    PieceResult result = PIECE_OK;
    for (size_t i = 0; result == PIECE_OK && i < plan_node_constraint_count(theme_plan, theme); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(theme_plan, theme, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE || constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING ||
            constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            result = add_constraint(plan, id, constraint);
        }
    }
    return result;
}

/*
 * Whether the section is held for the seam stage (R-171): the loop's end, unless it is Given (a theme fill, or a
 * progression that covers it) or holds the body's first bar, which the seam copies before anything can be filled.
 */
static bool is_held(const PieceStructure *structure, size_t index, size_t cells) {
    const PieceSection *section = &structure->sections[index];
    size_t loop_end = structure->section_count;
    size_t first_body = structure->section_count;
    for (size_t i = 0; i < structure->section_count; i++) {
        if (structure->sections[i].part == PIECE_PART_BODY && first_body == structure->section_count) {
            first_body = i;
        }
        if (structure->sections[i].part == PIECE_PART_BODY || structure->sections[i].part == PIECE_PART_TURNAROUND) {
            loop_end = i;
        }
    }
    if (index != loop_end || index == first_body || section->fill == PIECE_FILL_THEME) {
        return false;
    }
    if (section->fill == PIECE_FILL_PROGRESSION) {
        size_t per_chord = section->cells_per_chord ? section->cells_per_chord : cells;
        return section->numeral_count * per_chord * (size_t)structure->meter.beats_per_bar < section->beats * cells;
    }
    return true;
}

static PieceResult add_section(PiecePlan *plan, const PieceStructure *structure, const PieceSection *section,
                               size_t start_beats, NodeId form, PlanSpan span, NodeId *out) {
    PlanConstraint constraint;
    PlanThemePairing pairing;
    /* A theme fill is the theme it was composed as: its archetype, pairing and cadences, not the structure's. */
    const PiecePlan *theme_plan = section->fill == PIECE_FILL_THEME ? section->theme_plan : NULL;
    NodeId given = theme_plan ? grow_find_theme(theme_plan, false) : PLAN_NODE_NONE;
    if (section->fill == PIECE_FILL_THEME && given == PLAN_NODE_NONE) {
        return PIECE_ERR_BAD_REQUEST;
    }
    bool theme = given == PLAN_NODE_NONE && is_theme_section(structure, section, start_beats, &pairing);
    PieceResult result = from_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, span, section->id, out));
    if (result == PIECE_OK && given != PLAN_NODE_NONE) {
        result = add_theme_constraints(plan, *out, theme_plan, given);
    } else if (result == PIECE_OK && theme) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)grow_pairing_archetype(&pairing);
        result = add_constraint(plan, *out, &constraint);
        if (result == PIECE_OK) {
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
            constraint.params.theme_pairing = pairing;
            result = add_constraint(plan, *out, &constraint);
        }
    } else if (result == PIECE_OK) {
        /* Any other length is a free phrase, which grow-theme lays out in ideas of a bar (grow_free.c). */
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_FREE_PHRASE;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && given == PLAN_NODE_NONE) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = section->cadence;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && theme && pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = ANTECEDENT_CADENCE;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = structure->key;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && structure->has_accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = structure->accompaniment;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SECTION_LETTER;
        snprintf(constraint.params.letter, sizeof(constraint.params.letter), "%s", section->letter);
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_PIECE_PART;
        constraint.params.piece_part.part = (int)section->part;
        snprintf(constraint.params.piece_part.id, sizeof(constraint.params.piece_part.id), "%s", section->id);
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->pickup.num != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_PICKUP;
        constraint.params.pickup = section->pickup;
        result = add_constraint(plan, *out, &constraint);
    }
    return result;
}

/* A constraint with no parameters on a node. */
static PieceResult add_flag(PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = kind;
    return add_constraint(plan, id, &constraint);
}

PieceResult piece_structure_build_plan(const PieceStructure *structure, PiecePlan **out) {
    if (!structure || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    PieceResult result = piece_structure_check(structure);
    if (result != PIECE_OK) {
        return result;
    }
    size_t cells;
    PlanTime bar = grow_bar_length(&structure->meter);
    Motive theme_storage;
    const Motive *motive = basic_motive(structure, &theme_storage);
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS];
    PieceStructure copy = *structure;
    memcpy(sections, structure->sections, structure->section_count * sizeof(PieceSection));
    copy.sections = sections;
    piece_structure_default_cadences(&copy);
    if ((motive && motive->note_count < 2) ||
        (structure->has_accompaniment && !grow_accompaniment_valid(&structure->meter, &structure->accompaniment)) ||
        !grow_cells_per_bar(&structure->meter, &cells)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (motive && plan_time_compare(motive->length, bar) > 0) {
        return PIECE_ERR_MOTIVE_TOO_LONG;
    }
    /* The sections laid out are the intro, the body and the turnaround; the ending version lays out the ending. */
    size_t laid = 0;
    size_t beats = 0;
    size_t material_bars = 1;
    size_t per_bar = (size_t)structure->meter.beats_per_bar;
    for (size_t i = 0; i < copy.section_count; i++) {
        if (copy.sections[i].part != PIECE_PART_ENDING) {
            laid = i + 1;
            beats += copy.sections[i].beats;
            material_bars += MATERIAL_BARS_PER_SECTION + (copy.sections[i].beats + per_bar - 1) / per_bar;
        }
    }
    PiecePlan *plan = plan_create();
    if (!plan) {
        return PIECE_ERR_ALLOC;
    }
    /* The seam is a bar after the loop's end; the material starts on the first barline at or after the seam's end: its
       motives are bars of the meter. */
    beats += per_bar;
    PlanSpan material = {{0, 1}, {0, 1}};
    if (plan_time_make(bar.num * (long)((beats + per_bar - 1) / per_bar), bar.den, &material.start) != PLAN_OK) {
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanSpan form = {{0, 1}, {0, 1}};
    NodeId form_id = PLAN_NODE_NONE;
    NodeId source = PLAN_NODE_NONE;
    PlanLanePayload payload;
    LaneSpanId lane;
    PlanTime bars_length = {0, 1};
    if (plan_time_make(bar.num * (long)material_bars, bar.den, &bars_length) != PLAN_OK) {
        plan_destroy(plan);
        return PIECE_ERR_BAD_REQUEST;
    }
    material.length = bars_length;
    result = from_plan(plan_time_add(material.start, material.length, &form.length));
    if (result == PIECE_OK) {
        result = from_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "structure", &form_id));
    }
    /* The meter and the key are Given over the sections and the material: the piece is in its home key throughout. */
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = structure->meter;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, form, &payload, &lane));
        result = result == PIECE_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.key = structure->key;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_KEY, 0, form, &payload, &lane));
        result = result == PIECE_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    /* The sections in time order, each from the end of the one before. */
    PlanTime start = {0, 1};
    size_t start_beats = 0;
    for (size_t i = 0; result == PIECE_OK && i < laid; i++) {
        NodeId id;
        PlanSpan span = {start, beats_of(&structure->meter, copy.sections[i].beats)};
        result = add_section(plan, &copy, &copy.sections[i], start_beats, form_id, span, &id);
        if (result == PIECE_OK && copy.sections[i].fill == PIECE_FILL_THEME &&
            plan_give_section(plan, id, copy.sections[i].theme_plan, grow_find_theme(copy.sections[i].theme_plan, false)) !=
                GIVEN_OK) {
            result = PIECE_ERR_BAD_REQUEST;
        }
        /* A progression fill: the chords Given from the section's start, a bar each unless it says; the harmony stage
           fills what is left of the section. */
        if (result == PIECE_OK && copy.sections[i].fill == PIECE_FILL_PROGRESSION) {
            const PieceSection *given = &copy.sections[i];
            size_t per_chord = given->cells_per_chord ? given->cells_per_chord : cells;
            if (plan_give_progression(plan, id, 0, given->numerals, given->numeral_count, per_chord) != GIVEN_OK) {
                result = PIECE_ERR_BAD_REQUEST;
            }
        }
        if (result == PIECE_OK && is_held(&copy, i, cells)) {
            result = add_flag(plan, id, PLAN_CONSTRAINT_HOLD);
        }
        if (result == PIECE_OK) {
            result = from_plan(plan_time_add(start, span.length, &start));
        }
        start_beats += copy.sections[i].beats;
    }
    /* The seam: the bar the loop leads into, open and empty until the seam stage copies the body's first bar into it. */
    if (result == PIECE_OK) {
        NodeId seam;
        PlanSpan span = {start, bar};
        result = from_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, span, "seam", &seam));
        result = result == PIECE_OK ? add_flag(plan, seam, PLAN_CONSTRAINT_SEAM) : result;
        if (result == PIECE_OK) {
            PlanConstraint key;
            memset(&key, 0, sizeof(key));
            key.kind = PLAN_CONSTRAINT_KEY;
            key.params.key = structure->key;
            result = add_constraint(plan, seam, &key);
        }
    }
    if (result == PIECE_OK) {
        result = piece_add_material(plan, motive, NULL, form_id, material, bar, &source);
    }
    if (result == PIECE_OK && structure->has_style) {
        PlanConstraint style;
        memset(&style, 0, sizeof(style));
        style.kind = PLAN_CONSTRAINT_STYLE;
        style.params.style = structure->style;
        result = add_constraint(plan, form_id, &style);
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }
    if (result != PIECE_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PIECE_OK;
}
