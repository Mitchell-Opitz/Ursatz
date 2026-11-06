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
/* The widest leap a form may have, a fifth, unless the basic idea has a wider one. */
#define FORM_LEAP 7
/* Draws of an operation's open parameters tried for each candidate, from fixed seeds. */
#define FORM_SEEDS 3
/* The step between the seeds of one operation's draws (any works). */
#define FORM_SEED_STEP 7919UL
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
    SLOT_CADENTIAL,
    SLOT_CADENTIAL_ENDING /* cadential 1 and 2: made by the P13 operators, but a slot's own motive is restated exactly */
} Slot;

typedef struct {
    unsigned long magic;
    Motive result;
    VariantChain chain;
    VariantRecord record;
    MotiveConnection connection;
    /* A pool form (grow_form.h) has no chain of registry steps: its item, its chain as saved, and the bonus in 1/4s
       (2 approved, 1 unflagged); 0 for a registry form. */
    unsigned pool_bonus;
    char item[64];
    char pool_chain[256];
} Payload;

/* What a target asks for, read from the plan. */
typedef struct {
    Slot slot;
    GrowSlot function_slot; /* the slot whose function the idea has (grow.h) */
    GrowSlot origin;        /* the slot whose motive the idea is made from: basic for m0 and the section's own */
    PlanMotiveRef ref;
    Motive basic;    /* the motive the idea is made from, with m0's harmony, features and role */
    Motive previous; /* the idea before this one, else basic */
    Motive taken[4]; /* the motives already written for other ideas of the theme (m1 to m4) */
    size_t taken_count;
    Rational room;   /* the longest a form may be: the motive node's bar, or the idea if it is shorter */
    PlanSpan idea;
    bool answers_in_dominant; /* the idea's chords start on the dominant: a response to the basic idea */
    bool has_cadence; /* the idea's melody ends in a cadence */
    CadenceType cadence;
    MotiveScore limit; /* the remoteness over which a form's connection is halved; denominator 0 is 3/4 */
} Request;

static MotiveDevelopContext context_or_default(const void *context) {
    if (context) {
        return *(const MotiveDevelopContext *)context;
    }
    MotiveDevelopContext defaults;
    motive_develop_context_default(&defaults);
    return defaults;
}

static Slot slot_of(const char *name, GrowSlot *function_slot) {
    static const struct {
        const char *name;
        Slot slot;
        GrowSlot function_slot;
    } SLOTS[] = {{"repetition", SLOT_REPETITION, GROW_SLOT_BASIC},
                 {"consequent basic idea", SLOT_RESTATED, GROW_SLOT_BASIC},
                 {"contrasting idea", SLOT_CONTRAST, GROW_SLOT_CONTRASTING},
                 {"consequent contrasting idea", SLOT_CONTRAST, GROW_SLOT_CONTRASTING},
                 {"fragment 1", SLOT_FRAGMENT_1, GROW_SLOT_CONTINUATION},
                 {"fragment 2", SLOT_FRAGMENT_2, GROW_SLOT_CONTINUATION},
                 {"cadential", SLOT_CADENTIAL, GROW_SLOT_CADENTIAL},
                 {"cadential 1", SLOT_CADENTIAL_ENDING, GROW_SLOT_CADENTIAL},
                 {"cadential 2", SLOT_CADENTIAL_ENDING, GROW_SLOT_CADENTIAL}};
    for (size_t i = 0; name && i < sizeof(SLOTS) / sizeof(SLOTS[0]); i++) {
        if (strcmp(name, SLOTS[i].name) == 0) {
            *function_slot = SLOTS[i].function_slot;
            return SLOTS[i].slot;
        }
    }
    return SLOT_NONE;
}

unsigned grow_slot_function_bit(GrowSlot slot) {
    switch (slot) {
    case GROW_SLOT_BASIC:
        return GROW_FUNCTION_BEGINNING;
    case GROW_SLOT_CONTRASTING:
        return GROW_FUNCTION_CONTRAST;
    case GROW_SLOT_CONTINUATION:
        return GROW_FUNCTION_CONTINUATION;
    case GROW_SLOT_CADENTIAL:
        return GROW_FUNCTION_CADENTIAL;
    case GROW_SLOT_COUNT:
        break;
    }
    return 0;
}

/* The motive_ref of the idea's melody span (voice 0), and that span's. False when the idea has none. */
static bool idea_melody(const PiecePlan *plan, NodeId idea, PlanMotiveRef *ref, PlanSpan *span,
                        const CadenceType **cadence) {
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
        if (motive_pitch(basic, i) != motive_pitch(motive, i) ||
            rational_cmp(basic->notes[i].onset, motive->notes[i].onset) != 0 ||
            rational_cmp(basic->notes[i].duration, motive->notes[i].duration) != 0) {
            return;
        }
    }
    memcpy(motive->chords, basic->chords, sizeof(motive->chords));
    motive->chord_count = basic->chord_count;
    memcpy(motive->features, basic->features, sizeof(motive->features));
    motive->feature_count = basic->feature_count;
    motive->accompaniment = basic->accompaniment;
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
        const CadenceType *cadence = NULL;
        if (plan_node_kind(plan, id) == PLAN_NODE_IDEA && idea_melody(plan, id, &ref, &idea_span, &cadence) &&
            ref.motive == target.id) {
            idea = id;
            request.has_cadence = cadence != NULL;
            request.cadence = cadence ? *cadence : CADENCE_TYPE_PERFECT_AUTHENTIC;
        }
    }
    if (idea == PLAN_NODE_NONE) {
        return false;
    }
    request.slot = slot_of(plan_node_name(plan, idea), &request.function_slot);
    if (request.slot == SLOT_NONE) {
        return false;
    }
    /* The motive the idea is made from is a slot's own when the ref names its node. */
    for (GrowSlot own = GROW_SLOT_CONTRASTING; own < GROW_SLOT_COUNT; own++) {
        if (grow_find_slot_node(plan, own) == request.ref.motive) {
            request.origin = own;
        }
    }
    request.idea = plan_node_span(plan, idea);
    LaneSpanId lanes[FORM_IDEAS];
    size_t lane_count = plan_node_lane_spans(plan, idea, lanes, FORM_IDEAS);
    for (size_t i = 0; i < lane_count && i < FORM_IDEAS; i++) {
        for (size_t k = 0; plan_lane_span_lane(plan, lanes[i]) == PLAN_LANE_HARMONY &&
                           k < plan_lane_span_constraint_count(plan, lanes[i]);
             k++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, lanes[i], k);
            if (constraint->kind == PLAN_CONSTRAINT_HARMONY_SHAPE &&
                constraint->params.harmony_shape.starts_on == PLAN_HARMONY_ANCHOR_DOMINANT) {
                request.answers_in_dominant = true;
            }
        }
    }
    request.room = rational_cmp(request.idea.length, node_span.length) < 0 ? request.idea.length : node_span.length;
    request.previous = request.basic;
    request.limit = context->remoteness_limit;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        PlanMotiveRef ref;
        PlanSpan span;
        Rational end;
        if (id != idea && plan_node_kind(plan, id) == PLAN_NODE_IDEA && idea_melody(plan, id, &ref, &span, NULL) &&
            plan_span_end(span, &end) == PLAN_OK && rational_cmp(end, request.idea.start) == 0 &&
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

/* Every pitch within the range, and in the key's scale (a minor key also has the raised seventh). */
static bool within_range(const Motive *motive) {
    static const bool MAJOR[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    static const bool MINOR[12] = {true, false, true, true, false, true, false, true, true, false, true, true};
    for (size_t i = 0; i < motive->note_count; i++) {
        int pitch = motive_pitch(motive, i);
        if (pitch < FORM_LOW || pitch > FORM_HIGH) {
            return false;
        }
        if (motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED) {
            int degree = ((pitch - motive->tonic_pc) % 12 + 12) % 12;
            if (!(motive->minor ? MINOR : MAJOR)[degree]) {
                return false;
            }
        }
    }
    return true;
}

static int widest_leap(const Motive *motive) {
    int widest = 0;
    for (size_t i = 1; i < motive->note_count; i++) {
        int leap = motive_pitch(motive, i) - motive_pitch(motive, i - 1);
        widest = (leap < 0 ? -leap : leap) > widest ? (leap < 0 ? -leap : leap) : widest;
    }
    return widest;
}

static bool same_pitches(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (motive_pitch(a, i) != motive_pitch(b, i)) {
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

/*
 * The label of a candidate, as the record and --explain show it: the chain, the connection score as a percentage, and the
 * derivation in plain words (after the remoteness limit when it is not 3/4), cut where the label ends. The record's score
 * is 1 plus the connection (grow_motives_checker).
 */
static void label_of(const Payload *payload, char *out, size_t size) {
    if (payload->pool_bonus > 0) {
        snprintf(out, size, "pool %s %lu%%: %s", payload->item,
                 payload->connection.total.numerator * 100 / payload->connection.total.denominator, payload->pool_chain);
        return;
    }
    char chain[FILL_LABEL_MAX];
    size_t used = 0;
    chain[0] = '\0';
    for (size_t s = 0; s < payload->record.step_count && used < sizeof(chain); s++) {
        used += (size_t)snprintf(chain + used, sizeof(chain) - used, "%s%s", s > 0 ? " then " : "", payload->record.steps[s].op);
    }
    const MotiveScore *total = &payload->connection.total;
    /* The limit is named when it is not the usual 3/4 (a fast tempo, R-163). */
    char limit[24];
    limit[0] = '\0';
    if (payload->connection.limit.numerator * 4 != payload->connection.limit.denominator * 3) {
        snprintf(limit, sizeof(limit), "limit %lu/%lu; ", payload->connection.limit.numerator,
                 payload->connection.limit.denominator);
    }
    snprintf(out, size, "%s %lu%%: %s%s", chain, total->numerator * 100 / total->denominator, limit,
             payload->record.derivation);
}

/* Whether a motive fits the idea: a full motive in range, no wider a leap than the basic idea's (a fifth at least) and no
   longer than the idea's room. */
static bool form_fits(const Request *request, const Motive *motive) {
    int allowed = widest_leap(&request->basic) > FORM_LEAP ? widest_leap(&request->basic) : FORM_LEAP;
    return motive->form == MOTIVE_SKETCH_FORM_FULL && within_range(motive) && widest_leap(motive) <= allowed &&
           rational_cmp(motive->length, request->room) <= 0;
}

/* An idea that ends in a cadence ends on its tone: the tonic for a PAC, the third for an IAC. */
static bool cadence_tone_holds(const Request *request, const Motive *motive) {
    if (!request->has_cadence || motive->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        return true;
    }
    int tonic = motive->tonic_pc;
    int last = ((motive_pitch(motive, motive->note_count - 1) - tonic) % 12 + 12) % 12;
    int third = motive->minor ? 3 : 4;
    return !((request->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC && last != 0) || (request->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC && last != third));
}

/*
 * Offers the result of a chain applied to the basic idea if it is a form that fits: a full motive in range, no longer than
 * the room, not an earlier candidate, with the function asked for (NULL for any) and, with contrast, a contour other
 * than the basic idea's. False only when out of memory.
 */
static bool offer(Proposal *proposal, const VariantChain *chain, unsigned long seed, const char *function, bool contrast) {
    const Request *request = proposal->request;
    bool fragment = request->slot == SLOT_FRAGMENT_1 || request->slot == SLOT_FRAGMENT_2 ||
                    request->slot == SLOT_CADENTIAL || request->slot == SLOT_CADENTIAL_ENDING;
    Motive result;
    VariantRecord record;
    if (proposal->out->count >= FORM_MAX ||
        variant_apply_chain(&request->basic, chain, seed, &result, &record) != VARIANT_OK) {
        return true;
    }
    /* A form that lies below the range (the dominant form of a low motive) is taken up an octave. */
    int lowest = 127;
    int highest = 0;
    for (size_t i = 0; i < result.note_count; i++) {
        lowest = motive_pitch(&result, i) < lowest ? motive_pitch(&result, i) : lowest;
        highest = motive_pitch(&result, i) > highest ? motive_pitch(&result, i) : highest;
    }
    if (lowest < FORM_LOW && highest + 12 <= FORM_HIGH) {
        for (size_t i = 0; i < result.note_count; i++) {
            if (result.notes[i].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
                result.notes[i].pitch += 12;
            }
        }
    }
    if (!form_fits(request, &result) || (!fragment && result.note_count != request->basic.note_count) ||
        (contrast && grow_same_contour(&request->basic, &result))) {
        return true;
    }
    for (size_t i = 0; i < proposal->out->count; i++) {
        const Payload *held = proposal->out->items[i].payload;
        if (motive_equal(&held->result, &result)) {
            return true;
        }
    }
    if (!cadence_tone_holds(request, &result)) {
        return true;
    }
    /* A cadential idea is a liquidation: fewer notes than the basic idea, as the P13 operators make it. */
    if (request->slot == SLOT_CADENTIAL && !plan_motive_ref_restates(&request->ref) &&
        result.note_count >= request->basic.note_count) {
        return true;
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
    if (!motive_connection_limited(&request->basic, &request->previous, &result, &distance, &request->limit,
                                   &payload->connection)) {
        free(payload);
        return true;
    }
    char label[FILL_LABEL_MAX];
    label_of(payload, label, sizeof(label));
    return fill_candidates_add(proposal->out, label, payload, motive_payload_destroy);
}

/*
 * Offers the pool's forms that suit the idea (grow_form.h): approved ones before unflagged ones, so that when two are
 * the same motive the approved one is the candidate. False only when out of memory.
 */
static bool offer_pool(Proposal *proposal, const GrowPoolForm *pool, size_t pool_count) {
    const Request *request = proposal->request;
    unsigned wanted = grow_slot_function_bit(request->function_slot);
    for (int approved = 1; approved >= 0; approved--) {
        for (size_t i = 0; i < pool_count; i++) {
            const GrowPoolForm *form = &pool[i];
            if ((form->approved ? 1 : 0) != approved || !form->motive || form->slot_motive_of != request->origin ||
                (form->functions & wanted) == 0 || proposal->out->count >= FORM_MAX ||
                !form_fits(request, form->motive) || !cadence_tone_holds(request, form->motive)) {
                continue;
            }
            bool held = false;
            for (size_t k = 0; k < proposal->out->count; k++) {
                held = held || motive_equal(&((const Payload *)proposal->out->items[k].payload)->result, form->motive);
            }
            if (held) {
                continue;
            }
            Payload *payload = calloc(1, sizeof(*payload));
            if (!payload) {
                return false;
            }
            payload->magic = FORM_MAGIC;
            payload->result = *form->motive;
            payload->pool_bonus = approved ? 2 : 1;
            snprintf(payload->item, sizeof(payload->item), "%s", form->item);
            snprintf(payload->pool_chain, sizeof(payload->pool_chain), "%s", form->chain);
            if (!motive_connection_limited(&request->basic, &request->previous, form->motive, NULL, &request->limit,
                                           &payload->connection)) {
                free(payload);
                continue;
            }
            char label[FILL_LABEL_MAX];
            label_of(payload, label, sizeof(label));
            if (!fill_candidates_add(proposal->out, label, payload, motive_payload_destroy)) {
                return false;
            }
        }
    }
    return true;
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
                if (!offer(proposal, &chain, seed * FORM_SEED_STEP + (unsigned long)o, function, contrast)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/* The motive's first chord (its own harmony, else its best implied chord) is the unaltered tonic. */
static bool opens_on_tonic(const Motive *motive) {
    if (motive->chord_count > 0) {
        return motive->chords[0].degree == 1 && motive->chords[0].alteration == 0;
    }
    MotiveSketchChord implied;
    return motive_implied_chord(motive, &implied) && implied.degree == 1 && implied.alteration == 0;
}

/* The basic idea in its dominant form. */
static bool offer_dominant_form(Proposal *proposal) {
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.step_count = 1;
    return variant_step_init(&chain.steps[0], "dominant-form") ? offer(proposal, &chain, 1, NULL, false) : true;
}

/*
 * The liquidation of a continuation in units that halve (ch. VIII): the first fragment is the basic idea's characteristic
 * features liquidated and shortened by half, the second is the first liquidated and shortened again. Both are chains
 * from the basic idea, so a fragment does not wait for the one before it.
 */
static bool offer_liquidation(Proposal *proposal, size_t times) {
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    for (size_t t = 0; t < times; t++) {
        VariantStep *step = &chain.steps[t];
        if (!variant_step_init(step, "liquidate-features") || !variant_step_set(step, "steps", 1, 0) ||
            !variant_step_set(step, "shorten", 1, 0)) {
            return true;
        }
    }
    chain.step_count = times;
    return offer(proposal, &chain, 1, NULL, false);
}

static bool offer_exact(Proposal *proposal) {
    VariantChain chain;
    memset(&chain, 0, sizeof(chain));
    chain.step_count = 1;
    return variant_step_init(&chain.steps[0], "exact") ? offer(proposal, &chain, 1, NULL, false) : true;
}

/* Transpositions of the basic idea by steps of its key's scale; the one step the ref asks for, when it asks for one. */
static bool offer_transpositions(Proposal *proposal) {
    static const long STEPS[] = {1, 2, 3, -1, -2, -3};
    const MotiveRelationStep *asked = &proposal->request->ref.step;
    bool sequence = proposal->request->ref.has_step && asked->kind == MOTIVE_RELATION_TRANSPOSE && asked->value[0] != 0 &&
                    asked->value[1] != 0;
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
        if (sequence && STEPS[i] != asked->value[1]) {
            continue;
        }
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
    MotiveDevelopContext shared = context_or_default(context);
    return read_request(plan, target, &shared, &request);
}

static bool form_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    (void)rng;
    Request request;
    MotiveDevelopContext shared_value = context_or_default(context);
    const MotiveDevelopContext *shared = &shared_value;
    if (!read_request(plan, target, shared, &request)) {
        return true;
    }
    Proposal proposal = {&request, out};
    if (shared->pool_count > 0 && !offer_pool(&proposal, shared->pool, shared->pool_count)) {
        return false;
    }
    switch (request.slot) {
    case SLOT_RESTATED:
        return offer_exact(&proposal);
    case SLOT_REPETITION:
        /* A restated repetition (the melody restates the basic idea) stays exact. A response is the dominant form when
           the basic idea opens on the tonic and the chords answer on the dominant; else a transposition. */
        if (plan_motive_ref_restates(&request.ref)) {
            return offer_exact(&proposal);
        }
        if (request.answers_in_dominant && opens_on_tonic(&request.basic) && !offer_dominant_form(&proposal)) {
            return false;
        }
        return out->count > 0 ? true : offer_transpositions(&proposal);
    case SLOT_CONTRAST:
        /* The first idea of a slot with its own motive restates it (grow_idea_dev). */
        return plan_motive_ref_restates(&request.ref) ? offer_exact(&proposal)
                                                                : offer_operations(&proposal, "contrast", true);
    case SLOT_CADENTIAL:
        return plan_motive_ref_restates(&request.ref) ? offer_exact(&proposal)
                                                                : offer_operations(&proposal, "cadential", false);
    case SLOT_FRAGMENT_1:
        return plan_motive_ref_restates(&request.ref) ? offer_exact(&proposal) : offer_liquidation(&proposal, 1);
    case SLOT_FRAGMENT_2:
        return offer_liquidation(&proposal, 2);
    case SLOT_CADENTIAL_ENDING:
        return plan_motive_ref_restates(&request.ref) ? offer_exact(&proposal) : true;
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
    PlanNote lane_notes[MOTIVE_MAX_NOTES];
    motive_lane_notes(&payload->result, lane_notes);
    notes.notes.notes = lane_notes;
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
    unsigned long total = payload->connection.total.denominator;
    out->score_numerator = payload->connection.total.numerator + total;
    out->score_denominator = total;
    if (payload->pool_bonus > 0) {
        /* A pool form: plus 1/2 when approved, 1/4 when unflagged. */
        out->score_numerator = 4 * out->score_numerator + payload->pool_bonus * total;
        out->score_denominator = 4 * total;
    }
    return true;
}

const Motive *grow_form_candidate_motive(const FillCandidate *candidate) {
    const Payload *payload = candidate ? candidate->payload : NULL;
    return payload && payload->magic == FORM_MAGIC ? &payload->result : NULL;
}
