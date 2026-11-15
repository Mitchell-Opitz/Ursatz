#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <analysis/motive_sketch.h>

#include <ursatz_composer/motive.h>

#include "motive/motive_internal.h"

/*
 * The bar, the offset in the bar and the beat of a time, from the library's motive sketch (motive_sketch_bar_of,
 * motive_sketch_offset_in_bar, motive_sketch_on_beat), against the results the helpers motive_tones.c had before B6a gave
 * (recorded in EXPECTED when those helpers were compared with the library, with no difference; they are deleted).
 *
 * Declared difference: the old offset of a time before the first barline in a sketch with no pickup was a bar plus the
 * time; the library says bad. No motive has that time (a motive without a pickup starts on its barline), so the table
 * records "bad".
 */

#define FIXTURES 8
#define TIMES 33

typedef struct {
    const char *name;
    PlanMeter meter;
    long pickup_num;
    long pickup_den;
} Fixture;

static const Fixture FIXTURE[FIXTURES] = {
    {"4/4", {4, 4, 0}, 0, 1},   {"4/4 pickup 1/4", {4, 4, 0}, 1, 4}, {"3/4", {3, 4, 0}, 0, 1},
    {"3/4 pickup 1/8", {3, 4, 0}, 1, 8}, {"6/8", {6, 8, 0}, 0, 1},   {"6/8 pickup 1/8", {6, 8, 0}, 1, 8},
    {"2/2 pickup 1/2", {2, 2, 0}, 1, 2}, {"5/4 pickup 3/16", {5, 4, 0}, 3, 16},
};

/* The times asked: k/16 whole notes, k = 0 to 32. */
static MotiveSketch make_sketch(const Fixture *fixture) {
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    sketch.note_count = 1;
    sketch.notes[0].pitch = 60;
    sketch.notes[0].onset = msk_int(0);
    sketch.notes[0].duration = msk_make(1, 4);
    assert(motive_set_meter(&sketch, fixture->meter));
    sketch.pickup = msk_make((unsigned long long)fixture->pickup_num, (unsigned long long)fixture->pickup_den);
    sketch.length = msk_make(1, 4);
    assert(motive_sketch_validate(&sketch));
    return sketch;
}

/* "bar:offset:B" for the library, B when the time is on a beat, b when not; offset is "bad" when bad. */
static void describe(const MotiveSketch *sketch, Rational time, char *out, size_t size) {
    long bar = motive_sketch_bar_of(sketch, time);
    Rational offset = motive_sketch_offset_in_bar(sketch, bar, time);
    bool on_beat = motive_sketch_on_beat(sketch, time);
    if (msk_ok(offset)) {
        snprintf(out, size, "%ld:%llu/%llu:%c", bar, (unsigned long long)offset.num, (unsigned long long)offset.den,
                 on_beat ? 'B' : 'b');
    } else {
        snprintf(out, size, "%ld:bad:%c", bar, on_beat ? 'B' : 'b');
    }
}

static const char *const EXPECTED[FIXTURES] = {
    "0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 0:3/4:B 0:13/16:b 0:7/8:b 0:15/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:b 1:11/16:b 1:3/4:B 1:13/16:b 1:7/8:b 1:15/16:b 2:0/1:B",
    "-1:3/4:B -1:13/16:b -1:7/8:b -1:15/16:b 0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 0:3/4:B 0:13/16:b 0:7/8:b 0:15/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:b 1:11/16:b 1:3/4:B",
    "0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:b 1:11/16:b 2:0/1:B 2:1/16:b 2:1/8:b 2:3/16:b 2:1/4:B 2:5/16:b 2:3/8:b 2:7/16:b 2:1/2:B",
    "-1:5/8:b -1:11/16:b 0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:b 1:11/16:b 2:0/1:B 2:1/16:b 2:1/8:b 2:3/16:b 2:1/4:B 2:5/16:b 2:3/8:b",
    "0:0/1:B 0:1/16:b 0:1/8:B 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:B 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:B 0:11/16:b 1:0/1:B 1:1/16:b 1:1/8:B 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:B 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:B 1:11/16:b 2:0/1:B 2:1/16:b 2:1/8:B 2:3/16:b 2:1/4:B 2:5/16:b 2:3/8:B 2:7/16:b 2:1/2:B",
    "-1:5/8:B -1:11/16:b 0:0/1:B 0:1/16:b 0:1/8:B 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:B 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:B 0:11/16:b 1:0/1:B 1:1/16:b 1:1/8:B 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:B 1:7/16:b 1:1/2:B 1:9/16:b 1:5/8:B 1:11/16:b 2:0/1:B 2:1/16:b 2:1/8:B 2:3/16:b 2:1/4:B 2:5/16:b 2:3/8:B",
    "-1:1/2:B -1:9/16:b -1:5/8:b -1:11/16:b -1:3/4:b -1:13/16:b -1:7/8:b -1:15/16:b 0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:b 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 0:3/4:b 0:13/16:b 0:7/8:b 0:15/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:b 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B",
    "-1:17/16:b -1:9/8:b -1:19/16:b 0:0/1:B 0:1/16:b 0:1/8:b 0:3/16:b 0:1/4:B 0:5/16:b 0:3/8:b 0:7/16:b 0:1/2:B 0:9/16:b 0:5/8:b 0:11/16:b 0:3/4:B 0:13/16:b 0:7/8:b 0:15/16:b 0:1/1:B 0:17/16:b 0:9/8:b 0:19/16:b 1:0/1:B 1:1/16:b 1:1/8:b 1:3/16:b 1:1/4:B 1:5/16:b 1:3/8:b 1:7/16:b 1:1/2:B 1:9/16:b",
};

static void row(const Fixture *fixture, char *out, size_t size) {
    MotiveSketch sketch = make_sketch(fixture);
    out[0] = '\0';
    for (int k = 0; k < TIMES; k++) {
        char cell[64];
        describe(&sketch, msk_make((unsigned long long)k, 16), cell, sizeof(cell));
        if (k > 0) {
            strncat(out, " ", size - strlen(out) - 1);
        }
        strncat(out, cell, size - strlen(out) - 1);
    }
}

int main(void) {
    char library_row[4096];
    for (int i = 0; i < FIXTURES; i++) {
        MotiveSketch sketch = make_sketch(&FIXTURE[i]);
        PlanMeter meter;
        assert(motive_meter_of(&sketch, &meter));
        assert(meter.beats_per_bar == FIXTURE[i].meter.beats_per_bar && meter.beat_unit == FIXTURE[i].meter.beat_unit);
        row(&FIXTURE[i], library_row, sizeof(library_row));
        if (strcmp(EXPECTED[i], library_row) != 0) {
            fprintf(stderr, "%s: recorded\n  %s\nlibrary\n  %s\n", FIXTURE[i].name, EXPECTED[i], library_row);
        }
        assert(strcmp(EXPECTED[i], library_row) == 0);
    }
    printf("ok\n");
    return 0;
}
