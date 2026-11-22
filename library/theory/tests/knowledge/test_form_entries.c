#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K7 formal function and K8 theme type entries (R-708): the nine theme types found by the codes of their beginning and ending,
 * the sentence's fragment with its fields as the build embeds them, the contrasting idea's contrast target, and user files whose
 * links name nothing the loader accepts, each refused with the file, the id and the field.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static void test_theme_types(void) {
    static const char *const BEGINNINGS[3] = {"presentation", "antecedent", "compound-basic-idea"};
    static const char *const ENDINGS[3] = {"continuation", "consequent", "cadential"};
    KnowledgeSnapshot *snapshot = build();
    size_t found = 0;
    for (int b = 0; b < 3; b++) {
        for (int e = 0; e < 3; e++) {
            const ThemeType *type = knowledge_snapshot_theme_type_of(snapshot, BEGINNINGS[b], ENDINGS[e]);
            if (type == NULL) {
                /* the nine entries are the sentence, the period and seven hybrids: every pairing has one */
                assert(false);
            }
            found++;
            assert(strcmp(type->beginning->code, BEGINNINGS[b]) == 0 && strcmp(type->ending->code, ENDINGS[e]) == 0);
            assert(type->beginning->level != FORMAL_LEVEL_IDEA && type->ending->level != FORMAL_LEVEL_IDEA);
            int units = 0;
            for (size_t i = 0; i < type->idea_count; i++) {
                units += type->ideas[i].units;
                assert(type->ideas[i].function->level == FORMAL_LEVEL_IDEA);
                assert(type->ideas[i].in_beginning == (i < 2));
            }
            assert(units == 8 && type->ideas[type->idea_count - 1].cadence == THEME_IDEA_CADENCE_FINAL);
        }
    }
    assert(found == 9);
    const ThemeType *sentence = knowledge_snapshot_theme_type_of(snapshot, "presentation", "continuation");
    assert(strcmp(sentence->id, "omt.theme-type.sentence") == 0 && strcmp(sentence->code, "sentence") == 0 && sentence->idea_count == 5);
    const ThemeType *period = knowledge_snapshot_theme_type_of(snapshot, "antecedent", "consequent");
    assert(strcmp(period->id, "omt.theme-type.period") == 0 && period->idea_count == 4);
    assert(strcmp(period->ideas[1].name, "contrasting idea") == 0 && period->ideas[1].cadence == THEME_IDEA_CADENCE_FIRST);
    assert(strcmp(period->ideas[3].name, "consequent contrasting idea") == 0 && period->ideas[3].cadence == THEME_IDEA_CADENCE_FINAL);
    assert(knowledge_snapshot_theme_type_of(snapshot, "presentation", "nothing") == NULL);
    assert(knowledge_snapshot_theme_type_of(snapshot, NULL, "continuation") == NULL);
    assert(knowledge_snapshot_theme_type_of(NULL, "presentation", "continuation") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void test_fragment(void) {
    KnowledgeSnapshot *snapshot = build();
    const ThemeType *sentence = knowledge_snapshot_theme_type_of(snapshot, "presentation", "continuation");
    const ThemeTypeIdea *fragment = &sentence->ideas[2];
    assert(strcmp(fragment->name, "fragment 1") == 0 && strcmp(fragment->slot, "continuation") == 0 && !fragment->in_beginning);
    const FormalFunction *function = fragment->function;
    assert(strcmp(function->id, "omt.function.fragment") == 0 && strcmp(function->code, "fragment") == 0);
    assert(function->core == FORMAL_CORE_MEDIAL && function->level == FORMAL_LEVEL_IDEA);
    assert(function->harmonic_rhythm == FORMAL_PACE_FASTER && function->rhythmic_activity == FORMAL_PACE_FASTER);
    assert(!function->keeps_motive_harmony && function->contrast_against == NULL && function->aspect_count == 0);
    assert(function->harmony_count == 2);
    assert(strcmp(function->harmony[0]->code, "prolongation") == 0 && strcmp(function->harmony[1]->code, "sequence") == 0);
    assert(function->harmony[0]->subtype == VOCABULARY_PROGRESSION_TYPE);
    const FormalFunction *basic = sentence->ideas[0].function;
    assert(basic->keeps_motive_harmony && basic->cadence == FORMAL_CADENCE_ANY && strcmp(basic->harmony[0]->code, "prolongation") == 0);
    assert(sentence->beginning->cadence == FORMAL_CADENCE_NONE && strcmp(sentence->beginning->id, "omt.function.presentation") == 0);
    knowledge_snapshot_destroy(snapshot);
}

static void test_contrast(void) {
    KnowledgeSnapshot *snapshot = build();
    const ThemeType *period = knowledge_snapshot_theme_type_of(snapshot, "antecedent", "consequent");
    const FormalFunction *contrasting = period->ideas[1].function;
    assert(strcmp(contrasting->code, "contrasting-idea") == 0 && contrasting->core == FORMAL_CORE_CONCLUDING);
    assert(contrasting->contrast_against == period->ideas[0].function);
    assert(strcmp(contrasting->contrast_against->code, "basic-idea") == 0);
    assert(contrasting->aspect_count == 2 && strcmp(contrasting->aspects[0].aspect, "rhythm") == 0 &&
           strcmp(contrasting->aspects[1].aspect, "contour") == 0);
    for (size_t i = 0; i < 2; i++) {
        assert(contrasting->aspects[i].amount.num == 1 && contrasting->aspects[i].amount.den == 2);
    }
    assert(!contrasting->keeps_motive_harmony && contrasting->harmony_count == 2);
    assert(strcmp(contrasting->harmony[0]->code, "cadential") == 0);
    knowledge_snapshot_destroy(snapshot);
}

static void refused(const char *entries, const char *path, const char *id, const char *field) {
    char text[2048];
    snprintf(text, sizeof(text), "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[%s]}", entries);
    KnowledgeUserFile file = {path, text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_FIELD);
    assert(snapshot == NULL);
    assert(strstr(message, path) != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

#define FUNCTION_HEAD                                                                                                    \
    "{\"id\":\"user.function.odd\",\"kind\":\"function\",\"subtype\":\"formal-function\",\"name\":\"odd\","            \
    "\"content\":{\"code\":\"odd\",\"core\":\"medial\",\"level\":\"idea\",\"harmonic_rhythm\":\"any\","               \
    "\"rhythmic_activity\":\"any\",\"motive_harmony\":\"keep\","
#define FUNCTION_TAIL "},\"citations\":[],\"status\":\"reference\",\"version\":1}"

static void test_refusals(void) {
    refused(FUNCTION_HEAD "\"harmony\":[\"user.vocabulary.nothing\"]" FUNCTION_TAIL, "mine.json", "user.function.odd", "content.harmony");
    refused(FUNCTION_HEAD "\"harmony\":[\"omt.vocabulary.tonic\"]" FUNCTION_TAIL, "mine.json", "user.function.odd", "content.harmony");
    refused(FUNCTION_HEAD "\"harmony\":[\"omt.vocabulary.cadential\"],\"contrast\":{\"against\":\"omt.vocabulary.tonic\","
                          "\"aspects\":[{\"aspect\":\"rhythm\",\"amount\":\"1/2\"}]}" FUNCTION_TAIL,
            "mine.json", "user.function.odd", "content.contrast.against");
    refused(FUNCTION_HEAD "\"harmony\":[\"omt.vocabulary.cadential\"],\"contrast\":{\"against\":\"omt.function.basic-idea\","
                          "\"aspects\":[{\"aspect\":\"loudness\",\"amount\":\"1/2\"}]}" FUNCTION_TAIL,
            "mine.json", "user.function.odd", "content.contrast.aspects");
    refused("{\"id\":\"user.function.odd\",\"kind\":\"function\",\"subtype\":\"mood\",\"name\":\"odd\",\"content\":{},"
            "\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.function.odd", "subtype");
    refused("{\"id\":\"user.theme-type.odd\",\"kind\":\"theme-type\",\"name\":\"odd\",\"content\":{\"code\":\"odd\","
            "\"beginning\":\"omt.function.basic-idea\",\"ending\":\"omt.function.continuation\",\"ideas\":["
            "{\"name\":\"a\",\"function\":\"omt.function.basic-idea\",\"slot\":\"basic\",\"half\":\"beginning\",\"units\":4,"
            "\"cadence\":\"none\"},{\"name\":\"b\",\"function\":\"omt.function.cadential-idea\",\"slot\":\"cadential\","
            "\"half\":\"ending\",\"units\":4,\"cadence\":\"final\"}]},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.theme-type.odd", "content.beginning");
    refused("{\"id\":\"user.theme-type.odd\",\"kind\":\"theme-type\",\"name\":\"odd\",\"content\":{\"code\":\"odd\","
            "\"beginning\":\"omt.function.presentation\",\"ending\":\"omt.function.continuation\",\"ideas\":["
            "{\"name\":\"a\",\"function\":\"omt.function.presentation\",\"slot\":\"basic\",\"half\":\"beginning\",\"units\":4,"
            "\"cadence\":\"none\"},{\"name\":\"b\",\"function\":\"omt.function.cadential-idea\",\"slot\":\"cadential\","
            "\"half\":\"ending\",\"units\":4,\"cadence\":\"final\"}]},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.theme-type.odd", "content.ideas");
}

int main(void) {
    test_theme_types();
    test_fragment();
    test_contrast();
    test_refusals();
    return 0;
}
