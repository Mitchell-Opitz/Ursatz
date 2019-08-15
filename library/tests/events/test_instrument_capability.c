#include "events/instrument_capability.h"

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

static Instrument *make_instrument(const char *id_value, const char *name, const char *low, const char *high,
                                   int transposition_semitones) {
    Instrument *instrument = NULL;
    instrument_create(make_id(id_value), name, make_pitch(low), make_pitch(high), transposition_semitones,
                       &instrument);
    return instrument;
}

static void test_create_rejects_null_instrument(void) {
    InstrumentCapability *capability = NULL;
    InstrumentCapabilityResult r = instrument_capability_create_from_instrument(NULL, &capability);
    assert(r == INSTRUMENT_CAPABILITY_ERR_NULL_INSTRUMENT);
    assert(capability == NULL);
}

static void test_create_copies_range_and_transposition(void) {
    Instrument *instrument = make_instrument("instrument-clarinet", "B-flat Clarinet", "D3", "Bb6", -2);

    InstrumentCapability *capability = NULL;
    InstrumentCapabilityResult r = instrument_capability_create_from_instrument(instrument, &capability);
    assert(r == INSTRUMENT_CAPABILITY_OK);
    assert(capability != NULL);
    assert(instrument_capability_transposition_semitones(capability) == -2);

    PitchIdentity *expected_low = make_pitch("D3");
    PitchIdentity *expected_high = make_pitch("Bb6");
    assert(pitch_identity_equals(instrument_capability_range_lowest(capability), expected_low));
    assert(pitch_identity_equals(instrument_capability_range_highest(capability), expected_high));
    pitch_identity_destroy(expected_low);
    pitch_identity_destroy(expected_high);

    instrument_capability_destroy(capability);
    instrument_destroy(instrument);
}

/* Separable from Instrument identity: destroying the source Instrument
   does not invalidate an already-derived InstrumentCapability, since
   the capability copies range/transposition rather than referencing
   the Instrument. */
static void test_capability_survives_source_instrument_destruction(void) {
    Instrument *instrument = make_instrument("instrument-clarinet", "B-flat Clarinet", "D3", "Bb6", -2);

    InstrumentCapability *capability = NULL;
    instrument_capability_create_from_instrument(instrument, &capability);

    instrument_destroy(instrument);

    PitchIdentity *expected_low = make_pitch("D3");
    assert(pitch_identity_equals(instrument_capability_range_lowest(capability), expected_low));
    pitch_identity_destroy(expected_low);
    assert(instrument_capability_transposition_semitones(capability) == -2);

    instrument_capability_destroy(capability);
}

static void test_equals_same_range_and_transposition(void) {
    Instrument *instrument_a = make_instrument("instrument-clarinet", "B-flat Clarinet", "D3", "Bb6", -2);
    Instrument *instrument_b = make_instrument("instrument-clarinet-2", "Another Clarinet", "D3", "Bb6", -2);

    InstrumentCapability *a = NULL;
    InstrumentCapability *b = NULL;
    instrument_capability_create_from_instrument(instrument_a, &a);
    instrument_capability_create_from_instrument(instrument_b, &b);

    assert(a != b);
    assert(instrument_capability_equals(a, b));

    instrument_capability_destroy(a);
    instrument_capability_destroy(b);
    instrument_destroy(instrument_a);
    instrument_destroy(instrument_b);
}

static void test_inequals_different_transposition(void) {
    Instrument *instrument_a = make_instrument("instrument-clarinet", "B-flat Clarinet", "D3", "Bb6", -2);
    Instrument *instrument_b = make_instrument("instrument-clarinet-c", "C Clarinet", "D3", "Bb6", 0);

    InstrumentCapability *a = NULL;
    InstrumentCapability *b = NULL;
    instrument_capability_create_from_instrument(instrument_a, &a);
    instrument_capability_create_from_instrument(instrument_b, &b);

    assert(!instrument_capability_equals(a, b));

    instrument_capability_destroy(a);
    instrument_capability_destroy(b);
    instrument_destroy(instrument_a);
    instrument_destroy(instrument_b);
}

int main(void) {
    test_create_rejects_null_instrument();
    test_create_copies_range_and_transposition();
    test_capability_survives_source_instrument_destruction();
    test_equals_same_range_and_transposition();
    test_inequals_different_transposition();
    printf("test_instrument_capability: ok\n");
    return 0;
}