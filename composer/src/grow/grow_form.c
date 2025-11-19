#include <ursatz_composer/grow_form.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_connect.h>
#include <ursatz_composer/variant_registry.h>

#include "grow/grow_internal.h"
#include "motive/motive_internal.h"

#define FORM_MAGIC 0x47464d31UL
/* The pitches a form may use, so the melody has room to place it (the contrasting ideas' range). */
#define FORM_LOW 58
#define FORM_HIGH 84
/* Draws of an operation's open parameters tried for each candidate, from fixed seeds. */
#define FORM_SEEDS 3
/* Candidates one slot offers at most. */
#define FORM_MAX 96
#define FORM_IDEAS 16

typedef enum {
    SLOT_NONE,
    SLOT_REPETITION,
    SLOT_RESTATED,
    SLOT_CONTRAST,
    SLOT_FRAGMENT_1,
    SLOT_FRAGMENT_2,
    SLOT_CADENTIAL
} Slot;

typedef struct {
    unsigned long magic;
    Motive result;
    VariantChain chain;
    VariantRecord record;
    MotiveConnection connection;
} Payload;

/* What a target asks for, read from the plan. */
typedef struct {
    Slot slot;
    PlanMotiveRef ref;
    Motive basic;    /* the motive the idea is made from, with m0's harmony, features and role */
    Motive previous; /* the idea before this one, else basic */
    Motive taken[4]; /* the motives already written for other ideas of the theme (m1 to m4) */
    size_t taken_count;
    PlanTime room;   /* the longest a form may be: the motive node's bar, or the idea if it is shorter */
    PlanSpan idea;
    bool has_cadence; /* the idea's melody ends in a cadence */
    PlanCadenceType cadence;
} Request;

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

static Slot slot_of(const char *name) {
    static const struct {
        const char *name;
        Slot slot;
    } SLOTS[] = {{"repetition", SLOT_REPETITION},
                 {"consequent basic idea", SLOT_RESTATED},
                 {"contrasting idea", SLOT_CONTRAST},
                 {"consequent contrasting idea", SLOT_CONTRAST},
                 {"fragment 1", SLOT_FRAGMENT_1},
                 {"fragment 2", SLOT_FRAGMENT_2},
                 {"cadential", SLOT_CADENTIAL}                 };
    for (size_t i = 0; name && i < sizeof(SLOTS) / sizeof(SLOTS[0]); i++) {
        if (strcmp(name, SLOTS[i].name) == 0) {
            return SLOTS[i].slot;
        }
    }
    return SLOT_NONE;
}

/* The motive_ref of the idea's melody span (voice 0), and that span's. False when the idea has none. */
static bool idea_melody(const PiecePlan *plan, NodeId idea, PlanMotiveRef *ref, PlanSpan *span,
                        const PlanCadenceType **cadence) {
    LaneSpanId ids[FORM_IDEAS];
    size_t found = plan_node_lane_spans(plan, idea, ids, FORM_IDEAS);
    for (size_t i = 0; i < found && i < FORM_IDEAS; i++) {
        if (plan_lane_span_lane(plan, ids[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, ids[i]) != 0) {
            continue;
        }
        bool found_ref = false;
        for (size_t k = 0; k < plan_lane_span_constraint_count(plan, ids[i]); k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, ids[i], k);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
                *ref = constraint->params.motive;
                *span = plan_lane_span_span(plan, ids[i]);
                found_ref = true;
            } else if (constraint->kind == PLAN_CONSTRAINT_CADENCE && cadence) {
                *cadence = &constraint->params.cadence;
            }
        }
        if (found_ref) {
            return true;
        }
    }
    return false;
}

/* m0 with the harmony, features and role of the context's basic motive, when its notes are m0's. */
static void with_extras(const PiecePlan *plan, NodeId node, const MotiveDevelopContext *context, Motive *motive) {
    const Motive *basic = context->basic;
    if (!basic || node != grow_find_source(plan) || basic->note_count != motive->note_count) {
        return;
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        if (basic->notes[i].midi_pitch != motive->notes[i].midi_pitch ||
            plan_time_compare(basic->notes[i].offset, motive->notes[i].offset) != 0 ||
            plan_time_compare(basic->notes[i].duration, motive->notes[i].duration) != 0) {
            return;
        }
    }
    memcpy(motive->harmony, basic->harmony, sizeof(motive->harmony));
    motive->harmony_count = basic->harmony_count;
    memcpy(motive->features, basic->features, sizeof(motive->features));
    motive->feature_count = basic->feature_count;
    motive->role = basic->role;
    motive->pickup = basic->pickup;
}

/* A motive node's motive, with the extras m0 has. */
static bool read_node(const PiecePlan *plan, NodeId node, const MotiveDevelopContext *context, Motive *out) {
    if (!motive_develop_read_node(plan, node, context, out)) {
        return false;
    }
    with_extras(plan, node, context, out);
    return true;
}

static bool read_request(const PiecePlan *plan, FillTarget target, const MotiveDevelopContext *context, Request *out) {
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_MOTIVE ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED) {
        return false;
    }
    PlanSpan node_span = plan_node_span(plan, target.id);
    LaneSpanId taken;
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, node_span, &taken, 1) > 0) {
        return false;
    }
    Request request;
    memset(&request, 0, sizeof(request));
    bool has_ref = false;
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id) && !has_ref; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive != target.id) {
            request.ref = constraint->params.motive;
            has_ref = true;
        }
    }
    if (!has_ref || !read_node(plan, request.ref.motive, context, &request.basic)) {
        return false;
    }

    /* The idea whose melody asks for this node, and the idea right before it. */
    NodeId idea = PLAN_NODE_NONE;
    PlanSpan idea_span = {{0, 1}, {0, 1}};
    for (NodeId id = 1; id <= plan_node_count(plan) && idea == PLAN_NODE_NONE; id++) {
        PlanMotiveRef ref;
        const PlanCadenceType *cadence = NULL;
        if (plan_node_kind(plan, id) == PLAN_NODE_IDEA && idea_melody(plan, id, &ref, &idea_span, &cadence) &&
            ref.motive == target.id) {
            idea = id;
            request.has_cadence = cadence != NULL;
            request.cadence = cadence ? *cadence : PLAN_CADENCE_PAC;
        }
    }
    if (idea == PLAN_NODE_NONE) {
        return false;
    }
    request.slot = slot_of(plan_node_name(plan, idea));
    if (request.slot == SLOT_NONE) {
        return false;
    }
    request.idea = plan_node_span(plan, idea);
    request.room = plan_time_compare(request.idea.length, node_span.length) < 0 ? request.idea.length : node_span.length;
    request.previous = request.basic;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        PlanMotiveRef ref;
        PlanSpan span;
        PlanTime end;
        if (id != idea && plan_node_kind(plan, id) == PLAN_NODE_IDEA && idea_melody(plan, id, &ref, &span, NULL) &&
            plan_span_end(span, &end) == PLAN_OK && plan_time_compare(end, request.idea.start) == 0 &&
            ref.motive != request.ref.motive) {
            Motive before;
            if (read_node(plan, ref.motive, context, &before)) {
                request.previous = before;
            }
        }
    }
    for (NodeId id = 1; id <= plan_node_count(plan) && request.taken_count < 4; id++) {
        const char *name = plan_node_name(plan, id);
        if (id != target.id && plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && name[0] == 'm' && name[1] != '0' &&
            name[2] == '\0' && read_node(plan, id, context, &request.taken[request.taken_count])) {
            request.taken_count++;
        }
    }
    *out = request;
    return true;
}

/* ------------------------------------------------------------- candidates */

typedef struct {
    const Request *request;
    FillCandidateList *out;
} Proposal;

static void payload_destroy(void *payload) {
    free(payload);
}

/* Every pitch within the range, and in the key's scale (a minor key also has the raised seventh). */
static bool within_range(const Motive *motive) {
    static const bool MAJOR[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    static const bool MINOR[12] = {true, false, true, true, false, true, false, true, true, false, true, true};
    for (size_t i = 0; i < motive->note_count; i++) {
        int pitch = motive->notes[i].midi_pitch;
        if (pitch < FORM_LOW || pitch > FORM_HIGH) {
            return false;
        }
        if (motive->key_source != MOTIVE_KEY_UNSPECIFIED) {
            int degree = ((pitch - motive->key.tonic_pitch_class) % 12 + 12) % 12;
            if (!(motive->key.mode == PLAN_MODE_MINOR ? MINOR : MAJOR)[degree]) {
                return false;
            }
        }
    }
    return true;
}

static bool same_pitches(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi_pitch != b->notes[i].midi_pitch) {
            return false;
        }
    }
    return true;
}

static bool has_function(const char *const *functions, size_t count, const char *wanted) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(functions[i], wanted) == 0) {
            return true;
        }
    }
    return false;
}

/* The label of a candidate: its chain. */
static void label_of(const Payload *payload, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t s = 0; s < payload->record.step_count && used < size; s++) {
        used += (size_t)snprintf(out + used, size - used, "%s%s", s > 0 ? " then " : "", payload->record.steps[s].op);
    }
}

/*
 * Offers the result of a chain applied to the basic idea if it is a form that fits: a full motive in range, no longer than
 * the room, not an earlier candidate, with the function asked for (NULL for any) and, with contrast, a contour other
 * than the basic idea's. False only when out of memory.
 */
static bool offer(Proposal *proposal, const VariantChain *chain, unsigned long seed, const char *function, bool contrast) {
    const Request *request = proposal->request;
    Motive result;
    VariantRecord record;
    if (proposal->out->count >= FORM_MAX ||
        variant_apply_chain(&request->basic, chain, seed, &result, &record) != VARIANT_OK ||
        result.form != MOTIVE_FORM_FULL || result.note_count != request->basic.note_count || !within_range(&result) ||
        plan_time_compare(result.length, request->room) > 0 ||
        (contrast && grow_same_contour(&request->basic, &result))) {
        return true;
    }
    for (size_t i = 0; i < proposal->out->count; i++) {
        const Payload *held = proposal->out->items[i].payload;
        if (motive_equal(&held->result, &result)) {
            return true;
        }
    }
    /* An idea that ends in a cadence ends on its tone: the tonic for a PAC, the third for an IAC. */
    if (request->has_cadence && result.key_source != MOTIVE_KEY_UNSPECIFIED) {
        int tonic = result.key.tonic_pitch_class;
        int last = ((result.notes[result.note_count - 1].midi_pitch - tonic) % 12 + 12) % 12;
        int third = result.key.mode == PLAN_MODE_MINOR ? 3 : 4;
        if ((request->cadence == PLAN_CADENCE_PAC && last != 0) || (request->cadence == PLAN_CADENCE_IAC && last != third)) {
            return true;
        }
    }
    /* A contrasting idea is not another idea's again. */
    for (size_t i = 0; contrast && i < request->taken_count; i++) {
        if (same_pitches(&request->taken[i], &result)) {
            return true;
        }
    }
    /* Remoteness from the basic idea, from the chain that made the form: no search. */
    MotiveScore distance;
    if (!motive_chain_distance(chain, &distance)) {
        return true;
    }
    record.remoteness_num = distance.numerator;
    record.remoteness_den = distance.denominator;
    if (function) {
        const char *functions[VARIANT_FUNCTIONS];
        size_t count = 0;
        if (!variant_functions(&request->basic, &result, &record, functions, &count) ||
            !has_function(functions, count, function)) {
            return true;
        }
    }
    Payload *payload = calloc(1, sizeof(*payload));
    if (!payload) {
        return false;
    }
    payload->magic = FORM_MAGIC;
    payload->result = result;
    payload->chain = *chain;
    payload->record = record;
    if (!motive_connection(&request->basic, &request->previous, &result, &distance, &payload->connection)) {
        free(payload);
        return true;
    }
    char label[FILL_LABEL_MAX];
    label_of(payload, label, sizeof(label));
    return fill_candidates_add(proposal->out, label, payload, payload_destroy);
}

/* One-step chains of every operation the basic idea takes (never exact), open parameters drawn from fixed seeds. */
static bool offer_operations(Proposal *proposal, const char *function, bool contrast) {
    for (size_t o = 0; o < variant_registry_count(); o++) {
        const VariantOperation *operation = variant_registry_at(o);
        if (strcmp(operation->name, "exact") == 0 ||
            (operation->forms & VARIANT_FORM_BIT(proposal->request->basic.form)) == 0) {
            continue;
        }
        for (unsigned long seed = 1; seed <= FORM_SEEDS; seed++) {
            VariantChain chain;
            memset(&chain, 0, sizeof(chain));
            if (variant_step_init(&chain.steps[0], operation->name)) {
                chain.step_count = 1;
                if (!offer(proposal, &chain, seed * 7919UL + (unsigned long)o, function, contrast)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool offer_exact(Proposal *proposal) {
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.step_count = 1;
    return variant_step_init(&chain.steps[0], "exact") ? offer(proposal, &chain, 1, NULL, false) : true;
}

/* Transpositions of the basic idea by steps of its key's scale. */
static bool offer_transpositions(Proposal *proposal) {
    static const long STEPS[] = {1, 2, 3, -1, -2, -3};
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
        VariantChain chain;
        memset(&chain, 0, sizeof(chain));
        chain.step_count = 1;
        if (variant_step_init(&chain.steps[0], "transpose") && variant_step_set(&chain.steps[0], "tonal", 1, 0) &&
            variant_step_set(&chain.steps[0], "by", STEPS[i], 0) &&
            !offer(proposal, &chain, 1, NULL, false)) {
            return false;
        }
    }
    return true;
}

static bool form_applies(const PiecePlan *plan, FillTarget target, void *context) {
    Request request;
    return read_request(plan, target, context_or_default(context), &request);
}

static bool form_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    (void)rng;
    Request request;
    if (!read_request(plan, target, context_or_default(context), &request)) {
        return true;
    }
    Proposal proposal = {&request, out};
    switch (request.slot) {
    case SLOT_RESTATED:
        return offer_exact(&proposal);
    case SLOT_REPETITION:
        /* A restated repetition (the melody restates the basic idea) stays exact; a response is a transposition. */
        return request.ref.transform == PLAN_TRANSFORM_RESTATED ? offer_exact(&proposal)
                                                                : offer_transpositions(&proposal);
    case SLOT_CONTRAST:
        return offer_operations(&proposal, "contrast", true);
    case SLOT_CADENTIAL:
        return offer_operations(&proposal, "cadential", false);
    case SLOT_FRAGMENT_1:
    case SLOT_FRAGMENT_2:
    case SLOT_NONE:
        break;
    }
    return true;
}

static PlanResult form_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    const Payload *payload = candidate->payload;
    if (!payload || payload->magic != FORM_MAGIC) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanSpan span = {plan_node_span(plan, target.id).start, payload->result.length};
    PlanLanePayload notes;
    notes.notes.notes = (PlanNote *)payload->result.notes;
    notes.notes.note_count = payload->result.note_count;
    LaneSpanId id;
    PlanResult result = plan_add_lane_span(plan, target.id, PLAN_LANE_MELODY, 0, span, &notes, &id);
    return result == PLAN_OK ? plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED) : result;
}

FillResult grow_form_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context) {
    FillOperator operator_;
    if (!registry) {
        return FILL_ERR_NULL;
    }
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = GROW_FORM_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan motive node of a grown theme's idea";
    operator_.generator.supported_outputs = "PiecePlan melody lane span of the motive node";
    operator_.generator.configuration_description = "none";
    operator_.generator.constraints_description = "the slot of the idea whose melody asks for the motive";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_GROW;
    operator_.applies = form_applies;
    operator_.generate = form_generate;
    operator_.apply = form_apply;
    operator_.context = (void *)context;
    return fill_registry_register(registry, &operator_);
}

bool grow_motives_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out) {
    const Payload *payload = candidate ? candidate->payload : NULL;
    if (!payload || payload->magic != FORM_MAGIC) {
        return motive_develop_checker(plan, target, operator_, candidate, context, out);
    }
    memset(out, 0, sizeof(*out));
    out->score_numerator = payload->connection.total.numerator + payload->connection.total.denominator;
    out->score_denominator = payload->connection.total.denominator;
    return true;
}

const Motive *grow_form_candidate_motive(const FillCandidate *candidate) {
    const Payload *payload = candidate ? candidate->payload : NULL;
    return payload && payload->magic == FORM_MAGIC ? &payload->result : NULL;
}
