#include <ursatz_composer/motive_develop.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/plan_state.h>

#include "motive/motive_internal.h"

/*
 * The motivic development operators: Grow operators for a Constrained motive node that carries a motive_ref
 * constraint. The constraint names the source motive node and the transformation; the operator writes the result as
 * the Realized melody lane span of the target and touches nothing else.
 */

#define PAYLOAD_MAGIC 0x4D445631UL
#define DEFAULT_CANDIDATES 6
#define ATTEMPTS_PER_CANDIDATE 8

/* The transformations each operator answers to, as bits of 1 << PlanMotiveTransform. */
#define TRANSFORM_BIT(transform) (1u << (transform))
#define FAMILY_RESTATE                                                                                       \
    (TRANSFORM_BIT(PLAN_TRANSFORM_RESTATED) | TRANSFORM_BIT(PLAN_TRANSFORM_TRANSPOSED) |                    \
     TRANSFORM_BIT(PLAN_TRANSFORM_INVERTED) | TRANSFORM_BIT(PLAN_TRANSFORM_RETROGRADE) |                    \
     TRANSFORM_BIT(PLAN_TRANSFORM_INTERVAL_CHANGED))

#define FAMILY_FRAGMENT (TRANSFORM_BIT(PLAN_TRANSFORM_FRAGMENTED) | TRANSFORM_BIT(PLAN_TRANSFORM_LIQUIDATED))

typedef struct {
    unsigned long magic;
    MotiveDevSpec spec;
    Motive result;
} Payload;

void motive_develop_context_default(MotiveDevelopContext *context) {
    memset(context, 0, sizeof(*context));
    context->meter.beats_per_bar = 4;
    context->meter.beat_unit = 4;
    context->candidates = DEFAULT_CANDIDATES;
}

static const MotiveDevelopContext *context_or_default(const void *context) {
    static MotiveDevelopContext defaults;
    static bool ready = false;
    if (context) {
        return context;
    }
    if (!ready) {
        motive_develop_context_default(&defaults);
        ready = true;
    }
    return &defaults;
}

/* --------------------------------------------------------------- request */

/* What a target asks for, read from the plan. */
typedef struct {
    PlanMotiveRef ref;
    Motive source;
    PlanTime start;
    PlanTime room; /* the length of the target's span */
} Request;

/* The first melody lane span (voice 0) of node that is Given or Realized. */
static bool melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            *out = ids[i];
            return true;
        }
    }
    return false;
}

/* The source motive: its notes, and the meter and key of the lanes over it, else the context's. */
static bool read_source(const PiecePlan *plan, NodeId node, const MotiveDevelopContext *context, Motive *out) {
    LaneSpanId span;
    if (!melody_span_of(plan, node, &span)) {
        return false;
    }
    const PlanLanePayload *payload = plan_lane_span_payload(plan, span);
    if (!payload || payload->notes.note_count == 0 || payload->notes.note_count > MOTIVE_MAX_NOTES) {
        return false;
    }
    Motive motive;
    memset(&motive, 0, sizeof(motive));
    motive.note_count = payload->notes.note_count;
    memcpy(motive.notes, payload->notes.notes, motive.note_count * sizeof(PlanNote));
    if (motive_check_notes(&motive, &motive.length, NULL) != MOTIVE_OK) {
        return false;
    }

    PlanSpan range = plan_lane_span_span(plan, span);
    LaneSpanId lane;
    motive.meter = context->meter;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, range, &lane, 1) > 0) {
        motive.meter = plan_lane_span_payload(plan, lane)->meter;
    }
    motive.key_source = MOTIVE_KEY_UNSPECIFIED;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, range, &lane, 1) > 0) {
        motive.key = plan_lane_span_payload(plan, lane)->key;
        motive.key_source = MOTIVE_KEY_GIVEN;
    } else if (context->has_key) {
        motive.key = context->key;
        motive.key_source = MOTIVE_KEY_GIVEN;
    }
    *out = motive;
    return true;
}

/*
 * The target must be an open motive node with a motive_ref constraint for one of the transformations in family, whose
 * source is another motive node with a melody. It must not have a melody of its own, and the melody lane must be free
 * over its span.
 */
static bool read_request(const PiecePlan *plan, FillTarget target, const MotiveDevelopContext *context,
                         unsigned family, Request *out) {
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_MOTIVE ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    PlanSpan span = plan_node_span(plan, target.id);
    LaneSpanId taken;
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, &taken, 1) > 0) {
        return false;
    }
    size_t count = plan_node_constraint_count(plan, target.id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF ||
            !(family & TRANSFORM_BIT(constraint->params.motive.transform)) ||
            constraint->params.motive.motive == target.id) {
            continue;
        }
        Request request;
        memset(&request, 0, sizeof(request));
        request.ref = constraint->params.motive;
        if (!read_source(plan, request.ref.motive, context, &request.source)) {
            continue;
        }
        request.start = span.start;
        request.room = span.length;
        *out = request;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------- proposing */

typedef struct {
    const Request *request;
    const MotiveDevelopContext *context;
    FillCandidateList *out;
    size_t added;
} Proposal;

static void payload_destroy(void *payload) {
    free(payload);
}

static bool already_proposed(const Proposal *proposal, const Motive *result) {
    for (size_t i = 0; i < proposal->out->count; i++) {
        const Payload *held = proposal->out->items[i].payload;
        if (held && held->magic == PAYLOAD_MAGIC && motive_equal(&held->result, result)) {
            return true;
        }
    }
    return false;
}

/*
 * Tries one transformation. A result that does not come out (nothing to change, a pitch out of range, a contour that
 * would change), that does not fit the target's span, or that an earlier candidate already is, is dropped without
 * comment. False only when out of memory.
 */
static bool try_spec(Proposal *proposal, const MotiveDevSpec *spec) {
    Motive result;
    if (motive_develop(&proposal->request->source, spec, &result) != MOTIVE_DEV_OK ||
        plan_time_compare(result.length, proposal->request->room) > 0 || already_proposed(proposal, &result)) {
        return true;
    }
    Payload *payload = calloc(1, sizeof(*payload));
    if (!payload) {
        return false;
    }
    payload->magic = PAYLOAD_MAGIC;
    payload->spec = *spec;
    payload->result = result;
    char label[FILL_LABEL_MAX];
    motive_dev_label(spec, label, sizeof(label));
    if (!fill_candidates_add(proposal->out, label, payload, payload_destroy)) {
        return false;
    }
    proposal->added++;
    return true;
}

/* A number from -magnitude to magnitude, never 0, from the run's stream. */
static int draw_nonzero(FillRng *rng, int magnitude) {
    int r = (int)fill_rng_below(rng, (uint64_t)(2 * magnitude));
    return r < magnitude ? r - magnitude : r - magnitude + 1;
}

static size_t wanted(const MotiveDevelopContext *context) {
    return context->candidates > 0 ? context->candidates : DEFAULT_CANDIDATES;
}

/* Draws transpositions until the wanted number of candidates is there. */
static bool propose_transpositions(Proposal *proposal, FillRng *rng, bool tonal, bool alternate) {
    size_t goal = proposal->added + wanted(proposal->context);
    bool key = proposal->request->source.key_source != MOTIVE_KEY_UNSPECIFIED;
    for (size_t attempt = 0; proposal->added < goal && attempt < goal * ATTEMPTS_PER_CANDIDATE; attempt++) {
        MotiveDevSpec spec;
        memset(&spec, 0, sizeof(spec));
        spec.kind = MOTIVE_DEV_TRANSPOSE;
        spec.tonal = tonal || (alternate && key && attempt % 2 == 1);
        spec.amount = draw_nonzero(rng, spec.tonal ? 7 : 12);
        if (!try_spec(proposal, &spec)) {
            return false;
        }
    }
    return true;
}

static bool propose_restatements(Proposal *proposal, FillRng *rng) {
    const PlanMotiveRef *ref = &proposal->request->ref;
    const Motive *source = &proposal->request->source;
    MotiveDevSpec spec;
    memset(&spec, 0, sizeof(spec));

    switch (ref->transform) {
    case PLAN_TRANSFORM_RESTATED:
        spec.kind = MOTIVE_DEV_EXACT;
        return try_spec(proposal, &spec) && propose_transpositions(proposal, rng, false, true);
    case PLAN_TRANSFORM_TRANSPOSED:
        spec.kind = MOTIVE_DEV_TRANSPOSE;
        spec.tonal = ref->tonal;
        spec.amount = ref->tonal ? ref->steps : ref->semitones;
        if (spec.amount != 0) {
            return try_spec(proposal, &spec);
        }
        return propose_transpositions(proposal, rng, ref->tonal, false);
    case PLAN_TRANSFORM_INVERTED: {
        spec.kind = MOTIVE_DEV_INVERT;
        spec.tonal = ref->tonal;
        if (ref->has_axis) {
            spec.has_axis = true;
            spec.axis = ref->axis;
            return try_spec(proposal, &spec);
        }
        /* The first note, then axes inside the motive's range. */
        if (!try_spec(proposal, &spec)) {
            return false;
        }
        int lowest = source->notes[0].midi_pitch;
        int highest = lowest;
        for (size_t i = 1; i < source->note_count; i++) {
            lowest = source->notes[i].midi_pitch < lowest ? source->notes[i].midi_pitch : lowest;
            highest = source->notes[i].midi_pitch > highest ? source->notes[i].midi_pitch : highest;
        }
        size_t goal = proposal->added + wanted(proposal->context) - 1;
        for (size_t attempt = 0; proposal->added < goal && attempt < goal * ATTEMPTS_PER_CANDIDATE; attempt++) {
            spec.has_axis = true;
            spec.axis = lowest + (int)fill_rng_below(rng, (uint64_t)(highest - lowest + 1));
            if (!try_spec(proposal, &spec)) {
                return false;
            }
        }
        return true;
    }
    case PLAN_TRANSFORM_RETROGRADE:
        spec.kind = MOTIVE_DEV_RETROGRADE;
        return try_spec(proposal, &spec);
    case PLAN_TRANSFORM_INTERVAL_CHANGED: {
        spec.kind = MOTIVE_DEV_INTERVAL_CHANGE;
        spec.keep_contour = ref->keep_contour;
        if (ref->amount != 0) {
            spec.interval = ref->interval;
            spec.amount = ref->amount;
            return try_spec(proposal, &spec);
        }
        size_t goal = proposal->added + wanted(proposal->context);
        for (size_t attempt = 0; proposal->added < goal && attempt < goal * ATTEMPTS_PER_CANDIDATE; attempt++) {
            spec.interval = (int)fill_rng_below(rng, (uint64_t)source->note_count);
            spec.amount = draw_nonzero(rng, 2);
            if (!try_spec(proposal, &spec)) {
                return false;
            }
        }
        return true;
    }
    default:
        return true;
    }
}

/*
 * Fragments: the positions and lengths the constraint leaves open are drawn, as many as the candidates asked for.
 * Liquidations: every number of notes dropped, from one up to the most that leaves the minimum, since each is a step of
 * the same progressive loss.
 */
static bool propose_fragments(Proposal *proposal, FillRng *rng) {
    const PlanMotiveRef *ref = &proposal->request->ref;
    size_t total = proposal->request->source.note_count;
    size_t minimum = proposal->context->min_length > 0 ? proposal->context->min_length : MOTIVE_DEV_MIN_LENGTH;
    MotiveDevSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.min_length = proposal->context->min_length;

    if (ref->transform == PLAN_TRANSFORM_LIQUIDATED) {
        spec.kind = MOTIVE_DEV_LIQUIDATE;
        spec.keep_rhythm = ref->keep_rhythm;
        if (ref->drop > 0) {
            spec.drop = (size_t)ref->drop;
            return try_spec(proposal, &spec);
        }
        for (size_t drop = 1; total > drop && total - drop >= minimum; drop++) {
            spec.drop = drop;
            if (!try_spec(proposal, &spec)) {
                return false;
            }
        }
        return true;
    }

    spec.kind = MOTIVE_DEV_FRAGMENT;
    if (ref->position != PLAN_FRAGMENT_ANY && ref->length > 0) {
        spec.position = ref->position;
        spec.length = (size_t)ref->length;
        return try_spec(proposal, &spec);
    }
    if (total <= minimum) {
        return true; /* no fragment is shorter than the source and long enough */
    }
    size_t goal = proposal->added + wanted(proposal->context);
    for (size_t attempt = 0; proposal->added < goal && attempt < goal * ATTEMPTS_PER_CANDIDATE; attempt++) {
        spec.position = ref->position != PLAN_FRAGMENT_ANY
                            ? ref->position
                            : (PlanFragmentPosition)(PLAN_FRAGMENT_LEADING + fill_rng_below(rng, 3));
        spec.length = ref->length > 0 ? (size_t)ref->length
                                      : minimum + (size_t)fill_rng_below(rng, (uint64_t)(total - minimum));
        if (!try_spec(proposal, &spec)) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------- the hooks */

static bool restate_applies(const PiecePlan *plan, FillTarget target, void *context) {
    Request request;
    return read_request(plan, target, context_or_default(context), FAMILY_RESTATE, &request);
}

static bool restate_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                             FillCandidateList *out) {
    const MotiveDevelopContext *shared = context_or_default(context);
    Request request;
    if (!read_request(plan, target, shared, FAMILY_RESTATE, &request)) {
        return true;
    }
    Proposal proposal = {&request, shared, out, 0};
    return propose_restatements(&proposal, rng);
}

static bool fragment_applies(const PiecePlan *plan, FillTarget target, void *context) {
    Request request;
    return read_request(plan, target, context_or_default(context), FAMILY_FRAGMENT, &request);
}

static bool fragment_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                              FillCandidateList *out) {
    const MotiveDevelopContext *shared = context_or_default(context);
    Request request;
    if (!read_request(plan, target, shared, FAMILY_FRAGMENT, &request)) {
        return true;
    }
    Proposal proposal = {&request, shared, out, 0};
    return propose_fragments(&proposal, rng);
}

static PlanResult develop_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    const Payload *payload = candidate->payload;
    if (!payload || payload->magic != PAYLOAD_MAGIC) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanSpan span = {plan_node_span(plan, target.id).start, payload->result.length};
    PlanLanePayload notes;
    notes.notes.notes = payload->result.notes;
    notes.notes.note_count = payload->result.note_count;
    LaneSpanId id;
    PlanResult result = plan_add_lane_span(plan, target.id, PLAN_LANE_MELODY, 0, span, &notes, &id);
    return result == PLAN_OK ? plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED) : result;
}

static FillResult register_operator(FillOperatorRegistry *registry, const char *name, const char *description,
                                    bool (*applies)(const PiecePlan *, FillTarget, void *),
                                    bool (*generate)(const PiecePlan *, FillTarget, FillRng *, void *,
                                                     FillCandidateList *),
                                    const MotiveDevelopContext *context) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = name;
    operator_.generator.supported_inputs = "PiecePlan motive node with a motive_ref constraint";
    operator_.generator.supported_outputs = "PiecePlan melody lane span of the motive node";
    operator_.generator.configuration_description = description;
    operator_.generator.constraints_description = "the motive_ref constraint's transformation and parameters";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_GROW;
    operator_.applies = applies;
    operator_.generate = generate;
    operator_.apply = develop_apply;
    operator_.context = (void *)context;
    return fill_registry_register(registry, &operator_);
}

FillResult motive_develop_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context) {
    if (!registry) {
        return FILL_ERR_NULL;
    }
    FillResult result =
        register_operator(registry, MOTIVE_RESTATE_OPERATOR_NAME,
                          "restatement by exact repetition, transposition, inversion, retrograde or interval change",
                          restate_applies, restate_generate, context);
    return result != FILL_OK ? result
                             : register_operator(registry, MOTIVE_FRAGMENT_OPERATOR_NAME,
                                                 "fragments and liquidations with a minimum length",
                                                 fragment_applies, fragment_generate, context);
}

/* ------------------------------------------------------------ candidates */

const Motive *motive_develop_candidate_motive(const FillCandidate *candidate) {
    const Payload *payload = candidate ? candidate->payload : NULL;
    return payload && payload->magic == PAYLOAD_MAGIC ? &payload->result : NULL;
}

const MotiveDevSpec *motive_develop_candidate_spec(const FillCandidate *candidate) {
    const Payload *payload = candidate ? candidate->payload : NULL;
    return payload && payload->magic == PAYLOAD_MAGIC ? &payload->spec : NULL;
}
