#include "events/instrument.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *pitch = NULL;
    pitch_identity_create(token, strlen(token) + 1, &pitch);
    return pitch;
}

/* Every failure path below returns before instrument_create takes
   ownership of anything (src/events/instrument.c validates id, name,
   range_lowest, range_highest in that order before any assignment), so
   every argument that WAS successfully constructed for a given call
   -- even ones that are not the null one under test -- is still owned
   by the caller and must be destroyed here. */

static void test_create_rejects_null_id(void) {
    PitchIdentity *low = make_pitch("D3");
    PitchIdentity *high = make_pitch("Bb6");

    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(NULL, "B-flat Clarinet", low, high, -2, &instrument);
    assert(r == INSTRUMENT_ERR_NULL_ID);
    assert(instrument == NULL);

    pitch_identity_destroy(low);
    pitch_identity_destroy(high);
}

static void test_create_rejects_null_name(void) {
    EntityID *id = make_id("instrument-clarinet");
    PitchIdentity *low = make_pitch("D3");
    PitchIdentity *high = make_pitch("Bb6");

    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(id, NULL, low, high, -2, &instrument);
    assert(r == INSTRUMENT_ERR_NULL_NAME);
    assert(instrument == NULL);

    entity_id_destroy(id);
    pitch_identity_destroy(low);
    pitch_identity_destroy(high);
}

static void test_create_rejects_empty_name(void) {
    EntityID *id = make_id("instrument-clarinet");
    PitchIdentity *low = make_pitch("D3");
    PitchIdentity *high = make_pitch("Bb6");

    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(id, "", low, high, -2, &instrument);
    assert(r == INSTRUMENT_ERR_NULL_NAME);
    assert(instrument == NULL);

    entity_id_destroy(id);
    pitch_identity_destroy(low);
    pitch_identity_destroy(high);
}

static void test_create_rejects_null_range_lowest(void) {
    EntityID *id = make_id("instrument-clarinet");
    PitchIdentity *high = make_pitch("Bb6");

    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(id, "B-flat Clarinet", NULL, high, -2, &instrument);
    assert(r == INSTRUMENT_ERR_NULL_RANGE_LOWEST);
    assert(instrument == NULL);

    entity_id_destroy(id);
    pitch_identity_destroy(high);
}

static void test_create_rejects_null_range_highest(void) {
    EntityID *id = make_id("instrument-clarinet");
    PitchIdentity *low = make_pitch("D3");

    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(id, "B-flat Clarinet", low, NULL, -2, &instrument);
    assert(r == INSTRUMENT_ERR_NULL_RANGE_HIGHEST);
    assert(instrument == NULL);

    entity_id_destroy(id);
    pitch_identity_destroy(low);
}

static void test_create_accepts_valid(void) {
    Instrument *instrument = NULL;
    InstrumentResult r = instrument_create(make_id("instrument-clarinet"), "B-flat Clarinet", make_pitch("D3"),
                                            make_pitch("Bb6"), -2, &instrument);
    assert(r == INSTRUMENT_OK);
    assert(instrument != NULL);
    assert(strcmp(instrument_name(instrument), "B-flat Clarinet") == 0);
    assert(instrument_transposition_semitones(instrument) == -2);

    PitchIdentity *expected_low = make_pitch("D3");
    PitchIdentity *expected_high = make_pitch("Bb6");
    assert(pitch_identity_equals(instrument_range_lowest(instrument), expected_low));
    assert(pitch_identity_equals(instrument_range_highest(instrument), expected_high));
    pitch_identity_destroy(expected_low);
    pitch_identity_destroy(expected_high);

    instrument_destroy(instrument);
}

static void test_identity_equals_same_id(void) {
    Instrument *a = NULL;
    Instrument *b = NULL;
    instrument_create(make_id("instrument-clarinet"), "B-flat Clarinet", make_pitch("D3"), make_pitch("Bb6"), -2,
                       &a);
    instrument_create(make_id("instrument-clarinet"), "B-flat Clarinet (duplicate)", make_pitch("D3"),
                       make_pitch("Bb6"), -2, &b);

    assert(a != b);
    assert(instrument_identity_equals(a, b));

    instrument_destroy(a);
    instrument_destroy(b);
}

static void test_identity_not_equal_different_id(void) {
    Instrument *a = NULL;
    Instrument *b = NULL;
    instrument_create(make_id("instrument-clarinet"), "B-flat Clarinet", make_pitch("D3"), make_pitch("Bb6"), -2,
                       &a);
    instrument_create(make_id("instrument-flute"), "Flute", make_pitch("C4"), make_pitch("D7"), 0, &b);

    assert(!instrument_identity_equals(a, b));

    instrument_destroy(a);
    instrument_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_name();
    test_create_rejects_empty_name();
    test_create_rejects_null_range_lowest();
    test_create_rejects_null_range_highest();
    test_create_accepts_valid();
    test_identity_equals_same_id();
    test_identity_not_equal_different_id();
    printf("test_instrument: ok\n");
    return 0;
}