#include <ursatz_composer/piece.h>

#include <stdio.h>
#include <string.h>

#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "piece/form_return_internal.h"
#include "piece/piece_internal.h"
#include "motive/motive_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

/*
 * The plans of a form whose sections the request gives (the game loop, R-419): the loop version and the ending version
 * (R-172), the request's checks, and the cadences and the ending its form document defaults. The parts are the form's
 * roles counted from the intro; the loop, and the role the ending version leaves out, come from the document.
 */

_Static_assert(SECTION_ROLE_BODY - SECTION_ROLE_INTRO == PIECE_PART_BODY && SECTION_ROLE_TURNAROUND - SECTION_ROLE_INTRO == PIECE_PART_TURNAROUND &&
                   SECTION_ROLE_ENDING - SECTION_ROLE_INTRO == PIECE_PART_ENDING,
               "a part is its role counted from the intro");

static const char *const PART_NAMES[PIECE_PART_COUNT] = {"intro", "body", "turnaround", "ending"};
static const char *const FILL_NAMES[] = {"empty", "motif", "theme", "progression"};

const char *piece_part_name(PiecePart part) {
    return part >= 0 && part < PIECE_PART_COUNT ? PART_NAMES[part] : "unknown";
}

const char *piece_function_name(PieceFunction function) {
    switch (function) {
    case PIECE_FUNCTION_NONE:
        return "none";
    case PIECE_FUNCTION_TRANSITION:
        return "transition";
    case PIECE_FUNCTION_CODA:
        return "coda";
    }
    return "unknown";
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

/* ------------------------------------------------------------- the form's roles */

static SectionRole role_of(PiecePart part) {
    return (SectionRole)(SECTION_ROLE_INTRO + (int)part);
}

/* Whether the part is in the loop: the body and the turnaround. */
static bool in_loop(const FormSpec *form, PiecePart part) {
    return form_spec_role_in_loop(form, role_of(part));
}

/* The part the loop starts with (the body), and the one it closes with (the turnaround) when it has more than one. */
static PiecePart first_loop_part(const FormSpec *form) {
    return (PiecePart)(form->loop_roles[0] - SECTION_ROLE_INTRO);
}

/* Whether the ending version leaves the part out: the loop's closing part, replaced by the ending. */
static bool is_dropped(const FormSpec *form, PiecePart part) {
    return form->loop_role_count > 1 && role_of(part) == form->loop_roles[form->loop_role_count - 1];
}

/* The form of the request: a built-in whose sections the request gives. */
static PieceResult request_form(const PieceRequest *request, FormSpec **form) {
    *form = NULL;
    FormSpecResult found = request->form ? form_spec_builtin(request->form, form) : FORM_SPEC_ERR_UNKNOWN;
    if (found != FORM_SPEC_OK) {
        return found == FORM_SPEC_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST;
    }
    if (!(*form)->request_sections) {
        form_spec_destroy(*form);
        *form = NULL;
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

/* ------------------------------------------------------------- the checks and the defaults */

/* Whether the pickup is on the meter's grid, not negative and shorter than a bar. */
static bool pickup_valid(PlanMeter meter, Rational pickup, Rational bar) {
    if (pickup.den == 0 || pickup.num < 0 || rational_cmp(pickup, bar) >= 0) {
        return false;
    }
    return plan_meter_pickup_on_grid(meter, pickup);
}

static PieceResult check(const FormSpec *form, const PieceRequest *request) {
    if (!request || (request->section_count > 0 && !request->sections)) {
        return PIECE_ERR_NULL;
    }
    size_t cells;
    if (!grow_cells_per_bar(&request->meter, &cells) || analysis_key_tonic_pitch_class(&request->key) < 0 ||
        analysis_key_tonic_pitch_class(&request->key) > 11 ||
        !composer_key_valid(&request->key) ||
        request->meter.beats_per_bar <= 0 || request->meter.beat_unit <= 0 ||
        request->section_count > PIECE_STRUCTURE_MAX_SECTIONS || (request->has_accompaniment && request->has_voicing)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    Rational bar = grow_bar_length(&request->meter);
    size_t beats = 0;
    bool body = false;
    int last_part = PIECE_PART_INTRO;
    for (size_t i = 0; i < request->section_count; i++) {
        const PieceSection *section = &request->sections[i];
        if (section->description.id[0] == '\0' || strnlen(section->description.id, PIECE_SECTION_ID_MAX) == PIECE_SECTION_ID_MAX ||
            piece_section_part(section) < PIECE_PART_INTRO || piece_section_part(section) > PIECE_PART_ENDING || (int)piece_section_part(section) < last_part ||
            !form_spec_has_role(form, role_of(piece_section_part(section))) || section->description.beats < 1 || !pickup_valid(request->meter, section->description.pickup, bar) ||
            section->fill < PIECE_FILL_EMPTY || section->fill > PIECE_FILL_PROGRESSION ||
            piece_section_function(section) < PIECE_FUNCTION_NONE || piece_section_function(section) > PIECE_FUNCTION_CODA ||
            /* A transition leads on to the next section: not the turnaround (it leads back) or the ending. A coda closes. */
            (piece_section_function(section) == PIECE_FUNCTION_TRANSITION &&
             (piece_section_part(section) == PIECE_PART_TURNAROUND || piece_section_part(section) == PIECE_PART_ENDING)) ||
            (piece_section_function(section) == PIECE_FUNCTION_CODA && piece_section_part(section) != PIECE_PART_ENDING)) {
            return PIECE_ERR_BAD_REQUEST;
        }
        for (size_t other = 0; other < i; other++) {
            if (strcmp(request->sections[other].description.id, section->description.id) == 0) {
                return PIECE_ERR_BAD_REQUEST;
            }
        }
        last_part = (int)piece_section_part(section);
        body = body || piece_section_part(section) == PIECE_PART_BODY;
        beats += section->description.beats;
    }
    size_t per_bar = (size_t)request->meter.beats_per_bar;
    if (!body || (beats + per_bar - 1) / per_bar > PIECE_STRUCTURE_MAX_BARS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

PieceResult piece_request_check(const PieceRequest *request) {
    if (!request) {
        return PIECE_ERR_NULL;
    }
    FormSpec *form;
    PieceResult result = request_form(request, &form);
    if (result != PIECE_OK) {
        return result;
    }
    result = check(form, request);
    form_spec_destroy(form);
    return result;
}

void piece_default_cadences(const FormSpec *form, PieceRequest *request) {
    if (!form || !request || !request->sections) {
        return;
    }
    size_t count = request->section_count;
    for (size_t i = 0; i < count; i++) {
        PieceSection *section = &request->sections[i];
        if (section->description.has_cadence) {
            continue;
        }
        section->description.has_cadence = true;
        section->description.cadence = form->default_cadence;
        if (piece_section_function(section) == PIECE_FUNCTION_TRANSITION) {
            section->description.cadence = CADENCE_TYPE_HALF;
            continue;
        }
        /* The first rule whose roles hold the last section of one of them. */
        for (size_t r = 0; r < form->last_cadence_count; r++) {
            const FormCadenceRule *rule = &form->last_cadences[r];
            size_t last = count;
            for (size_t k = 0; k < count; k++) {
                for (size_t role = 0; role < rule->role_count; role++) {
                    if (role_of(piece_section_part(&request->sections[k])) == rule->roles[role]) {
                        last = k;
                    }
                }
            }
            if (last == i) {
                section->description.cadence = rule->cadence;
                break;
            }
        }
    }
}

bool piece_default_ending(const FormSpec *form, PieceRequest *request, size_t capacity) {
    if (!form || !request || !request->sections) {
        return false;
    }
    for (size_t i = 0; i < request->section_count; i++) {
        if (piece_section_part(&request->sections[i]) == PIECE_PART_ENDING) {
            return true;
        }
    }
    if (!form->has_default_ending || request->section_count >= capacity || request->meter.beats_per_bar <= 0) {
        return false;
    }
    const FormDefaultEnding *ending = &form->default_ending;
    PieceSection *section = &request->sections[request->section_count++];
    piece_section_init(section);
    snprintf(section->description.id, sizeof(section->description.id), "%s", ending->id);
    snprintf(section->description.letter, sizeof(section->description.letter), "%s", ending->letter);
    piece_section_set_part(section, (PiecePart)(ending->role - SECTION_ROLE_INTRO));
    section->description.beats = ending->bars * (size_t)request->meter.beats_per_bar;
    section->description.pickup = (Rational){0, 1};
    section->description.has_cadence = true;
    section->description.cadence = ending->cadence;
    section->fill = PIECE_FILL_EMPTY;
    return true;
}

/* ------------------------------------------------------------- the plan */

/* A length of `count` beats of the meter. */
static Rational beats_of(const PlanMeter *meter, size_t count) {
    Rational out = {0, 1};
    if (rational_make((long)count, (long long)meter->beat_unit, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The pairing a theme section is built from, by its archetype; false for a hybrid that is not one. */
static bool theme_pairing(const PieceSection *section, SectionPairing *out) {
    PlanArchetype into = section->has_into ? section->into : PLAN_ARCHETYPE_SENTENCE;
    switch (into) {
    case PLAN_ARCHETYPE_SENTENCE:
        out->beginning = SECTION_BEGINNING_PRESENTATION;
        out->ending = SECTION_ENDING_CONTINUATION;
        return true;
    case PLAN_ARCHETYPE_PERIOD:
        out->beginning = SECTION_BEGINNING_ANTECEDENT;
        out->ending = SECTION_ENDING_CONSEQUENT;
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
static bool is_theme_section(const PieceRequest *request, const PieceSection *section, size_t start_beats,
                             SectionPairing *pairing) {
    size_t per_bar = (size_t)request->meter.beats_per_bar;
    if (section->description.pickup.num != 0 || start_beats % per_bar != 0 || section->description.beats % per_bar != 0) {
        return false;
    }
    size_t bars = section->description.beats / per_bar;
    return (bars == 4 || bars == 8) && theme_pairing(section, pairing) &&
           grow_theme_buildable(&request->meter, bars, pairing, section->description.cadence, ANTECEDENT_CADENCE);
}

/*
 * The construction of a motif section that has one (R-468): the section is a theme of its whole bars, 3 to 16, on a
 * barline with no pickup. Given, or for "auto" chosen from the bars and the piece's motive (R-469, NULL is a motive
 * still to be invented, of one bar). False when the section is none of that, or the construction cannot be built.
 */
static bool section_construction(const PieceRequest *request, const PieceSection *section, size_t start_beats,
                                 const Motive *motive, SectionPairing *pairing, SectionConstruction *out) {
    size_t per_bar = (size_t)request->meter.beats_per_bar;
    if (!section->has_construction || section->fill != PIECE_FILL_MOTIF || section->description.pickup.num != 0 ||
        start_beats % per_bar != 0 || section->description.beats % per_bar != 0 || !theme_pairing(section, pairing)) {
        return false;
    }
    size_t bars = section->description.beats / per_bar;
    if (section->construction_auto) {
        return grow_construction_choose(motive, &request->meter, bars, pairing, section->description.cadence, out);
    }
    *out = section->construction;
    return grow_construction_buildable(&request->meter, bars, pairing, section->description.cadence, ANTECEDENT_CADENCE, out);
}

/* The key over the start of a span, when one key span holds it. */
static bool key_over(const PiecePlan *plan, PlanSpan span, AnalysisKey *out) {
    LaneSpanId ids[2];
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, ids, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, ids[0]), span)) {
        return false;
    }
    *out = plan_lane_span_payload(plan, ids[0])->key;
    return true;
}

/*
 * The m0 of the theme plan of a theme fill, in the request's key: the pitches move by the key difference, as the
 * theme's lanes do (plan_give_section). False when the plan has no theme section, no m0, or is in another mode.
 */
static bool theme_motive(const PieceRequest *request, const PieceSection *section, Motive *out) {
    const PiecePlan *theme_plan = section->theme_plan;
    NodeId theme = theme_plan ? grow_find_theme(theme_plan, false) : PLAN_NODE_NONE;
    NodeId source = theme_plan ? grow_find_source(theme_plan) : PLAN_NODE_NONE;
    PlanSpan span;
    PlanMeter meter;
    AnalysisKey key;
    if (theme == PLAN_NODE_NONE || source == PLAN_NODE_NONE) {
        return false;
    }
    span = plan_node_span(theme_plan, theme);
    if (!return_meter_over(theme_plan, span, &meter) || !key_over(theme_plan, span, &key) ||
        composer_key_is_minor(&key) != composer_key_is_minor(&request->key) || !grow_read_motive(theme_plan, source, &meter, &key, out)) {
        return false;
    }
    int shift = util_pitch_class(analysis_key_tonic_pitch_class(&request->key) - analysis_key_tonic_pitch_class(&key));
    shift = shift > 6 ? shift - 12 : shift;
    for (size_t i = 0; i < out->note_count; i++) {
        if (out->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            continue;
        }
        out->notes[i].pitch += shift;
        if (out->notes[i].pitch < 0 || out->notes[i].pitch > 127) {
            return false;
        }
    }
    motive_set_key(out, &request->key, out->key_source);
    return true;
}

/* The basic motive (R-170): the request's, else the first motif section's or theme fill's, else NULL to have one invented. */
static const Motive *basic_motive(const PieceRequest *request, Motive *storage) {
    if (request->motive) {
        return request->motive;
    }
    for (size_t i = 0; i < request->section_count; i++) {
        const PieceSection *section = &request->sections[i];
        if (section->fill == PIECE_FILL_MOTIF && section->motive) {
            return section->motive;
        }
        if (section->fill == PIECE_FILL_THEME && theme_motive(request, section, storage)) {
            return storage;
        }
    }
    return NULL;
}

/* The section of a plan that a piece part and id name, or PLAN_NODE_NONE. */
static NodeId find_part_section(const PiecePlan *plan, const PieceSection *section) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
            continue;
        }
        const SectionDescription *described = plan_node_section(plan, id);
        if (described->has_role && described->role >= SECTION_ROLE_INTRO && described->role <= SECTION_ROLE_ENDING &&
            (int)(described->role - SECTION_ROLE_INTRO) == (int)piece_section_part(section) && strcmp(described->id, section->description.id) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* The piece's basic motive as the realized loop plan holds it (m0, in the request's key). */
static bool loop_motive(const PieceRequest *request, const PiecePlan *loop_plan, Motive *out) {
    NodeId source = grow_find_source(loop_plan);
    PlanMeter meter;
    AnalysisKey key;
    if (source == PLAN_NODE_NONE) {
        return false;
    }
    PlanSpan span = plan_node_span(loop_plan, source);
    return return_meter_over(loop_plan, span, &meter) && key_over(loop_plan, span, &key) &&
           composer_key_is_minor(&key) == composer_key_is_minor(&request->key) && analysis_key_tonic_pitch_class(&key) == analysis_key_tonic_pitch_class(&request->key) &&
           grow_read_motive(loop_plan, source, &meter, &key, out);
}

/*
 * Whether the section is held for the seam stage (R-171): the loop's end, unless it is Given (a theme fill, or a
 * progression that covers it) or holds the body's first bar, which the seam copies before anything can be filled.
 */
static bool is_held(const FormSpec *form, const PieceRequest *request, size_t index, size_t cells) {
    const PieceSection *section = &request->sections[index];
    size_t loop_end = request->section_count;
    size_t first_body = request->section_count;
    for (size_t i = 0; i < request->section_count; i++) {
        if (piece_section_part(&request->sections[i]) == first_loop_part(form) && first_body == request->section_count) {
            first_body = i;
        }
        if (in_loop(form, piece_section_part(&request->sections[i]))) {
            loop_end = i;
        }
    }
    if (index != loop_end || index == first_body || section->fill == PIECE_FILL_THEME) {
        return false;
    }
    if (section->fill == PIECE_FILL_PROGRESSION) {
        size_t per_chord = section->cells_per_chord ? section->cells_per_chord : cells;
        return section->numeral_count * per_chord * (size_t)request->meter.beats_per_bar < section->description.beats * cells;
    }
    return true;
}

static PieceResult add_request_section(PiecePlan *plan, const PieceRequest *request, const PieceSection *section,
                                       size_t start_beats, const Motive *motive, NodeId form, PlanSpan span, NodeId *out) {
    PlanConstraint constraint;
    SectionPairing pairing;
    SectionConstruction construction;
    /* A theme fill is the theme it was composed as: its archetype, pairing and cadences, not the request's. */
    const PiecePlan *theme_plan = section->fill == PIECE_FILL_THEME ? section->theme_plan : NULL;
    NodeId given = theme_plan ? grow_find_theme(theme_plan, false) : PLAN_NODE_NONE;
    if (section->fill == PIECE_FILL_THEME && given == PLAN_NODE_NONE) {
        return PIECE_ERR_BAD_REQUEST;
    }
    bool constructed = given == PLAN_NODE_NONE && section->has_construction;
    if (constructed && !section_construction(request, section, start_beats, motive, &pairing, &construction)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    bool theme = constructed || (given == PLAN_NODE_NONE && is_theme_section(request, section, start_beats, &pairing));
    const bool transition = piece_section_function(section) == PIECE_FUNCTION_TRANSITION;
    const bool coda = piece_section_function(section) == PIECE_FUNCTION_CODA;
    if (transition || coda) {
        /* A function is the section's own growing (R-508): empty or motif fill, and no given theme. */
        if (given != PLAN_NODE_NONE || (section->fill != PIECE_FILL_EMPTY && section->fill != PIECE_FILL_MOTIF) ||
            section->has_construction) {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    if (transition && (!theme || (section->has_into && section->into != PLAN_ARCHETYPE_SENTENCE) ||
                       section->description.cadence != CADENCE_TYPE_HALF)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (coda) {
        /* A coda is 8 whole bars from a barline, no pickup, closing on a perfect authentic cadence. */
        size_t per_bar = (size_t)request->meter.beats_per_bar;
        if (section->description.pickup.num != 0 || start_beats % per_bar != 0 || section->description.beats != 8 * per_bar ||
            section->description.cadence != CADENCE_TYPE_PERFECT_AUTHENTIC) {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    /* The description: a theme fill is the theme it was composed as, with its pairing and cadences, not the request's. */
    SectionDescription description;
    section_description_init(&description);
    if (coda) {
        /* No pairing or free phrase: grow-coda lays the section out (R-509). */
    } else if (given != PLAN_NODE_NONE) {
        const SectionDescription *given_description = plan_node_section(theme_plan, given);
        description.has_pairing = given_description->has_pairing;
        description.pairing = given_description->pairing;
        description.has_cadence = given_description->has_cadence;
        description.cadence = given_description->cadence;
        description.has_first_cadence = given_description->has_first_cadence;
        description.first_cadence = given_description->first_cadence;
    } else if (theme) {
        description.has_pairing = true;
        description.pairing = pairing;
        description.has_construction = constructed;
        if (constructed) {
            description.construction = construction;
        }
    } else {
        /* Any other length is a free phrase, which grow-theme lays out in ideas of a bar (grow_free.c). */
        description.free_phrase = true;
    }
    if (given == PLAN_NODE_NONE) {
        description.has_cadence = true;
        description.cadence = section->description.cadence;
    }
    if (theme && pairing.beginning == SECTION_BEGINNING_ANTECEDENT) {
        description.has_first_cadence = true;
        description.first_cadence = ANTECEDENT_CADENCE;
    }
    if (transition || coda) {
        description.has_function = true;
        description.function.function = transition ? SECTION_FUNCTION_TRANSITION : SECTION_FUNCTION_CODA;
    }
    snprintf(description.letter, sizeof(description.letter), "%s", section->description.letter);
    description.has_role = true;
    description.role = (SectionRole)(SECTION_ROLE_INTRO + (int)piece_section_part(section));
    snprintf(description.id, sizeof(description.id), "%s", section->description.id);
    PieceResult result = piece_result_of_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, span, section->description.id, out));
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_section(plan, *out, &description));
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && request->has_accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = request->accompaniment;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && request->has_voicing) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_VOICING;
        constraint.params.voicing = request->voicing;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->description.pickup.num != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_PICKUP;
        constraint.params.pickup = section->description.pickup;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    return result;
}

/*
 * The plan of the request. With a loop plan it is the ending version (R-172): the intro and the body Given from the
 * realized loop, the turnaround and the seam left out, the ending laid out open after the body.
 */
PieceResult form_build_request_plan(const FormSpec *form, const PieceRequest *request, const PiecePlan *loop_plan,
                                    PiecePlan **out) {
    if (!form || !request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    PieceResult result = check(form, request);
    if (result != PIECE_OK) {
        return result;
    }
    size_t cells;
    Rational bar = grow_bar_length(&request->meter);
    Motive theme_storage;
    const Motive *motive = basic_motive(request, &theme_storage);
    if (loop_plan) {
        if (!loop_motive(request, loop_plan, &theme_storage)) {
            return PIECE_ERR_BAD_REQUEST;
        }
        motive = &theme_storage;
    }
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS];
    PieceRequest copy = *request;
    memcpy(sections, request->sections, request->section_count * sizeof(PieceSection));
    copy.sections = sections;
    piece_default_cadences(form, &copy);
    if ((motive && motive->note_count < 2) ||
        (request->has_accompaniment && !grow_accompaniment_valid(&request->meter, &request->accompaniment)) ||
        !grow_cells_per_bar(&request->meter, &cells)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    size_t motive_bars = motive ? grow_motive_layout_bars(motive, bar) : 1;
    /* The sections laid out are the intro, the body and the turnaround; the ending version lays out the ending. */
    size_t laid = 0;
    size_t beats = 0;
    size_t material_bars = motive_bars;
    size_t per_bar = (size_t)request->meter.beats_per_bar;
    bool has_ending = false;
    /* The motive fills the basic idea of every section laid out: its construction's, else 2 bars of an 8-bar theme, 1 of a 4-bar one or a free phrase. */
    size_t basic_bars = 2;
    for (size_t i = 0; i < copy.section_count; i++) {
        has_ending = has_ending || piece_section_part(&copy.sections[i]) == PIECE_PART_ENDING;
        if (loop_plan ? !is_dropped(form, piece_section_part(&copy.sections[i])) : piece_section_part(&copy.sections[i]) != PIECE_PART_ENDING) {
            size_t section_basic = copy.sections[i].description.beats == 8 * per_bar ? 2 : 1;
            if (copy.sections[i].has_construction) {
                SectionPairing pairing;
                SectionConstruction construction;
                if (!section_construction(&copy, &copy.sections[i], beats, motive, &pairing, &construction)) {
                    return PIECE_ERR_BAD_REQUEST;
                }
                section_basic = construction.idea_bars[GROW_IDEA_BASIC];
                /* The repeats and the addition of a construction have motives of their own in the material (R-468). */
                material_bars += grow_extension_material_bars(&pairing, &construction);
            }
            basic_bars = section_basic < basic_bars ? section_basic : basic_bars;
            laid = i + 1;
            beats += copy.sections[i].description.beats;
            material_bars += MATERIAL_BARS_PER_SECTION * motive_bars + (copy.sections[i].description.beats + per_bar - 1) / per_bar;
        }
    }
    if (loop_plan && !has_ending) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (motive && !grow_motive_fits_idea(motive, bar, basic_bars)) {
        return PIECE_ERR_MOTIVE_TOO_LONG;
    }
    PiecePlan *plan = plan_create();
    if (!plan) {
        return PIECE_ERR_ALLOC;
    }
    /* The seam is a bar after the loop's end; the material starts on the first barline at or after the seam's end: its
       motives are bars of the meter. The ending version has no seam. */
    beats += loop_plan ? 0 : per_bar;
    PlanSpan material = {{0, 1}, {0, 1}};
    if (rational_mul_int(bar, (long long)((beats + per_bar - 1) / per_bar), &material.start) != RATIONAL_OK) {
        plan_destroy(plan);
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanSpan span_form = {{0, 1}, {0, 1}};
    NodeId form_id = PLAN_NODE_NONE;
    NodeId source = PLAN_NODE_NONE;
    PlanLanePayload payload;
    LaneSpanId lane;
    Rational bars_length = {0, 1};
    if (rational_mul_int(bar, (long long)material_bars, &bars_length) != RATIONAL_OK) {
        plan_destroy(plan);
        return PIECE_ERR_BAD_REQUEST;
    }
    material.length = bars_length;
    result = piece_result_of_plan(plan_result_of_rational(rational_add(material.start, material.length, &span_form.length)));
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, span_form, form->plan_name, &form_id));
    }
    /* The meter and the key are Given over the sections and the material: the piece is in its home key throughout. */
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = piece_result_of_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, span_form, &payload, &lane));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.key = request->key;
        result = piece_result_of_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_KEY, 0, span_form, &payload, &lane));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    /* The sections in time order, each from the end of the one before. */
    Rational start = {0, 1};
    size_t start_beats = 0;
    for (size_t i = 0; result == PIECE_OK && i < laid; i++) {
        if (loop_plan && is_dropped(form, piece_section_part(&copy.sections[i]))) {
            continue;
        }
        NodeId id;
        /* The ending version takes the intro and the body as the loop realized them. */
        bool from_loop = loop_plan && piece_section_part(&copy.sections[i]) != PIECE_PART_ENDING;
        NodeId loop_section = from_loop ? find_part_section(loop_plan, &copy.sections[i]) : PLAN_NODE_NONE;
        PlanSpan span = {start, beats_of(&request->meter, copy.sections[i].description.beats)};
        result = add_request_section(plan, &copy, &copy.sections[i], start_beats, motive, form_id, span, &id);
        if (result == PIECE_OK && from_loop) {
            if (loop_section == PLAN_NODE_NONE || plan_give_section(plan, id, loop_plan, loop_section) != GIVEN_OK) {
                result = PIECE_ERR_BAD_REQUEST;
            }
        } else if (result == PIECE_OK && copy.sections[i].fill == PIECE_FILL_THEME &&
                   plan_give_section(plan, id, copy.sections[i].theme_plan,
                                     grow_find_theme(copy.sections[i].theme_plan, false)) != GIVEN_OK) {
            result = PIECE_ERR_BAD_REQUEST;
        }
        /* A progression fill: the chords Given from the section's start, a bar each unless it says; the harmony stage
           fills what is left of the section. */
        if (result == PIECE_OK && !from_loop && copy.sections[i].fill == PIECE_FILL_PROGRESSION) {
            const PieceSection *given = &copy.sections[i];
            size_t per_chord = given->cells_per_chord ? given->cells_per_chord : cells;
            if (plan_give_progression(plan, id, 0, given->numerals, given->numeral_count, per_chord) != GIVEN_OK) {
                result = PIECE_ERR_BAD_REQUEST;
            }
        }
        if (result == PIECE_OK && !loop_plan && is_held(form, &copy, i, cells)) {
            SectionDescription held = *plan_node_section(plan, id);
            held.leads_in = true;
            result = piece_result_of_plan(plan_node_set_section(plan, id, &held));
        }
        if (result == PIECE_OK) {
            result = piece_result_of_plan(plan_result_of_rational(rational_add(start, span.length, &start)));
        }
        start_beats += copy.sections[i].description.beats;
    }
    /* The seam: the bar the loop leads into, open and empty until the seam stage copies the body's first bar into it. */
    if (result == PIECE_OK && !loop_plan) {
        NodeId seam;
        PlanSpan span = {start, bar};
        result = piece_result_of_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, span, "seam", &seam));
        if (result == PIECE_OK) {
            SectionDescription seam_description;
            section_description_init(&seam_description);
            seam_description.has_role = true;
            seam_description.role = SECTION_ROLE_SEAM;
            result = piece_result_of_plan(plan_node_set_section(plan, seam, &seam_description));
        }
        if (result == PIECE_OK) {
            PlanConstraint key;
            memset(&key, 0, sizeof(key));
            key.kind = PLAN_CONSTRAINT_KEY;
            key.params.key = request->key;
            result = piece_add_constraint(plan, seam, &key);
        }
    }
    if (result == PIECE_OK) {
        result = piece_add_material(plan, motive, NULL, form_id, material, bar, &source);
    }
    if (result == PIECE_OK && request->has_style) {
        PlanConstraint style;
        memset(&style, 0, sizeof(style));
        style.kind = PLAN_CONSTRAINT_STYLE;
        style.params.style = request->style;
        result = piece_add_constraint(plan, form_id, &style);
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }
    result = result == PIECE_OK ? piece_set_form_record(plan, form) : result;
    if (result != PIECE_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PIECE_OK;
}

PieceResult piece_build_ending(const PiecePlan *loop_plan, const PieceRequest *request, PiecePlan **out) {
    if (!loop_plan || !request || !out) {
        if (out) {
            *out = NULL;
        }
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    FormSpec *form;
    PieceResult result = request_form(request, &form);
    if (result != PIECE_OK) {
        return result;
    }
    result = form_build_request_plan(form, request, loop_plan, out);
    form_spec_destroy(form);
    return result;
}
