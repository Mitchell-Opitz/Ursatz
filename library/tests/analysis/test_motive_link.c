#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motive_link.h"

/*
 * Analysis-regression tests for motive links: the gate, both directions, replaying a found chain,
 * sketch fingerprints and the text form of relation steps.
 */

static Rational q(unsigned long long n, unsigned long long d) {
    return rational_ratio(n, d);
}

/* C4:q E4:e G4:e F4:q E4:q in C major, 4/4. */
static MotiveSketch motive(void) {
    static const int pitches[5] = {60, 64, 67, 65, 64};
    static const unsigned long long onset_n[5] = {0, 1, 3, 1, 3};
    static const unsigned long long onset_d[5] = {1, 4, 8, 2, 4};
    static const unsigned long long dur_d[5] = {4, 8, 8, 4, 4};
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    for (size_t i = 0; i < 5; i++) {
        sketch.notes[i].pitch = pitches[i];
        sketch.notes[i].onset = q(onset_n[i], onset_d[i]);
        sketch.notes[i].duration = q(1, dur_d[i]);
    }
    sketch.note_count = 5;
    sketch.has_scale = true;
    sketch.tonic_pc = 0;
    sketch.bar = q(1, 1);
    sketch.beat = q(1, 4);
    sketch.pickup = q(0, 1);
    sketch.length = q(1, 1);
    return sketch;
}

static MotiveRelationStep step(MotiveRelationKind kind) {
    MotiveRelationStep result;
    memset(&result, 0, sizeof(result));
    result.kind = kind;
    return result;
}

/* a tonally transposed by 2, then augmented 2:1. */
static MotiveSketch transposed_augmented(const MotiveSketch *a) {
    MotiveRelationStep transpose = step(MOTIVE_RELATION_TRANSPOSE);
    transpose.value[0] = 1;
    transpose.value[1] = 2;
    MotiveRelationStep augment = step(MOTIVE_RELATION_AUGMENT);
    augment.value[0] = 2;
    augment.den[0] = 1;
    MotiveSketch middle;
    MotiveSketch b;
    assert(motive_relation_apply(a, &transpose, &middle) == MOTIVE_RELATION_OK);
    assert(motive_relation_apply(&middle, &augment, &b) == MOTIVE_RELATION_OK);
    return b;
}

static void test_linked_forward(void) {
    MotiveSketch a = motive();
    MotiveSketch b = transposed_augmented(&a);
    MotiveLink link;
    assert(motive_link_pair(&a, &b, NULL, &link) == MOTIVE_LINK_OK);
    assert(!link.gated);
    assert(link.linked);
    assert(!link.reversed);
    assert(link.recognition.found);
    assert(link.recognition.step_count == 2);
    MotiveSketch at = a;
    for (size_t i = 0; i < link.recognition.step_count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&at, &link.recognition.steps[i], &next) == MOTIVE_RELATION_OK);
        at = next;
    }
    assert(motive_sketch_equal(&at, &b));
}

static void test_linked_reversed(void) {
    MotiveSketch whole = motive();
    MotiveRelationStep fragment = step(MOTIVE_RELATION_FRAGMENT);
    fragment.value[0] = 1;
    fragment.value[1] = 3;
    MotiveSketch part;
    assert(motive_relation_apply(&whole, &fragment, &part) == MOTIVE_RELATION_OK);
    assert(part.note_count == 3);

    MotiveRecognizeOptions search;
    motive_recognize_options_default(&search);
    search.max_depth = 2;
    MotiveRecognition forward;
    assert(motive_variant_recognize(&part, &whole, &search, &forward) == MOTIVE_RECOGNIZE_OK);
    assert(!forward.found);

    MotiveLinkOptions options;
    motive_link_options_default(&options);
    options.min_nearness = 0.0;
    MotiveLink link;
    assert(motive_link_pair(&part, &whole, &options, &link) == MOTIVE_LINK_OK);
    assert(!link.gated);
    assert(link.linked);
    assert(link.reversed);
    assert(link.recognition.found);
    MotiveSketch at = whole;
    for (size_t i = 0; i < link.recognition.step_count; i++) {
        MotiveSketch next;
        assert(motive_relation_apply(&at, &link.recognition.steps[i], &next) == MOTIVE_RELATION_OK);
        at = next;
    }
    assert(motive_sketch_equal(&at, &part));
}

static void test_gated(void) {
    MotiveSketch a = motive();
    MotiveSketch other;
    memset(&other, 0, sizeof(other));
    static const int pitches[3] = {72, 67, 62};
    for (size_t i = 0; i < 3; i++) {
        other.notes[i].pitch = pitches[i];
        other.notes[i].onset = q(i, 2);
        other.notes[i].duration = q(1, 2);
    }
    other.note_count = 3;
    other.has_scale = true;
    other.bar = q(1, 1);
    other.beat = q(1, 4);
    other.pickup = q(0, 1);
    other.length = q(3, 2);
    MotiveLink link;
    assert(motive_link_pair(&a, &other, NULL, &link) == MOTIVE_LINK_OK);
    assert(link.gated);
    assert(!link.linked);
    assert(link.nearness < 0.5);
    assert(link.nearness == motive_link_nearness(&a, &other));
}

static void test_invalid(void) {
    MotiveSketch a = motive();
    MotiveSketch bad = a;
    bad.note_count = 0;
    MotiveLink link;
    assert(motive_link_pair(&a, &bad, NULL, &link) == MOTIVE_LINK_INVALID);
    MotiveLinkOptions options;
    motive_link_options_default(&options);
    assert(options.max_depth == 2 && options.beam == 64 && options.min_nearness == 0.5 && !options.contour);
    options.max_depth = 0;
    assert(motive_link_pair(&a, &a, &options, &link) == MOTIVE_LINK_INVALID);
    motive_link_options_default(&options);
    options.min_nearness = 1.5;
    assert(motive_link_pair(&a, &a, &options, &link) == MOTIVE_LINK_INVALID);
    assert(motive_link_pair(&a, &a, NULL, NULL) == MOTIVE_LINK_INVALID);
}

static void test_fingerprints(void) {
    MotiveSketch a = motive();
    MotiveSketch same = motive();
    assert(motive_sketch_fingerprint(&a) == motive_sketch_fingerprint(&same));
    MotiveSketch moved = motive();
    moved.notes[2].onset = q(5, 16);
    assert(motive_sketch_fingerprint(&a) != motive_sketch_fingerprint(&moved));
    MotiveSketch pitched = motive();
    pitched.notes[1].pitch = 65;
    assert(motive_sketch_fingerprint(&a) != motive_sketch_fingerprint(&pitched));
    MotiveSketch tonic = motive();
    tonic.tonic_pc = 7;
    assert(motive_sketch_fingerprint(&a) != motive_sketch_fingerprint(&tonic));
    MotiveSketch minor = motive();
    minor.minor = true;
    assert(motive_sketch_fingerprint(&a) != motive_sketch_fingerprint(&minor));
    MotiveSketch meter = motive();
    meter.bar = q(3, 4);
    assert(motive_sketch_fingerprint(&a) != motive_sketch_fingerprint(&meter));
    /* Entries past the used count do not count. */
    MotiveSketch junk = motive();
    junk.notes[10].pitch = 99;
    assert(motive_sketch_fingerprint(&a) == motive_sketch_fingerprint(&junk));
}

static bool ratio_index(MotiveRelationKind kind, size_t index) {
    return ((kind == MOTIVE_RELATION_AUGMENT || kind == MOTIVE_RELATION_DIMINISH ||
             kind == MOTIVE_RELATION_RHYTHM_SHIFT) &&
            index == 0) ||
           (kind == MOTIVE_RELATION_CONDENSE && index == 2) ||
           (kind == MOTIVE_RELATION_SHIFT_FEATURE && index == 1);
}

static void assert_round_trip(const MotiveRelationStep *original, const char *expected) {
    char text[160];
    assert(motive_relation_step_text(original, text, sizeof(text)));
    if (expected) {
        assert(strcmp(text, expected) == 0);
    }
    MotiveRelationStep back;
    memset(&back, 0x7f, sizeof(back));
    assert(motive_relation_step_parse(text, &back));
    assert(memcmp(original, &back, sizeof(back)) == 0);
}

static void test_step_text(void) {
    assert(motive_relation_param_name(MOTIVE_RELATION_TRANSPOSE, 1) != NULL);
    assert(strcmp(motive_relation_param_name(MOTIVE_RELATION_TRANSPOSE, 1), "by") == 0);
    assert(motive_relation_param_name(MOTIVE_RELATION_EXACT, 0) == NULL);
    assert(motive_relation_param_name(MOTIVE_RELATION_TRANSPOSE, 3) == NULL);
    assert(motive_relation_param_name(MOTIVE_RELATION_KIND_COUNT, 0) == NULL);

    for (int k = 0; k < MOTIVE_RELATION_KIND_COUNT; k++) {
        MotiveRelationKind kind = (MotiveRelationKind)k;
        size_t params = motive_relation_param_count(kind);
        for (size_t i = 0; i < params; i++) {
            assert(motive_relation_param_name(kind, i) != NULL);
        }
        /* Every parameter given. */
        MotiveRelationStep full = step(kind);
        for (size_t i = 0; i < params; i++) {
            full.value[i] = (long)(i + 2);
            if (ratio_index(kind, i)) {
                full.den[i] = 3;
            }
        }
        assert_round_trip(&full, NULL);
        /* Every parameter at its default. */
        MotiveRelationStep defaults = step(kind);
        if (kind == MOTIVE_RELATION_INVERT) {
            defaults.value[0] = -1;
        }
        assert_round_trip(&defaults, k == MOTIVE_RELATION_EXACT ? "exact" : NULL);
        /* A negative value. */
        if (params > 0 && !ratio_index(kind, 0)) {
            MotiveRelationStep negative = step(kind);
            negative.value[0] = -2;
            assert_round_trip(&negative, NULL);
        }
    }

    MotiveRelationStep transpose = step(MOTIVE_RELATION_TRANSPOSE);
    transpose.value[0] = 1;
    transpose.value[1] = -2;
    assert_round_trip(&transpose, "transpose tonal=1 by=-2");
    MotiveRelationStep augment = step(MOTIVE_RELATION_AUGMENT);
    augment.value[0] = 3;
    augment.den[0] = 2;
    assert_round_trip(&augment, "augment ratio=3:2");
    MotiveRelationStep invert = step(MOTIVE_RELATION_INVERT);
    invert.value[0] = -1;
    assert_round_trip(&invert, "invert");

    MotiveRelationStep out;
    assert(!motive_relation_step_parse("transpose sideways=1", &out));
    assert(!motive_relation_step_parse("transpose by=1 by=2", &out));
    assert(!motive_relation_step_parse("transpose by", &out));
    assert(!motive_relation_step_parse("transpose by=x", &out));
    assert(!motive_relation_step_parse("augment ratio=3", &out));
    assert(!motive_relation_step_parse("augment ratio=0:2", &out));
    assert(!motive_relation_step_parse("sway by=1", &out));
    assert(!motive_relation_step_parse("", &out));

    char small[8];
    assert(!motive_relation_step_text(&transpose, small, sizeof(small)));
    assert(strlen(small) < sizeof(small));
    MotiveRelationStep bad = step(MOTIVE_RELATION_KIND_COUNT);
    char text[32];
    assert(!motive_relation_step_text(&bad, text, sizeof(text)));
}

int main(void) {
    test_linked_forward();
    test_linked_reversed();
    test_gated();
    test_invalid();
    test_fingerprints();
    test_step_text();
    printf("motive link tests passed\n");
    return 0;
}
