#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/preferences.h>

/*
 * The critic's rules (R-717): the knowledge entries michelle.rule.* (evaluators) and fux.rule.* (constraints) pinned to the Composer's
 * tables both ways. Every evaluator entry's code is a CriticEvaluator code with the function name the table gives and a default weight
 * equal to the default of critic.weights.<code>; every evaluator has its entry and a soft rule row keyed critic.weights.<code>.
 * Every constraint entry is a hard row of the rule table keyed melody.waive and the other way round.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    return snapshot;
}

static bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static const ComposerRule *rule_named(const char *name, size_t *count_out) {
    size_t count = 0;
    const ComposerRule *rules = composer_preferences_rules(&count);
    size_t found = 0;
    const ComposerRule *row = NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(rules[i].name, name) == 0) {
            row = &rules[i];
            found++;
        }
    }
    *count_out = found;
    return row;
}

static int default_weight(const char *code) {
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    cJSON *document = composer_preferences_write(&defaults);
    const cJSON *critic = cJSON_GetObjectItemCaseSensitive(document, "critic");
    const cJSON *weights = cJSON_GetObjectItemCaseSensitive(critic, "weights");
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(weights, code);
    assert(cJSON_IsNumber(weight));
    int value = weight->valueint;
    cJSON_Delete(document);
    return value;
}

static void test_evaluators(void) {
    KnowledgeSnapshot *snapshot = build();
    bool seen[CRITIC_EVALUATOR_COUNT] = {false};
    size_t evaluators = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const char *id = knowledge_snapshot_entry_at(snapshot, i)->id;
        const RuleEntry *rule = starts_with(id, "michelle.rule.") ? knowledge_snapshot_rule(snapshot, id) : NULL;
        if (rule == NULL || rule->subtype != RULE_EVALUATOR) {
            continue;
        }
        CriticEvaluator evaluator;
        assert(critic_evaluator_from_code(rule->code, &evaluator));
        assert(!seen[evaluator]);
        seen[evaluator] = true;
        evaluators++;
        assert(strcmp(rule->function, critic_evaluator_function(evaluator)) == 0);
        assert(rule->default_weight == default_weight(rule->code));
        /* The id is the code with its underscores as hyphens. */
        char expected[64];
        snprintf(expected, sizeof(expected), "michelle.rule.%s", rule->code);
        for (char *c = expected; *c != '\0'; c++) {
            *c = *c == '_' ? '-' : *c;
        }
        assert(strcmp(id, expected) == 0);
    }
    assert(evaluators == CRITIC_EVALUATOR_COUNT);
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        const char *code = critic_evaluator_code((CriticEvaluator)i);
        CriticEvaluator back;
        size_t matches = 0;
        const ComposerRule *row = rule_named(code, &matches);
        char key[64];
        snprintf(key, sizeof(key), "critic.weights.%s", code);
        assert(seen[i] && critic_evaluator_from_code(code, &back) && back == (CriticEvaluator)i);
        assert(matches == 1 && !row->hard && strcmp(row->key, key) == 0);
    }
    assert(critic_evaluator_code(CRITIC_EVALUATOR_COUNT) == NULL && critic_evaluator_function(CRITIC_EVALUATOR_COUNT) == NULL);
    CriticEvaluator untouched = CRITIC_WIDE_RANGE;
    assert(!critic_evaluator_from_code("colour", &untouched) && !critic_evaluator_from_code(NULL, &untouched) && untouched == CRITIC_WIDE_RANGE);
    knowledge_snapshot_destroy(snapshot);
}

static void test_constraints(void) {
    KnowledgeSnapshot *snapshot = build();
    size_t rule_count = 0;
    const ComposerRule *rules = composer_preferences_rules(&rule_count);
    size_t constraints = 0;
    size_t hard = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const char *id = knowledge_snapshot_entry_at(snapshot, i)->id;
        const RuleEntry *rule = starts_with(id, "fux.rule.") ? knowledge_snapshot_rule(snapshot, id) : NULL;
        if (rule == NULL || rule->subtype != RULE_CONSTRAINT) {
            continue;
        }
        size_t matches = 0;
        const ComposerRule *row = rule_named(rule->code, &matches);
        constraints++;
        assert(matches == 1 && row->hard && strcmp(row->key, "melody.waive") == 0);
        assert(strcmp(rule->setting, row->key) == 0 && rule->waivable);
    }
    for (size_t i = 0; i < rule_count; i++) {
        if (!rules[i].hard) {
            continue;
        }
        char id[96];
        snprintf(id, sizeof(id), "fux.rule.%s", rules[i].name);
        const RuleEntry *rule = knowledge_snapshot_rule(snapshot, id);
        hard++;
        assert(rule != NULL && rule->subtype == RULE_CONSTRAINT && strcmp(rule->code, rules[i].name) == 0);
    }
    assert(constraints == 7 && hard == 7);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_evaluators();
    test_constraints();
    printf("test_critic_rules: ok\n");
    return 0;
}
