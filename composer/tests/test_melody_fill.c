#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_applied.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_state.h>

/*
 * The melody operator, run through the fill loop after the harmony and bass operators, with the motive Given or
 * developed by the motivic development operators. Results are judged from what is in the plan: by the frameworks
 * (melody_check_lane reads the plan and calls standard-counterpoint, the NonChordTone labeler and
 * standard-harmony), and by the properties below, worked out here from the plan and not from the operator.
 */

#define MAX_CHORDS 64
#define MAX_NOTES 256
#define SIGNATURE_SIZE 16384

/* What the sweep has seen, to show that it exercises developments and non-chord tones. */
static int developments_written;
static int non_chord_tones_written;

typedef struct {
    int beats_per_bar;
    int beat_unit;
    KeyStructureMode mode;
    int tonic;
    int bars;
    CadenceType cadence;
    unsigned long seed;
} Scenario;

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    NodeId source;      /* the Given motive */
    NodeId developed;   /* the open motive node a development is written to */
    LaneSpanId target;  /* the melody span to fill, once added */
    PlanSpan span;      /* the phrase */
    PlanSpan form_span;
    Rational bar_length;
    Motive motive;
} Fixture;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Rational time_plus(Rational a, Rational b) {
    Rational sum;
    assert(rational_add(a, b, &sum) == RATIONAL_OK);
    return sum;
}

static LaneSpanId add_notes(PiecePlan *plan, NodeId owner, PlanLaneKind lane, PlanSpan span, int pitch) {
    PlanNote note;
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    note.midi = pitch;
    note.spelled = false;
    note.offset = fraction(0, 1);
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, owner, lane, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

/* A motive of about a bar for each meter: a rhythm of longs and shorts that turns up and comes back. */
static const char *motive_text(int beats, int unit) {
    if (unit == 8) {
        return "E4:q G4:e F4:e D4:e C4:e";
    }
    if (beats == 2) {
        return "E4:q G4:e F4:e";
    }
    if (beats == 3) {
        return "E4:q G4:e F4:e D4:q";
    }
    return "E4:q G4:e F4:e D4:q C4:q";
}

/*
 * One form node two bars longer than the phrase, a meter and a key over it, the harmony span to fill, and after
 * the phrase a Given motive in the first extra bar and the open node a development goes to in the second.
 */
static Fixture build(const Scenario *scenario, bool with_development) {
    Fixture fixture;
    PlanLanePayload payload;
    LaneSpanId id;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    MotiveOptions options;
    PlanSpan extra;
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plan_create();
    fixture.bar_length = fraction(scenario->beats_per_bar, (unsigned long)scenario->beat_unit);
    fixture.span.start = fraction(0, 1);
    fixture.span.length = fraction(scenario->bars * (long)fixture.bar_length.num, fixture.bar_length.den);
    fixture.form_span.start = fraction(0, 1);
    fixture.form_span.length = time_plus(fixture.span.length, time_plus(fixture.bar_length, fixture.bar_length));

    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, fixture.form_span, NULL, &fixture.owner) ==
           PLAN_OK);
    assert(plan_node_set_state(fixture.plan, fixture.owner, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = scenario->beats_per_bar;
    payload.meter.beat_unit = scenario->beat_unit;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_METER, 0, fixture.form_span, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, fixture.form_span, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.has_function = false;
    LaneSpanId harmony;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_HARMONY, 0, fixture.span, &payload, &harmony) ==
           PLAN_OK);
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = scenario->cadence;
    assert(plan_lane_span_add_constraint(fixture.plan, harmony, &cadence) == PLAN_OK);

    /* The motives live after the phrase: a section, a phrase and an idea hold them. */
    extra.start = fixture.span.length;
    extra.length = time_plus(fixture.bar_length, fixture.bar_length);
    assert(plan_add_node(fixture.plan, fixture.owner, PLAN_NODE_SECTION, extra, NULL, &section) == PLAN_OK);
    assert(plan_add_node(fixture.plan, section, PLAN_NODE_PHRASE, extra, NULL, &phrase) == PLAN_OK);
    assert(plan_add_node(fixture.plan, phrase, PLAN_NODE_IDEA, extra, NULL, &idea) == PLAN_OK);
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats_per_bar;
    options.meter.beat_unit = scenario->beat_unit;
    assert(motive_from_notes_text(motive_text(scenario->beats_per_bar, scenario->beat_unit), &options, &fixture.motive,
                                  NULL) == MOTIVE_OK);
    assert(motive_attach(fixture.plan, idea, extra.start, &fixture.motive, "motive", &fixture.source, NULL) ==
           PLAN_OK);
    if (with_development) {
        PlanSpan second = {time_plus(extra.start, fixture.bar_length), fixture.bar_length};
        PlanConstraint develop;
        memset(&develop, 0, sizeof(develop));
        develop.kind = PLAN_CONSTRAINT_MOTIVE_REF;
        assert(plan_add_node(fixture.plan, idea, PLAN_NODE_MOTIVE, second, "developed", &fixture.developed) ==
               PLAN_OK);
        develop.params.motive.motive = fixture.source;
        develop.params.motive.has_step = true;
        develop.params.motive.step = (MotiveRelationStep){.kind = MOTIVE_RELATION_TRANSPOSE, .value = {1, 2}};
        assert(plan_node_add_constraint(fixture.plan, fixture.developed, &develop) == PLAN_OK);
    }
    return fixture;
}

static FillOutcome run(Fixture *fixture, unsigned long seed, bool harmony, bool bass, bool develop,
                       const MelodyFillParams *melody, FillRecord **record) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = melody ? melody_fill_checker : harmony_phrase_checker;
    if (harmony) {
        assert(harmony_phrase_register(registry) == FILL_OK);
    }
    if (bass) {
        assert(harmony_bass_register(registry, NULL) == FILL_OK);
    }
    if (develop) {
        config.checker = motive_develop_checker;
        assert(motive_develop_register(registry, NULL) == FILL_OK);
    }
    if (melody) {
        assert(melody_fill_register(registry, melody) == FILL_OK);
    }
    assert(fill_run(fixture->plan, registry, &config, &outcome, record) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* The harmony and then the bass, which the melody is written over. */
static void write_harmony_and_bass(Fixture *fixture, unsigned long seed) {
    FillOutcome outcome = run(fixture, seed, true, false, false, NULL, NULL);
    assert(outcome.status == FILL_STATUS_COMPLETE || outcome.status == FILL_STATUS_STUCK);
    fill_outcome_destroy(&outcome);
    add_notes(fixture->plan, fixture->owner, PLAN_LANE_BASS, fixture->span, 40);
    outcome = run(fixture, seed, false, true, false, NULL, NULL);
    fill_outcome_destroy(&outcome);
}

/* The Given motive's node, or the developed one when a development was written to it. */
static NodeId motive_for(const Fixture *fixture, bool developed, bool *is_developed) {
    LaneSpanId ids[4];
    *is_developed = false;
    if (developed && fixture->developed != PLAN_NODE_NONE && plan_node_lane_spans(fixture->plan, fixture->developed, ids, 4) > 0 &&
        plan_lane_span_state(fixture->plan, ids[0]) == PLAN_STATE_REALIZED) {
        *is_developed = true;
        return fixture->developed;
    }
    return fixture->source;
}

static void add_target(Fixture *fixture, NodeId motive, bool transposed) {
    fixture->target = add_notes(fixture->plan, fixture->owner, PLAN_LANE_MELODY, fixture->span, 60);
    PlanConstraint ref;
    memset(&ref, 0, sizeof(ref));
    ref.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    ref.params.motive.motive = motive;
    ref.params.motive.has_step = transposed;
    ref.params.motive.step.kind = MOTIVE_RELATION_TRANSPOSE;
    assert(plan_lane_span_add_constraint(fixture->plan, fixture->target, &ref) == PLAN_OK);
}

/* The chord tones of a chord, worked out from its Roman numeral: the oracle the strong beats are held to. */
static unsigned chord_pcs(KeyStructureMode mode, int tonic, const char *roman) {
    static const int MAJOR_TRIAD[3] = {0, 4, 7};
    static const int MINOR_TRIAD[3] = {0, 3, 7};
    static const int DIMINISHED_TRIAD[3] = {0, 3, 6};
    static const int DOMINANT[4] = {0, 4, 7, 10};
    static const int MAJOR_SEVENTH[4] = {0, 4, 7, 11};
    static const int MINOR_SEVENTH[4] = {0, 3, 7, 10};
    static const int HALF_DIMINISHED[4] = {0, 3, 6, 10};
    static const int DIMINISHED_SEVENTH[4] = {0, 3, 6, 9};
    static const int MAJOR_QUALITY[8] = {0, 0, 1, 1, 0, 3, 1, 2};
    static const int MINOR_QUALITY[8] = {0, 1, 2, 0, 1, 3, 0, 3};
    HarmonyMode harmony_mode = mode == KEY_STRUCTURE_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
    HarmonyAppliedChord applied;
    HarmonyChord chord;
    const int *intervals;
    size_t count;
    int root;
    unsigned mask = 0;
    if (harmony_roman_parse_applied(mode, roman, &applied)) {
        root = harmony_applied_root_semitone(harmony_mode, &applied);
        switch (applied.quality) {
        case HARMONY_APPLIED_MAJOR_TRIAD:
            intervals = MAJOR_TRIAD;
            count = 3;
            break;
        case HARMONY_APPLIED_DOMINANT_SEVENTH:
            intervals = DOMINANT;
            count = 4;
            break;
        case HARMONY_APPLIED_DIMINISHED_TRIAD:
            intervals = DIMINISHED_TRIAD;
            count = 3;
            break;
        case HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH:
            intervals = DIMINISHED_SEVENTH;
            count = 4;
            break;
        default:
            intervals = HALF_DIMINISHED;
            count = 4;
            break;
        }
    } else {
        int quality;
        assert(harmony_roman_parse(mode, roman, &chord));
        quality = mode == KEY_STRUCTURE_MODE_MINOR ? MINOR_QUALITY[chord.degree] : MAJOR_QUALITY[chord.degree];
        root = mode == KEY_STRUCTURE_MODE_MINOR && chord.degree == 7 && chord.leading_tone
                   ? 11
                   : harmony_applied_scale_semitone(harmony_mode, chord.degree);
        if (mode == KEY_STRUCTURE_MODE_MINOR && chord.degree == 7 && chord.leading_tone) {
            quality = 2;
        }
        if (!chord.seventh) {
            intervals = quality == 1 ? MINOR_TRIAD : (quality == 2 ? DIMINISHED_TRIAD : MAJOR_TRIAD);
            count = 3;
        } else {
            intervals = quality == 3 ? DOMINANT
                        : quality == 2 ? (chord.degree == 7 ? DIMINISHED_SEVENTH : HALF_DIMINISHED)
                        : quality == 1 ? MINOR_SEVENTH
                                       : MAJOR_SEVENTH;
            count = 4;
        }
    }
    for (size_t k = 0; k < count; k++) {
        mask |= 1u << ((tonic + root + intervals[k]) % 12);
    }
    return mask;
}

typedef struct {
    Rational start;
    Rational end;
    int pitch;
} Sounding;

/* The melody notes over the phrase, as absolute times, in order. */
static size_t read_melody(const Fixture *fixture, Sounding *out, size_t capacity, size_t *span_count) {
    LaneSpanId ids[MAX_CHORDS];
    size_t found = plan_lane_find(fixture->plan, PLAN_LANE_MELODY, 0, fixture->span, ids, MAX_CHORDS);
    size_t count = 0;
    *span_count = found;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, ids[i]);
        const PlanNotes *notes = &plan_lane_span_payload(fixture->plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(count < capacity);
            out[count].start = time_plus(span.start, notes->notes[n].offset);
            out[count].end = time_plus(out[count].start, notes->notes[n].duration);
            out[count].pitch = notes->notes[n].midi;
            count++;
        }
    }
    return count;
}

static size_t sounding_at(const Sounding *notes, size_t count, Rational at) {
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(notes[i].start, at) <= 0 && rational_cmp(at, notes[i].end) < 0) {
            return i;
        }
    }
    return count;
}

static void signature(const Fixture *fixture, char *out, size_t size) {
    Sounding notes[MAX_NOTES];
    size_t spans;
    size_t count = read_melody(fixture, notes, MAX_NOTES, &spans);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        used += (size_t)snprintf(out + used, size - used, "%lld/%lld+%lld/%lld:%d ", notes[i].start.num, notes[i].start.den,
                                 notes[i].end.num, notes[i].end.den, notes[i].pitch);
        assert(used + 128 < size);
    }
}

/* What the plan holds once the melody operator has run, judged from the plan alone. */
static void check_melody(const Fixture *fixture, const Scenario *scenario, const MelodyFillParams *params) {
    Sounding notes[MAX_NOTES];
    size_t spans;
    size_t count = read_melody(fixture, notes, MAX_NOTES, &spans);
    LaneSpanId chords[MAX_CHORDS];
    size_t chord_count = plan_lane_find(fixture->plan, PLAN_LANE_HARMONY, 0, fixture->span, chords, MAX_CHORDS);
    assert(count >= 2 && chord_count >= 2);

    /* The melody passes the frameworks' rules: standard-counterpoint (melodic, and harmonic against the bass),
       the NonChordTone labeler and standard-harmony's resolution, read from the plan. */
    AnalysisKey key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    const char *violation = "unread";
    assert(melody_check_lane(fixture->plan, fixture->span, key, params, &violation));
    assert(violation == NULL);

    /* The spans tile the phrase, are settled, and the notes follow one another from its start to its end. */
    Rational at = fixture->span.start;
    LaneSpanId melody[MAX_CHORDS];
    assert(plan_lane_find(fixture->plan, PLAN_LANE_MELODY, 0, fixture->span, melody, MAX_CHORDS) == spans);
    for (size_t i = 0; i < spans; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, melody[i]);
        assert(rational_cmp(span.start, at) == 0);
        assert(plan_lane_span_state(fixture->plan, melody[i]) == PLAN_STATE_REALIZED);
        assert(plan_span_end(span, &at) == PLAN_OK);
    }
    assert(rational_cmp(at, time_plus(fixture->span.start, fixture->span.length)) == 0);
    assert(rational_cmp(notes[0].start, fixture->span.start) == 0);
    for (size_t i = 1; i < count; i++) {
        assert(rational_cmp(notes[i - 1].end, notes[i].start) == 0);
    }
    assert(rational_cmp(notes[count - 1].end, at) == 0);
    for (size_t i = 0; i < count; i++) {
        assert(notes[i].pitch >= params->range_low && notes[i].pitch <= params->range_high);
    }

    /* The first beat of every bar and the start of every chord are strong beats, and a note that starts on one is a
       tone of the chord sounding there. */
    for (size_t n = 0; n < count; n++) {
        long beat_num = notes[n].start.num * (long)fixture->bar_length.den;
        long beat_den = (long)notes[n].start.den * fixture->bar_length.num;
        bool downbeat = beat_num % beat_den == 0;
        for (size_t c = 0; c < chord_count; c++) {
            PlanSpan chord_span = plan_lane_span_span(fixture->plan, chords[c]);
            Rational end;
            assert(plan_span_end(chord_span, &end) == PLAN_OK);
            if (rational_cmp(notes[n].start, chord_span.start) >= 0 && rational_cmp(notes[n].start, end) < 0) {
                bool chord_start = rational_cmp(notes[n].start, chord_span.start) == 0;
                unsigned tones = chord_pcs(scenario->mode, scenario->tonic,
                                           plan_lane_span_payload(fixture->plan, chords[c])->harmony.roman_numeral);
                non_chord_tones_written += !(tones & (1u << (notes[n].pitch % 12)));
                if (downbeat || chord_start) {
                    unsigned mask = chord_pcs(scenario->mode, scenario->tonic,
                                              plan_lane_span_payload(fixture->plan, chords[c])->harmony.roman_numeral);
                    assert(mask & (1u << (notes[n].pitch % 12)));
                }
            }
        }
    }

    /* Exactly one note is the highest. */
    int top = 0;
    size_t at_top = 0;
    for (size_t i = 0; i < count; i++) {
        if (notes[i].pitch > top) {
            top = notes[i].pitch;
            at_top = 1;
        } else if (notes[i].pitch == top) {
            at_top++;
        }
    }
    assert(at_top == 1);

    /* The cadence arrives on its stable tone, and the leading tone resolves to the tonic at an authentic one. */
    int last = notes[count - 1].pitch % 12;
    if (scenario->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC) {
        assert(last == scenario->tonic);
    }
    if (scenario->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC) {
        assert(last == (scenario->tonic + (scenario->mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 4)) % 12);
    }
    if (scenario->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || scenario->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC) {
        PlanSpan final_chord = plan_lane_span_span(fixture->plan, chords[chord_count - 1]);
        Rational before = final_chord.start;
        size_t from = count;
        size_t to = sounding_at(notes, count, final_chord.start);
        for (size_t i = 0; i < count; i++) {
            if (rational_cmp(notes[i].end, before) <= 0) {
                from = i;
            }
        }
        assert(to < count);
        if (from < count && to == from + 1 && notes[from].pitch % 12 == (scenario->tonic + 11) % 12) {
            assert(notes[to].pitch == notes[from].pitch + 1);
        }
    }
}

static bool pipeline(const Scenario *scenario, bool developed, const MelodyFillParams *params, char *text,
                     char *label) {
    Fixture fixture = build(scenario, developed);
    write_harmony_and_bass(&fixture, scenario->seed);

    /* The development, written to its own node. */
    FillOutcome outcome;
    if (developed) {
        outcome = run(&fixture, scenario->seed, false, false, true, NULL, NULL);
        fill_outcome_destroy(&outcome);
    }
    bool is_developed;
    NodeId motive = motive_for(&fixture, developed, &is_developed);
    add_target(&fixture, motive, is_developed);

    LaneSpanId given_ids[4];
    assert(plan_node_lane_spans(fixture.plan, fixture.source, given_ids, 4) == 1);
    const PlanNotes *given = &plan_lane_span_payload(fixture.plan, given_ids[0])->notes;
    int given_pitch = given->notes[0].midi;
    size_t given_count = given->note_count;

    FillRecord *record = NULL;
    outcome = run(&fixture, scenario->seed, false, false, false, params, &record);
    bool written = plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_REALIZED;
    label[0] = '\0';
    text[0] = '\0';
    if (!written) {
        /* No valid candidate: the span stays open, and the loop says so. */
        bool found = false;
        for (size_t i = 0; i < outcome.stuck_count; i++) {
            if (outcome.stuck[i].target.id == fixture.target) {
                assert(outcome.stuck[i].reason == FILL_STUCK_ALL_REJECTED);
                found = true;
            }
        }
        assert(found);
        assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
        assert(rational_cmp(plan_lane_span_span(fixture.plan, fixture.target).length, fixture.span.length) == 0);
    }
    fill_outcome_destroy(&outcome);
    if (written) {
        check_melody(&fixture, scenario, params);
        signature(&fixture, text, SIGNATURE_SIZE);
        /* The record says whether the motive was the original or a development. */
        assert(record);
        bool recorded = false;
        for (size_t i = 0; i < record->step_count; i++) {
            const FillStep *step = &record->steps[i];
            if (step->chosen >= 0 && strcmp(step->considered[step->chosen].operator_name, MELODY_FILL_OPERATOR_NAME) == 0) {
                snprintf(label, FILL_LABEL_MAX, "%s", step->considered[step->chosen].label);
                assert(strncmp(label, is_developed ? "developed" : "original", is_developed ? 9 : 8) == 0);
                developments_written += is_developed;
                recorded = true;
            }
        }
        assert(recorded);
    }
    fill_record_destroy(record);

    /* The Given motive is untouched. */
    assert(plan_lane_span_state(fixture.plan, given_ids[0]) == PLAN_STATE_GIVEN);
    given = &plan_lane_span_payload(fixture.plan, given_ids[0])->notes;
    assert(given->note_count == given_count && given->notes[0].midi == given_pitch);
    plan_destroy(fixture.plan);
    return written;
}

static bool check_scenario(const Scenario *scenario, bool developed, const MelodyFillParams *params) {
    static char first[SIGNATURE_SIZE];
    static char second[SIGNATURE_SIZE];
    char label_a[FILL_LABEL_MAX];
    char label_b[FILL_LABEL_MAX];
    bool written = pipeline(scenario, developed, params, first, label_a);
    /* The same seed gives the same melody, or the same refusal. */
    assert(pipeline(scenario, developed, params, second, label_b) == written);
    assert(strcmp(first, second) == 0);
    assert(strcmp(label_a, label_b) == 0);
    return written;
}

static void test_sweep(void) {
    static const int meters[4][2] = {{2, 4}, {3, 4}, {4, 4}, {6, 8}};
    static const int tonics[3] = {0, 6, 10};
    static const int bars[2] = {5, 8};
    static const CadenceType cadences[5] = {CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE,
                                                CADENCE_TYPE_PLAGAL};
    MelodyFillParams params = melody_fill_params_default();
    int total = 0;
    int written = 0;
    for (int m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int t = 0; t < 3; t++) {
                for (int b = 0; b < 2; b++) {
                    for (int c = 0; c < 5; c++) {
                        for (unsigned long seed = 0; seed < 2; seed++) {
                            Scenario scenario = {meters[m][0], meters[m][1], mode ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR,
                                                 tonics[t], bars[b], cadences[c], seed};
                            total++;
                            written += check_scenario(&scenario, seed == 1, &params);
                        }
                    }
                }
            }
        }
    }
    /* Most phrases have a melody; the rest are the ones no line follows without breaking a hard rule. */
    printf("melody written for %d of %d phrases\n", written, total);
    assert(written * 10 >= total * 3);
    assert(developments_written > 0 && non_chord_tones_written > 0);
}

/* The parameters change what is written: with no non-chord tones every note is a chord tone. */
static void test_chord_tones_only(void) {
    MelodyFillParams params = melody_fill_params_default();
    int written = 0;
    params.nct_types = 0;
    for (unsigned long seed = 0; seed < 12; seed++) {
        Scenario scenario = {4, 4, KEY_STRUCTURE_MODE_MAJOR, 0, 8, CADENCE_TYPE_PERFECT_AUTHENTIC, seed};
        written += check_scenario(&scenario, false, &params);
    }
    assert(written > 0);
}

/* A range no placement of the motive fits in leaves the span open: no valid candidate. */
static void test_unplaceable_motive(void) {
    Scenario scenario = {4, 4, KEY_STRUCTURE_MODE_MAJOR, 0, 8, CADENCE_TYPE_PERFECT_AUTHENTIC, 1};
    MelodyFillParams narrow = melody_fill_params_default();
    narrow.range_low = 60;
    narrow.range_high = 61;
    char text[SIGNATURE_SIZE];
    char label[FILL_LABEL_MAX];
    assert(!pipeline(&scenario, false, &narrow, text, label));

    /* A motive later than the phrase has room for is not placed either. */
    MelodyFillParams late = melody_fill_params_default();
    late.start_cell = 1000;
    assert(!pipeline(&scenario, false, &late, text, label));
}

/* The melody operator does not apply to a span whose harmony or bass is still open. */
static void test_needs_harmony_and_bass(void) {
    Scenario scenario = {4, 4, KEY_STRUCTURE_MODE_MAJOR, 0, 8, CADENCE_TYPE_PERFECT_AUTHENTIC, 1};
    Fixture fixture = build(&scenario, false);
    MelodyFillParams params = melody_fill_params_default();
    add_target(&fixture, fixture.source, false);
    FillOutcome outcome = run(&fixture, 1, false, false, false, &params, NULL);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.stuck_count >= 1);
    bool found = false;
    for (size_t i = 0; i < outcome.stuck_count; i++) {
        if (outcome.stuck[i].target.id == fixture.target) {
            assert(outcome.stuck[i].reason == FILL_STUCK_NO_OPERATOR);
            found = true;
        }
    }
    assert(found);
    assert(plan_lane_span_state(fixture.plan, fixture.target) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(fixture.plan);
}

/*
 * Ideas. A phrase of two ideas over one harmony span and one bass span; the melody is two spans, one for each idea,
 * each asking for the motive, the second asking for the cadence. The chords cross the middle of the phrase where they
 * like. What is held to, worked out here from the plan:
 *   - the ideas are written in time order: the second is not offered until the first is written;
 *   - the join: the leap from the last note of the first idea to the first note of the second is one
 *     standard-counterpoint allows;
 *   - the climax: the phrase has one highest note, in the idea the climax position falls in (the second), higher
 *     than every note of the first, and the first keeps to three semitones under the top of the range;
 *   - the end tone is asked of the idea with the cadence only: the second ends on the tonic, the first on any
 *     chord tone.
 */
typedef struct {
    PiecePlan *plan;
    NodeId phrase;
    NodeId source;
    LaneSpanId first;
    LaneSpanId second;
    PlanSpan span;
} IdeaFixture;

static IdeaFixture build_ideas(int beats, int unit, KeyStructureMode mode, int tonic) {
    IdeaFixture fixture;
    PlanLanePayload payload;
    LaneSpanId id;
    NodeId form;
    NodeId first_idea;
    NodeId second_idea;
    NodeId section;
    NodeId material;
    NodeId material_idea;
    MotiveOptions options;
    Motive motive;
    Rational bar = fraction(beats, (unsigned long)unit);
    PlanSpan form_span = {fraction(0, 1), fraction(beats * 5L, (unsigned long)unit)};
    PlanSpan phrase_span = {fraction(0, 1), fraction(beats * 4L, (unsigned long)unit)};
    PlanSpan first_span = {fraction(0, 1), fraction(beats * 2L, (unsigned long)unit)};
    PlanSpan second_span = {first_span.length, first_span.length};
    PlanSpan extra = {phrase_span.length, bar};
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plan_create();
    fixture.span = phrase_span;
    assert(plan_add_node(fixture.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form_span, NULL, &form) == PLAN_OK);
    assert(plan_add_node(fixture.plan, form, PLAN_NODE_SECTION, phrase_span, NULL, &section) == PLAN_OK);
    assert(plan_add_node(fixture.plan, section, PLAN_NODE_PHRASE, phrase_span, "phrase", &fixture.phrase) == PLAN_OK);
    assert(plan_add_node(fixture.plan, fixture.phrase, PLAN_NODE_IDEA, first_span, "first", &first_idea) == PLAN_OK);
    assert(plan_add_node(fixture.plan, fixture.phrase, PLAN_NODE_IDEA, second_span, "second", &second_idea) ==
           PLAN_OK);
    assert(plan_add_node(fixture.plan, form, PLAN_NODE_SECTION, extra, NULL, &material) == PLAN_OK);
    assert(plan_add_node(fixture.plan, material, PLAN_NODE_PHRASE, extra, NULL, &material_idea) == PLAN_OK);
    assert(plan_add_node(fixture.plan, material_idea, PLAN_NODE_IDEA, extra, NULL, &material_idea) == PLAN_OK);
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = beats;
    options.meter.beat_unit = unit;
    assert(motive_from_notes_text(motive_text(beats, unit), &options, &motive, NULL) == MOTIVE_OK);
    assert(motive_attach(fixture.plan, material_idea, extra.start, &motive, "m0", &fixture.source, NULL) == PLAN_OK);
    for (NodeId node = 1; node <= plan_node_count(fixture.plan); node++) {
        if (plan_node_kind(fixture.plan, node) != PLAN_NODE_MOTIVE) {
            assert(plan_node_set_state(fixture.plan, node, PLAN_STATE_GIVEN) == PLAN_OK);
        }
    }

    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = beats;
    payload.meter.beat_unit = unit;
    assert(plan_add_lane_span(fixture.plan, form, PLAN_LANE_METER, 0, form_span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = plan_key_of_pitch_class(tonic, mode);
    assert(plan_add_lane_span(fixture.plan, form, PLAN_LANE_KEY, 0, form_span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.has_function = false;
    assert(plan_add_lane_span(fixture.plan, fixture.phrase, PLAN_LANE_HARMONY, 0, phrase_span, &payload, &id) ==
           PLAN_OK);
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    assert(plan_lane_span_add_constraint(fixture.plan, id, &cadence) == PLAN_OK);

    PlanConstraint ref;

    memset(&ref, 0, sizeof(ref));

    ref.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    ref.params.motive.motive = fixture.source;
    fixture.first = add_notes(fixture.plan, first_idea, PLAN_LANE_MELODY, first_span, 60);
    fixture.second = add_notes(fixture.plan, second_idea, PLAN_LANE_MELODY, second_span, 60);
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.first, &ref) == PLAN_OK);
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.second, &ref) == PLAN_OK);
    assert(plan_lane_span_add_constraint(fixture.plan, fixture.second, &cadence) == PLAN_OK);
    return fixture;
}

static bool leap_allowed(int distance) {
    int size = distance < 0 ? -distance : distance;
    return size <= 7 || size == 12 || distance == 8;
}

/* The melody notes of one span as MIDI pitches, in time order. */
static size_t span_pitches(const IdeaFixture *fixture, LaneSpanId span, int *out, size_t capacity) {
    const PlanNotes *notes = &plan_lane_span_payload(fixture->plan, span)->notes;
    assert(notes->note_count <= capacity);
    for (size_t i = 0; i < notes->note_count; i++) {
        out[i] = notes->notes[i].midi;
    }
    return notes->note_count;
}

static void test_ideas_in_order(void) {
    static const int meters[3][2] = {{4, 4}, {3, 4}, {6, 8}};
    int completed = 0;
    int first_ends_off_tonic = 0;
    int stuck_second_first = 0;
    for (int m = 0; m < 3; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (unsigned long seed = 1; seed <= 12; seed++) {
                IdeaFixture fixture = build_ideas(meters[m][0], meters[m][1], mode ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR,
                                                  mode ? 9 : 0);
                Fixture harmony_only;
                FillRecord *record = NULL;
                memset(&harmony_only, 0, sizeof(harmony_only));
                harmony_only.plan = fixture.plan;
                harmony_only.owner = fixture.phrase;
                harmony_only.span = fixture.span;
                write_harmony_and_bass(&harmony_only, seed);
                MelodyFillParams params = melody_fill_params_default();
                FillOutcome outcome = run(&harmony_only, seed, false, false, false, &params, &record);
                if (outcome.status != FILL_STATUS_COMPLETE) {
                    /* An idea that cannot be placed leaves the span open; the first is never skipped for it. */
                    assert(plan_lane_span_state(fixture.plan, fixture.second) == PLAN_STATE_CONSTRAINED ||
                           plan_lane_span_state(fixture.plan, fixture.first) == PLAN_STATE_CONSTRAINED);
                    assert(plan_lane_span_state(fixture.plan, fixture.first) != PLAN_STATE_CONSTRAINED ||
                           plan_lane_span_state(fixture.plan, fixture.second) == PLAN_STATE_CONSTRAINED);
                    fill_outcome_destroy(&outcome);
                    fill_record_destroy(record);
                    plan_destroy(fixture.plan);
                    continue;
                }
                completed++;

                /* Time order: the second idea is offered and refused until the first is written. */
                size_t first_applied = record->step_count;
                size_t second_applied = record->step_count;
                bool second_refused_first = false;
                for (size_t i = 0; i < record->step_count; i++) {
                    const FillStep *step = &record->steps[i];
                    if (step->outcome == FILL_STEP_APPLIED && step->target.id == fixture.first) {
                        first_applied = i;
                    } else if (step->outcome == FILL_STEP_APPLIED && step->target.id == fixture.second) {
                        second_applied = i;
                    } else if (step->outcome == FILL_STEP_NO_OPERATOR && step->target.id == fixture.second &&
                               first_applied == record->step_count) {
                        second_refused_first = true;
                    }
                }
                assert(first_applied < second_applied);
                stuck_second_first += second_refused_first;

                int first[MAX_NOTES];
                int second[MAX_NOTES];
                size_t first_count = span_pitches(&fixture, fixture.first, first, MAX_NOTES);
                size_t second_count = span_pitches(&fixture, fixture.second, second, MAX_NOTES);
                assert(first_count > 0 && second_count > 0);

                /* The join is not held to the melodic rules: a motive starts as its own pitches do (R-335). */
                /* One climax, in the second idea, above the first; the first leaves room for it. */
                int first_top = 0;
                int second_top = 0;
                size_t at_top = 0;
                for (size_t i = 0; i < first_count; i++) {
                    first_top = first[i] > first_top ? first[i] : first_top;
                    assert(first[i] >= 60 && first[i] <= 84);
                }
                for (size_t i = 0; i < second_count; i++) {
                    second_top = second[i] > second_top ? second[i] : second_top;
                    assert(second[i] >= 60 && second[i] <= 84);
                }
                for (size_t i = 0; i < second_count; i++) {
                    at_top += second[i] == second_top;
                }
                assert(second_top > first_top && at_top == 1);

                /* The end tone belongs to the idea with the cadence. */
                int tonic = mode ? 9 : 0;
                assert(((second[second_count - 1] - tonic) % 12 + 12) % 12 == 0);
                first_ends_off_tonic += ((first[first_count - 1] - tonic) % 12 + 12) % 12 != 0;

                fill_outcome_destroy(&outcome);
                fill_record_destroy(record);
                plan_destroy(fixture.plan);
            }
        }
    }
    /* Most phrases are written, the first idea is not made to end on the tonic, and the loop did offer the second early. */
    assert(completed >= 18);
    assert(first_ends_off_tonic > 0);
    assert(stuck_second_first > 0);
}

/*
 * Tone roles (R-215). A motive of a bar with a passing tone and a neighbor tone is placed in two ideas, over I and then over
 * IV. Held to, worked out here from the plan:
 *   - a note the motive labels a chord tone is a tone of the chord it sounds over, in both ideas;
 *   - a note the motive labels a non-chord tone is either a chord tone (a figure that could not be formed becomes the
 *     nearest chord tone) or a figure that resolves: a step from the next note, a chord tone;
 *   - over IV, which the motive was not written for, a clash is a figure the framework labels or a chord tone.
 * A motive with no key has no tones, and its plan carries no motive_tones constraint.
 */
typedef struct {
    PiecePlan *plan;
    NodeId source;
    LaneSpanId first;
    LaneSpanId second;
} ToneScene;

static ToneScene build_tone_scene(bool keyed) {
    static const char *const MOTIVE = "C4:q D4:e E4:e G4:e A4:e G4:q";
    ToneScene scene;
    PlanLanePayload payload;
    LaneSpanId id;
    NodeId form;
    NodeId section;
    NodeId phrase;
    NodeId idea_one;
    NodeId idea_two;
    NodeId material;
    NodeId material_idea;
    PlanSpan bar_zero = {fraction(0, 1), fraction(1, 1)};
    PlanSpan bar_one = {fraction(1, 1), fraction(1, 1)};
    PlanSpan phrase_span = {fraction(0, 1), fraction(2, 1)};
    PlanSpan form_span = {fraction(0, 1), fraction(3, 1)};
    PlanSpan extra = {fraction(2, 1), fraction(1, 1)};
    MotiveOptions options;
    Motive motive;
    memset(&scene, 0, sizeof(scene));
    scene.plan = plan_create();
    assert(plan_add_node(scene.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form_span, NULL, &form) == PLAN_OK);
    assert(plan_add_node(scene.plan, form, PLAN_NODE_SECTION, phrase_span, NULL, &section) == PLAN_OK);
    assert(plan_add_node(scene.plan, section, PLAN_NODE_PHRASE, phrase_span, "phrase", &phrase) == PLAN_OK);
    assert(plan_add_node(scene.plan, phrase, PLAN_NODE_IDEA, bar_zero, "first", &idea_one) == PLAN_OK);
    assert(plan_add_node(scene.plan, phrase, PLAN_NODE_IDEA, bar_one, "second", &idea_two) == PLAN_OK);
    assert(plan_add_node(scene.plan, form, PLAN_NODE_SECTION, extra, NULL, &material) == PLAN_OK);
    assert(plan_add_node(scene.plan, material, PLAN_NODE_PHRASE, extra, NULL, &material_idea) == PLAN_OK);
    assert(plan_add_node(scene.plan, material_idea, PLAN_NODE_IDEA, extra, NULL, &material_idea) == PLAN_OK);
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    if (keyed) {
        options.has_key = true;
        options.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    }
    assert(motive_from_notes_text(MOTIVE, &options, &motive, NULL) == MOTIVE_OK);
    assert(motive_attach(scene.plan, material_idea, extra.start, &motive, "m0", &scene.source, NULL) == PLAN_OK);
    assert(plan_node_has_constraint(scene.plan, scene.source, PLAN_CONSTRAINT_MOTIVE_TONES) == keyed);
    for (NodeId node = 1; node <= plan_node_count(scene.plan); node++) {
        if (plan_node_kind(scene.plan, node) != PLAN_NODE_MOTIVE) {
            assert(plan_node_set_state(scene.plan, node, PLAN_STATE_GIVEN) == PLAN_OK);
        }
    }
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(scene.plan, form, PLAN_LANE_METER, 0, form_span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(scene.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    assert(plan_add_lane_span(scene.plan, form, PLAN_LANE_KEY, 0, form_span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(scene.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    static const char *const CHORDS[2] = {"I", "IV"};
    static const int BASS[2] = {48, 41};
    PlanSpan bars[2] = {bar_zero, bar_one};
    for (int bar = 0; bar < 2; bar++) {
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, CHORDS[bar]);
        payload.harmony.has_function = false;
        assert(plan_add_lane_span(scene.plan, phrase, PLAN_LANE_HARMONY, 0, bars[bar], &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(scene.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
        id = add_notes(scene.plan, phrase, PLAN_LANE_BASS, bars[bar], BASS[bar]);
        assert(plan_lane_span_set_state(scene.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    }
    PlanConstraint ref;
    memset(&ref, 0, sizeof(ref));
    ref.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    ref.params.motive.motive = scene.source;
    scene.first = add_notes(scene.plan, idea_one, PLAN_LANE_MELODY, bar_zero, 60);
    scene.second = add_notes(scene.plan, idea_two, PLAN_LANE_MELODY, bar_one, 60);
    assert(plan_lane_span_add_constraint(scene.plan, scene.first, &ref) == PLAN_OK);
    assert(plan_lane_span_add_constraint(scene.plan, scene.second, &ref) == PLAN_OK);
    return scene;
}

/* Whether a pitch is a tone of the chord. */
static bool tone_of(const char *roman, int pitch) {
    return ((chord_pcs(KEY_STRUCTURE_MODE_MAJOR, 0, roman) >> (((pitch % 12) + 12) % 12)) & 1u) != 0;
}

static int step_between(int a, int b) {
    int distance = a > b ? a - b : b - a;
    return distance >= 1 && distance <= 2;
}

static void test_tone_roles(void) {
    /* The motive's labels against its base harmony I: the passing and the neighbor tone, the rest chord tones. */
    static const bool CHORD_NOTE[6] = {true, false, true, true, false, true};
    int kept[2][2] = {{0, 0}, {0, 0}}; /* [idea][passing, neighbor] kept as non-chord tones */
    int completed = 0;
    for (unsigned long seed = 1; seed <= 24; seed++) {
        ToneScene scene = build_tone_scene(true);
        Fixture fixture;
        FillRecord *record = NULL;
        memset(&fixture, 0, sizeof(fixture));
        fixture.plan = scene.plan;
        MelodyFillParams params = melody_fill_params_default();
        FillOutcome outcome = run(&fixture, seed, false, false, false, &params, &record);
        if (outcome.status != FILL_STATUS_COMPLETE) {
            fill_outcome_destroy(&outcome);
            fill_record_destroy(record);
            plan_destroy(scene.plan);
            continue;
        }
        completed++;
        const LaneSpanId spans[2] = {scene.first, scene.second};
        const char *const chords[2] = {"I", "IV"};
        for (int idea = 0; idea < 2; idea++) {
            const PlanNotes *notes = &plan_lane_span_payload(scene.plan, spans[idea])->notes;
            assert(notes->note_count >= 6);
            for (size_t k = 0; k < 6; k++) {
                int pitch = notes->notes[k].midi;
                bool chord_tone = tone_of(chords[idea], pitch);
                if (CHORD_NOTE[k]) {
                    assert(chord_tone);
                } else if (!chord_tone) {
                    /* A figure that resolves: a step from the next note, which is a chord tone. */
                    assert(step_between(pitch, notes->notes[k + 1].midi));
                    assert(tone_of(chords[idea], notes->notes[k + 1].midi));
                    kept[idea][k == 1 ? 0 : 1]++;
                }
            }
        }
        fill_outcome_destroy(&outcome);
        fill_record_destroy(record);
        plan_destroy(scene.plan);
    }
    assert(completed >= 12);
    /* Over the chord the motive was written for both figures are kept. Over IV a clash stays only as a figure the framework
       labels (each note was held to chord tone or a resolving step above); the motive's D, E and G are not figures there, so
       they move to chord tones (R-328, R-335). */
    assert(kept[0][0] > 0 && kept[0][1] > 0);

    /* A motive with no key has no tones: the plan carries no motive_tones constraint and it is placed over both chords. */
    ToneScene plain = build_tone_scene(false);
    Fixture fixture;
    FillRecord *record = NULL;
    memset(&fixture, 0, sizeof(fixture));
    fixture.plan = plain.plan;
    MelodyFillParams params = melody_fill_params_default();
    FillOutcome outcome = run(&fixture, 3, false, false, false, &params, &record);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    assert(plan_lane_span_state(plain.plan, plain.first) != PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    fill_record_destroy(record);
    plan_destroy(plain.plan);
}

int main(void) {
    test_ideas_in_order();
    test_sweep();
    test_chord_tones_only();
    test_unplaceable_motive();
    test_needs_harmony_and_bass();
    test_tone_roles();
    puts("test_melody_fill: ok");
    return 0;
}
