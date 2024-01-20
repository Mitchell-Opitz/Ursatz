#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <harmony_applied.h>

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

typedef struct {
    int beats_per_bar;
    int beat_unit;
    PlanKeyMode mode;
    int tonic;
    int bars;
    PlanCadenceType cadence;
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
    PlanTime bar_length;
    Motive motive;
} Fixture;

static PlanTime fraction(long num, unsigned long den) {
    PlanTime time;
    assert(plan_time_make(num, den, &time) == PLAN_OK);
    return time;
}

static PlanTime time_plus(PlanTime a, PlanTime b) {
    PlanTime sum;
    assert(plan_time_add(a, b, &sum) == PLAN_OK);
    return sum;
}

static LaneSpanId add_notes(PiecePlan *plan, NodeId owner, PlanLaneKind lane, PlanSpan span, int pitch) {
    PlanNote note;
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    note.midi_pitch = pitch;
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
    payload.key.tonic_pitch_class = scenario->tonic;
    payload.key.mode = scenario->mode;
    assert(plan_add_lane_span(fixture.plan, fixture.owner, PLAN_LANE_KEY, 0, fixture.form_span, &payload, &id) ==
           PLAN_OK);
    assert(plan_lane_span_set_state(fixture.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
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
        PlanConstraint develop = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
        assert(plan_add_node(fixture.plan, idea, PLAN_NODE_MOTIVE, second, "developed", &fixture.developed) ==
               PLAN_OK);
        develop.params.motive.motive = fixture.source;
        develop.params.motive.transform = PLAN_TRANSFORM_TRANSPOSED;
        develop.params.motive.tonal = true;
        develop.params.motive.steps = 2;
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

static void add_target(Fixture *fixture, NodeId motive, PlanMotiveTransform transform) {
    fixture->target = add_notes(fixture->plan, fixture->owner, PLAN_LANE_MELODY, fixture->span, 60);
    PlanConstraint ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    ref.params.motive.motive = motive;
    ref.params.motive.transform = transform;
    assert(plan_lane_span_add_constraint(fixture->plan, fixture->target, &ref) == PLAN_OK);
}

/* The chord tones of a chord, worked out from its Roman numeral: the oracle the strong beats are held to. */
static unsigned chord_pcs(PlanKeyMode mode, int tonic, const char *roman) {
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
    HarmonyMode harmony_mode = mode == PLAN_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
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
        quality = mode == PLAN_MODE_MINOR ? MINOR_QUALITY[chord.degree] : MAJOR_QUALITY[chord.degree];
        root = mode == PLAN_MODE_MINOR && chord.degree == 7 && chord.leading_tone
                   ? 11
                   : harmony_applied_scale_semitone(harmony_mode, chord.degree);
        if (mode == PLAN_MODE_MINOR && chord.degree == 7 && chord.leading_tone) {
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
    PlanTime start;
    PlanTime end;
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
            out[count].pitch = notes->notes[n].midi_pitch;
            count++;
        }
    }
    return count;
}

static size_t sounding_at(const Sounding *notes, size_t count, PlanTime at) {
    for (size_t i = 0; i < count; i++) {
        if (plan_time_compare(notes[i].start, at) <= 0 && plan_time_compare(at, notes[i].end) < 0) {
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
        used += (size_t)snprintf(out + used, size - used, "%ld/%lu+%ld/%lu:%d ", notes[i].start.num, notes[i].start.den,
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
    PlanKey key = {scenario->tonic, scenario->mode};
    const char *violation = "unread";
    assert(melody_check_lane(fixture->plan, fixture->span, key, params, &violation));
    assert(violation == NULL);

    /* The spans tile the phrase, are settled, and the notes follow one another from its start to its end. */
    PlanTime at = fixture->span.start;
    LaneSpanId melody[MAX_CHORDS];
    assert(plan_lane_find(fixture->plan, PLAN_LANE_MELODY, 0, fixture->span, melody, MAX_CHORDS) == spans);
    for (size_t i = 0; i < spans; i++) {
        PlanSpan span = plan_lane_span_span(fixture->plan, melody[i]);
        assert(plan_time_compare(span.start, at) == 0);
        assert(plan_lane_span_state(fixture->plan, melody[i]) == PLAN_STATE_REALIZED);
        assert(plan_span_end(span, &at) == PLAN_OK);
    }
    assert(plan_time_compare(at, time_plus(fixture->span.start, fixture->span.length)) == 0);
    assert(plan_time_compare(notes[0].start, fixture->span.start) == 0);
    for (size_t i = 1; i < count; i++) {
        assert(plan_time_compare(notes[i - 1].end, notes[i].start) == 0);
    }
    assert(plan_time_compare(notes[count - 1].end, at) == 0);
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
            PlanTime end;
            assert(plan_span_end(chord_span, &end) == PLAN_OK);
            if (plan_time_compare(notes[n].start, chord_span.start) >= 0 && plan_time_compare(notes[n].start, end) < 0) {
                bool chord_start = plan_time_compare(notes[n].start, chord_span.start) == 0;
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
    if (scenario->cadence == PLAN_CADENCE_PAC) {
        assert(last == scenario->tonic);
    }
    if (scenario->cadence == PLAN_CADENCE_IAC) {
        assert(last == (scenario->tonic + (scenario->mode == PLAN_MODE_MINOR ? 3 : 4)) % 12);
    }
    if (scenario->cadence == PLAN_CADENCE_PAC || scenario->cadence == PLAN_CADENCE_IAC) {
        PlanSpan final_chord = plan_lane_span_span(fixture->plan, chords[chord_count - 1]);
        PlanTime before = final_chord.start;
        size_t from = count;
        size_t to = sounding_at(notes, count, final_chord.start);
        for (size_t i = 0; i < count; i++) {
            if (plan_time_compare(notes[i].end, before) <= 0) {
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
    add_target(&fixture, motive, is_developed ? PLAN_TRANSFORM_TRANSPOSED : PLAN_TRANSFORM_NONE);

    LaneSpanId given_ids[4];
    assert(plan_node_lane_spans(fixture.plan, fixture.source, given_ids, 4) == 1);
    const PlanNotes *given = &plan_lane_span_payload(fixture.plan, given_ids[0])->notes;
    int given_pitch = given->notes[0].midi_pitch;
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
        assert(plan_time_compare(plan_lane_span_span(fixture.plan, fixture.target).length, fixture.span.length) == 0);
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
                recorded = true;
            }
        }
        assert(recorded);
    }
    fill_record_destroy(record);

    /* The Given motive is untouched. */
    assert(plan_lane_span_state(fixture.plan, given_ids[0]) == PLAN_STATE_GIVEN);
    given = &plan_lane_span_payload(fixture.plan, given_ids[0])->notes;
    assert(given->note_count == given_count && given->notes[0].midi_pitch == given_pitch);
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
    static const PlanCadenceType cadences[5] = {PLAN_CADENCE_PAC, PLAN_CADENCE_IAC, PLAN_CADENCE_HC, PLAN_CADENCE_DC,
                                                PLAN_CADENCE_PC};
    MelodyFillParams params = melody_fill_params_default();
    int total = 0;
    int written = 0;
    for (int m = 0; m < 4; m++) {
        for (int mode = 0; mode < 2; mode++) {
            for (int t = 0; t < 3; t++) {
                for (int b = 0; b < 2; b++) {
                    for (int c = 0; c < 5; c++) {
                        for (unsigned long seed = 0; seed < 2; seed++) {
                            Scenario scenario = {meters[m][0], meters[m][1], mode ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR,
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
}

/* The parameters change what is written: with no non-chord tones every note is a chord tone. */
static void test_chord_tones_only(void) {
    MelodyFillParams params = melody_fill_params_default();
    int written = 0;
    params.nct_types = 0;
    for (unsigned long seed = 0; seed < 12; seed++) {
        Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, seed};
        written += check_scenario(&scenario, false, &params);
    }
    assert(written > 0);
}

/* A range no placement of the motive fits in leaves the span open: no valid candidate. */
static void test_unplaceable_motive(void) {
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, 1};
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
    Scenario scenario = {4, 4, PLAN_MODE_MAJOR, 0, 8, PLAN_CADENCE_PAC, 1};
    Fixture fixture = build(&scenario, false);
    MelodyFillParams params = melody_fill_params_default();
    add_target(&fixture, fixture.source, PLAN_TRANSFORM_NONE);
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

int main(void) {
    test_sweep();
    test_chord_tones_only();
    test_unplaceable_motive();
    test_needs_harmony_and_bass();
    puts("test_melody_fill: ok");
    return 0;
}
