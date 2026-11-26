#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K7 formal function and K8 theme type entries (R-708): the nine theme types found by the codes of their beginning and ending,
 * the sentence's fragment with its fields as the build embeds them, the contrasting idea's contrast target, and user files whose
 * links name nothing the loader accepts, each refused with the file, the id and the field. The sentence and the period carry the K8
 * slot expectations of R-749 (jobs, harmony alternatives, climax units and sounds per unit); the seven hybrids carry none.
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

static void test_expectations(void) {
    KnowledgeSnapshot *snapshot = build();
    const ThemeType *sentence = knowledge_snapshot_theme_type_of(snapshot, "presentation", "continuation");
    const ThemeType *period = knowledge_snapshot_theme_type_of(snapshot, "antecedent", "consequent");
    for (size_t i = 0; i < sentence->idea_count; i++) {
        assert(sentence->ideas[i].expect.present);
    }
    for (size_t i = 0; i < period->idea_count; i++) {
        assert(period->ideas[i].expect.present);
    }
    const ThemeTypeExpect *fragment = &sentence->ideas[3].expect;
    assert(strcmp(sentence->ideas[3].name, "fragment 2") == 0);
    assert(fragment->job_count == 1 && fragment->jobs[0] == THEME_JOB_LIQUIDATION && fragment->climax == 1u);
    const ThemeTypeExpect *repetition = &sentence->ideas[1].expect;
    assert(repetition->harmony_count == 2 && repetition->harmony[0].restate && !repetition->harmony[1].restate);
    assert(repetition->harmony[0].chord_count == 0 && repetition->job_count == 0);
    const ThemeTypeExpect *basic = &sentence->ideas[0].expect;
    assert(basic->job_count == 2 && basic->harmony_count == 4 && basic->climax == 0);
    assert(basic->jobs[0] == (THEME_JOB_LONG | THEME_JOB_RUNNING | THEME_JOB_SYNCOPATED));
    assert(strcmp(basic->harmony[2].chords[0], "I V@2") == 0 && strcmp(basic->harmony[2].chords[1], "I") == 0);
    assert(basic->sounds[0] == (THEME_SOUND_NINTH_DOWNBEAT | THEME_SOUND_APPOGGIATURA) && basic->sounds[1] == 0);
    const ThemeTypeExpect *cadential = &sentence->ideas[4].expect;
    assert(cadential->jobs[1] == THEME_JOB_CADENCE && cadential->climax == 1u && cadential->sounds[1] == THEME_SOUND_NINTH_DOWNBEAT);
    assert(strcmp(cadential->harmony[0].type->code, "cadential") == 0);
    const ThemeTypeExpect *sequence = &sentence->ideas[2].expect;
    assert(strcmp(sequence->harmony[2].chords[0], "VI II@2") == 0 && strcmp(sequence->harmony[2].type->code, "sequence") == 0);
    assert(period->ideas[2].expect.harmony_count == 1 && period->ideas[2].expect.harmony[0].restate);
    for (const ThemeType *type = sentence; type != NULL; type = type == sentence ? period : NULL) {
        for (size_t i = 0; i < type->idea_count; i++) {
            const ThemeTypeExpect *expect = &type->ideas[i].expect;
            assert(expect->job_count == 0 || expect->job_count == (size_t)type->ideas[i].units);
            for (size_t k = 0; k < expect->harmony_count; k++) {
                assert(expect->harmony[k].restate ? expect->harmony[k].chord_count == 0
                                                  : expect->harmony[k].chord_count == (size_t)type->ideas[i].units);
                assert(expect->harmony[k].type != NULL);
            }
        }
    }
    static const char *const HYBRIDS[][2] = {{"antecedent", "continuation"}, {"antecedent", "cadential"}, {"compound-basic-idea", "continuation"},
                                             {"compound-basic-idea", "consequent"}, {"compound-basic-idea", "cadential"},
                                             {"presentation", "consequent"}, {"presentation", "cadential"}};
    for (size_t h = 0; h < 7; h++) {
        const ThemeType *type = knowledge_snapshot_theme_type_of(snapshot, HYBRIDS[h][0], HYBRIDS[h][1]);
        assert(type != NULL && type != sentence && type != period);
        for (size_t i = 0; i < type->idea_count; i++) {
            assert(!type->ideas[i].expect.present && type->ideas[i].expect.harmony_count == 0);
        }
    }
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

#define THEME_HEAD                                                                                                       \
    "{\"id\":\"user.theme-type.odd\",\"kind\":\"theme-type\",\"name\":\"odd\",\"content\":{\"code\":\"odd\","                    \
    "\"beginning\":\"omt.function.presentation\",\"ending\":\"omt.function.continuation\",\"ideas\":["                         \
    "{\"name\":\"a\",\"function\":\"omt.function.basic-idea\",\"slot\":\"basic\",\"half\":\"beginning\","
#define THEME_TAIL                                                                                                       \
    "{\"name\":\"b\",\"function\":\"omt.function.cadential-idea\",\"slot\":\"cadential\",\"half\":\"ending\",\"units\":6,"   \
    "\"cadence\":\"final\"}]},\"citations\":[],\"status\":\"reference\",\"version\":1}"

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
    refused(THEME_HEAD "\"units\":2,\"cadence\":\"none\",\"expect\":{\"harmony\":[{\"chords\":[\"I\",\"V\"],"
                       "\"type\":\"omt.vocabulary.cadential\"}]}}," THEME_TAIL,
            "mine.json", "user.theme-type.odd", "harmony type omt.vocabulary.cadential is not one of omt.function.basic-idea's");
    refused(THEME_HEAD "\"units\":2,\"cadence\":\"none\",\"expect\":{\"harmony\":[{\"chords\":[\"I\"],"
                       "\"type\":\"omt.vocabulary.prolongational\"}]}}," THEME_TAIL,
            "mine.json", "user.theme-type.odd", "content.ideas");
    refused(THEME_HEAD "\"units\":2,\"cadence\":\"none\",\"expect\":{\"jobs\":[[\"long\"]]}}," THEME_TAIL, "mine.json",
            "user.theme-type.odd", "content.ideas");
    refused(THEME_HEAD "\"units\":2,\"cadence\":\"none\",\"expect\":{\"jobs\":[[\"long\"],[\"dancing\"]]}}," THEME_TAIL, "mine.json",
            "user.theme-type.odd", "content.ideas");
    refused(THEME_HEAD "\"units\":2,\"cadence\":\"none\",\"expect\":{\"climax\":[2]}}," THEME_TAIL, "mine.json",
            "user.theme-type.odd", "content.ideas");
}

int main(void) {
    test_theme_types();
    test_fragment();
    test_contrast();
    test_expectations();
    test_refusals();
    return 0;
}
