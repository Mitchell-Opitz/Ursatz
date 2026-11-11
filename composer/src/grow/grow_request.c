#include <ursatz_composer/grow.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-texture/texture_rules.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "util/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"

const char *grow_result_name(GrowResult result) {
    switch (result) {
    case GROW_OK:
        return "ok";
    case GROW_ERR_NULL:
        return "null argument";
    case GROW_ERR_ALLOC:
        return "allocation failed";
    case GROW_ERR_MOTIVE_TOO_LONG:
        return "motive too long";
    case GROW_ERR_BAD_REQUEST:
        return "bad request";
    case GROW_ERR_STUCK:
        return "stuck";
    }
    return "unknown";
}

const char *grow_stage_name(GrowStage stage) {
    static const char *const NAMES[GROW_STAGE_COUNT] = {"structure", "motives", "harmony", "bass", "melody", "texture"};
    return stage >= 0 && stage < GROW_STAGE_COUNT ? NAMES[stage] : "unknown";
}

const char *grow_slot_name(GrowSlot slot) {
    const ThemeShapeSlot *row = theme_shape_slot(slot);
    return row ? row->name : "unknown";
}

const char *grow_slot_function(GrowSlot slot) {
    const ThemeShapeSlot *row = theme_shape_slot(slot);
    return row ? row->function : "unknown";
}

const char *grow_slot_node_name(GrowSlot slot) {
    const ThemeShapeSlot *row = theme_shape_slot(slot);
    return row ? row->node_name : NULL;
}

NodeId grow_find_slot_node(const PiecePlan *plan, GrowSlot slot) {
    const char *wanted = grow_slot_node_name(slot);
    for (NodeId id = 1; wanted && id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, wanted) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

const char *grow_idea_name(GrowIdea idea) {
    return theme_shape_idea_name(idea);
}

bool grow_idea_place(const PlanThemePairing *pairing, GrowIdea idea, GrowPlace *place) {
    const ThemeShapeIdea *row = theme_shape_idea_in(pairing, idea);
    if (row && place) {
        *place = (GrowPlace)row->place;
    }
    return row != NULL;
}

bool grow_idea_in_pairing(const PlanThemePairing *pairing, GrowIdea idea) {
    return theme_shape_idea_in(pairing, idea) != NULL;
}

size_t grow_place_units(const PlanThemePairing *pairing, GrowPlace place) {
    const ThemeShapeIdea *row = theme_shape_place_in(pairing, (unsigned)place);
    return row ? row->units : 2;
}

const char *grow_idea_node_name(GrowIdea idea, char *buffer, size_t size) {
    snprintf(buffer, size, "i%d", (int)idea);
    return buffer;
}

NodeId grow_find_idea_node(const PiecePlan *plan, GrowIdea idea) {
    char wanted[8];
    grow_idea_node_name(idea, wanted, sizeof(wanted));
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, wanted) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool grow_idea_own(const GrowThemeSpec *spec, GrowPlace idea) {
    bool first;
    GrowSlot slot = grow_idea_slot(&spec->pairing, idea, &first);
    return (idea != GROW_PLACE_BASIC && spec->has_idea_source[idea]) ||
           (idea != GROW_PLACE_BASIC && first && spec->has_slot_source[slot]);
}

GrowSlot grow_idea_slot(const PlanThemePairing *pairing, GrowPlace idea, bool *first) {
    const ThemeShapeIdea *row = theme_shape_place_in(pairing, (unsigned)idea);
    /* The cadential place has a row only under a continuation; the other endings never ask for it, and it was a cadential slot's first. */
    GrowSlot slot = row ? row->slot : idea == GROW_PLACE_CADENTIAL ? GROW_SLOT_CADENTIAL : GROW_SLOT_BASIC;
    if (first) {
        *first = row ? row->first : idea == GROW_PLACE_CADENTIAL;
    }
    return slot;
}

bool grow_slot_used(const PlanThemePairing *pairing, GrowSlot slot, size_t *units) {
    size_t room = 0;
    bool used = false;
    for (size_t i = 0; i < theme_shape_idea_count(); i++) {
        const ThemeShapeIdea *row = theme_shape_idea_at(i);
        if (row->slot != slot || row->half != (row->in_beginning ? (int)pairing->beginning : (int)pairing->ending)) {
            continue;
        }
        used = true;
        if (row->first && room == 0) {
            room = row->units;
        }
    }
    if (units) {
        *units = room;
    }
    return used;
}

bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    return harmony_rhythm_cells_per_bar(meter, cells);
}

Rational grow_bar_length(const PlanMeter *meter) {
    Rational bar = {0, 1};
    if (rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar) != RATIONAL_OK) {
        bar.num = 0;
        bar.den = 1;
    }
    return bar;
}

GrowMotiveFit grow_motive_fit(const Motive *motive, Rational bar, size_t idea_bars) {
    Rational span = {0, 1};
    long long bars = 0;
    if (bar.num <= 0 || rational_sub(motive->length, motive_pickup(motive), &span) != RATIONAL_OK || span.num <= 0 ||
        rational_div(span, bar, &span) != RATIONAL_OK || rational_ceil(span, &bars) != RATIONAL_OK) {
        return GROW_FIT_SHORTER;
    }
    return bars < (long long)idea_bars ? GROW_FIT_SHORTER : (bars == (long long)idea_bars ? GROW_FIT_WHOLE : GROW_FIT_LONGER);
}

bool grow_motive_fits_idea(const Motive *motive, Rational bar, size_t idea_bars) {
    Rational room = {0, 1};
    /* A pickup is part of the motive's length, so it counts toward the idea's: the motive is placed from its start. */
    return grow_motive_fit(motive, bar, idea_bars) != GROW_FIT_LONGER &&
           rational_mul_int(bar, (long long)idea_bars, &room) == RATIONAL_OK && rational_cmp(motive->length, room) <= 0;
}

bool grow_motive_fills_part(const Motive *motive, Rational bar, size_t part_bars, PlanThemeBeginning beginning) {
    Rational room = {0, 1};
    return (beginning == PLAN_THEME_BEGINNING_PRESENTATION || beginning == PLAN_THEME_BEGINNING_ANTECEDENT) &&
           grow_motive_fit(motive, bar, part_bars) == GROW_FIT_WHOLE &&
           rational_mul_int(bar, (long long)part_bars, &room) == RATIONAL_OK && rational_cmp(motive->length, room) <= 0;
}

bool grow_part_basic_idea(const Motive *motive, Rational bar, size_t part_bars, size_t basic_bars,
                          PlanThemeBeginning beginning, Motive *basic, MotiveRelationStep *step) {
    Rational idea_end = {0, 1};
    size_t leading = 0;
    if (!grow_motive_fills_part(motive, bar, part_bars, beginning) ||
        rational_add(motive_pickup(motive), grow_scaled(bar, basic_bars), &idea_end) != RATIONAL_OK) {
        return false;
    }
    /* The notes that start inside the idea's bars; a note running past the idea's end makes no basic idea. */
    while (leading < motive->note_count && rational_cmp(motive->notes[leading].onset, idea_end) < 0) {
        leading++;
    }
    memset(step, 0, sizeof(*step));
    step->kind = MOTIVE_RELATION_FRAGMENT;
    step->value[0] = MOTIVE_FRAGMENT_LEADING;
    step->value[1] = (long)leading;
    return leading >= 2 && leading < motive->note_count && motive_develop(motive, step, basic) == MOTIVE_DEV_OK &&
           basic->note_count >= 2 && rational_cmp(basic->length, idea_end) <= 0;
}

size_t grow_motive_layout_bars(const Motive *motive, Rational bar) {
    Rational span = {0, 1};
    long long bars = 0;
    if (bar.num <= 0 || rational_div(motive->length, bar, &span) != RATIONAL_OK || rational_ceil(span, &bars) != RATIONAL_OK ||
        bars < 1) {
        return 1;
    }
    return (size_t)bars;
}

static Rational times(Rational time, size_t count) {
    Rational out = {0, 1};
    if (rational_mul_int(time, (long long)count, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

static GrowResult from_plan(PlanResult result) {
    return result == PLAN_OK ? GROW_OK : (result == PLAN_ERR_ALLOC ? GROW_ERR_ALLOC : GROW_ERR_BAD_REQUEST);
}

/* Whether a half of the theme, half_bars bars, has room for a cadence at all. */
static bool half_fits(size_t half_bars, size_t cells_per_bar, CadenceType cadence) {
    size_t cells = half_bars * cells_per_bar;
    /* A cadential progression has no opening tonic, so it needs one cell fewer than the phrase model. */
    return cells + 1 >= harmony_phrase_room_cells(cadence, cells_per_bar);
}

/* Whether an antecedent can end in `first` below a theme that ends in `last`: weak, and weaker. */
static bool cadences_ordered(CadenceType first, CadenceType last) {
    return (first == CADENCE_TYPE_HALF || first == CADENCE_TYPE_IMPERFECT_AUTHENTIC) &&
           harmony_cadence_strength(first) <
               harmony_cadence_strength(last);
}

/* Records a refusal: its result, the request path of its field and its message. */
static GrowResult refuse(GrowRefusal *refusal, GrowResult result, const char *path, const char *format, ...) {
    if (refusal) {
        va_list args;
        refusal->result = result;
        snprintf(refusal->path, sizeof(refusal->path), "%s", path);
        va_start(args, format);
        vsnprintf(refusal->message, sizeof(refusal->message), format, args);
        va_end(args);
    }
    return result;
}

bool grow_accompaniment_valid(const PlanMeter *meter, const PlanAccompaniment *accompaniment) {
    bool compound = meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0;
    int low = accompaniment->low ? accompaniment->low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
    int high = accompaniment->high ? accompaniment->high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;
    return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_MOTIVE &&
           (accompaniment->pattern == PLAN_ACCOMPANIMENT_MOTIVE) == (accompaniment->motive_node[0] != '\0') &&
           accompaniment->texture >= -1 && accompaniment->texture < (int)STANDARD_TEXTURE_COUNT &&
           (accompaniment->texture < 0 || standard_texture_is_homophonic((StandardTexture)accompaniment->texture)) &&
           low >= 0 && high <= 127 && low < high &&
           !(accompaniment->pattern == PLAN_ACCOMPANIMENT_ALBERTI && compound);
}

bool grow_theme_buildable(const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                          CadenceType cadence, CadenceType first) {
    size_t cells_per_bar;
    bool antecedent = pairing->beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    return grow_cells_per_bar(meter, &cells_per_bar) && (bars == 4 || bars == 8) &&
           grow_pairing_archetype(pairing) >= 0 &&
           (cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC || cadence == CADENCE_TYPE_HALF) &&
           half_fits(bars / 2, cells_per_bar, cadence) &&
           (!antecedent || (cadences_ordered(first, cadence) && half_fits(bars / 2, cells_per_bar, first)));
}

/* The bars of the beginning (or the ending) of a theme with a construction: those of the ideas its pairing has there. */
static size_t construction_half_bars(const PlanThemePairing *pairing, bool beginning, const PlanConstruction *construction) {
    size_t bars = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(pairing, (GrowIdea)idea);
        if (row && row->in_beginning == beginning) {
            bars += construction->idea_bars[idea];
        }
    }
    return bars;
}

/* The bars of the first idea of the slot under the construction; 0 when the pairing has none. */
static size_t construction_slot_bars(const PlanThemePairing *pairing, GrowSlot slot, const PlanConstruction *construction) {
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(pairing, (GrowIdea)idea);
        if (row && row->slot == slot && row->first) {
            return construction->idea_bars[idea];
        }
    }
    return 0;
}

/*
 * Why a construction does not fit the pairing and the bars, NULL when it does: an idea the pairing does not have, an idea
 * it has with no bar, ideas that do not add up to the theme, an extension (not built yet). The message goes in `message`.
 */
static bool construction_fault(const PlanThemePairing *pairing, size_t bars, const PlanConstruction *construction,
                               char *message, size_t size) {
    size_t total = 0;
    const ThemeShapeHalf *beginning = theme_shape_beginning(pairing->beginning);
    const ThemeShapeHalf *ending = theme_shape_ending(pairing->ending);
    char name[64];
    snprintf(name, sizeof(name), "%s+%s", beginning ? beginning->plan_name : "?", ending ? ending->plan_name : "?");
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        if (!theme_shape_idea_in(pairing, (GrowIdea)idea)) {
            if (construction->idea_bars[idea] != 0) {
                snprintf(message, size, "the %s has no %s", name, grow_idea_name((GrowIdea)idea));
                return true;
            }
        } else if (construction->idea_bars[idea] == 0) {
            snprintf(message, size, "the %s needs a bar or more", grow_idea_name((GrowIdea)idea));
            return true;
        } else {
            total += construction->idea_bars[idea];
        }
    }
    if (total != bars) {
        snprintf(message, size, "the ideas are %zu bars; the theme has %zu", total, bars);
        return true;
    }
    if (construction->extension_count != 0) {
        snprintf(message, size, "an extension is not built yet");
        return true;
    }
    return false;
}

bool grow_construction_buildable(const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                                 CadenceType cadence, CadenceType first, const PlanConstruction *construction) {
    size_t cells_per_bar;
    char message[96];
    bool antecedent = pairing->beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    return construction && grow_cells_per_bar(meter, &cells_per_bar) && bars >= 3 && bars <= 16 &&
           grow_pairing_archetype(pairing) >= 0 &&
           (cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC || cadence == CADENCE_TYPE_HALF) &&
           !construction_fault(pairing, bars, construction, message, sizeof(message)) &&
           half_fits(construction_half_bars(pairing, false, construction), cells_per_bar, cadence) &&
           (!antecedent || (cadences_ordered(first, cadence) &&
                            half_fits(construction_half_bars(pairing, true, construction), cells_per_bar, first)));
}

/* The bars of an idea that has `units` eighths of a theme of `bars` bars, at least one (a half-bar idea is a bar's). */
static size_t idea_bars(size_t bars, size_t units) {
    size_t whole = bars * units / 8;
    return whole < 1 ? 1 : whole;
}

/*
 * Why a motive given for an idea cannot be used there, NULL when it can. The idea has `units` eighths of the theme, or,
 * when `given_bars` is not 0, that many bars (a construction's).
 */
static const char *motive_reason(const GrowRequest *request, const Motive *motive, size_t units, size_t given_bars,
                                 Rational bar) {
    Rational room;
    if (motive->form != MOTIVE_SKETCH_FORM_FULL || motive->note_count < 2) {
        return "needs a full motif of at least two notes";
    }
    if (motive_meter(motive).beats_per_bar != request->meter.beats_per_bar ||
        motive_meter(motive).beat_unit != request->meter.beat_unit) {
        return "is not in the theme's meter";
    }
    if (motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED &&
        (motive->tonic_pc != analysis_key_tonic_pitch_class(&request->key) ||
         motive->minor != (request->key.mode == KEY_STRUCTURE_MODE_MINOR))) {
        return "is not in the theme's key";
    }
    /* The idea has units eighths of the theme; the longest motive is as long as that idea. */
    if (given_bars > 0) {
        if ((rational_mul_int(bar, (long long)given_bars, &room) == RATIONAL_OK && rational_cmp(motive->length, room) > 0) ||
            !grow_motive_fits_idea(motive, bar, given_bars)) {
            return "is longer than the idea it fills";
        }
        return NULL;
    }
    if ((units > 0 && rational_make(bar.num * (long)request->bars * (long)units, bar.den * 8, &room) == RATIONAL_OK &&
         rational_cmp(motive->length, room) > 0) ||
        !grow_motive_fits_idea(motive, bar, idea_bars(request->bars, units))) {
        return "is longer than the idea it fills";
    }
    return NULL;
}

/* Whether a slot's motive is one the theme can be built from: see GrowRequest.slot_motives. */
static GrowResult slot_motive_valid(const GrowRequest *request, GrowSlot slot, Rational bar, GrowRefusal *refusal) {
    size_t units;
    char path[64];
    snprintf(path, sizeof(path), "/slots/%s", grow_slot_name(slot));
    if (!grow_slot_used(&request->pairing, slot, &units)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, path, "the %s idea has no place in this pairing", grow_slot_name(slot));
    }
    size_t given = request->construction ? construction_slot_bars(&request->pairing, slot, request->construction) : 0;
    const char *reason = motive_reason(request, request->slot_motives[slot], units, given, bar);
    return reason ? refuse(refusal, GROW_ERR_BAD_REQUEST, path, "the %s motif %s", grow_slot_name(slot), reason) : GROW_OK;
}

/* Whether the motive of a single idea is one the theme can be built from: see GrowRequest.idea_motives. */
static GrowResult idea_motive_valid(const GrowRequest *request, GrowIdea idea, Rational bar, GrowRefusal *refusal) {
    GrowPlace place;
    char path[64];
    snprintf(path, sizeof(path), "/ideas/%.50s", grow_idea_name(idea));
    if (idea == GROW_IDEA_BASIC || !grow_idea_place(&request->pairing, idea, &place)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, path, "the idea %s has no place in this pairing", grow_idea_name(idea));
    }
    size_t given = request->construction ? request->construction->idea_bars[idea] : 0;
    const char *reason =
        motive_reason(request, request->idea_motives[idea], grow_place_units(&request->pairing, place), given, bar);
    return reason ? refuse(refusal, GROW_ERR_BAD_REQUEST, path, "the motif of idea %s %s", grow_idea_name(idea), reason)
                  : GROW_OK;
}

/* Why the pairing, the bars, the cadences or the key cannot be built, with the field to blame. */
static GrowResult theme_refusal(const GrowRequest *request, CadenceType first, GrowRefusal *refusal) {
    size_t cells_per_bar;
    bool antecedent = request->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    CadenceType cadence = request->cadence;
    const PlanConstruction *construction = request->construction;
    if (construction ? (request->bars < 3 || request->bars > 16) : (request->bars != 4 && request->bars != 8)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/bars", "a theme has 4 or 8 bars, or 3 to 16 with a construction, not %zu",
                      request->bars);
    }
    char fault[96];
    if (construction && construction_fault(&request->pairing, request->bars, construction, fault, sizeof(fault))) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/construction/ideas", "%s", fault);
    }
    size_t beginning_bars = construction ? construction_half_bars(&request->pairing, true, construction) : request->bars / 2;
    size_t ending_bars = construction ? construction_half_bars(&request->pairing, false, construction) : request->bars / 2;
    if (!grow_cells_per_bar(&request->meter, &cells_per_bar) || grow_pairing_archetype(&request->pairing) < 0) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/hybrid", "this pairing cannot be built in %d/%d",
                      (int)request->meter.beats_per_bar, (int)request->meter.beat_unit);
    }
    if (cadence != CADENCE_TYPE_PERFECT_AUTHENTIC && cadence != CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
        cadence != CADENCE_TYPE_HALF) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/cadence", "a theme ends in a PAC, an IAC or an HC");
    }
    if (antecedent && !cadences_ordered(first, cadence)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/first_cadence",
                      "an antecedent ends in a cadence weaker than the theme's (an HC or an IAC below a PAC)");
    }
    if (!half_fits(ending_bars, cells_per_bar, cadence)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/bars", "%zu bars leave no room for the cadence", request->bars);
    }
    if (antecedent && !half_fits(beginning_bars, cells_per_bar, first)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/first_cadence", "half of %zu bars leaves no room for the first cadence",
                      request->bars);
    }
    return GROW_OK;
}

/* The degree (1 to 7) a numeral is built on, ignoring accidentals and case; 0 when it cannot be read. */
static int numeral_degree(const char *numeral, bool *inverted) {
    static const char *const ROMANS[] = {"VII", "VI", "IV", "V", "III", "II", "I"};
    static const int DEGREES[] = {7, 6, 4, 5, 3, 2, 1};
    char upper[16];
    size_t length = 0;
    const char *at = numeral;
    while (*at == 'b' || *at == '#') {
        at++;
    }
    while (*at && length + 1 < sizeof(upper) && strchr("ivIV", *at)) {
        upper[length++] = (char)(*at >= 'a' ? *at - 'a' + 'A' : *at);
        at++;
    }
    upper[length] = '\0';
    int degree = 0;
    for (size_t i = 0; i < sizeof(DEGREES) / sizeof(DEGREES[0]); i++) {
        if (strcmp(upper, ROMANS[i]) == 0) {
            degree = DEGREES[i];
        }
    }
    /* The figures after the roman numeral: a 6, 64, 65, 43, 42 or 2 is an inversion; 7 and none are root position. */
    while (*at == 'o' || *at == '+' || *at == '/' || *at == '\xc2' || *at == '\xb0') {
        at++;
    }
    *inverted = *at && *at != '7' && *at != '9' && *at != '@';
    return numeral[0] == 'b' || numeral[0] == '#' ? 0 : degree;
}

/* The name of a cadence the plan can end in, and what it needs of the chord that arrives. */
static const char *cadence_label(CadenceType cadence) {
    return cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? "PAC" : cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ? "IAC" : "HC";
}

static const char *cadence_needs(CadenceType cadence) {
    return cadence == CADENCE_TYPE_PERFECT_AUTHENTIC   ? "a tonic chord in root position"
           : cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ? "a tonic chord"
                                                         : "the dominant";
}

/* Whether a chord can arrive the cadence: the tonic (root position for a PAC), or the dominant for a half cadence. */
static bool cadence_allows(CadenceType cadence, const char *numeral) {
    bool inverted;
    int degree = numeral_degree(numeral, &inverted);
    return cadence == CADENCE_TYPE_HALF ? degree == 5
           : cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? degree == 1 && !inverted
                                                       : degree == 1;
}

/* The first given chord that starts in `bar` and cannot arrive `cadence`, refused at its entry of "chords"; else GROW_OK. */
static GrowResult check_cadence_bar(const GrowRequest *request, Rational bar_length, long bar, CadenceType cadence,
                                    GrowRefusal *refusal) {
    long entry = -1;
    long previous_bar = -1;
    for (size_t i = 0; i < request->chord_count; i++) {
        const GivenChord *chord = &request->chords[i];
        Rational at = {0, 1};
        long at_bar = -1;
        long long whole;
        if (rational_div(chord->onset, bar_length, &at) == RATIONAL_OK && rational_floor(at, &whole) == RATIONAL_OK) {
            at_bar = (long)whole;
        }
        /* The chords of one bar are one entry of the request's list. */
        if (at_bar != previous_bar) {
            entry++;
            previous_bar = at_bar;
        }
        if (at_bar == bar && !cadence_allows(cadence, chord->numeral)) {
            char path[64];
            snprintf(path, sizeof(path), "/chords/%ld", entry);
            return refuse(refusal, GROW_ERR_BAD_REQUEST, path, "chords bar %ld: %s cannot end a %s; the cadence needs %s", bar,
                          chord->numeral, cadence_label(cadence), cadence_needs(cadence));
        }
    }
    return GROW_OK;
}

GrowResult grow_check_given_cadences(const GrowRequest *request, CadenceType first, GrowRefusal *refusal) {
    Rational bar = grow_bar_length(&request->meter);
    if (request->chord_count == 0 || !request->chords) {
        return GROW_OK;
    }
    /* The cadence fixes the harmony of the last bar of the theme, and of the first part for an antecedent. */
    GrowResult result = check_cadence_bar(request, bar, (long)request->bars - 1, request->cadence, refusal);
    if (result == GROW_OK && request->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        size_t beginning_bars =
            request->construction ? construction_half_bars(&request->pairing, true, request->construction) : request->bars / 2;
        result = check_cadence_bar(request, bar, (long)beginning_bars - 1, first, refusal);
    }
    return result;
}

GrowResult grow_build_plan(const GrowRequest *request, PiecePlan **out) {
    return grow_build_plan_checked(request, out, NULL);
}

GrowResult grow_build_plan_checked(const GrowRequest *request, PiecePlan **out, GrowRefusal *refusal) {
    if (refusal) {
        memset(refusal, 0, sizeof(*refusal));
    }
    if (!request || !request->motive || !out) {
        return refuse(refusal, GROW_ERR_NULL, "", "no request");
    }
    *out = NULL;
    /* An antecedent ends in a weak cadence, HC unless the request says IAC, and the theme in a stronger one. */
    bool antecedent = request->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    CadenceType first = request->first_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? CADENCE_TYPE_HALF : request->first_cadence;
    int archetype = grow_pairing_archetype(&request->pairing);
    /* An accompaniment motive makes the accompaniment: pattern MOTIVE, naming the node "acc". */
    const Motive *accompaniment_motive = request->accompaniment_motive;
    PlanAccompaniment accompaniment = request->accompaniment;
    bool has_accompaniment = request->has_accompaniment || accompaniment_motive;
    if (accompaniment_motive) {
        if (!request->has_accompaniment) {
            memset(&accompaniment, 0, sizeof(accompaniment));
            accompaniment.texture = -1;
        }
        accompaniment.pattern = PLAN_ACCOMPANIMENT_MOTIVE;
        snprintf(accompaniment.motive_node, sizeof(accompaniment.motive_node), "%s", GROW_ACCOMPANIMENT_NAME);
    }
    bool buildable = request->construction
                         ? grow_construction_buildable(&request->meter, request->bars, &request->pairing, request->cadence,
                                                       first, request->construction)
                         : grow_theme_buildable(&request->meter, request->bars, &request->pairing, request->cadence, first);
    GrowResult checked = buildable ? GROW_OK : theme_refusal(request, first, refusal);
    if (checked != GROW_OK) {
        return checked;
    }
    if (request->motive->note_count < 2) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/slots/basic", "the basic motif needs at least two notes");
    }
    if (analysis_key_tonic_pitch_class(&request->key) < 0 || analysis_key_tonic_pitch_class(&request->key) > 11) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/key", "the key has no tonic");
    }
    Rational bar = grow_bar_length(&request->meter);
    size_t basic_units = 0;
    /* The basic motive fills the basic idea: 2 bars of an 8-bar theme, 1 of a 4-bar one. */
    if (!grow_slot_used(&request->pairing, GROW_SLOT_BASIC, &basic_units)) {
        return refuse(refusal, GROW_ERR_MOTIVE_TOO_LONG, "/slots/basic", "this pairing has no basic idea");
    }
    /* A motive as long as the presentation or the antecedent fills that part whole (it has no ideas of its own). */
    bool part_whole = false;
    bool too_long = false;
    size_t basic_bars = request->construction ? request->construction->idea_bars[GROW_IDEA_BASIC]
                                              : idea_bars(request->bars, basic_units);
    size_t part_bars = request->construction ? construction_half_bars(&request->pairing, true, request->construction)
                                             : request->bars / 2;
    if (!grow_motive_fits_idea(request->motive, bar, basic_bars)) {
        Motive basic;
        MotiveRelationStep step;
        part_whole = grow_part_basic_idea(request->motive, bar, part_bars, basic_bars, request->pairing.beginning, &basic, &step);
        too_long = !part_whole;
    }
    /* The slot motives, each a node of the material after m0. */
    size_t slot_count = 0;
    /* The bars the motives take beyond the one each has in the base layout: a motive takes its own bars. */
    size_t extra_bars = grow_motive_layout_bars(request->motive, bar) - 1;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        if (request->slot_motives[slot]) {
            /* A part-length motive leaves no contrasting or repeating idea to give a motive to. */
            if (part_whole && slot == GROW_SLOT_CONTRASTING) {
                return refuse(refusal, GROW_ERR_BAD_REQUEST, "/slots/contrasting",
                              "the basic motif fills the whole %s; leave the contrasting idea out",
                              antecedent ? "antecedent" : "presentation");
            }
            GrowResult valid = slot_motive_valid(request, slot, bar, refusal);
            if (valid != GROW_OK) {
                return valid;
            }
            slot_count++;
            extra_bars += grow_motive_layout_bars(request->slot_motives[slot], bar) - 1;
        }
    }
    /* The motives of single ideas follow the slot motives. */
    size_t idea_count = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        if (request->idea_motives[idea]) {
            GrowPlace place;
            if (part_whole && grow_idea_place(&request->pairing, (GrowIdea)idea, &place) && place == GROW_PLACE_REPETITION) {
                char path[64];
                snprintf(path, sizeof(path), "/ideas/%.50s", grow_idea_name((GrowIdea)idea));
                return refuse(refusal, GROW_ERR_BAD_REQUEST, path,
                              "the basic motif fills the whole %s; leave the contrasting idea out",
                              antecedent ? "antecedent" : "presentation");
            }
            GrowResult valid = idea_motive_valid(request, (GrowIdea)idea, bar, refusal);
            if (valid != GROW_OK) {
                return valid;
            }
            idea_count++;
            extra_bars += grow_motive_layout_bars(request->idea_motives[idea], bar) - 1;
        }
    }
    if (has_accompaniment && !grow_accompaniment_valid(&request->meter, &accompaniment)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/accompaniment",
                      "the accompaniment is not valid here (a pattern, a texture and a range the meter allows)");
    }
    if (accompaniment_motive && (!accompaniment_motive->accompaniment || accompaniment_motive->note_count < 1 ||
                                 rational_cmp(accompaniment_motive->length, bar) > 0)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/accompaniment", "the accompaniment motif is not an accompaniment of up to a bar");
    }
    if (has_accompaniment && request->has_voicing) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/voicing", "a voicing and an accompaniment cannot be given together");
    }
    if (too_long) {
        size_t bars = grow_motive_layout_bars(request->motive, bar);
        return refuse(refusal, GROW_ERR_MOTIVE_TOO_LONG, "/slots/basic",
                      "the motif is %zu bars; a %zu-bar theme takes a motif of up to %zu bars, or of %zu for its "
                      "presentation or antecedent",
                      bars, request->bars, basic_bars, part_bars);
    }
    /* The derived motives take the bars of the longest motive given: the section's own and the four of the ideas. */
    size_t longest = grow_motive_layout_bars(request->motive, bar);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        size_t bars = request->slot_motives[slot] ? grow_motive_layout_bars(request->slot_motives[slot], bar) : 0;
        longest = bars > longest ? bars : longest;
    }
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        size_t bars = request->idea_motives[idea] ? grow_motive_layout_bars(request->idea_motives[idea], bar) : 0;
        longest = bars > longest ? bars : longest;
    }
    extra_bars += GROW_PLACE_COUNT * (longest - 1);
    /* The fragments' segments and the liquidated ideas of the ending are motives of their own in the material
       (grow_ending.c): three at most for a continuation, two for a cadential ending. */
    size_t segment_bars = request->pairing.ending == PLAN_THEME_ENDING_CONTINUATION
                              ? 3
                              : (request->pairing.ending == PLAN_THEME_ENDING_CADENTIAL ? 2 : 0);
    /* A construction's contrasting ideas shorter than their source are made from shortened copies of it (R-471). */
    segment_bars += request->construction ? 2 * longest : 0;
    size_t material_bars = GROW_MATERIAL_BARS + slot_count + idea_count + extra_bars + segment_bars;

    PiecePlan *plan = plan_create();
    if (!plan) {
        return GROW_ERR_ALLOC;
    }
    PlanSpan theme = {{0, 1}, times(bar, request->bars)};
    PlanSpan material = {theme.length, times(bar, material_bars)};
    PlanSpan form = {{0, 1}, {0, 1}};
    Rational total;
    GrowResult result = from_plan(plan_result_of_rational(rational_add(theme.length, material.length, &total)));
    form.length = total;
    NodeId form_id = PLAN_NODE_NONE;
    NodeId theme_id = PLAN_NODE_NONE;
    NodeId section = PLAN_NODE_NONE;
    NodeId phrase = PLAN_NODE_NONE;
    NodeId idea = PLAN_NODE_NONE;
    PlanLanePayload payload;
    LaneSpanId lane;
    PlanConstraint constraint;

    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "form", &form_id));
    }
    /* The meter and the key are Given over the theme and the material. */
    if (result == GROW_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, form, &payload, &lane));
        result = result == GROW_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == GROW_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.key = request->key;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_KEY, 0, form, &payload, &lane));
        result = result == GROW_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }

    /* The theme: open, with what it is to be. */
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, theme, GROW_THEME_NAME, &theme_id));
    }
    memset(&constraint, 0, sizeof(constraint));
    if (result == GROW_OK) {
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)archetype;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
        constraint.params.theme_pairing = request->pairing;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = request->cadence;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK && antecedent) {
        /* The second cadence of the theme is the antecedent's. */
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = first;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK && request->construction) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CONSTRUCTION;
        constraint.params.construction = *request->construction;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    if (result == GROW_OK && has_accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = accompaniment;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    if (result == GROW_OK && request->has_voicing) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_VOICING;
        constraint.params.voicing = request->voicing;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    /* The material: not part of the piece, holding the motive. */
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, material, GROW_MATERIAL_NAME, &section));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MATERIAL;
        result = from_plan(plan_node_add_constraint(plan, section, &constraint));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, GROW_MATERIAL_NAME, &phrase));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, GROW_MATERIAL_NAME, &idea));
    }
    if (result == GROW_OK) {
        result = from_plan(motive_attach_sourced(plan, idea, material.start, request->motive, GROW_SOURCE_NAME,
                                                 request->source, NULL, NULL));
    }
    /* The slot motives follow m0, each at its own bars. */
    size_t slot_bar = grow_motive_layout_bars(request->motive, bar);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; result == GROW_OK && slot < GROW_SLOT_COUNT; slot++) {
        Rational start;
        if (!request->slot_motives[slot]) {
            continue;
        }
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, slot_bar), &start)));
        slot_bar += grow_motive_layout_bars(request->slot_motives[slot], bar);
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->slot_motives[slot],
                                                             grow_slot_node_name(slot), NULL, NULL))
                                   : result;
    }
    /* The motives of single ideas follow the slot motives, each at its own bars. */
    size_t idea_bar = slot_bar;
    for (int idea_at = 0; result == GROW_OK && idea_at < GROW_IDEA_COUNT; idea_at++) {
        Rational start;
        char name[8];
        if (!request->idea_motives[idea_at]) {
            continue;
        }
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, idea_bar), &start)));
        idea_bar += grow_motive_layout_bars(request->idea_motives[idea_at], bar);
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->idea_motives[idea_at],
                                                             grow_idea_node_name((GrowIdea)idea_at, name, sizeof(name)),
                                                             NULL, NULL))
                                   : result;
    }
    if (result == GROW_OK && accompaniment_motive) {
        Rational start;
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, material_bars - 1), &start)));
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, accompaniment_motive,
                                                             GROW_ACCOMPANIMENT_NAME, NULL, NULL))
                                   : result;
    }
    if (result == GROW_OK && request->chord_count > 0 &&
        plan_give_chords(plan, theme_id, request->chords, request->chord_count) != GIVEN_OK) {
        result = refuse(refusal, GROW_ERR_BAD_REQUEST, "/chords", "the given chords do not fit the theme");
    }
    if (result == GROW_OK) {
        result = grow_check_given_cadences(request, first, refusal);
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, section, PLAN_STATE_REALIZED));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED));
    }
    if (result != GROW_OK) {
        plan_destroy(plan);
        if (refusal && refusal->message[0] == '\0') {
            refuse(refusal, result, "", "the theme plan could not be built");
        }
        return result;
    }
    *out = plan;
    return GROW_OK;
}

NodeId grow_find_source(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, GROW_SOURCE_NAME) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool grow_read_motive(const PiecePlan *plan, NodeId node, const PlanMeter *meter, const AnalysisKey *key, Motive *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
                return false;
            }
            motive_init(out, *meter);
            motive_set_key(out, key, MOTIVE_SKETCH_KEY_GIVEN);
            out->note_count = notes->note_count;
            for (size_t n = 0; n < notes->note_count; n++) {
                out->notes[n].pitch = notes->notes[n].midi;
                out->notes[n].onset = notes->notes[n].offset;
                out->notes[n].duration = notes->notes[n].duration;
            }
            MotiveSketchNote *last = &out->notes[out->note_count - 1];
            return rational_add(last->onset, last->duration, &out->length) == RATIONAL_OK;
        }
    }
    return false;
}

NodeId grow_find_theme(const PiecePlan *plan, bool open_only) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        bool archetype = false;
        bool pairing = false;
        if (plan_node_kind(plan, id) != PLAN_NODE_SECTION ||
            (open_only && plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED)) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            PlanConstraintKind kind = plan_node_constraint_at(plan, id, i)->kind;
            archetype = archetype || kind == PLAN_CONSTRAINT_ARCHETYPE;
            pairing = pairing || kind == PLAN_CONSTRAINT_THEME_PAIRING;
        }
        if (archetype && pairing) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}
