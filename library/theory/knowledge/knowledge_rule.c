#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K13 rule: the common fields, then the fields of the entry's subtype. */

static const char *const SUBTYPES[] = {"constraint", "evaluator", "observation"};
static const char *const SCOPES[] = {"note", "line", "line-pair", "sonority", "progression", "phrase", "section", "movement", "performer-part", "set"};
static const char *const STAGES[] = {"motive", "harmony", "melody", "texture", "counterpoint"};
/* The names a message may put between braces: the fields of a finding the critic fills in. */
static const char *const PLACEHOLDERS[] = {"bar",     "beat",  "first",  "last",   "count",  "value", "limit",
                                           "planned", "heard", "idea",   "against", "aspect", "amount"};
#define PLACEHOLDER_COUNT (sizeof PLACEHOLDERS / sizeof PLACEHOLDERS[0])

/* Lowercase letter words joined by single hyphens or underscores: "rhythmic_monotony", "forbidden-leap". */
static bool code_text(const char *text) {
    size_t length = strlen(text);
    if (length == 0 || text[0] < 'a' || text[0] > 'z' || text[length - 1] < 'a' || text[length - 1] > 'z') {
        return false;
    }
    for (size_t i = 1; i + 1 < length; i++) {
        bool letter = text[i] >= 'a' && text[i] <= 'z';
        bool separator = text[i] == '_' || text[i] == '-';
        if (!letter && (!separator || text[i + 1] < 'a' || text[i + 1] > 'z')) {
            return false;
        }
    }
    return true;
}

/* A C function name: lowercase letters, digits and underscores, starting with a letter. */
static bool function_text(const char *text) {
    if (text[0] < 'a' || text[0] > 'z') {
        return false;
    }
    for (size_t i = 1; text[i] != '\0'; i++) {
        bool ok = (text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') || text[i] == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool name_text(const char *text) {
    size_t length = strlen(text);
    if (length == 0 || length >= RULE_PARAMETER_NAME_MAX) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (!(text[i] >= 'a' && text[i] <= 'z') && text[i] != '_') {
            return false;
        }
    }
    return true;
}

/* The first placeholder of `message` that is not in the list (copied into `bad`), or false when every one is. A brace that never
   closes counts as a placeholder with the rest of the text as its name. */
static bool unknown_placeholder(const char *message, char *bad, size_t bad_size) {
    for (const char *p = message; *p != '\0'; p++) {
        if (*p != '{') {
            continue;
        }
        const char *close = strchr(p, '}');
        size_t length = close == NULL ? strlen(p + 1) : (size_t)(close - p - 1);
        bool known = false;
        for (size_t i = 0; close != NULL && i < PLACEHOLDER_COUNT; i++) {
            known = known || (strlen(PLACEHOLDERS[i]) == length && strncmp(p + 1, PLACEHOLDERS[i], length) == 0);
        }
        if (!known) {
            snprintf(bad, bad_size, "%.*s", (int)length, p + 1);
            return true;
        }
        p = close;
    }
    return false;
}

/* "n/d" with a positive denominator. */
static bool read_ratio(const char *text, Rational *out) {
    const char *p = text;
    bool negative = *p == '-';
    p += negative ? 1 : 0;
    long long num = 0;
    long long den = 0;
    size_t digits = 0;
    for (; *p >= '0' && *p <= '9' && digits < 12; p++, digits++) {
        num = num * 10 + (*p - '0');
    }
    if (digits == 0 || *p != '/') {
        return false;
    }
    p++;
    digits = 0;
    for (; *p >= '0' && *p <= '9' && digits < 12; p++, digits++) {
        den = den * 10 + (*p - '0');
    }
    if (digits == 0 || *p != '\0' || den == 0) {
        return false;
    }
    return rational_make(negative ? -num : num, den, out) == RATIONAL_OK;
}

static KnowledgeStatus read_parameters(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const cJSON *parameters = knowledge_member(content, "parameters");
    RuleEntry *rule = &slot->rule;
    if (parameters == NULL) {
        return KNOWLEDGE_OK;
    }
    if (!cJSON_IsObject(parameters)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.parameters", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    for (const cJSON *item = parameters->child; item != NULL; item = item->next) {
        char field[64];
        snprintf(field, sizeof field, "content.parameters.%.24s", item->string);
        if (rule->parameter_count == RULE_PARAMETER_MAX) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content.parameters", "more than 4 parameters");
            return KNOWLEDGE_ERR_FIELD;
        }
        RuleParameter *parameter = &rule->parameters[rule->parameter_count];
        if (!name_text(item->string)) {
            knowledge_fail(message, message_size, path, slot->entry.id, field, "the name is not lowercase letters and underscores");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (knowledge_is_integer(item)) {
            if (rational_from_int(item->valueint, &parameter->value) != RATIONAL_OK) {
                knowledge_fail(message, message_size, path, slot->entry.id, field, "not an integer or n/d");
                return KNOWLEDGE_ERR_FIELD;
            }
        } else if (!cJSON_IsString(item) || !read_ratio(item->valuestring, &parameter->value)) {
            knowledge_fail(message, message_size, path, slot->entry.id, field, "not an integer or n/d with a positive denominator");
            return KNOWLEDGE_ERR_FIELD;
        }
        snprintf(parameter->name, sizeof parameter->name, "%s", item->string);
        rule->parameter_count++;
    }
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_evaluator(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const cJSON *weight = knowledge_member(content, "default_weight");
    if (!knowledge_in_range(weight, 0, 20)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.default_weight",
                       weight == NULL ? "missing" : "not an integer from 0 to 20");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->rule.default_weight = weight->valueint;
    return read_parameters(slot, content, path, message, message_size);
}

static KnowledgeStatus read_constraint(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    int index = 0;
    KnowledgeStatus status = knowledge_read_word(slot, content, "stage", STAGES, 5, "not motive, harmony, melody, texture or counterpoint",
                                                 &index, path, message, message_size);
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    slot->rule.stage = STAGES[index];
    const cJSON *waivable = knowledge_member(content, "waivable");
    if (!cJSON_IsBool(waivable)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.waivable", waivable == NULL ? "missing" : "not true or false");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->rule.waivable = cJSON_IsTrue(waivable);
    return knowledge_read_text(slot, content, "setting", &slot->rule.setting, path, message, message_size);
}

KnowledgeStatus knowledge_rule_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                    size_t message_size) {
    int index = knowledge_word_index(subtype, SUBTYPES, 3);
    if (index < 0) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype",
                       subtype == NULL ? "missing" : "not constraint, evaluator or observation");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *content = knowledge_member(entry, "content");
    if (!cJSON_IsObject(content)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    RuleEntry *rule = &slot->rule;
    rule->id = slot->entry.id;
    rule->name = slot->entry.name;
    rule->subtype = (RuleSubtype)index;
    const char *code = knowledge_string_of(knowledge_member(content, "code"));
    if (code == NULL || !code_text(code)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.code", code == NULL ? "missing" : "not lowercase words");
        return KNOWLEDGE_ERR_FIELD;
    }
    rule->code = knowledge_own(slot, code);
    if (rule->code == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    int scope = 0;
    KnowledgeStatus status = knowledge_read_word(slot, content, "scope", SCOPES, 10, "not a scope of the rule kind", &scope, path, message,
                                                 message_size);
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    rule->scope = (RuleScope)scope;
    const char *text = knowledge_string_of(knowledge_member(content, "message"));
    char bad[32];
    if (text == NULL || strlen(text) > 200) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.message",
                       text == NULL ? "missing" : "longer than 200 characters");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (unknown_placeholder(text, bad, sizeof bad)) {
        char problem[96];
        snprintf(problem, sizeof problem, "unknown placeholder {%s}", bad);
        knowledge_fail(message, message_size, path, slot->entry.id, "content.message", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    rule->message = knowledge_own(slot, text);
    const char *function = knowledge_string_of(knowledge_member(content, "function"));
    if (function == NULL || !function_text(function)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.function",
                       function == NULL ? "missing" : "not a lowercase name");
        return KNOWLEDGE_ERR_FIELD;
    }
    rule->function = knowledge_own(slot, function);
    if (rule->message == NULL || rule->function == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    if (rule->subtype == RULE_EVALUATOR) {
        return read_evaluator(slot, content, path, message, message_size);
    }
    return rule->subtype == RULE_CONSTRAINT ? read_constraint(slot, content, path, message, message_size) : KNOWLEDGE_OK;
}
