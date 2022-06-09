#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scale_patterns.h"

#define I(n) (1u << (n))

static void test_valid_and_names(void) {
    for (int i = 0; i < SCALE_PATTERN_COUNT; i++) {
        assert(scale_pattern_valid((ScalePatternId)i));
        assert(scale_pattern((ScalePatternId)i) != NULL);
        assert(scale_pattern((ScalePatternId)i)->id == (ScalePatternId)i);
    }
    assert(!scale_pattern_valid(SCALE_PATTERN_COUNT));
    assert(!scale_pattern_valid((ScalePatternId)-1));
    assert(scale_pattern(SCALE_PATTERN_COUNT) == NULL);

    assert(strcmp(scale_pattern_name(SCALE_PATTERN_PARALLEL_CHORDS), "parallel-chords") == 0);
    assert(strcmp(scale_pattern_name(SCALE_PATTERN_FIVE_SIX_ASCENDING), "five-six-ascending") == 0);
    assert(strcmp(scale_pattern_name(SCALE_PATTERN_FIVE_SIX_DESCENDING), "five-six-descending") == 0);
    assert(strcmp(scale_pattern_name(SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING),
                  "five-six-descending-alternating") == 0);
    assert(strcmp(scale_pattern_name(SCALE_PATTERN_SEVEN_SIX_DESCENDING), "seven-six-descending") == 0);
    assert(scale_pattern_name(SCALE_PATTERN_COUNT) == NULL);
}

static void test_parallel_chords(void) {
    const ScalePattern *p = scale_pattern(SCALE_PATTERN_PARALLEL_CHORDS);
    assert(p->kind == SEQUENCE_KIND_PARALLEL_CHORDS);
    assert(p->both_directions);
    assert(p->unit_length == 1);
    assert(p->unit[0].bass_moves && !p->unit[0].suspended);
    assert(p->unit[0].intervals == (I(3) | I(6) | I(8)));
}

static void test_five_six(void) {
    const ScalePattern *asc = scale_pattern(SCALE_PATTERN_FIVE_SIX_ASCENDING);
    const ScalePattern *desc = scale_pattern(SCALE_PATTERN_FIVE_SIX_DESCENDING);
    const ScalePattern *alt = scale_pattern(SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING);
    assert(asc->kind == SEQUENCE_KIND_FIVE_SIX && desc->kind == SEQUENCE_KIND_FIVE_SIX &&
           alt->kind == SEQUENCE_KIND_FIVE_SIX);
    assert(asc->direction == SCALE_DIRECTION_ASCENDING && !asc->both_directions);
    assert(desc->direction == SCALE_DIRECTION_DESCENDING && !desc->both_directions);
    assert(alt->direction == SCALE_DIRECTION_DESCENDING && !alt->both_directions);

    /* Note by note: a fifth and then a sixth over each bass note. */
    for (const ScalePattern *p = asc; p != NULL; p = (p == asc ? desc : NULL)) {
        assert(p->unit_length == 2);
        assert(p->unit[0].bass_moves && p->unit[0].intervals == I(5));
        assert(!p->unit[1].bass_moves && p->unit[1].intervals == I(6));
        assert(!p->unit[0].suspended && !p->unit[1].suspended);
    }
    /* Alternating: the fifth on one bass note, the sixth on the next. */
    assert(alt->unit_length == 2);
    assert(alt->unit[0].bass_moves && alt->unit[0].intervals == I(5));
    assert(alt->unit[1].bass_moves && alt->unit[1].intervals == I(6));
}

static void test_seven_six(void) {
    const ScalePattern *p = scale_pattern(SCALE_PATTERN_SEVEN_SIX_DESCENDING);
    assert(p->kind == SEQUENCE_KIND_SEVEN_SIX);
    assert(p->direction == SCALE_DIRECTION_DESCENDING && !p->both_directions);
    assert(p->unit_length == 2);
    /* A new bass note and a held upper voice, a seventh; then the bass stays and
       the upper voice resolves to a sixth. */
    assert(p->unit[0].bass_moves && p->unit[0].suspended && p->unit[0].intervals == I(7));
    assert(!p->unit[1].bass_moves && !p->unit[1].suspended && p->unit[1].intervals == I(6));
    assert(sequence_kind_is_suspension_chain(p->kind));
}

static void test_allows_direction(void) {
    const ScalePattern *parallel = scale_pattern(SCALE_PATTERN_PARALLEL_CHORDS);
    assert(scale_pattern_allows_direction(parallel, SCALE_DIRECTION_ASCENDING));
    assert(scale_pattern_allows_direction(parallel, SCALE_DIRECTION_DESCENDING));

    const ScalePattern *asc = scale_pattern(SCALE_PATTERN_FIVE_SIX_ASCENDING);
    assert(scale_pattern_allows_direction(asc, SCALE_DIRECTION_ASCENDING));
    assert(!scale_pattern_allows_direction(asc, SCALE_DIRECTION_DESCENDING));

    const ScalePattern *seven_six = scale_pattern(SCALE_PATTERN_SEVEN_SIX_DESCENDING);
    assert(!scale_pattern_allows_direction(seven_six, SCALE_DIRECTION_ASCENDING));
    assert(scale_pattern_allows_direction(seven_six, SCALE_DIRECTION_DESCENDING));

    assert(!scale_pattern_allows_direction(NULL, SCALE_DIRECTION_ASCENDING));
    assert(!scale_pattern_allows_direction(asc, SCALE_DIRECTION_COUNT));
}

static void test_every_pattern_is_well_formed(void) {
    for (int i = 0; i < SCALE_PATTERN_COUNT; i++) {
        const ScalePattern *p = scale_pattern((ScalePatternId)i);
        assert(sequence_kind_valid(p->kind));
        assert(scale_direction_valid(p->direction));
        assert(p->unit_length >= 1 && p->unit_length <= SCALE_MAX_UNIT);
        /* The bass moves at some stage of every unit, or the pattern is not a scale. */
        bool moves = false;
        for (size_t s = 0; s < p->unit_length; s++) {
            moves = moves || p->unit[s].bass_moves;
            assert(p->unit[s].intervals != 0);
            assert((p->unit[s].intervals & ~0x1FEu) == 0);
        }
        assert(moves);
    }
}

int main(void) {
    test_valid_and_names();
    test_parallel_chords();
    test_five_six();
    test_seven_six();
    test_allows_direction();
    test_every_pattern_is_well_formed();
    printf("test_scale_patterns: ok\n");
    return 0;
}
