#include "pitch/accidental.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_invalid_kind(void) {
    Accidental *accidental = NULL;
    assert(accidental_create((AccidentalKind)99, &accidental) == ACCIDENTAL_ERR_INVALID_KIND);
    assert(accidental == NULL);
}

static void test_offsets(void) {
    AccidentalKind kinds[] = {ACCIDENTAL_DOUBLE_FLAT, ACCIDENTAL_FLAT, ACCIDENTAL_NATURAL, ACCIDENTAL_SHARP,
                               ACCIDENTAL_DOUBLE_SHARP};
    int offsets[] = {-2, -1, 0, 1, 2};

    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        Accidental *accidental = NULL;
        assert(accidental_create(kinds[i], &accidental) == ACCIDENTAL_OK);
        assert(accidental_kind(accidental) == kinds[i]);
        assert(accidental_semitone_offset(accidental) == offsets[i]);
        accidental_destroy(accidental);
    }
}

/* Property: kind -> offset -> kind round-trips for every offset in range. */
static void test_from_offset_round_trips(void) {
    for (int offset = ACCIDENTAL_MIN_OFFSET; offset <= ACCIDENTAL_MAX_OFFSET; offset++) {
        Accidental *accidental = NULL;
        assert(accidental_from_offset(offset, &accidental) == ACCIDENTAL_OK);
        assert(accidental_semitone_offset(accidental) == offset);

        Accidental *again = NULL;
        assert(accidental_create(accidental_kind(accidental), &again) == ACCIDENTAL_OK);
        assert(accidental_equals(accidental, again));

        accidental_destroy(accidental);
        accidental_destroy(again);
    }
}

static void test_from_offset_rejects_out_of_range(void) {
    Accidental *accidental = NULL;
    assert(accidental_from_offset(3, &accidental) == ACCIDENTAL_ERR_INVALID_OFFSET);
    assert(accidental_from_offset(-3, &accidental) == ACCIDENTAL_ERR_INVALID_OFFSET);
    assert(accidental == NULL);
}

/* A natural has offset 0 but is still a different accidental from none --
   and from every other kind. */
static void test_equality(void) {
    Accidental *sharp = NULL, *sharp_again = NULL, *flat = NULL, *natural = NULL;
    assert(accidental_create(ACCIDENTAL_SHARP, &sharp) == ACCIDENTAL_OK);
    assert(accidental_create(ACCIDENTAL_SHARP, &sharp_again) == ACCIDENTAL_OK);
    assert(accidental_create(ACCIDENTAL_FLAT, &flat) == ACCIDENTAL_OK);
    assert(accidental_create(ACCIDENTAL_NATURAL, &natural) == ACCIDENTAL_OK);

    assert(accidental_equals(sharp, sharp_again));
    assert(!accidental_equals(sharp, flat));
    assert(!accidental_equals(natural, sharp));
    assert(!accidental_equals(sharp, NULL));

    accidental_destroy(sharp);
    accidental_destroy(sharp_again);
    accidental_destroy(flat);
    accidental_destroy(natural);
}

int main(void) {
    test_create_rejects_invalid_kind();
    test_offsets();
    test_from_offset_round_trips();
    test_from_offset_rejects_out_of_range();
    test_equality();
    return 0;
}
