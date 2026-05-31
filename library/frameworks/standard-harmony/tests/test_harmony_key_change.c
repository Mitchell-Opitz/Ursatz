#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_key_change.h"
#include "harmony_test_util.h"

#define TON KEY_CHANGE_EXTENT_TONICIZATION
#define EXT KEY_CHANGE_EXTENT_EXTENDED_TONICIZATION
#define WEAK KEY_CHANGE_EXTENT_WEAK_MODULATION
#define MOD KEY_CHANGE_EXTENT_MODULATION

static bool classify_is(int chords, bool cadence, bool continues, KeyChangeExtent expected) {
    HarmonyKeyChange change = {chords, cadence, continues};
    KeyChangeExtent extent = KEY_CHANGE_EXTENT_COUNT;
    return harmony_key_change_classify(&change, &extent) && extent == expected;
}

static void test_a_tonicization_is_short_without_a_cadence(void) {
    assert(classify_is(1, false, false, TON));
    assert(classify_is(2, false, false, TON)); /* V7/V to V */
}

static void test_a_longer_passage_without_a_cadence_is_an_extended_tonicization(void) {
    assert(classify_is(3, false, false, EXT));
    assert(classify_is(8, false, false, EXT));
}

static void test_a_cadence_in_the_new_key_makes_a_modulation(void) {
    assert(classify_is(6, true, true, MOD));
    assert(classify_is(6, true, false, WEAK));
    assert(classify_is(2, true, true, MOD)); /* the cadence, not the length, establishes the key */
}

static void test_invalid_changes(void) {
    HarmonyKeyChange none = {0, false, false};
    HarmonyKeyChange negative = {-1, false, false};
    HarmonyKeyChange continues_without_cadence = {4, false, true};
    HarmonyKeyChange good = {4, true, true};
    KeyChangeExtent extent = KEY_CHANGE_EXTENT_COUNT;
    assert(!harmony_key_change_classify(&none, &extent));
    assert(!harmony_key_change_classify(&negative, &extent));
    assert(!harmony_key_change_classify(&continues_without_cadence, &extent));
    assert(extent == KEY_CHANGE_EXTENT_COUNT);
    assert(!harmony_key_change_classify(NULL, &extent));
    assert(!harmony_key_change_classify(&good, NULL));
}

static void test_a_pivot_makes_a_pivot_modulation(void) {
    HarmonyPivot pivot = {4, 2};
    assert(harmony_modulation_kind(&pivot) == MODULATION_KIND_PIVOT);
    assert(harmony_modulation_kind(NULL) == MODULATION_KIND_DIRECT);
}

static bool label_is(HarmonyMode home, int degree, HarmonyMode mode, const char *expected) {
    HarmonyRelatedKey key = {degree, mode};
    char out[16];
    return harmony_extended_tonicization_label(home, &key, out, sizeof(out)) && strcmp(out, expected) == 0;
}

static void test_the_extended_tonicization_label(void) {
    assert(label_is(HARMONY_MODE_MAJOR, 2, HARMONY_MODE_MINOR, "ii")); /* F minor in E-flat major */
    assert(label_is(HARMONY_MODE_MAJOR, 5, HARMONY_MODE_MAJOR, "V"));
    assert(label_is(HARMONY_MODE_MAJOR, 6, HARMONY_MODE_MINOR, "vi"));
    assert(label_is(HARMONY_MODE_MINOR, 3, HARMONY_MODE_MAJOR, "III"));
    assert(label_is(HARMONY_MODE_MINOR, 5, HARMONY_MODE_MINOR, "v"));
}

static void test_label_buffers_and_invalid_input(void) {
    HarmonyRelatedKey key = {2, HARMONY_MODE_MINOR};
    HarmonyRelatedKey tonic = {1, HARMONY_MODE_MAJOR};
    char out[8] = "xxxxxxx";
    assert(harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, &key, out, 3) && strcmp(out, "ii") == 0);
    strcpy(out, "xxxxxxx");
    assert(!harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, &key, out, 2) && strcmp(out, "xxxxxxx") == 0);
    assert(!harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, &key, out, 0));
    assert(!harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, &tonic, out, 8));
    assert(!harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, NULL, out, 8));
    assert(!harmony_extended_tonicization_label(HARMONY_MODE_MAJOR, &key, NULL, 8));
}

static bool confirm_is(KeyChangeExtent length, const double *cadences, size_t count, KeyChangeExtent expected) {
    KeyChangeExtent extent = KEY_CHANGE_EXTENT_COUNT;
    return harmony_key_change_confirm(length, 8.0, 12.0, cadences, count, &extent) && extent == expected;
}

static void test_a_cadence_in_the_region_confirms_a_modulation(void) {
    double inside[] = {2.0, 11.0};
    assert(confirm_is(MOD, inside, 2, MOD));
    assert(confirm_is(WEAK, inside, 2, WEAK));
    assert(confirm_is(TON, inside, 2, WEAK)); /* short, cadenced, then home */
    assert(confirm_is(EXT, inside, 2, WEAK));
}

static void test_a_region_without_a_cadence_is_a_tonicization_whatever_its_length(void) {
    double outside[] = {2.0, 12.0}; /* the end of the region is open: 12 belongs to the next one */
    assert(confirm_is(MOD, outside, 2, TON));
    assert(confirm_is(WEAK, outside, 2, TON));
    assert(confirm_is(TON, outside, 2, TON));
    assert(confirm_is(EXT, outside, 2, EXT));
    double none[1];
    assert(confirm_is(MOD, none, 0, TON));
}

static void test_the_length_rule_is_the_fallback_without_cadences(void) {
    assert(confirm_is(MOD, NULL, 0, MOD));
    assert(confirm_is(WEAK, NULL, 0, WEAK));
    assert(confirm_is(TON, NULL, 0, TON));
    double at_start[] = {8.0};
    assert(confirm_is(TON, at_start, 1, WEAK)); /* the start of the region is inside */
}

static void test_invalid_confirmation(void) {
    double c[] = {9.0};
    KeyChangeExtent extent = KEY_CHANGE_EXTENT_COUNT;
    assert(!harmony_key_change_confirm(KEY_CHANGE_EXTENT_COUNT, 8.0, 12.0, c, 1, &extent));
    assert(!harmony_key_change_confirm(MOD, 12.0, 8.0, c, 1, &extent));
    assert(!harmony_key_change_confirm(MOD, 8.0, 12.0, c, 1, NULL));
    assert(extent == KEY_CHANGE_EXTENT_COUNT);
}

int main(void) {
    test_a_cadence_in_the_region_confirms_a_modulation();
    test_a_region_without_a_cadence_is_a_tonicization_whatever_its_length();
    test_the_length_rule_is_the_fallback_without_cadences();
    test_invalid_confirmation();
    test_a_tonicization_is_short_without_a_cadence();
    test_a_longer_passage_without_a_cadence_is_an_extended_tonicization();
    test_a_cadence_in_the_new_key_makes_a_modulation();
    test_invalid_changes();
    test_a_pivot_makes_a_pivot_modulation();
    test_the_extended_tonicization_label();
    test_label_buffers_and_invalid_input();
    printf("test_harmony_key_change: all tests passed\n");
    return 0;
}
