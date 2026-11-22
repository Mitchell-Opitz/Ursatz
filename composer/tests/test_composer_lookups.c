#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/motive_tones.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/style.h>
#include <ursatz_composer/variant_registry.h>
#include <ursatz_composer/voicing.h>
#include "support/test_knowledge.h"

/*
 * The small lookups of the Composer's headers, each called here with its answer asserted: the name tables (every value has its own
 * name, a value out of range is "unknown" or has none), the round trips of names, the default parameters,
 * the pitch names a note list writes and the meter's chord cell.
 */

/* Every name of values 0 to count - 1 is a distinct non-empty word that is not "unknown". */
static void expect_names(const char *(*name_of)(int), int count, const char *unknown) {
    for (int i = 0; i < count; i++) {
        const char *name = name_of(i);
        assert(name != NULL && name[0] != '\0' && strcmp(name, unknown) != 0);
        for (int j = i + 1; j < count; j++) {
            assert(strcmp(name, name_of(j)) != 0);
        }
    }
    assert(strcmp(name_of(count), unknown) == 0 && strcmp(name_of(-1), unknown) == 0);
}

static const char *given_name(int v) { return given_result_name((GivenResult)v); }
static const char *regenerate_name(int v) { return regenerate_result_name((RegenerateResult)v); }
static const char *style_name(int v) { return composer_style_result_name((ComposerStyleResult)v); }
static const char *variant_name(int v) { return variant_result_name((VariantResult)v); }
static const char *develop_name(int v) { return motive_dev_result_name((MotiveDevResult)v); }

static void test_result_names(void) {
    expect_names(given_name, GIVEN_ERR_PLAN + 1, "unknown");
    expect_names(regenerate_name, REGENERATE_ERR_STUCK + 1, "unknown");
    expect_names(style_name, COMPOSER_STYLE_ERR_ALLOC + 1, "unknown");
    expect_names(variant_name, VARIANT_ERR_MEMORY + 1, "unknown");
    expect_names(develop_name, MOTIVE_DEV_ERR_NO_ACCOMPANIMENT + 1, "unknown");
    assert(strcmp(given_result_name(GIVEN_OK), "ok") == 0 && strcmp(given_result_name(GIVEN_ERR_PAST_END), "past the section's end") == 0);
    assert(strcmp(variant_result_name(VARIANT_OK), "ok") == 0 && strcmp(variant_result_name(VARIANT_ERR_EMPTY), "empty chain") == 0);
    assert(strcmp(motive_dev_result_name(MOTIVE_DEV_OK), "ok") == 0);
}

static void test_chord_cell_of_a_meter(void) {
    Rational length;
    PlanMeter four_four = {4, 4, 0};
    PlanMeter six_eight = {6, 8, 0};
    PlanMeter five_four = {5, 4, 0};
    assert(given_cell_length(&four_four, &length) && length.num == 1 && length.den == 2); /* half a bar */
    assert(given_cell_length(&six_eight, &length) && length.num == 3 && length.den == 8); /* a dotted quarter */
    assert(!given_cell_length(&five_four, &length));
    assert(!given_cell_length(NULL, &length));
}

static void test_motive_names(void) {
    char out[32];
    motive_pitch_name(60, out, sizeof(out));
    assert(strcmp(out, "C4") == 0);
    motive_pitch_name(61, out, sizeof(out));
    assert(strcmp(out, "C#4") == 0);
    motive_pitch_name(62, out, sizeof(out));
    assert(strcmp(out, "D4") == 0);
    motive_pitch_name(58, out, sizeof(out));
    assert(strcmp(out, "A#3") == 0);
}

static void test_melody_names(void) {
    for (int rule = 0; rule < MELODY_RULE_COUNT; rule++) {
        MelodyHardRule found = MELODY_RULE_COUNT;
        assert(melody_hard_rule_find(melody_hard_rule_name((MelodyHardRule)rule), &found) && found == (MelodyHardRule)rule);
    }
    for (int group = 0; group < MELODY_GROUP_COUNT; group++) {
        MelodySoftGroup found = MELODY_GROUP_COUNT;
        assert(melody_soft_group_find(melody_soft_group_name((MelodySoftGroup)group), &found) && found == (MelodySoftGroup)group);
    }
    MelodyHardRule rule = MELODY_RULE_NO_CROSSING;
    MelodySoftGroup group = MELODY_GROUP_TENDENCY;
    assert(!melody_hard_rule_find("bogus", &rule) && !melody_hard_rule_find(NULL, &rule) && rule == MELODY_RULE_NO_CROSSING);
    assert(!melody_soft_group_find("bogus", &group) && !melody_soft_group_find(NULL, &group) && group == MELODY_GROUP_TENDENCY);
}

static void test_defaults(void) {
    VoicingParams voicing = voicing_params_default();
    assert(voicing.candidates == 4 && voicing.beam == 64);
    CounterpointParams counterpoint = counterpoint_params_default();
    assert(counterpoint.candidates == 4 && counterpoint.beam == 128);
}

static void test_registry_and_plan_names(void) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        VariantParam param;
        for (size_t p = 0; variant_operation_param(operation, p, &param); p++) {
            assert(variant_operation_param_index(operation, param.name) == (int)p);
        }
        assert(variant_operation_param_index(operation, "no-such-parameter") == -1);
        assert(variant_operation_param_index(operation, NULL) == -1);
    }
    assert(harmony_mode_of_material(test_materials()->major) == HARMONY_MODE_MAJOR);
    assert(harmony_mode_of_material(test_materials()->natural_minor) == HARMONY_MODE_MINOR);
    assert(harmony_mode_of_material(NULL) == HARMONY_MODE_MAJOR);

    RenderedPart part = {PLAN_LANE_MELODY, 0};
    assert(strcmp(rendered_part_lane_name(&part), "melody") == 0);
    part.lane = PLAN_LANE_BASS;
    assert(strcmp(rendered_part_lane_name(&part), "bass") == 0);
    part.lane = PLAN_LANE_INNER_VOICE;
    part.voice = 2;
    assert(strcmp(rendered_part_lane_name(&part), "inner_voice") == 0);
}

int main(void) {
    test_result_names();
    test_chord_cell_of_a_meter();
    test_motive_names();
    test_melody_names();
    test_defaults();
    test_registry_and_plan_names();
    printf("test_composer_lookups: ok\n");
    return 0;
}
