#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hybrid_rules.h>
#include <phrase_beginning.h>
#include <phrase_ending.h>
#include <phrase_level_form.h>

#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/* How many candidates are drawn for each one asked for before the search is given up. */
#define DRAWS_PER_CANDIDATE 6
/* The pitch of the placeholder notes a bass, a melody and an inner voice span hold until the operators write them. */
#define BASS_PLACEHOLDER 40
#define MELODY_PLACEHOLDER 60
#define INNER_PLACEHOLDER 55

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
static bool read_keys(const PiecePlan *plan, ThemeRequest *request) {
    LaneSpanId spans[MAX_KEY_SPANS];
    PlanTime end;
    PlanTime half;
    PlanSpan up_to = {{0, 1}, {0, 1}};
    if (plan_span_end(request->span, &end) != PLAN_OK || plan_time_make(end.num, end.den, &up_to.length) != PLAN_OK ||
        plan_time_make(request->span.length.num, request->span.length.den * 2, &half) != PLAN_OK ||
        plan_time_add(request->span.start, half, &half) != PLAN_OK) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, up_to, spans, MAX_KEY_SPANS);
    if (found == 0 || found > MAX_KEY_SPANS) {
        return false;
    }
    size_t at = found;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, spans[i]);
        PlanTime span_end;
        if (plan_span_end(span, &span_end) == PLAN_OK && plan_time_compare(span.start, request->span.start) <= 0 &&
            plan_time_compare(request->span.start, span_end) < 0) {
            at = i;
        }
    }
    if (at == found) {
        return false;
    }
    PlanSpan first = plan_lane_span_span(plan, spans[at]);
    PlanTime first_end;
    if (plan_span_end(first, &first_end) != PLAN_OK) {
        return false;
    }
    request->key = plan_lane_span_payload(plan, spans[at])->key;
    if (at > 0 && plan_time_compare(first.start, request->span.start) == 0) {
        request->has_start_change = true;
        request->from_key = plan_lane_span_payload(plan, spans[at - 1])->key;
    }
    if (plan_time_compare(first_end, end) >= 0) {
        return at + 1 == found || plan_time_compare(plan_lane_span_span(plan, spans[at + 1]).start, end) >= 0;
    }
    /* A second span starts at the half and holds the rest of the section. */
    if (at + 1 >= found || plan_time_compare(first_end, half) != 0) {
        return false;
    }
    PlanSpan second = plan_lane_span_span(plan, spans[at + 1]);
    PlanTime second_end;
    if (plan_time_compare(second.start, half) != 0 || plan_span_end(second, &second_end) != PLAN_OK ||
        plan_time_compare(second_end, end) < 0) {
        return false;
    }
    request->has_end_change = true;
    request->end_key = plan_lane_span_payload(plan, spans[at + 1])->key;
    return true;
}

static bool read_request(const PiecePlan *plan, FillTarget target, ThemeRequest *out) {
    ThemeRequest request;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0) {
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
    if (!read_keys(plan, &request)) {
        return false;
    }
    request.bar = grow_bar_length(&request.meter);
    request.source = request.has_motive_ref ? request.motive_ref.motive : grow_find_source(plan);
    if (request.source == PLAN_NODE_NONE || !grow_cells_per_bar(&request.meter, &request.cells_per_bar) ||
        request.bar.num == 0) {
        return false;
    }
    /* The theme is 4 or 8 whole bars. */
    PlanTime bars = {0, 1};
    size_t count = 0;
    while (plan_time_compare(bars, request.span.length) < 0 && count <= 8) {
        if (plan_time_add(bars, request.bar, &bars) != PLAN_OK) {
            return false;
        }
        count++;
    }
    if (plan_time_compare(bars, request.span.length) != 0 || (count != 4 && count != 8)) {
        return false;
    }
    request.bars = count;
    *out = request;
    return true;
}

static bool grow_applies(const PiecePlan *plan, FillTarget target, void *context) {
    ThemeRequest request;
    (void)context;
    return read_request(plan, target, &request);
}

/* ------------------------------------------------------------ the spec */

PlanTime grow_scaled(PlanTime unit, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make(unit.num * (long)count, unit.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

PlanTime grow_unit(const GrowThemeRequest *request) {
    PlanTime unit = {0, 1};
    if (plan_time_make(request->bar.num * (long)request->bars, request->bar.den * 8, &unit) != PLAN_OK) {
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
        if (direction(a->notes[i].midi_pitch - a->notes[i - 1].midi_pitch) !=
            direction(b->notes[i].midi_pitch - b->notes[i - 1].midi_pitch)) {
            return false;
        }
    }
    return true;
}

int grow_phrase_beginning(PlanThemeBeginning beginning) {
    switch (beginning) {
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return PHRASE_BEGINNING_ANTECEDENT;
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        return PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return PHRASE_BEGINNING_PRESENTATION;
    }
    return PHRASE_BEGINNING_COUNT;
}

int grow_phrase_ending(PlanThemeEnding ending) {
    switch (ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return PHRASE_ENDING_CONTINUATION;
    case PLAN_THEME_ENDING_CONSEQUENT:
        return PHRASE_ENDING_CONSEQUENT;
    case PLAN_THEME_ENDING_CADENTIAL:
        return PHRASE_ENDING_CADENTIAL;
    }
    return PHRASE_ENDING_COUNT;
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

bool grow_idea_dev(const GrowThemeSpec *spec, GrowIdea idea, MotiveDevSpec *dev) {
    memset(dev, 0, sizeof(*dev));
    switch (idea) {
    case GROW_IDEA_REPETITION: /* also the contrasting idea of the other beginnings */
        if (spec->pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION) {
            *dev = spec->contrast;
        } else if (spec->statement_response) {
            dev->kind = MOTIVE_DEV_TRANSPOSE;
            dev->tonal = true;
            dev->amount = 1;
        } else {
            dev->kind = MOTIVE_DEV_EXACT;
        }
        return true;
    case GROW_IDEA_FRAGMENT_1:
    case GROW_IDEA_FRAGMENT_2: {
        size_t second = idea == GROW_IDEA_FRAGMENT_2;
        if (spec->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT) {
            if (second) {
                *dev = spec->consequent_contrast;
            } else {
                dev->kind = MOTIVE_DEV_EXACT;
            }
        } else if (spec->pairing.ending == PLAN_THEME_ENDING_CADENTIAL) {
            dev->kind = MOTIVE_DEV_LIQUIDATE;
            dev->drop = spec->cadential_drop[second];
            dev->min_length = 1;
        } else {
            dev->kind = MOTIVE_DEV_FRAGMENT;
            dev->position = PLAN_FRAGMENT_LEADING;
            dev->length = spec->fragment_notes[second];
            dev->min_length = 1;
        }
        return true;
    }
    case GROW_IDEA_CADENTIAL:
        if (spec->pairing.ending != PLAN_THEME_ENDING_CONTINUATION) {
            return false;
        }
        dev->kind = MOTIVE_DEV_LIQUIDATE;
        dev->drop = spec->liquidate_drop;
        dev->min_length = 1;
        return true;
    default:
        return false;
    }
}

void grow_idea_ref(const GrowThemeSpec *spec, GrowIdea idea, NodeId source, PlanMotiveRef *out) {
    MotiveDevSpec dev;
    memset(out, 0, sizeof(*out));
    out->motive = source;
    if (!grow_idea_dev(spec, idea, &dev)) {
        out->transform = PLAN_TRANSFORM_NONE;
        return;
    }
    switch (dev.kind) {
    case MOTIVE_DEV_EXACT:
        out->transform = PLAN_TRANSFORM_RESTATED;
        break;
    case MOTIVE_DEV_TRANSPOSE:
        out->transform = PLAN_TRANSFORM_TRANSPOSED;
        out->tonal = dev.tonal;
        out->steps = dev.tonal ? dev.amount : 0;
        out->semitones = dev.tonal ? 0 : dev.amount;
        break;
    case MOTIVE_DEV_INVERT:
        out->transform = PLAN_TRANSFORM_INVERTED;
        out->tonal = dev.tonal;
        out->has_axis = dev.has_axis;
        out->axis = dev.axis;
        break;
    case MOTIVE_DEV_INTERVAL_CHANGE:
        out->transform = PLAN_TRANSFORM_INTERVAL_CHANGED;
        out->interval = dev.interval;
        out->amount = dev.amount;
        out->keep_contour = dev.keep_contour;
        break;
    case MOTIVE_DEV_FRAGMENT:
        out->transform = PLAN_TRANSFORM_FRAGMENTED;
        out->position = dev.position;
        out->length = (int)dev.length;
        break;
    case MOTIVE_DEV_LIQUIDATE:
        out->transform = PLAN_TRANSFORM_LIQUIDATED;
        out->drop = (int)dev.drop;
        out->keep_rhythm = dev.keep_rhythm;
        break;
    default:
        out->transform = PLAN_TRANSFORM_NONE;
        break;
    }
}

bool grow_idea_develop(const GrowThemeSpec *spec, GrowIdea idea, Motive *out) {
    MotiveDevSpec dev;
    return grow_idea_dev(spec, idea, &dev) && motive_develop(&spec->source, &dev, out) == MOTIVE_DEV_OK;
}

bool grow_split_possible(const GrowThemeRequest *request, PlanHarmonyShapeKind shape, PlanCadenceType cadence) {
    /* A tonic, the predominants the mode has (two in a major key, one in a minor), a dominant unless the cadence is half. */
    size_t chords = (shape == PLAN_HARMONY_SHAPE_CADENTIAL ? 0 : 1) + (request->key.mode == PLAN_MODE_MINOR ? 1 : 2) +
                    (cadence == PLAN_CADENCE_HC ? 0 : 1);
    size_t cells = request->bars / 2 * request->cells_per_bar;
    /* The last chord is the whole last bar. */
    return cells <= request->cells_per_bar || cells - request->cells_per_bar <= chords * request->cells_per_bar;
}

/* Whether two developments are the same kind of change by the same amount. */
static bool same_kind(const MotiveDevSpec *a, const MotiveDevSpec *b) {
    return a->kind == b->kind && a->amount == b->amount && a->interval == b->interval &&
           (a->kind != MOTIVE_DEV_INVERT || a->axis == b->axis);
}

/* The most developments of m0 one contrasting idea is chosen among: inversions, interval changes everywhere and one. */
#define CONTRAST_MAX 160
/* The widest the notes of a contrasting idea may go, so the melody has room to place it. */
#define CONTRAST_LOW 58
#define CONTRAST_HIGH 84

/* Whether the last note of a development lands on the pitch class asked for (any when want is negative). */
static bool lands_on(const Motive *result, int want) {
    return want < 0 || ((result->notes[result->note_count - 1].midi_pitch % 12) + 12) % 12 == want;
}

bool grow_draw_contrast(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec, int want_pc,
                        const MotiveDevSpec *avoid, MotiveDevSpec *out) {
    PlanTime room = grow_scaled(grow_unit(request), 2);
    const Motive *source = &spec->source;
    size_t intervals = source->note_count - 1;
    MotiveDevSpec fits[CONTRAST_MAX];
    MotiveDevSpec exact[CONTRAST_MAX];
    size_t fit_count = 0;
    size_t exact_count = 0;

    for (int kind = 0; kind < 3; kind++) {
        /* Mirrored in the key's steps about a note near the first, so the contour turns over; every interval
           widened or narrowed; or the one interval changed. */
        int first = source->notes[0].midi_pitch;
        int low = kind == 0 ? first - 2 : (kind == 1 ? -2 : 1);
        int high = kind == 0 ? first + 2 : (kind == 1 ? 2 : (int)intervals);
        for (int value = low; value <= high; value++) {
            for (int amount = (kind == 2 ? -4 : 0); amount <= (kind == 2 ? 4 : 0); amount++) {
                MotiveDevSpec dev;
                Motive result;
                memset(&dev, 0, sizeof(dev));
                if (kind == 0) {
                    dev.kind = MOTIVE_DEV_INVERT;
                    dev.tonal = true;
                    dev.has_axis = true;
                    dev.axis = value;
                } else {
                    dev.kind = MOTIVE_DEV_INTERVAL_CHANGE;
                    dev.interval = kind == 1 ? 0 : value;
                    dev.amount = kind == 1 ? value : amount;
                }
                if ((kind != 0 && dev.amount == 0) ||
                    motive_develop(source, &dev, &result) != MOTIVE_DEV_OK || grow_same_contour(source, &result) ||
                    plan_time_compare(result.length, room) > 0 || (avoid && same_kind(avoid, &dev))) {
                    continue;
                }
                bool inside = true;
                for (size_t k = 0; k < result.note_count; k++) {
                    inside = inside && result.notes[k].midi_pitch >= CONTRAST_LOW && result.notes[k].midi_pitch <= CONTRAST_HIGH;
                }
                if (!inside) {
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
    /* One that lands where the cadence needs it, if there is one. */
    const MotiveDevSpec *pool = exact_count > 0 ? exact : fits;
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
 * must fit one bar and have two notes or more. A diminution or augmentation no usual ratio can write in the meter is
 * made an interval change instead. False when the ref asks for a transform a basic idea is not made from (a fragment,
 * a liquidation) or the draw does not come out.
 */
#define SECTION_MOTIVE_DRAWS 8

static const int AUGMENT_RATIOS[][2] = {{2, 1}, {3, 2}, {4, 3}};
static const int DIMINISH_RATIOS[][2] = {{1, 2}, {2, 3}, {3, 4}};
#define RHYTHM_RATIOS 3

/* Whether any usual ratio augments or diminishes the motive into a rhythm the meter can write. */
static bool rhythm_writable(const Motive *m0, bool augment) {
    MotiveDevSpec dev;
    Motive result;
    memset(&dev, 0, sizeof(dev));
    dev.kind = augment ? MOTIVE_DEV_AUGMENT : MOTIVE_DEV_DIMINISH;
    for (size_t r = 0; r < RHYTHM_RATIOS; r++) {
        dev.ratio_num = (unsigned long)(augment ? AUGMENT_RATIOS : DIMINISH_RATIOS)[r][0];
        dev.ratio_den = (unsigned long)(augment ? AUGMENT_RATIOS : DIMINISH_RATIOS)[r][1];
        if (motive_develop(m0, &dev, &result) == MOTIVE_DEV_OK) {
            return true;
        }
    }
    return false;
}

static bool draw_section_motive(const ThemeRequest *request, FillRng *rng, const Motive *m0, GrowThemeSpec *spec) {
    PlanMotiveRef rhythm_fallback;
    const PlanMotiveRef *asked = &request->motive_ref;
    if ((asked->transform == PLAN_TRANSFORM_AUGMENTED || asked->transform == PLAN_TRANSFORM_DIMINISHED) &&
        asked->ratio_num == 0 && !rhythm_writable(m0, asked->transform == PLAN_TRANSFORM_AUGMENTED)) {
        /* No usual ratio can be written in the meter (a whole bar of 3/4 has no diminution without triplets): the
           section still sounds unlike m0, with an interval changed. */
        rhythm_fallback = *asked;
        rhythm_fallback.transform = PLAN_TRANSFORM_INTERVAL_CHANGED;
        asked = &rhythm_fallback;
    }
    for (size_t draw = 0; draw < SECTION_MOTIVE_DRAWS; draw++) {
        PlanMotiveRef ref = *asked;
        MotiveDevSpec dev;
        Motive result;
        memset(&dev, 0, sizeof(dev));
        switch (asked->transform) {
        case PLAN_TRANSFORM_NONE:
        case PLAN_TRANSFORM_RESTATED:
            ref.transform = PLAN_TRANSFORM_RESTATED;
            dev.kind = MOTIVE_DEV_EXACT;
            break;
        case PLAN_TRANSFORM_TRANSPOSED:
            dev.kind = MOTIVE_DEV_TRANSPOSE;
            dev.tonal = asked->tonal || asked->semitones == 0;
            ref.tonal = dev.tonal;
            if (dev.tonal) {
                int steps = (int)fill_rng_below(rng, 3) + 1;
                ref.steps = asked->steps != 0 ? asked->steps : (fill_rng_below(rng, 2) == 0 ? steps : -steps);
                dev.amount = ref.steps;
            } else {
                dev.amount = asked->semitones;
            }
            break;
        case PLAN_TRANSFORM_INVERTED:
            dev.kind = MOTIVE_DEV_INVERT;
            dev.tonal = asked->tonal;
            dev.has_axis = true;
            dev.axis = asked->has_axis ? asked->axis : m0->notes[0].midi_pitch;
            ref.has_axis = true;
            ref.axis = dev.axis;
            break;
        case PLAN_TRANSFORM_RETROGRADE:
            dev.kind = MOTIVE_DEV_RETROGRADE;
            break;
        case PLAN_TRANSFORM_INTERVAL_CHANGED:
            dev.kind = MOTIVE_DEV_INTERVAL_CHANGE;
            dev.keep_contour = asked->keep_contour;
            if (asked->amount != 0) {
                dev.interval = asked->interval;
                dev.amount = asked->amount;
            } else {
                int amount = (int)fill_rng_below(rng, 2) + 1;
                dev.interval = (int)fill_rng_below(rng, m0->note_count - 1) + 1;
                dev.amount = fill_rng_below(rng, 2) == 0 ? amount : -amount;
            }
            ref.interval = dev.interval;
            ref.amount = dev.amount;
            break;
        case PLAN_TRANSFORM_AUGMENTED:
        case PLAN_TRANSFORM_DIMINISHED: {
            /* The ratio asked for, else the first of the usual ones the meter can write. */
            bool augment = asked->transform == PLAN_TRANSFORM_AUGMENTED;
            const int(*ratios)[2] = augment ? AUGMENT_RATIOS : DIMINISH_RATIOS;
            dev.kind = augment ? MOTIVE_DEV_AUGMENT : MOTIVE_DEV_DIMINISH;
            for (size_t r = 0; r < RHYTHM_RATIOS; r++) {
                ref.ratio_num = asked->ratio_num > 0 ? asked->ratio_num : ratios[r][0];
                ref.ratio_den = asked->ratio_den > 0 ? asked->ratio_den : ratios[r][1];
                dev.ratio_num = (unsigned long)ref.ratio_num;
                dev.ratio_den = (unsigned long)ref.ratio_den;
                if (asked->ratio_num > 0 || motive_develop(m0, &dev, &result) == MOTIVE_DEV_OK) {
                    break;
                }
            }
            break;
        }
        default:
            return false;
        }
        if (motive_develop(m0, &dev, &result) != MOTIVE_DEV_OK || result.note_count < 2 ||
            plan_time_compare(result.length, request->bar) > 0) {
            continue;
        }
        spec->has_section_motive = true;
        spec->section_ref = ref;
        spec->source = result;
        return true;
    }
    return false;
}

/* Draws every choice of one candidate from rng. False when the draw does not make a sentence the motive fits. */
static bool draw_spec(const PiecePlan *plan, const ThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    memset(spec, 0, sizeof(*spec));
    spec->magic = GROW_PAYLOAD_MAGIC;
    spec->bars = request->bars;
    spec->meter = request->meter;
    spec->key = request->key;
    spec->cadence = request->cadence;
    spec->pairing = request->pairing;
    spec->first_cadence = request->first_cadence;
    if (!grow_read_motive(plan, request->source, &request->meter, &request->key, &spec->source) ||
        spec->source.note_count < 2) {
        return false;
    }
    if (request->has_motive_ref) {
        Motive m0 = spec->source;
        if (!draw_section_motive(request, rng, &m0, spec)) {
            return false;
        }
    }
    /* The basic idea is two bars of eight, one of four. */
    spec->basic_cells = request->bars == 4 ? request->cells_per_bar : 2 * request->cells_per_bar;
    return grow_beginning_draw(request, rng, spec) && grow_ending_draw(request, rng, spec);
}

static bool same_spec(const GrowThemeSpec *a, const GrowThemeSpec *b) {
    return a->has_section_motive == b->has_section_motive && motive_equal(&a->source, &b->source) &&
           a->statement_response == b->statement_response && a->fragment_notes[0] == b->fragment_notes[0] &&
           a->fragment_notes[1] == b->fragment_notes[1] && a->liquidate_drop == b->liquidate_drop &&
           a->compound_ends_on == b->compound_ends_on && same_kind(&a->contrast, &b->contrast) &&
           same_kind(&a->consequent_contrast, &b->consequent_contrast) &&
           a->cadential_drop[0] == b->cadential_drop[0] && a->cadential_drop[1] == b->cadential_drop[1];
}

static const char *dev_name(const MotiveDevSpec *dev) {
    return dev->kind == MOTIVE_DEV_INVERT ? "inverted" : "intervals changed";
}

static void spec_label(const GrowThemeSpec *spec, char *label, size_t size) {
    static const char *const BEGINNINGS[] = {"presentation", "antecedent", "compound basic idea"};
    static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};
    if (spec->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
        spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION) {
        snprintf(label, size, "sentence %zu bars %s fragments %zu+%zu drop %zu", spec->bars,
                 spec->statement_response ? "statement-response" : "exact", spec->fragment_notes[0],
                 spec->fragment_notes[1], spec->liquidate_drop);
        return;
    }
    size_t used = (size_t)snprintf(label, size, "%s + %s %zu bars", BEGINNINGS[spec->pairing.beginning],
                                   ENDINGS[spec->pairing.ending], spec->bars);
    if (used < size && spec->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION) {
        used += (size_t)snprintf(label + used, size - used, " %s", spec->statement_response ? "statement-response" : "exact");
    } else if (used < size) {
        used += (size_t)snprintf(label + used, size - used, " contrast %s", dev_name(&spec->contrast));
    }
    if (used < size && spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION) {
        snprintf(label + used, size - used, " fragments %zu+%zu drop %zu", spec->fragment_notes[0],
                 spec->fragment_notes[1], spec->liquidate_drop);
    } else if (used < size && spec->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT) {
        snprintf(label + used, size - used, " consequent contrast %s", dev_name(&spec->consequent_contrast));
    } else if (used < size) {
        snprintf(label + used, size - used, " drops %zu+%zu", spec->cadential_drop[0], spec->cadential_drop[1]);
    }
}

static bool grow_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    const GrowContext *shared = context;
    ThemeRequest request;
    if (!read_request(plan, target, &request)) {
        return true;
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
        if (!draw_spec(plan, &request, rng, spec)) {
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

PlanResult grow_add_node(GrowBuild *build, NodeId parent, PlanNodeKind kind, size_t start_unit, size_t units,
                         const char *name, NodeId *out) {
    PlanSpan span;
    PlanResult result = plan_time_add(build->request->span.start, grow_scaled(build->unit, start_unit), &span.start);
    span.length = grow_scaled(build->unit, units);
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, parent, kind, span, name, out);
    }
    return result == PLAN_OK ? plan_node_set_state(build->plan, *out, PLAN_STATE_REALIZED) : result;
}

PlanResult grow_add_motive(GrowBuild *build, size_t slot, const PlanMotiveRef *ref, NodeId *out) {
    static const char *const MOTIVE_NAMES[GROW_IDEA_COUNT] = {"m0", "m1", "m2", "m3", "m4"};
    PlanSpan span = {{0, 1}, build->request->bar};
    PlanResult result = plan_time_add(build->material_span.start,
                                      grow_scaled(build->request->bar, build->slot_base + slot - 1), &span.start);
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
                         accompaniment->pattern == PLAN_ACCOMPANIMENT_ALBERTI
                     ? 1
                     : 3;
    PlanResult result = PLAN_OK;
    for (int voice = 1; result == PLAN_OK && voice <= voices; voice++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        note.midi_pitch = INNER_PLACEHOLDER;
        note.offset = (PlanTime){0, 1};
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
        note.midi_pitch = INNER_PLACEHOLDER;
        note.offset = (PlanTime){0, 1};
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
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        note.midi_pitch = MELODY_PLACEHOLDER;
        note.offset = (PlanTime){0, 1};
        note.duration = span.length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        result = plan_add_lane_span(build->plan, owner, PLAN_LANE_MELODY, 0, span, &payload, &id);
        if (result == PLAN_OK && cadence) {
            result = plan_lane_span_add_constraint(build->plan, id, cadence);
        }
    }
    return result;
}

PlanResult grow_add_chord_targets(GrowBuild *build, NodeId owner, const PlanHarmonyShape *shape,
                                  const PlanConstraint *cadence) {
    PiecePlan *plan = build->plan;
    PlanSpan span = plan_node_span(plan, owner);
    PlanLanePayload payload;
    PlanNote note;
    LaneSpanId harmony;
    LaneSpanId bass;
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    PlanResult result = plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, &harmony);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_HARMONY_SHAPE;
    constraint.params.harmony_shape = *shape;
    if (result == PLAN_OK) {
        result = plan_lane_span_add_constraint(plan, harmony, &constraint);
    }
    if (result == PLAN_OK && cadence) {
        result = plan_lane_span_add_constraint(plan, harmony, cadence);
    }
    memset(&payload, 0, sizeof(payload));
    note.midi_pitch = BASS_PLACEHOLDER;
    note.offset = (PlanTime){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    if (result == PLAN_OK) {
        result = plan_add_lane_span(plan, owner, PLAN_LANE_BASS, 0, span, &payload, &bass);
    }
    if (result == PLAN_OK && build->request->has_accompaniment) {
        result = add_accompaniment_targets(build, owner, span);
    }
    if (result == PLAN_OK && build->request->has_voicing) {
        result = add_voicing_targets(build, owner, span, cadence);
    }
    return result;
}

PlanResult grow_add_melody_target(GrowBuild *build, NodeId idea, const PlanMotiveRef *ref,
                                  const PlanConstraint *cadence) {
    PiecePlan *plan = build->plan;
    /* A voicing that writes the soprano takes the place of the melody: its spans are made with the chords. */
    if (build->request->has_voicing && build->request->voicing.write_soprano) {
        return PLAN_OK;
    }
    PlanSpan span = plan_node_span(plan, idea);
    PlanLanePayload payload;
    PlanNote note;
    LaneSpanId melody;
    memset(&payload, 0, sizeof(payload));
    note.midi_pitch = MELODY_PLACEHOLDER;
    note.offset = (PlanTime){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    PlanResult result = plan_add_lane_span(plan, idea, PLAN_LANE_MELODY, 0, span, &payload, &melody);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = *ref;
    if (result == PLAN_OK) {
        result = plan_lane_span_add_constraint(plan, melody, &constraint);
    }
    if (result == PLAN_OK && cadence) {
        result = plan_lane_span_add_constraint(plan, melody, cadence);
    }
    return result;
}

/*
 * The first bar of the material no motive node holds yet: the motives each section adds go after the ones there, so
 * the sections of a piece share the material section without overlapping.
 */
static size_t first_free_bar(const PiecePlan *plan, NodeId material, PlanSpan material_span, PlanTime bar) {
    size_t free_bar = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, material); i++) {
        PlanSpan span = plan_node_span(plan, plan_node_child_at(plan, material, i));
        PlanTime end;
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
    return free_bar;
}

/* The section's own motive in the material, at the first free bar, made from m0 as the spec's section_ref says. */
static PlanResult add_section_motive(GrowBuild *build, size_t bar_index, NodeId *out) {
    char name[64];
    const char *section = plan_node_name(build->plan, build->request->section);
    snprintf(name, sizeof(name), "%s motive", section ? section : "section");
    PlanSpan span = {{0, 1}, build->request->bar};
    PlanResult result =
        plan_time_add(build->material_span.start, grow_scaled(build->request->bar, bar_index), &span.start);
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
    build.unit = grow_unit(&request);
    build.cadence.kind = PLAN_CONSTRAINT_CADENCE;
    build.cadence.params.cadence = request.cadence;
    build.phrase = PLAN_NODE_NONE;
    build.material = plan_node_parent(plan, request.source);
    build.material_span = plan_node_span(plan, build.material);
    build.source = request.source;
    build.slot_base = first_free_bar(plan, build.material, build.material_span, request.bar);
    PlanResult result = PLAN_OK;
    if (spec->has_section_motive) {
        result = add_section_motive(&build, build.slot_base, &build.source);
        build.slot_base++;
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
