#include <ursatz_composer/harmony_embellish.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

#define DEFAULT_DENSITY 500
#define DENSITY_SCALE 1000

HarmonyEmbellishParams harmony_embellish_params_default(void) {
    HarmonyEmbellishParams params = {DEFAULT_DENSITY, HARMONY_EMBELLISH_ALL};
    return params;
}

PlanResult harmony_embellish_open(PiecePlan *plan, PlanSpan range) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
    if (count == 0) {
        return PLAN_OK;
    }
    LaneSpanId *ids = malloc(count * sizeof(*ids));
    if (!ids) {
        return PLAN_ERR_ALLOC;
    }
    plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, ids, count);
    PlanConstraint marker = {PLAN_CONSTRAINT_EMBELLISH, {0}};
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED) {
            result = plan_lane_span_set_state(plan, ids[i], PLAN_STATE_CONSTRAINED);
            if (result == PLAN_OK && !harmony_span_has_constraint(plan, ids[i], PLAN_CONSTRAINT_EMBELLISH)) {
                result = plan_lane_span_add_constraint(plan, ids[i], &marker);
            }
        }
    }
    free(ids);
    return result;
}

static const HarmonyEmbellishParams *params_of(void *context) {
    static const HarmonyEmbellishParams defaults = {DEFAULT_DENSITY, HARMONY_EMBELLISH_ALL};
    return context ? context : &defaults;
}

static bool embellish_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseContext phrase;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && harmony_context_read(plan, (LaneSpanId)target.id, &phrase);
}

static bool add_plain(FillCandidateList *out) {
    EmbellishCandidate *candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        return false;
    }
    candidate->plain = true;
    return fill_candidates_add(out, "plain", candidate, free);
}

/* Embellishments proposed for one chord, at most. */
#define MAX_PROPOSALS 4

static void shuffle(Edit *items, size_t count, FillRng *rng) {
    for (size_t i = count; i > 1; i--) {
        size_t j = (size_t)fill_rng_below(rng, i);
        Edit swap = items[i - 1];
        items[i - 1] = items[j];
        items[j] = swap;
    }
}

static void edit_label(const Edit *edit, char *label, size_t size) {
    size_t used = 0;
    label[0] = '\0';
    for (size_t i = 0; i < edit->lead_count + edit->piece_count && used + 1 < size; i++) {
        const PhraseChord *chord = i < edit->lead_count ? &edit->lead[i] : &edit->pieces[i - edit->lead_count];
        int written = snprintf(label + used, size - used, i ? " %s" : "%s", chord->roman);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
    }
}

/*
 * Proposes the embellishments of the enabled families for the marked chord. Each shape is built
 * and judged by standard-harmony before it is proposed, so only ones that pass are offered, in an
 * order drawn from rng and at most MAX_PROPOSALS of them. None proposed leaves the chord as it is.
 */
static bool propose_edits(const PiecePlan *plan, const PhraseContext *phrase, const HarmonyEmbellishParams *params,
                          FillRng *rng, FillCandidateList *out) {
    OptionList *options = calloc(1, sizeof(*options));
    EmbellishCandidate *scratch = malloc(sizeof(*scratch));
    bool ok = options && scratch;
    if (ok && (params->families & HARMONY_EMBELLISH_PROLONGATION)) {
        harmony_options_prolongation(plan, phrase, rng, options);
    }
    if (ok && (params->families & HARMONY_EMBELLISH_PREDOMINANT)) {
        harmony_options_predominant(phrase, rng, options);
    }
    if (ok && (params->families & HARMONY_EMBELLISH_APPLIED)) {
        harmony_options_applied(plan, phrase, options);
    }
    if (ok) {
        shuffle(options->items, options->count, rng);
    }
    size_t proposed = 0;
    for (size_t i = 0; ok && i < options->count && proposed < MAX_PROPOSALS; i++) {
        if (!harmony_edit_compose(phrase, &options->items[i], scratch)) {
            continue;
        }
        HeardPhrase heard = {scratch->chords, scratch->count, scratch->mode, scratch->cadence,
                             scratch->cells_per_bar, scratch->cell_length, false, NULL, false, false, NULL, NULL, false,
                             &scratch->metre};
        PhraseVerdict verdict;
        if (!harmony_judge(&heard, &verdict)) {
            ok = false;
        } else if (!verdict.hard) {
            EmbellishCandidate *candidate = malloc(sizeof(*candidate));
            char label[FILL_LABEL_MAX];
            if (!candidate) {
                ok = false;
                break;
            }
            *candidate = *scratch;
            edit_label(&scratch->edit, label, sizeof(label));
            ok = fill_candidates_add(out, label, candidate, free);
            proposed++;
        }
    }
    free(scratch);
    free(options);
    return ok;
}

static bool embellish_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                               FillCandidateList *out) {
    const HarmonyEmbellishParams *params = params_of(context);
    PhraseContext phrase;
    if (target.kind != FILL_TARGET_LANE_SPAN || !harmony_context_read(plan, (LaneSpanId)target.id, &phrase)) {
        return false;
    }
    /* The draw is made whether or not it is used, so the random stream does not depend on the parameters. */
    unsigned density = params->density > DENSITY_SCALE ? DENSITY_SCALE : params->density;
    bool embellish = fill_rng_below(rng, DENSITY_SCALE) < density && params->families != 0;
    if (embellish && !propose_edits(plan, &phrase, params, rng, out)) {
        return false;
    }
    /* A chord nothing fits is left as it is. */
    return out->count > 0 || add_plain(out);
}

static PlanResult clear_marker(PiecePlan *plan, LaneSpanId id) {
    size_t count = plan_lane_span_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        if (plan_lane_span_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_EMBELLISH) {
            return plan_lane_span_remove_constraint(plan, id, i);
        }
    }
    return PLAN_OK;
}

static PlanResult add_chord(PiecePlan *plan, NodeId owner, const PhraseChord *chord, PlanSpan span, LaneSpanId *id) {
    PlanLanePayload payload;
    harmony_chord_payload(chord, &payload);
    PlanResult result = plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, id);
    return result == PLAN_OK ? plan_lane_span_set_state(plan, *id, PLAN_STATE_REALIZED) : result;
}

/* Applies an edit: the lead chords go into the end of the chord before, the pieces replace the marked chord. */
static PlanResult apply_edit(PiecePlan *plan, const PhraseContext *phrase, const EmbellishCandidate *candidate) {
    const Edit *edit = &candidate->edit;
    size_t t = phrase->target;
    PlanTime cell = phrase->metre.cell_length;
    size_t lead_cells = 0;
    size_t piece_cells = 0;
    for (size_t i = 0; i < edit->lead_count; i++) {
        lead_cells += edit->lead[i].cells;
    }
    for (size_t i = 0; i < edit->piece_count; i++) {
        piece_cells += edit->pieces[i].cells;
    }
    if (edit->piece_count == 0 || piece_cells != phrase->chords[t].cells ||
        phrase->chords[t].cells != candidate->target_cells || lead_cells != edit->steal_cells ||
        (edit->steal_cells > 0 && (t == 0 || phrase->chords[t - 1].cells <= edit->steal_cells))) {
        return PLAN_ERR_BAD_PAYLOAD;
    }

    PlanSpan span;
    PlanResult result = PLAN_OK;
    if (edit->steal_cells > 0) {
        LaneSpanId before = phrase->ids[t - 1];
        size_t kept = phrase->chords[t - 1].cells - edit->steal_cells;
        PlanSpan before_span = plan_lane_span_span(plan, before);
        result = harmony_cells_span(before_span.start, cell, 0, kept, &span);
        if (result == PLAN_OK) {
            result = plan_lane_span_resize(plan, before, span.length);
        }
        size_t at = kept;
        for (size_t i = 0; result == PLAN_OK && i < edit->lead_count; i++) {
            LaneSpanId id;
            result = harmony_cells_span(before_span.start, cell, at, edit->lead[i].cells, &span);
            if (result == PLAN_OK) {
                result = add_chord(plan, phrase->owner, &edit->lead[i], span, &id);
            }
            at += edit->lead[i].cells;
        }
    }

    LaneSpanId first = phrase->ids[t];
    LaneSpanId last = first;
    PlanTime start = plan_lane_span_span(plan, first).start;
    if (result == PLAN_OK) {
        result = harmony_cells_span(start, cell, 0, edit->pieces[0].cells, &span);
    }
    if (result == PLAN_OK) {
        PlanLanePayload payload;
        harmony_chord_payload(&edit->pieces[0], &payload);
        result = plan_lane_span_resize(plan, first, span.length);
        if (result == PLAN_OK) {
            result = plan_lane_span_write(plan, first, &payload);
        }
    }
    size_t at = edit->pieces[0].cells;
    for (size_t i = 1; result == PLAN_OK && i < edit->piece_count; i++) {
        result = harmony_cells_span(start, cell, at, edit->pieces[i].cells, &span);
        if (result == PLAN_OK) {
            result = add_chord(plan, phrase->owner, &edit->pieces[i], span, &last);
        }
        at += edit->pieces[i].cells;
    }
    /* The cadence arrives on the last chord of the phrase, wherever that now is. */
    if (result == PLAN_OK && t + 1 == phrase->count && last != first) {
        result = harmony_move_cadence(plan, first, last);
    }
    return result;
}

static PlanResult embellish_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const EmbellishCandidate *edit = candidate ? candidate->payload : NULL;
    PhraseContext phrase;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !edit) {
        return PLAN_ERR_NULL;
    }
    if (!edit->plain) {
        if (!harmony_context_read(plan, (LaneSpanId)target.id, &phrase)) {
            return PLAN_ERR_BAD_PAYLOAD;
        }
        PlanResult result = apply_edit(plan, &phrase, edit);
        if (result != PLAN_OK) {
            return result;
        }
    }
    return clear_marker(plan, (LaneSpanId)target.id);
}

FillResult harmony_embellish_register(FillOperatorRegistry *registry, const HarmonyEmbellishParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_EMBELLISH_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span marked for embellishment";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "density in thousandths, and the families that are on";
    operator_.generator.constraints_description = "standard-harmony chord function, prolongation and tonicization";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = embellish_applies;
    operator_.generate = embellish_generate;
    operator_.apply = embellish_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
