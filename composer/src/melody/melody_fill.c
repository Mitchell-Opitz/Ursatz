#include <ursatz_composer/melody_fill.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "melody/melody_internal.h"

#define DEFAULT_TOLERANCE 2
#define DEFAULT_RANGE_LOW 60
#define DEFAULT_RANGE_HIGH 79
#define DEFAULT_START_CELL 0
#define DEFAULT_NCT_TYPES (MELODY_NCT_PASSING | MELODY_NCT_NEIGHBOR)
#define DEFAULT_DENSITY 0
#define DEFAULT_CLIMAX 667

/* Melodies tried per call, and kept; ones that repeat an earlier one are dropped. */
#define MELODY_ATTEMPTS 48
#define MELODY_KEEP 6

MelodyFillParams melody_fill_params_default(void) {
    MelodyFillParams params = {DEFAULT_TOLERANCE, DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH,
                               DEFAULT_START_CELL, DEFAULT_NCT_TYPES, DEFAULT_DENSITY, DEFAULT_CLIMAX};
    return params;
}

static const MelodyFillParams *params_of(void *context) {
    static const MelodyFillParams defaults = {DEFAULT_TOLERANCE, DEFAULT_RANGE_LOW, DEFAULT_RANGE_HIGH,
                                              DEFAULT_START_CELL, DEFAULT_NCT_TYPES, DEFAULT_DENSITY, DEFAULT_CLIMAX};
    return context ? context : &defaults;
}

static bool same_melody(const MelodyCandidate *a, const MelodyCandidate *b) {
    if (a->count != b->count) {
        return false;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch || a->notes[i].onset != b->notes[i].onset ||
            a->notes[i].duration != b->notes[i].duration) {
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
    /* A span the operator does not read, or a motive that cannot be placed, leaves the span open. */
    if (ok && (target.kind != FILL_TARGET_LANE_SPAN || !melody_context_read(plan, (LaneSpanId)target.id, read))) {
        ok = false;
    }
    for (size_t attempt = 0; ok && attempt < MELODY_ATTEMPTS && kept_count < MELODY_KEEP; attempt++) {
        bool repeat = false;
        if (!melody_build(read, params, rng, scratch)) {
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
    notes = calloc(melody->count, sizeof(*notes));
    if (!read || !notes) {
        free(read);
        free(notes);
        return PLAN_ERR_ALLOC;
    }
    /* The plan must still be what the melody was made for. */
    if (!melody_context_read(plan, (LaneSpanId)target.id, read) || read->count != melody->context.count ||
        read->length != melody->context.length) {
        result = PLAN_ERR_BAD_PAYLOAD;
    }
    for (size_t i = 0; result == PLAN_OK && i < melody->count; i++) {
        notes[i].midi_pitch = melody->notes[i].pitch;
        result = melody_time(melody->notes[i].onset, &notes[i].offset);
        if (result == PLAN_OK) {
            result = melody_time(melody->notes[i].duration, &notes[i].duration);
        }
    }
    if (result == PLAN_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = notes;
        payload.notes.note_count = melody->count;
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
    operator_.generator.constraints_description = "chord tones on strong beats, non-chord tones approached and left as the framework defines them";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = melody_applies;
    operator_.generate = melody_generate;
    operator_.apply = melody_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
