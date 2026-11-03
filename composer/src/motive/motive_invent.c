#include <ursatz_composer/motive_develop.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-counterpoint/melodic_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"

/*
 * Motive invention: a Fill operator for the Constrained one-bar melody span of a motive node that carries an
 * invent_motive constraint. It proposes short motives, each filling the bar, and writes the chosen one as the span's
 * notes. The checker judges a candidate with standard-counterpoint's melodic rules and scores it by how well the
 * developments the form templates make of it can still be recognized as it.
 */

#define INVENT_PAYLOAD_MAGIC 0x4D494E56UL
#define DEFAULT_CANDIDATES 6
#define ATTEMPTS_PER_CANDIDATE 64
#define MIN_NOTES 3
#define MAX_NOTES 6
#define RANGE_LOW 60  /* C4 */
#define RANGE_HIGH 84 /* C6 */
#define MAX_UNITS 4

typedef struct {
    unsigned long magic;
    Motive motive;
} InventPayload;

static MotiveDevelopContext invent_context(const void *context) {
    if (context) {
        return *(const MotiveDevelopContext *)context;
    }
    MotiveDevelopContext defaults;
    motive_develop_context_default(&defaults);
    return defaults;
}

/* --------------------------------------------------------------- request */

typedef struct {
    PlanMeter meter;
    AnalysisKey key;
    Rational bar;
} InventRequest;

static bool has_invent_constraint(const PiecePlan *plan, LaneSpanId id) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        if (plan_lane_span_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_INVENT_MOTIVE) {
            return true;
        }
    }
    return false;
}

/*
 * The target must be an open melody span (voice 0) of a motive node, with an invent_motive constraint, one bar of
 * its meter long and starting on a barline of it. The meter and key are the plan's lanes over it, else the context's.
 */
static bool read_invent_request(const PiecePlan *plan, FillTarget target, const MotiveDevelopContext *context,
                                InventRequest *out) {
    LaneSpanId id = (LaneSpanId)target.id;
    if (target.kind != FILL_TARGET_LANE_SPAN || !plan_has_lane_span(plan, id) ||
        plan_lane_span_lane(plan, id) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, id) != 0 ||
        plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED ||
        plan_node_kind(plan, plan_lane_span_owner(plan, id)) != PLAN_NODE_MOTIVE || !has_invent_constraint(plan, id)) {
        return false;
    }
    PlanSpan span = plan_lane_span_span(plan, id);
    LaneSpanId lane;
    out->meter = context->meter;
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, span, &lane, 1) > 0) {
        out->meter = plan_lane_span_payload(plan, lane)->meter;
    }
    out->key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, &lane, 1) > 0) {
        out->key = plan_lane_span_payload(plan, lane)->key;
    } else if (context->has_key) {
        out->key = context->key;
    }
    if (out->meter.beats_per_bar <= 0 || out->meter.beat_unit <= 0 ||
        rational_make(out->meter.beats_per_bar, (long long)out->meter.beat_unit, &out->bar) != RATIONAL_OK ||
        rational_cmp(span.length, out->bar) != 0) {
        return false;
    }
    /* The span starts on a barline: its start is a whole number of bars. */
    unsigned long long scaled_num = (unsigned long long)span.start.num * (unsigned long long)out->bar.den;
    unsigned long long scaled_den = (unsigned long long)span.start.den * (unsigned long long)out->bar.num;
    return span.start.num >= 0 && scaled_den != 0 && scaled_num % scaled_den == 0;
}

/* ---------------------------------------------------------------- rhythm */

/*
 * The cells of the bar: a beat in a simple meter, a dotted quarter in 6/8. A cell holds one note, or two (two halves
 * of a simple beat; a quarter and an eighth of a compound one), and the last note may hold the last cells together.
 * Every duration is a plain or dotted value. Durations are in whole notes as num/den.
 */
typedef struct {
    long num[MAX_NOTES];
    unsigned long den[MAX_NOTES];
    size_t count;
} Rhythm;

static bool add_duration(Rhythm *rhythm, long num, unsigned long den) {
    if (rhythm->count == MAX_NOTES) {
        return false;
    }
    Rational time;
    if (rational_make(num, den, &time) != RATIONAL_OK) {
        return false;
    }
    rhythm->num[rhythm->count] = time.num;
    rhythm->den[rhythm->count] = time.den;
    rhythm->count++;
    return true;
}

static bool draw_rhythm(const PlanMeter *meter, FillRng *rng, Rhythm *out) {
    bool compound = meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0 && meter->beats_per_bar > 3;
    size_t units = compound ? (size_t)meter->beats_per_bar / 3 : (size_t)meter->beats_per_bar;
    long unit_num = compound ? 3 : 1;
    unsigned long unit_den = (unsigned long)meter->beat_unit;
    if (units == 0 || units > MAX_UNITS) {
        return false;
    }
    /* How many cells the last note holds: 1, or (sometimes) 2 or more. */
    size_t held = 1;
    if (units > 1 && fill_rng_below(rng, 3) == 0) {
        held = 2 + (size_t)fill_rng_below(rng, units - 1);
        if (held > units) {
            held = units;
        }
    }
    memset(out, 0, sizeof(*out));
    for (size_t unit = 0; unit + held < units; unit++) {
        bool split = fill_rng_below(rng, 2) == 0;
        bool ok;
        if (!split) {
            ok = add_duration(out, unit_num, unit_den);
        } else if (compound) {
            ok = add_duration(out, 2, unit_den) && add_duration(out, 1, unit_den);
        } else {
            ok = add_duration(out, unit_num, unit_den * 2) && add_duration(out, unit_num, unit_den * 2);
        }
        if (!ok) {
            return false;
        }
    }
    if (held == 1 && fill_rng_below(rng, 2) == 0) {
        /* The last cell split like the others. */
        if (compound) {
            return add_duration(out, 2, unit_den) && add_duration(out, 1, unit_den);
        }
        return add_duration(out, unit_num, unit_den * 2) && add_duration(out, unit_num, unit_den * 2);
    }
    return add_duration(out, unit_num * (long)held, unit_den);
}

/* ----------------------------------------------------------------- pitch */

/* Steps of the scale a note may move: steps most often, leaps up to a sixth. */
static int draw_move(FillRng *rng) {
    static const int MOVES[] = {1, 1, 1, 1, 2, 2, 3, 4, 5};
    int size = MOVES[fill_rng_below(rng, sizeof(MOVES) / sizeof(MOVES[0]))];
    if (fill_rng_below(rng, 5) == 0) {
        return 0;
    }
    return fill_rng_below(rng, 2) == 0 ? size : -size;
}

static bool draw_motive(const InventRequest *request, FillRng *rng, Motive *out) {
    Rhythm rhythm;
    if (!draw_rhythm(&request->meter, rng, &rhythm) || rhythm.count < MIN_NOTES || rhythm.count > MAX_NOTES) {
        return false;
    }
    static const long OPENINGS[] = {0, 2, 4}; /* scale degrees 1, 3 and 5 */
    /* The opening in the octave of the tonic above C4 or the one above it (position 0 is the tonic of MIDI octave 0,
       so MIDI octave 5 holds C4). A pitch outside C4 to C6 rejects the draw. */
    long octave = 5 + (long)fill_rng_below(rng, 2);
    long position = octave * 7 + OPENINGS[fill_rng_below(rng, 3)];
    memset(out, 0, sizeof(*out));
    out->meter = request->meter;
    out->key = request->key;
    out->key_source = MOTIVE_KEY_GIVEN;
    Rational onset = {0, 1};
    for (size_t i = 0; i < rhythm.count; i++) {
        if (i > 0) {
            position += draw_move(rng);
        }
        int pitch = motive_position_pitch(&request->key, position, 0);
        if (pitch < RANGE_LOW || pitch > RANGE_HIGH) {
            return false;
        }
        Rational duration;
        if (rational_make(rhythm.num[i], rhythm.den[i], &duration) != RATIONAL_OK) {
            return false;
        }
        out->notes[i].midi_pitch = pitch;
        out->notes[i].offset = onset;
        out->notes[i].duration = duration;
        if (rational_add(onset, duration, &onset) != RATIONAL_OK) {
            return false;
        }
    }
    out->note_count = rhythm.count;
    out->length = onset;
    return true;
}

/* -------------------------------------------------------------- checking */

/* standard-counterpoint's hard melodic rules on the motive, as one line; the clausula vera is for strict species. */
bool motive_melodic_fault(const Motive *motive, const char **fault) {
    PitchSpelling *spellings[MOTIVE_MAX_NOTES];
    size_t made = 0;
    bool ok = true;
    *fault = NULL;
    for (size_t i = 0; i < motive->note_count; i++) {
        long position;
        int alteration;
        motive_scale_position(&motive->key, motive->notes[i].midi_pitch, &position, &alteration);
        long degree = ((position % 7) + 7) % 7 + 1;
        spellings[made] = harmony_spell_pitch(&motive->key, (int)degree, motive->notes[i].midi_pitch);
        if (!spellings[made]) {
            *fault = "a pitch cannot be spelled";
            break;
        }
        made++;
    }
    if (!*fault) {
        StandardCounterpointLine line = {(const PitchSpelling *const *)spellings, made};
        StandardCounterpointLimits limits = standard_counterpoint_default_limits();
        unsigned hard = 0;
        unsigned soft = 0;
        if (!standard_counterpoint_melodic_check(&line, &limits, &hard, &soft)) {
            ok = false;
        }
        hard &= ~(1u << STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA);
        for (int rule = 0; ok && rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
            if (hard & (1u << rule)) {
                *fault = standard_counterpoint_melodic_rule_name((StandardCounterpointMelodicRule)rule);
                break;
            }
        }
    }
    for (size_t i = 0; i < made; i++) {
        pitch_spelling_destroy(spellings[i]);
    }
    return ok;
}

/*
 * The soft score: the mean recognizability of the developments the form templates make of the motive (a tonal
 * inversion, a diminution, an interval change). A development that cannot be made scores 0.
 */
static bool invent_score(const Motive *motive, const MotiveScoreWeights *weights, MotiveScore *out) {
    MotiveDevSpec specs[3];
    memset(specs, 0, sizeof(specs));
    specs[0].kind = MOTIVE_DEV_INVERT;
    specs[0].tonal = true;
    specs[1].kind = MOTIVE_DEV_DIMINISH;
    specs[2].kind = MOTIVE_DEV_INTERVAL_CHANGE;
    specs[2].amount = 1;
    specs[2].interval = 1;
    Rational sum = rational_ratio(0, 1);
    for (size_t i = 0; i < 3; i++) {
        Motive development;
        MotiveRecognizability score;
        if (motive_develop(motive, &specs[i], &development) != MOTIVE_DEV_OK ||
            !motive_recognizability(motive, &development, weights, &score)) {
            continue;
        }
        if (rational_add(sum, motive_score_rational(score.total), &sum) != RATIONAL_OK) {
            return false;
        }
    }
    Rational mean;
    return rational_div_int(sum, 3, &mean) == RATIONAL_OK && motive_score_from_rational(mean, out);
}

/* -------------------------------------------------------------- operator */

static bool invent_applies(const PiecePlan *plan, FillTarget target, void *context) {
    InventRequest request;
    MotiveDevelopContext shared = invent_context(context);
    return read_invent_request(plan, target, &shared, &request);
}

static bool invent_already(const FillCandidateList *list, const Motive *motive) {
    for (size_t i = 0; i < list->count; i++) {
        const InventPayload *held = list->items[i].payload;
        if (held && held->magic == INVENT_PAYLOAD_MAGIC && motive_equal(&held->motive, motive)) {
            return true;
        }
    }
    return false;
}

static void invent_label(const Motive *motive, char *label, size_t size) {
    int used = snprintf(label, size, "invented");
    for (size_t i = 0; used > 0 && (size_t)used + 1 < size && i < motive->note_count; i++) {
        int written = snprintf(label + used, size - (size_t)used, " %d", motive->notes[i].midi_pitch);
        if (written < 0) {
            break;
        }
        used += written;
    }
}

static bool invent_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    MotiveDevelopContext shared_value = invent_context(context);
    const MotiveDevelopContext *shared = &shared_value;
    InventRequest request;
    if (!read_invent_request(plan, target, shared, &request)) {
        return true;
    }
    size_t goal = shared->candidates > 0 ? shared->candidates : DEFAULT_CANDIDATES;
    size_t added = 0;
    for (size_t attempt = 0; added < goal && attempt < goal * ATTEMPTS_PER_CANDIDATE; attempt++) {
        Motive motive;
        const char *fault;
        if (!draw_motive(&request, rng, &motive) || invent_already(out, &motive)) {
            continue;
        }
        /* Only motives that keep the hard melodic rules are proposed, as melody-fill proposes its melodies. */
        if (!motive_melodic_fault(&motive, &fault)) {
            return false;
        }
        if (fault) {
            continue;
        }
        InventPayload *payload = calloc(1, sizeof(*payload));
        if (!payload) {
            return false;
        }
        payload->magic = INVENT_PAYLOAD_MAGIC;
        payload->motive = motive;
        char label[FILL_LABEL_MAX];
        invent_label(&motive, label, sizeof(label));
        if (!fill_candidates_add(out, label, payload, free)) {
            return false;
        }
        added++;
    }
    return true;
}

/* The span takes the notes and is Realized; the motive node it belongs to is complete with it, so it is too. */
static PlanResult invent_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const InventPayload *payload = candidate ? candidate->payload : NULL;
    InventRequest request;
    if (!payload || payload->magic != INVENT_PAYLOAD_MAGIC) {
        return PLAN_ERR_NULL;
    }
    MotiveDevelopContext request_context = invent_context(context);
    if (!read_invent_request(plan, target, &request_context, &request)) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanLanePayload notes;
    memset(&notes, 0, sizeof(notes));
    notes.notes.notes = payload->motive.notes;
    notes.notes.note_count = payload->motive.note_count;
    PlanResult result = plan_lane_span_write(plan, (LaneSpanId)target.id, &notes);
    if (result == PLAN_OK) {
        NodeId owner = plan_lane_span_owner(plan, (LaneSpanId)target.id);
        if (plan_node_state(plan, owner) == PLAN_STATE_CONSTRAINED) {
            result = plan_node_set_state(plan, owner, PLAN_STATE_REALIZED);
        }
    }
    return result;
}

FillResult motive_invent_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = MOTIVE_INVENT_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan one-bar melody lane span of a motive node with invent_motive";
    operator_.generator.supported_outputs = "PiecePlan melody lane span";
    operator_.generator.configuration_description = "candidates per call, default meter and key";
    operator_.generator.constraints_description = "3 to 6 notes filling the bar, opening on degree 1, 3 or 5, C4 to C6, standard-counterpoint melodic rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = invent_applies;
    operator_.generate = invent_generate;
    operator_.apply = invent_apply;
    operator_.context = (void *)context;
    return fill_registry_register(registry, &operator_);
}

bool motive_invent_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out) {
    (void)plan;
    (void)target;
    (void)operator_;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    const InventPayload *payload = candidate ? candidate->payload : NULL;
    if (!payload || payload->magic != INVENT_PAYLOAD_MAGIC) {
        return true;
    }
    const char *fault;
    if (!motive_melodic_fault(&payload->motive, &fault)) {
        return false;
    }
    if (fault) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", fault);
        return true;
    }
    MotiveDevelopContext shared_value = invent_context(context);
    const MotiveDevelopContext *shared = &shared_value;
    MotiveScoreWeights weights = shared->weights;
    if (weights.contour + weights.intervals + weights.rhythm + weights.length == 0) {
        weights = motive_score_default_weights();
    }
    MotiveScore score;
    if (!invent_score(&payload->motive, &weights, &score)) {
        return false;
    }
    out->score_numerator = score.numerator;
    out->score_denominator = score.denominator;
    return true;
}

const Motive *motive_invent_candidate_motive(const FillCandidate *candidate) {
    const InventPayload *payload = candidate ? candidate->payload : NULL;
    return payload && payload->magic == INVENT_PAYLOAD_MAGIC ? &payload->motive : NULL;
}
