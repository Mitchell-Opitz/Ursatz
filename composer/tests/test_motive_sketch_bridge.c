#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <analysis/motive_relation.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/motive_sketch_bridge.h>

/*
 * The bridge between the Composer's Motive and the library's MotiveSketch: what a sketch holds for a motive, the chord
 * a motive without harmony is given, a rhythm motive's unknown pitches, and the round trip of each kind of motive.
 */

static const PlanMeter FOUR_FOUR = {4, 4, 0};
static const PlanMeter SIX_EIGHT = {6, 8, 0};
static const AnalysisKey C_MAJOR = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
static const AnalysisKey D_MINOR = {NULL, PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};

static bool er_is(Rational time, long long num, long long den) {
    return time.num == num && time.den == den;
}

static Motive load(MotiveFormat format, const char *text, const PlanMeter *meter, const AnalysisKey *key) {
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
    MotiveResult result = motive_load(text, strlen(text), format, &options, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "'%s': %s: %s\n", text, motive_result_name(result), error.message);
    }
    assert(result == MOTIVE_OK);
    return motive;
}

static void add_harmony(Motive *motive, const char *text) {
    MotiveError error;
    assert(motive_harmony_parse(text, &motive->meter, motive->harmony, &motive->harmony_count, &error) == MOTIVE_OK);
}

static void add_feature(Motive *motive, const char *text) {
    MotiveError error;
    assert(motive_feature_parse(text, &motive->features[motive->feature_count], &error) == MOTIVE_OK);
    motive->feature_count++;
}

static void assert_round_trip(const Motive *motive) {
    MotiveSketch sketch;
    assert(motive_to_sketch(motive, &sketch));
    assert(motive_sketch_validate(&sketch));
    Motive back;
    assert(motive_from_sketch(&sketch, motive, &back));
    assert(motive_equal(motive, &back));
}

static void test_sketch_contents(void) {
    Motive motive = load(MOTIVE_FORMAT_NOTES, "r:e C4:e E4:q G4:q C5:q.", &FOUR_FOUR, &C_MAJOR);
    add_harmony(&motive, "I V6@2");
    add_feature(&motive, "a:0-1");
    add_feature(&motive, "b:2-3");
    motive.role = MOTIVE_ROLE_ACCOMPANIMENT;

    MotiveSketch sketch;
    assert(motive_to_sketch(&motive, &sketch));
    assert(sketch.note_count == 4);
    assert(sketch.notes[0].pitch == 60 && er_is(sketch.notes[0].onset, 1, 8) && er_is(sketch.notes[0].duration, 1, 8));
    assert(sketch.notes[3].pitch == 72 && er_is(sketch.notes[3].onset, 3, 4) && er_is(sketch.notes[3].duration, 3, 8));
    assert(sketch.has_scale && sketch.tonic_pc == 0 && !sketch.minor);
    assert(er_is(sketch.bar, 1, 1) && er_is(sketch.beat, 1, 4) && er_is(sketch.pickup, 0, 1));
    assert(er_is(sketch.length, 9, 8));
    assert(sketch.chord_count == 2 && sketch.chords[1].degree == 5 && sketch.chords[1].inversion == 1);
    assert(er_is(sketch.chords[1].onset, 1, 2));
    assert(sketch.feature_count == 2 && strcmp(sketch.features[1].label, "b") == 0 && sketch.features[1].first == 2);
    assert(sketch.accompaniment);

    Motive minor = load(MOTIVE_FORMAT_NOTES, "D4:q F4:q A4:q", &(PlanMeter){3, 4, 0}, &D_MINOR);
    assert(motive_to_sketch(&minor, &sketch));
    assert(sketch.has_scale && sketch.tonic_pc == 2 && sketch.minor && er_is(sketch.bar, 3, 4) && !sketch.accompaniment);
}

static void test_implied_chord(void) {
    MotiveSketch sketch;
    Motive motive = load(MOTIVE_FORMAT_NOTES, "D4:q F4:q A4:q", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch(&motive, &sketch));
    assert(sketch.chord_count == 1 && sketch.chords[0].degree == 2 && sketch.chords[0].alteration == 0);
    assert(er_is(sketch.chords[0].onset, 0, 1));

    /* The tonic wins a tie, then the dominant over the rest. */
    motive = load(MOTIVE_FORMAT_NOTES, "C4:q G4:q", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch(&motive, &sketch) && sketch.chord_count == 1 && sketch.chords[0].degree == 1);
    motive = load(MOTIVE_FORMAT_NOTES, "G4:q B4:q D5:q", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch(&motive, &sketch) && sketch.chords[0].degree == 5);

    /* Own harmony stands as it is; no key, no chord. */
    add_harmony(&motive, "IV");
    assert(motive_to_sketch(&motive, &sketch) && sketch.chord_count == 1 && sketch.chords[0].degree == 4);
    motive = load(MOTIVE_FORMAT_NOTES, "C4:q E4:q G4:q C5:q", &FOUR_FOUR, NULL);
    motive.key_source = MOTIVE_KEY_UNSPECIFIED;
    assert(motive_to_sketch(&motive, &sketch) && !sketch.has_scale && sketch.chord_count == 0);
}

static void test_rhythm_form(void) {
    MotiveSketch sketch;
    Motive rhythm = load(MOTIVE_FORMAT_RHYTHM, "x:q x:e x:e x:h", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch(&rhythm, &sketch));
    assert(sketch.note_count == 4 && sketch.chord_count == 0);
    for (size_t i = 0; i < sketch.note_count; i++) {
        assert(sketch.notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN);
    }
    assert(er_is(sketch.notes[3].onset, 1, 2) && er_is(sketch.notes[3].duration, 1, 2));
    Motive back;
    assert(motive_from_sketch(&sketch, &rhythm, &back));
    assert(back.form == MOTIVE_FORM_RHYTHM && motive_equal(&rhythm, &back));
    for (size_t i = 0; i < back.note_count; i++) {
        assert(back.notes[i].midi == rhythm.notes[0].midi);
    }

    /* A contour keeps the pitches that sound it. */
    Motive contour = load(MOTIVE_FORMAT_CONTOUR, "0 2 1 3", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch(&contour, &sketch));
    assert(sketch.notes[1].pitch == contour.notes[1].midi && sketch.notes[1].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN);
}

static void test_round_trips(void) {
    Motive plain = load(MOTIVE_FORMAT_NOTES, "C4:q E4:q G4:q C5:q", &FOUR_FOUR, &C_MAJOR);
    assert_round_trip(&plain);

    /* The chord the sketch was given is not written back as the motive's own. */
    MotiveSketch sketch;
    Motive back;
    assert(motive_to_sketch(&plain, &sketch) && sketch.chord_count == 1);
    assert(motive_from_sketch(&sketch, &plain, &back) && back.harmony_count == 0);

    Motive own = plain;
    add_harmony(&own, "I V6@2 I@3");
    add_feature(&own, "a:0-1");
    add_feature(&own, "tail:2-3");
    own.role = MOTIVE_ROLE_ACCOMPANIMENT;
    assert_round_trip(&own);

    /* Harmony of one chord at 0 that happens to be the implied one is still its own when the motive has it. */
    Motive one = plain;
    add_harmony(&one, "I");
    assert_round_trip(&one);

    Motive pickup = load(MOTIVE_FORMAT_NOTES, "G4:q C5:q. B4:e A4:q F4:h", &FOUR_FOUR, &C_MAJOR);
    pickup.pickup = (Rational){1, 4};
    assert_round_trip(&pickup);

    Motive minor = load(MOTIVE_FORMAT_NOTES, "D4:e F4:e A4:e D5:e E5:q", &SIX_EIGHT, &D_MINOR);
    assert_round_trip(&minor);
    Motive dotted = load(MOTIVE_FORMAT_NOTES, "r:q D4:q. F4:e A4:h", &(PlanMeter){3, 4, 0}, &D_MINOR);
    assert_round_trip(&dotted);

    Motive rhythm = load(MOTIVE_FORMAT_RHYTHM, "x:q x:e x:e x:h", &FOUR_FOUR, &C_MAJOR);
    assert_round_trip(&rhythm);
    Motive contour = load(MOTIVE_FORMAT_CONTOUR, "0 2 1 3", &FOUR_FOUR, &C_MAJOR);
    assert_round_trip(&contour);

    Motive keyless = load(MOTIVE_FORMAT_NOTES, "C4:q E4:q G4:q C5:q", &FOUR_FOUR, NULL);
    keyless.key_source = MOTIVE_KEY_UNSPECIFIED;
    assert_round_trip(&keyless);
}

static void test_a_relation_matches_the_registry(void) {
    Motive source = load(MOTIVE_FORMAT_NOTES, "C4:q E4:q G4:q C5:q", &FOUR_FOUR, &C_MAJOR);
    add_harmony(&source, "I V6@2");
    add_feature(&source, "a:0-1");

    MotiveSketch in;
    MotiveSketch out;
    assert(motive_to_sketch(&source, &in));
    MotiveRelationKind kind;
    assert(motive_relation_find("transpose", &kind));
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    step.value[1] = 2; /* tonal off, by 2 semitones */
    assert(motive_relation_apply(&in, &step, &out) == MOTIVE_RELATION_OK);
    Motive from_library;
    assert(motive_from_sketch(&out, &source, &from_library));

    MotiveDevSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_DEV_TRANSPOSE;
    spec.amount = 2;
    Motive from_composer;
    assert(motive_develop(&source, &spec, &from_composer) == MOTIVE_DEV_OK);
    assert(motive_equal(&from_library, &from_composer));
}

static void test_refusals(void) {
    MotiveSketch sketch;
    Motive motive = load(MOTIVE_FORMAT_NOTES, "C4:q E4:q", &FOUR_FOUR, &C_MAJOR);
    assert(!motive_to_sketch(NULL, &sketch) && !motive_to_sketch(&motive, NULL));
    assert(!motive_from_sketch(NULL, &motive, &motive) && !motive_from_sketch(&sketch, NULL, &motive));
    Motive back;
    assert(motive_to_sketch(&motive, &sketch));
    sketch.note_count = 0;
    assert(!motive_from_sketch(&sketch, &motive, &back));

    /* A meter the Composer does not have. */
    assert(motive_to_sketch(&motive, &sketch));
    sketch.bar = msk_make(7, 32);
    sketch.beat = msk_make(1, 32);
    assert(!motive_from_sketch(&sketch, &motive, &back));
}

static void test_result_mapping(void) {
    static const struct {
        MotiveRelationResult library;
        MotiveDevResult composer;
    } MAP[] = {{MOTIVE_RELATION_OK, MOTIVE_DEV_OK},
               {MOTIVE_RELATION_NOT_APPLICABLE, MOTIVE_DEV_ERR_NOT_APPLICABLE},
               {MOTIVE_RELATION_UNCHANGED, MOTIVE_DEV_ERR_UNCHANGED},
               {MOTIVE_RELATION_RANGE, MOTIVE_DEV_ERR_RANGE},
               {MOTIVE_RELATION_NOT_EXPRESSIBLE, MOTIVE_DEV_ERR_NOT_EXPRESSIBLE},
               {MOTIVE_RELATION_NO_SCALE, MOTIVE_DEV_ERR_NO_KEY},
               {MOTIVE_RELATION_NO_HARMONY, MOTIVE_DEV_ERR_NOT_APPLICABLE},
               {MOTIVE_RELATION_NO_FEATURE, MOTIVE_DEV_ERR_NOT_APPLICABLE},
               {MOTIVE_RELATION_INVALID, MOTIVE_DEV_ERR_INVALID}};
    for (size_t i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++) {
        assert(motive_sketch_result(MAP[i].library) == MAP[i].composer);
    }
}

static void test_chords_from_base_harmony(void) {
    Motive motive = load(MOTIVE_FORMAT_NOTES, "A5:q A5:q B5:e C6:e B5:h", &FOUR_FOUR, &C_MAJOR);
    add_harmony(&motive, "I");
    MotiveChord base[2];
    MotiveError error;
    size_t base_count = 0;
    assert(motive_harmony_parse("VI V@2", &FOUR_FOUR, base, &base_count, &error) == MOTIVE_OK && base_count == 2);

    /* The base harmony stands in for the motive's own harmony. */
    MotiveSketch sketch;
    assert(motive_to_sketch_with_chords(&motive, base, base_count, &sketch));
    assert(sketch.chord_count == 2 && sketch.chords[0].degree == 6 && er_is(sketch.chords[0].onset, 0, 1));
    assert(sketch.chords[1].degree == 5 && er_is(sketch.chords[1].onset, 1, 2));
    assert(sketch.note_count == 5 && sketch.has_scale);

    /* No chords: none are added, not even the implied one. */
    Motive plain = load(MOTIVE_FORMAT_NOTES, "A5:q A5:q B5:e C6:e B5:h", &FOUR_FOUR, &C_MAJOR);
    assert(motive_to_sketch_with_chords(&plain, NULL, 0, &sketch) && sketch.chord_count == 0);
    assert(!motive_to_sketch_with_chords(&plain, NULL, 1, &sketch));

    /* The labels are the library's against those chords: over VI the B passes up to the C. */
    MotiveToneLabel labels[MOTIVE_MAX_NOTES];
    assert(motive_tone_labels(&plain, base, 1, NULL, 0, labels) == MOTIVE_TONE_OK);
    assert(labels[0].role == MOTIVE_TONE_CHORD && labels[1].role == MOTIVE_TONE_CHORD);
    assert(labels[2].role == MOTIVE_TONE_NON_CHORD && labels[2].type == NON_CHORD_TONE_PASSING && labels[2].resolves_to == 3);
    assert(labels[3].role == MOTIVE_TONE_CHORD);
    assert(labels[4].role == MOTIVE_TONE_UNCLASSIFIED && labels[4].unresolved && labels[4].resolves_to == -1);

    /* An override is kept and marked. */
    MotiveToneOverride override = {2, MOTIVE_TONE_CHORD, NON_CHORD_TONE_PASSING};
    assert(motive_tone_labels(&plain, base, 1, &override, 1, labels) == MOTIVE_TONE_OK);
    assert(labels[2].role == MOTIVE_TONE_CHORD && labels[2].overridden && !labels[0].overridden);
    override.note = 5;
    assert(motive_tone_labels(&plain, base, 1, &override, 1, labels) == MOTIVE_TONE_BAD_OVERRIDE);

    /* Without a key or pitches there is nothing to label. */
    Motive rhythm = load(MOTIVE_FORMAT_RHYTHM, "x:q x:e x:e x:h", &FOUR_FOUR, &C_MAJOR);
    assert(motive_tone_labels(&rhythm, base, 1, NULL, 0, labels) == MOTIVE_TONE_NO_SCALE);
}

int main(void) {
    test_sketch_contents();
    test_implied_chord();
    test_rhythm_form();
    test_round_trips();
    test_a_relation_matches_the_registry();
    test_refusals();
    test_result_mapping();
    test_chords_from_base_harmony();
    puts("test_motive_sketch_bridge: ok");
    return 0;
}
