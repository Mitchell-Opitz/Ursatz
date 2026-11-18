#include "model/time/meter_arithmetic.h"

#include <assert.h>
#include <stdio.h>

/*
 * The local rhythmic grid (R-588): meter_map_grid_at gives the beat and the division of the measure that holds a time
 * (half the beat in a simple meter, a third in a compound one); meter_grid_holds says whether a length is a whole number
 * of one of the grid's levels no shorter than the finest.
 */

static Rational q(long long n, long long d) {
    Rational r;
    assert(rational_make(n, d, &r) == RATIONAL_OK);
    return r;
}

static MusicalTime at(long long n, long long d) {
    MusicalTime t;
    assert(musical_time_make(n, d, &t) == MUSICAL_TIME_OK);
    return t;
}

typedef struct {
    unsigned top;
    int bottom_exponent;
    long long start_num;
    long long start_den;
} Change;

static MeterMap *build(const Change *changes, size_t count) {
    TimeSignature *signatures[4];
    MeterMapChangePoint points[4];
    for (size_t i = 0; i < count; i++) {
        NoteValue *unit = NULL;
        assert(note_value_create(changes[i].bottom_exponent, &unit) == NOTE_VALUE_OK);
        assert(time_signature_create(changes[i].top, unit, &signatures[i]) == TIME_SIGNATURE_OK);
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = at(changes[i].start_num, changes[i].start_den);
    }
    EntityID *id = NULL;
    assert(entity_id_create("grid-map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    return map;
}

static MeterGrid grid_at(const MeterMap *map, MusicalTime t) {
    MeterGrid grid;
    assert(meter_map_grid_at(map, t, &grid) == METER_MAP_OK);
    return grid;
}

static void assert_levels(MeterGrid grid, Rational start, Rational beat, Rational division) {
    assert(grid.signature != NULL);
    assert(rational_equals(grid.measure_start, start) && rational_equals(grid.beat, beat) &&
           rational_equals(grid.division, division));
}

static bool holds(const MeterGrid *grid, long long num, long long den, long long finest_den) {
    return meter_grid_holds(grid, q(num, den), q(1, finest_den));
}

static void test_levels(void) {
    Change four[] = {{4, 2, 0, 1}};
    MeterMap *map = build(four, 1);
    assert_levels(grid_at(map, at(0, 1)), q(0, 1), q(1, 4), q(1, 8));
    assert_levels(grid_at(map, at(5, 8)), q(0, 1), q(1, 4), q(1, 8));
    assert_levels(grid_at(map, at(1, 1)), q(1, 1), q(1, 4), q(1, 8)); /* a barline is in the measure it starts */
    meter_map_destroy(map);

    Change six[] = {{6, 3, 0, 1}};
    map = build(six, 1);
    assert_levels(grid_at(map, at(1, 2)), q(0, 1), q(3, 8), q(1, 8)); /* compound: a third of the beat */
    meter_map_destroy(map);

    Change five[] = {{5, 3, 0, 1}};
    map = build(five, 1);
    assert_levels(grid_at(map, at(1, 4)), q(0, 1), q(1, 8), q(1, 16));
    meter_map_destroy(map);

    /* the grid follows the meter change */
    Change change[] = {{4, 2, 0, 1}, {6, 3, 1, 1}};
    map = build(change, 2);
    assert_levels(grid_at(map, at(7, 8)), q(0, 1), q(1, 4), q(1, 8));
    assert_levels(grid_at(map, at(5, 4)), q(1, 1), q(3, 8), q(1, 8));
    MeterGrid grid;
    assert(meter_map_grid_at(map, at(-1, 8), &grid) == METER_MAP_ERR_TIME_BEFORE_START);
    assert(meter_map_grid_at(NULL, at(0, 1), &grid) == METER_MAP_ERR_NULL_ARGUMENT);
    assert(meter_map_grid_at(map, at(0, 1), NULL) == METER_MAP_ERR_NULL_ARGUMENT);
    meter_map_destroy(map);
}

static void test_holds_in_four_four(void) {
    Change four[] = {{4, 2, 0, 1}};
    MeterMap *map = build(four, 1);
    MeterGrid grid = grid_at(map, at(0, 1));
    /* with the finest level 1/32 */
    assert(holds(&grid, 1, 4, 32));  /* the beat */
    assert(holds(&grid, 1, 2, 32));  /* two beats */
    assert(holds(&grid, 1, 8, 32));  /* the division */
    assert(holds(&grid, 3, 16, 32)); /* three halvings of the division (1/16) */
    assert(holds(&grid, 1, 32, 32));
    assert(holds(&grid, 1, 12, 32)); /* the triplet eighth: the 3:2 tuplet of the division */
    assert(holds(&grid, 1, 6, 32));  /* the triplet quarter: the 3:2 tuplet of the beat */
    assert(!holds(&grid, 1, 24, 32));
    assert(!holds(&grid, 1, 48, 32));
    assert(!holds(&grid, 1, 64, 32));
    assert(holds(&grid, 1, 3, 32));  /* two triplet quarters */
    assert(!holds(&grid, 1, 5, 32)); /* not a whole number of any level */
    assert(!holds(&grid, 0, 1, 32) && !holds(&grid, -1, 4, 32)); /* not positive */
    /* with the finest level 1/64 */
    assert(holds(&grid, 1, 64, 64));
    assert(holds(&grid, 3, 64, 64));
    assert(!holds(&grid, 1, 48, 64));
    assert(!holds(&grid, 1, 128, 64));
    /* with the finest level 1/8 the sixteenth is below the grid */
    assert(!holds(&grid, 1, 16, 8) && !holds(&grid, 1, 12, 8) && holds(&grid, 1, 8, 8) && holds(&grid, 1, 4, 8));
    assert(!meter_grid_holds(NULL, q(1, 4), q(1, 32)));
    meter_map_destroy(map);
}

static void test_holds_in_six_eight(void) {
    Change six[] = {{6, 3, 0, 1}};
    MeterMap *map = build(six, 1);
    MeterGrid grid = grid_at(map, at(0, 1));
    assert(holds(&grid, 3, 8, 32));  /* the beat */
    assert(holds(&grid, 3, 4, 32));  /* two beats */
    assert(holds(&grid, 1, 8, 32));  /* the division */
    assert(holds(&grid, 1, 16, 32));
    assert(holds(&grid, 1, 32, 32));
    assert(holds(&grid, 3, 16, 32)); /* the duplet: the 2:3 of the beat, a half beat */
    assert(!holds(&grid, 1, 12, 32));
    assert(!holds(&grid, 1, 24, 32));
    assert(!holds(&grid, 1, 48, 32));
    assert(!holds(&grid, 1, 6, 32));
    assert(!holds(&grid, 1, 64, 32));
    meter_map_destroy(map);
}

static void test_holds_in_five_eight(void) {
    Change five[] = {{5, 3, 0, 1}};
    MeterMap *map = build(five, 1);
    MeterGrid grid = grid_at(map, at(0, 1));
    assert(holds(&grid, 1, 8, 32));
    assert(holds(&grid, 1, 16, 32));
    assert(holds(&grid, 1, 12, 32)); /* the 3:2 tuplet of the beat */
    assert(holds(&grid, 1, 24, 32)); /* the 3:2 tuplet of the division */
    assert(holds(&grid, 1, 6, 32));
    assert(!holds(&grid, 1, 48, 32));
    assert(!holds(&grid, 1, 64, 32));
    assert(holds(&grid, 1, 64, 64));
    meter_map_destroy(map);
}

int main(void) {
    test_levels();
    test_holds_in_four_four();
    test_holds_in_six_eight();
    test_holds_in_five_eight();
    printf("test_meter_grid: ok\n");
    return 0;
}
