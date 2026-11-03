#include <assert.h>
#include <string.h>

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/harmony_sequence.h>

static PlanMeter meter(int beats, int unit) {
    PlanMeter result = {beats, unit, 0};
    return result;
}

static void test_meters_and_minimum_bars(void) {
    PlanMeter supported[4] = {meter(2, 4), meter(3, 4), meter(4, 4), meter(6, 8)};
    for (int i = 0; i < 4; i++) {
        assert(harmony_meter_supported(&supported[i]));
        /* A cadence needs at least two bars, and a plagal cadence more than an authentic one. */
        assert(harmony_phrase_min_bars(CADENCE_TYPE_HALF, &supported[i]) >= 2);
        assert(harmony_phrase_min_bars(CADENCE_TYPE_PLAGAL, &supported[i]) > harmony_phrase_min_bars(CADENCE_TYPE_PERFECT_AUTHENTIC, &supported[i]));
        assert(harmony_phrase_min_bars(CADENCE_TYPE_PERFECT_AUTHENTIC, &supported[i]) >= harmony_phrase_min_bars(CADENCE_TYPE_HALF, &supported[i]));
    }
    PlanMeter odd[3] = {meter(5, 4), meter(7, 8), meter(4, 8)};
    for (int i = 0; i < 3; i++) {
        assert(!harmony_meter_supported(&odd[i]));
        assert(harmony_phrase_min_bars(CADENCE_TYPE_PERFECT_AUTHENTIC, &odd[i]) == 0);
    }
    /* In 4/4 a PAC needs 2 bars (I V on one, I on the next) and a PC 3; in 3/4 a PAC needs 3 and a PC 5. */
    assert(harmony_phrase_min_bars(CADENCE_TYPE_PERFECT_AUTHENTIC, &supported[2]) == 2);
    assert(harmony_phrase_min_bars(CADENCE_TYPE_PLAGAL, &supported[2]) == 3);
    assert(harmony_phrase_min_bars(CADENCE_TYPE_PERFECT_AUTHENTIC, &supported[1]) == 3);
    assert(harmony_phrase_min_bars(CADENCE_TYPE_PLAGAL, &supported[1]) == 5);
}

static void test_names(void) {
    GalantSchemaId schema;
    ScalePatternId pattern;
    assert(harmony_schema_find("romanesca", &schema) && schema == GALANT_SCHEMA_ROMANESCA);
    assert(harmony_schema_find("do-re-mi", &schema) && schema == GALANT_SCHEMA_DO_RE_MI);
    assert(!harmony_schema_find("Romanesca", &schema));
    assert(!harmony_schema_find("", &schema));
    assert(!harmony_schema_find(NULL, &schema));
    assert(harmony_sequence_find("five-six-ascending", &pattern) && pattern == SCALE_PATTERN_FIVE_SIX_ASCENDING);
    assert(!harmony_sequence_find("descending-fifths", &pattern));
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        GalantSchemaId found;
        assert(harmony_schema_find(galant_schema_name((GalantSchemaId)id), &found) && (int)found == id);
    }
}

/* The schemas whose data makes chords are available, the modulating ones and the inconsistent ones are not. */
static void test_availability(void) {
    assert(harmony_schema_available(GALANT_SCHEMA_ROMANESCA, KEY_STRUCTURE_MODE_MAJOR));
    assert(harmony_schema_available(GALANT_SCHEMA_DO_RE_MI, KEY_STRUCTURE_MODE_MAJOR));
    assert(harmony_schema_available(GALANT_SCHEMA_DO_RE_MI, KEY_STRUCTURE_MODE_MINOR));
    assert(!harmony_schema_available(GALANT_SCHEMA_MODULATING_PRINNER, KEY_STRUCTURE_MODE_MAJOR));
    assert(!harmony_schema_available(GALANT_SCHEMA_QUIESCENZA, KEY_STRUCTURE_MODE_MAJOR));
    assert(!harmony_schema_available(GALANT_SCHEMA_COUNT, KEY_STRUCTURE_MODE_MAJOR));
}

int main(void) {
    test_meters_and_minimum_bars();
    test_names();
    test_availability();
    return 0;
}
