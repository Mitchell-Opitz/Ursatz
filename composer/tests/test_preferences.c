#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/preferences.h>
#include "support/test_composer.h"

/*
 * The preferences document (R-411): the defaults written and read back, the melody part of the document in the shape
 * docs/preferences.md gives, members read over the defaults, and every melody limit refused with its path.
 */

static const char *DEFAULT_MELODY =
    "{\"range\":{\"low\":60,\"high\":84},\"nct_types\":[\"passing\",\"neighbor\"],\"density\":0,\"climax_position\":667,"
    "\"climax_rise\":4,\"climax_slack\":150,\"second_nearest_chord_tone\":\"1/3\",\"attempts\":48,\"keep\":6,"
    "\"plain_shaping\":false,\"weights\":{\"melodic\":1,\"against_bass\":1,\"tendency\":1,\"climax\":1,\"contour\":1,"
    "\"ending_reduction\":1,\"waves\":1,\"central_range\":1,\"monotony\":1,\"seam\":1},"
    "\"closure\":{\"final\":0,\"first\":0},\"rhythm_repeat_bars\":0,\"leaps\":0,\"register\":0,\"waive\":[],\"reverse\":[]}";

static const char *DEFAULT_MOTIF =
    "{\"bars\":{\"one\":1,\"two\":1},\"pickup_one_in\":4,\"shapes_one_bar\":{\"repeat\":3,\"contrast\":2},"
    "\"shapes_two_bars\":{\"repeat\":3,\"contrast\":2,\"repeat_contrast\":2},"
    "\"plans_two\":{\"I-V\":3,\"I-IV\":1,\"I-I\":1},\"plans_three\":{\"I-V-I\":2,\"I-IV-I\":1},"
    "\"features\":{\"dotted\":1,\"leap\":1,\"repeat\":1},\"leap\":{\"low\":5,\"high\":9},"
    "\"rung_moves\":{\"step\":6,\"skip\":2},"
    "\"decorations\":{\"chord_tone\":1,\"passing\":1,\"neighbor\":1,\"appoggiatura\":1,\"escape\":1},"
    "\"durations\":{\"half_beat\":2,\"beat\":4,\"two_beats\":1,\"quarter_beats\":0,\"long_short\":0,\"short_long\":0,"
    "\"triplet\":0,\"triplet_two_beats\":0},\"accented_one_in\":0,"
    "\"accented_types\":{\"appoggiatura\":1,\"passing\":1,\"suspension\":1},\"avoid_repeats\":false,"
    "\"leaps_per_motif\":11,\"leap_recovery\":false,\"pickup_from_accent\":false,\"contrast_fidelity_min\":\"0/1\","
    "\"long_on_weak\":0,\"final_long\":false,\"compound_beats\":false,"
    "\"endings\":{\"free\":1,\"closed\":0,\"open\":0,\"lead_in\":0},\"long_nct_one_in\":0,"
    "\"long_nct_types\":{\"appoggiatura\":1,\"suspension\":1,\"retardation\":1,\"passing\":1,\"neighbor\":1},"
    "\"long_nct_intervals\":{\"second\":1,\"fourth\":1,\"sixth\":1,\"seventh\":1},"
    "\"long_nct_beats\":{\"downbeat\":1,\"middle\":1,\"other\":1},"
    "\"long_nct_lengths\":{\"beat\":1,\"dotted_beat\":1,\"two_beats\":1},\"syncopation_one_in\":0,"
    "\"syncopation_types\":{\"off_beat\":1,\"tie_beat\":1,\"tie_bar\":1,\"anticipation\":1},"
    "\"syncopation_levels\":{\"eighth\":1,\"sixteenth\":1},\"skips_per_motif\":11}";

static const char *DEFAULT_DEVELOPMENT =
    "{\"fidelity_minimum\":{"
    "\"melody\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"7/20\"},"
    "\"balanced\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"3/10\"},"
    "\"theme\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"1/5\"}},"
    "\"continuation\":{\"every_segment\":false,\"fragment_lengths\":{\"segment\":1,\"half_bar\":0,\"beat\":0},"
    "\"diminish\":false,\"liquidate\":false,\"faster_harmony\":false},\"basic_as_written\":false,"
    "\"contrast\":{\"distance\":{\"similar\":1,\"distant\":0},\"new_material\":0},"
    "\"functions\":{\"harmony\":false,\"surface\":false,\"contrast\":false}}";

static const char *DEFAULT_CANDIDATES = "{\"variety\":0,\"pool\":4}";

static bool read_text(const char *text, ComposerPreferences *out, char *path, char *message) {
    cJSON *object = cJSON_Parse(text);
    assert(object);
    bool ok = composer_preferences_read(object, out, path, COMPOSER_PREFERENCES_PATH_MAX, message,
                                        COMPOSER_PREFERENCES_MESSAGE_MAX);
    cJSON_Delete(object);
    return ok;
}

/* The text is refused at path, with a message. */
static void refused(const char *text, const char *path) {
    ComposerPreferences preferences;
    char found[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (read_text(text, &preferences, found, message) || strcmp(found, path) != 0 || message[0] == '\0') {
        fprintf(stderr, "%s: expected a refusal at %s, got path \"%s\" message \"%s\"\n", text, path, found, message);
        assert(false);
    }
    /* A refusal leaves the preferences usable: the defaults with whatever was read before it. */
    assert(preferences.melody.attempts >= 1);
}

/* The L4 members (phase L4): every one set reads back equal through write and read, each is a change from the defaults. */
static void l4_members_round_trip(void) {
    static const char *const TEXT =
        "{\"melody\":{\"closure\":{\"final\":20,\"first\":7},\"rhythm_repeat_bars\":3,\"leaps\":9,\"register\":20},"
        "\"development\":{\"contrast\":{\"distance\":{\"similar\":2,\"distant\":5},\"new_material\":4}}}";
    ComposerPreferences read;
    ComposerPreferences again;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text(TEXT, &read, path, message));
    assert(read.melody.closure_final == 20 && read.melody.closure_first == 7 && read.melody.rhythm_repeat_bars == 3 &&
           read.melody.leap_weight == 9 && read.melody.register_weight == 20);
    assert(read.contrast.distance[GROW_CONTRAST_SIMILAR] == 2 && read.contrast.distance[GROW_CONTRAST_DISTANT] == 5 &&
           read.contrast.new_material == 4);
    assert(!test_preferences_are_default(&read));
    cJSON *document = composer_preferences_write(&read);
    assert(document);
    assert(composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.melody.closure_final == 20 && again.melody.closure_first == 7 && again.melody.rhythm_repeat_bars == 3 &&
           again.melody.leap_weight == 9 && again.melody.register_weight == 20);
    assert(again.contrast.distance[GROW_CONTRAST_SIMILAR] == 2 && again.contrast.distance[GROW_CONTRAST_DISTANT] == 5 &&
           again.contrast.new_material == 4);
    cJSON *second = composer_preferences_write(&again);
    char *first_text = cJSON_PrintUnformatted(document);
    char *second_text = cJSON_PrintUnformatted(second);
    assert(first_text && second_text && strcmp(first_text, second_text) == 0);
    cJSON_free(first_text);
    cJSON_free(second_text);
    cJSON_Delete(second);
    cJSON_Delete(document);

    /* Each member alone is a change; a name left out keeps its default; distance all 0 reads as similar 1. */
    static const char *const ALONE[] = {"{\"melody\":{\"closure\":{\"final\":1}}}", "{\"melody\":{\"closure\":{\"first\":1}}}",
                                        "{\"melody\":{\"rhythm_repeat_bars\":16}}", "{\"melody\":{\"leaps\":1}}",
                                        "{\"melody\":{\"register\":1}}",
                                        "{\"development\":{\"contrast\":{\"distance\":{\"distant\":1}}}}",
                                        "{\"development\":{\"contrast\":{\"new_material\":1}}}"};
    for (size_t i = 0; i < sizeof(ALONE) / sizeof(ALONE[0]); i++) {
        assert(read_text(ALONE[i], &read, path, message));
        assert(!test_preferences_are_default(&read));
    }
    assert(read_text("{\"development\":{\"contrast\":{\"distance\":{\"similar\":0,\"distant\":0}}}}", &read, path, message));
    assert(read.contrast.distance[GROW_CONTRAST_SIMILAR] == 1 && read.contrast.distance[GROW_CONTRAST_DISTANT] == 0);
    assert(test_preferences_are_default(&read));
    assert(read_text("{\"melody\":{\"closure\":{\"first\":3}}}", &read, path, message));
    assert(read.melody.closure_final == 0 && read.melody.closure_first == 3);
}

static void defaults_round_trip(void) {
    ComposerPreferences defaults;
    ComposerPreferences read;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    composer_preferences_default(&defaults);
    assert(test_preferences_are_default(&defaults));

    cJSON *document = composer_preferences_write(&defaults);
    assert(document);
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "melody"));
    assert(text && strcmp(text, DEFAULT_MELODY) == 0);
    cJSON_free(text);
    text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "motif"));
    assert(text && strcmp(text, DEFAULT_MOTIF) == 0);
    cJSON_free(text);
    text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "development"));
    assert(text && strcmp(text, DEFAULT_DEVELOPMENT) == 0);
    cJSON_free(text);
    text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "candidates"));
    assert(text && strcmp(text, DEFAULT_CANDIDATES) == 0);
    cJSON_free(text);
    /* The whole document is the four groups, in this order. */
    text = cJSON_PrintUnformatted(document);
    char whole[8192];
    snprintf(whole, sizeof(whole), "{\"melody\":%s,\"motif\":%s,\"development\":%s,\"candidates\":%s}", DEFAULT_MELODY,
             DEFAULT_MOTIF, DEFAULT_DEVELOPMENT, DEFAULT_CANDIDATES);
    assert(text && strcmp(text, whole) == 0);
    cJSON_free(text);

    assert(composer_preferences_read(document, &read, path, sizeof(path), message, sizeof(message)));
    assert(test_preferences_are_default(&read));
    assert(read.melody.range_low == defaults.melody.range_low && read.melody.keep == defaults.melody.keep &&
           read.melody.nct_types == defaults.melody.nct_types);
    cJSON *again = composer_preferences_write(&read);
    char *first = cJSON_PrintUnformatted(document);
    char *second = cJSON_PrintUnformatted(again);
    assert(first && second && strcmp(first, second) == 0);
    cJSON_free(first);
    cJSON_free(second);
    cJSON_Delete(again);
    cJSON_Delete(document);

    /* The defaults are the constants the stage used before the settings existed. */
    assert(defaults.melody.range_low == 60 && defaults.melody.range_high == 84 && defaults.melody.climax_position == 667);
    assert(defaults.melody.climax_rise == 4 && defaults.melody.climax_slack == 150);
    assert(defaults.melody.second_nearest.num == 1 && defaults.melody.second_nearest.den == 3);
    assert(defaults.melody.attempts == 48 && defaults.melody.keep == 6 && defaults.melody.weights.seam == 1);
    assert(defaults.motif.pickup_one_in == 4 && defaults.motif.leap_low == 5 && defaults.motif.leap_high == 9);
    assert(defaults.motif.durations[0] == 2 && defaults.motif.durations[1] == 4 && defaults.motif.durations[2] == 1);
    assert(defaults.motif.accented_one_in == 0 && defaults.motif.accented_types[0] == 1 &&
           defaults.motif.accented_types[1] == 1 && defaults.motif.accented_types[2] == 1 &&
           !defaults.motif.avoid_repeats && defaults.motif.leaps_per_motif == 11 && !defaults.motif.leap_recovery &&
           !defaults.motif.pickup_from_accent && defaults.motif.contrast_fidelity_min.num == 0 &&
           defaults.motif.contrast_fidelity_min.den == 1 && defaults.motif.long_on_weak == 0 &&
           !defaults.motif.final_long);
    assert(defaults.development.minimum[GROW_DEVELOP_BALANCED][GROW_ROLE_PRESENTATION].numerator == 11 &&
           defaults.development.minimum[GROW_DEVELOP_BALANCED][GROW_ROLE_PRESENTATION].denominator == 20);
    assert(defaults.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_CONTRAST].numerator == 0);
}

static void members_read_over_defaults(void) {
    ComposerPreferences preferences;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text("{}", &preferences, path, message) && test_preferences_are_default(&preferences));
    assert(read_text("{\"melody\":{}}", &preferences, path, message) && test_preferences_are_default(&preferences));
    assert(read_text("{\"melody\":{\"keep\":3,\"attempts\":12,\"range\":{\"high\":90},\"density\":2,"
                     "\"nct_types\":[\"suspension\",\"passing\"],\"second_nearest_chord_tone\":\"1/2\","
                     "\"plain_shaping\":true,\"weights\":{\"waves\":0,\"central_range\":20}}}",
                     &preferences, path, message));
    assert(!test_preferences_are_default(&preferences));
    assert(preferences.melody.keep == 3 && preferences.melody.attempts == 12 && preferences.melody.density == 2);
    assert(preferences.melody.range_low == 60 && preferences.melody.range_high == 90);
    assert(preferences.melody.nct_types == (MELODY_NCT_SUSPENSION | MELODY_NCT_PASSING));
    assert(preferences.melody.second_nearest.num == 1 && preferences.melody.second_nearest.den == 2);
    assert(preferences.melody.plain_shaping && preferences.melody.weights.waves == 0 &&
           preferences.melody.weights.central_range == 20 && preferences.melody.weights.melodic == 1);

    /* What is read is written back as read. */
    cJSON *document = composer_preferences_write(&preferences);
    ComposerPreferences again;
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.melody.nct_types == preferences.melody.nct_types && again.melody.keep == 3 &&
           again.melody.weights.waves == 0 && again.melody.range_high == 90);
    cJSON_Delete(document);

    assert(read_text("{\"motif\":{\"pickup_one_in\":0,\"bars\":{\"two\":0},\"features\":{\"leap\":0,\"repeat\":0},"
                     "\"leap\":{\"low\":4,\"high\":4},\"plans_two\":{\"I-IV\":7}},"
                     "\"development\":{\"fidelity_minimum\":{\"theme\":{\"presentation\":\"1/1\"}}}}",
                     &preferences, path, message));
    assert(!test_preferences_are_default(&preferences));
    assert(preferences.motif.pickup_one_in == 0 && preferences.motif.bars[0] == 1 && preferences.motif.bars[1] == 0);
    assert(preferences.motif.features[0] == 1 && preferences.motif.features[1] == 0 && preferences.motif.features[2] == 0);
    assert(preferences.motif.leap_low == 4 && preferences.motif.leap_high == 4 && preferences.motif.plans_two[1] == 7 &&
           preferences.motif.plans_two[0] == 3);
    assert(preferences.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_PRESENTATION].numerator == 1 &&
           preferences.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_PRESENTATION].denominator == 1 &&
           preferences.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_CONTINUATION].numerator == 7 &&
           preferences.development.minimum[GROW_DEVELOP_BALANCED][GROW_ROLE_PRESENTATION].numerator == 11);
    document = composer_preferences_write(&preferences);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.motif.pickup_one_in == 0 && again.motif.leap_low == 4 && again.motif.plans_two[1] == 7 &&
           again.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_PRESENTATION].numerator == 1);
    cJSON_Delete(document);

    /* All nine listening settings set read back equal through write and read (R-422). */
    assert(read_text("{\"motif\":{\"accented_one_in\":3,\"accented_types\":{\"appoggiatura\":0,\"passing\":2,\"suspension\":5},"
                     "\"avoid_repeats\":true,\"leaps_per_motif\":2,\"leap_recovery\":true,\"pickup_from_accent\":true,"
                     "\"contrast_fidelity_min\":\"1/3\",\"long_on_weak\":4,\"final_long\":true}}",
                     &preferences, path, message));
    assert(!test_preferences_are_default(&preferences));
    assert(preferences.motif.accented_one_in == 3 && preferences.motif.accented_types[0] == 0 &&
           preferences.motif.accented_types[1] == 2 && preferences.motif.accented_types[2] == 5 &&
           preferences.motif.avoid_repeats && preferences.motif.leaps_per_motif == 2 && preferences.motif.leap_recovery &&
           preferences.motif.pickup_from_accent && preferences.motif.contrast_fidelity_min.num == 1 &&
           preferences.motif.contrast_fidelity_min.den == 3 && preferences.motif.long_on_weak == 4 &&
           preferences.motif.final_long);
    document = composer_preferences_write(&preferences);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.motif.accented_one_in == 3 && again.motif.accented_types[2] == 5 && again.motif.avoid_repeats &&
           again.motif.leaps_per_motif == 2 && again.motif.leap_recovery && again.motif.pickup_from_accent &&
           again.motif.contrast_fidelity_min.num == 1 && again.motif.contrast_fidelity_min.den == 3 &&
           again.motif.long_on_weak == 4 && again.motif.final_long);
    char *written = cJSON_PrintUnformatted(document);
    cJSON *rewritten = composer_preferences_write(&again);
    char *rewritten_text = cJSON_PrintUnformatted(rewritten);
    assert(written && rewritten_text && strcmp(written, rewritten_text) == 0);
    cJSON_free(written);
    cJSON_free(rewritten_text);
    cJSON_Delete(rewritten);
    cJSON_Delete(document);

    /* Every L2 setting set reads back equal through write and read (R-422, R-443). */
    assert(read_text("{\"motif\":{\"durations\":{\"half_beat\":1,\"beat\":1,\"two_beats\":1,\"quarter_beats\":2,"
                     "\"long_short\":3,\"short_long\":4,\"triplet\":5,\"triplet_two_beats\":6},"
                     "\"compound_beats\":true,\"endings\":{\"free\":0,\"closed\":2,\"open\":1,\"lead_in\":1},"
                     "\"long_nct_one_in\":7,\"long_nct_types\":{\"appoggiatura\":0,\"suspension\":2,\"retardation\":3,"
                     "\"passing\":4,\"neighbor\":5},\"long_nct_intervals\":{\"second\":2,\"fourth\":0,\"sixth\":3,"
                     "\"seventh\":4},\"long_nct_beats\":{\"downbeat\":0,\"middle\":6,\"other\":7},"
                     "\"long_nct_lengths\":{\"beat\":8,\"dotted_beat\":0,\"two_beats\":9}}}",
                     &preferences, path, message));
    assert(!test_preferences_are_default(&preferences));
    assert(preferences.motif.durations[3] == 2 && preferences.motif.durations[7] == 6 && preferences.motif.compound_beats &&
           preferences.motif.endings[0] == 0 && preferences.motif.endings[1] == 2 && preferences.motif.endings[3] == 1 &&
           preferences.motif.long_nct_one_in == 7 && preferences.motif.long_nct_types[0] == 0 &&
           preferences.motif.long_nct_types[4] == 5 && preferences.motif.long_nct_intervals[1] == 0 &&
           preferences.motif.long_nct_beats[2] == 7 && preferences.motif.long_nct_lengths[2] == 9);
    document = composer_preferences_write(&preferences);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(memcmp(&again.motif.durations, &preferences.motif.durations, sizeof(preferences.motif.durations)) == 0 &&
           again.motif.compound_beats && memcmp(again.motif.endings, preferences.motif.endings, sizeof(again.motif.endings)) == 0 &&
           again.motif.long_nct_one_in == 7 &&
           memcmp(again.motif.long_nct_types, preferences.motif.long_nct_types, sizeof(again.motif.long_nct_types)) == 0 &&
           memcmp(again.motif.long_nct_intervals, preferences.motif.long_nct_intervals,
                  sizeof(again.motif.long_nct_intervals)) == 0 &&
           memcmp(again.motif.long_nct_beats, preferences.motif.long_nct_beats, sizeof(again.motif.long_nct_beats)) == 0 &&
           memcmp(again.motif.long_nct_lengths, preferences.motif.long_nct_lengths, sizeof(again.motif.long_nct_lengths)) == 0);
    cJSON_Delete(document);
    /* Every L3 setting set reads back equal through write and read (R-422, R-453). */
    assert(read_text("{\"motif\":{\"syncopation_one_in\":3,\"syncopation_types\":{\"off_beat\":0,\"tie_beat\":2,\"tie_bar\":0,"
                     "\"anticipation\":5},\"syncopation_levels\":{\"eighth\":4,\"sixteenth\":0},\"skips_per_motif\":2,"
                     "\"plan\":[\"ii\",\"V@1:3\"]},\"development\":{\"continuation\":{\"every_segment\":true,"
                     "\"fragment_lengths\":{\"segment\":0,\"half_bar\":3,\"beat\":2},\"diminish\":true,\"liquidate\":true,"
                     "\"faster_harmony\":true},\"basic_as_written\":true}}",
                     &preferences, path, message));
    assert(!test_preferences_are_default(&preferences));
    assert(preferences.motif.syncopation_one_in == 3 && preferences.motif.syncopation_types[0] == 0 &&
           preferences.motif.syncopation_types[1] == 2 && preferences.motif.syncopation_types[3] == 5 &&
           preferences.motif.syncopation_levels[0] == 4 && preferences.motif.syncopation_levels[1] == 0 &&
           preferences.motif.skips_per_motif == 2 && preferences.motif.plan_count == 2 &&
           preferences.motif.plan[0].degree == 2 && preferences.motif.plan[0].bar == 0 &&
           preferences.motif.plan[1].degree == 5 && preferences.motif.plan[1].bar == 1 && preferences.motif.plan[1].beat == 3);
    assert(preferences.continuation.every_segment && preferences.continuation.fragment_lengths[0] == 0 &&
           preferences.continuation.fragment_lengths[1] == 3 && preferences.continuation.fragment_lengths[2] == 2 &&
           preferences.continuation.diminish && preferences.continuation.liquidate &&
           preferences.continuation.faster_harmony && preferences.basic_as_written);
    document = composer_preferences_write(&preferences);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.motif.plan_count == 2 && again.motif.plan[1].degree == 5 && again.motif.plan[1].beat == 3 &&
           again.motif.skips_per_motif == 2 && again.motif.syncopation_types[3] == 5 && again.basic_as_written &&
           again.continuation.fragment_lengths[2] == 2 && again.continuation.faster_harmony);
    written = cJSON_PrintUnformatted(document);
    rewritten = composer_preferences_write(&again);
    rewritten_text = cJSON_PrintUnformatted(rewritten);
    assert(written && rewritten_text && strcmp(written, rewritten_text) == 0);
    assert(strstr(written, "\"plan\":[\"II\",\"V@1:3\"]"));
    cJSON_free(written);
    cJSON_free(rewritten_text);
    cJSON_Delete(rewritten);
    cJSON_Delete(document);
    /* The default document has no plan; the places and marks read as the grammar says. */
    assert(read_text("{\"motif\":{\"plan\":[\"i\",\"V\"]}}", &preferences, path, message) && preferences.motif.plan_count == 2 &&
           preferences.motif.plan[0].degree == 1 && preferences.motif.plan[1].bar == 0);
    assert(read_text("{\"motif\":{\"plan\":[\"I@1\",\"vii\u00b0@2\",\"IVo@2:3\"]}}", &preferences, path, message) &&
           preferences.motif.plan[0].bar == 1 && preferences.motif.plan[0].beat == 1 && preferences.motif.plan[1].degree == 7 &&
           preferences.motif.plan[1].bar == 2 && preferences.motif.plan[1].beat == 1 && preferences.motif.plan[2].degree == 4 &&
           preferences.motif.plan[2].beat == 3);
    assert(read_text("{\"motif\":{\"plan\":[\"I\",\"V\",\"vi\",\"IV\"]}}", &preferences, path, message) == false);
    assert(read_text("{\"motif\":{\"plan\":[\"I\",\"V@1:3\",\"vi@2\",\"IV@2:4\"]}}", &preferences, path, message) &&
           preferences.motif.plan_count == 4);

    /* The first three durations still read as they did, the others keep their defaults. */
    assert(read_text("{\"motif\":{\"durations\":{\"half_beat\":1,\"beat\":2,\"two_beats\":3}}}", &preferences, path, message));
    assert(preferences.motif.durations[0] == 1 && preferences.motif.durations[1] == 2 && preferences.motif.durations[2] == 3 &&
           preferences.motif.durations[3] == 0 && preferences.motif.durations[7] == 0);

    /* A single setting equal to its default is still the defaults. */
    assert(read_text("{\"melody\":{\"keep\":6}}", &preferences, path, message) && test_preferences_are_default(&preferences));
}

static void limits_are_refused(void) {
    refused("{\"melody\":{\"keep\":17}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":0}}", "/melody/keep");
    refused("{\"melody\":{\"attempts\":4,\"keep\":5}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":5,\"attempts\":4}}", "/melody/keep");
    refused("{\"melody\":{\"attempts\":257}}", "/melody/attempts");
    refused("{\"melody\":{\"keep\":2.5}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":\"6\"}}", "/melody/keep");
    refused("{\"melody\":{\"range\":{\"high\":71}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"low\":80}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"low\":35}}}", "/melody/range/low");
    refused("{\"melody\":{\"range\":{\"low\":97,\"high\":108}}}", "/melody/range/low");
    refused("{\"melody\":{\"range\":{\"high\":109}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"mid\":70}}}", "/melody/range/mid");
    refused("{\"melody\":{\"nct_types\":[\"passing\",\"escape\"]}}", "/melody/nct_types");
    refused("{\"melody\":{\"nct_types\":[\"passing\",\"passing\"]}}", "/melody/nct_types");
    refused("{\"melody\":{\"nct_types\":\"passing\"}}", "/melody/nct_types");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"4/3\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"1/17\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"1/0\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"one third\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"density\":5}}", "/melody/density");
    refused("{\"melody\":{\"climax_position\":1001}}", "/melody/climax_position");
    refused("{\"melody\":{\"climax_slack\":1001}}", "/melody/climax_slack");
    refused("{\"melody\":{\"climax_rise\":0}}", "/melody/climax_rise");
    refused("{\"melody\":{\"climax_rise\":13}}", "/melody/climax_rise");
    refused("{\"melody\":{\"plain_shaping\":1}}", "/melody/plain_shaping");
    refused("{\"melody\":{\"weights\":{\"waves\":21}}}", "/melody/weights/waves");
    refused("{\"melody\":{\"weights\":{\"waves\":-1}}}", "/melody/weights/waves");
    refused("{\"melody\":{\"weights\":{\"bogus\":1}}}", "/melody/weights/bogus");
    refused("{\"melody\":{\"bogus\":1}}", "/melody/bogus");
    refused("{\"melody\":{\"rhythm_repeat_bars\":17}}", "/melody/rhythm_repeat_bars");
    refused("{\"melody\":{\"closure\":{\"final\":21}}}", "/melody/closure/final");
    refused("{\"melody\":{\"closure\":{\"first\":-1}}}", "/melody/closure/first");
    refused("{\"melody\":{\"closure\":{\"last\":1}}}", "/melody/closure/last");
    refused("{\"melody\":{\"closure\":5}}", "/melody/closure");
    refused("{\"melody\":{\"leaps\":21}}", "/melody/leaps");
    refused("{\"melody\":{\"register\":21}}", "/melody/register");
    refused("{\"development\":{\"contrast\":{\"new_material\":21}}}", "/development/contrast/new_material");
    refused("{\"development\":{\"contrast\":{\"distance\":{\"distant\":21}}}}", "/development/contrast/distance/distant");
    refused("{\"development\":{\"contrast\":{\"distance\":{\"similar\":21}}}}", "/development/contrast/distance/similar");
    refused("{\"development\":{\"contrast\":{\"distance\":{\"far\":1}}}}", "/development/contrast/distance/far");
    refused("{\"development\":{\"contrast\":{\"bogus\":1}}}", "/development/contrast/bogus");
    refused("{\"development\":{\"contrast\":1}}", "/development/contrast");
    refused("{\"melody\":5}", "/melody");
    refused("{\"motif\":5}", "/motif");
    refused("{\"motif\":{\"bars\":{\"one\":0,\"two\":0}}}", "/motif/bars");
    refused("{\"motif\":{\"bars\":{\"one\":21}}}", "/motif/bars/one");
    refused("{\"motif\":{\"bars\":{\"three\":1}}}", "/motif/bars/three");
    refused("{\"motif\":{\"bars\":1}}", "/motif/bars");
    refused("{\"motif\":{\"shapes_one_bar\":{\"repeat\":0,\"contrast\":0}}}", "/motif/shapes_one_bar");
    refused("{\"motif\":{\"shapes_two_bars\":{\"repeat_contrast\":-1}}}", "/motif/shapes_two_bars/repeat_contrast");
    refused("{\"motif\":{\"plans_two\":{\"I-V\":0,\"I-IV\":0,\"I-I\":0}}}", "/motif/plans_two");
    refused("{\"motif\":{\"plans_three\":{\"V-I-V\":1}}}", "/motif/plans_three/V-I-V");
    refused("{\"motif\":{\"features\":{\"dotted\":0,\"leap\":0,\"repeat\":0}}}", "/motif/features");
    refused("{\"motif\":{\"decorations\":{\"suspension\":1}}}", "/motif/decorations/suspension");
    refused("{\"motif\":{\"durations\":{\"half_beat\":0,\"beat\":0,\"two_beats\":0}}}", "/motif/durations");
    refused("{\"motif\":{\"rung_moves\":{\"step\":0,\"skip\":0}}}", "/motif/rung_moves");
    refused("{\"motif\":{\"durations\":{\"quintuplet\":1}}}", "/motif/durations/quintuplet");
    refused("{\"motif\":{\"long_nct_one_in\":65}}", "/motif/long_nct_one_in");
    refused("{\"motif\":{\"long_nct_one_in\":-1}}", "/motif/long_nct_one_in");
    refused("{\"motif\":{\"endings\":{\"free\":0,\"closed\":0,\"open\":0,\"lead_in\":0}}}", "/motif/endings");
    refused("{\"motif\":{\"endings\":{\"half\":1}}}", "/motif/endings/half");
    refused("{\"motif\":{\"long_nct_types\":{\"appoggiatura\":0,\"suspension\":0,\"retardation\":0,\"passing\":0,"
            "\"neighbor\":0}}}",
            "/motif/long_nct_types");
    refused("{\"motif\":{\"long_nct_intervals\":{\"second\":0,\"fourth\":0,\"sixth\":0,\"seventh\":0}}}",
            "/motif/long_nct_intervals");
    refused("{\"motif\":{\"long_nct_beats\":{\"downbeat\":0,\"middle\":0,\"other\":0}}}", "/motif/long_nct_beats");
    refused("{\"motif\":{\"long_nct_lengths\":{\"beat\":21}}}", "/motif/long_nct_lengths/beat");
    refused("{\"motif\":{\"compound_beats\":1}}", "/motif/compound_beats");
    refused("{\"motif\":{\"pickup_one_in\":65}}", "/motif/pickup_one_in");
    refused("{\"motif\":{\"pickup_one_in\":-1}}", "/motif/pickup_one_in");
    refused("{\"motif\":{\"leap\":{\"low\":8,\"high\":6}}}", "/motif/leap/high");
    refused("{\"motif\":{\"leap\":{\"high\":4}}}", "/motif/leap/high");
    refused("{\"motif\":{\"leap\":{\"low\":2}}}", "/motif/leap/low");
    refused("{\"motif\":{\"leap\":{\"high\":13}}}", "/motif/leap/high");
    refused("{\"motif\":{\"leap\":{\"mid\":6}}}", "/motif/leap/mid");
    refused("{\"motif\":{\"accented_one_in\":65}}", "/motif/accented_one_in");
    refused("{\"motif\":{\"accented_types\":{\"appoggiatura\":0,\"passing\":0,\"suspension\":0}}}", "/motif/accented_types");
    refused("{\"motif\":{\"accented_types\":{\"escape\":1}}}", "/motif/accented_types/escape");
    refused("{\"motif\":{\"leaps_per_motif\":12}}", "/motif/leaps_per_motif");
    refused("{\"motif\":{\"long_on_weak\":21}}", "/motif/long_on_weak");
    refused("{\"motif\":{\"avoid_repeats\":1}}", "/motif/avoid_repeats");
    refused("{\"motif\":{\"leap_recovery\":\"yes\"}}", "/motif/leap_recovery");
    refused("{\"motif\":{\"pickup_from_accent\":null}}", "/motif/pickup_from_accent");
    refused("{\"motif\":{\"final_long\":0}}", "/motif/final_long");
    refused("{\"motif\":{\"contrast_fidelity_min\":\"3/2\"}}", "/motif/contrast_fidelity_min");
    refused("{\"motif\":{\"contrast_fidelity_min\":\"1/1001\"}}", "/motif/contrast_fidelity_min");
    refused("{\"motif\":{\"contrast_fidelity_min\":0.5}}", "/motif/contrast_fidelity_min");
    refused("{\"motif\":{\"bogus\":1}}", "/motif/bogus");
    refused("{\"development\":5}", "/development");
    refused("{\"development\":{\"bogus\":{}}}", "/development/bogus");
    refused("{\"development\":{\"fidelity_minimum\":{\"theme\":{\"cadential\":\"3/2\"}}}}",
            "/development/fidelity_minimum/theme/cadential");
    refused("{\"development\":{\"fidelity_minimum\":{\"theme\":{\"cadential\":\"1/1001\"}}}}",
            "/development/fidelity_minimum/theme/cadential");
    refused("{\"development\":{\"fidelity_minimum\":{\"theme\":{\"cadential\":0.5}}}}",
            "/development/fidelity_minimum/theme/cadential");
    refused("{\"development\":{\"fidelity_minimum\":{\"theme\":{\"contrast\":\"0/1\"}}}}",
            "/development/fidelity_minimum/theme/contrast");
    refused("{\"development\":{\"fidelity_minimum\":{\"contrast\":{}}}}", "/development/fidelity_minimum/contrast");
    refused("{\"development\":{\"fidelity_minimum\":{\"theme\":5}}}", "/development/fidelity_minimum/theme");
    refused("{\"motif\":{\"syncopation_one_in\":65}}", "/motif/syncopation_one_in");
    refused("{\"motif\":{\"skips_per_motif\":12}}", "/motif/skips_per_motif");
    refused("{\"motif\":{\"syncopation_types\":{\"off_beat\":0,\"tie_beat\":0,\"tie_bar\":0,\"anticipation\":0}}}",
            "/motif/syncopation_types");
    refused("{\"motif\":{\"syncopation_types\":{\"slide\":1}}}", "/motif/syncopation_types/slide");
    refused("{\"motif\":{\"syncopation_levels\":{\"eighth\":21}}}", "/motif/syncopation_levels/eighth");
    refused("{\"motif\":{\"plan\":[]}}", "/motif/plan");
    refused("{\"motif\":{\"plan\":\"I\"}}", "/motif/plan");
    refused("{\"motif\":{\"plan\":[\"I\",\"V\",\"I\",\"V\",\"I\"]}}", "/motif/plan");
    refused("{\"motif\":{\"plan\":[\"VIII\"]}}", "/motif/plan/0");
    refused("{\"motif\":{\"plan\":[\"I\",5]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V:3\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@2:\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@3\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@1:0\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@1:13\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I@1:2\",\"V@2\"]}}", "/motif/plan/0");
    refused("{\"motif\":{\"plan\":[\"I@2\",\"V@2:2\"]}}", "/motif/plan/0");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@1:3\",\"I\"]}}", "/motif/plan/2");
    refused("{\"motif\":{\"plan\":[\"I\",\"V\",\"I@2\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@1:1\"]}}", "/motif/plan/1");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@1:3\",\"I@1:3\"]}}", "/motif/plan/2");
    refused("{\"motif\":{\"plan\":[\"I\",\"V@2:1\",\"I@1:3\"]}}", "/motif/plan/2");
    refused("{\"motif\":{\"plan\":[\"I\",\"V\",\"vi\",\"IV\"]}}", "/motif/plan");
    refused("{\"development\":{\"continuation\":5}}", "/development/continuation");
    refused("{\"development\":{\"continuation\":{\"bogus\":true}}}", "/development/continuation/bogus");
    refused("{\"development\":{\"continuation\":{\"diminish\":1}}}", "/development/continuation/diminish");
    refused("{\"development\":{\"continuation\":{\"every_segment\":null}}}", "/development/continuation/every_segment");
    refused("{\"development\":{\"continuation\":{\"fragment_lengths\":{\"segment\":0}}}}",
            "/development/continuation/fragment_lengths");
    refused("{\"development\":{\"continuation\":{\"fragment_lengths\":{\"beat\":21}}}}",
            "/development/continuation/fragment_lengths/beat");
    refused("{\"development\":{\"continuation\":{\"fragment_lengths\":{\"bar\":1}}}}",
            "/development/continuation/fragment_lengths/bar");
    refused("{\"development\":{\"continuation\":{\"fragment_lengths\":3}}}", "/development/continuation/fragment_lengths");
    refused("{\"development\":{\"basic_as_written\":\"yes\"}}", "/development/basic_as_written");
    refused("{\"bogus\":{}}", "/bogus");

    ComposerPreferences preferences;
    cJSON *list = cJSON_CreateArray();
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    assert(!composer_preferences_read(list, &preferences, path, sizeof(path), NULL, 0) && path[0] == '\0');
    assert(!composer_preferences_read(NULL, &preferences, NULL, 0, NULL, 0));
    cJSON_Delete(list);
}

/* The dotted keys of the document's leaves (objects descended into, lists and values as leaves), in document order. */
static size_t document_leaves(const cJSON *object, const char *prefix, char leaves[][96], size_t count) {
    for (const cJSON *item = object->child; item; item = item->next) {
        char key[96];
        snprintf(key, sizeof(key), "%s%s%s", prefix, prefix[0] ? "." : "", item->string);
        if (cJSON_IsObject(item)) {
            count = document_leaves(item, key, leaves, count);
        } else {
            assert(count < 200);
            snprintf(leaves[count++], 96, "%s", key);
        }
    }
    return count;
}

/* Every leaf of the default document has a key row, in order; the rows are the leaves plus motif.plan; the rules are the enums. */
static void key_and_rule_tables(void) {
    static char leaves[200][96];
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    cJSON *document = composer_preferences_write(&defaults);
    assert(document);
    size_t leaf_count = document_leaves(document, "", leaves, 0);
    cJSON_Delete(document);
    size_t key_count = 0;
    const ComposerPreferenceKey *keys = composer_preferences_keys(&key_count);
    assert(keys && leaf_count > 0 && key_count == leaf_count + 1); /* every leaf has its key (checked one by one below), and the plan is the one more */
    size_t at = 0;
    for (size_t i = 0; i < key_count; i++) {
        if (strcmp(keys[i].key, "motif.plan") == 0) {
            assert(keys[i].type == COMPOSER_KEY_PLAN && keys[i].hard && i > 0 && strcmp(keys[i - 1].key, "motif.skips_per_motif") == 0);
            continue;
        }
        assert(at < leaf_count && strcmp(keys[i].key, leaves[at]) == 0);
        at++;
        assert(keys[i].type != COMPOSER_KEY_WORDS || (keys[i].words && keys[i].words[0]));
        assert(keys[i].type != COMPOSER_KEY_INTEGER || keys[i].min <= keys[i].max);
    }
    assert(at == leaf_count);
    /* The hard keys are the ones R-671 names; the weights and every other preference are soft. */
    size_t hard = 0;
    for (size_t i = 0; i < key_count; i++) {
        hard += keys[i].hard;
    }
    assert(hard == 11 + 9);
    for (size_t i = 0; i < key_count; i++) {
        if (strcmp(keys[i].key, "melody.waive") == 0) {
            for (unsigned rule = 0; rule < MELODY_RULE_COUNT; rule++) {
                assert(keys[i].words[rule] && strcmp(keys[i].words[rule], melody_hard_rule_name((MelodyHardRule)rule)) == 0);
            }
            assert(keys[i].words[MELODY_RULE_COUNT] == NULL && keys[i].hard);
        } else if (strcmp(keys[i].key, "melody.reverse") == 0) {
            for (unsigned group = 0; group < MELODY_GROUP_COUNT; group++) {
                assert(keys[i].words[group] && strcmp(keys[i].words[group], melody_soft_group_name((MelodySoftGroup)group)) == 0);
            }
            assert(keys[i].words[MELODY_GROUP_COUNT] == NULL && !keys[i].hard);
        }
    }
    size_t rule_count = 0;
    const ComposerRule *rules = composer_preferences_rules(&rule_count);
    assert(rules && rule_count == MELODY_GROUP_COUNT + MELODY_RULE_COUNT);
    for (unsigned group = 0; group < MELODY_GROUP_COUNT; group++) {
        assert(!rules[group].hard && strcmp(rules[group].name, melody_soft_group_name((MelodySoftGroup)group)) == 0);
    }
    for (unsigned rule = 0; rule < MELODY_RULE_COUNT; rule++) {
        const ComposerRule *row = &rules[MELODY_GROUP_COUNT + rule];
        assert(row->hard && strcmp(row->name, melody_hard_rule_name((MelodyHardRule)rule)) == 0 && strcmp(row->key, "melody.waive") == 0);
    }
    /* Every rule's key is a row of the key table, or the group of rows under it. */
    for (size_t i = 0; i < rule_count; i++) {
        bool found = false;
        for (size_t k = 0; k < key_count; k++) {
            size_t length = strlen(rules[i].key);
            /* A rule's key is a row, or the group of rows under it (melody.closure holds final and first). */
            found = found || strcmp(keys[k].key, rules[i].key) == 0 ||
                    (strncmp(keys[k].key, rules[i].key, length) == 0 && keys[k].key[length] == '.');
        }
        assert(found);
    }
}

/* waive and reverse: read, written back as read, each a change from the defaults, and refused with a path and the names. */
static void waive_and_reverse(void) {
    ComposerPreferences read;
    ComposerPreferences again;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text("{\"melody\":{\"waive\":[\"parallel-fifths\",\"strong-beat-chord-tone\"],\"reverse\":[\"leaps\",\"melodic\"]}}", &read,
                     path, message));
    assert(read.melody.waive == ((1u << MELODY_RULE_PARALLEL_FIFTHS) | (1u << MELODY_RULE_STRONG_BEAT_CHORD_TONE)));
    assert(read.melody.reverse == ((1u << MELODY_GROUP_LEAPS) | (1u << MELODY_GROUP_MELODIC)));
    assert(!test_preferences_are_default(&read));
    cJSON *document = composer_preferences_write(&read);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.melody.waive == read.melody.waive && again.melody.reverse == read.melody.reverse);
    /* Written in the order of the enums, whatever order they were read in. */
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "melody"));
    assert(text && strstr(text, "\"waive\":[\"parallel-fifths\",\"strong-beat-chord-tone\"],\"reverse\":[\"melodic\",\"leaps\"]"));
    cJSON_free(text);
    cJSON_Delete(document);
    assert(read_text("{\"melody\":{\"waive\":[\"no-crossing\"]}}", &read, path, message) && !test_preferences_are_default(&read));
    assert(read_text("{\"melody\":{\"reverse\":[\"seam\"]}}", &read, path, message) && !test_preferences_are_default(&read));
    assert(read_text("{\"melody\":{\"waive\":[],\"reverse\":[]}}", &read, path, message) && test_preferences_are_default(&read));

    refused("{\"melody\":{\"waive\":[\"range\"]}}", "/melody/waive/0");
    refused("{\"melody\":{\"waive\":[\"no-crossing\",\"no-crossing\"]}}", "/melody/waive/1");
    refused("{\"melody\":{\"waive\":\"no-crossing\"}}", "/melody/waive");
    refused("{\"melody\":{\"waive\":[3]}}", "/melody/waive/0");
    refused("{\"melody\":{\"reverse\":[\"waves\",\"nothing\"]}}", "/melody/reverse/1");
    refused("{\"melody\":{\"reverse\":[\"leaps\",\"leaps\"]}}", "/melody/reverse/1");
    refused("{\"melody\":{\"reverse\":{}}}", "/melody/reverse");
    assert(!read_text("{\"melody\":{\"waive\":[\"range\"]}}", &read, path, message));
    assert(strstr(message, "is not a rule the melody stage can waive: forbidden-leap, forbidden-outline, no-crossing"));
    assert(!read_text("{\"melody\":{\"reverse\":[\"range\"]}}", &read, path, message));
    assert(strstr(message, "is not a soft rule group: melodic, against_bass") && strstr(message, "leaps, register"));
}

/* The candidates group (R-673): read, written back as read, each member a change, and refused at its path. */
static void candidates_group(void) {
    ComposerPreferences read;
    ComposerPreferences again;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text("{\"candidates\":{\"variety\":12,\"pool\":7}}", &read, path, message));
    assert(read.candidates.variety == 12 && read.candidates.pool == 7 && !test_preferences_are_default(&read));
    cJSON *document = composer_preferences_write(&read);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.candidates.variety == 12 && again.candidates.pool == 7);
    cJSON_Delete(document);
    assert(read_text("{\"candidates\":{\"variety\":1}}", &read, path, message) && read.candidates.pool == 4 &&
           !test_preferences_are_default(&read));
    assert(read_text("{\"candidates\":{\"pool\":2}}", &read, path, message) && !test_preferences_are_default(&read));
    assert(read_text("{\"candidates\":{\"variety\":0,\"pool\":4}}", &read, path, message) && test_preferences_are_default(&read));
    refused("{\"candidates\":{\"variety\":21}}", "/candidates/variety");
    refused("{\"candidates\":{\"pool\":1}}", "/candidates/pool");
    refused("{\"candidates\":{\"pool\":9}}", "/candidates/pool");
    refused("{\"candidates\":{\"extra\":1}}", "/candidates/extra");
    refused("{\"candidates\":3}", "/candidates");
}

int main(void) {
    defaults_round_trip();
    l4_members_round_trip();
    members_read_over_defaults();
    limits_are_refused();
    key_and_rule_tables();
    waive_and_reverse();
    candidates_group();
    printf("test_preferences: ok\n");
    return 0;
}
