#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_tones.h>
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * The tones of a motive (motive_tones.h): its base harmony, the constraint the plan keeps, the limit on chords in a bar, and the names the
 * tone options use; and motive_chord_numeral.
 *
 *   harmony    one chord I proposed for "1:q 3:e 2:e 5:h" in C major 4/4, none for a rhythm motive, given once the motive has chords
 *   tones      the constraint (the chord and one label for each note) and its write on a node, which replaces its older
 *   limit      two chords in a bar are fine, three are not
 *   names      the types and the roles round-trip through their text
 *   numerals   the numeral of a chord
 */

static const PlanMeter FOUR_FOUR = {4, 4, 0};

static Motive degrees_motive(const char *text) {
    Motive motive;
    MotiveOptions options = *test_motive_options();
    MotiveError error;
    SpelledKey key = test_key(0, "major");
    options.has_meter = true;
    options.meter = FOUR_FOUR;
    options.has_key = true;
    options.key = key;
    assert(motive_from_degrees_text(text, &options, &motive, &error) == MOTIVE_OK);
    return motive;
}

static Motive rhythm_motive(void) {
    Motive motive;
    MotiveOptions options = *test_motive_options();
    MotiveError error;
    options.has_meter = true;
    options.meter = FOUR_FOUR;
    assert(motive_from_rhythm_text("x:q x:e x:e x:h", &options, &motive, &error) == MOTIVE_OK);
    return motive;
}

static Rational time_of(long numerator, unsigned long denominator) {
    Rational time;
    assert(rational_make(numerator, denominator, &time) == RATIONAL_OK);
    return time;
}

static MotiveSketchChord chord_at(Rational onset, int degree) {
    MotiveSketchChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.onset = onset;
    chord.degree = degree;
    return chord;
}

static void test_harmony(void) {
    Motive motive = degrees_motive("1:q 3:e 2:e 5:h");
    MotiveBaseHarmony harmony;
    memset(&harmony, 0, sizeof(harmony));
    /* No chords of its own: one chord proposed from the notes, the tonic from the bar's first beat. */
    assert(motive_base_harmony(&motive, &harmony));
    assert(harmony.count == 1 && !harmony.given);
    assert(harmony.chords[0].degree == 1 && harmony.chords[0].alteration == 0 && harmony.chords[0].inversion == 0 && !harmony.chords[0].seventh);
    assert(harmony.chords[0].onset.num == 0);
    /* The motive's own chords are the base harmony as they stand. */
    motive.chords[0] = chord_at(time_of(0, 1), 5);
    motive.chord_count = 1;
    assert(motive_base_harmony(&motive, &harmony));
    assert(harmony.count == 1 && harmony.given && harmony.chords[0].degree == 5);
    /* A rhythm motive sounds no known pitch, and NULL has no harmony: *out stays. */
    Motive rhythm = rhythm_motive();
    harmony.count = 7;
    assert(!motive_base_harmony(&rhythm, &harmony) && harmony.count == 7);
    assert(!motive_base_harmony(NULL, &harmony) && !motive_base_harmony(&motive, NULL));
}

static void test_tones(void) {
    Motive motive = degrees_motive("1:q 3:e 2:e 5:h");
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    assert(motive_tones_constraint(&motive, &constraint));
    const PlanMotiveTones *tones = &constraint.params.motive_tones;
    assert(constraint.kind == PLAN_CONSTRAINT_MOTIVE_TONES);
    /* One chord I over the whole motive. */
    assert(tones->chord_count == 1 && strcmp(tones->chords[0].numeral, "I") == 0);
    assert(tones->chords[0].onset.num == 0 && rational_cmp(tones->chords[0].length, time_of(1, 1)) == 0);
    /* One label in note order: C, E and G are chord tones; D, the note between E and G, is a non-chord tone and the role of motive_tone_roles names
       it: "escape", for D is reached by step from E and left by a leap to G (motive_tone_roles). */
    assert(tones->label_count == 4);
    assert(strcmp(tones->labels[0], "chord") == 0 && strcmp(tones->labels[1], "chord") == 0);
    assert(strcmp(tones->labels[2], "escape") == 0);
    assert(strcmp(tones->labels[3], "chord") == 0);
    /* A rhythm motive and NULL have none. */
    Motive rhythm = rhythm_motive();
    assert(!motive_tones_constraint(&rhythm, &constraint) && !motive_tones_constraint(NULL, &constraint) && !motive_tones_constraint(&motive, NULL));

    /* Written on a node, twice: the node holds one tones constraint. */
    Plan *plan = plan_create();
    NodeId form, section, phrase, idea;
    PlanSpan four = {time_of(0, 1), time_of(4, 1)};
    PlanSpan two = {time_of(0, 1), time_of(2, 1)};
    PlanSpan one = {time_of(0, 1), time_of(1, 1)};
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, four, "piece", &form) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, four, "A", &section) == PLAN_OK);
    assert(plan_add_node(plan, section, PLAN_NODE_PHRASE, two, NULL, &phrase) == PLAN_OK);
    assert(plan_add_node(plan, phrase, PLAN_NODE_IDEA, one, "basic idea", &idea) == PLAN_OK);
    assert(motive_node_write_tones(plan, idea, &motive) == PLAN_OK);
    assert(motive_node_write_tones(plan, idea, &motive) == PLAN_OK);
    size_t found = 0;
    for (size_t i = 0; i < plan_node_constraint_count(plan, idea); i++) {
        found += plan_node_constraint_at(plan, idea, i)->kind == PLAN_CONSTRAINT_MOTIVE_TONES;
    }
    assert(found == 1);
    /* A motive with no tones leaves the constraints as they are. */
    size_t before = plan_node_constraint_count(plan, idea);
    assert(motive_node_write_tones(plan, idea, &rhythm) == PLAN_OK && plan_node_constraint_count(plan, idea) == before);
    plan_destroy(plan);
}

static void test_limit(void) {
    Motive motive = degrees_motive("1:q 3:q 5:q 3:q 1:q 3:q 5:q 3:q");
    assert(motive_harmony_bar_limit_ok(&motive) && motive_harmony_bar_limit_ok(NULL));
    /* Two chords in a bar, and chords spread over the bars. */
    motive.chords[0] = chord_at(time_of(0, 1), 1);
    motive.chord_count = 1;
    assert(motive_harmony_bar_limit_ok(&motive));
    motive.chords[1] = chord_at(time_of(1, 2), 5);
    motive.chord_count = 2;
    assert(motive_harmony_bar_limit_ok(&motive));
    motive.chords[2] = chord_at(time_of(1, 1), 4);
    motive.chord_count = 3;
    assert(motive_harmony_bar_limit_ok(&motive));
    /* Three in one bar are one too many. */
    motive.chords[2] = chord_at(time_of(3, 4), 4);
    assert(!motive_harmony_bar_limit_ok(&motive));
}

static void test_names(void) {
    /* Every type's name parses back to it, and "escape-tone" is another way to write escape. */
    for (int type = 0; type < NON_CHORD_TONE_COUNT; type++) {
        const char *name = motive_tone_type_name((NonChordTone)type);
        NonChordTone parsed = NON_CHORD_TONE_COUNT;
        assert(name && motive_tone_type_parse(name, &parsed) && (int)parsed == type);
    }
    NonChordTone type = NON_CHORD_TONE_PASSING;
    assert(motive_tone_type_parse("escape-tone", &type) && type == NON_CHORD_TONE_ESCAPE_TONE);
    assert(motive_tone_type_name(NON_CHORD_TONE_COUNT) == NULL);
    type = NON_CHORD_TONE_PEDAL;
    assert(!motive_tone_type_parse("bogus", &type) && !motive_tone_type_parse("", &type) && !motive_tone_type_parse(NULL, &type));
    assert(!motive_tone_type_parse("passing", NULL) && type == NON_CHORD_TONE_PEDAL);
    /* A role is "chord" or a non-chord tone's name. */
    MotiveToneRole role = MOTIVE_TONE_NON_CHORD;
    assert(motive_tone_role_parse("chord", &role, &type) && role == MOTIVE_TONE_CHORD && type == NON_CHORD_TONE_PASSING);
    assert(motive_tone_role_parse("passing", &role, &type) && role == MOTIVE_TONE_NON_CHORD && type == NON_CHORD_TONE_PASSING);
    assert(motive_tone_role_parse("escape-tone", &role, &type) && role == MOTIVE_TONE_NON_CHORD && type == NON_CHORD_TONE_ESCAPE_TONE);
    assert(!motive_tone_role_parse("bogus", &role, &type));
    assert(!motive_tone_role_parse(NULL, &role, &type) && !motive_tone_role_parse("chord", NULL, &type) &&
           !motive_tone_role_parse("chord", &role, NULL));
}

static void test_numerals(void) {
    char text[MOTIVE_CHORD_NUMERAL_MAX];
    MotiveSketchChord chord = chord_at(time_of(0, 1), 1);
    assert(motive_chord_numeral(&chord, text, sizeof(text)) && strcmp(text, "I") == 0);
    /* The dominant seventh in first inversion, and a flat seventh degree. */
    chord = chord_at(time_of(0, 1), 5);
    chord.inversion = 1;
    chord.seventh = true;
    assert(motive_chord_numeral(&chord, text, sizeof(text)) && strcmp(text, "V65") == 0);
    chord = chord_at(time_of(0, 1), 7);
    chord.alteration = -1;
    assert(motive_chord_numeral(&chord, text, sizeof(text)) && strcmp(text, "bVII") == 0);
    /* What has no numeral: a degree outside 1 to 7, an alteration of 2, NULL arguments and a buffer too short. */
    chord = chord_at(time_of(0, 1), 0);
    assert(!motive_chord_numeral(&chord, text, sizeof(text)));
    chord.degree = 8;
    assert(!motive_chord_numeral(&chord, text, sizeof(text)));
    chord.degree = 1;
    chord.alteration = 2;
    assert(!motive_chord_numeral(&chord, text, sizeof(text)));
    chord.alteration = -2;
    assert(!motive_chord_numeral(&chord, text, sizeof(text)));
    chord.alteration = 0;
    assert(!motive_chord_numeral(NULL, text, sizeof(text)) && !motive_chord_numeral(&chord, NULL, sizeof(text)));
    chord.degree = 5;
    chord.inversion = 1;
    chord.seventh = true;
    assert(!motive_chord_numeral(&chord, text, 2));
}

int main(void) {
    test_harmony();
    test_tones();
    test_limit();
    test_names();
    test_numerals();
    printf("test_motive_tones: ok\n");
    return 0;
}
