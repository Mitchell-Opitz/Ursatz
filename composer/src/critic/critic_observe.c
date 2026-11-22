#include <stdio.h>
#include <string.h>

#include <theory/knowledge/rule_entry.h>

#include <ursatz_composer/critic.h>

/* The critic's words (R-720): a message from the template of an entry, and the scored observations of a judgment. */

/* Appends text to out at *used, always leaving room for the end. */
static void put(char *out, size_t size, size_t *used, const char *text) {
    for (size_t i = 0; text[i] != '\0' && *used + 1 < size; i++) {
        out[(*used)++] = text[i];
    }
}

/* A Rational as an integer, or n/d. */
static void rational_text(Rational value, char *text, size_t size) {
    if (value.den == 1) {
        snprintf(text, size, "%lld", value.num);
    } else {
        snprintf(text, size, "%lld/%lld", value.num, value.den);
    }
}

/* The text of the placeholder `name`, or false when the finding and the fields hold no value for it. */
static bool field_text(const char *name, const CriticFinding *finding, const CriticFields *fields, char *text, size_t size) {
    if (finding != NULL) {
        if (strcmp(name, "bar") == 0 && finding->bar >= 0) {
            snprintf(text, size, "%d", finding->bar);
            return true;
        }
        if (strcmp(name, "beat") == 0 && finding->bar >= 0) {
            Rational beat = finding->beat.den > 0 ? finding->beat : rational_ratio(1, 1);
            rational_text(beat, text, size);
            return true;
        }
        if (strcmp(name, "first") == 0 || strcmp(name, "last") == 0 || strcmp(name, "count") == 0) {
            snprintf(text, size, "%d", strcmp(name, "first") == 0 ? finding->first : (strcmp(name, "last") == 0 ? finding->last : finding->count));
            return true;
        }
        if (strcmp(name, "value") == 0 || strcmp(name, "limit") == 0) {
            rational_text(strcmp(name, "value") == 0 ? finding->value : finding->limit, text, size);
            return true;
        }
        const char *string = strcmp(name, "idea") == 0 ? finding->idea : (strcmp(name, "against") == 0 ? finding->against : (strcmp(name, "aspect") == 0 ? finding->aspect : NULL));
        if (string != NULL && string[0] != '\0') {
            snprintf(text, size, "%s", string);
            return true;
        }
    }
    if (fields != NULL) {
        if ((strcmp(name, "planned") == 0 && fields->planned[0] != '\0') || (strcmp(name, "heard") == 0 && fields->heard[0] != '\0')) {
            snprintf(text, size, "%s", strcmp(name, "planned") == 0 ? fields->planned : fields->heard);
            return true;
        }
        if (strcmp(name, "amount") == 0 && fields->has_amount) {
            rational_text(fields->amount, text, size);
            return true;
        }
    }
    return false;
}

size_t critic_message(const char *template, const CriticFinding *finding, const CriticFields *fields, char *out, size_t size) {
    size_t used = 0;
    if (out == NULL || size == 0) {
        return 0;
    }
    for (const char *p = template != NULL ? template : ""; *p != '\0'; p++) {
        const char *close = *p == '{' ? strchr(p, '}') : NULL;
        char name[24];
        char text[64];
        if (close != NULL && (size_t)(close - p) < sizeof(name)) {
            snprintf(name, sizeof(name), "%.*s", (int)(close - p - 1), p + 1);
            if (field_text(name, finding, fields, text, sizeof(text))) {
                put(out, size, &used, text);
                p = close;
                continue;
            }
        }
        char one[2] = {*p, '\0'};
        put(out, size, &used, one);
    }
    out[used] = '\0';
    return used;
}

/* The template of the entry michelle.rule.<code> (underscores of the code as hyphens). */
static const char *template_of(const KnowledgeSnapshot *knowledge, const char *code) {
    char id[64];
    snprintf(id, sizeof(id), "michelle.rule.%s", code);
    for (char *c = id; *c != '\0'; c++) {
        *c = *c == '_' ? '-' : *c;
    }
    const RuleEntry *rule = knowledge_snapshot_rule(knowledge, id);
    return rule != NULL ? rule->message : NULL;
}

CriticResult critic_judge(const CriticLine *line, const CriticLine *const *set, size_t set_count, const KnowledgeSnapshot *knowledge,
                          const CriticSettings *settings, CriticJudgment *out) {
    if (line == NULL || settings == NULL || out == NULL) {
        return CRITIC_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    CriticResult status = critic_evaluate(line, set, set_count, knowledge, out->findings);
    if (status != CRITIC_OK) {
        return status;
    }
    out->score = critic_score(out->findings, settings);
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        const CriticFinding *finding = &out->findings[i];
        if (finding->penalty.num <= 0) {
            continue;
        }
        const char *template = template_of(knowledge, critic_evaluator_code((CriticEvaluator)i));
        CriticObservation *observation = &out->observations[out->observation_count++];
        snprintf(observation->rule, sizeof(observation->rule), "%s", critic_evaluator_code((CriticEvaluator)i));
        observation->scored = true;
        observation->bar = finding->bar;
        observation->beat = finding->beat;
        observation->penalty = finding->penalty;
        critic_message(template, finding, NULL, observation->message, sizeof(observation->message));
    }
    return CRITIC_OK;
}
