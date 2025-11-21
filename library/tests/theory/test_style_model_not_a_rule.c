#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "theory/style_model_blend.h"
#include "theory/style_model_build.h"

/*
 * A StyleModel built from corpus statistics cannot become a Rule or a
 * Constraint. Learned style is a Tendency -- observed frequency, never
 * must/must-not -- and there is no path from it to the rules/ types:
 *
 *   - the code of every style header and of tendency.h (comments removed)
 *     names no Rule, Constraint, RuleResult, ConstraintResult or
 *     ConstraintEvalResult, includes nothing from rules/, so no function can
 *     accept or return one;
 *   - no .c file of the style model includes rules/ either;
 *   - what a built model offers is Tendencies, whose frequency is a
 *     fraction in [0, 1] and nothing else: a statistic's count can never
 *     exceed its total, so it can never state "required" (above 1) or a
 *     truth value.
 */

#define THEORY_DIR "../../src/theory/"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)size + 1);
    assert(text != NULL);
    assert(fread(text, 1, (size_t)size, f) == (size_t)size);
    text[size] = '\0';
    fclose(f);
    return text;
}

/* Blanks out comments, in place, so only code is searched. */
static void strip_comments(char *text) {
    for (char *p = text; *p != '\0'; p++) {
        if (p[0] == '/' && p[1] == '*') {
            char *end = strstr(p + 2, "*/");
            assert(end != NULL);
            for (char *q = p; q < end + 2; q++) {
                if (*q != '\n') {
                    *q = ' ';
                }
            }
            p = end + 1;
        } else if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n') {
                *p++ = ' ';
            }
            if (*p == '\0') {
                break;
            }
        }
    }
}

/* True if `word` appears in text as a whole identifier or as the start of a longer one, e.g. "Rule" in "RuleResult". */
static int mentions(const char *text, const char *word) {
    size_t length = strlen(word);
    for (const char *p = text; (p = strstr(p, word)) != NULL; p += length) {
        int starts_word = p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
        if (starts_word) {
            return 1;
        }
    }
    return 0;
}

static void assert_no_rules_vocabulary(const char *file) {
    char path[256];
    snprintf(path, sizeof(path), THEORY_DIR "%s", file);
    char *text = read_file(path);
    strip_comments(text);
    const char *forbidden[] = {"Rule", "Constraint", "rules/", "rule_", "constraint_"};
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++) {
        if (mentions(text, forbidden[i]) != 0) {
            fprintf(stderr, "%s names \"%s\" in code: a StyleModel/Tendency must have no path to a Rule\n", file,
                    forbidden[i]);
            abort();
        }
    }
    free(text);
}

static void test_no_path_to_a_rule(void) {
    const char *files[] = {"tendency.h",          "tendency.c",          "style_model.h",       "style_model.c",
                           "style_features.h",    "style_features.c",    "style_fraction.h",    "style_fraction.c",
                           "style_model_build.h", "style_model_build.c", "style_model_blend.h", "style_model_blend.c"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        assert_no_rules_vocabulary(files[i]);
    }
}

static void test_a_built_model_offers_only_frequencies(void) {
    const StyleStatistic statistics[] = {{"c", "always", 4, 4}, {"c", "never", 0, 4}, {"c", "sometimes", 1, 4}};
    EntityID *id = NULL;
    assert(entity_id_create("style-frequencies", &id) == ENTITY_ID_OK);
    StyleModel *model = NULL;
    assert(style_model_from_statistics(id, "frequencies", "corpus", "v1", statistics, 3, &model) ==
           STYLE_MODEL_BUILD_OK);

    Tendency **tendencies = NULL;
    size_t count = 0;
    assert(style_model_tendencies(model, &tendencies, &count) == STYLE_TENDENCIES_OK && count == 3);
    for (size_t i = 0; i < count; i++) {
        /* A frequency, never above 1: even a statistic seen every time is "4 of 4", not "required". */
        assert(tendency_frequency_denominator(tendencies[i]) > 0);
        assert(tendency_frequency_numerator(tendencies[i]) <= tendency_frequency_denominator(tendencies[i]));
    }
    /* "never seen" is a frequency of 0, and is not "forbidden": it stays an ordinary Tendency. */
    assert(strcmp(tendency_label(tendencies[1]), "never") == 0);
    assert(tendency_frequency_numerator(tendencies[1]) == 0);
    style_tendencies_free(tendencies, count);

    /* A statistic that would claim more than "always" cannot be built, so it cannot be promoted by exceeding 1. */
    const StyleStatistic impossible[] = {{"c", "required", 5, 4}};
    EntityID *other_id = NULL;
    assert(entity_id_create("style-impossible", &other_id) == ENTITY_ID_OK);
    StyleModel *rejected = NULL;
    assert(style_model_from_statistics(other_id, "impossible", "corpus", "v1", impossible, 1, &rejected) ==
           STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC);
    assert(rejected == NULL);
    entity_id_destroy(other_id);
    style_model_destroy(model);
}

int main(void) {
    test_no_path_to_a_rule();
    test_a_built_model_offers_only_frequencies();
    printf("test_style_model_not_a_rule: ok\n");
    return 0;
}
