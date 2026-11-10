#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/preferences.h>

/*
 * The preferences document (R-411): the defaults written and read back, the melody part of the document in the shape
 * docs/preferences.md gives, members read over the defaults, and every melody limit refused with its path.
 */

static const char *DEFAULT_MELODY =
    "{\"range\":{\"low\":60,\"high\":84},\"nct_types\":[\"passing\",\"neighbor\"],\"density\":0,\"climax_position\":667,"
    "\"climax_rise\":4,\"climax_slack\":150,\"second_nearest_chord_tone\":\"1/3\",\"attempts\":48,\"keep\":6,"
    "\"plain_shaping\":false,\"weights\":{\"melodic\":1,\"against_bass\":1,\"tendency\":1,\"climax\":1,\"contour\":1,"
    "\"ending_reduction\":1,\"waves\":1,\"central_range\":1,\"monotony\":1,\"seam\":1}}";

static const char *DEFAULT_MOTIF =
    "{\"bars\":{\"one\":1,\"two\":1},\"pickup_one_in\":4,\"shapes_one_bar\":{\"repeat\":3,\"contrast\":2},"
    "\"shapes_two_bars\":{\"repeat\":3,\"contrast\":2,\"repeat_contrast\":2},"
    "\"plans_two\":{\"I-V\":3,\"I-IV\":1,\"I-I\":1},\"plans_three\":{\"I-V-I\":2,\"I-IV-I\":1},"
    "\"features\":{\"dotted\":1,\"leap\":1,\"repeat\":1},\"leap\":{\"low\":5,\"high\":9},"
    "\"rung_moves\":{\"step\":6,\"skip\":2},"
    "\"decorations\":{\"chord_tone\":1,\"passing\":1,\"neighbor\":1,\"appoggiatura\":1,\"escape\":1},"
    "\"durations\":{\"half_beat\":2,\"beat\":4,\"two_beats\":1},\"accented_one_in\":0,"
    "\"accented_types\":{\"appoggiatura\":1,\"passing\":1,\"suspension\":1},\"avoid_repeats\":false,"
    "\"leaps_per_motif\":11,\"leap_recovery\":false,\"pickup_from_accent\":false,\"contrast_fidelity_min\":\"0/1\","
    "\"long_on_weak\":0,\"final_long\":false}";

static const char *DEFAULT_DEVELOPMENT =
    "{\"fidelity_minimum\":{"
    "\"melody\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"7/20\"},"
    "\"balanced\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"3/10\"},"
    "\"theme\":{\"presentation\":\"11/20\",\"continuation\":\"7/20\",\"cadential\":\"1/5\"}}}";

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

static void defaults_round_trip(void) {
    ComposerPreferences defaults;
    ComposerPreferences read;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    composer_preferences_default(&defaults);
    assert(composer_preferences_is_default(&defaults));

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
    /* The whole document is the three groups, in this order. */
    text = cJSON_PrintUnformatted(document);
    char whole[4096];
    snprintf(whole, sizeof(whole), "{\"melody\":%s,\"motif\":%s,\"development\":%s}", DEFAULT_MELODY, DEFAULT_MOTIF,
             DEFAULT_DEVELOPMENT);
    assert(text && strcmp(text, whole) == 0);
    cJSON_free(text);

    assert(composer_preferences_read(document, &read, path, sizeof(path), message, sizeof(message)));
    assert(composer_preferences_is_default(&read));
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
    assert(defaults.melody.second_nearest.numerator == 1 && defaults.melody.second_nearest.denominator == 3);
    assert(defaults.melody.attempts == 48 && defaults.melody.keep == 6 && defaults.melody.weights.seam == 1);
    assert(defaults.motif.pickup_one_in == 4 && defaults.motif.leap_low == 5 && defaults.motif.leap_high == 9);
    assert(defaults.motif.durations[0] == 2 && defaults.motif.durations[1] == 4 && defaults.motif.durations[2] == 1);
    assert(defaults.motif.accented_one_in == 0 && defaults.motif.accented_types[0] == 1 &&
           defaults.motif.accented_types[1] == 1 && defaults.motif.accented_types[2] == 1 &&
           !defaults.motif.avoid_repeats && defaults.motif.leaps_per_motif == 11 && !defaults.motif.leap_recovery &&
           !defaults.motif.pickup_from_accent && defaults.motif.contrast_fidelity_min.numerator == 0 &&
           defaults.motif.contrast_fidelity_min.denominator == 1 && defaults.motif.long_on_weak == 0 &&
           !defaults.motif.final_long);
    assert(defaults.development.minimum[GROW_DEVELOP_BALANCED][GROW_ROLE_PRESENTATION].numerator == 11 &&
           defaults.development.minimum[GROW_DEVELOP_BALANCED][GROW_ROLE_PRESENTATION].denominator == 20);
    assert(defaults.development.minimum[GROW_DEVELOP_THEME][GROW_ROLE_CONTRAST].numerator == 0);
}

static void members_read_over_defaults(void) {
    ComposerPreferences preferences;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text("{}", &preferences, path, message) && composer_preferences_is_default(&preferences));
    assert(read_text("{\"melody\":{}}", &preferences, path, message) && composer_preferences_is_default(&preferences));
    assert(read_text("{\"melody\":{\"keep\":3,\"attempts\":12,\"range\":{\"high\":90},\"density\":2,"
                     "\"nct_types\":[\"suspension\",\"passing\"],\"second_nearest_chord_tone\":\"1/2\","
                     "\"plain_shaping\":true,\"weights\":{\"waves\":0,\"central_range\":20}}}",
                     &preferences, path, message));
    assert(!composer_preferences_is_default(&preferences));
    assert(preferences.melody.keep == 3 && preferences.melody.attempts == 12 && preferences.melody.density == 2);
    assert(preferences.melody.range_low == 60 && preferences.melody.range_high == 90);
    assert(preferences.melody.nct_types == (MELODY_NCT_SUSPENSION | MELODY_NCT_PASSING));
    assert(preferences.melody.second_nearest.numerator == 1 && preferences.melody.second_nearest.denominator == 2);
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
    assert(!composer_preferences_is_default(&preferences));
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
    assert(!composer_preferences_is_default(&preferences));
    assert(preferences.motif.accented_one_in == 3 && preferences.motif.accented_types[0] == 0 &&
           preferences.motif.accented_types[1] == 2 && preferences.motif.accented_types[2] == 5 &&
           preferences.motif.avoid_repeats && preferences.motif.leaps_per_motif == 2 && preferences.motif.leap_recovery &&
           preferences.motif.pickup_from_accent && preferences.motif.contrast_fidelity_min.numerator == 1 &&
           preferences.motif.contrast_fidelity_min.denominator == 3 && preferences.motif.long_on_weak == 4 &&
           preferences.motif.final_long);
    document = composer_preferences_write(&preferences);
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.motif.accented_one_in == 3 && again.motif.accented_types[2] == 5 && again.motif.avoid_repeats &&
           again.motif.leaps_per_motif == 2 && again.motif.leap_recovery && again.motif.pickup_from_accent &&
           again.motif.contrast_fidelity_min.numerator == 1 && again.motif.contrast_fidelity_min.denominator == 3 &&
           again.motif.long_on_weak == 4 && again.motif.final_long);
    char *written = cJSON_PrintUnformatted(document);
    cJSON *rewritten = composer_preferences_write(&again);
    char *rewritten_text = cJSON_PrintUnformatted(rewritten);
    assert(written && rewritten_text && strcmp(written, rewritten_text) == 0);
    cJSON_free(written);
    cJSON_free(rewritten_text);
    cJSON_Delete(rewritten);
    cJSON_Delete(document);

    /* A single setting equal to its default is still the defaults. */
    assert(read_text("{\"melody\":{\"keep\":6}}", &preferences, path, message) && composer_preferences_is_default(&preferences));
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
    refused("{\"bogus\":{}}", "/bogus");

    ComposerPreferences preferences;
    cJSON *list = cJSON_CreateArray();
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    assert(!composer_preferences_read(list, &preferences, path, sizeof(path), NULL, 0) && path[0] == '\0');
    assert(!composer_preferences_read(NULL, &preferences, NULL, 0, NULL, 0));
    cJSON_Delete(list);
}

int main(void) {
    defaults_round_trip();
    members_read_over_defaults();
    limits_are_refused();
    printf("test_preferences: ok\n");
    return 0;
}
