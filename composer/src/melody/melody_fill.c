#include <ursatz_composer/melody_fill.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "melody/melody_internal.h"

#define DEFAULT_TOLERANCE 2
#define DEFAULT_RANGE_LOW 60
#define DEFAULT_RANGE_HIGH 84
#define DEFAULT_START_CELL 0
#define DEFAULT_NCT_TYPES (MELODY_NCT_PASSING | MELODY_NCT_NEIGHBOR)
#define DEFAULT_DENSITY 0
#define DEFAULT_CLIMAX 667

/* Melodies tried per call, and kept; ones that repeat an earlier one are dropped. */
#define MELODY_ATTEMPTS 48
#define MELODY_KEEP 6

MelodyFillParams melody_fill_params_default(void) {
    MelodyFillParams params = {DEFAULT_TOLERANCE, DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH,
                               DEFAULT_START_CELL, DEFAULT_NCT_TYPES, DEFAULT_DENSITY, DEFAULT_CLIMAX, false};
    return params;
}

static const MelodyFillParams *params_of(void *context) {
    static const MelodyFillParams defaults = {DEFAULT_TOLERANCE, DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH,
                                              DEFAULT_START_CELL, DEFAULT_NCT_TYPES, DEFAULT_DENSITY, DEFAULT_CLIMAX, false};
    return context ? context : &defaults;
}

static bool same_melody(const MelodyCandidate *a, const MelodyCandidate *b) {
    if (a->count != b->count || a->pickup_count != b->pickup_count) {
        return false;
    }
    for (size_t i = 0; i < a->pickup_count; i++) {
        if (a->pickup_notes[i].pitch != b->pickup_notes[i].pitch) {
            return false;
        }
    }
    for (size_t i = 0; i < a->count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch || rational_cmp(a->notes[i].onset, b->notes[i].onset) != 0 ||
            rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* The label says whether the motive is the original or a development, then the pitches. */
static void melody_label(const MelodyCandidate *melody, char *label, size_t size) {
    int used = snprintf(label, size, "%s", melody->context.motive_given ? "original" : "developed");
    for (size_t i = 0; used > 0 && (size_t)used + 1 < size && i < melody->count; i++) {
        int written = snprintf(label + used, size - (size_t)used, " %d", melody->notes[i].pitch);
        if (written < 0) {
            break;
        }
        used += written;
    }
}

static bool melody_applies(const PiecePlan *plan, FillTarget target, void *context) {
    MelodyContext *read;
    bool applies;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN) {
        return false;
    }
    read = malloc(sizeof(*read));
    if (!read) {
        return false;
    }
    applies = melody_context_read(plan, (LaneSpanId)target.id, read);
    free(read);
    return applies;
}

static bool melody_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    const MelodyFillParams *params = params_of(context);
    MelodyContext *read = malloc(sizeof(*read));
    MelodyCandidate *kept[MELODY_KEEP];
    size_t kept_count = 0;
    MelodyCandidate *scratch = malloc(sizeof(*scratch));
    bool ok = read && scratch;
    int shifts[5];
    size_t shift_count = 0;
    /* A span the operator does not read, or a motive that cannot be placed, leaves the span open. */
    if (ok && (target.kind != FILL_TARGET_LANE_SPAN || !melody_context_read(plan, (LaneSpanId)target.id, read))) {
        ok = false;
    }
    /* The octave of the motive's notes is chosen per attempt from the ranked shifts (R-334); one that fits none leaves the span open. */
    if (ok) {
        shift_count = melody_realize_shifts(read, params, shifts);
    }
    for (size_t attempt = 0; ok && shift_count > 0 && attempt < MELODY_ATTEMPTS && kept_count < MELODY_KEEP; attempt++) {
        bool repeat = false;
        PhraseVerdict verdict;
        /* The first half of the attempts keeps the figures the framework labels; the second moves every clash to a chord tone. */
        if (!melody_build(read, params, shifts[attempt % shift_count], attempt < MELODY_ATTEMPTS / 2, rng, scratch)) {
            continue;
        }
        /* Only melodies that pass the hard rules are proposed; none passing leaves the span open. */
        if (!melody_judge(scratch, &verdict)) {
            ok = false;
            break;
        }
        if (verdict.hard) {
            continue;
        }
        for (size_t i = 0; i < kept_count; i++) {
            repeat = repeat || same_melody(kept[i], scratch);
        }
        if (repeat) {
            continue;
        }
        MelodyCandidate *payload = malloc(sizeof(*payload));
        char label[FILL_LABEL_MAX];
        if (!payload) {
            ok = false;
            break;
        }
        *payload = *scratch;
        melody_label(payload, label, sizeof(label));
        ok = fill_candidates_add(out, label, payload, free);
        kept[kept_count++] = payload;
    }
    free(scratch);
    free(read);
    return ok;
}

/* The target keeps its id and takes the notes, as one Realized span over the phrase. */
static PlanResult melody_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const MelodyCandidate *melody = candidate ? candidate->payload : NULL;
    MelodyContext *read;
    PlanNote *notes;
    PlanLanePayload payload;
    PlanResult result = PLAN_OK;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !melody || melody->magic != MELODY_PAYLOAD_MAGIC ||
        melody->count == 0) {
        return PLAN_ERR_NULL;
    }
    read = malloc(sizeof(*read));
    notes = calloc(melody->count + melody->pickup_count, sizeof(*notes));
    if (!read || !notes) {
        free(read);
        free(notes);
        return PLAN_ERR_ALLOC;
    }
    /* The plan must still be what the melody was made for. */
    if (!melody_context_read(plan, (LaneSpanId)target.id, read) || read->count != melody->context.count ||
        rational_cmp(read->length, melody->context.length) != 0 ||
        rational_cmp(read->pickup, melody->context.pickup) != 0) {
        result = PLAN_ERR_BAD_PAYLOAD;
    }
    /* The span starts with the pickup: the notes are written from its start, the pickup's first. */
    for (size_t i = 0; result == PLAN_OK && i < melody->pickup_count + melody->count; i++) {
        const MelodyNote *note = i < melody->pickup_count ? &melody->pickup_notes[i]
                                                           : &melody->notes[i - melody->pickup_count];
        notes[i].midi = note->pitch;
        notes[i].spelled = false;
        notes[i].offset = position_add(note->onset, melody->context.pickup);
        notes[i].duration = note->duration;
        if (position_is_far(notes[i].offset) || position_is_far(notes[i].duration)) {
            result = PLAN_ERR_BAD_TIME;
        }
    }
    if (result == PLAN_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = notes;
        payload.notes.note_count = melody->pickup_count + melody->count;
        result = plan_lane_span_write(plan, (LaneSpanId)target.id, &payload);
    }
    free(read);
    free(notes);
    return result;
}

FillResult melody_fill_register(FillOperatorRegistry *registry, const MelodyFillParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = MELODY_FILL_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan melody lane span over a written harmony and bass lane";
    operator_.generator.supported_outputs = "PiecePlan melody lane span";
    operator_.generator.configuration_description = "interval tolerance, melody range, motive start cell, non-chord tone types and density";
    operator_.generator.constraints_description = "chord tones on strong beats, NonChordTone figures, standard-counterpoint melodic and harmonic rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = melody_applies;
    operator_.generate = melody_generate;
    operator_.apply = melody_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
