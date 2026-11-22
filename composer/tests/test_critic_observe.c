#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan_constraint.h>
#include <plan/plan_json.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/critic.h>

#include "check/hearing.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The critic's words (R-720). critic_message fills every placeholder of the 12 michelle entries and keeps one it has no value for. A
 * theme grown with development.functions.contrast on is judged and observed: every scored observation is an evaluator's with a penalty
 * above 0, no judgment holds more than 32, and the unscored ones agree with what the hearing found about the plan (theme_heard when the
 * theme's response says it was not heard as requested, cadence_heard for each planned cadence the Analyzer check does not agree).
 */

static bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static CriticFinding full_finding(void) {
    CriticFinding finding;
    memset(&finding, 0, sizeof(finding));
    finding.evaluator = CRITIC_LEAP_LOAD;
    finding.penalty = rational_ratio(1, 4);
    finding.bar = 3;
    finding.beat = rational_ratio(3, 2);
    finding.first = 2;
    finding.last = 4;
    finding.count = 5;
    finding.value = rational_ratio(3, 4);
    finding.limit = rational_ratio(1, 2);
    snprintf(finding.idea, sizeof(finding.idea), "contrasting idea");
    snprintf(finding.against, sizeof(finding.against), "basic idea");
    snprintf(finding.aspect, sizeof(finding.aspect), "rhythm");
    return finding;
}

static void test_message(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    CriticFinding finding = full_finding();
    CriticFields fields;
    char out[256];
    memset(&fields, 0, sizeof(fields));
    snprintf(fields.planned, sizeof(fields.planned), "half");
    snprintf(fields.heard, sizeof(fields.heard), "perfect-authentic");
    fields.amount = rational_ratio(1, 2);
    fields.has_amount = true;
    size_t rules = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(knowledge); i++) {
        const char *id = knowledge_snapshot_entry_at(knowledge, i)->id;
        const RuleEntry *rule = starts_with(id, "michelle.rule.") ? knowledge_snapshot_rule(knowledge, id) : NULL;
        if (rule == NULL) {
            continue;
        }
        rules++;
        size_t length = critic_message(rule->message, &finding, &fields, out, sizeof(out));
        assert(length == strlen(out) && length > 0 && strchr(out, '{') == NULL && strchr(out, '}') == NULL);
    }
    assert(rules == 12);
    /* the values as text */
    critic_message("bars {first} to {last} in {bar} at {beat}: {value} of {limit}, {count}", &finding, &fields, out, sizeof(out));
    assert(strcmp(out, "bars 2 to 4 in 3 at 3/2: 3/4 of 1/2, 5") == 0);
    critic_message("the {planned} cadence is heard as {heard}; {idea}, {against}, {aspect}, {amount}", &finding, &fields, out, sizeof(out));
    assert(strcmp(out, "the half cadence is heard as perfect-authentic; contrasting idea, basic idea, rhythm, 1/2") == 0);
    /* an unknown name and a name with no value stay as written */
    critic_message("the {colour} is {bar}", &finding, &fields, out, sizeof(out));
    assert(strcmp(out, "the {colour} is 3") == 0);
    finding.bar = -1;
    critic_message("bar {bar} beat {beat} {amount}", &finding, NULL, out, sizeof(out));
    assert(strcmp(out, "bar {bar} beat {beat} {amount}") == 0);
    critic_message("{bar} {heard}", NULL, NULL, out, sizeof(out));
    assert(strcmp(out, "{bar} {heard}") == 0);
    /* a short buffer is cut and ended */
    finding = full_finding();
    assert(critic_message("bars {first} to {last}", &finding, NULL, out, 8) == 7 && strcmp(out, "bars 2 ") == 0);
    assert(critic_message("text", NULL, NULL, out, 0) == 0);
    assert(critic_message(NULL, NULL, NULL, out, sizeof(out)) == 0 && out[0] == '\0');
}

/* The plan of the first theme composed from a request fixture and a seed, with the contrast target on or off; NULL when the seed makes none. */
static Plan *theme_plan(const char *fixture, unsigned long seed, bool contrast, bool *heard_as_requested) {
    char *text = test_must_read_file(fixture, NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    assert(cJSON_AddNumberToObject(request, "seed", (double)seed) && cJSON_AddBoolToObject(request, "plans", true));
    if (contrast) {
        cJSON *preferences = cJSON_Parse("{\"development\":{\"functions\":{\"contrast\":true}}}");
        assert(preferences && cJSON_AddItemToObject(request, "preferences", preferences));
    }
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), printed, NULL, &response);
    free(printed);
    if (status != URSATZ_OK) {
        composer_response_destroy(&response);
        return NULL;
    }
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    char *plan_text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(theme, "plan"));
    *heard_as_requested = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(theme, "heard_as_requested"));
    cJSON_Delete(document);
    assert(plan_text);
    Plan *plan = NULL;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), plan_text, composer_plan_read_hooks(), &plan, &error) == PLAN_OK);
    free(plan_text);
    return plan;
}

static NodeId node_named(const Plan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_has_node(plan, id) ? plan_node_name(plan, id) : NULL;
        if (found != NULL && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

typedef struct {
    size_t composed;
    size_t theme_heard;
    size_t cadence_heard;
    size_t contrast_heard;
    size_t neither;
} Tally;

/* One theme judged and observed, its observations checked against the hearing and the response. */
static void judge_theme(const char *fixture, unsigned long seed, bool contrast, Tally *tally) {
    bool requested = false;
    Plan *plan = theme_plan(fixture, seed, contrast, &requested);
    if (plan == NULL) {
        return;
    }
    tally->composed++;
    ComposerHearing *hearing = NULL;
    assert(hearing_create(plan, test_knowledge(), &hearing) == HEARING_OK);
    CriticLine line;
    assert(critic_line_from_render(plan, hearing_rendered(hearing), &line) == CRITIC_OK);
    CriticSettings settings;
    CriticJudgment judgment;
    critic_settings_default(&settings);
    const CriticLine *set[] = {&line};
    assert(critic_judge(&line, set, 1, test_knowledge(), &settings, &judgment) == CRITIC_OK);
    size_t scored = judgment.observation_count;
    for (size_t i = 0; i < scored; i++) {
        CriticEvaluator evaluator;
        assert(judgment.observations[i].scored && critic_evaluator_from_code(judgment.observations[i].rule, &evaluator));
        assert(judgment.findings[evaluator].penalty.num > 0 && rational_equals(judgment.observations[i].penalty, judgment.findings[evaluator].penalty));
        assert(judgment.observations[i].message[0] != '\0' && strchr(judgment.observations[i].message, '{') == NULL);
        assert(i == 0 || evaluator > 0);
    }
    NodeId theme = node_named(plan, "theme");
    assert(theme != PLAN_NODE_NONE);
    assert(critic_observe(hearing, plan, theme, &line, test_knowledge(), &judgment) == CRITIC_OK);
    assert(judgment.observation_count <= CRITIC_OBSERVATION_MAX && judgment.observation_count >= scored);
    size_t theme_heard = 0, cadence_heard = 0, contrast_heard = 0;
    for (size_t i = scored; i < judgment.observation_count; i++) {
        const CriticObservation *observation = &judgment.observations[i];
        assert(!observation->scored && observation->penalty.num == 0 && observation->message[0] != '\0');
        assert(strchr(observation->message, '{') == NULL);
        theme_heard += strcmp(observation->rule, "theme_heard") == 0;
        cadence_heard += strcmp(observation->rule, "cadence_heard") == 0;
        contrast_heard += strcmp(observation->rule, "contrast_heard") == 0;
    }
    assert(theme_heard + cadence_heard + contrast_heard == judgment.observation_count - scored);
    /* theme_heard is the response's heard_as_requested false (a hybrid is heard as asked only with its beginning and ending) */
    assert(theme_heard == (requested ? 0u : 1u));
    /* cadence_heard is one for each planned cadence the Analyzer check does not agree */
    AnalyzerCheck check;
    memset(&check, 0, sizeof(check));
    size_t disagreed = 0;
    if (analyzer_check_section_in(hearing, theme, &check) == ANALYZER_CHECK_OK) {
        for (size_t i = 0; i < check.item_count; i++) {
            disagreed += check.items[i].kind == ANALYZER_CHECK_CADENCE && !check.items[i].agreed;
        }
    }
    analyzer_check_destroy(&check);
    assert(cadence_heard == disagreed);
    tally->theme_heard += theme_heard;
    tally->cadence_heard += cadence_heard;
    tally->contrast_heard += contrast_heard;
    tally->neither += theme_heard + cadence_heard == 0;
    critic_line_destroy(&line);
    hearing_destroy(hearing);
    plan_destroy(plan);
}

/* A contrast target the melody cannot meet (a rhythm distance of 99) is heard as missed, with the distance heard and the amount asked for. */
static void test_unmet_contrast(void) {
    bool requested = false;
    Plan *plan = theme_plan(COMPOSER_FIXTURE_DIR "/theme/period-contrast.json", 1, true, &requested);
    assert(plan != NULL);
    NodeId idea = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan) && idea == PLAN_NODE_NONE; id++) {
        idea = plan_has_node(plan, id) && plan_node_find_constraint(plan, id, PLAN_CONSTRAINT_CONTRAST) != NULL ? id : PLAN_NODE_NONE;
    }
    assert(idea != PLAN_NODE_NONE);
    PlanConstraint asked = *plan_node_find_constraint(plan, idea, PLAN_CONSTRAINT_CONTRAST);
    for (size_t i = 0; i < plan_node_constraint_count(plan, idea); i++) {
        if (plan_node_constraint_at(plan, idea, i)->kind == PLAN_CONSTRAINT_CONTRAST) {
            assert(plan_node_remove_constraint(plan, idea, i) == PLAN_OK);
            break;
        }
    }
    asked.params.contrast.aspect_count = 1;
    asked.params.contrast.aspects[0].aspect = CONTRAST_ASPECT_RHYTHM;
    asked.params.contrast.aspects[0].amount = rational_ratio(99, 1);
    assert(plan_node_add_constraint(plan, idea, &asked) == PLAN_OK);
    ComposerHearing *hearing = NULL;
    assert(hearing_create(plan, test_knowledge(), &hearing) == HEARING_OK);
    CriticLine line;
    CriticSettings settings;
    CriticJudgment judgment;
    assert(critic_line_from_render(plan, hearing_rendered(hearing), &line) == CRITIC_OK);
    critic_settings_default(&settings);
    assert(critic_judge(&line, NULL, 0, test_knowledge(), &settings, &judgment) == CRITIC_OK);
    assert(critic_observe(hearing, plan, node_named(plan, "theme"), &line, test_knowledge(), &judgment) == CRITIC_OK);
    size_t missed = 0;
    for (size_t i = 0; i < judgment.observation_count; i++) {
        if (strcmp(judgment.observations[i].rule, "contrast_heard") == 0) {
            missed++;
            assert(!judgment.observations[i].scored && judgment.observations[i].bar >= 1);
            assert(strstr(judgment.observations[i].message, "from the") != NULL && strstr(judgment.observations[i].message, "in rhythm") != NULL);
            assert(strstr(judgment.observations[i].message, "the plan asks for 99") != NULL);
        }
    }
    assert(missed == 1);
    critic_line_destroy(&line);
    hearing_destroy(hearing);
    plan_destroy(plan);
}

int main(void) {
    test_message();
    test_unmet_contrast();
    static const char *const FIXTURES[] = {COMPOSER_FIXTURE_DIR "/theme/one-motive.json", COMPOSER_FIXTURE_DIR "/theme/period-contrast.json"};
    Tally tally;
    memset(&tally, 0, sizeof(tally));
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= 6; seed++) {
            judge_theme(FIXTURES[f], seed, true, &tally);
        }
    }
    printf("test_critic_observe: %zu themes, theme_heard %zu, cadence_heard %zu, contrast_heard %zu, neither %zu\n", tally.composed,
           tally.theme_heard, tally.cadence_heard, tally.contrast_heard, tally.neither);
    /* Measured 2026-11-22 on the two fixtures, seeds 1 to 6, the contrast target on: 12 themes compose, 5 are heard as another form than the
       pairing asks for, 1 has a planned cadence heard otherwise, none misses its contrast target, 7 have neither. */
    assert(tally.composed == 12 && tally.theme_heard == 5 && tally.cadence_heard == 1 && tally.contrast_heard == 0 && tally.neither == 7);
    return 0;
}
