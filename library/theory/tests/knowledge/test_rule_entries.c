#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K13 rule entries (R-717): the 21 repository entries load with their subtype, scope, weight and parameters (fourteen from
 * michelle: ten evaluators with weight 1 and four observations; seven constraints from fux, every one at stage melody with the
 * setting melody.waive). A user file with a rule of an unknown subtype, a weight of 21, a parameter 1/0 or a message placeholder
 * outside the list is refused with the file, the id and the field.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static const RuleParameter *parameter(const RuleEntry *rule, const char *name) {
    for (size_t i = 0; i < rule->parameter_count; i++) {
        if (strcmp(rule->parameters[i].name, name) == 0) {
            return &rule->parameters[i];
        }
    }
    return NULL;
}

static void check_parameter(const RuleEntry *rule, const char *name, long long num, long long den) {
    const RuleParameter *found = parameter(rule, name);
    assert(found != NULL && found->value.num == num && found->value.den == den);
}

static void test_counts(void) {
    KnowledgeSnapshot *snapshot = build();
    size_t evaluators = 0, observations = 0, constraints = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry_at(snapshot, i);
        const RuleEntry *rule = knowledge_snapshot_rule(snapshot, entry->id);
        assert((rule != NULL) == (strcmp(entry->kind, "rule") == 0));
        if (rule == NULL) {
            continue;
        }
        assert(strcmp(rule->id, entry->id) == 0 && rule->code != NULL && rule->message != NULL && rule->function != NULL);
        if (rule->subtype == RULE_EVALUATOR) {
            evaluators++;
            assert(rule->default_weight == 1 && strncmp(entry->id, "michelle.rule.", 14) == 0);
        } else if (rule->subtype == RULE_OBSERVATION) {
            observations++;
            assert(strncmp(entry->id, "michelle.rule.", 14) == 0);
        } else {
            constraints++;
            assert(strncmp(entry->id, "fux.rule.", 9) == 0);
            assert(strcmp(rule->stage, "melody") == 0 && rule->waivable && strcmp(rule->setting, "melody.waive") == 0);
        }
    }
    assert(evaluators == 10 && observations == 4 && constraints == 7);
    knowledge_snapshot_destroy(snapshot);
}

static void test_fields(void) {
    KnowledgeSnapshot *snapshot = build();
    const RuleEntry *rule = knowledge_snapshot_rule(snapshot, "michelle.rule.rhythmic-monotony");
    assert(rule != NULL && rule->subtype == RULE_EVALUATOR && rule->scope == RULE_SCOPE_PHRASE);
    assert(strcmp(rule->code, "rhythmic_monotony") == 0 && strcmp(rule->function, "critic_rhythmic_monotony") == 0);
    assert(rule->parameter_count == 1);
    check_parameter(rule, "run", 3, 1);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.contrast-distance");
    assert(rule != NULL && rule->parameter_count == 3);
    check_parameter(rule, "least", 1, 2);
    check_parameter(rule, "register", 7, 1);
    check_parameter(rule, "register_span", 5, 1);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.wide-range");
    assert(rule != NULL && rule->scope == RULE_SCOPE_LINE && rule->parameter_count == 2);
    check_parameter(rule, "limit", 16, 1);
    check_parameter(rule, "span", 8, 1);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.leap-load");
    assert(rule != NULL);
    check_parameter(rule, "allowance", 1, 2);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.open-ending");
    assert(rule != NULL && rule->parameter_count == 0);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.one-rhythm");
    assert(rule != NULL && rule->scope == RULE_SCOPE_SET);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.voice-fault-heard");
    assert(rule != NULL && rule->subtype == RULE_OBSERVATION && rule->scope == RULE_SCOPE_LINE_PAIR);
    assert(strcmp(rule->function, "analyzer_check_piece_in") == 0);
    rule = knowledge_snapshot_rule(snapshot, "michelle.rule.cadence-heard");
    assert(rule != NULL && rule->scope == RULE_SCOPE_SECTION);
    rule = knowledge_snapshot_rule(snapshot, "fux.rule.parallel-fifths");
    assert(rule != NULL && rule->subtype == RULE_CONSTRAINT && rule->scope == RULE_SCOPE_LINE_PAIR);
    assert(strcmp(rule->function, "standard_counterpoint_harmonic_check") == 0);
    rule = knowledge_snapshot_rule(snapshot, "fux.rule.strong-beat-chord-tone");
    assert(rule != NULL && rule->scope == RULE_SCOPE_NOTE && strcmp(rule->function, "melody_check") == 0);
    assert(knowledge_snapshot_rule(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_rule(snapshot, "michelle.rule.nothing") == NULL);
    assert(knowledge_snapshot_rule(NULL, "michelle.rule.wide-range") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void refused(const char *subtype, const char *content, const char *id, const char *field) {
    char text[1536];
    snprintf(text, sizeof(text),
             "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[{\"id\":\"%s\",\"kind\":\"rule\","
             "\"subtype\":\"%s\",\"name\":\"mine\",\"content\":%s,\"citations\":[],\"status\":\"reference\",\"version\":1}]}",
             id, subtype, content);
    KnowledgeUserFile file = {"mine.json", text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_FIELD);
    assert(snapshot == NULL);
    assert(strstr(message, "mine.json") != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

static void test_refusals(void) {
    const char *head = "{\"code\":\"mine\",\"scope\":\"line\",\"function\":\"critic_mine\",";
    char content[512];
    refused("judgment", "{\"code\":\"mine\"}", "user.rule.mine", "subtype");
    snprintf(content, sizeof content, "%s\"message\":\"too wide\",\"default_weight\":21}", head);
    refused("evaluator", content, "user.rule.heavy", "content.default_weight");
    snprintf(content, sizeof content, "%s\"message\":\"too wide\",\"default_weight\":1,\"parameters\":{\"run\":\"1/0\"}}", head);
    refused("evaluator", content, "user.rule.zero", "content.parameters.run");
    snprintf(content, sizeof content, "%s\"message\":\"the {colour} is wide\",\"default_weight\":1}", head);
    refused("evaluator", content, "user.rule.colour", "content.message");
    snprintf(content, sizeof content, "%s\"message\":\"the {bar is wide\",\"default_weight\":1}", head);
    refused("evaluator", content, "user.rule.brace", "content.message");
    snprintf(content, sizeof content, "%s\"message\":\"too wide\",\"default_weight\":1,\"parameters\":{\"run\":1.5}}", head);
    refused("evaluator", content, "user.rule.half", "content.parameters.run");
    snprintf(content, sizeof content, "%s\"message\":\"too wide\",\"stage\":\"melody\",\"waivable\":true}", head);
    refused("constraint", content, "user.rule.nosetting", "content.setting");
    snprintf(content, sizeof content, "{\"code\":\"mine\",\"scope\":\"galaxy\",\"function\":\"critic_mine\",\"message\":\"too wide\"}");
    refused("observation", content, "user.rule.scope", "content.scope");
}

int main(void) {
    test_counts();
    test_fields();
    test_refusals();
    return 0;
}
