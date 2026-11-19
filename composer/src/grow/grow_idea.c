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

/* What a theme's ideas are made of: their places, developments, sources and nodes (grow_theme.c reads the request, grow_draw.c draws the spec). */

/* ------------------------------------------------------------ the spec */

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

int grow_phrase_beginning(SectionBeginning beginning) {
    const ThemeShapeHalf *half = theme_shape_beginning(beginning);
    PhraseBeginning found;
    return half && phrase_beginning_from_name(half->library_name, &found) ? (int)found : PHRASE_BEGINNING_COUNT;
}

int grow_phrase_ending(SectionEnding ending) {
    const ThemeShapeHalf *half = theme_shape_ending(ending);
    PhraseEnding found;
    return half && phrase_ending_from_name(half->library_name, &found) ? (int)found : PHRASE_ENDING_COUNT;
}

int grow_pairing_archetype(const SectionPairing *pairing) {
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
        if (spec->pairing.beginning != SECTION_BEGINNING_PRESENTATION) {
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
        if (spec->pairing.ending == SECTION_ENDING_CONSEQUENT) {
            if (second) {
                *dev = spec->consequent_contrast;
            } else {
                dev->kind = MOTIVE_RELATION_EXACT;
            }
        } else if (spec->pairing.ending == SECTION_ENDING_CADENTIAL) {
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
    if (!spec->has_distant[idea] && grow_idea_segment(spec, idea, &segment)) {
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
    if (spec->pairing.ending != SECTION_ENDING_CONTINUATION ||
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
    if (spec->has_distant[idea]) {
        return &spec->distant[idea];
    }
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
    if (build->spec->has_distant[idea]) {
        return build->distant_node[idea];
    }
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
    if (slot == GROW_SLOT_CONTRASTING && build->spec->has_new_material) {
        return build->new_node;
    }
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
    return (shape == PLAN_HARMONY_SHAPE_CADENTIAL ? 0 : 1) + (composer_key_is_minor(&request->key) ? 1 : 2) +
           (cadence == CADENCE_TYPE_HALF ? 0 : 1);
}

bool grow_split_possible(const GrowThemeRequest *request, size_t half_bars, PlanHarmonyShapeKind shape,
                         CadenceType cadence) {
    size_t chords = grow_chords_before_last(request, shape, cadence);
    size_t cells = half_bars * request->cells_per_bar;
    /* The last chord is the whole last bar. */
    return cells <= request->cells_per_bar || cells - request->cells_per_bar <= chords * request->cells_per_bar;
}

