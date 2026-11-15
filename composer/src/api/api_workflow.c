#include "api_workflow.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <semantics/cadence_type.h>
#include <ursatz_analyzer/staleness.h>

#include <ursatz_composer/explain.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "util/plan_util.h"
#include "util/util.h"

/* --- the sections of a plan ------------------------------------------------------------------------ */

static const char *const BEGINNING_NAMES[] = {"presentation", "antecedent", "compound basic idea"};
static const char *const ENDING_NAMES[] = {"continuation", "consequent", "cadential"};

const char *api_key_name(const AnalysisKey *key, char *out, size_t size) {
    util_key_name(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, out, size);
    return out;
}

size_t api_bars_in(const PiecePlan *plan, Rational length) {
    LaneSpanId meters[1];
    PlanSpan whole = plan_node_span(plan, plan_root(plan));
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, whole, meters, 1) == 0) {
        return 0;
    }
    const PlanMeter *meter = &plan_lane_span_payload(plan, meters[0])->meter;
    long numerator = length.num * (long)meter->beat_unit;
    long denominator = (long)length.den * meter->beats_per_bar;
    return denominator > 0 ? (size_t)((numerator + denominator / 2) / denominator) : 0;
}

size_t api_plan_sections(const PiecePlan *plan, ApiSection *out, size_t capacity) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        if (plan_node_kind(plan, node) != PLAN_NODE_SECTION || plan_node_has_constraint(plan, node, PLAN_CONSTRAINT_MATERIAL)) {
            continue;
        }
        if (count < capacity) {
            ApiSection *section = &out[count];
            memset(section, 0, sizeof(*section));
            section->node = node;
            const char *name = plan_node_name(plan, node);
            snprintf(section->name, sizeof(section->name), "%s", name ? name : "");
            snprintf(section->letter, sizeof(section->letter), "%.*s", (int)sizeof(section->letter) - 1, section->name);
            section->bars = api_bars_in(plan, plan_node_span(plan, node).length);
            for (size_t c = 0; c < plan_node_constraint_count(plan, node); c++) {
                const PlanConstraint *constraint = plan_node_constraint_at(plan, node, c);
                switch (constraint->kind) {
                case PLAN_CONSTRAINT_SECTION_LETTER:
                    memcpy(section->letter, constraint->params.letter, sizeof(section->letter));
                    break;
                case PLAN_CONSTRAINT_KEY:
                    section->has_key = true;
                    section->key = constraint->params.key;
                    break;
                case PLAN_CONSTRAINT_CADENCE:
                    if (section->cadence_count < 2) {
                        section->cadences[section->cadence_count++] = constraint->params.cadence;
                    }
                    break;
                case PLAN_CONSTRAINT_THEME_PAIRING:
                    section->has_pairing = true;
                    section->pairing = constraint->params.theme_pairing;
                    break;
                case PLAN_CONSTRAINT_RETURN: {
                    section->returns = true;
                    section->variation = constraint->params.section_return.variation;
                    const char *source = plan_node_name(plan, constraint->params.section_return.section);
                    snprintf(section->returns_letter, sizeof(section->returns_letter), "%.*s",
                             (int)sizeof(section->returns_letter) - 1, source ? source : "");
                    for (size_t c2 = 0; c2 < plan_node_constraint_count(plan, constraint->params.section_return.section); c2++) {
                        const PlanConstraint *letter =
                            plan_node_constraint_at(plan, constraint->params.section_return.section, c2);
                        if (letter->kind == PLAN_CONSTRAINT_SECTION_LETTER) {
                            memcpy(section->returns_letter, letter->params.letter, sizeof(section->returns_letter));
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        count++;
    }
    return count;
}

cJSON *api_section_json(const ApiSection *section) {
    char key[24] = "";
    cJSON *object = cJSON_CreateObject();
    cJSON *cadences = cJSON_CreateArray();
    bool ok = object && cadences && cJSON_AddStringToObject(object, "letter", section->letter) &&
              cJSON_AddNumberToObject(object, "bars", (double)section->bars);
    ok = ok && api_add(object, "key", section->has_key ? cJSON_CreateString(api_key_name(&section->key, key, sizeof key)) : cJSON_CreateNull());
    for (size_t c = 0; ok && c < section->cadence_count; c++) {
        ok = cJSON_AddItemToArray(cadences, cJSON_CreateString(plan_cadence_name(section->cadences[c])));
    }
    if (ok) {
        ok = api_add(object, "cadences", cadences);
        cadences = NULL;
    }
    if (ok && section->has_pairing) {
        char pairing[64];
        snprintf(pairing, sizeof pairing, "%s + %s", BEGINNING_NAMES[section->pairing.beginning],
                 ENDING_NAMES[section->pairing.ending]);
        ok = cJSON_AddStringToObject(object, "pairing", pairing) != NULL;
    } else {
        ok = ok && cJSON_AddNullToObject(object, "pairing");
    }
    if (ok && section->returns) {
        cJSON *returns = cJSON_CreateObject();
        ok = returns && cJSON_AddStringToObject(returns, "section", section->returns_letter) &&
             cJSON_AddStringToObject(returns, "variation", plan_return_variation_name(section->variation)) &&
             api_add(object, "returns", returns);
    } else {
        ok = ok && cJSON_AddNullToObject(object, "returns");
    }
    cJSON_Delete(cadences);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* --- the plan of a request ------------------------------------------------------------------------- */

ComposerStatus api_read_plan_text(const cJSON *request, const char *name, char **text, ComposerError *error) {
    char path[64];
    snprintf(path, sizeof path, "/%s", name);
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(request, name);
    *text = NULL;
    if (!cJSON_IsObject(member)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "the request needs \"%s\": a plan object", name);
    }
    char *printed = cJSON_PrintUnformatted(member);
    if (!printed) {
        return api_fail_alloc(error);
    }
    PiecePlan *plan = NULL;
    PlanJsonError detail;
    PlanResult read = plan_from_json(printed, &plan, &detail);
    if (read != PLAN_OK) {
        if (read == PLAN_ERR_FORMAT && analyzer_document_status(printed) != ANALYZER_DOCUMENT_UNREADABLE) {
            cJSON_free(printed);
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path,
                            "the input is an Ursatz-Analyzer analysis document, not a plan; it describes a piece that is "
                            "already written (load an analysis with plan_from_analyzer_result)");
        }
        cJSON_free(printed);
        if (read == PLAN_ERR_ALLOC) {
            return api_fail_alloc(error);
        }
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "cannot read plan: %s%s%s", detail.message,
                        detail.path[0] ? " at " : "", detail.path);
    }
    cJSON_free(printed);
    *text = plan_to_json(plan, &detail);
    plan_destroy(plan);
    if (!*text) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "cannot read plan: %s", detail.message);
    }
    return COMPOSER_OK;
}

ComposerStatus api_plan_from_text(const char *text, const char *name, PiecePlan **out, ComposerError *error) {
    PlanJsonError detail;
    if (plan_from_json(text, out, &detail) != PLAN_OK) {
        char path[64];
        snprintf(path, sizeof path, "/%s", name);
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "cannot read plan: %s", detail.message);
    }
    return COMPOSER_OK;
}

/* --- what a request asks to have made -------------------------------------------------------------- */

ComposerStatus api_read_outputs(const cJSON *request, bool workflow, ApiOutputs *out, ComposerError *error) {
    memset(out, 0, sizeof(*out));
    out->export_options = plan_export_options_default();
    bool present = false;
    const char *explain = NULL;
    if (!api_member_bool(request, "midi", &out->midi, &present, error) ||
        !api_member_bool(request, "musicxml", &out->musicxml, &present, error) ||
        !api_member_bool(request, "plain_midi", &out->plain_midi, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (workflow && (!api_member_bool(request, "record", &out->record, &present, error) ||
                     !api_member_string(request, "explain", &explain, &present, error))) {
        return COMPOSER_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            out->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            out->explain = 2;
        } else {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    if (out->plain_midi) {
        out->export_options.track_names = false;
        out->export_options.section_markers = false;
    }
    return api_read_tempo(request, &out->export_options, error);
}

ComposerStatus api_render_outputs(const PiecePlan *plan, const ApiOutputs *outputs, ComposerResponse *out) {
    PlanJsonError plan_error;
    out->plan_json = plan_to_json(plan, &plan_error);
    if (!out->plan_json) {
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "cannot write the plan: %s", plan_error.message);
    }
    if (!outputs->midi && !outputs->musicxml) {
        return COMPOSER_OK;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "/midi", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(&rendered, &outputs->export_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == COMPOSER_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(&rendered, &outputs->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s",
                          detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

/* The records of the stages as one JSON array, the text of the --record file. */
static char *record_text(const ComposeOutcome *outcome) {
    size_t size = 16;
    char *text = malloc(size);
    if (!text) {
        return NULL;
    }
    size_t used = (size_t)snprintf(text, size, "[");
    bool first = true;
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        if (!outcome->records[stage]) {
            continue;
        }
        char *json = fill_record_to_json(outcome->records[stage]);
        if (!json) {
            free(text);
            return NULL;
        }
        const char *name = compose_stage_name((ComposeStage)stage);
        size_t need = used + strlen(json) + strlen(name) + 64;
        if (need > size) {
            size = need * 2;
            char *grown = realloc(text, size);
            if (!grown) {
                free(json);
                free(text);
                return NULL;
            }
            text = grown;
        }
        used += (size_t)snprintf(text + used, size - used, "%s{\"stage\":\"%s\",\"record\":%s}", first ? "\n" : ",\n", name, json);
        first = false;
        free(json);
    }
    snprintf(text + used, size - used, "\n]\n");
    return text;
}

ComposerStatus api_record_outputs(const PiecePlan *plan, const ComposeOutcome *outcome, const ApiOutputs *outputs,
                                  cJSON *document, ComposerResponse *out) {
    if (outputs->record && outcome) {
        char *text = record_text(outcome);
        if (!text) {
            return api_fail_alloc(&out->error);
        }
        bool added = api_add(document, "record", cJSON_CreateString(text));
        free(text);
        if (!added) {
            return api_fail_alloc(&out->error);
        }
    }
    if (outputs->explain && outcome) {
        Explanation explanation;
        if (explain_build(plan, outcome, &explanation) != EXPLAIN_OK) {
            return api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "/explain", "cannot explain the run");
        }
        char *text = outputs->explain == 2 ? explain_to_json(&explanation) : explain_to_text(&explanation);
        explain_destroy(&explanation);
        if (!text) {
            return api_fail_alloc(&out->error);
        }
        bool added = api_add(document, "explanation", cJSON_CreateString(text));
        free(text);
        if (!added) {
            return api_fail_alloc(&out->error);
        }
    }
    return COMPOSER_OK;
}

/* --- the result of a compose ----------------------------------------------------------------------- */

static const char *letter_of(const ApiSection *sections, size_t count, NodeId node) {
    for (size_t i = 0; i < count; i++) {
        if (sections[i].node == node) {
            return sections[i].letter;
        }
    }
    return "?";
}

static void form_label(int form, char *out, size_t size) {
    snprintf(out, size, "%s", form >= 0 ? form_hear_form_name(form) : "none");
}

/* What was heard in a section, in words: "heard as period" or "heard as sentence, not period". */
static void heard_text(const ComposeHeard *heard, char *out, size_t size) {
    char requested[96];
    char found[96];
    form_label(heard->requested_form, requested, sizeof(requested));
    if (!heard->in_scope) {
        snprintf(out, size, "kept");
    } else if (heard->heard_result != FORM_HEAR_OK) {
        snprintf(out, size, "not heard (%s)", form_hear_result_name(heard->heard_result));
    } else if (heard->heard.count == 0 || heard->heard_form < 0) {
        snprintf(out, size, "no theme heard");
    } else {
        form_label(heard->heard_form, found, sizeof(found));
        if (heard->heard_as_requested) {
            snprintf(out, size, "heard as %s", found);
        } else {
            snprintf(out, size, "heard as %s, not %s", found, requested);
        }
    }
}

cJSON *api_heard_json(const PiecePlan *plan, const ComposeOutcome *outcome) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    count = count > API_MAX_SECTIONS ? API_MAX_SECTIONS : count;
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        char text[256];
        heard_text(heard, text, sizeof(text));
        cJSON *item = cJSON_CreateObject();
        cJSON *cadences = cJSON_CreateArray();
        bool ok = item && cadences && cJSON_AddStringToObject(item, "letter", letter_of(sections, count, heard->section));
        if (ok && heard->in_scope) {
            ok = cJSON_AddStringToObject(item, "heard", text) != NULL;
        } else if (ok) {
            ok = cJSON_AddNullToObject(item, "heard") && cJSON_AddFalseToObject(item, "in_scope");
        }
        ok = ok && cJSON_AddBoolToObject(item, "heard_as_requested", heard->heard_as_requested);
        for (size_t c = 0; ok && heard->heard_result == FORM_HEAR_OK && c < heard->heard.cadence_count; c++) {
            ok = cJSON_AddItemToArray(cadences, cJSON_CreateString(cadence_type_name((CadenceType)heard->heard.cadences[c].type)));
        }
        ok = ok && api_add(item, "cadences", cadences);
        if (ok) {
            cadences = NULL;
        }
        cJSON_Delete(cadences);
        if (!ok || !cJSON_AddItemToArray(array, item)) {
            cJSON_Delete(item);
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static const char *given_lane_name(const GivenReport *run) {
    switch (run->lane) {
    case PLAN_LANE_HARMONY:
        return "progression";
    case PLAN_LANE_BASS:
        return "bass line";
    case PLAN_LANE_MELODY:
        return "melody";
    default:
        return "inner voice";
    }
}

/* The bar of its section a run starts in, from 1. */
static size_t given_bar(const PiecePlan *plan, const GivenReport *run) {
    Rational offset;
    PlanSpan section = plan_node_span(plan, run->owner);
    if (rational_sub(run->span.start, section.start, &offset) != RATIONAL_OK) {
        return 1;
    }
    return api_bars_in(plan, offset) + 1;
}

static cJSON *given_json(const PiecePlan *plan, const ApiSection *sections, size_t section_count, const ApiGivenRuns *given) {
    cJSON *object = cJSON_CreateObject();
    cJSON *runs = cJSON_CreateArray();
    bool ok = object && runs && cJSON_AddBoolToObject(object, "unchanged", given->unchanged);
    for (size_t i = 0; ok && i < given->count && i < API_MAX_GIVEN_RUNS; i++) {
        const GivenReport *run = &given->runs[i];
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddStringToObject(item, "kind", given_lane_name(run)) &&
             cJSON_AddStringToObject(item, "section", letter_of(sections, section_count, run->owner)) &&
             cJSON_AddNumberToObject(item, "bar", (double)given_bar(plan, run)) &&
             cJSON_AddNumberToObject(item, run->lane == PLAN_LANE_HARMONY ? "chords" : "bars",
                                     (double)(run->lane == PLAN_LANE_HARMONY ? run->spans : api_bars_in(plan, run->span.length))) &&
             cJSON_AddBoolToObject(item, "unchanged", run->unchanged) && cJSON_AddItemToArray(runs, item);
        if (!ok) {
            cJSON_Delete(item);
        }
    }
    if (!ok || !api_add(object, "runs", runs)) {
        cJSON_Delete(object);
        if (!object) {
            cJSON_Delete(runs);
        }
        return NULL;
    }
    return object;
}

cJSON *api_compose_document(const PiecePlan *plan, unsigned long seed, bool realized, const ComposeOutcome *outcome,
                            const ApiGivenRuns *given, cJSON *extra) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    count = count > API_MAX_SECTIONS ? API_MAX_SECTIONS : count;
    cJSON *document = cJSON_CreateObject();
    bool ok = document && cJSON_AddNumberToObject(document, "seed", (double)seed) &&
              cJSON_AddBoolToObject(document, "realized", realized);
    for (cJSON *member = extra ? extra->child : NULL; ok && member;) {
        cJSON *next = member->next;
        cJSON *detached = cJSON_DetachItemViaPointer(extra, member);
        ok = api_add(document, detached->string, detached);
        member = next;
    }
    cJSON_Delete(extra);
    ok = ok && api_add(document, "sections", api_heard_json(plan, outcome));
    if (ok && outcome->analyzer_check_count > 0) {
        cJSON *analyzer = cJSON_CreateObject();
        ok = analyzer && cJSON_AddNumberToObject(analyzer, "planned", (double)outcome->analyzer_planned) &&
             cJSON_AddNumberToObject(analyzer, "agreed", (double)outcome->analyzer_agreed) && api_add(document, "analyzer", analyzer);
        if (!ok) {
            cJSON_Delete(analyzer);
        }
    }
    if (ok && outcome->style_measured) {
        ok = api_add(document, "style_distance", api_style_distance_json(&outcome->style_distance));
    }
    cJSON *returns = ok ? cJSON_CreateObject() : NULL;
    cJSON *pairs = returns ? cJSON_CreateArray() : NULL;
    ok = ok && pairs && cJSON_AddNumberToObject(returns, "planned", (double)outcome->returns_planned) &&
         cJSON_AddNumberToObject(returns, "heard", (double)outcome->returns_heard);
    for (size_t i = 0; ok && i < outcome->returns.count; i++) {
        const ReturnHeardPair *pair = &outcome->returns.pairs[i];
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddStringToObject(item, "later", letter_of(sections, count, pair->later)) &&
             cJSON_AddStringToObject(item, "earlier", letter_of(sections, count, pair->earlier)) &&
             cJSON_AddNumberToObject(item, "similarity", floor(pair->similarity * 10000.0 + 0.5) / 10000.0) &&
             cJSON_AddItemToArray(pairs, item);
        if (!ok) {
            cJSON_Delete(item);
        }
    }
    if (ok) {
        ok = api_add(returns, "pairs", pairs);
        pairs = NULL;
        ok = ok && api_add(document, "returns", returns);
        returns = NULL;
    }
    cJSON_Delete(pairs);
    cJSON_Delete(returns);
    if (ok && given->count > 0) {
        ok = api_add(document, "given", given_json(plan, sections, count, given));
    }
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* --- a plan that is stuck -------------------------------------------------------------------------- */

#define MAX_OPEN_SHOWN 24

static const char *const NODE_KINDS[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};
static const char *const LANES[] = {"meter", "key", "harmony", "bass", "melody", "inner voice"};

/* Why the stage left a target open: the last step the record has for it. */
static void stuck_reason(const ComposeOutcome *outcome, FillTarget target, char *out, size_t size) {
    snprintf(out, size, "not reached");
    const FillRecord *record = outcome->records[outcome->stuck_stage];
    for (size_t s = 0; record && s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (!fill_target_equals(step->target, target)) {
            continue;
        }
        if (step->outcome == FILL_STEP_NO_OPERATOR) {
            snprintf(out, size, "no operator applies");
        } else if (step->outcome == FILL_STEP_ALL_REJECTED) {
            const char *why = "";
            for (size_t c = 0; c < step->considered_count; c++) {
                why = step->considered[c].reason[0] ? step->considered[c].reason : why;
            }
            snprintf(out, size, "every candidate was rejected%s%s", why[0] ? ": " : "", why);
        }
    }
}

static bool add_open(cJSON *list, const char *what, const char *reason) {
    cJSON *item = cJSON_CreateObject();
    if (!item || !cJSON_AddStringToObject(item, "what", what) || !cJSON_AddStringToObject(item, "reason", reason) ||
        !cJSON_AddItemToArray(list, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

ComposerStatus api_compose_stuck(const PiecePlan *plan, const ComposeOutcome *outcome, unsigned long first,
                                 unsigned long last, ComposerResponse *out) {
    cJSON *document = cJSON_CreateObject();
    cJSON *stuck = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    bool ok = document && stuck && list && cJSON_AddNumberToObject(stuck, "first", (double)first) &&
              cJSON_AddNumberToObject(stuck, "last", (double)last) &&
              cJSON_AddStringToObject(stuck, "stage", compose_stage_name(outcome->stuck_stage)) &&
              cJSON_AddBoolToObject(stuck, "key_rejected", outcome->key_rejected);
    size_t shown = 0;
    size_t open = 0;
    for (NodeId id = 1; ok && id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED) {
            continue;
        }
        open++;
        if (shown < MAX_OPEN_SHOWN) {
            char reason[160];
            char what[96];
            FillTarget target = {FILL_TARGET_NODE, id};
            stuck_reason(outcome, target, reason, sizeof(reason));
            const char *name = plan_node_name(plan, id);
            snprintf(what, sizeof what, "%s %s", NODE_KINDS[plan_node_kind(plan, id)], name ? name : "?");
            ok = add_open(list, what, reason);
            shown++;
        }
    }
    for (LaneSpanId id = 1; ok && id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id) || plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED) {
            continue;
        }
        open++;
        if (shown < MAX_OPEN_SHOWN) {
            char reason[160];
            char what[96];
            FillTarget target = {FILL_TARGET_LANE_SPAN, id};
            stuck_reason(outcome, target, reason, sizeof(reason));
            const char *owner = plan_node_name(plan, plan_lane_span_owner(plan, id));
            snprintf(what, sizeof what, "%s lane of %s", LANES[plan_lane_span_lane(plan, id)], owner ? owner : "?");
            ok = add_open(list, what, reason);
            shown++;
        }
    }
    ok = ok && api_add(stuck, "open", list);
    list = NULL;
    ok = ok && cJSON_AddNumberToObject(stuck, "more", (double)(open > shown ? open - shown : 0)) && api_add(document, "stuck", stuck);
    if (ok) {
        stuck = NULL;
    }
    cJSON_Delete(list);
    cJSON_Delete(stuck);
    if (!ok) {
        cJSON_Delete(document);
        return api_fail_alloc(&out->error);
    }
    ComposerError error;
    api_fail(&error, COMPOSER_ERR_UNSATISFIABLE, "", "the plan could not be completed from seeds %lu to %lu: stuck at the %s stage%s",
             first, last, compose_stage_name(outcome->stuck_stage),
             outcome->key_rejected ? " (the keys or cadences heard did not fit the plan)" : "");
    ComposerStatus status = api_respond_json(out, document);
    out->error = error;
    return status == COMPOSER_OK ? COMPOSER_ERR_UNSATISFIABLE : status;
}

static void append_number(char *out, size_t size, unsigned long value, bool first) {
    size_t used = strlen(out);
    snprintf(out + used, size - used, first ? "%lu" : ", %lu", value);
}

/* The sizes the form has, for the message about one it has not: the bars its sizes come in, then their counts. */
void api_form_sizes_text(const FormSpec *form, char *out, size_t size) {
    unsigned long bars[FORM_SPEC_MAX_SIZES];
    unsigned long counts[FORM_SPEC_MAX_SIZES];
    size_t bars_count = 0;
    size_t count_count = 0;
    out[0] = '\0';
    /* Ascending, each once. */
    for (size_t i = 0; i < form->size_count; i++) {
        unsigned long b = (unsigned long)form->sizes[i].bars;
        unsigned long c = (unsigned long)form->sizes[i].count;
        size_t at = 0;
        while (at < bars_count && bars[at] < b) {
            at++;
        }
        if (at == bars_count || bars[at] != b) {
            memmove(bars + at + 1, bars + at, (bars_count - at) * sizeof(bars[0]));
            bars[at] = b;
            bars_count++;
        }
        at = 0;
        while (c != 0 && at < count_count && counts[at] < c) {
            at++;
        }
        if (c != 0 && (at == count_count || counts[at] != c)) {
            memmove(counts + at + 1, counts + at, (count_count - at) * sizeof(counts[0]));
            counts[at] = c;
            count_count++;
        }
    }
    for (size_t i = 0; i < bars_count; i++) {
        append_number(out, size, bars[i], i == 0);
    }
    size_t used = strlen(out);
    snprintf(out + used, size - used, " bars");
    for (size_t i = 0; i < count_count; i++) {
        used = strlen(out);
        snprintf(out + used, size - used, i ? ", %lu" : " (or count %lu", counts[i]);
    }
    if (count_count > 0) {
        used = strlen(out);
        snprintf(out + used, size - used, ")");
    }
}
