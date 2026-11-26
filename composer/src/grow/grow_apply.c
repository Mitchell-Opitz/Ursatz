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

/* Applying a candidate: the nodes, the targets and the motives a theme adds to the plan. */

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
    PlanSpan span = {{0, 1}, position_times(build->request->bar, bars)};
    PlanResult result = plan_result_of_rational(rational_add(
        build->material_span.start, position_times(build->request->bar, build->slot_base + (slot - 1) * bars), &span.start));
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
        memset(&note, 0, sizeof(note));
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
        memset(&note, 0, sizeof(note));
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
            memset(&note, 0, sizeof(note));
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
    Plan *plan = build->plan;
    PhraseChord fixed;
    if (degree > 0 && !harmony_make_chord(harmony_key_mode(&build->request->key), degree, 0, false, false, 1, &fixed)) {
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
        memset(&note, 0, sizeof(note));
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
size_t grow_count_bars(PlanSpan material_span, Rational bar) {
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
    return grow_count_bars(build->material_span, build->request->bar);
}

/*
 * The motive node made from the first bar of the Given melody that touches the idea (R-071), Given: the idea's melody
 * gaps ask for it, so the idea is made from the notes the user gave. One node a Given span, in the last free bar of the
 * material (the bars after the sections' own motives are the motives the ideas are made from). PLAN_NODE_NONE when the
 * span's first bar has no note or the material has no bar free.
 */
static NodeId given_motive(GrowBuild *build, PlanSpan idea_span) {
    Plan *plan = build->plan;
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
        if (rational_add(build->material_span.start, position_times(build->request->bar, bar - 1), &start) != RATIONAL_OK) {
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
    Plan *plan = build->plan;
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
        memset(&note, 0, sizeof(note));
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
    Plan *plan = build->plan;
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
size_t grow_first_free_bar(const Plan *plan, NodeId material, PlanSpan material_span, Rational bar,
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
        bool taken = rational_add(material_span.start, position_times(bar, at), &start) != RATIONAL_OK;
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
    PlanSpan span = {{0, 1}, position_times(build->request->bar, build->motive_bars > 0 ? build->motive_bars : 1)};
    PlanResult result =
        plan_result_of_rational(rational_add(build->material_span.start, position_times(build->request->bar, bar_index), &span.start));
    if (result == PLAN_OK) {
        result = plan_add_node(build->plan, build->material, PLAN_NODE_MOTIVE, span, name, out);
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = build->spec->section_ref;
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, *out, &constraint) : result;
}

PlanResult grow_apply(Plan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const GrowThemeSpec *spec = candidate ? candidate->payload : NULL;
    const GrowContext *shared = context;
    GrowThemeRequest request;
    if (spec && spec->magic == GROW_FREE_MAGIC) {
        return grow_free_apply(plan, target, (const GrowFreeSpec *)candidate->payload);
    }
    if (spec && spec->magic == GROW_LIQUIDATE_MAGIC) {
        return grow_liquidate_apply(plan, target, (const GrowLiquidateSpec *)candidate->payload);
    }
    if (spec && spec->magic == GROW_CODA_MAGIC) {
        return grow_coda_apply(plan, target, (const GrowCodaSpec *)candidate->payload);
    }
    if (!spec || spec->magic != GROW_PAYLOAD_MAGIC) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    if (!grow_read_request(plan, target, &request) || request.bars != spec->bars) {
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
    build.motive_bars = grow_derived_bars(spec, request.bar);
    build.segment_node[0] = PLAN_NODE_NONE;
    build.segment_node[1] = PLAN_NODE_NONE;
    /* A section's motives: its own, when it has one, and the four the ideas are made from, then the fragments' segments. */
    size_t made = (spec->fragment_from_segment[0] ? 1 : 0) + (spec->fragment_from_segment[1] ? 1 : 0);
    size_t shortened_bars = 0;
    for (size_t k = 0; k < GROW_PLACE_COUNT; k++) {
        made += spec->has_liquidated[k] ? 1 : 0;
        build.liquid_node[k] = PLAN_NODE_NONE;
        build.short_node[k] = PLAN_NODE_NONE;
        build.distant_node[k] = PLAN_NODE_NONE;
        shortened_bars += spec->has_shortened[k] ? grow_motive_layout_bars(&spec->shortened[k], request.bar) : 0;
        shortened_bars += spec->has_distant[k] ? grow_motive_layout_bars(&spec->distant[k], request.bar) : 0;
    }
    build.new_node = PLAN_NODE_NONE;
    size_t new_bars = spec->has_new_material
                          ? grow_motive_layout_bars(&spec->slot_source[GROW_SLOT_CONTRASTING], request.bar)
                          : 0;
    shortened_bars += new_bars;
    size_t extended_bars = grow_extended_bars(spec, request.bar);
    for (size_t k = 0; k < SECTION_CONSTRUCTION_EXTENSIONS; k++) {
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
            rational_add(build.material_span.start, position_times(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start, &spec->shortened[k], name,
                                                      PLAN_STATE_REALIZED, &build.short_node[k], NULL)
                                   : result;
        short_at += grow_motive_layout_bars(&spec->shortened[k], request.bar);
    }
    /* The distant contrasts follow the shortened sources, each at its own bars (R-475). */
    for (size_t k = 0; result == PLAN_OK && k < GROW_PLACE_COUNT; k++) {
        if (!spec->has_distant[k]) {
            continue;
        }
        Rational start;
        char name[32];
        snprintf(name, sizeof(name), "distant for m%zu", k);
        result = plan_result_of_rational(
            rational_add(build.material_span.start, position_times(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start, &spec->distant[k], name,
                                                      PLAN_STATE_REALIZED, &build.distant_node[k], NULL)
                                   : result;
        short_at += grow_motive_layout_bars(&spec->distant[k], request.bar);
    }
    /* The contrasting slot's new material follows the distant contrasts, at its own bars (R-477). */
    if (result == PLAN_OK && spec->has_new_material) {
        Rational start;
        result = plan_result_of_rational(
            rational_add(build.material_span.start, position_times(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start,
                                                      &spec->slot_source[GROW_SLOT_CONTRASTING], "new material",
                                                      PLAN_STATE_REALIZED, &build.new_node, NULL)
                                   : result;
        short_at += new_bars;
    }
    /* The motives of the extensions follow, each at its own bars (R-468): the repeats' and the addition's. */
    for (size_t k = 0; result == PLAN_OK && k < SECTION_CONSTRUCTION_EXTENSIONS; k++) {
        if (!spec->has_extended[k]) {
            continue;
        }
        Rational start;
        char name[32];
        size_t bars = grow_motive_layout_bars(&spec->extended[k], request.bar);
        snprintf(name, sizeof(name), "extension %zu", k + 1);
        result = plan_result_of_rational(rational_add(build.material_span.start, position_times(request.bar, short_at), &start));
        result = result == PLAN_OK ? motive_attach_as(plan, build.material, start, &spec->extended[k], name,
                                                      PLAN_STATE_REALIZED, &build.extension_node[k], NULL)
                                   : result;
        short_at += bars > 0 ? bars : 1;
    }
    if (result != PLAN_OK) {
        return result;
    }
    result = grow_beginning_build(&build);
    result = result == PLAN_OK ? grow_ending_build(&build) : result;
    /* A blueprint's anchors, events and climax go on the melody spans that hold them, now that the spans are made (R-743). */
    if (result == PLAN_OK && shared && shared->blueprint) {
        result = grow_blueprint_place(plan, request.section, shared->blueprint);
    }
    return result;
}


void grow_build_init(GrowBuild *build, Plan *plan, const GrowThemeRequest *theme, size_t bars) {
    memset(build, 0, sizeof(*build));
    build->plan = plan;
    build->request = theme;
    build->cadence.kind = PLAN_CONSTRAINT_CADENCE;
    build->cadence.params.cadence = theme->cadence;
    build->material = plan_node_parent(plan, theme->source);
    build->material_span = plan_node_span(plan, build->material);
    build->source = theme->source;
    build->slot_base = grow_first_free_bar(plan, build->material, build->material_span, theme->bar, bars);
}
