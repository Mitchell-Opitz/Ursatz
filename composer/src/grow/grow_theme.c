#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/hybrid_rules.h>
#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/variant_registry.h>

#include "util/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"

/* How many candidates are drawn for each one asked for before the search is given up. */
#define DRAWS_PER_CANDIDATE 6
/* The most gaps Given material leaves in one idea or one subphrase for one lane. */
#define GROW_MAX_GAPS 16

/* ------------------------------------------------------------- the plan */

/* The theme section, the lanes under it, and the motive m0. False when the target is not one grow-theme grows. */
typedef GrowThemeRequest ThemeRequest;

/* The most key spans read before a section: the ones of the piece up to its end, with a few to spare. */
#define MAX_KEY_SPANS 32

/*
 * The key lane over the section (R-069): one key, or the key it starts in and a second key from its half; and, when the
 * key lane has a span that ends where the section starts, the key before it. Spans are contiguous. False when the
 * lane does not tile the section that way.
 */
bool grow_read_keys(const PiecePlan *plan, GrowThemeRequest *request) {
    LaneSpanId spans[MAX_KEY_SPANS];
    Rational end;
    Rational half;
    PlanSpan up_to = {{0, 1}, {0, 1}};
    if (plan_span_end(request->span, &end) != PLAN_OK || rational_make(end.num, end.den, &up_to.length) != RATIONAL_OK ||
        rational_div_int(request->span.length, 2, &half) != RATIONAL_OK ||
        rational_add(request->span.start, half, &half) != RATIONAL_OK) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, up_to, spans, MAX_KEY_SPANS);
    if (found == 0 || found > MAX_KEY_SPANS) {
        return false;
    }
    size_t at = found;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, spans[i]);
        Rational span_end;
        if (plan_span_end(span, &span_end) == PLAN_OK && rational_cmp(span.start, request->span.start) <= 0 &&
            rational_cmp(request->span.start, span_end) < 0) {
            at = i;
        }
    }
    if (at == found) {
        return false;
    }
    PlanSpan first = plan_lane_span_span(plan, spans[at]);
    Rational first_end;
    if (plan_span_end(first, &first_end) != PLAN_OK) {
        return false;
    }
    request->key = plan_lane_span_payload(plan, spans[at])->key;
    if (at > 0 && rational_cmp(first.start, request->span.start) == 0) {
        request->has_start_change = true;
        request->from_key = plan_lane_span_payload(plan, spans[at - 1])->key;
    }
    if (rational_cmp(first_end, end) >= 0) {
        return at + 1 == found || rational_cmp(plan_lane_span_span(plan, spans[at + 1]).start, end) >= 0;
    }
    /* A second span starts at the half and holds the rest of the section. */
    if (at + 1 >= found || rational_cmp(first_end, half) != 0) {
        return false;
    }
    PlanSpan second = plan_lane_span_span(plan, spans[at + 1]);
    Rational second_end;
    if (rational_cmp(second.start, half) != 0 || plan_span_end(second, &second_end) != PLAN_OK ||
        rational_cmp(second_end, end) < 0) {
        return false;
    }
    request->has_end_change = true;
    request->end_key = plan_lane_span_payload(plan, spans[at + 1])->key;
    return true;
}

/* Whether the section starts in the first key of the plan, the home key. */
static bool starts_in_home_key(const PiecePlan *plan, const GrowThemeRequest *request) {
    LaneSpanId spans[MAX_KEY_SPANS];
    Rational end;
    PlanSpan whole = {{0, 1}, {0, 1}};
    if (plan_span_end(request->span, &end) != PLAN_OK || rational_make(end.num, end.den, &whole.length) != RATIONAL_OK ||
        plan_lane_find(plan, PLAN_LANE_KEY, 0, whole, spans, MAX_KEY_SPANS) == 0) {
        return false;
    }
    const AnalysisKey *home = &plan_lane_span_payload(plan, spans[0])->key;
    return analysis_key_tonic_pitch_class(home) == analysis_key_tonic_pitch_class(&request->key) &&
           home->mode == request->key.mode;
}

static bool read_request(const PiecePlan *plan, FillTarget target, ThemeRequest *out) {
    ThemeRequest request;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0) {
        return false;
    }
    /* A section that returns an earlier one is form-return's, not grown afresh; a held one is the seam stage's. */
    if (form_return_handles(plan, target.id) || plan_node_has_constraint(plan, target.id, PLAN_CONSTRAINT_HOLD)) {
        return false;
    }
    request.section = target.id;
    request.span = plan_node_span(plan, target.id);
    bool archetype = false;
    bool pairing = false;
    bool cadence = false;
    PlanArchetype requested = PLAN_ARCHETYPE_SENTENCE;
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE) {
            archetype = true;
            requested = constraint->params.archetype;
        } else if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            pairing = true;
            request.pairing = constraint->params.theme_pairing;
        } else if (constraint->kind == PLAN_CONSTRAINT_CADENCE && !cadence) {
            /* The first cadence is the theme's; a second is the weak one an antecedent ends in. */
            cadence = true;
            request.cadence = constraint->params.cadence;
        } else if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            request.has_first = true;
            request.first_cadence = constraint->params.cadence;
        } else if (constraint->kind == PLAN_CONSTRAINT_ACCOMPANIMENT) {
            request.has_accompaniment = true;
            request.accompaniment = constraint->params.accompaniment;
        } else if (constraint->kind == PLAN_CONSTRAINT_VOICING) {
            request.has_voicing = true;
            request.voicing = constraint->params.voicing;
        } else if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && !request.has_motive_ref) {
            request.has_motive_ref = true;
            request.motive_ref = constraint->params.motive;
        } else if (constraint->kind == PLAN_CONSTRAINT_PEDAL) {
            request.pedal = true;
        } else if (constraint->kind == PLAN_CONSTRAINT_CONSTRUCTION && !request.has_construction) {
            request.has_construction = true;
            request.construction = constraint->params.construction;
        }
    }
    LaneSpanId lane[2];
    if (!archetype || !pairing || !cadence || grow_pairing_archetype(&request.pairing) != (int)requested ||
        (request.pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) != request.has_first ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, request.span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), request.span)) {
        return false;
    }
    request.meter = plan_lane_span_payload(plan, lane[0])->meter;
    if (!grow_read_keys(plan, &request)) {
        return false;
    }
    request.bar = grow_bar_length(&request.meter);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        request.slot_node[slot] = grow_find_slot_node(plan, slot);
    }
    /* The ideas with a motive of their own, by their place under the pairing. */
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        request.idea_node[place] = PLAN_NODE_NONE;
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place;
        if (grow_idea_place(&request.pairing, (GrowIdea)idea, &place)) {
            request.idea_node[place] = grow_find_idea_node(plan, (GrowIdea)idea);
        }
    }
    request.source = request.has_motive_ref ? request.motive_ref.motive : grow_find_source(plan);
    if (request.source == PLAN_NODE_NONE || !grow_cells_per_bar(&request.meter, &request.cells_per_bar) ||
        request.bar.num == 0) {
        return false;
    }
    /* The theme is 4 or 8 whole bars, or 3 to 16 with a construction (R-468). */
    Rational bars = {0, 1};
    size_t count = 0;
    size_t most = request.has_construction ? 16 : 8;
    while (rational_cmp(bars, request.span.length) < 0 && count <= most) {
        if (rational_add(bars, request.bar, &bars) != RATIONAL_OK) {
            return false;
        }
        count++;
    }
    if (rational_cmp(bars, request.span.length) != 0 ||
        (request.has_construction ? (count < 3 || count > 16) : (count != 4 && count != 8))) {
        return false;
    }
    request.bars = count;
    GrowLayout layout;
    if (!grow_layout_of(&request, &layout)) {
        return false;
    }
    PlanSpan first_half = request.span;
    first_half.length = layout.beginning;
    static const PlanLaneKind GIVEN_LANES[] = {PLAN_LANE_MELODY, PLAN_LANE_HARMONY, PLAN_LANE_BASS};
    for (size_t l = 0; l < sizeof(GIVEN_LANES) / sizeof(GIVEN_LANES[0]); l++) {
        LaneSpanId held[GROW_MAX_GAPS];
        size_t found = plan_lane_find(plan, GIVEN_LANES[l], 0, first_half, held, GROW_MAX_GAPS);
        for (size_t i = 0; i < found && i < GROW_MAX_GAPS; i++) {
            request.given_first_half = request.given_first_half || plan_lane_span_state(plan, held[i]) == PLAN_STATE_GIVEN;
        }
    }
    /* A pedal is kept only under a presentation that opens in the home key, with no Given material in its first half. */
    request.pedal = request.pedal && request.pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
                    !request.has_start_change && !request.given_first_half && starts_in_home_key(plan, &request);
    *out = request;
    return true;
}

static bool grow_applies(const PiecePlan *plan, FillTarget target, void *context) {
    ThemeRequest request;
    (void)context;
    return read_request(plan, target, &request) || grow_free_applies(plan, target);
}

/* ------------------------------------------------------------ the spec */

Rational grow_scaled(Rational unit, size_t count) {
    Rational out = {0, 1};
    if (rational_mul_int(unit, (long long)count, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

Rational grow_unit(const GrowThemeRequest *request) {
    Rational unit = {0, 1};
    if (rational_make(request->bar.num * (long)request->bars, request->bar.den * 8, &unit) != RATIONAL_OK) {
        unit.num = 0;
        unit.den = 1;
    }
    return unit;
}

static int direction(int semitones) {
    return semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
}

bool grow_same_contour(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 1; i < a->note_count; i++) {
        if (direction(motive_pitch(a, i) - motive_pitch(a, i - 1)) !=
            direction(motive_pitch(b, i) - motive_pitch(b, i - 1))) {
            return false;
        }
    }
    return true;
}

int grow_phrase_beginning(PlanThemeBeginning beginning) {
    const ThemeShapeHalf *half = theme_shape_beginning(beginning);
    PhraseBeginning found;
    return half && phrase_beginning_from_name(half->library_name, &found) ? (int)found : PHRASE_BEGINNING_COUNT;
}

int grow_phrase_ending(PlanThemeEnding ending) {
    const ThemeShapeHalf *half = theme_shape_ending(ending);
    PhraseEnding found;
    return half && phrase_ending_from_name(half->library_name, &found) ? (int)found : PHRASE_ENDING_COUNT;
}

int grow_pairing_archetype(const PlanThemePairing *pairing) {
    PhraseLevelForm form;
    if (!standard_form_pairing_form(grow_phrase_beginning(pairing->beginning), grow_phrase_ending(pairing->ending),
                                    &form)) {
        return -1;
    }
    switch (form) {
    case PHRASE_LEVEL_FORM_SENTENCE:
        return PLAN_ARCHETYPE_SENTENCE;
    case PHRASE_LEVEL_FORM_PERIOD:
        return PLAN_ARCHETYPE_PERIOD;
    case PHRASE_LEVEL_FORM_HYBRID:
        return PLAN_ARCHETYPE_HYBRID;
    default:
        return -1;
    }
}

bool grow_idea_dev(const GrowThemeSpec *spec, GrowPlace idea, MotiveRelationStep *dev) {
    memset(dev, 0, sizeof(*dev));
    if (grow_idea_own(spec, idea) || spec->has_liquidated[idea]) {
        /* The idea's own motive, or the slot's own motive beginning the slot, is the idea as it is. */
        dev->kind = MOTIVE_RELATION_EXACT;
        return true;
    }
    switch (idea) {
    case GROW_PLACE_REPETITION: /* also the contrasting idea of the other beginnings */
        if (spec->pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION) {
            *dev = spec->contrast;
        } else if (spec->repetition == GROW_REPEAT_DOMINANT_FORM) {
            dev->kind = MOTIVE_RELATION_DOMINANT_FORM;
        } else if (spec->repetition == GROW_REPEAT_SEQUENCE) {
            /* One step up in the key's scale, the chord moving with it (grow_beginning.c). */
            dev->kind = MOTIVE_RELATION_TRANSPOSE;
            dev->value[0] = 1;
            dev->value[1] = 1;
        } else {
            dev->kind = MOTIVE_RELATION_EXACT;
        }
        return true;
    case GROW_PLACE_FRAGMENT_1:
    case GROW_PLACE_FRAGMENT_2: {
        size_t second = idea == GROW_PLACE_FRAGMENT_2;
        if (grow_idea_segment(spec, idea, NULL) && spec->fragment_cut[second] > 0) {
            /* A segment cut to its leading notes (R-462). */
            dev->kind = MOTIVE_RELATION_FRAGMENT;
            dev->value[0] = MOTIVE_FRAGMENT_LEADING;
            dev->value[1] = (long)spec->fragment_cut[second];
            dev->value[2] = 1;
            return true;
        }
        if (grow_idea_segment(spec, idea, NULL) && spec->fragment_diminished[second]) {
            /* A whole segment in diminution, the registry's 1:2 (R-463). */
            dev->kind = MOTIVE_RELATION_DIMINISH;
            return true;
        }
        if (grow_idea_segment(spec, idea, NULL)) {
            /* A segment is the fragment as it is, or in sequence one step down in the key's scale. */
            dev->kind = second && spec->fragment_sequence ? MOTIVE_RELATION_TRANSPOSE : MOTIVE_RELATION_EXACT;
            if (dev->kind == MOTIVE_RELATION_TRANSPOSE) {
                dev->value[0] = 1;
                dev->value[1] = -1;
            }
            return true;
        }
        if (spec->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT) {
            if (second) {
                *dev = spec->consequent_contrast;
            } else {
                dev->kind = MOTIVE_RELATION_EXACT;
            }
        } else if (spec->pairing.ending == PLAN_THEME_ENDING_CADENTIAL) {
            /* A cadential idea is its liquidated residue (has_liquidated above) or its own motive: no note liquidation. */
            return false;
        } else {
            dev->kind = MOTIVE_RELATION_FRAGMENT;
            dev->value[0] = MOTIVE_FRAGMENT_LEADING;
            dev->value[1] = (long)spec->fragment_notes[second];
            dev->value[2] = 1;
        }
        return true;
    }
    case GROW_PLACE_CADENTIAL:
        /* The cadential idea is its liquidated residue (has_liquidated above) or its own motive: no note liquidation. */
        return false;
    default:
        return false;
    }
}

void grow_idea_ref(const GrowThemeSpec *spec, GrowPlace idea, NodeId source, PlanMotiveRef *out) {
    MotiveRelationStep dev;
    memset(out, 0, sizeof(*out));
    out->motive = source;
    out->steps = spec->has_liquidated[idea] ? spec->liquidation_steps[idea] : 0;
    const MotiveSketchFeature *segment;
    if (grow_idea_segment(spec, idea, &segment)) {
        snprintf(out->segment, sizeof(out->segment), "%s", segment->label);
    } else if (spec->has_liquidated[idea] && spec->liquidation_segment[idea] < spec->segment_count) {
        /* A residue names the segment it keeps (R-345). */
        snprintf(out->segment, sizeof(out->segment), "%s", spec->segments[spec->liquidation_segment[idea]].label);
    }
    if (grow_idea_dev(spec, idea, &dev)) {
        out->has_step = true;
        out->step = dev;
        /* The plan names the relation and its parameters; the shortest result is the operators' (motive_develop.h). */
        out->step.value[2] = dev.kind == MOTIVE_RELATION_FRAGMENT || dev.kind == MOTIVE_RELATION_LIQUIDATE ? 0 : dev.value[2];
    }
}

bool grow_idea_segment(const GrowThemeSpec *spec, GrowPlace idea, const MotiveSketchFeature **segment) {
    if (spec->pairing.ending != PLAN_THEME_ENDING_CONTINUATION ||
        (idea != GROW_PLACE_FRAGMENT_1 && idea != GROW_PLACE_FRAGMENT_2)) {
        return false;
    }
    size_t k = idea == GROW_PLACE_FRAGMENT_2;
    if (!spec->fragment_from_segment[k]) {
        return false;
    }
    if (segment) {
        *segment = &spec->segments[spec->fragment_segment[k]];
    }
    return true;
}

const Motive *grow_idea_source(const GrowThemeSpec *spec, GrowPlace idea) {
    if (grow_idea_segment(spec, idea, NULL)) {
        return &spec->fragment_motive[idea == GROW_PLACE_FRAGMENT_2];
    }
    if (spec->has_liquidated[idea]) {
        return &spec->liquidated[idea];
    }
    if (spec->has_shortened[idea]) {
        return &spec->shortened[idea];
    }
    if (idea != GROW_PLACE_BASIC && spec->has_idea_source[idea]) {
        return &spec->idea_source[idea];
    }
    GrowSlot slot = grow_idea_slot(&spec->pairing, idea, NULL);
    return slot != GROW_SLOT_BASIC && spec->has_slot_source[slot] ? &spec->slot_source[slot] : &spec->source;
}

NodeId grow_idea_node(const GrowBuild *build, GrowPlace idea) {
    if (grow_idea_segment(build->spec, idea, NULL)) {
        return build->segment_node[idea == GROW_PLACE_FRAGMENT_2];
    }
    if (build->spec->has_liquidated[idea]) {
        return build->liquid_node[idea];
    }
    if (build->spec->has_shortened[idea]) {
        return build->short_node[idea];
    }
    if (idea != GROW_PLACE_BASIC && build->request->idea_node[idea] != PLAN_NODE_NONE) {
        return build->request->idea_node[idea];
    }
    GrowSlot slot = grow_idea_slot(&build->spec->pairing, idea, NULL);
    return slot != GROW_SLOT_BASIC && build->spec->has_slot_source[slot] ? build->request->slot_node[slot]
                                                                         : build->source;
}

bool grow_idea_develop(const GrowThemeSpec *spec, GrowPlace idea, Motive *out) {
    PlanMotiveRef ref;
    memset(&ref, 0, sizeof(ref));
    ref.has_step = grow_idea_dev(spec, idea, &ref.step);
    return ref.has_step && grow_develop_motive(grow_idea_source(spec, idea), &ref, out);
}

size_t grow_chords_before_last(const GrowThemeRequest *request, PlanHarmonyShapeKind shape, CadenceType cadence) {
    /* A tonic, the predominants the mode has (two in a major key, one in a minor), a dominant unless the cadence is half. */
    return (shape == PLAN_HARMONY_SHAPE_CADENTIAL ? 0 : 1) + (request->key.mode == KEY_STRUCTURE_MODE_MINOR ? 1 : 2) +
           (cadence == CADENCE_TYPE_HALF ? 0 : 1);
}

bool grow_split_possible(const GrowThemeRequest *request, size_t half_bars, PlanHarmonyShapeKind shape,
                         CadenceType cadence) {
    size_t chords = grow_chords_before_last(request, shape, cadence);
    size_t cells = half_bars * request->cells_per_bar;
    /* The last chord is the whole last bar. */
    return cells <= request->cells_per_bar || cells - request->cells_per_bar <= chords * request->cells_per_bar;
}

/* Whether two developments are the same kind of change by the same amount. */
static bool same_kind(const MotiveRelationStep *a, const MotiveRelationStep *b) {
    return a->kind == b->kind && a->value[0] == b->value[0] && a->value[1] == b->value[1];
}

/* The most developments of m0 one contrasting idea is chosen among: inversions, interval changes everywhere and one. */
#define CONTRAST_MAX 160
/* The widest the notes of a contrasting idea may go, so the melody has room to place it. */
#define CONTRAST_LOW 58
#define CONTRAST_HIGH 84

/* Whether the last note of a development lands on the pitch class asked for (any when want is negative). */
static bool lands_on(const Motive *result, int want) {
    return want < 0 || ((motive_pitch(result, result->note_count - 1) % 12) + 12) % 12 == want;
}

bool grow_shorten_source(GrowThemeSpec *spec, GrowPlace place) {
    const Motive *from = grow_idea_source(spec, place);
    Rational room = spec->layout.length[place];
    if (rational_cmp(from->length, room) <= 0) {
        return true;
    }
    for (size_t notes = from->note_count - 1; notes >= 2; notes--) {
        MotiveRelationStep step;
        Motive result;
        memset(&step, 0, sizeof(step));
        step.kind = MOTIVE_RELATION_FRAGMENT;
        step.value[0] = MOTIVE_FRAGMENT_LEADING;
        step.value[1] = (long)notes;
        step.value[2] = 1;
        if (motive_develop(from, &step, &result) == MOTIVE_DEV_OK && rational_cmp(result.length, room) <= 0) {
            spec->shortened[place] = result;
            spec->has_shortened[place] = true;
            return true;
        }
    }
    return false;
}

bool grow_draw_contrast(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec,
                        GrowPlace place, const Motive *source, int want_pc, const MotiveRelationStep *avoid,
                        MotiveRelationStep *out) {
    (void)request;
    Rational room = spec->layout.length[place];
    size_t intervals = source->note_count - 1;
    MotiveRelationStep fits[CONTRAST_MAX];
    MotiveRelationStep exact[CONTRAST_MAX];
    size_t fit_count = 0;
    size_t exact_count = 0;
    bool diatonic = grow_motive_in_range(source, 0, 127, true);

    /* In the key's scale first when the motive is; if no development of it is, in range only. */
    for (int pass = 0; pass < 2 && fit_count == 0; pass++) {
    bool in_scale = pass == 0 && diatonic;
    for (int kind = 0; kind < 3; kind++) {
        /* Mirrored in the key's steps about a note near the first, so the contour turns over; every interval
           widened or narrowed; or the one interval changed. */
        int first = motive_pitch(source, 0);
        int low = kind == 0 ? first - 2 : (kind == 1 ? -2 : 1);
        int high = kind == 0 ? first + 2 : (kind == 1 ? 2 : (int)intervals);
        for (int value = low; value <= high; value++) {
            for (int amount = (kind == 2 ? -4 : 0); amount <= (kind == 2 ? 4 : 0); amount++) {
                MotiveRelationStep dev;
                Motive result;
                memset(&dev, 0, sizeof(dev));
                if (kind == 0) {
                    dev.kind = MOTIVE_RELATION_INVERT;
                    dev.value[0] = value;
                    dev.value[1] = 1;
                } else {
                    dev.kind = MOTIVE_RELATION_INTERVAL_CHANGE;
                    dev.value[0] = kind == 1 ? 0 : value;
                    dev.value[1] = kind == 1 ? value : amount;
                }
                if ((kind != 0 && dev.value[1] == 0) ||
                    motive_develop(source, &dev, &result) != MOTIVE_DEV_OK || grow_same_contour(source, &result) ||
                    rational_cmp(result.length, room) > 0 || (avoid && same_kind(avoid, &dev))) {
                    continue;
                }
                /* In range, and in the key's scale when the motive is, as the motives stage takes a form (grow_form.c). */
                if (!grow_motive_in_range(&result, CONTRAST_LOW, CONTRAST_HIGH, in_scale)) {
                    continue;
                }
                if (fit_count < CONTRAST_MAX) {
                    fits[fit_count++] = dev;
                }
                if (lands_on(&result, want_pc) && exact_count < CONTRAST_MAX) {
                    exact[exact_count++] = dev;
                }
            }
        }
    }
    }
    /* One that lands where the cadence needs it, if there is one. */
    const MotiveRelationStep *pool = exact_count > 0 ? exact : fits;
    size_t count = exact_count > 0 ? exact_count : fit_count;
    if (count == 0) {
        return false;
    }
    *out = pool[fill_rng_below(rng, count)];
    return true;
}

/*
 * The section's own motive: the section's MOTIVE_REF made concrete, every parameter it leaves open drawn from rng, so
 * the motive operators propose exactly this one (an inversion about the first note, a diminution or augmentation by
 * half or double, or by the next ratio the meter can write, an interval changed by a semitone or two, a tonal transposition by up to three steps). The result
 * must fit the basic idea (two bars of eight, one of four) and have two notes or more. A diminution or augmentation no usual ratio can write in the meter is
 * made an interval change instead. False when the ref asks for a transform a basic idea is not made from (a fragment,
 * a liquidation) or the draw does not come out.
 */
#define SECTION_MOTIVE_DRAWS 8

static const int AUGMENT_RATIOS[][2] = {{2, 1}, {3, 2}, {4, 3}};
static const int DIMINISH_RATIOS[][2] = {{1, 2}, {2, 3}, {3, 4}};
#define RHYTHM_RATIOS 3

/* Whether the motive has a triplet: a note's onset or duration, the pickup or the length with a denominator divisible
 * by 3. */
static bool has_triplets(const Motive *motive) {
    if (motive->pickup.den % 3 == 0 || motive->length.den % 3 == 0) {
        return true;
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        if (motive->notes[i].onset.den % 3 == 0 || motive->notes[i].duration.den % 3 == 0) {
            return true;
        }
    }
    return false;
}

/* Whether a ratio's result is one to use for m0: a motive with triplets keeps them, so a result without any is passed
 * over (a binary m0 accepts every result). */
static bool keeps_triplets(const Motive *m0, const Motive *result) {
    return !has_triplets(m0) || has_triplets(result);
}

/* Whether any usual ratio augments or diminishes the motive into a rhythm the meter can write. */
static bool rhythm_writable(const Motive *m0, bool augment) {
    MotiveRelationStep dev;
    Motive result;
    memset(&dev, 0, sizeof(dev));
    dev.kind = augment ? MOTIVE_RELATION_AUGMENT : MOTIVE_RELATION_DIMINISH;
    for (size_t r = 0; r < RHYTHM_RATIOS; r++) {
        dev.value[0] = (augment ? AUGMENT_RATIOS : DIMINISH_RATIOS)[r][0];
        dev.den[0] = (augment ? AUGMENT_RATIOS : DIMINISH_RATIOS)[r][1];
        if (motive_develop(m0, &dev, &result) == MOTIVE_DEV_OK && keeps_triplets(m0, &result)) {
            return true;
        }
    }
    return false;
}

static bool draw_section_motive(const ThemeRequest *request, FillRng *rng, const Motive *m0, GrowThemeSpec *spec) {
    PlanMotiveRef rhythm_fallback;
    const PlanMotiveRef *asked = &request->motive_ref;
    if (asked->has_step && (asked->step.kind == MOTIVE_RELATION_AUGMENT || asked->step.kind == MOTIVE_RELATION_DIMINISH) &&
        asked->step.value[0] == 0 && !rhythm_writable(m0, asked->step.kind == MOTIVE_RELATION_AUGMENT)) {
        /* No usual ratio can be written in the meter (a whole bar of 3/4 has no diminution without triplets): the
           section still sounds unlike m0, with an interval changed. */
        rhythm_fallback = *asked;
        memset(&rhythm_fallback.step, 0, sizeof(rhythm_fallback.step));
        rhythm_fallback.step.kind = MOTIVE_RELATION_INTERVAL_CHANGE;
        asked = &rhythm_fallback;
    }
    MotiveRelationStep given;
    memset(&given, 0, sizeof(given));
    if (asked->has_step) {
        given = asked->step;
    }
    for (size_t draw = 0; draw < SECTION_MOTIVE_DRAWS; draw++) {
        PlanMotiveRef ref = *asked;
        MotiveRelationStep dev;
        Motive result;
        memset(&dev, 0, sizeof(dev));
        ref.has_step = true;
        switch (given.kind) {
        case MOTIVE_RELATION_EXACT:
            dev.kind = MOTIVE_RELATION_EXACT;
            break;
        case MOTIVE_RELATION_TRANSPOSE: {
            dev.kind = MOTIVE_RELATION_TRANSPOSE;
            bool tonal = given.value[0] != 0 || given.value[1] == 0;
            dev.value[0] = tonal ? 1 : 0;
            if (tonal) {
                int steps = (int)fill_rng_below(rng, 3) + 1;
                dev.value[1] = given.value[1] != 0 ? given.value[1] : (fill_rng_below(rng, 2) == 0 ? steps : -steps);
            } else {
                dev.value[1] = given.value[1];
            }
            break;
        }
        case MOTIVE_RELATION_INVERT:
            dev.kind = MOTIVE_RELATION_INVERT;
            dev.value[1] = given.value[1];
            dev.value[0] = given.value[0] >= 0 ? given.value[0] : motive_pitch(m0, 0);
            break;
        case MOTIVE_RELATION_RETROGRADE:
            dev.kind = MOTIVE_RELATION_RETROGRADE;
            break;
        case MOTIVE_RELATION_INTERVAL_CHANGE:
            dev.kind = MOTIVE_RELATION_INTERVAL_CHANGE;
            dev.value[2] = given.value[2];
            if (given.value[1] != 0) {
                dev.value[0] = given.value[0];
                dev.value[1] = given.value[1];
            } else {
                int amount = (int)fill_rng_below(rng, 2) + 1;
                dev.value[0] = (long)fill_rng_below(rng, m0->note_count - 1) + 1;
                dev.value[1] = fill_rng_below(rng, 2) == 0 ? amount : -amount;
            }
            break;
        case MOTIVE_RELATION_AUGMENT:
        case MOTIVE_RELATION_DIMINISH: {
            /* The ratio asked for, else the first of the usual ones the meter can write. */
            bool augment = given.kind == MOTIVE_RELATION_AUGMENT;
            const int(*ratios)[2] = augment ? AUGMENT_RATIOS : DIMINISH_RATIOS;
            dev.kind = given.kind;
            for (size_t r = 0; r < RHYTHM_RATIOS; r++) {
                dev.value[0] = given.value[0] > 0 ? given.value[0] : ratios[r][0];
                dev.den[0] = given.den[0] > 0 ? given.den[0] : ratios[r][1];
                if (given.value[0] > 0 ||
                    (motive_develop(m0, &dev, &result) == MOTIVE_DEV_OK && keeps_triplets(m0, &result))) {
                    break;
                }
            }
            break;
        }
        default:
            return false;
        }
        ref.step = dev;
        if (motive_develop(m0, &dev, &result) != MOTIVE_DEV_OK || result.note_count < 2 ||
            rational_cmp(result.length, grow_scaled(request->bar, spec->layout.basic_bars)) > 0) {
            continue;
        }
        spec->has_section_motive = true;
        spec->section_ref = ref;
        spec->source = result;
        return true;
    }
    return false;
}

/* A motive as long as the first part: its leading idea-length bars are the basic idea (see grow_part_basic_idea). */
static bool draw_part_whole(const ThemeRequest *request, GrowThemeSpec *spec) {
    Motive basic;
    MotiveRelationStep step;
    if (!grow_part_basic_idea(&spec->source, request->bar, spec->layout.beginning_bars, spec->layout.basic_bars,
                              request->pairing.beginning, &basic, &step)) {
        return false;
    }
    memset(&spec->section_ref, 0, sizeof(spec->section_ref));
    spec->section_ref.motive = request->source;
    spec->section_ref.has_step = true;
    spec->section_ref.step = step;
    spec->has_section_motive = true;
    spec->part_whole = true;
    spec->source = basic;
    return true;
}

_Static_assert(PLAN_SEGMENT_LABEL_MAX == MOTIVE_FEATURE_LABEL_MAX, "a plan's segment label holds a feature label");

/* The bars a derived motive node takes in the material: those of the longest motive the theme is made from. */
static size_t derived_bars(const GrowThemeSpec *spec, Rational bar) {
    size_t bars = grow_motive_layout_bars(&spec->source, bar);
    for (size_t k = 0; k < GROW_SLOT_COUNT; k++) {
        size_t slot_bars = spec->has_slot_source[k] ? grow_motive_layout_bars(&spec->slot_source[k], bar) : 0;
        bars = slot_bars > bars ? slot_bars : bars;
    }
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        size_t idea_bars = spec->has_idea_source[k] ? grow_motive_layout_bars(&spec->idea_source[k], bar) : 0;
        bars = idea_bars > bars ? idea_bars : bars;
    }
    return bars;
}

static size_t count_bars(PlanSpan material_span, Rational bar);

/* Whether the material has bars after the motives for the fragments' segments (a plan made before them may not). */
static bool material_has_room(const PiecePlan *plan, const ThemeRequest *request, const GrowThemeSpec *spec) {
    NodeId material = plan_node_parent(plan, request->source);
    PlanSpan span = plan_node_span(plan, material);
    size_t wanted = ((spec->has_section_motive ? 1 : 0) + GROW_PLACE_COUNT - 1) * derived_bars(spec, request->bar) + GROW_MADE_NODES;
    /* An extension that is a repeat or an addition has a motive of its own, a bar of the material at least (R-468). */
    for (size_t k = 0; k < spec->layout.extension_count; k++) {
        wanted += spec->layout.extensions[k].kind == PLAN_EXTENSION_REPEAT || spec->layout.extensions[k].kind == PLAN_EXTENSION_ADDITION;
    }
    return grow_first_free_bar(plan, material, span, request->bar, wanted) + wanted <= count_bars(span, request->bar);
}

/*
 * Whether the material has the bars apply takes for the motives the drawn spec makes: the same count apply uses, the
 * fragments' segments and a liquidation for each idea that has one, which the estimate before the draw (GROW_MADE_NODES) can
 * fall short of.
 */
static bool made_nodes_fit(const PiecePlan *plan, const ThemeRequest *request, const GrowThemeSpec *spec) {
    NodeId material = plan_node_parent(plan, request->source);
    PlanSpan span = plan_node_span(plan, material);
    size_t made = (spec->fragment_from_segment[0] ? 1 : 0) + (spec->fragment_from_segment[1] ? 1 : 0);
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        made += spec->has_liquidated[k] ? 1 : 0;
        made += spec->has_shortened[k] ? grow_motive_layout_bars(&spec->shortened[k], request->bar) : 0;
    }
    made += grow_extended_bars(spec, request->bar);
    size_t wanted = ((spec->has_section_motive ? 1 : 0) + GROW_PLACE_COUNT - 1) * derived_bars(spec, request->bar) + made;
    return grow_first_free_bar(plan, material, span, request->bar, wanted) + wanted <= count_bars(span, request->bar);
}

/*
 * The segments of the theme's motive (R-311): the features of the compose run's basic motive when the motive is its first
 * notes (a section's own motive is made from m0 and is not), else a proposal from the motive's bars and rhythmic groups.
 * The run's segments only: nothing is written back to the motif.
 */
static void draw_segments(const GrowContext *shared, GrowThemeSpec *spec) {
    Motive motive = spec->source;
    const Motive *basic = shared ? shared->basic : NULL;
    bool same = basic && !spec->has_section_motive && basic->note_count >= motive.note_count;
    for (size_t i = 0; same && i < motive.note_count; i++) {
        same = motive_pitch(basic, i) == motive_pitch(&motive, i) &&
               rational_cmp(basic->notes[i].onset, motive.notes[i].onset) == 0 &&
               rational_cmp(basic->notes[i].duration, motive.notes[i].duration) == 0;
    }
    if (same) {
        motive.pickup = basic->pickup;
        motive.feature_count = 0;
        for (size_t f = 0; f < basic->feature_count; f++) {
            if (basic->features[f].last < motive.note_count) {
                motive.features[motive.feature_count++] = basic->features[f];
            }
        }
    }
    MotiveAnalysis analysis;
    spec->segment_count = 0;
    if (!motive_analyze(&motive, &analysis)) {
        return;
    }
    spec->segment_count = analysis.segment_count < MOTIVE_MAX_FEATURES ? analysis.segment_count : MOTIVE_MAX_FEATURES;
    memcpy(spec->segments, analysis.segments, spec->segment_count * sizeof(spec->segments[0]));
}

/*
 * m0 as the motive-forms read it (grow_form.c): with the basic motive's harmony, features, pickup and role when m0 is the
 * basic motive itself, else the notes alone; its segments (its features, or a proposal) become its features, so that
 * liquidate-features liquidates segments.
 */
static void draw_liquid_base(const GrowContext *shared, GrowThemeSpec *spec) {
    const Motive *basic = shared ? shared->basic : NULL;
    Motive base = spec->source;
    bool same = basic && !spec->has_section_motive && basic->note_count == base.note_count;
    for (size_t i = 0; same && i < base.note_count; i++) {
        same = motive_pitch(basic, i) == motive_pitch(&base, i) &&
               rational_cmp(basic->notes[i].onset, base.notes[i].onset) == 0 &&
               rational_cmp(basic->notes[i].duration, base.notes[i].duration) == 0;
    }
    if (same) {
        memcpy(base.chords, basic->chords, sizeof(base.chords));
        base.chord_count = basic->chord_count;
        memcpy(base.features, basic->features, sizeof(base.features));
        base.feature_count = basic->feature_count;
        base.accompaniment = basic->accompaniment;
        base.pickup = basic->pickup;
    }
    MotiveAnalysis analysis;
    if (!motive_analyze(&base, &analysis)) {
        return;
    }
    base.feature_count = analysis.segment_count < MOTIVE_MAX_FEATURES ? analysis.segment_count : MOTIVE_MAX_FEATURES;
    memcpy(base.features, analysis.segments, base.feature_count * sizeof(base.features[0]));
    spec->liquid_base = base;
    spec->has_liquid_base = true;
}

/* Draws every choice of one candidate from rng. False when the draw does not make a sentence the motive fits. */
static bool draw_spec(const PiecePlan *plan, const ThemeRequest *request, const GrowContext *shared, FillRng *rng,
                      GrowThemeSpec *spec) {
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_PAYLOAD_MAGIC;
    spec->bars = request->bars;
    if (!grow_layout_of(request, &spec->layout)) {
        return false;
    }
    spec->meter = request->meter;
    spec->key = request->key;
    spec->cadence = request->cadence;
    spec->pairing = request->pairing;
    spec->first_cadence = request->first_cadence;
    spec->continuation = request->continuation;
    spec->has_construction = request->has_construction;
    spec->basic_as_written = request->basic_as_written;
    spec->basic_has_chords = shared && shared->basic && shared->basic->chord_count > 0;
    if (!grow_read_motive(plan, request->source, &request->meter, &request->key, &spec->source) ||
        spec->source.note_count < 2) {
        return false;
    }
    size_t basic_bars = spec->layout.basic_bars;
    if (grow_motive_fit(&spec->source, request->bar, basic_bars) == GROW_FIT_LONGER) {
        if (request->has_motive_ref || !draw_part_whole(request, spec)) {
            return false;
        }
    }
    if (request->has_motive_ref) {
        Motive m0 = spec->source;
        if (!draw_section_motive(request, rng, &m0, spec)) {
            return false;
        }
    }
    /* The slots with a motive of their own. */
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        if (request->slot_node[slot] == PLAN_NODE_NONE) {
            continue;
        }
        if (!grow_read_motive(plan, request->slot_node[slot], &request->meter, &request->key,
                              &spec->slot_source[slot]) ||
            spec->slot_source[slot].note_count < 2) {
            return false;
        }
        spec->has_slot_source[slot] = true;
    }
    /* The ideas with a motive of their own. */
    for (size_t place = GROW_PLACE_BASIC + 1; place < GROW_PLACE_COUNT; place++) {
        if (request->idea_node[place] == PLAN_NODE_NONE) {
            continue;
        }
        if (!grow_read_motive(plan, request->idea_node[place], &request->meter, &request->key,
                              &spec->idea_source[place]) ||
            spec->idea_source[place].note_count < 2) {
            return false;
        }
        spec->has_idea_source[place] = true;
    }
    draw_segments(shared, spec);
    draw_liquid_base(shared, spec);
    spec->segment_room = material_has_room(plan, request, spec);
    spec->basic_cells = spec->layout.basic_bars * request->cells_per_bar;
    return grow_beginning_draw(request, rng, spec) && grow_ending_draw(request, rng, spec) &&
           grow_extensions_draw(request, spec) && made_nodes_fit(plan, request, spec);
}

/* Whether two specs make the same motives for their extensions. */
static bool same_extended(const GrowThemeSpec *a, const GrowThemeSpec *b) {
    for (size_t k = 0; k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (a->has_extended[k] != b->has_extended[k] || (a->has_extended[k] && !motive_equal(&a->extended[k], &b->extended[k]))) {
            return false;
        }
    }
    return true;
}

static bool same_spec(const GrowThemeSpec *a, const GrowThemeSpec *b) {
    return a->has_section_motive == b->has_section_motive && motive_equal(&a->source, &b->source) &&
           a->repetition == b->repetition && a->fragment_notes[0] == b->fragment_notes[0] &&
           a->fragment_notes[1] == b->fragment_notes[1] &&
           a->fragment_from_segment[0] == b->fragment_from_segment[0] &&
           a->fragment_from_segment[1] == b->fragment_from_segment[1] &&
           a->fragment_segment[0] == b->fragment_segment[0] && a->fragment_segment[1] == b->fragment_segment[1] &&
           a->fragment_sequence == b->fragment_sequence && a->fragment_cut[0] == b->fragment_cut[0] &&
           a->fragment_cut[1] == b->fragment_cut[1] && a->fragment_diminished[0] == b->fragment_diminished[0] &&
           a->fragment_diminished[1] == b->fragment_diminished[1] &&
           a->liquidation_steps[GROW_PLACE_FRAGMENT_1] == b->liquidation_steps[GROW_PLACE_FRAGMENT_1] &&
           a->liquidation_steps[GROW_PLACE_FRAGMENT_2] == b->liquidation_steps[GROW_PLACE_FRAGMENT_2] &&
           a->liquidation_steps[GROW_PLACE_CADENTIAL] == b->liquidation_steps[GROW_PLACE_CADENTIAL] &&
           a->liquidation_segment[GROW_PLACE_FRAGMENT_1] == b->liquidation_segment[GROW_PLACE_FRAGMENT_1] &&
           a->liquidation_segment[GROW_PLACE_FRAGMENT_2] == b->liquidation_segment[GROW_PLACE_FRAGMENT_2] &&
           a->liquidation_segment[GROW_PLACE_CADENTIAL] == b->liquidation_segment[GROW_PLACE_CADENTIAL] &&
           a->liquidation_head[GROW_PLACE_FRAGMENT_1] == b->liquidation_head[GROW_PLACE_FRAGMENT_1] &&
           a->liquidation_head[GROW_PLACE_FRAGMENT_2] == b->liquidation_head[GROW_PLACE_FRAGMENT_2] &&
           a->liquidation_head[GROW_PLACE_CADENTIAL] == b->liquidation_head[GROW_PLACE_CADENTIAL] &&
           a->compound_ends_on == b->compound_ends_on && same_kind(&a->contrast, &b->contrast) &&
           same_kind(&a->consequent_contrast, &b->consequent_contrast) && same_extended(a, b);
}

static const char *dev_name(const MotiveRelationStep *dev) {
    return dev->kind == MOTIVE_RELATION_INVERT ? "inverted" : "intervals changed";
}

static const char *repetition_name(GrowRepetition repetition) {
    static const char *const NAMES[GROW_REPEAT_COUNT] = {"exact", "dominant form", "sequence"};
    return NAMES[repetition];
}

/* How an idea is liquidated: "head of a", "segments, 2 steps", or "own" for an idea with a motive of its own. */
static void liquidation_label(const GrowThemeSpec *spec, GrowPlace idea, char *out, size_t size) {
    if (!spec->has_liquidated[idea]) {
        snprintf(out, size, "own");
    } else if (spec->liquidation_head[idea]) {
        snprintf(out, size, "head of %s", spec->segments[spec->liquidation_segment[idea]].label);
    } else {
        snprintf(out, size, "segments, %zu steps", spec->liquidation_steps[idea]);
    }
}

/* The continuation of the label: the fragments (leading notes, or segments) and how the cadential idea is liquidated. */
static void continuation_label(const GrowThemeSpec *spec, char *out, size_t size) {
    char fragments[64];
    char liquidated[32];
    if (spec->fragment_from_segment[0]) {
        char first[16] = "";
        char second[16] = "";
        if (spec->fragment_cut[0] > 0) {
            snprintf(first, sizeof(first), " cut %zu", spec->fragment_cut[0]);
        } else if (spec->fragment_diminished[0]) {
            snprintf(first, sizeof(first), " diminished");
        }
        if (spec->fragment_cut[1] > 0) {
            snprintf(second, sizeof(second), " cut %zu", spec->fragment_cut[1]);
        } else if (spec->fragment_diminished[1]) {
            snprintf(second, sizeof(second), " diminished");
        }
        if (spec->has_liquidated[GROW_PLACE_FRAGMENT_2]) {
            snprintf(fragments, sizeof(fragments), "segment %s%s then liquidated", spec->segments[spec->fragment_segment[0]].label,
                     first);
        } else {
            snprintf(fragments, sizeof(fragments), "segment %s%s then %s%s%s", spec->segments[spec->fragment_segment[0]].label,
                     first, spec->segments[spec->fragment_segment[1]].label, second, spec->fragment_sequence ? " down" : "");
        }
    } else if (spec->has_liquidated[GROW_PLACE_FRAGMENT_2]) {
        snprintf(fragments, sizeof(fragments), "%zu notes then liquidated", spec->fragment_notes[0]);
    } else {
        snprintf(fragments, sizeof(fragments), "%zu+%zu notes", spec->fragment_notes[0], spec->fragment_notes[1]);
    }
    liquidation_label(spec, GROW_PLACE_CADENTIAL, liquidated, sizeof(liquidated));
    snprintf(out, size, "fragments %s, cadential %s", fragments, liquidated);
}

/* The theme's length in the label: "8" or, with a construction, the halves' bars, "4+5". */
static void bars_text(const GrowThemeSpec *spec, char *out, size_t size) {
    if (spec->has_construction) {
        snprintf(out, size, "%zu+%zu", spec->layout.beginning_bars, spec->layout.ending_bars);
    } else {
        snprintf(out, size, "%zu", spec->bars);
    }
}

static void base_label(const GrowThemeSpec *spec, char *label, size_t size) {
    static const char *const BEGINNINGS[] = {"presentation", "antecedent", "compound basic idea"};
    static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};
    char bars[24];
    bars_text(spec, bars, sizeof(bars));
    if (spec->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
        spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION) {
        char continuation[96];
        continuation_label(spec, continuation, sizeof(continuation));
        snprintf(label, size, "sentence %s bars %s %s", bars, repetition_name(spec->repetition), continuation);
        return;
    }
    size_t used = (size_t)snprintf(label, size, "%s + %s %s bars", BEGINNINGS[spec->pairing.beginning],
                                   ENDINGS[spec->pairing.ending], bars);
    if (used < size && spec->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION) {
        used += (size_t)snprintf(label + used, size - used, " %s", repetition_name(spec->repetition));
    } else if (used < size) {
        used += (size_t)snprintf(label + used, size - used, " contrast %s", dev_name(&spec->contrast));
    }
    if (used < size && spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION) {
        char continuation[96];
        continuation_label(spec, continuation, sizeof(continuation));
        snprintf(label + used, size - used, " %s", continuation);
    } else if (used < size && spec->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT) {
        snprintf(label + used, size - used, " consequent contrast %s", dev_name(&spec->consequent_contrast));
    } else if (used < size) {
        char first[32];
        char second[32];
        liquidation_label(spec, GROW_PLACE_CADENTIAL_1, first, sizeof(first));
        liquidation_label(spec, GROW_PLACE_CADENTIAL_2, second, sizeof(second));
        snprintf(label + used, size - used, " cadential %s then %s", first, second);
    }
}

/* The extensions of a construction after the halves' bars in the label (R-468): "repeated fragment 2", "evaded cadence". */
static void extension_label(const GrowThemeSpec *spec, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t k = 0; k < spec->layout.extension_count && used + 1 < size; k++) {
        const GrowLayoutExtension *extension = &spec->layout.extensions[k];
        const ThemeShapeIdea *row = theme_shape_place_in(&spec->pairing, (unsigned)extension->place);
        const char *idea = row ? row->name : "idea";
        switch (extension->kind) {
        case PLAN_EXTENSION_REPEAT:
            used += (size_t)snprintf(out + used, size - used, " repeated %s", idea);
            break;
        case PLAN_EXTENSION_STRETCH:
            used += (size_t)snprintf(out + used, size - used, " stretched %s", idea);
            break;
        case PLAN_EXTENSION_ADDITION:
            used += (size_t)snprintf(out + used, size - used, " addition");
            break;
        case PLAN_EXTENSION_EVADED:
            used += (size_t)snprintf(out + used, size - used, " evaded cadence");
            break;
        }
    }
}

/*
 * The label of a candidate: its halves' bars, then its extensions, then its choices, so the extensions are not lost when
 * the label is cut short: "sentence 4+6 bars evaded cadence exact fragments ...".
 */
static void spec_label(const GrowThemeSpec *spec, char *label, size_t size) {
    char base[256];
    char extensions[128];
    base_label(spec, base, sizeof(base));
    extension_label(spec, extensions, sizeof(extensions));
    const char *bars = strstr(base, " bars");
    size_t head = bars ? (size_t)(bars - base) + strlen(" bars") : strlen(base);
    snprintf(label, size, "%.*s%s%s", (int)head, base, extensions, base + head);
}

static bool grow_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    const GrowContext *shared = context;
    ThemeRequest request;
    if (!read_request(plan, target, &request)) {
        return grow_free_applies(plan, target) ? grow_free_generate(plan, target, rng, shared, out) : true;
    }
    request.development = shared ? shared->development : GROW_DEVELOP_BALANCED;
    if (shared) {
        request.continuation = shared->continuation;
        request.basic_as_written = shared->basic_as_written;
        request.contrast = shared->contrast;
        request.motif = shared->motif;
    }
    size_t wanted = shared && shared->candidates > 0 ? shared->candidates : GROW_DEFAULT_CANDIDATES;
    GrowThemeSpec *kept[16];
    size_t kept_count = 0;
    if (wanted > 16) {
        wanted = 16;
    }
    for (size_t draw = 0; kept_count < wanted && draw < wanted * DRAWS_PER_CANDIDATE; draw++) {
        GrowThemeSpec *spec = malloc(sizeof(*spec));
        if (!spec) {
            return false;
        }
        bool repeat = false;
        if (!draw_spec(plan, &request, shared, rng, spec)) {
            free(spec);
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_spec(kept[i], spec);
        }
        if (repeat) {
            free(spec);
            continue;
        }
        char label[FILL_LABEL_MAX];
        spec_label(spec, label, sizeof(label));
        if (!fill_candidates_add(out, label, spec, free)) {
            return false;
        }
        kept[kept_count++] = spec;
    }
    return true;
}

/* ------------------------------------------------------------- applying */

PlanResult grow_add_node(GrowBuild *build, NodeId parent, PlanNodeKind kind, Rational start, Rational length,
                         const char *name, NodeId *out) {
    PlanSpan span;
    PlanResult result = plan_result_of_rational(rational_add(build->request->span.start, start, &span.start));
    span.length = length;
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, parent, kind, span, name, out);
    }
    return result == PLAN_OK ? plan_node_set_state(build->plan, *out, PLAN_STATE_REALIZED) : result;
}

PlanResult grow_add_motive(GrowBuild *build, size_t slot, const PlanMotiveRef *ref, NodeId *out) {
    static const char *const MOTIVE_NAMES[GROW_PLACE_COUNT] = {"m0", "m1", "m2", "m3", "m4"};
    /* An idea with a motive of its own asks for that node: no motive is made for it. */
    if (build->request->idea_node[slot] != PLAN_NODE_NONE) {
        *out = build->request->idea_node[slot];
        return PLAN_OK;
    }
    size_t bars = build->motive_bars > 0 ? build->motive_bars : 1;
    PlanSpan span = {{0, 1}, grow_scaled(build->request->bar, bars)};
    PlanResult result = plan_result_of_rational(rational_add(
        build->material_span.start, grow_scaled(build->request->bar, build->slot_base + (slot - 1) * bars), &span.start));
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, build->material, PLAN_NODE_MOTIVE, span, MOTIVE_NAMES[slot], out);
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = *ref;
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, *out, &constraint) : result;
}

/*
 * The open inner voice spans an accompaniment is written in, over the same time as a harmony span: voice 1 with the
 * constraint, and the voices 2 and 3 a block, an oom-pah or ANY writes. The pattern says how many are needed; for ANY
 * all three are opened, and the ones the pattern chosen does not use are written empty.
 */
static PlanResult add_accompaniment_targets(GrowBuild *build, NodeId owner, PlanSpan span) {
    const PlanAccompaniment *accompaniment = &build->request->accompaniment;
    int voices = accompaniment->pattern == PLAN_ACCOMPANIMENT_ARPEGGIATED ||
                         accompaniment->pattern == PLAN_ACCOMPANIMENT_ALBERTI ||
                         accompaniment->pattern == PLAN_ACCOMPANIMENT_MOTIVE
                     ? 1
                     : 3;
    PlanResult result = PLAN_OK;
    for (int voice = 1; result == PLAN_OK && voice <= voices; voice++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        note.midi = INNER_PLACEHOLDER;
        note.spelled = false;
        note.offset = (Rational){0, 1};
        note.duration = span.length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        result = plan_add_lane_span(build->plan, owner, PLAN_LANE_INNER_VOICE, voice, span, &payload, &id);
        if (result == PLAN_OK && voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
            constraint.params.accompaniment = *accompaniment;
            result = plan_lane_span_add_constraint(build->plan, id, &constraint);
        }
    }
    return result;
}

/*
 * The open spans a four-part voicing is written in, over the same time as a harmony span: the alto (voice 1) with the
 * constraint, the tenor (voice 2), and, when the voicing writes the soprano, the melody, with the cadence the harmony
 * ends in so the voicing can end the soprano on it.
 */
static PlanResult add_voicing_targets(GrowBuild *build, NodeId owner, PlanSpan span, const PlanConstraint *cadence) {
    const PlanVoicing *voicing = &build->request->voicing;
    PlanResult result = PLAN_OK;
    for (int voice = 1; result == PLAN_OK && voice <= 2; voice++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        note.midi = INNER_PLACEHOLDER;
        note.spelled = false;
        note.offset = (Rational){0, 1};
        note.duration = span.length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        result = plan_add_lane_span(build->plan, owner, PLAN_LANE_INNER_VOICE, voice, span, &payload, &id);
        if (result == PLAN_OK && voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_VOICING;
            constraint.params.voicing = *voicing;
            result = plan_lane_span_add_constraint(build->plan, id, &constraint);
        }
    }
    if (result == PLAN_OK && voicing->write_soprano) {
        /* The melody the voicing writes is the span's gaps: a Given melody is not written over. */
        PlanSpan gaps[GROW_MAX_GAPS];
        size_t gap_count = given_gaps(build->plan, PLAN_LANE_MELODY, 0, span, gaps, GROW_MAX_GAPS);
        Rational span_end;
        gap_count = gap_count > GROW_MAX_GAPS ? GROW_MAX_GAPS : gap_count;
        result = plan_span_end(span, &span_end);
        for (size_t g = 0; result == PLAN_OK && g < gap_count; g++) {
            PlanLanePayload payload;
            PlanNote note;
            LaneSpanId id;
            Rational gap_end;
            memset(&payload, 0, sizeof(payload));
            note.midi = MELODY_PLACEHOLDER;
            note.spelled = false;
            note.offset = (Rational){0, 1};
            note.duration = gaps[g].length;
            payload.notes.notes = &note;
            payload.notes.note_count = 1;
            result = plan_span_end(gaps[g], &gap_end);
            if (result == PLAN_OK) {
                result = plan_add_lane_span(build->plan, owner, PLAN_LANE_MELODY, 0, gaps[g], &payload, &id);
            }
            if (result == PLAN_OK && cadence && rational_cmp(gap_end, span_end) == 0) {
                result = plan_lane_span_add_constraint(build->plan, id, cadence);
            }
        }
    }
    return result;
}

/* The shape of a harmony target over a gap of its owner: the anchors and the cadence belong to the ends of the owner. */
static PlanHarmonyShape gap_shape(const PlanHarmonyShape *shape, bool at_start, bool at_end) {
    PlanHarmonyShape out = *shape;
    if (!at_end) {
        /* A span that stops short of the owner's end is not where a cadence arrives: it is prolonged. */
        if (out.kind != PLAN_HARMONY_SHAPE_PROLONGATION && out.starts_on == PLAN_HARMONY_ANCHOR_PIVOT) {
            out.starts_on = PLAN_HARMONY_ANCHOR_ANY;
        }
        out.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
        out.ends_on = PLAN_HARMONY_ANCHOR_ANY;
    }
    if (!at_start) {
        out.starts_on = PLAN_HARMONY_ANCHOR_ANY;
        /* A phrase that begins in the middle of its owner opens with no tonic. */
        if (out.kind == PLAN_HARMONY_SHAPE_PHRASE_MODEL) {
            out.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
        }
    }
    return out;
}

/* The chords of an owner: open targets asking for the shape, or, with a degree, that one diatonic chord written whole. */
static PlanResult add_chord_targets(GrowBuild *build, NodeId owner, const PlanHarmonyShape *shape,
                                    const PlanConstraint *cadence, int degree, const PlanSpan *over) {
    PiecePlan *plan = build->plan;
    PhraseChord fixed;
    if (degree > 0 && !harmony_make_chord(build->request->key.mode, degree, 0, false, false, 1, &fixed)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanSpan span = over ? *over : plan_node_span(plan, owner);
    Rational span_end;
    PlanSpan gaps[GROW_MAX_GAPS];
    PlanResult result = plan_span_end(span, &span_end);
    /* Given harmony and Given bass are not written over: targets are opened for what is left. */
    size_t gap_count = given_gaps(plan, PLAN_LANE_HARMONY, 0, span, gaps, GROW_MAX_GAPS);
    gap_count = gap_count > GROW_MAX_GAPS ? GROW_MAX_GAPS : gap_count;
    for (size_t g = 0; result == PLAN_OK && g < gap_count; g++) {
        PlanLanePayload payload;
        LaneSpanId harmony;
        Rational gap_end;
        PlanConstraint constraint;
        memset(&payload, 0, sizeof(payload));
        if (degree > 0) {
            harmony_chord_payload(&fixed, &payload);
        } else {
            strcpy(payload.harmony.roman_numeral, "I");
            payload.harmony.has_function = false;
        }
        result = plan_span_end(gaps[g], &gap_end);
        bool at_start = rational_cmp(gaps[g].start, span.start) == 0;
        bool at_end = result == PLAN_OK && rational_cmp(gap_end, span_end) == 0;
        if (result == PLAN_OK) {
            result = plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, gaps[g], &payload, &harmony);
        }
        if (degree > 0) {
            /* A chord written whole has no shape to ask for. */
            result = result == PLAN_OK ? plan_lane_span_set_state(plan, harmony, PLAN_STATE_REALIZED) : result;
            continue;
        }
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_HARMONY_SHAPE;
        constraint.params.harmony_shape = gap_shape(shape, at_start, at_end);
        if (result == PLAN_OK) {
            result = plan_lane_span_add_constraint(plan, harmony, &constraint);
        }
        if (result == PLAN_OK && cadence && at_end) {
            result = plan_lane_span_add_constraint(plan, harmony, cadence);
        }
    }
    gap_count = given_gaps(plan, PLAN_LANE_BASS, 0, span, gaps, GROW_MAX_GAPS);
    gap_count = gap_count > GROW_MAX_GAPS ? GROW_MAX_GAPS : gap_count;
    for (size_t g = 0; result == PLAN_OK && g < gap_count; g++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId bass;
        memset(&payload, 0, sizeof(payload));
        note.midi = BASS_PLACEHOLDER;
        note.spelled = false;
        note.offset = (Rational){0, 1};
        note.duration = gaps[g].length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        result = plan_add_lane_span(plan, owner, PLAN_LANE_BASS, 0, gaps[g], &payload, &bass);
    }
    if (result == PLAN_OK && build->request->has_accompaniment) {
        result = add_accompaniment_targets(build, owner, span);
    }
    if (result == PLAN_OK && build->request->has_voicing) {
        result = add_voicing_targets(build, owner, span, cadence);
    }
    return result;
}

PlanResult grow_add_chord_targets(GrowBuild *build, NodeId owner, const PlanHarmonyShape *shape,
                                  const PlanConstraint *cadence) {
    return add_chord_targets(build, owner, shape, cadence, 0, NULL);
}

PlanResult grow_add_chord_targets_over(GrowBuild *build, NodeId owner, PlanSpan span, const PlanHarmonyShape *shape,
                                       const PlanConstraint *cadence) {
    return add_chord_targets(build, owner, shape, cadence, 0, &span);
}

PlanResult grow_add_chord_written(GrowBuild *build, NodeId owner, int degree) {
    PlanHarmonyShape unused;
    memset(&unused, 0, sizeof(unused));
    return add_chord_targets(build, owner, &unused, NULL, degree, NULL);
}

/* The bars the material holds, counted. */
static size_t count_bars(PlanSpan material_span, Rational bar) {
    Rational length = {0, 1};
    size_t count = 0;
    while (rational_cmp(length, material_span.length) < 0 && count < 4096) {
        if (rational_add(length, bar, &length) != RATIONAL_OK) {
            break;
        }
        count++;
    }
    return count;
}

static size_t material_bars(const GrowBuild *build) {
    return count_bars(build->material_span, build->request->bar);
}

/*
 * The motive node made from the first bar of the Given melody that touches the idea (R-071), Given: the idea's melody
 * gaps ask for it, so the idea is made from the notes the user gave. One node a Given span, in the last free bar of the
 * material (the bars after the sections' own motives are the motives the ideas are made from). PLAN_NODE_NONE when the
 * span's first bar has no note or the material has no bar free.
 */
static NodeId given_motive(GrowBuild *build, PlanSpan idea_span) {
    PiecePlan *plan = build->plan;
    LaneSpanId ids[8];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, idea_span, ids, 8);
    LaneSpanId given = PLAN_LANE_SPAN_NONE;
    for (size_t i = 0; i < found && i < 8 && given == PLAN_LANE_SPAN_NONE; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN) {
            given = ids[i];
        }
    }
    if (given == PLAN_LANE_SPAN_NONE) {
        return PLAN_NODE_NONE;
    }
    char name[48];
    snprintf(name, sizeof(name), "given %zu", (size_t)given);
    for (size_t i = 0; i < plan_node_child_count(plan, build->material); i++) {
        NodeId child = plan_node_child_at(plan, build->material, i);
        if (strcmp(plan_node_name(plan, child) ? plan_node_name(plan, child) : "", name) == 0) {
            return child;
        }
    }
    /* The first bar of the Given span, clipped to the bar. */
    Motive motive;
    const PlanNotes *notes = &plan_lane_span_payload(plan, given)->notes;
    motive_init(&motive, build->request->meter);
    for (size_t n = 0; n < notes->note_count && motive.note_count < MOTIVE_MAX_NOTES; n++) {
        PlanNote note = notes->notes[n];
        Rational end;
        if (rational_cmp(note.offset, build->request->bar) >= 0) {
            break;
        }
        if (rational_add(note.offset, note.duration, &end) != RATIONAL_OK) {
            return PLAN_NODE_NONE;
        }
        if (rational_cmp(end, build->request->bar) > 0 &&
            rational_sub(build->request->bar, note.offset, &note.duration) != RATIONAL_OK) {
            return PLAN_NODE_NONE;
        }
        motive.notes[motive.note_count].pitch = note.midi;
        motive.notes[motive.note_count].onset = note.offset;
        motive.notes[motive.note_count].duration = note.duration;
        motive.note_count++;
    }
    if (motive.note_count == 0 ||
        rational_add(motive.notes[motive.note_count - 1].onset, motive.notes[motive.note_count - 1].duration,
                      &motive.length) != RATIONAL_OK) {
        return PLAN_NODE_NONE;
    }
    /* The last bar of the material no node starts in. */
    size_t bars = material_bars(build);
    for (size_t bar = bars; bar > 0; bar--) {
        Rational start;
        bool taken = false;
        if (rational_add(build->material_span.start, grow_scaled(build->request->bar, bar - 1), &start) != RATIONAL_OK) {
            return PLAN_NODE_NONE;
        }
        for (size_t i = 0; i < plan_node_child_count(plan, build->material); i++) {
            PlanSpan other = plan_node_span(plan, plan_node_child_at(plan, build->material, i));
            Rational other_end;
            taken = taken || (plan_span_end(other, &other_end) == PLAN_OK && rational_cmp(other.start, start) <= 0 &&
                              rational_cmp(start, other_end) < 0);
        }
        NodeId node;
        if (!taken && plan_lane_find(plan, PLAN_LANE_MELODY, 0, (PlanSpan){start, motive.length}, NULL, 0) == 0) {
            return motive_attach(plan, build->material, start, &motive, name, &node, NULL) == PLAN_OK ? node : PLAN_NODE_NONE;
        }
    }
    return PLAN_NODE_NONE;
}

PlanResult grow_add_melody_target(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref,
                                  const PlanConstraint *cadence) {
    PiecePlan *plan = build->plan;
    /* A voicing that writes the soprano takes the place of the melody: its spans are made with the chords. */
    if (build->request->has_voicing && build->request->voicing.write_soprano) {
        return PLAN_OK;
    }
    PlanSpan span = plan_node_span(plan, idea);
    Rational span_end;
    PlanSpan gaps[GROW_MAX_GAPS];
    PlanResult result = plan_span_end(span, &span_end);
    size_t gap_count = given_gaps(plan, PLAN_LANE_MELODY, 0, span, gaps, GROW_MAX_GAPS);
    gap_count = gap_count > GROW_MAX_GAPS ? GROW_MAX_GAPS : gap_count;
    /* An idea with a Given melody inside it is made from that melody's notes where it is not written, unless it ends the
       theme: its melody must end on the cadence's tone, which the first bar of the Given melody need not. */
    PlanMotiveRef asked = *ref;
    if (gap_count > 0 && !cadence && given_touches(plan, PLAN_LANE_MELODY, 0, span)) {
        NodeId motive = given_motive(build, span);
        if (motive != PLAN_NODE_NONE) {
            memset(&asked, 0, sizeof(asked));
            asked.motive = motive;
        }
    }
    for (size_t g = 0; result == PLAN_OK && g < gap_count; g++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId melody;
        Rational gap_end;
        memset(&payload, 0, sizeof(payload));
        note.midi = MELODY_PLACEHOLDER;
        note.spelled = false;
        note.offset = (Rational){0, 1};
        note.duration = gaps[g].length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        result = plan_span_end(gaps[g], &gap_end);
        if (result == PLAN_OK) {
            result = plan_add_lane_span(plan, idea, PLAN_LANE_MELODY, 0, gaps[g], &payload, &melody);
        }
        PlanConstraint constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
        constraint.params.motive = asked;
        if (result == PLAN_OK) {
            result = plan_lane_span_add_constraint(plan, melody, &constraint);
        }
        if (result == PLAN_OK && cadence && rational_cmp(gap_end, span_end) == 0) {
            result = plan_lane_span_add_constraint(plan, melody, cadence);
        }
    }
    return result;
}

PlanResult grow_add_melody_written(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref, const Motive *motive) {
    PiecePlan *plan = build->plan;
    /* A voicing that writes the soprano takes the place of the melody (grow_add_melody_target). */
    if (build->request->has_voicing && build->request->voicing.write_soprano) {
        return PLAN_OK;
    }
    PlanNote notes[MOTIVE_MAX_NOTES];
    PlanLanePayload payload;
    LaneSpanId melody;
    PlanConstraint constraint;
    for (size_t i = 0; i < motive->note_count; i++) {
        notes[i] = plan_note(motive_pitch(motive, i), motive->notes[i].onset, motive->notes[i].duration);
    }
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = notes;
    payload.notes.note_count = motive->note_count;
    PlanResult result = plan_add_lane_span(plan, idea, PLAN_LANE_MELODY, 0, plan_node_span(plan, idea), &payload, &melody);
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = *ref;
    if (result == PLAN_OK) {
        result = plan_lane_span_add_constraint(plan, melody, &constraint);
    }
    return result == PLAN_OK ? plan_lane_span_set_state(plan, melody, PLAN_STATE_REALIZED) : result;
}

/*
 * The bar of the material the motives a section adds start in: the motives each section adds go after the ones there,
 * so the sections of a piece share the material section without overlapping. A section regenerated (R-072) leaves a gap
 * where its motives were: the first gap of the bars wanted is taken, and when there is none, the bar after the last
 * motive, which is where the motives of a piece nothing was removed from always go.
 */
size_t grow_first_free_bar(const PiecePlan *plan, NodeId material, PlanSpan material_span, Rational bar,
                          size_t wanted) {
    size_t free_bar = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, material); i++) {
        PlanSpan span = plan_node_span(plan, plan_node_child_at(plan, material, i));
        Rational end;
        if (plan_span_end(span, &end) != PLAN_OK) {
            continue;
        }
        /* (end - start) / bar, rounded up. */
        long long num = ((long long)end.num * (long long)material_span.start.den -
                         (long long)material_span.start.num * (long long)end.den) *
                        (long long)bar.den;
        long long den = (long long)end.den * (long long)material_span.start.den * (long long)bar.num;
        if (num <= 0 || den <= 0) {
            continue;
        }
        size_t bars = (size_t)((num + den - 1) / den);
        free_bar = bars > free_bar ? bars : free_bar;
    }
    /* A gap below the last motive: a run of bars no node starts in, long enough. */
    size_t run = 0;
    for (size_t at = 0; at < free_bar; at++) {
        Rational start;
        bool taken = rational_add(material_span.start, grow_scaled(bar, at), &start) != RATIONAL_OK;
        for (size_t i = 0; !taken && i < plan_node_child_count(plan, material); i++) {
            PlanSpan other = plan_node_span(plan, plan_node_child_at(plan, material, i));
            Rational other_end;
            taken = plan_span_end(other, &other_end) == PLAN_OK && rational_cmp(other.start, start) <= 0 &&
                    rational_cmp(start, other_end) < 0;
        }
        run = taken ? 0 : run + 1;
        if (run == wanted && wanted > 0) {
            return at + 1 - wanted;
        }
    }
    return free_bar;
}

/* The section's own motive in the material, at the first free bar, made from m0 as the spec's section_ref says. */
static PlanResult add_section_motive(GrowBuild *build, size_t bar_index, NodeId *out) {
    char name[64];
    const char *section = plan_node_name(build->plan, build->request->section);
    snprintf(name, sizeof(name), "%s motive", section ? section : "section");
    PlanSpan span = {{0, 1}, grow_scaled(build->request->bar, build->motive_bars > 0 ? build->motive_bars : 1)};
    PlanResult result =
        plan_result_of_rational(rational_add(build->material_span.start, grow_scaled(build->request->bar, bar_index), &span.start));
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, build->material, PLAN_NODE_MOTIVE, span, name, out);
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = build->spec->section_ref;
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, *out, &constraint) : result;
}

static PlanResult grow_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const GrowThemeSpec *spec = candidate ? candidate->payload : NULL;
    ThemeRequest request;
    (void)context;
    if (spec && spec->magic == GROW_FREE_MAGIC) {
        return grow_free_apply(plan, target, (const GrowFreeSpec *)candidate->payload);
    }
    if (!spec || spec->magic != GROW_PAYLOAD_MAGIC) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    if (!read_request(plan, target, &request) || request.bars != spec->bars) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    GrowBuild build;
    memset(&build, 0, sizeof(build));
    build.plan = plan;
    build.spec = spec;
    build.request = &request;
    build.layout = spec->layout;
    build.cadence.kind = PLAN_CONSTRAINT_CADENCE;
    build.cadence.params.cadence = request.cadence;
    build.phrase = PLAN_NODE_NONE;
    build.material = plan_node_parent(plan, request.source);
    build.material_span = plan_node_span(plan, build.material);
    build.source = request.source;
    /* A derived motive takes the bars of the longest motive the theme is made from. */
    build.motive_bars = derived_bars(spec, request.bar);
    build.segment_node[0] = PLAN_NODE_NONE;
    build.segment_node[1] = PLAN_NODE_NONE;
    /* A section's motives: its own, when it has one, and the four the ideas are made from, then the fragments' segments. */
    size_t made = (spec->fragment_from_segment[0] ? 1 : 0) + (spec->fragment_from_segment[1] ? 1 : 0);
    size_t shortened_bars = 0;
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        made += spec->has_liquidated[k] ? 1 : 0;
        build.liquid_node[k] = PLAN_NODE_NONE;
        build.short_node[k] = PLAN_NODE_NONE;
        shortened_bars += spec->has_shortened[k] ? grow_motive_layout_bars(&spec->shortened[k], request.bar) : 0;
    }
    size_t extended_bars = grow_extended_bars(spec, request.bar);
    for (size_t k = 0; k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        build.extension_node[k] = PLAN_NODE_NONE;
    }
    build.slot_base = grow_first_free_bar(plan, build.material, build.material_span, request.bar,
                                          ((spec->has_section_motive ? 1 : 0) + GROW_PLACE_COUNT - 1) * build.motive_bars +
                                              made + shortened_bars + extended_bars);
    PlanResult result = PLAN_OK;
    if (spec->has_section_motive) {
        result = add_section_motive(&build, build.slot_base, &build.source);
        build.slot_base += build.motive_bars;
    }
    if (result != PLAN_OK) {
        return result;
    }
    build.result_base = build.slot_base + (GROW_PLACE_COUNT - 1) * build.motive_bars;
    /* The shortened sources are made nodes too, after the segments and the liquidations, each at its own bars (R-471). */
    size_t short_at = build.result_base + made;
    for (size_t k = 0; result == PLAN_OK && k < GROW_PLACE_COUNT; k++) {
        if (!spec->has_shortened[k]) {
            continue;
        }
        Rational start;
        char name[32];
        snprintf(name, sizeof(name), "shortened for m%zu", k);
        result = plan_result_of_rational(
            rational_add(build.material_span.start, grow_scaled(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start, &spec->shortened[k], name,
                                                      PLAN_STATE_REALIZED, &build.short_node[k], NULL)
                                   : result;
        short_at += grow_motive_layout_bars(&spec->shortened[k], request.bar);
    }
    /* The motives of the extensions follow, each at its own bars (R-468): the repeats' and the addition's. */
    for (size_t k = 0; result == PLAN_OK && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (!spec->has_extended[k]) {
            continue;
        }
        Rational start;
        char name[32];
        size_t bars = grow_motive_layout_bars(&spec->extended[k], request.bar);
        snprintf(name, sizeof(name), "extension %zu", k + 1);
        result = plan_result_of_rational(rational_add(build.material_span.start, grow_scaled(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start, &spec->extended[k], name,
                                                      PLAN_STATE_REALIZED, &build.extension_node[k], NULL)
                                   : result;
        short_at += bars > 0 ? bars : 1;
    }
    if (result != PLAN_OK) {
        return result;
    }
    result = grow_beginning_build(&build);
    return result == PLAN_OK ? grow_ending_build(&build) : result;
}

FillResult grow_register(FillOperatorRegistry *registry, const GrowContext *context) {
    FillOperator operator_;
    if (!registry) {
        return FILL_ERR_NULL;
    }
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = GROW_THEME_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan section with an archetype and a theme pairing";
    operator_.generator.supported_outputs = "PiecePlan phrase, ideas, motive nodes and open lane spans";
    operator_.generator.configuration_description = "the number of candidates";
    operator_.generator.constraints_description = "standard-form sentence rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_GROW;
    operator_.applies = grow_applies;
    operator_.generate = grow_generate;
    operator_.apply = grow_apply;
    operator_.context = (void *)context;
    return fill_registry_register(registry, &operator_);
}
