#include <stdio.h>
#include <string.h>

#include <model/time/meter_arithmetic.h>
#include <theory/form/hybrid_rules.h>
#include <theory/knowledge/rule_entry.h>
#include <theory/vocabulary/contrast_aspect.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/form_hear.h>

#include "check/form_internal.h"
#include "check/hearing.h"
#include "critic/critic_internal.h"
#include "grow/grow_internal.h"

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

/* ---------------------------------------------------------------------------------------- the hearing */

/* Appends an unscored observation by the rule `code`; a full judgment drops it. */
static CriticResult add_observation(const KnowledgeSnapshot *knowledge, const char *code, const CriticFinding *finding,
                                    const CriticFields *fields, int bar, CriticJudgment *judgment) {
    const char *template = template_of(knowledge, code);
    if (template == NULL) {
        return CRITIC_ERR_KNOWLEDGE;
    }
    if (judgment->observation_count == CRITIC_OBSERVATION_MAX) {
        return CRITIC_OK;
    }
    CriticObservation *observation = &judgment->observations[judgment->observation_count++];
    memset(observation, 0, sizeof(*observation));
    snprintf(observation->rule, sizeof(observation->rule), "%s", code);
    observation->bar = bar;
    observation->beat = rational_ratio(1, 1);
    observation->penalty = rational_ratio(0, 1);
    critic_message(template, finding, fields, observation->message, sizeof(observation->message));
    return CRITIC_OK;
}

/* A finding that only names a place and what was measured there: observations have no evaluator. */
static void blank_finding(CriticFinding *finding, int bar) {
    memset(finding, 0, sizeof(*finding));
    finding->bar = bar;
    finding->beat = rational_ratio(1, 1);
    finding->penalty = rational_ratio(0, 1);
    finding->value = rational_ratio(0, 1);
    finding->limit = rational_ratio(0, 1);
}

/* The bar number the export writes for the bar of the render that holds `time`. */
static int bar_number_at(const RenderedPlan *rendered, Rational time) {
    int measure = 0;
    if (rendered == NULL || rendered->meter_map == NULL || meter_map_measure_of(rendered->meter_map, time, &measure) != METER_MAP_OK) {
        return -1;
    }
    return measure + 1 - meter_map_first_counted_measure(rendered->meter_map);
}

static CriticResult observe_cadences(ComposerHearing *hearing, NodeId theme, const KnowledgeSnapshot *knowledge, int first_bar,
                                     CriticJudgment *judgment) {
    AnalyzerCheck check;
    memset(&check, 0, sizeof(check));
    if (analyzer_check_section_in(hearing, theme, &check) != ANALYZER_CHECK_OK) {
        return CRITIC_OK; /* a section the check cannot read is not held against the plan */
    }
    CriticResult status = CRITIC_OK;
    for (size_t i = 0; i < check.item_count && status == CRITIC_OK; i++) {
        const AnalyzerCheckItem *item = &check.items[i];
        if (item->kind != ANALYZER_CHECK_CADENCE || item->agreed) {
            continue;
        }
        CriticFinding finding;
        CriticFields fields;
        int bar = first_bar < 0 ? -1 : first_bar + (int)item->bar;
        blank_finding(&finding, bar);
        memset(&fields, 0, sizeof(fields));
        snprintf(fields.planned, sizeof(fields.planned), "%s", item->planned);
        snprintf(fields.heard, sizeof(fields.heard), "%s", item->heard);
        status = add_observation(knowledge, "cadence_heard", &finding, &fields, bar, judgment);
    }
    analyzer_check_destroy(&check);
    return status;
}

/* The form a pairing asks for (a PhraseLevelForm), or -1. */
static int requested_form(const SectionPairing *pairing) {
    switch (grow_pairing_archetype(pairing)) {
    case PLAN_ARCHETYPE_SENTENCE:
        return PHRASE_LEVEL_FORM_SENTENCE;
    case PLAN_ARCHETYPE_PERIOD:
        return PHRASE_LEVEL_FORM_PERIOD;
    case PLAN_ARCHETYPE_HYBRID:
        return PHRASE_LEVEL_FORM_HYBRID;
    default:
        return -1;
    }
}

/* theme_heard: the form heard is not the form asked for; a hybrid is heard as asked only with the beginning and the ending asked for (compose_run). */
static CriticResult observe_theme(ComposerHearing *hearing, const Plan *plan, NodeId theme, const KnowledgeSnapshot *knowledge,
                                  int first_bar, CriticJudgment *judgment) {
    const SectionDescription *description = plan_section_or_empty(plan, theme);
    int wanted = description->has_pairing ? requested_form(&description->pairing) : -1;
    FormHeard heard;
    memset(&heard, 0, sizeof(heard));
    if (wanted < 0 || form_hear_in(hearing, plan, plan_node_span(plan, theme), &heard) != FORM_HEAR_OK) {
        return CRITIC_OK;
    }
    int heard_form = heard.count > 0 ? heard.themes[0].form : -1;
    bool as_requested = heard_form >= 0 && heard_form == wanted &&
                        (wanted != PHRASE_LEVEL_FORM_HYBRID || (heard.themes[0].beginning == grow_phrase_beginning(description->pairing.beginning) &&
                                                                heard.themes[0].ending == grow_phrase_ending(description->pairing.ending)));
    CriticResult status = CRITIC_OK;
    if (!as_requested) {
        CriticFinding finding;
        CriticFields fields;
        blank_finding(&finding, first_bar);
        memset(&fields, 0, sizeof(fields));
        snprintf(fields.planned, sizeof(fields.planned), "%s", form_hear_form_name(wanted));
        snprintf(fields.heard, sizeof(fields.heard), "%s", heard_form >= 0 ? form_hear_form_name(heard_form) : "no theme");
        status = add_observation(knowledge, "theme_heard", &finding, &fields, first_bar, judgment);
    }
    form_heard_destroy(&heard);
    return status;
}

/* contrast_heard: a contrast constraint of the theme's ideas whose aspect the melody measures under its amount. */
static CriticResult observe_contrasts(const Plan *plan, NodeId theme, const CriticLine *line, const KnowledgeSnapshot *knowledge,
                                      CriticJudgment *judgment) {
    PlanSpan span = plan_node_span(plan, theme);
    CriticResult status = CRITIC_OK;
    for (NodeId id = 1; id <= plan_node_count(plan) && status == CRITIC_OK; id++) {
        const PlanConstraint *constraint = plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_IDEA && plan_span_contains(span, plan_node_span(plan, id))
                                               ? plan_node_find_constraint(plan, id, PLAN_CONSTRAINT_CONTRAST)
                                               : NULL;
        if (constraint == NULL) {
            continue;
        }
        for (size_t p = 0; p < line->pair_count; p++) {
            const CriticIdeaPair *pair = &line->pairs[p];
            ContrastVector vector;
            bool measured = false;
            if (strcmp(pair->idea, plan_node_name(plan, id)) != 0 || strcmp(pair->against, constraint->params.contrast.against) != 0) {
                continue;
            }
            status = critic_pair_contrast(line, pair, &vector, &measured);
            for (size_t a = 0; measured && status == CRITIC_OK && a < constraint->params.contrast.aspect_count; a++) {
                const PlanContrastAspect *aspect = &constraint->params.contrast.aspects[a];
                Rational heard = vector.distance[aspect->aspect];
                if (rational_cmp(heard, aspect->amount) >= 0) {
                    continue;
                }
                CriticFinding finding;
                CriticFields fields;
                int bar = pair->idea_count > 0 ? line->notes[pair->idea_first].bar : -1;
                blank_finding(&finding, bar);
                memset(&fields, 0, sizeof(fields));
                snprintf(finding.idea, sizeof(finding.idea), "%s", pair->idea);
                snprintf(finding.against, sizeof(finding.against), "%s", pair->against);
                snprintf(finding.aspect, sizeof(finding.aspect), "%s", contrast_aspect_name(aspect->aspect));
                finding.value = heard;
                fields.amount = aspect->amount;
                fields.has_amount = true;
                status = add_observation(knowledge, "contrast_heard", &finding, &fields, bar, judgment);
            }
        }
    }
    return status;
}

CriticResult critic_observe(ComposerHearing *hearing, const Plan *plan, NodeId theme, const CriticLine *line, const KnowledgeSnapshot *knowledge,
                            CriticJudgment *inout) {
    if (hearing == NULL || plan == NULL || line == NULL || inout == NULL) {
        return CRITIC_ERR_NULL;
    }
    if (!plan_has_node(plan, theme)) {
        return CRITIC_ERR_RENDER;
    }
    int first_bar = bar_number_at(hearing_rendered(hearing), plan_node_span(plan, theme).start);
    CriticResult status = observe_cadences(hearing, theme, knowledge, first_bar, inout);
    status = status == CRITIC_OK ? observe_theme(hearing, plan, theme, knowledge, first_bar, inout) : status;
    return status == CRITIC_OK ? observe_contrasts(plan, theme, line, knowledge, inout) : status;
}
