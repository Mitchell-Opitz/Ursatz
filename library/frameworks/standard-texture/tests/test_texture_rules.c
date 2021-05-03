#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "texture_rules.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_texture_rule, standard_texture_constraint,
                                    STANDARD_TEXTURE_RULE_COUNT);
}

/* Texture names: the labels a Texture carries. */
static void test_names(void) {
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_MONOPHONY), "monophony") == 0);
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_HETEROPHONY), "heterophony") == 0);
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_HOMOPHONY), "homophony") == 0);
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_HOMORHYTHM), "homorhythm") == 0);
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT), "melody and accompaniment") == 0);
    assert(strcmp(standard_texture_name(STANDARD_TEXTURE_POLYPHONY), "polyphony") == 0);
    assert(standard_texture_name(STANDARD_TEXTURE_COUNT) == NULL);
    assert(standard_texture_name((StandardTexture)-1) == NULL);
}

static void test_from_name(void) {
    for (int t = 0; t < STANDARD_TEXTURE_COUNT; t++) {
        StandardTexture read = STANDARD_TEXTURE_COUNT;
        assert(standard_texture_from_name(standard_texture_name((StandardTexture)t), &read));
        assert(read == (StandardTexture)t);
    }

    StandardTexture untouched = STANDARD_TEXTURE_POLYPHONY;
    assert(!standard_texture_from_name("Monophony", &untouched));
    assert(!standard_texture_from_name("monophonic", &untouched));
    assert(!standard_texture_from_name("", &untouched));
    assert(!standard_texture_from_name(NULL, &untouched));
    assert(!standard_texture_from_name("monophony", NULL));
    assert(untouched == STANDARD_TEXTURE_POLYPHONY);
}

/* Four main categories; homorhythm and melody and accompaniment are
   subcategories of homophony. */
static void test_categories(void) {
    assert(standard_texture_is_category(STANDARD_TEXTURE_MONOPHONY));
    assert(standard_texture_is_category(STANDARD_TEXTURE_HETEROPHONY));
    assert(standard_texture_is_category(STANDARD_TEXTURE_HOMOPHONY));
    assert(standard_texture_is_category(STANDARD_TEXTURE_POLYPHONY));
    assert(!standard_texture_is_category(STANDARD_TEXTURE_HOMORHYTHM));
    assert(!standard_texture_is_category(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT));
    assert(!standard_texture_is_category(STANDARD_TEXTURE_COUNT));

    int categories = 0;
    for (int t = 0; t < STANDARD_TEXTURE_COUNT; t++) {
        if (standard_texture_is_category((StandardTexture)t)) {
            categories++;
        }
    }
    assert(categories == 4);

    assert(standard_texture_is_homophonic(STANDARD_TEXTURE_HOMOPHONY));
    assert(standard_texture_is_homophonic(STANDARD_TEXTURE_HOMORHYTHM));
    assert(standard_texture_is_homophonic(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT));
    assert(!standard_texture_is_homophonic(STANDARD_TEXTURE_MONOPHONY));
    assert(!standard_texture_is_homophonic(STANDARD_TEXTURE_HETEROPHONY));
    assert(!standard_texture_is_homophonic(STANDARD_TEXTURE_POLYPHONY));
}

static void test_category_of(void) {
    StandardTexture category = STANDARD_TEXTURE_COUNT;
    assert(standard_texture_category(STANDARD_TEXTURE_MONOPHONY, &category));
    assert(category == STANDARD_TEXTURE_MONOPHONY);
    assert(standard_texture_category(STANDARD_TEXTURE_HETEROPHONY, &category));
    assert(category == STANDARD_TEXTURE_HETEROPHONY);
    assert(standard_texture_category(STANDARD_TEXTURE_POLYPHONY, &category));
    assert(category == STANDARD_TEXTURE_POLYPHONY);
    assert(standard_texture_category(STANDARD_TEXTURE_HOMOPHONY, &category));
    assert(category == STANDARD_TEXTURE_HOMOPHONY);
    assert(standard_texture_category(STANDARD_TEXTURE_HOMORHYTHM, &category));
    assert(category == STANDARD_TEXTURE_HOMOPHONY);
    assert(standard_texture_category(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT, &category));
    assert(category == STANDARD_TEXTURE_HOMOPHONY);

    category = STANDARD_TEXTURE_POLYPHONY;
    assert(!standard_texture_category(STANDARD_TEXTURE_COUNT, &category));
    assert(!standard_texture_category(STANDARD_TEXTURE_MONOPHONY, NULL));
    assert(category == STANDARD_TEXTURE_POLYPHONY);
}

/* Monophony is one line; the others are voices together. */
static void test_minimum_voices(void) {
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_MONOPHONY) == 1);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_HETEROPHONY) == 2);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_HOMOPHONY) == 2);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_HOMORHYTHM) == 2);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT) == 2);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_POLYPHONY) == 2);
    assert(standard_texture_minimum_voices(STANDARD_TEXTURE_COUNT) == 0);
}

/* Each texture has its own rule. */
static void test_each_texture_has_a_rule(void) {
    assert((int)STANDARD_TEXTURE_RULE_COUNT == (int)STANDARD_TEXTURE_COUNT);
    for (int i = 0; i < STANDARD_TEXTURE_RULE_COUNT; i++) {
        assert(standard_texture_rule((StandardTextureRule)i) != NULL);
    }
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_from_name();
    test_categories();
    test_category_of();
    test_minimum_voices();
    test_each_texture_has_a_rule();
    printf("test_texture_rules (standard-texture): ok\n");
    return 0;
}
