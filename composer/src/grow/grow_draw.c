#include <ursatz_composer/grow_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/hybrid_rules.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/tonality/tonal_key.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <plan/plan_state.h>
#include <ursatz_composer/variant_registry.h>

#include "util/position.h"
#include "structure/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "util/util.h"

/* Drawing a theme's spec: the section motive, the ideas' motives, the segments and the label of a spec. */

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

static bool draw_section_motive(const GrowThemeRequest *request, FillRng *rng, const Motive *m0, GrowThemeSpec *spec) {
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
            rational_cmp(result.length, position_times(request->bar, spec->layout.basic_bars)) > 0) {
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
static bool draw_part_whole(const GrowThemeRequest *request, GrowThemeSpec *spec) {
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
size_t grow_derived_bars(const GrowThemeSpec *spec, Rational bar) {
    size_t bars = grow_motive_layout_bars(&spec->source, bar);
    for (size_t k = 0; k < GROW_SLOT_COUNT; k++) {
        size_t slot_bars = spec->has_slot_source[k] ? grow_motive_layout_bars(&spec->slot_source[k], bar) : 0;
        bars = slot_bars > bars ? slot_bars : bars;
    }
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        size_t idea_bars = spec->has_idea_source[k] ? grow_motive_layout_bars(&spec->idea_source[k], bar) : 0;
        bars = idea_bars > bars ? idea_bars : bars;
        size_t distant_bars = spec->has_distant[k] ? grow_motive_layout_bars(&spec->distant[k], bar) : 0;
        bars = distant_bars > bars ? distant_bars : bars;
    }
    return bars;
}

/* Whether the material has bars after the motives for the fragments' segments (a plan made before them may not). */
static bool material_has_room(const Plan *plan, const GrowThemeRequest *request, const GrowThemeSpec *spec) {
    NodeId material = plan_node_parent(plan, request->source);
    PlanSpan span = plan_node_span(plan, material);
    size_t wanted = ((spec->has_section_motive ? 1 : 0) + GROW_PLACE_COUNT - 1) * grow_derived_bars(spec, request->bar) + GROW_MADE_NODES;
    /* An extension that is a repeat or an addition has a motive of its own, a bar of the material at least (R-468). */
    for (size_t k = 0; k < spec->layout.extension_count; k++) {
        wanted += spec->layout.extensions[k].kind == SECTION_EXTENSION_REPEAT || spec->layout.extensions[k].kind == SECTION_EXTENSION_ADDITION;
    }
    return grow_first_free_bar(plan, material, span, request->bar, wanted) + wanted <= grow_count_bars(span, request->bar);
}

/*
 * Whether the material has the bars apply takes for the motives the drawn spec makes: the same count apply uses, the
 * fragments' segments and a liquidation for each idea that has one, which the estimate before the draw (GROW_MADE_NODES) can
 * fall short of.
 */
static bool made_nodes_fit(const Plan *plan, const GrowThemeRequest *request, const GrowThemeSpec *spec) {
    NodeId material = plan_node_parent(plan, request->source);
    PlanSpan span = plan_node_span(plan, material);
    size_t made = (spec->fragment_from_segment[0] ? 1 : 0) + (spec->fragment_from_segment[1] ? 1 : 0);
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        made += spec->has_liquidated[k] ? 1 : 0;
        made += spec->has_shortened[k] ? grow_motive_layout_bars(&spec->shortened[k], request->bar) : 0;
        made += spec->has_distant[k] ? grow_motive_layout_bars(&spec->distant[k], request->bar) : 0;
    }
    made += spec->has_new_material ? grow_motive_layout_bars(&spec->slot_source[GROW_SLOT_CONTRASTING], request->bar) : 0;
    made += grow_extended_bars(spec, request->bar);
    size_t wanted = ((spec->has_section_motive ? 1 : 0) + GROW_PLACE_COUNT - 1) * grow_derived_bars(spec, request->bar) + made;
    return grow_first_free_bar(plan, material, span, request->bar, wanted) + wanted <= grow_count_bars(span, request->bar);
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
bool grow_draw_spec(const Plan *plan, const GrowThemeRequest *request, const GrowContext *shared, FillRng *rng,
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
    /* The context's basic motive is m0's: an independent motive's notes are not its. */
    const GrowContext *basics = request->independent ? NULL : shared;
    spec->basic_has_chords = basics && basics->basic && basics->basic->chord_count > 0;
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
    draw_segments(basics, spec);
    draw_liquid_base(basics, spec);
    spec->segment_room = material_has_room(plan, request, spec);
    spec->basic_cells = spec->layout.basic_bars * request->cells_per_bar;
    return grow_beginning_draw(request, rng, spec) && grow_ending_draw(request, rng, spec) &&
           grow_extensions_draw(request, spec) && made_nodes_fit(plan, request, spec);
}

/* Whether two specs make the same motives for their extensions. */
static bool same_extended(const GrowThemeSpec *a, const GrowThemeSpec *b) {
    for (size_t k = 0; k < SECTION_CONSTRUCTION_EXTENSIONS; k++) {
        if (a->has_extended[k] != b->has_extended[k] || (a->has_extended[k] && !motive_equal(&a->extended[k], &b->extended[k]))) {
            return false;
        }
    }
    return true;
}

bool grow_same_spec(const GrowThemeSpec *a, const GrowThemeSpec *b) {
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
           a->compound_ends_on == b->compound_ends_on && grow_same_kind(&a->contrast, &b->contrast) &&
           grow_same_kind(&a->consequent_contrast, &b->consequent_contrast) && same_extended(a, b);
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
    if (spec->pairing.beginning == SECTION_BEGINNING_PRESENTATION &&
        spec->pairing.ending == SECTION_ENDING_CONTINUATION) {
        char continuation[96];
        continuation_label(spec, continuation, sizeof(continuation));
        snprintf(label, size, "sentence %s bars %s %s", bars, repetition_name(spec->repetition), continuation);
        return;
    }
    size_t used = (size_t)snprintf(label, size, "%s + %s %s bars", BEGINNINGS[spec->pairing.beginning],
                                   ENDINGS[spec->pairing.ending], bars);
    if (used < size && spec->pairing.beginning == SECTION_BEGINNING_PRESENTATION) {
        used += (size_t)snprintf(label + used, size - used, " %s", repetition_name(spec->repetition));
    } else if (used < size) {
        used += (size_t)snprintf(label + used, size - used, " contrast %s", dev_name(&spec->contrast));
    }
    if (used < size && spec->pairing.ending == SECTION_ENDING_CONTINUATION) {
        char continuation[96];
        continuation_label(spec, continuation, sizeof(continuation));
        snprintf(label + used, size - used, " %s", continuation);
    } else if (used < size && spec->pairing.ending == SECTION_ENDING_CONSEQUENT) {
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
        case SECTION_EXTENSION_REPEAT:
            used += (size_t)snprintf(out + used, size - used, " repeated %s", idea);
            break;
        case SECTION_EXTENSION_STRETCH:
            used += (size_t)snprintf(out + used, size - used, " stretched %s", idea);
            break;
        case SECTION_EXTENSION_ADDITION:
            used += (size_t)snprintf(out + used, size - used, " addition");
            break;
        case SECTION_EXTENSION_EVADED:
            used += (size_t)snprintf(out + used, size - used, " evaded cadence");
            break;
        }
    }
}

/*
 * The label of a candidate: its halves' bars, then its extensions, then its choices, so the extensions are not lost when
 * the label is cut short: "sentence 4+6 bars evaded cadence exact fragments ...".
 */
void grow_spec_label(const GrowThemeSpec *spec, char *label, size_t size) {
    char base[256];
    char extensions[128];
    base_label(spec, base, sizeof(base));
    extension_label(spec, extensions, sizeof(extensions));
    const char *bars = strstr(base, " bars");
    size_t head = bars ? (size_t)(bars - base) + strlen(" bars") : strlen(base);
    snprintf(label, size, "%.*s%s%s", (int)head, base, extensions, base + head);
}

