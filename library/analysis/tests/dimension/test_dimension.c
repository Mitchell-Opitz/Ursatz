#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/dimension/dimension.h"
#include "ursatz_test_knowledge.h"

/*
 * The dimension measures and the contrast vector (R-709, R-730): the scales of the modes measure their brightness rank; C4 D4 E4 quarters in a 3-beat span of 3/4 have register 62 and density 1;
 * the dotted rhythm q. e h against q. e q differs by 1/2 in rhythm (the duration ratios 1/3 and 4 against 1/3 and 2) and the notes C D E
 * against C D C by 1/2 in contour (up up against up down); a span against itself is 0 in every aspect; the refusals.
 */

static Rational q(long long num, long long den) {
    return rational_ratio(num, den);
}

static bool is(Rational value, long long num, long long den) {
    return value.num == num && value.den == den;
}

static DimensionNote note(long long onset_num, long long onset_den, long long length_num, long long length_den, int pitch) {
    DimensionNote result = {q(onset_num, onset_den), q(length_num, length_den), pitch};
    return result;
}

static void test_measures(void) {
    const DimensionNote notes[3] = {note(0, 1, 1, 4, 60), note(1, 4, 1, 4, 62), note(1, 2, 1, 4, 64)};
    DimensionSpan span = {notes, 3, q(3, 4), q(1, 4)};
    Rational value = q(7, 1);
    assert(dimension_measure(DIMENSION_REGISTER, &span, &value) == DIMENSION_OK && is(value, 62, 1));
    assert(dimension_measure(DIMENSION_DENSITY, &span, &value) == DIMENSION_OK && is(value, 1, 1));
    /* a dotted-quarter beat of 3/8 (6/8): three notes over two beats */
    DimensionSpan compound = {notes, 3, q(3, 4), q(3, 8)};
    assert(dimension_measure(DIMENSION_DENSITY, &compound, &value) == DIMENSION_OK && is(value, 3, 2));
    assert(strcmp(dimension_name(DIMENSION_REGISTER), "register") == 0);
    assert(strcmp(dimension_name(DIMENSION_DENSITY), "density") == 0);
    assert(strcmp(dimension_name(DIMENSION_BRIGHTNESS), "brightness") == 0);
    assert(dimension_name(DIMENSION_COUNT) == NULL && dimension_name((Dimension)-1) == NULL);
}

/* The notes of a scale on C, one quarter each, and its brightness (R-730) against the center C. */
static void scale_brightness(const char *word, const int *pitches, size_t count, long long expected) {
    DimensionNote notes[8];
    for (size_t i = 0; i < count; i++) {
        notes[i] = note((long long)i, 4, 1, 4, pitches[i]);
    }
    DimensionSpan span = {notes, count, q((long long)count, 4), q(1, 4), true, 0, knowledge_snapshot_material_named(test_knowledge(), word)};
    Rational value = q(0, 1);
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &span, &value) == DIMENSION_OK && is(value, expected, 1));
}

static void test_brightness(void) {
    static const int LOCRIAN[] = {60, 61, 63, 65, 66, 68, 70};
    static const int LYDIAN[] = {60, 62, 64, 66, 67, 69, 71};
    static const int DORIAN[] = {60, 62, 63, 65, 67, 69, 70};
    static const int MAJOR[] = {60, 62, 64, 65, 67, 69, 71};
    static const int NATURAL_MINOR[] = {60, 62, 63, 65, 67, 68, 70};
    scale_brightness("locrian", LOCRIAN, 7, 1);
    scale_brightness("lydian", LYDIAN, 7, 7);
    scale_brightness("dorian", DORIAN, 7, 4);
    scale_brightness("major", MAJOR, 7, 6);
    scale_brightness("minor", NATURAL_MINOR, 7, 3);
    /* A span holding only C, Eb and G: the third lowered, the fifth raised, the rest not sounding, so the material answers for them. */
    const DimensionNote triad[3] = {note(0, 1, 1, 4, 60), note(1, 4, 1, 4, 63), note(1, 2, 1, 4, 67)};
    DimensionSpan span = {triad, 3, q(3, 4), q(1, 4), true, 0, knowledge_snapshot_material_named(test_knowledge(), "dorian")};
    Rational value = q(0, 1);
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &span, &value) == DIMENSION_OK && is(value, 4, 1)); /* 1 + 1 + 0 + 0 + 1 + 1 + 0 */
    span.material = knowledge_snapshot_material_named(test_knowledge(), "phrygian");
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &span, &value) == DIMENSION_OK && is(value, 2, 1)); /* 1 + 0 + 0 + 0 + 1 + 0 + 0 */
    span.material = NULL;
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &span, &value) == DIMENSION_OK && is(value, 4, 1)); /* 1 + 1/2 + 0 + 1/2 + 1 + 1/2 + 1/2 */
    /* A degree sounding in both forms is the share of its duration in the raised one. */
    const DimensionNote both[2] = {note(0, 1, 1, 4, 64), note(1, 4, 3, 4, 63)};
    DimensionSpan third = {both, 2, q(1, 1), q(1, 4), true, 0, NULL};
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &third, &value) == DIMENSION_OK && is(value, 15, 4)); /* 1 + 1/2 + 1/4 + 1/2 + 1/2 + 1/2 + 1/2 */
    /* A span with no center is refused. */
    span.has_center = false;
    assert(dimension_measure(DIMENSION_BRIGHTNESS, &span, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(strcmp(dimension_name(DIMENSION_BRIGHTNESS), "brightness") == 0);
}

/*
 * Tension (R-764): a block I-IV-V-I (a chord a bar, root position) measures 1, a bar of V7 3, a bar half C E G and half B D F 13/2; a
 * sonority of two pitch classes does not count, a melody alone is refused, and an empty span is refused.
 */
static void test_tension(void) {
    Rational value = q(0, 1);
    static const int PROGRESSION[4][3] = {{60, 64, 67}, {65, 69, 72}, {67, 71, 74}, {60, 64, 67}};
    DimensionNote block[12];
    for (size_t bar = 0; bar < 4; bar++) {
        for (size_t tone = 0; tone < 3; tone++) {
            block[bar * 3 + tone] = note((long long)bar, 1, 1, 1, PROGRESSION[bar][tone]);
        }
    }
    DimensionSpan span = {block, 12, q(4, 1), q(1, 4)};
    assert(strcmp(dimension_name(DIMENSION_TENSION), "tension") == 0);
    assert(dimension_measure(DIMENSION_TENSION, &span, &value) == DIMENSION_OK);
    assert(is(value, 1, 1));
    const DimensionNote dominant[4] = {note(0, 1, 1, 1, 55), note(0, 1, 1, 1, 59), note(0, 1, 1, 1, 62), note(0, 1, 1, 1, 65)};
    DimensionSpan seventh = {dominant, 4, q(1, 1), q(1, 4)};
    assert(dimension_measure(DIMENSION_TENSION, &seventh, &value) == DIMENSION_OK && is(value, 3, 1));
    const DimensionNote halves[6] = {note(0, 1, 1, 2, 60), note(0, 1, 1, 2, 64), note(0, 1, 1, 2, 67),
                                     note(1, 2, 1, 2, 59), note(1, 2, 1, 2, 62), note(1, 2, 1, 2, 65)};
    DimensionSpan mixed = {halves, 6, q(1, 1), q(1, 4)};
    assert(dimension_measure(DIMENSION_TENSION, &mixed, &value) == DIMENSION_OK && is(value, 13, 2));
    /* a note that starts later cuts the sonority: a quarter of two tones, then three tones for the rest of the bar */
    const DimensionNote late[3] = {note(0, 1, 1, 1, 60), note(0, 1, 1, 1, 67), note(1, 4, 3, 4, 64)};
    DimensionSpan cut = {late, 3, q(1, 1), q(1, 4)};
    assert(dimension_measure(DIMENSION_TENSION, &cut, &value) == DIMENSION_OK && is(value, 1, 1));
    const DimensionNote melody[3] = {note(0, 1, 1, 4, 60), note(1, 4, 1, 4, 62), note(1, 2, 1, 4, 64)};
    DimensionSpan alone = {melody, 3, q(3, 4), q(1, 4)};
    assert(dimension_measure(DIMENSION_TENSION, &alone, &value) == DIMENSION_ERR_INVALID_INPUT);
    DimensionSpan empty = {NULL, 0, q(1, 1), q(1, 4)};
    assert(dimension_measure(DIMENSION_TENSION, &empty, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(is(value, 1, 1)); /* untouched by the refusals */
}

static void test_contrast(void) {
    const DimensionNote a[3] = {note(0, 1, 3, 8, 60), note(3, 8, 1, 8, 62), note(1, 2, 1, 2, 64)};
    const DimensionNote b[3] = {note(0, 1, 3, 8, 60), note(3, 8, 1, 8, 62), note(1, 2, 1, 4, 60)};
    DimensionSpan first = {a, 3, q(1, 1), q(1, 4)};
    DimensionSpan second = {b, 3, q(3, 4), q(1, 4)};
    ContrastVector vector;
    assert(dimension_contrast(&first, &second, &vector) == DIMENSION_OK);
    assert(is(vector.distance[CONTRAST_ASPECT_RHYTHM], 1, 2));
    assert(is(vector.distance[CONTRAST_ASPECT_CONTOUR], 1, 2));
    assert(is(vector.distance[CONTRAST_ASPECT_REGISTER], 4, 3)); /* 62 against 182/3 */
    assert(is(vector.distance[CONTRAST_ASPECT_DENSITY], 1, 4));  /* 3/4 against 1 note per beat */
    /* symmetric */
    ContrastVector back;
    assert(dimension_contrast(&second, &first, &back) == DIMENSION_OK);
    for (int i = 0; i < CONTRAST_ASPECT_COUNT; i++) {
        assert(is(back.distance[i], vector.distance[i].num, vector.distance[i].den));
    }
    /* a span against itself is alike in every aspect */
    assert(dimension_contrast(&first, &first, &vector) == DIMENSION_OK);
    for (int i = 0; i < CONTRAST_ASPECT_COUNT; i++) {
        assert(is(vector.distance[i], 0, 1));
    }
    /* a span of one note: alike against another one note, wholly unlike a longer span in contour, and in rhythm against a different count */
    const DimensionNote single[1] = {note(0, 1, 1, 4, 67)};
    DimensionSpan one = {single, 1, q(1, 4), q(1, 4)};
    assert(dimension_contrast(&one, &one, &vector) == DIMENSION_OK && is(vector.distance[CONTRAST_ASPECT_CONTOUR], 0, 1));
    assert(dimension_contrast(&one, &first, &vector) == DIMENSION_OK);
    assert(is(vector.distance[CONTRAST_ASPECT_CONTOUR], 1, 1) && is(vector.distance[CONTRAST_ASPECT_RHYTHM], 1, 1));
}

static void test_refusals(void) {
    const DimensionNote notes[1] = {note(0, 1, 1, 4, 60)};
    DimensionSpan good = {notes, 1, q(1, 4), q(1, 4)};
    DimensionSpan empty = {notes, 0, q(1, 4), q(1, 4)};
    DimensionSpan no_length = {notes, 1, q(0, 1), q(1, 4)};
    DimensionSpan no_beat = {notes, 1, q(1, 4), q(0, 1)};
    DimensionSpan no_notes = {NULL, 1, q(1, 4), q(1, 4)};
    const DimensionNote zero_note[1] = {note(0, 1, 0, 1, 60)};
    DimensionSpan zero_duration = {zero_note, 1, q(1, 4), q(1, 4)};
    Rational value = q(9, 1);
    ContrastVector vector;
    memset(&vector, 0, sizeof(vector));
    assert(dimension_measure(DIMENSION_REGISTER, &empty, &value) == DIMENSION_ERR_INVALID_INPUT && is(value, 9, 1));
    assert(dimension_measure(DIMENSION_DENSITY, &no_length, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_measure(DIMENSION_DENSITY, &no_beat, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_measure(DIMENSION_REGISTER, &no_notes, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_measure(DIMENSION_REGISTER, &zero_duration, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_measure(DIMENSION_COUNT, &good, &value) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_measure(DIMENSION_REGISTER, &good, NULL) == DIMENSION_ERR_NULL_ARGUMENT);
    assert(dimension_measure(DIMENSION_REGISTER, NULL, &value) == DIMENSION_ERR_NULL_ARGUMENT);
    assert(dimension_contrast(&good, &empty, &vector) == DIMENSION_ERR_INVALID_INPUT && is(vector.distance[0], 0, 0));
    assert(dimension_contrast(&empty, &good, &vector) == DIMENSION_ERR_INVALID_INPUT);
    assert(dimension_contrast(&good, &good, NULL) == DIMENSION_ERR_NULL_ARGUMENT);
    assert(dimension_contrast(NULL, &good, &vector) == DIMENSION_ERR_NULL_ARGUMENT);
}

int main(void) {
    test_measures();
    test_brightness();
    test_tension();
    test_contrast();
    test_refusals();
    printf("test_dimension: ok\n");
    return 0;
}
