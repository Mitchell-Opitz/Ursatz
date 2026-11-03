#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <generation/candidate.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * Motivic development. Every transformation is run through the fill loop, across seeds and parameters, on source
 * motives with a rest, a dotted rhythm, a leading rest and a minor key. Each development is then judged a relative of
 * its source by a detector written here: Ursatz-Analyzer has no motive-relationship API, so the detector checks the
 * pitch and rhythm relation the transformation implies, from the notes alone, with its own scale tables, and never reads
 * what the generating code recorded about the choice it made.
 */

#define TARGET_START 4
#define TARGET_ROOM 8
#define SEEDS 6

/* ------------------------------------------------------------- time helpers */

static Rational tmake(long num, long long den) {
    Rational time;
    RationalResult result = rational_make(num, den, &time);
    assert(result == RATIONAL_OK);
    return time;
}

static Rational tmul(Rational a, Rational b) {
    return tmake(a.num * b.num, a.den * b.den);
}

static Rational tdiv(Rational a, Rational b) {
    return tmake(a.num * (long)b.den, a.den * (unsigned long)b.num);
}

static Rational tadd(Rational a, Rational b) {
    return tmake(a.num * (long)b.den + b.num * (long)a.den, a.den * b.den);
}

static Rational tsub(Rational a, Rational b) {
    return tmake(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den);
}

static bool teq(Rational a, Rational b) {
    return rational_cmp(a, b) == 0;
}

static bool is_time(Rational time, long num, long long den) {
    return time.num == num && time.den == den;
}

/* ------------------------------------------------------------------ sources */

typedef struct {
    const char *name;
    const char *notes;
    PlanMeter meter;
    AnalysisKey key;
} Source;

static const Source SOURCES[] = {
    {"minor", "D4:q F4:e A4:e D5:h", {4, 4, 0}, {NULL, PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR}},
    {"rest and dotted", "G4:q r:e A4:e C5:q. B4:e", {4, 4, 0}, {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR}},
    {"dotted", "G4:q. A4:e B4:q D5:q", {4, 4, 0}, {NULL, PITCH_LETTER_G, 0, KEY_STRUCTURE_MODE_MAJOR}},
    {"leading rest", "r:q E4:q G4:q C5:q", {4, 4, 0}, {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR}},
};
#define SOURCE_COUNT (sizeof(SOURCES) / sizeof(SOURCES[0]))

static Motive load(const char *notes, const PlanMeter *meter, const AnalysisKey *key) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    if (key) {
        options.has_key = true;
        options.key = *key;
    }
    Motive motive;
    MotiveError error;
    MotiveResult result = motive_from_notes_text(notes, &options, &motive, &error);
    assert(result == MOTIVE_OK);
    return motive;
}

static Motive load_source(const Source *source) {
    return load(source->notes, &source->meter, &source->key);
}

/* ------------------------------------------------------------------- scenes */

typedef struct {
    PiecePlan *plan;
    NodeId source;
    NodeId target;
} Scene;

static NodeId add_given(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span) {
    NodeId id = PLAN_NODE_NONE;
    PlanResult added = plan_add_node(plan, parent, kind, span, NULL, &id);
    assert(added == PLAN_OK);
    assert(plan_node_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    return id;
}

/*
 * A plan with the source motive Given at 0 and an open motive node after it, which carries a motive_ref constraint to
 * the source. Everything else is Given, so the fill loop has the one target.
 */
static Scene make_scene(const Motive *source, PlanMotiveRef ref) {
    Scene scene;
    scene.plan = plan_create();
    PlanSpan all = {tmake(0, 1), tmake(16, 1)};
    NodeId form = add_given(scene.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all);
    NodeId section = add_given(scene.plan, form, PLAN_NODE_SECTION, all);
    NodeId phrase = add_given(scene.plan, section, PLAN_NODE_PHRASE, all);
    NodeId idea = add_given(scene.plan, phrase, PLAN_NODE_IDEA, all);
    assert(motive_attach(scene.plan, idea, tmake(0, 1), source, "source", &scene.source, NULL) == PLAN_OK);
    PlanSpan room = {tmake(TARGET_START, 1), tmake(TARGET_ROOM, 1)};
    assert(plan_add_node(scene.plan, idea, PLAN_NODE_MOTIVE, room, "development", &scene.target) == PLAN_OK);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = ref;
    constraint.params.motive.motive = scene.source;
    assert(plan_node_add_constraint(scene.plan, scene.target, &constraint) == PLAN_OK);
    return scene;
}

static PlanMotiveRef ref_of(PlanMotiveTransform transform) {
    PlanMotiveRef ref;
    memset(&ref, 0, sizeof(ref));
    ref.transform = transform;
    return ref;
}

static MotiveDevelopContext context_for(const Source *source) {
    MotiveDevelopContext context;
    motive_develop_context_default(&context);
    context.meter = source->meter;
    context.has_key = true;
    context.key = source->key;
    return context;
}

/* The motive written on the target, if the run wrote one. */
static bool read_target(const Scene *scene, const Source *source, Motive *out) {
    LaneSpanId ids[4];
    if (plan_node_lane_spans(scene->plan, scene->target, ids, 4) != 1) {
        return false;
    }
    const PlanLanePayload *payload = plan_lane_span_payload(scene->plan, ids[0]);
    Motive motive;
    memset(&motive, 0, sizeof(motive));
    motive.meter = source->meter;
    motive.key = source->key;
    motive.key_source = MOTIVE_KEY_GIVEN;
    motive.note_count = payload->notes.note_count;
    memcpy(motive.notes, payload->notes.notes, motive.note_count * sizeof(PlanNote));
    PlanSpan span = plan_lane_span_span(scene->plan, ids[0]);
    motive.length = span.length;
    assert(plan_lane_span_state(scene->plan, ids[0]) == PLAN_STATE_REALIZED);
    assert(plan_lane_span_lane(scene->plan, ids[0]) == PLAN_LANE_MELODY);
    *out = motive;
    return true;
}

typedef struct {
    bool completed;
    FillStatus status;
    FillStuckReason stuck;
    char *record_json;
} Run;

/* Runs the loop on the scene with the operators registered and the recognizability checker. */
static Run run_loop(Scene *scene, MotiveDevelopContext *context, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    assert(registry);
    assert(motive_develop_register(registry, context) == FILL_OK);
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = motive_develop_checker;
    config.checker_context = context;
    FillOutcome outcome;
    FillRecord *record = NULL;
    assert(fill_run(scene->plan, registry, &config, &outcome, &record) == FILL_OK);

    Run run;
    memset(&run, 0, sizeof(run));
    run.status = outcome.status;
    run.completed = outcome.status == FILL_STATUS_COMPLETE;
    run.stuck = outcome.stuck_count > 0 ? outcome.stuck[0].reason : FILL_STUCK_NO_OPERATOR;
    run.record_json = fill_record_to_json(record);
    assert(run.record_json);

    /* The step that filled the target chose a candidate with the best score among those not rejected. */
    for (size_t i = 0; i < record->step_count; i++) {
        const FillStep *step = &record->steps[i];
        if (step->outcome != FILL_STEP_APPLIED) {
            continue;
        }
        const FillConsidered *chosen = &step->considered[step->chosen];
        assert(!chosen->rejected && chosen->score_denominator >= 1 && chosen->score_numerator <= chosen->score_denominator);
        for (size_t j = 0; j < step->considered_count; j++) {
            const FillConsidered *other = &step->considered[j];
            assert(other->score_denominator >= 1);
            if (!other->rejected) {
                assert(other->score_numerator * chosen->score_denominator <=
                       chosen->score_numerator * other->score_denominator);
            }
        }
    }
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    return run;
}

/* ---------------------------------------------------------------- detector */

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

/* The step of the key's scale a pitch is on, or LONG_MIN when it is not in the scale. */
static long step_of(AnalysisKey key, int pitch) {
    const int *scale = key.mode == KEY_STRUCTURE_MODE_MINOR ? MINOR_SCALE : MAJOR_SCALE;
    int relative = pitch - analysis_key_tonic_pitch_class(&key);
    long octave = relative >= 0 ? relative / 12 : -((-relative + 11) / 12);
    int rest = (int)(relative - octave * 12);
    for (int i = 0; i < 7; i++) {
        if (scale[i] == rest) {
            return octave * 7 + i;
        }
    }
    return LONG_MIN;
}

static bool same_rhythm(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (!teq(a->notes[i].offset, b->notes[i].offset) || !teq(a->notes[i].duration, b->notes[i].duration)) {
            return false;
        }
    }
    return true;
}

static bool same_notes(const Motive *a, const Motive *b) {
    if (!same_rhythm(a, b)) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi != b->notes[i].midi) {
            return false;
        }
    }
    return true;
}

/* The rhythm is the source's and every pitch moved by one amount: in semitones, or in steps of the scale. */
static bool is_transposition(const Motive *source, const Motive *result, AnalysisKey key) {
    if (!same_rhythm(source, result)) {
        return false;
    }
    bool real = true;
    bool tonal = true;
    int shift = result->notes[0].midi - source->notes[0].midi;
    long first = step_of(key, source->notes[0].midi);
    long first_result = step_of(key, result->notes[0].midi);
    for (size_t i = 0; i < source->note_count; i++) {
        real = real && result->notes[i].midi - source->notes[i].midi == shift;
        long from = step_of(key, source->notes[i].midi);
        long to = step_of(key, result->notes[i].midi);
        tonal = tonal && from != LONG_MIN && to != LONG_MIN && first != LONG_MIN && first_result != LONG_MIN &&
                to - from == first_result - first;
    }
    return real || tonal;
}

/* The rhythm is the source's and every pitch mirrored about one pitch: in semitones, or in steps of the scale. */
static bool is_inversion(const Motive *source, const Motive *result, AnalysisKey key) {
    if (!same_rhythm(source, result)) {
        return false;
    }
    bool real = true;
    bool tonal = true;
    int sum = source->notes[0].midi + result->notes[0].midi;
    long step_sum = LONG_MIN;
    for (size_t i = 0; i < source->note_count; i++) {
        real = real && source->notes[i].midi + result->notes[i].midi == sum;
        long from = step_of(key, source->notes[i].midi);
        long to = step_of(key, result->notes[i].midi);
        if (from == LONG_MIN || to == LONG_MIN) {
            tonal = false;
        } else if (step_sum == LONG_MIN) {
            step_sum = from + to;
        } else {
            tonal = tonal && from + to == step_sum;
        }
    }
    return real || tonal;
}

/* The pitches and the durations in reverse order, and the rests between the notes in reverse order too. */
static bool is_retrograde(const Motive *source, const Motive *result) {
    size_t n = source->note_count;
    if (result->note_count != n) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        const PlanNote *from = &source->notes[n - 1 - i];
        if (result->notes[i].midi != from->midi || !teq(result->notes[i].duration, from->duration)) {
            return false;
        }
    }
    /* The rest between two neighbors, from the end of one to the start of the next. */
    for (size_t i = 0; i + 1 < n; i++) {
        Rational gap_result = tsub(result->notes[i + 1].offset, tadd(result->notes[i].offset, result->notes[i].duration));
        const PlanNote *later = &source->notes[n - 1 - i];
        const PlanNote *earlier = &source->notes[n - 2 - i];
        Rational gap_source = tsub(later->offset, tadd(earlier->offset, earlier->duration));
        if (!teq(gap_result, gap_source)) {
            return false;
        }
    }
    return true;
}

static int sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

/* The rhythm and the first pitch are the source's; some interval differs; a direction changes only if allowed. */
static bool is_interval_change(const Motive *source, const Motive *result, bool keep_contour) {
    if (!same_rhythm(source, result) || result->notes[0].midi != source->notes[0].midi) {
        return false;
    }
    bool differs = false;
    for (size_t i = 0; i + 1 < source->note_count; i++) {
        int before = source->notes[i + 1].midi - source->notes[i].midi;
        int after = result->notes[i + 1].midi - result->notes[i].midi;
        differs = differs || before != after;
        if (keep_contour && sign(before) != sign(after)) {
            return false;
        }
        /* Widening and narrowing move an interval along its own direction, by at most the largest amount drawn. */
        if (after - before > 2 || before - after > 2) {
            return false;
        }
    }
    return differs;
}

static bool note_matches(const PlanNote *a, const PlanNote *b) {
    return a->midi == b->midi && teq(a->duration, b->duration);
}

/* A run of neighboring notes of the source, with its rests, starting the result at 0. */
static bool is_fragment(const Motive *source, const Motive *result) {
    size_t n = source->note_count;
    size_t m = result->note_count;
    if (m < 2 || m >= n || !is_time(result->notes[0].offset, 0, 1)) {
        return false;
    }
    for (size_t start = 0; start + m <= n; start++) {
        bool ok = true;
        for (size_t i = 0; ok && i < m; i++) {
            ok = note_matches(&source->notes[start + i], &result->notes[i]) &&
                 teq(result->notes[i].offset, tsub(source->notes[start + i].offset, source->notes[start].offset));
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

/* Kept rhythm: a kept note stays where it was and holds on through the time of the notes dropped after it. */
static bool kept_note_matches(const PlanNote *source, const PlanNote *kept) {
    return source->midi == kept->midi && tsub(kept->duration, source->duration).num >= 0;
}

/* Some of the source's notes, in order. Kept rhythm: where they were. Otherwise one after another from 0. */
static bool matches_from(const Motive *source, const Motive *result, bool keep_rhythm, size_t from_source,
                         size_t from_result, Rational at) {
    if (from_result == result->note_count) {
        return true;
    }
    for (size_t i = from_source; i < source->note_count; i++) {
        if (source->note_count - i < result->note_count - from_result) {
            return false;
        }
        const PlanNote *candidate = &source->notes[i];
        const PlanNote *note = &result->notes[from_result];
        Rational expected = keep_rhythm ? candidate->offset : at;
        if ((keep_rhythm ? kept_note_matches(candidate, note) : note_matches(candidate, note)) &&
            teq(note->offset, expected)) {
            Rational next = tmake(at.num * (long)candidate->duration.den + candidate->duration.num * (long)at.den,
                                  at.den * candidate->duration.den);
            if (matches_from(source, result, keep_rhythm, i + 1, from_result + 1, next)) {
                return true;
            }
        }
    }
    return false;
}

static bool is_liquidation(const Motive *source, const Motive *result, bool keep_rhythm) {
    return result->note_count >= 2 && result->note_count < source->note_count &&
           matches_from(source, result, keep_rhythm, 0, 0, tmake(0, 1));
}

/* The pitches are the source's, and the onsets and the durations are all multiplied by one ratio. */
static bool is_scaling(const Motive *source, const Motive *result, bool augment) {
    if (source->note_count != result->note_count) {
        return false;
    }
    Rational ratio = tdiv(result->notes[0].duration, source->notes[0].duration);
    if (augment ? ratio.num <= (long)ratio.den : ratio.num >= (long)ratio.den) {
        return false;
    }
    for (size_t i = 0; i < source->note_count; i++) {
        if (result->notes[i].midi != source->notes[i].midi ||
            !teq(result->notes[i].duration, tmul(source->notes[i].duration, ratio)) ||
            !teq(result->notes[i].offset, tmul(source->notes[i].offset, ratio))) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------- the main run */

typedef struct {
    const char *name;
    PlanMotiveRef ref;
    /* The relation the development must have to the source. */
    bool (*related)(const Motive *source, const Motive *result, const PlanMotiveRef *ref, AnalysisKey key);
} Family;

static bool related_restated(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    return same_notes(s, r) || is_transposition(s, r, key);
}

static bool related_transposed(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    return is_transposition(s, r, key) && !same_notes(s, r);
}

static bool related_inverted(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    return is_inversion(s, r, key);
}

static bool related_retrograde(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    (void)key;
    return is_retrograde(s, r);
}

static bool related_interval(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)key;
    return is_interval_change(s, r, ref->keep_contour);
}

static bool related_fragment(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    (void)key;
    return is_fragment(s, r);
}

static bool related_liquidation(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)key;
    return is_liquidation(s, r, ref->keep_rhythm);
}

static bool related_augmented(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    (void)key;
    return is_scaling(s, r, true);
}

static bool related_diminished(const Motive *s, const Motive *r, const PlanMotiveRef *ref, AnalysisKey key) {
    (void)ref;
    (void)key;
    return is_scaling(s, r, false);
}

static PlanMotiveRef with(PlanMotiveTransform transform, bool tonal, bool keep_contour, bool keep_rhythm) {
    PlanMotiveRef ref = ref_of(transform);
    ref.tonal = tonal;
    ref.keep_contour = keep_contour;
    ref.keep_rhythm = keep_rhythm;
    return ref;
}

static void test_every_development_is_a_relative(void) {
    const Family families[] = {
        {"restated", with(PLAN_TRANSFORM_RESTATED, false, false, false), related_restated},
        {"transposed", with(PLAN_TRANSFORM_TRANSPOSED, false, false, false), related_transposed},
        {"transposed (tonal)", with(PLAN_TRANSFORM_TRANSPOSED, true, false, false), related_transposed},
        {"inverted", with(PLAN_TRANSFORM_INVERTED, false, false, false), related_inverted},
        {"inverted (tonal)", with(PLAN_TRANSFORM_INVERTED, true, false, false), related_inverted},
        {"retrograde", with(PLAN_TRANSFORM_RETROGRADE, false, false, false), related_retrograde},
        {"interval changed", with(PLAN_TRANSFORM_INTERVAL_CHANGED, false, false, false), related_interval},
        {"interval changed (keep contour)", with(PLAN_TRANSFORM_INTERVAL_CHANGED, false, true, false),
         related_interval},
        {"fragmented", with(PLAN_TRANSFORM_FRAGMENTED, false, false, false), related_fragment},
        {"liquidated", with(PLAN_TRANSFORM_LIQUIDATED, false, false, false), related_liquidation},
        {"liquidated (keep rhythm)", with(PLAN_TRANSFORM_LIQUIDATED, false, false, true), related_liquidation},
        {"augmented", with(PLAN_TRANSFORM_AUGMENTED, false, false, false), related_augmented},
        {"diminished", with(PLAN_TRANSFORM_DIMINISHED, false, false, false), related_diminished},
    };
    size_t runs = 0;

    for (size_t s = 0; s < SOURCE_COUNT; s++) {
        Motive source = load_source(&SOURCES[s]);
        for (size_t f = 0; f < sizeof(families) / sizeof(families[0]); f++) {
            Motive first_seen;
            bool varied = false;
            memset(&first_seen, 0, sizeof(first_seen));
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                MotiveDevelopContext context = context_for(&SOURCES[s]);
                Scene scene = make_scene(&source, families[f].ref);
                Run run = run_loop(&scene, &context, seed);
                Motive result;
                bool written = read_target(&scene, &SOURCES[s], &result);
                if (!run.completed || !written) {
                    fprintf(stderr, "%s / %s / seed %lu: no development (status %d)\n", SOURCES[s].name,
                            families[f].name, seed, (int)run.status);
                }
                assert(run.completed && written);
                if (!families[f].related(&source, &result, &families[f].ref, SOURCES[s].key)) {
                    fprintf(stderr, "%s / %s / seed %lu: not a relative\n", SOURCES[s].name, families[f].name, seed);
                }
                assert(families[f].related(&source, &result, &families[f].ref, SOURCES[s].key));

                /* Given material is never modified: the source is as it was, still Given. */
                Motive after;
                LaneSpanId ids[2];
                assert(plan_node_state(scene.plan, scene.source) == PLAN_STATE_GIVEN);
                assert(plan_node_lane_spans(scene.plan, scene.source, ids, 2) == 1);
                assert(plan_lane_span_state(scene.plan, ids[0]) == PLAN_STATE_GIVEN);
                const PlanLanePayload *payload = plan_lane_span_payload(scene.plan, ids[0]);
                memset(&after, 0, sizeof(after));
                after.note_count = payload->notes.note_count;
                memcpy(after.notes, payload->notes.notes, after.note_count * sizeof(PlanNote));
                after.length = source.length;
                assert(same_notes(&source, &after));
                assert(plan_node_state(scene.plan, scene.target) == PLAN_STATE_REALIZED);

                /* The same seed gives the same plan and the same record. */
                Scene again = make_scene(&source, families[f].ref);
                Run repeat = run_loop(&again, &context, seed);
                assert(strcmp(run.record_json, repeat.record_json) == 0);
                char *one = plan_to_json(scene.plan, NULL);
                char *two = plan_to_json(again.plan, NULL);
                assert(one && two && strcmp(one, two) == 0);
                free(one);
                free(two);

                if (seed == 1) {
                    first_seen = result;
                } else if (!same_notes(&first_seen, &result)) {
                    varied = true;
                }
                free(run.record_json);
                free(repeat.record_json);
                plan_destroy(scene.plan);
                plan_destroy(again.plan);
                runs++;
            }
            /* Open parameters are drawn from the seed, so the seeds do not all give the same development. */
            if (f == 1) {
                assert(varied);
            }
        }
    }
    assert(runs == SOURCE_COUNT * 13 * SEEDS);
}

/* ------------------------------------------------------ explicit parameters */

static Motive develop_with_ref(const Source *source, PlanMotiveRef ref, MotiveDevelopContext *context, Run *run) {
    Motive motive = load_source(source);
    Scene scene = make_scene(&motive, ref);
    *run = run_loop(&scene, context, 3);
    Motive result;
    memset(&result, 0, sizeof(result));
    if (run->completed) {
        assert(read_target(&scene, source, &result));
    }
    plan_destroy(scene.plan);
    return result;
}

static bool pitches_are(const Motive *motive, size_t count, const int *pitches) {
    if (motive->note_count != count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (motive->notes[i].midi != pitches[i]) {
            return false;
        }
    }
    return true;
}

static void test_explicit_parameters(void) {
    const Source *minor = &SOURCES[0]; /* D4:q F4:e A4:e D5:h in D minor */
    MotiveDevelopContext context = context_for(minor);
    Run run;
    Motive result;
    PlanMotiveRef ref;

    ref = ref_of(PLAN_TRANSFORM_TRANSPOSED);
    ref.semitones = 3;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){65, 68, 72, 77}));
    free(run.record_json);

    /* Two steps up the scale of D minor: D F A D becomes F A C F. */
    ref = ref_of(PLAN_TRANSFORM_TRANSPOSED);
    ref.tonal = true;
    ref.steps = 2;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){65, 69, 72, 77}));
    free(run.record_json);

    ref = ref_of(PLAN_TRANSFORM_INVERTED);
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed);
    free(run.record_json);
    ref.has_axis = true;
    ref.axis = 62;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 59, 55, 50}));
    free(run.record_json);
    ref.tonal = true;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 58, 55, 50}));
    free(run.record_json);

    ref = ref_of(PLAN_TRANSFORM_RETROGRADE);
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){74, 69, 65, 62}));
    assert(is_time(result.notes[0].offset, 0, 1) && is_time(result.notes[0].duration, 1, 2));
    assert(is_time(result.notes[1].offset, 1, 2) && is_time(result.notes[2].offset, 5, 8));
    assert(is_time(result.notes[3].offset, 3, 4) && is_time(result.length, 1, 1));
    free(run.record_json);

    /* The second interval, F to A, widened by a semitone; the note after it moves with it. */
    ref = ref_of(PLAN_TRANSFORM_INTERVAL_CHANGED);
    ref.interval = 2;
    ref.amount = 1;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 65, 70, 75}));
    free(run.record_json);
    ref.interval = 0;
    ref.amount = -1;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 64, 67, 71}));
    free(run.record_json);
    /* Narrowing the first interval past zero turns it round: allowed, unless the contour is to be kept. */
    ref.interval = 1;
    ref.amount = -5;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 60, 64, 69}));
    free(run.record_json);
    ref.keep_contour = true;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(!run.completed && run.stuck == FILL_STUCK_ALL_REJECTED);
    free(run.record_json);

    /* Fragments. */
    ref = ref_of(PLAN_TRANSFORM_FRAGMENTED);
    ref.position = PLAN_FRAGMENT_LEADING;
    ref.length = 3;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 3, (int[]){62, 65, 69}));
    assert(is_time(result.notes[1].offset, 1, 4) && is_time(result.notes[2].offset, 3, 8));
    assert(is_time(result.length, 1, 2));
    free(run.record_json);
    ref.position = PLAN_FRAGMENT_TRAILING;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 3, (int[]){65, 69, 74}));
    assert(is_time(result.notes[0].offset, 0, 1) && is_time(result.notes[1].offset, 1, 8) &&
           is_time(result.notes[2].offset, 1, 4) && is_time(result.length, 3, 4));
    free(run.record_json);
    ref.position = PLAN_FRAGMENT_MIDDLE;
    ref.length = 2;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 2, (int[]){65, 69}));
    free(run.record_json);

    /* Liquidation drops the weakest note first: A4, off the beat; then the later of F4 and D5. */
    ref = ref_of(PLAN_TRANSFORM_LIQUIDATED);
    ref.drop = 1;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 3, (int[]){62, 65, 74}));
    assert(is_time(result.notes[1].offset, 1, 4) && is_time(result.notes[2].offset, 3, 8));
    free(run.record_json);
    ref.keep_rhythm = true;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 3, (int[]){62, 65, 74}));
    assert(is_time(result.notes[2].offset, 1, 2) && is_time(result.length, 1, 1));
    free(run.record_json);
    ref.drop = 2;
    ref.keep_rhythm = false;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 2, (int[]){62, 65}));
    free(run.record_json);

    /* Rhythm. */
    ref = ref_of(PLAN_TRANSFORM_AUGMENTED);
    ref.ratio_num = 2;
    ref.ratio_den = 1;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && pitches_are(&result, 4, (int[]){62, 65, 69, 74}));
    assert(is_time(result.notes[1].offset, 1, 2) && is_time(result.notes[1].duration, 1, 4) &&
           is_time(result.notes[3].offset, 1, 1) && is_time(result.notes[3].duration, 1, 1) &&
           is_time(result.length, 2, 1));
    free(run.record_json);
    ref.ratio_num = 3;
    ref.ratio_den = 2;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && is_time(result.notes[0].duration, 3, 8) && is_time(result.notes[3].offset, 3, 4) &&
           is_time(result.length, 3, 2));
    free(run.record_json);
    ref = ref_of(PLAN_TRANSFORM_DIMINISHED);
    ref.ratio_num = 1;
    ref.ratio_den = 2;
    result = develop_with_ref(minor, ref, &context, &run);
    assert(run.completed && is_time(result.notes[0].duration, 1, 8) && is_time(result.notes[1].offset, 1, 8) &&
           is_time(result.notes[2].offset, 3, 16) && is_time(result.notes[3].duration, 1, 4) &&
           is_time(result.length, 1, 2));
    free(run.record_json);
}

/* ----------------------------------------------------------- degenerate cases */

static void test_degenerate_cases(void) {
    PlanMeter meter = {4, 4, 0};
    AnalysisKey c_major = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
    Motive single = load("C4:q", &meter, &c_major);
    Motive four = load("D4:q F4:e A4:e D5:h", &meter, &c_major);
    Motive result;
    MotiveDevSpec spec;

    /* One note has no intervals to change, no run to fragment or notes to drop; retrograde and a mirror about
       itself give the note again. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_INTERVAL_CHANGE;
    spec.amount = 1;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_NOT_APPLICABLE);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_FRAGMENT;
    spec.position = PLAN_FRAGMENT_LEADING;
    spec.length = 1;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_TOO_SHORT);
    spec.length = 2;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_NOT_APPLICABLE);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_LIQUIDATE;
    spec.drop = 1;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_TOO_SHORT);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_RETROGRADE;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_UNCHANGED);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_INVERT;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_ERR_UNCHANGED);

    /* What one note can do: be repeated, moved, and made longer or shorter. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_EXACT;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_OK && motive_equal(&single, &result));
    spec.kind = MOTIVE_DEV_TRANSPOSE;
    spec.amount = 7;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_OK && result.notes[0].midi == 67);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_AUGMENT;
    assert(motive_develop(&single, &spec, &result) == MOTIVE_DEV_OK && is_time(result.notes[0].duration, 1, 2));

    /* Fragments and liquidations stay recognizable: the minimum length is 2 notes, or what the spec says. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_FRAGMENT;
    spec.position = PLAN_FRAGMENT_LEADING;
    spec.length = 1;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_TOO_SHORT);
    spec.length = 4;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_NOT_APPLICABLE);
    spec.length = 2;
    spec.min_length = 3;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_TOO_SHORT);
    spec.min_length = 1;
    spec.length = 1;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_OK && result.note_count == 1);
    spec.position = PLAN_FRAGMENT_ANY;
    spec.length = 2;
    spec.min_length = 0;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_INVALID);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_LIQUIDATE;
    spec.drop = 3;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_TOO_SHORT);
    spec.drop = 0;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_INVALID);

    /* A rhythm that cannot be written in the meter is refused rather than bent: sixths are not notatable. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_DIMINISH;
    spec.ratio_num = 2;
    spec.ratio_den = 3;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_NOT_EXPRESSIBLE);
    spec.kind = MOTIVE_DEV_AUGMENT;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_INVALID);
    spec.ratio_num = 1;
    spec.ratio_den = 1;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_INVALID);

    /* A tonal transformation needs a key; a pitch off the end of MIDI is refused. */
    Motive unkeyed = load("D4:q F4:e A4:e D5:h", &meter, NULL);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_TRANSPOSE;
    spec.tonal = true;
    spec.amount = 2;
    assert(motive_develop(&unkeyed, &spec, &result) == MOTIVE_DEV_ERR_NO_KEY);
    spec.tonal = false;
    spec.amount = 60;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_RANGE);
    spec.amount = 0;
    assert(motive_develop(&four, &spec, &result) == MOTIVE_DEV_ERR_UNCHANGED);

    /* Through the loop: with nothing a transformation can do, the target is left open and the run says why. */
    MotiveDevelopContext context;
    motive_develop_context_default(&context);
    context.has_key = true;
    context.key = c_major;
    const PlanMotiveTransform none_for_one_note[] = {PLAN_TRANSFORM_INTERVAL_CHANGED, PLAN_TRANSFORM_FRAGMENTED,
                                                     PLAN_TRANSFORM_LIQUIDATED, PLAN_TRANSFORM_RETROGRADE};
    for (size_t i = 0; i < 4; i++) {
        Scene scene = make_scene(&single, ref_of(none_for_one_note[i]));
        Run run = run_loop(&scene, &context, 1);
        assert(!run.completed && run.status == FILL_STATUS_STUCK && run.stuck == FILL_STUCK_ALL_REJECTED);
        assert(plan_node_state(scene.plan, scene.target) == PLAN_STATE_CONSTRAINED);
        assert(plan_node_lane_spans(scene.plan, scene.target, NULL, 0) == 0);
        free(run.record_json);
        plan_destroy(scene.plan);
    }

    /* The development must fit the room it is given: the target is too short for a doubled motive. */
    PiecePlan *plan = plan_create();
    PlanSpan all = {tmake(0, 1), tmake(16, 1)};
    NodeId form = add_given(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all);
    NodeId section = add_given(plan, form, PLAN_NODE_SECTION, all);
    NodeId phrase = add_given(plan, section, PLAN_NODE_PHRASE, all);
    NodeId idea = add_given(plan, phrase, PLAN_NODE_IDEA, all);
    NodeId source_node;
    NodeId target_node;
    assert(motive_attach(plan, idea, tmake(0, 1), &four, NULL, &source_node, NULL) == PLAN_OK);
    PlanSpan tight = {tmake(2, 1), tmake(3, 2)};
    assert(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, tight, NULL, &target_node) == PLAN_OK);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = ref_of(PLAN_TRANSFORM_AUGMENTED);
    constraint.params.motive.motive = source_node;
    constraint.params.motive.ratio_num = 4;
    constraint.params.motive.ratio_den = 1;
    assert(plan_node_add_constraint(plan, target_node, &constraint) == PLAN_OK);
    Scene tight_scene = {plan, source_node, target_node};
    context.meter = meter;
    Run run = run_loop(&tight_scene, &context, 1);
    assert(!run.completed && run.stuck == FILL_STUCK_ALL_REJECTED);
    free(run.record_json);
    plan_destroy(plan);
}

/* ----------------------------------------------------------- recognizability */

static bool score_is(MotiveScore score, unsigned long num, unsigned long den) {
    return score.numerator == num && score.denominator == den;
}

/* The pickup scales with augmentation and diminution, and a tonal transposition or inversion spells F and B as degrees. */
static void test_pickups_and_tonal_spelling(void) {
    const PlanMeter three_four = {3, 4, 0};
    const AnalysisKey c_major = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
    Motive source = load("C4:q E4:q G4:q E4:q", &three_four, &c_major);
    Motive result;
    MotiveDevSpec spec;

    source.pickup = tmake(1, 8);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_AUGMENT;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(is_time(result.pickup, 1, 4) && is_time(result.notes[1].offset, 1, 2));
    spec.kind = MOTIVE_DEV_DIMINISH;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(is_time(result.pickup, 1, 16) && is_time(result.notes[1].offset, 1, 8));

    /* A pickup that reaches the bar, or leaves the thirty-second grid, is refused. */
    source.pickup = tmake(3, 8);
    spec.kind = MOTIVE_DEV_AUGMENT;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_ERR_NOT_EXPRESSIBLE);
    source.pickup = tmake(1, 32);
    spec.kind = MOTIVE_DEV_DIMINISH;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_ERR_NOT_EXPRESSIBLE);

    /* F and B are natural degrees of C major: a step moves them to natural degrees, never to F sharp or C sharp. */
    Motive line = load("C4:q F4:q B4:q E4:q", &(PlanMeter){4, 4, 0}, &c_major);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_TRANSPOSE;
    spec.tonal = true;
    spec.amount = 1;
    assert(motive_develop(&line, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.notes[0].midi == 62 && result.notes[1].midi == 67 && result.notes[2].midi == 72 &&
           result.notes[3].midi == 65);
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_INVERT;
    spec.tonal = true;
    assert(motive_develop(&line, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.notes[0].midi == 60 && result.notes[1].midi == 55 && result.notes[2].midi == 50 &&
           result.notes[3].midi == 57);
}

static void test_recognizability(void) {
    PlanMeter meter = {4, 4, 0};
    AnalysisKey d_minor = {NULL, PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};
    Motive source = load("D4:q F4:e A4:e D5:h", &meter, &d_minor);
    MotiveRecognizability r;

    /* The same motive, and one moved: nothing is lost. */
    assert(motive_recognizability(&source, &source, NULL, &r) && score_is(r.total, 1, 1));
    Motive moved = load("F4:q A4:e C5:e F5:h", &meter, &d_minor);
    assert(motive_recognizability(&source, &moved, NULL, &r) && score_is(r.total, 1, 1));

    /* Mirrored: every direction flips, the interval classes, the rhythm and the length stay: (0 + 2 + 2 + 1) / 8. */
    Motive mirrored = load("D4:q B3:e G3:e D3:h", &meter, &d_minor);
    assert(motive_recognizability(&source, &mirrored, NULL, &r));
    assert(score_is(r.contour, 0, 1) && score_is(r.intervals, 1, 1) && score_is(r.rhythm, 1, 1) &&
           score_is(r.length, 1, 1) && score_is(r.total, 5, 8));

    /* Backwards: contour 0, the same classes, only the middle ratio shared (1/3), (0 + 2 + 2/3 + 1) / 8 = 11/24. */
    Motive backwards = load("D5:h A4:e F4:e D4:q", &meter, &d_minor);
    assert(motive_recognizability(&source, &backwards, NULL, &r));
    assert(score_is(r.contour, 0, 1) && score_is(r.intervals, 1, 1) && score_is(r.rhythm, 1, 3) &&
           score_is(r.length, 1, 1) && score_is(r.total, 11, 24));

    /* The first three notes: 2 of 3 intervals, 2 of 3 ratios, 3 notes of 4: (2 + 4/3 + 4/3 + 3/4) / 8 = 65/96. */
    Motive fragment = load("D4:q F4:e A4:e", &meter, &d_minor);
    assert(motive_recognizability(&source, &fragment, NULL, &r));
    assert(score_is(r.contour, 2, 3) && score_is(r.intervals, 2, 3) && score_is(r.rhythm, 2, 3) &&
           score_is(r.length, 3, 4) && score_is(r.total, 65, 96));

    /* Twice as slow keeps the rhythm: only the ratios between neighbors count. */
    Motive slow = load("D4:h F4:q A4:q D5:w", &meter, &d_minor);
    assert(motive_recognizability(&source, &slow, NULL, &r) && score_is(r.total, 1, 1));

    /* The weights choose what counts. */
    MotiveScoreWeights contour_only = {1, 0, 0, 0};
    assert(motive_recognizability(&source, &mirrored, &contour_only, &r) && score_is(r.total, 0, 1));
    MotiveScoreWeights length_only = {0, 0, 0, 1};
    assert(motive_recognizability(&source, &fragment, &length_only, &r) && score_is(r.total, 3, 4));
    MotiveScoreWeights equal = {1, 1, 1, 1};
    assert(motive_recognizability(&source, &mirrored, &equal, &r) && score_is(r.total, 3, 4));
    MotiveScoreWeights none = {0, 0, 0, 0};
    assert(!motive_recognizability(&source, &source, &none, &r));
    MotiveScoreWeights defaults = motive_score_default_weights();
    assert(defaults.contour == 3 && defaults.intervals == 2 && defaults.rhythm == 2 && defaults.length == 1);

    /* One note against one: nothing to compare is nothing lost; one note against several is nothing shared. */
    Motive single = load("C4:q", &meter, &d_minor);
    Motive other = load("E4:q", &meter, &d_minor);
    assert(motive_recognizability(&single, &other, NULL, &r) && score_is(r.total, 1, 1));
    assert(motive_recognizability(&single, &source, NULL, &r));
    assert(score_is(r.contour, 0, 1) && score_is(r.intervals, 0, 1) && score_is(r.rhythm, 0, 1) &&
           score_is(r.length, 1, 4) && score_is(r.total, 1, 32));
}

/* Scores steer the loop and the suggestions, and a floor rejects. */
static void test_scores_in_the_loop(void) {
    const Source *minor = &SOURCES[0];
    Motive source = load_source(minor);
    MotiveDevelopContext context = context_for(minor);

    /* Suggestions come best first, each with its score. */
    Scene scene = make_scene(&source, ref_of(PLAN_TRANSFORM_FRAGMENTED));
    FillOperatorRegistry *registry = fill_registry_create();
    assert(motive_develop_register(registry, &context) == FILL_OK);
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = 5;
    config.checker = motive_develop_checker;
    config.checker_context = &context;
    FillTarget target = {FILL_TARGET_NODE, scene.target};
    FillSuggestions *suggestions = NULL;
    assert(fill_suggest(scene.plan, registry, &config, target, 4, &suggestions) == FILL_OK);
    const CandidateSet *set = fill_suggestions_set(suggestions);
    size_t count = candidate_set_candidate_count(set);
    assert(count >= 2 && count <= 4);
    for (size_t i = 0; i < count; i++) {
        const Candidate *candidate = candidate_set_candidate_at(set, i);
        assert(candidate_score_denominator(candidate) >= 1);
        assert(candidate_score_numerator(candidate) <= candidate_score_denominator(candidate));
        assert(strcmp(fill_suggestion_operator(suggestions, i), MOTIVE_FRAGMENT_OPERATOR_NAME) == 0);
        assert(strncmp(fill_suggestion_label(suggestions, i), "fragment ", 9) == 0);
        if (i > 0) {
            const Candidate *before = candidate_set_candidate_at(set, i - 1);
            assert(candidate_score_numerator(before) * candidate_score_denominator(candidate) >=
                   candidate_score_numerator(candidate) * candidate_score_denominator(before));
        }
    }
    fill_suggestions_destroy(suggestions);
    fill_registry_destroy(registry);
    plan_destroy(scene.plan);

    /* A floor above what any fragment scores leaves the target open. */
    context.floor.numerator = 19;
    context.floor.denominator = 20;
    scene = make_scene(&source, ref_of(PLAN_TRANSFORM_FRAGMENTED));
    Run run = run_loop(&scene, &context, 1);
    assert(!run.completed && run.stuck == FILL_STUCK_ALL_REJECTED);
    free(run.record_json);
    plan_destroy(scene.plan);
}

/* ---------------------------------------------------------------- plan JSON */

/* Whether the JSON writes a "length" that is a number, as a fragment's does (a motive_tones chord's is a time in quotes). */
static bool has_numeric_length(const char *json) {
    for (const char *at = strstr(json, "\"length\": "); at; at = strstr(at + 1, "\"length\": ")) {
        if (at[10] != '"') {
            return true;
        }
    }
    return false;
}

static void test_constraint_json(void) {
    PlanMotiveRef plain = ref_of(PLAN_TRANSFORM_INVERTED);
    Motive source = load_source(&SOURCES[0]);
    Scene scene = make_scene(&source, plain);
    char *json = plan_to_json(scene.plan, NULL);
    assert(json && strstr(json, "\"transform\": \"inverted\"") != NULL);
    /* A plain constraint writes what it always did. */
    assert(strstr(json, "\"tonal\"") == NULL && strstr(json, "\"axis\"") == NULL && !has_numeric_length(json));
    free(json);
    plan_destroy(scene.plan);

    /* The parameters survive the plan's JSON. */
    PlanMotiveRef full = ref_of(PLAN_TRANSFORM_INTERVAL_CHANGED);
    full.tonal = true;
    full.steps = -2;
    full.has_axis = true;
    full.axis = 62;
    full.interval = 2;
    full.amount = -1;
    full.keep_contour = true;
    full.position = PLAN_FRAGMENT_MIDDLE;
    full.length = 3;
    full.drop = 2;
    full.keep_rhythm = true;
    full.ratio_num = 3;
    full.ratio_den = 2;
    scene = make_scene(&source, full);
    json = plan_to_json(scene.plan, NULL);
    assert(json);
    PiecePlan *copy = NULL;
    PlanJsonError error;
    assert(plan_from_json(json, &copy, &error) == PLAN_OK);
    const PlanConstraint *read = plan_node_constraint_at(copy, scene.target, 0);
    assert(read && read->kind == PLAN_CONSTRAINT_MOTIVE_REF);
    const PlanMotiveRef *ref = &read->params.motive;
    assert(ref->transform == PLAN_TRANSFORM_INTERVAL_CHANGED && ref->motive == scene.source && ref->tonal &&
           ref->steps == -2 && ref->has_axis && ref->axis == 62 && ref->interval == 2 && ref->amount == -1 &&
           ref->keep_contour && ref->position == PLAN_FRAGMENT_MIDDLE && ref->length == 3 && ref->drop == 2 &&
           ref->keep_rhythm && ref->ratio_num == 3 && ref->ratio_den == 2);
    char *again = plan_to_json(copy, NULL);
    assert(again && strcmp(json, again) == 0);
    free(json);
    free(again);
    plan_destroy(copy);
    plan_destroy(scene.plan);

    /* The new transforms are named in the plan format. */
    scene = make_scene(&source, ref_of(PLAN_TRANSFORM_LIQUIDATED));
    json = plan_to_json(scene.plan, NULL);
    assert(json && strstr(json, "\"transform\": \"liquidated\"") != NULL);
    free(json);
    plan_destroy(scene.plan);
}

int main(void) {
    test_every_development_is_a_relative();
    test_explicit_parameters();
    test_degenerate_cases();
    test_pickups_and_tonal_spelling();
    test_recognizability();
    test_scores_in_the_loop();
    test_constraint_json();
    puts("test_motive_develop: ok");
    return 0;
}
