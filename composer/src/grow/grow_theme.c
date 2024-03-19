#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/* How many candidates are drawn for each one asked for before the search is given up. */
#define DRAWS_PER_CANDIDATE 6
/* The pitch of the placeholder notes a bass and a melody span hold until the operators write them. */
#define BASS_PLACEHOLDER 40
#define MELODY_PLACEHOLDER 60
/* Chords a phrase-model progression has before its last: a tonic, up to two predominants, a dominant. */
#define PHRASE_MODEL_SLOTS 4

/* ------------------------------------------------------------- the plan */

/* The theme section, the lanes under it, and the motive m0. False when the target is not one grow-theme grows. */
typedef struct {
    NodeId section;
    PlanSpan span;
    PlanMeter meter;
    PlanKey key;
    PlanCadenceType cadence;
    PlanThemePairing pairing;
    NodeId source;
    size_t bars;
    size_t cells_per_bar;
    PlanTime bar;
} ThemeRequest;

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
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_ARCHETYPE) {
            archetype = constraint->params.archetype == PLAN_ARCHETYPE_SENTENCE;
        } else if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            pairing = true;
            request.pairing = constraint->params.theme_pairing;
        } else if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            cadence = true;
            request.cadence = constraint->params.cadence;
        }
    }
    LaneSpanId lane[2];
    if (!archetype || !pairing || !cadence ||
        request.pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION ||
        request.pairing.ending != PLAN_THEME_ENDING_CONTINUATION ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, request.span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), request.span)) {
        return false;
    }
    request.meter = plan_lane_span_payload(plan, lane[0])->meter;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, request.span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), request.span)) {
        return false;
    }
    request.key = plan_lane_span_payload(plan, lane[0])->key;
    request.bar = grow_bar_length(&request.meter);
    request.source = grow_find_source(plan);
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

static PlanTime scaled(PlanTime unit, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make(unit.num * (long)count, unit.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The length of an idea in units, and where it starts, for a sentence of eight units. */
static const size_t IDEA_START[GROW_IDEA_COUNT] = {0, 2, 4, 5, 6};
static const size_t IDEA_UNITS[GROW_IDEA_COUNT] = {2, 2, 1, 1, 2};

static PlanTime unit_of(const ThemeRequest *request) {
    PlanTime unit = {0, 1};
    if (plan_time_make(request->bar.num * (long)request->bars, request->bar.den * 8, &unit) != PLAN_OK) {
        unit.num = 0;
        unit.den = 1;
    }
    return unit;
}

void grow_idea_ref(const GrowThemeSpec *spec, GrowIdea idea, NodeId source, PlanMotiveRef *out) {
    memset(out, 0, sizeof(*out));
    out->motive = source;
    switch (idea) {
    case GROW_IDEA_REPETITION:
        if (spec->statement_response) {
            out->transform = PLAN_TRANSFORM_TRANSPOSED;
            out->tonal = true;
            out->steps = 1;
        } else {
            out->transform = PLAN_TRANSFORM_RESTATED;
        }
        break;
    case GROW_IDEA_FRAGMENT_1:
    case GROW_IDEA_FRAGMENT_2:
        out->transform = PLAN_TRANSFORM_FRAGMENTED;
        out->position = PLAN_FRAGMENT_LEADING;
        out->length = (int)spec->fragment_notes[idea == GROW_IDEA_FRAGMENT_1 ? 0 : 1];
        break;
    case GROW_IDEA_CADENTIAL:
        out->transform = PLAN_TRANSFORM_LIQUIDATED;
        out->drop = (int)spec->liquidate_drop;
        break;
    default:
        out->transform = PLAN_TRANSFORM_NONE;
        break;
    }
}

bool grow_idea_develop(const GrowThemeSpec *spec, GrowIdea idea, Motive *out) {
    MotiveDevSpec dev;
    memset(&dev, 0, sizeof(dev));
    switch (idea) {
    case GROW_IDEA_REPETITION:
        if (spec->statement_response) {
            dev.kind = MOTIVE_DEV_TRANSPOSE;
            dev.tonal = true;
            dev.amount = 1;
        } else {
            dev.kind = MOTIVE_DEV_EXACT;
        }
        break;
    case GROW_IDEA_FRAGMENT_1:
    case GROW_IDEA_FRAGMENT_2:
        dev.kind = MOTIVE_DEV_FRAGMENT;
        dev.position = PLAN_FRAGMENT_LEADING;
        dev.length = spec->fragment_notes[idea == GROW_IDEA_FRAGMENT_1 ? 0 : 1];
        dev.min_length = 1;
        break;
    case GROW_IDEA_CADENTIAL:
        dev.kind = MOTIVE_DEV_LIQUIDATE;
        dev.drop = spec->liquidate_drop;
        dev.min_length = 1;
        break;
    default:
        return false;
    }
    return motive_develop(&spec->source, &dev, out) == MOTIVE_DEV_OK;
}

/* The longest leading fragment of at most `wanted` notes that fits an idea of `room`; 0 when none does. */
static size_t fitting_fragment(GrowThemeSpec *spec, size_t slot, size_t wanted, PlanTime room) {
    GrowIdea idea = slot == 0 ? GROW_IDEA_FRAGMENT_1 : GROW_IDEA_FRAGMENT_2;
    size_t most = spec->source.note_count - 1;
    for (size_t length = wanted < most ? wanted : most; length >= 1; length--) {
        Motive result;
        spec->fragment_notes[slot] = length;
        if (grow_idea_develop(spec, idea, &result) && plan_time_compare(result.length, room) <= 0) {
            return length;
        }
    }
    return 0;
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
    if (!grow_read_motive(plan, request->source, &request->meter, &request->key, &spec->source) ||
        spec->source.note_count < 2) {
        return false;
    }
    PlanTime unit = unit_of(request);

    /* Statement-response goes from tonic to dominant and back, which needs a cell for each. */
    size_t basic_cells = IDEA_UNITS[GROW_IDEA_BASIC] * request->bars / 8 * request->cells_per_bar;
    if (request->bars == 4) {
        basic_cells = request->cells_per_bar;
    }
    bool can_respond = basic_cells >= 2;
    uint64_t response = fill_rng_below(rng, 2);
    spec->statement_response = can_respond && response == 1;

    size_t first = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t second = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t count = spec->source.note_count;
    size_t drop_most = count / 2 > 1 ? count / 2 : 1;
    spec->liquidate_drop = 1 + (size_t)fill_rng_below(rng, drop_most);
    if (spec->liquidate_drop >= count) {
        spec->liquidate_drop = count - 1;
    }
    if (fitting_fragment(spec, 0, first, scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_1])) == 0 ||
        fitting_fragment(spec, 1, second, scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_2])) == 0) {
        return false;
    }
    for (GrowIdea idea = GROW_IDEA_REPETITION; idea < GROW_IDEA_COUNT; idea++) {
        if (!grow_idea_develop(spec, idea, &spec->developed[idea]) ||
            plan_time_compare(spec->developed[idea].length, scaled(unit, IDEA_UNITS[idea])) > 0) {
            return false;
        }
        spec->has_developed[idea] = true;
    }

    /* The continuation is a phrase model if it has room for one, else the cadential progression alone. */
    size_t cells = request->bars / 2 * request->cells_per_bar;
    spec->continuation_shape = cells >= harmony_phrase_min_cells(request->cadence, request->cells_per_bar)
                                   ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                                   : PLAN_HARMONY_SHAPE_CADENTIAL;
    /* Its chords change at least twice as fast as the presentation's longest could, and still deal the cells out. */
    size_t cap = basic_cells / 2 > 1 ? basic_cells / 2 : 1;
    size_t room = cells > request->cells_per_bar ? cells - request->cells_per_bar : 1;
    size_t least = (room + PHRASE_MODEL_SLOTS - 1) / PHRASE_MODEL_SLOTS;
    spec->continuation_cap = (int)(cap > least ? cap : least);
    return true;
}

static bool same_spec(const GrowThemeSpec *a, const GrowThemeSpec *b) {
    return a->statement_response == b->statement_response && a->fragment_notes[0] == b->fragment_notes[0] &&
           a->fragment_notes[1] == b->fragment_notes[1] && a->liquidate_drop == b->liquidate_drop;
}

static void spec_label(const GrowThemeSpec *spec, char *label, size_t size) {
    snprintf(label, size, "sentence %zu bars %s fragments %zu+%zu drop %zu", spec->bars,
             spec->statement_response ? "statement-response" : "exact", spec->fragment_notes[0],
             spec->fragment_notes[1], spec->liquidate_drop);
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

static PlanResult add_idea(PiecePlan *plan, NodeId parent, PlanTime section_start, PlanTime unit, GrowIdea idea,
                           const char *name, NodeId *out) {
    PlanSpan span;
    PlanResult result = plan_time_add(section_start, scaled(unit, IDEA_START[idea]), &span.start);
    span.length = scaled(unit, IDEA_UNITS[idea]);
    if (result == PLAN_OK) {
        result = plan_add_node(plan, parent, PLAN_NODE_IDEA, span, name, out);
    }
    return result == PLAN_OK ? plan_node_set_state(plan, *out, PLAN_STATE_REALIZED) : result;
}

static PlanResult add_node_in(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanTime start, PlanTime length,
                              const char *name, NodeId *out) {
    PlanSpan span = {start, length};
    PlanResult result = plan_add_node(plan, parent, kind, span, name, out);
    return result == PLAN_OK ? plan_node_set_state(plan, *out, PLAN_STATE_REALIZED) : result;
}

/* A harmony span and the bass span beside it, both open, over an idea or a subphrase. */
static PlanResult add_chord_targets(PiecePlan *plan, NodeId owner, const PlanHarmonyShape *shape,
                                    const PlanConstraint *cadence) {
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
    return result;
}

/* The open melody span of an idea, asking for the motive it is made from. */
static PlanResult add_melody_target(PiecePlan *plan, NodeId idea, const PlanMotiveRef *ref,
                                    const PlanConstraint *cadence) {
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
    PlanTime unit = unit_of(&request);
    PlanTime start = request.span.start;
    PlanConstraint cadence;
    memset(&cadence, 0, sizeof(cadence));
    cadence.kind = PLAN_CONSTRAINT_CADENCE;
    cadence.params.cadence = request.cadence;

    NodeId phrase;
    NodeId presentation;
    NodeId continuation;
    NodeId ideas[GROW_IDEA_COUNT];
    NodeId motives[GROW_IDEA_COUNT];
    PlanTime half;
    PlanResult result = plan_time_add(start, scaled(unit, 4), &half);

    /* The structure of the theme. */
    if (result == PLAN_OK) {
        result = add_node_in(plan, request.section, PLAN_NODE_PHRASE, start, request.span.length, "phrase", &phrase);
    }
    if (result == PLAN_OK) {
        result = plan_node_add_constraint(plan, phrase, &cadence);
    }
    if (result == PLAN_OK) {
        result = add_node_in(plan, phrase, PLAN_NODE_SUBPHRASE, start, scaled(unit, 4), "presentation", &presentation);
    }
    if (result == PLAN_OK) {
        result = add_idea(plan, presentation, start, unit, GROW_IDEA_BASIC, "basic idea", &ideas[GROW_IDEA_BASIC]);
    }
    if (result == PLAN_OK) {
        result = add_idea(plan, presentation, start, unit, GROW_IDEA_REPETITION, "repetition",
                          &ideas[GROW_IDEA_REPETITION]);
    }
    if (result == PLAN_OK) {
        result = add_node_in(plan, phrase, PLAN_NODE_SUBPHRASE, half, scaled(unit, 4), "continuation", &continuation);
    }
    if (result == PLAN_OK) {
        result = add_idea(plan, continuation, start, unit, GROW_IDEA_FRAGMENT_1, "fragment 1",
                          &ideas[GROW_IDEA_FRAGMENT_1]);
    }
    if (result == PLAN_OK) {
        result = add_idea(plan, continuation, start, unit, GROW_IDEA_FRAGMENT_2, "fragment 2",
                          &ideas[GROW_IDEA_FRAGMENT_2]);
    }
    if (result == PLAN_OK) {
        result = add_idea(plan, continuation, start, unit, GROW_IDEA_CADENTIAL, "cadential",
                          &ideas[GROW_IDEA_CADENTIAL]);
    }

    /* A motive in the material for each idea after the basic idea, each made from m0. */
    NodeId material_idea = plan_node_parent(plan, request.source);
    PlanSpan material = plan_node_span(plan, material_idea);
    static const char *const MOTIVE_NAMES[GROW_IDEA_COUNT] = {"m0", "m1", "m2", "m3", "m4"};
    PlanMotiveRef refs[GROW_IDEA_COUNT];
    for (GrowIdea idea = GROW_IDEA_BASIC; result == PLAN_OK && idea < GROW_IDEA_COUNT; idea++) {
        grow_idea_ref(spec, idea, request.source, &refs[idea]);
        if (idea == GROW_IDEA_BASIC) {
            motives[idea] = request.source;
            continue;
        }
        PlanSpan span = {{0, 1}, request.bar};
        result = plan_time_add(material.start, scaled(request.bar, (size_t)idea), &span.start);
        if (result == PLAN_OK) {
            result = plan_add_node(plan, material_idea, PLAN_NODE_MOTIVE, span, MOTIVE_NAMES[idea], &motives[idea]);
        }
        PlanConstraint constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
        constraint.params.motive = refs[idea];
        if (result == PLAN_OK) {
            result = plan_node_add_constraint(plan, motives[idea], &constraint);
        }
    }

    /* The harmony and bass to write, one pair for each of the presentation's ideas and one for the continuation. */
    PlanHarmonyShape basic_shape;
    PlanHarmonyShape repetition_shape;
    PlanHarmonyShape continuation_shape;
    memset(&basic_shape, 0, sizeof(basic_shape));
    memset(&repetition_shape, 0, sizeof(repetition_shape));
    memset(&continuation_shape, 0, sizeof(continuation_shape));
    basic_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    basic_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    basic_shape.ends_on = spec->statement_response ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    repetition_shape.starts_on = spec->statement_response ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
    continuation_shape.kind = spec->continuation_shape;
    continuation_shape.max_cells_per_chord = spec->continuation_cap;
    continuation_shape.split_at_bars = true;
    if (result == PLAN_OK) {
        result = add_chord_targets(plan, ideas[GROW_IDEA_BASIC], &basic_shape, NULL);
    }
    if (result == PLAN_OK) {
        result = add_chord_targets(plan, ideas[GROW_IDEA_REPETITION], &repetition_shape, NULL);
    }
    if (result == PLAN_OK) {
        result = add_chord_targets(plan, continuation, &continuation_shape, &cadence);
    }

    /* The melody to write, one span for each idea, the last asking for the cadence. */
    for (GrowIdea idea = GROW_IDEA_BASIC; result == PLAN_OK && idea < GROW_IDEA_COUNT; idea++) {
        PlanMotiveRef ref = refs[idea];
        ref.motive = motives[idea];
        result = add_melody_target(plan, ideas[idea], &ref, idea == GROW_IDEA_CADENTIAL ? &cadence : NULL);
    }
    return result;
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
