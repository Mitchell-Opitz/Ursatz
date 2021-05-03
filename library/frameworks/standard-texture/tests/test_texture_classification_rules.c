#include <assert.h>
#include <stdio.h>

#include "rule_check.h"
#include "texture_classification_rules.h"

typedef StandardTextureFeatures Features;

#define MONO STANDARD_TEXTURE_MONOPHONY
#define HETERO STANDARD_TEXTURE_HETEROPHONY
#define HOMO STANDARD_TEXTURE_HOMOPHONY
#define HOMORHYTHM STANDARD_TEXTURE_HOMORHYTHM
#define MELACC STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT
#define POLY STANDARD_TEXTURE_POLYPHONY

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_texture_classification_rule, standard_texture_classification_constraint,
                                    STANDARD_TEXTURE_CLASSIFICATION_RULE_COUNT);
}

static void check_classified(const Features *features, StandardTexture expected) {
    StandardTexture texture = STANDARD_TEXTURE_COUNT;
    assert(standard_texture_classify(features, &texture));
    assert(texture == expected);
}

static StandardTextureSet bits(StandardTexture a, int count, StandardTexture b, StandardTexture c) {
    StandardTextureSet set = STANDARD_TEXTURE_BIT(a);
    if (count > 1) {
        set |= STANDARD_TEXTURE_BIT(b);
    }
    if (count > 2) {
        set |= STANDARD_TEXTURE_BIT(c);
    }
    return set;
}

/* Examples 1 and 2: Bach's solo cello and Seeger's voice, one line. */
static void test_monophony(void) {
    Features solo = {1, false, false, false, false, false};
    check_classified(&solo, MONO);
    assert(standard_texture_matching(&solo) == STANDARD_TEXTURE_BIT(MONO));

    /* Several voices singing or playing in unison are monophony too. */
    Features unison = {4, true, false, true, false, false};
    check_classified(&unison, MONO);
    assert(standard_texture_matching(&unison) == STANDARD_TEXTURE_BIT(MONO));
}

/* Examples 3 and 4: the same melody with different embellishments. */
static void test_heterophony(void) {
    Features baktagir = {3, false, true, false, false, false};
    check_classified(&baktagir, HETERO);
    assert(standard_texture_matching(&baktagir) == STANDARD_TEXTURE_BIT(HETERO));

    /* Fiddle and flute with slight variation, moving at close to the same pace. */
    Features chieftains = {2, false, true, true, false, false};
    check_classified(&chieftains, HETERO);
    assert(standard_texture_set_contains(standard_texture_matching(&chieftains), HETERO));
    assert(standard_texture_set_contains(standard_texture_matching(&chieftains), HOMORHYTHM));
}

/* Example 5: block chords in a horn quartet. */
static void test_homorhythm(void) {
    Features horns = {4, false, false, true, false, false};
    check_classified(&horns, HOMORHYTHM);
    StandardTextureSet set = standard_texture_matching(&horns);
    assert(set == bits(HOMORHYTHM, 2, HOMO, HOMO));
}

/* Example 6: a melody stands out, but the voices keep rhythmic unison.
   Both subcategories hold; precedence names homorhythm. */
static void test_homorhythm_with_a_melody_standing_out(void) {
    Features thyme = {4, false, false, true, true, false};
    check_classified(&thyme, HOMORHYTHM);
    StandardTextureSet set = standard_texture_matching(&thyme);
    assert(standard_texture_set_contains(set, HOMORHYTHM));
    assert(standard_texture_set_contains(set, MELACC));
    assert(standard_texture_set_contains(set, HOMO));
    assert(!standard_texture_set_contains(set, POLY));
}

/* Examples 7 and 8: a clear melody over an accompaniment in a different
   rhythm. */
static void test_melody_and_accompaniment(void) {
    Features hindemith = {2, false, false, false, true, false};
    check_classified(&hindemith, MELACC);
    assert(standard_texture_matching(&hindemith) == bits(MELACC, 2, HOMO, HOMO));

    Features misty = {2, false, false, false, true, false};
    check_classified(&misty, MELACC);
}

/* Examples 9 and 10: independent melodic lines and rhythms. */
static void test_polyphony(void) {
    Features fugue = {3, false, false, false, false, true};
    check_classified(&fugue, POLY);
    assert(standard_texture_matching(&fugue) == STANDARD_TEXTURE_BIT(POLY));

    Features rent = {3, false, false, false, false, true};
    check_classified(&rent, POLY);
}

/* Homophony is the category of the two subcategories and is never the
   single answer. */
static void test_homophony_is_never_the_single_answer(void) {
    Features homophonic[] = {{4, false, false, true, false, false},
                             {2, false, false, false, true, false},
                             {4, false, false, true, true, false}};
    for (size_t i = 0; i < sizeof(homophonic) / sizeof(homophonic[0]); i++) {
        StandardTexture texture;
        assert(standard_texture_set_contains(standard_texture_matching(&homophonic[i]), HOMO));
        assert(standard_texture_classify(&homophonic[i], &texture));
        assert(texture != HOMO);
        assert(standard_texture_is_homophonic(texture));
    }
}

/* PRECEDENCE: monophony, heterophony, polyphony, homorhythm, melody and
   accompaniment. */
static void test_precedence(void) {
    /* A shared melody in rhythmic unison with a melody standing out. */
    Features all_but_poly = {3, false, true, true, true, false};
    StandardTextureSet set = standard_texture_matching(&all_but_poly);
    assert(standard_texture_set_contains(set, HETERO));
    assert(standard_texture_set_contains(set, HOMORHYTHM));
    assert(standard_texture_set_contains(set, MELACC));
    check_classified(&all_but_poly, HETERO);

    /* Unison and rhythmic unison: monophony, not homorhythm. */
    Features unison = {2, true, false, true, false, false};
    assert(!standard_texture_set_contains(standard_texture_matching(&unison), HOMORHYTHM));
    check_classified(&unison, MONO);
}

/* FEATURES: contradictions are not a passage. */
static void test_invalid_features(void) {
    Features none = {0, false, false, false, false, false};
    Features negative = {-1, false, false, false, false, false};
    Features lone_shared = {1, false, true, false, false, false};
    Features lone_melody = {1, false, false, false, true, false};
    Features lone_independent = {1, false, false, false, false, true};
    Features unison_variants = {2, true, true, false, false, false};
    Features unison_melody = {2, true, false, false, true, false};
    Features independent_unison = {3, true, false, false, false, true};
    Features independent_rhythm = {3, false, false, true, false, true};
    Features independent_variants = {3, false, true, false, false, true};

    const Features *invalid[] = {&none,          &negative,          &lone_shared,        &lone_melody,
                                 &lone_independent, &unison_variants, &unison_melody,      &independent_unison,
                                 &independent_rhythm, &independent_variants};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        StandardTexture texture = STANDARD_TEXTURE_COUNT;
        assert(!standard_texture_features_valid(invalid[i]));
        assert(standard_texture_matching(invalid[i]) == 0);
        assert(!standard_texture_classify(invalid[i], &texture));
        assert(texture == STANDARD_TEXTURE_COUNT);
    }

    Features valid = {2, false, false, false, false, false};
    assert(standard_texture_features_valid(&valid));
    assert(!standard_texture_features_valid(NULL));
}

/* Several voices with none of the features have no texture here. */
static void test_no_texture(void) {
    Features nothing = {3, false, false, false, false, false};
    StandardTexture texture = STANDARD_TEXTURE_COUNT;
    assert(standard_texture_features_valid(&nothing));
    assert(standard_texture_matching(&nothing) == 0);
    assert(!standard_texture_classify(&nothing, &texture));
    assert(texture == STANDARD_TEXTURE_COUNT);
}

static void test_classify_rejects_invalid_arguments(void) {
    Features solo = {1, false, false, false, false, false};
    StandardTexture texture;
    assert(!standard_texture_classify(NULL, &texture));
    assert(!standard_texture_classify(&solo, NULL));
    assert(standard_texture_matching(NULL) == 0);
}

static void test_set_contains(void) {
    StandardTextureSet set = STANDARD_TEXTURE_BIT(MONO) | STANDARD_TEXTURE_BIT(POLY);
    assert(standard_texture_set_contains(set, MONO));
    assert(standard_texture_set_contains(set, POLY));
    assert(!standard_texture_set_contains(set, HETERO));
    assert(!standard_texture_set_contains(set, STANDARD_TEXTURE_COUNT));
    assert(!standard_texture_set_contains(set, (StandardTexture)-1));
    assert(!standard_texture_set_contains(0, MONO));
}

/* Property: over every combination of features, matching is consistent
   with classification, and homophony is in the set exactly when one of
   its subcategories is. */
static void test_every_feature_combination(void) {
    int classified = 0;
    for (int voices = 0; voices <= 3; voices++) {
        for (int mask = 0; mask < 32; mask++) {
            Features f = {voices, (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0,
                          (mask & 16) != 0};
            StandardTextureSet set = standard_texture_matching(&f);
            StandardTexture texture;
            bool answered = standard_texture_classify(&f, &texture);

            assert(answered == (set != 0 && !(set == STANDARD_TEXTURE_BIT(HOMO))));
            if (!standard_texture_features_valid(&f)) {
                assert(set == 0 && !answered);
                continue;
            }
            bool homophonic = standard_texture_set_contains(set, HOMORHYTHM) ||
                              standard_texture_set_contains(set, MELACC);
            assert(standard_texture_set_contains(set, HOMO) == homophonic);
            if (answered) {
                assert(standard_texture_set_contains(set, texture));
                assert(standard_texture_minimum_voices(texture) <= voices);
                classified++;
            }
        }
    }
    assert(classified > 0);
}

int main(void) {
    test_rules_reference_framework();
    test_monophony();
    test_heterophony();
    test_homorhythm();
    test_homorhythm_with_a_melody_standing_out();
    test_melody_and_accompaniment();
    test_polyphony();
    test_homophony_is_never_the_single_answer();
    test_precedence();
    test_invalid_features();
    test_no_texture();
    test_classify_rejects_invalid_arguments();
    test_set_contains();
    test_every_feature_combination();
    printf("test_texture_classification_rules (standard-texture): ok\n");
    return 0;
}
