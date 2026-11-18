#include <ursatz_composer/piece.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "piece/piece_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

/* The placeholder note an open melody span holds until motive-invent writes it. */
#define INVENT_PLACEHOLDER 60

const char *piece_result_name(PieceResult result) {
    switch (result) {
    case PIECE_OK:
        return "ok";
    case PIECE_ERR_NULL:
        return "null argument";
    case PIECE_ERR_ALLOC:
        return "allocation failed";
    case PIECE_ERR_MOTIVE_TOO_LONG:
        return "motive too long";
    case PIECE_ERR_BAD_REQUEST:
        return "bad request";
    case PIECE_ERR_STUCK:
        return "stuck";
    case PIECE_ERR_BAD_SIZE:
        return "bad size";
    }
    return "unknown";
}

size_t piece_material_bars_of(size_t section_count, size_t motive_bars) {
    return motive_bars + MATERIAL_BARS_PER_SECTION * motive_bars * section_count;
}

static Rational bars_of(Rational bar, size_t count) {
    Rational out = {0, 1};
    if (rational_mul_int(bar, (long long)count, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

const FormSpecSize *form_spec_default_size(const FormSpec *form) {
    for (size_t i = 0; form && i < form->size_count; i++) {
        if (form->sizes[i].is_default) {
            return &form->sizes[i];
        }
    }
    return NULL;
}

const FormSpecSize *form_spec_size(const FormSpec *form, size_t bars, size_t count) {
    if (!form || form->request_sections) {
        return NULL;
    }
    if (bars == 0 && count == 0) {
        return form_spec_default_size(form);
    }
    for (size_t i = 0; i < form->size_count; i++) {
        const FormSpecSize *size = &form->sizes[i];
        /* A count picks the size with it, and bars then must match; bars alone pick the first size with them. */
        if (count != 0 ? size->count == count && (bars == 0 || size->bars == bars) : size->bars == bars) {
            return size;
        }
    }
    return NULL;
}

/*
 * The pattern a setting return is written in (R-518): the form's, unless it is the piece's own accompaniment or alberti in a
 * compound meter; then the next of block, arpeggiated, alberti and oom-pah after it, wrapping, skipping those two.
 */
static PlanAccompanimentPattern setting_pattern(const SectionDescription *section, const PieceRequest *request,
                                                const PlanAccompaniment *accompaniment) {
    PlanAccompanimentPattern pattern = (PlanAccompanimentPattern)section->setting;
    for (int tried = 0; tried < 4; tried++) {
        PlanAccompaniment candidate;
        memset(&candidate, 0, sizeof(candidate));
        candidate.pattern = pattern;
        candidate.texture = -1;
        if ((!accompaniment || accompaniment->pattern != pattern) && grow_accompaniment_valid(&request->meter, &candidate)) {
            return pattern;
        }
        pattern = pattern == PLAN_ACCOMPANIMENT_OOM_PAH ? PLAN_ACCOMPANIMENT_BLOCK : (PlanAccompanimentPattern)(pattern + 1);
    }
    return (PlanAccompanimentPattern)section->setting;
}

/* The accompaniment the sections ask for: the request's, or for a form that requires one, any pattern when it has none. */
static bool piece_accompaniment(const FormSpec *form, const PieceRequest *request, PlanAccompaniment *out) {
    if (request->has_accompaniment) {
        *out = request->accompaniment;
        return true;
    }
    if (form->accompaniment_required) {
        memset(out, 0, sizeof(*out));
        out->pattern = PLAN_ACCOMPANIMENT_ANY;
        out->texture = -1;
        return true;
    }
    return false;
}

/* The offset of the request's seed that draws which sections get a "sometimes" connective (R-486); the keys' stream is untouched. */
#define FORM_CONNECTIVE_SEED_OFFSET 7UL

/*
 * One section as grow-theme builds it, with its letter, its return and its layers; its motive comes after m0. A section
 * that leads in is held (R-486), and a connective is asked for when `connective` (a "sometimes" is drawn by the caller).
 */
static PieceResult add_section(PiecePlan *plan, const PieceRequest *request, const FormSpecSize *size,
                               const SectionDescription *section, const SectionConstruction *construction, PlanSpan span, NodeId form, const NodeId *sections,
                               const PlanAccompaniment *accompaniment, bool connective, NodeId *out) {
    PlanConstraint constraint;
    /*
     * A liquidation (R-494), a retransition (R-505) and a coda (R-509) are no theme: an operator section has no archetype,
     * pairing or antecedent cadence. A liquidation has the liquidation constraint in their place; a transition, retransition,
     * subordinate theme and coda have the section function constraint (R-503).
     */
    const bool liquidation = section->role == SECTION_ROLE_LIQUIDATION;
    const bool operator_section =
        liquidation || section->role == SECTION_ROLE_RETRANSITION || section->role == SECTION_ROLE_CODA;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, span, section->name, out));
    if (result == PIECE_OK && liquidation) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_LIQUIDATION;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && !operator_section) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)grow_pairing_archetype(&section->pairing);
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && !operator_section) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
        constraint.params.theme_pairing = section->pairing;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = section->cadence;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && !operator_section && section->pairing.beginning == SECTION_BEGINNING_ANTECEDENT) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = ANTECEDENT_CADENCE;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = *accompaniment;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SECTION_LETTER;
        memcpy(constraint.params.letter, section->letter, sizeof(constraint.params.letter));
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->role >= SECTION_ROLE_TRANSITION && section->role <= SECTION_ROLE_CODA) {
        static const SectionFunctionKind FUNCTIONS[] = {SECTION_FUNCTION_TRANSITION, SECTION_FUNCTION_RETRANSITION,
                                                     SECTION_FUNCTION_SUBORDINATE, SECTION_FUNCTION_CODA};
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SECTION_FUNCTION;
        constraint.params.section_function.function = FUNCTIONS[section->role - SECTION_ROLE_TRANSITION];
        constraint.params.section_function.independent = section->function.independent;
        constraint.params.section_function.lyric = section->function.lyric;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->returns[0] != '\0') {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_RETURN;
        constraint.params.section_return.section = sections[form_spec_section_index(size, section->returns)];
        constraint.params.section_return.variation = section->variation;
        constraint.params.section_return.across_modes = section->mode == SECTION_MODE_HOME;
        if (section->variation == SECTION_RETURN_SETTING) {
            constraint.params.section_return.setting = setting_pattern(section, request, accompaniment);
        }
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->has_figure) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_FIGURE;
        constraint.params.figure = section->figure;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->layers != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_LAYERS;
        constraint.params.layers.lanes = section->layers;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->pedal != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_PEDAL;
        constraint.params.pedal.degree = section->pedal;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && construction) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CONSTRUCTION;
        constraint.params.construction = *construction;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->leads_in) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_HOLD;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->leads_in && connective) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CONNECTIVE;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    return result;
}

/* The motive a section is made with: its own, or a returning section's source's (the source's, in turn). */
static bool section_motive(const FormSpecSize *size, size_t index, MotiveRelationStep *step) {
    const SectionDescription *section = &size->sections[index];
    if (form_spec_section_motive(section, step)) {
        return true;
    }
    size_t source = form_spec_section_index(size, section->returns);
    return source != FORM_SPEC_NO_RETURN && section_motive(size, source, step);
}

/* A section whose motive is not m0 itself asks for it as a MOTIVE_REF to m0. */
static PieceResult add_section_motive(PiecePlan *plan, const FormSpecSize *size, size_t index, NodeId id, NodeId source) {
    PlanConstraint constraint;
    MotiveRelationStep step;
    if (!section_motive(size, index, &step)) {
        return PIECE_OK;
    }
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive.motive = source;
    constraint.params.motive.has_step = true;
    constraint.params.motive.step = step;
    return piece_add_constraint(plan, id, &constraint);
}

/* m0 with no motive given: a Constrained one-bar motive node whose melody span motive-invent writes. */
static PieceResult add_open_motive(PiecePlan *plan, NodeId idea, PlanSpan span) {
    NodeId node;
    LaneSpanId melody;
    PlanLanePayload payload;
    PlanNote note;
    PlanConstraint constraint;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, span, GROW_SOURCE_NAME, &node));
    memset(&payload, 0, sizeof(payload));
    memset(&note, 0, sizeof(note));
    note.midi = INVENT_PLACEHOLDER;
    note.spelled = false;
    note.offset = (Rational){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &melody));
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_INVENT_MOTIVE;
        result = piece_result_of_plan(plan_lane_span_add_constraint(plan, melody, &constraint));
    }
    return result;
}

/*
 * An independent transition's motive (R-504): an open motive node of its own in the material's idea, at `bar_index` bars
 * from the material's start, named for the section, with the invent stage's melody span as m0's has.
 */
static PieceResult add_independent_motive(PiecePlan *plan, NodeId idea, PlanSpan material, Rational bar, size_t bar_index,
                                          const SectionDescription *section, NodeId *out) {
    char name[64];
    PlanSpan span = {{0, 1}, bar};
    NodeId node = PLAN_NODE_NONE;
    LaneSpanId melody;
    PlanLanePayload payload;
    PlanNote note;
    PlanConstraint constraint;
    snprintf(name, sizeof(name), "independent %s", section->name);
    PieceResult result = piece_result_of_plan(plan_result_of_rational(rational_add(material.start, bars_of(bar, bar_index), &span.start)));
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, span, name, &node));
    }
    memset(&payload, 0, sizeof(payload));
    memset(&note, 0, sizeof(note));
    note.midi = INVENT_PLACEHOLDER;
    note.spelled = false;
    note.offset = (Rational){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &melody));
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_INVENT_MOTIVE;
        result = piece_result_of_plan(plan_lane_span_add_constraint(plan, melody, &constraint));
    }
    *out = node;
    return result;
}

/* An independent transition asks for its own motive as a MOTIVE_REF with no step: the motive itself, not m0 made over. */
static PieceResult add_independent_ref(PiecePlan *plan, NodeId id, NodeId motive) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive.motive = motive;
    constraint.params.motive.has_step = false;
    return piece_add_constraint(plan, id, &constraint);
}

/* The material section, its phrase and its idea, Realized, with m0 in it; the source node in *out. */
PieceResult piece_add_material(PiecePlan *plan, const Motive *motive, const PlanSource *motive_source, NodeId form,
                               PlanSpan material, Rational bar, NodeId *out) {
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanConstraint constraint;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, material, GROW_MATERIAL_NAME, &section));
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MATERIAL;
        result = piece_add_constraint(plan, section, &constraint);
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, GROW_MATERIAL_NAME, &phrase));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, GROW_MATERIAL_NAME, &idea));
    }
    if (result == PIECE_OK && motive) {
        result = piece_result_of_plan(motive_attach_sourced(plan, idea, material.start, motive, GROW_SOURCE_NAME, motive_source,
                                                 out, NULL));
    } else if (result == PIECE_OK) {
        PlanSpan span = {material.start, bar};
        result = add_open_motive(plan, idea, span);
        *out = result == PIECE_OK ? grow_find_source(plan) : PLAN_NODE_NONE;
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, section, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED));
    }
    return result;
}

/*
 * Every section of the size must be one grow-theme can build in the meter: a theme-like role of whole bars, 4 or 8, or
 * 3 to 16 with a construction (R-468). The construction of each section that has one goes into constructions[i], given,
 * or for "auto" chosen from the section's bars and the motive (R-469, an open motive counts as one bar); the others'
 * are zeroed, with has[i] false for them.
 */
static bool size_buildable(const FormSpecSize *size, const PlanMeter *meter, const Motive *motive,
                           SectionConstruction *constructions, bool *has) {
    for (size_t i = 0; i < size->section_count; i++) {
        const SectionDescription *section = &size->sections[i];
        memset(&constructions[i], 0, sizeof(constructions[i]));
        has[i] = section->has_construction;
        if (section->beats != 0 || !section_role_is_theme(section->role)) {
            return false;
        }
        if (section->role == SECTION_ROLE_LIQUIDATION || section->role == SECTION_ROLE_RETRANSITION) {
            /* The operator lays out its own 4 bars (the reader refuses any other); it has no theme to be buildable. */
            if (section->bars != 4) {
                return false;
            }
        } else if (section->role == SECTION_ROLE_CODA) {
            /* The coda operator lays out its own 8 bars (the reader refuses any other). */
            if (section->bars != 8) {
                return false;
            }
        } else if (!section->has_construction) {
            if (!grow_theme_buildable(meter, section->bars, &section->pairing, section->cadence, ANTECEDENT_CADENCE)) {
                return false;
            }
        } else if (section->construction_auto) {
            if (!grow_construction_choose(motive, meter, section->bars, &section->pairing, section->cadence, &constructions[i])) {
                return false;
            }
        } else {
            constructions[i] = section->construction;
            if (!grow_construction_buildable(meter, section->bars, &section->pairing, section->cadence, ANTECEDENT_CADENCE,
                                             &constructions[i])) {
                return false;
            }
        }
    }
    return true;
}

/* The bars of a section's basic idea: its construction's, else 2 of an 8-bar theme and 1 of any other. */
static size_t section_basic_bars(const SectionDescription *section, const SectionConstruction *construction, bool has) {
    return has ? construction->idea_bars[GROW_IDEA_BASIC] : (section->bars == 8 ? 2 : 1);
}

bool piece_basic_idea(const PieceRequest *request, size_t *theme_bars, size_t *idea_bars) {
    FormSpec *form = NULL;
    if (!request || form_spec_builtin(request->form, &form) != FORM_SPEC_OK) {
        return false;
    }
    const FormSpecSize *size = form_spec_size(form, request->bars, request->count);
    size_t shortest = 8;
    size_t basic = 2;
    bool found = size != NULL;
    for (size_t i = 0; size && i < size->section_count; i++) {
        const SectionDescription *section = &size->sections[i];
        /* A section with a construction is its own bars long and its construction gives its basic idea; "auto" is
           judged by its length, as grow_construction_choose does when the motif is short (R-469). */
        size_t bars = section->has_construction ? section->bars : (section->bars == 8 ? 8 : 4);
        size_t idea = !section->has_construction ? (section->bars == 8 ? 2 : 1)
                      : section->construction_auto ? (section->bars >= 6 ? 2 : 1)
                                                   : section->construction.idea_bars[GROW_IDEA_BASIC];
        shortest = bars < shortest ? bars : shortest;
        basic = idea < basic ? idea : basic;
    }
    /* A form whose sections the request gives: the shortest of them, 8 bars or else 4. */
    if (form->request_sections) {
        size_t per_bar = request->meter.beats_per_bar > 0 ? (size_t)request->meter.beats_per_bar : 1;
        found = true;
        shortest = 8;
        basic = 2;
        for (size_t i = 0; i < request->section_count; i++) {
            const PieceSection *section = &request->sections[i];
            size_t bars = section->has_construction ? section->beats / per_bar : (section->beats == 8 * per_bar ? 8 : 4);
            size_t idea = !section->has_construction ? (section->beats == 8 * per_bar ? 2 : 1)
                          : section->construction_auto ? (bars >= 6 ? 2 : 1)
                                                       : section->construction.idea_bars[GROW_IDEA_BASIC];
            shortest = bars < shortest ? bars : shortest;
            basic = idea < basic ? idea : basic;
        }
    }
    form_spec_destroy(form);
    if (!found) {
        return false;
    }
    *theme_bars = shortest;
    *idea_bars = basic;
    return true;
}

PieceResult form_build_plan(const FormSpec *form, const PieceRequest *request, PiecePlan **out) {
    if (!form || !request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    if (form->request_sections) {
        return form_build_request_plan(form, request, NULL, out);
    }
    const FormSpecSize *size = form_spec_size(form, request->bars, request->count);
    if (!size) {
        return PIECE_ERR_BAD_SIZE;
    }
    PlanAccompaniment accompaniment;
    bool accompanied = piece_accompaniment(form, request, &accompaniment);
    size_t cells_per_bar;
    SectionConstruction constructions[FORM_SPEC_MAX_SECTIONS];
    bool has_construction[FORM_SPEC_MAX_SECTIONS];
    if (!form_spec_meter_allowed(form, &request->meter) || !grow_cells_per_bar(&request->meter, &cells_per_bar) ||
        analysis_key_tonic_pitch_class(&request->key) < 0 ||
        analysis_key_tonic_pitch_class(&request->key) > 11 ||
        !composer_key_valid(&request->key) ||
        (request->motive && request->motive->note_count < 2) ||
        (accompanied && !grow_accompaniment_valid(&request->meter, &accompaniment)) ||
        !size_buildable(size, &request->meter, request->motive, constructions, has_construction)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    Rational bar = grow_bar_length(&request->meter);
    /* The motive fills the basic idea of every section: its construction's, else 2 bars of an 8-bar theme section, 1 of a 4-bar one. */
    size_t basic_bars = 2;
    size_t extension_bars = 0;
    for (size_t i = 0; i < size->section_count; i++) {
        size_t section_bars = section_basic_bars(&size->sections[i], &constructions[i], has_construction[i]);
        basic_bars = section_bars < basic_bars ? section_bars : basic_bars;
        /* The repeats and the addition of a construction have motives of their own in the material (R-468). */
        extension_bars += has_construction[i] ? grow_extension_material_bars(&size->sections[i].pairing, &constructions[i]) : 0;
    }
    if (request->motive && !grow_motive_fits_idea(request->motive, bar, basic_bars)) {
        return PIECE_ERR_MOTIVE_TOO_LONG;
    }
    size_t motive_bars = request->motive ? grow_motive_layout_bars(request->motive, bar) : 1;
    /* An independent transition has a motive of its own, one bar, in the material (R-504). */
    size_t independent_count = 0;
    for (size_t i = 0; i < size->section_count; i++) {
        independent_count += size->sections[i].function.independent ? 1 : 0;
    }

    PiecePlan *plan = plan_create();
    if (!plan) {
        return PIECE_ERR_ALLOC;
    }
    PlanSpan piece = {{0, 1}, bars_of(bar, size->bars)};
    PlanSpan material = {piece.length, bars_of(bar, piece_material_bars_of(size->section_count, motive_bars) + extension_bars + request->given_melodies + independent_count)};
    PlanSpan extent = {{0, 1}, {0, 1}};
    PieceResult result = piece_result_of_plan(plan_result_of_rational(rational_add(piece.length, material.length, &extent.length)));
    NodeId form_id = PLAN_NODE_NONE;
    NodeId source = PLAN_NODE_NONE;
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    PlanLanePayload payload;
    LaneSpanId lane;

    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, extent, form->plan_name, &form_id));
    }
    /* The meter is Given over the piece and the material; the key lane comes with the sections (piece_plan_keys). */
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = piece_result_of_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, extent, &payload, &lane));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    /* The sections in order, then the material; the sections' motive references name m0, so they come last. */
    Rational start = {0, 1};
    FillRng connective_rng;
    fill_rng_seed(&connective_rng, request->seed + FORM_CONNECTIVE_SEED_OFFSET);
    for (size_t i = 0; result == PIECE_OK && i < size->section_count; i++) {
        PlanSpan span = {start, bars_of(bar, size->sections[i].bars)};
        int wants = size->sections[i].leads_in ? (int)size->sections[i].connective : SECTION_CONNECTIVE_NONE;
        bool connective = wants == SECTION_CONNECTIVE_ALWAYS ||
                          (wants == SECTION_CONNECTIVE_SOMETIMES && fill_rng_below(&connective_rng, 2) == 0);
        result = add_section(plan, request, size, &size->sections[i], has_construction[i] ? &constructions[i] : NULL, span, form_id,
                             sections, accompanied ? &accompaniment : NULL, connective, &sections[i]);
        if (result == PIECE_OK) {
            result = piece_result_of_plan(plan_result_of_rational(rational_add(start, span.length, &start)));
        }
    }
    if (result == PIECE_OK) {
        result = piece_add_material(plan, request->motive, request->source, form_id, material, bar, &source);
    }
    /* Each independent transition's motive, at the first bars after m0's layout and the extensions; its section names it. */
    NodeId independent[FORM_SPEC_MAX_SECTIONS];
    size_t independent_at = motive_bars + extension_bars;
    for (size_t i = 0; i < size->section_count; i++) {
        independent[i] = PLAN_NODE_NONE;
        if (result == PIECE_OK && size->sections[i].function.independent) {
            result = add_independent_motive(plan, plan_node_parent(plan, source), material, bar, independent_at++,
                                            &size->sections[i], &independent[i]);
        }
    }
    for (size_t i = 0; result == PIECE_OK && i < size->section_count; i++) {
        result = independent[i] != PLAN_NODE_NONE ? add_independent_ref(plan, sections[i], independent[i])
                                                  : add_section_motive(plan, size, i, sections[i], source);
    }
    if (result == PIECE_OK && request->has_style) {
        PlanConstraint style;
        memset(&style, 0, sizeof(style));
        style.kind = PLAN_CONSTRAINT_STYLE;
        style.params.style = request->style;
        result = piece_result_of_plan(plan_node_add_constraint(plan, form_id, &style));
    }
    /* The keys and the cadences across them are planned from the request's seed, home only unless it asks to modulate
   or the form always does; then the form is Given. */
    if (result == PIECE_OK) {
        PieceKeyPlan keys;
        FillRng rng;
        bool modulate = request->modulate || form->modulates;
        fill_rng_seed(&rng, request->seed);
        result = piece_plan_keys(plan, request->knowledge, form, size, modulate ? &rng : NULL, &keys);
        result = result == PIECE_OK && modulate ? piece_plan_cadences(plan, form, size, &keys) : result;
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }
    if (result != PIECE_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PIECE_OK;
}

PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out) {
    if (!request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    FormSpec *form = NULL;
    FormSpecResult found = request->form ? form_spec_builtin(request->form, &form) : FORM_SPEC_ERR_UNKNOWN;
    if (found != FORM_SPEC_OK) {
        return found == FORM_SPEC_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST;
    }
    PieceResult result = form_build_plan(form, request, out);
    form_spec_destroy(form);
    return result;
}
