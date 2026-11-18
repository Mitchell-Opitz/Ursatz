#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <theory/harmony/harmony_related_key.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece_plan_keys.h>

#include "support/test_knowledge.h"
#include "ursatz_test.h"
#include "util/request_parse.h"

/*
 * The Composer's knowledge: a key text is a tonic and the word of a material the snapshot knows, and the Composer writes
 * only the major and the natural minor; every entry that composes or reads takes a call context with a snapshot; and the key of a
 * form's region is the key its relation entry resolves, the closely related key standard-harmony names.
 */

static void test_key_words(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    AnalysisKey key;
    char message[160];

    /* A material the Composer does not compose in is refused, by one check, with the word it was written with. */
    static const char *const REFUSED[] = {"C dorian", "C harmonic minor", "C melodic minor", "C lydian"};
    static const char *const WORDS[] = {"dorian", "harmonic minor", "melodic minor", "lydian"};
    for (size_t i = 0; i < sizeof(REFUSED) / sizeof(REFUSED[0]); i++) {
        memset(message, 0, sizeof(message));
        assert(!request_parse_key(REFUSED[i], knowledge, &key));
        assert(request_key_refusal(REFUSED[i], knowledge, message, sizeof(message)));
        char expected[160];
        snprintf(expected, sizeof(expected), "the Composer writes major and minor keys; '%s' is not one of them", WORDS[i]);
        assert(strcmp(message, expected) == 0);
    }

    /* The major and the natural minor are accepted, in any case; the key carries the snapshot's material. */
    assert(request_parse_key("C minor", knowledge, &key));
    assert(analysis_key_tonic_pitch_class(&key) == 0 && key.material == test_materials()->natural_minor);
    assert(request_parse_key("c MINOR", knowledge, &key));
    assert(analysis_key_tonic_pitch_class(&key) == 0 && key.material == test_materials()->natural_minor);
    assert(request_parse_key("F# major", knowledge, &key));
    assert(analysis_key_tonic_pitch_class(&key) == 6 && key.material == test_materials()->major);
    assert(request_parse_key("G natural minor", knowledge, &key));
    assert(analysis_key_tonic_pitch_class(&key) == 7 && key.material == test_materials()->natural_minor);
    assert(!request_key_refusal("C minor", knowledge, message, sizeof(message)));

    /* Text that is no key, a word that names no material, and no snapshot are refused as "unknown key", not as a material. */
    assert(!request_parse_key("C", knowledge, &key) && !request_parse_key("H minor", knowledge, &key));
    assert(!request_parse_key("C nonsense", knowledge, &key) && !request_key_refusal("C nonsense", knowledge, message, sizeof(message)));
    assert(!request_parse_key("C minor", NULL, &key) && !request_key_refusal("C dorian", NULL, message, sizeof(message)));

    /* The test helper builds the same key from the word. */
    AnalysisKey built = test_key(9, "minor");
    assert(analysis_key_tonic_pitch_class(&built) == 9 && built.material == test_materials()->natural_minor);
    assert(analysis_key_equals(&built, &(AnalysisKey){NULL, PITCH_LETTER_A, 0, test_materials()->natural_minor}));
}

static void expect_refused(ComposerStatus status, const ComposerResponse *response) {
    assert(status == COMPOSER_ERR_REQUEST);
    assert(strcmp(response->error.message, "a knowledge snapshot is required") == 0);
}

static void test_context_required(void) {
    static const char REQUEST[] = "{\"input\": {\"text\": \"D4:q F4:q A4:q\"}, \"meter\": \"4/4\"}";
    const ComposerContext empty = {NULL};
    ComposerResponse response;

    /* A NULL context, and a context with no snapshot, are refused by every entry that composes or reads. */
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_motive(NULL, REQUEST, NULL, &response), &response);
    composer_response_destroy(&response);
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_motive(&empty, REQUEST, NULL, &response), &response);
    composer_response_destroy(&response);
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_random_motive(NULL, "{\"mode\": \"major\", \"meter\": \"4/4\"}", NULL, &response), &response);
    composer_response_destroy(&response);
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_piece(&empty, "{}", NULL, &response), &response);
    composer_response_destroy(&response);
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_plan(NULL, "{}", NULL, &response), &response);
    composer_response_destroy(&response);
    memset(&response, 0, sizeof(response));
    expect_refused(composer_api_vocabulary(&empty, &response), &response);
    composer_response_destroy(&response);

    /* The same request with the snapshot is read. */
    memset(&response, 0, sizeof(response));
    assert(composer_api_motive(test_context(), REQUEST, NULL, &response) == COMPOSER_OK && response.json != NULL);
    composer_response_destroy(&response);

    /* A key of a material the Composer does not write is refused at the key, naming the word. */
    memset(&response, 0, sizeof(response));
    assert(composer_api_motive(test_context(),
                               "{\"input\": {\"text\": \"D4:q F4:q A4:q\"}, \"meter\": \"4/4\", \"key\": \"C dorian\"}", NULL,
                               &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/key") == 0);
    assert(strstr(response.error.message, "the Composer writes major and minor keys; 'dorian' is not one of them") != NULL);
    composer_response_destroy(&response);
}

/* The key of a region's degree is what the relation entry resolves: the closely related key standard-harmony names, for every
   home key and degree, or both refuse (the diminished triads make no key). Degree 1 is the parallel key. */
static const char *const DEGREE_RELATIONS[8] = {NULL,
                                                "omt.relation.parallel",
                                                "omt.relation.supertonic",
                                                "omt.relation.mediant",
                                                "omt.relation.subdominant",
                                                "omt.relation.dominant",
                                                "omt.relation.submediant",
                                                "omt.relation.subtonic"};

static void test_region_keys_equal_the_related_keys(void) {
    FillRng rng;
    fill_rng_seed(&rng, 1);
    size_t made_count = 0;
    for (int tonic = 0; tonic < 12; tonic++) {
        for (int mode = 0; mode < 2; mode++) {
            AnalysisKey home = test_key_mode(tonic, (HarmonyMode)mode);
            HarmonyMode home_mode = harmony_key_mode(&home);
            for (int degree = 1; degree <= 7; degree++) {
                FormRegion one;
                memset(&one, 0, sizeof(one));
                snprintf(one.major[0], sizeof(one.major[0]), "%s", DEGREE_RELATIONS[degree]);
                one.major_count = 1;
                snprintf(one.minor[0], sizeof(one.minor[0]), "%s", DEGREE_RELATIONS[degree]);
                one.minor_count = 1;
                AnalysisKey key;
                bool made = piece_region_key(test_knowledge(), home, &one, &rng, &key);
                if (degree == 1) {
                    assert(made && analysis_key_tonic_pitch_class(&key) == tonic && harmony_key_mode(&key) != home_mode);
                    made_count++;
                    continue;
                }
                HarmonyRelatedKey related;
                bool expected = harmony_related_key_of(home_mode, degree, &related) && harmony_key_is_closely_related(home_mode, &related);
                assert(made == expected);
                if (made) {
                    assert(analysis_key_tonic_pitch_class(&key) == (tonic + harmony_related_key_semitone(home_mode, &related)) % 12);
                    assert(harmony_key_mode(&key) == related.mode);
                    made_count++;
                }
            }
        }
    }
    /* 24 home keys: the parallel key and five closely related keys each. */
    assert(made_count == 24 * 6);
    /* A degree no relation names is refused, and a home key with no material resolves nothing. */
    FormRegion none;
    memset(&none, 0, sizeof(none));
    snprintf(none.major[0], sizeof(none.major[0]), "omt.relation.none");
    none.major_count = 1;
    AnalysisKey key;
    assert(!piece_region_key(test_knowledge(), test_key(0, "major"), &none, &rng, &key));
    FormRegion fifth;
    memset(&fifth, 0, sizeof(fifth));
    snprintf(fifth.major[0], sizeof(fifth.major[0]), "omt.relation.dominant");
    fifth.major_count = 1;
    AnalysisKey zero = plan_key_of_pitch_class(0, NULL);
    assert(!piece_region_key(test_knowledge(), zero, &fifth, &rng, &key));
}

int main(void) {
    test_key_words();
    test_context_required();
    test_region_keys_equal_the_related_keys();
    printf("test_composer_materials: ok\n");
    return 0;
}
