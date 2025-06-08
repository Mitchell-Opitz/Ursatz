#include <ursatz_composer/fill_record.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"

#include <ursatz_composer/composer.h>

#include "fill/fill_internal.h"
#include "plan/plan_json_internal.h"

static const char *const OPERATOR_KINDS[] = {"refine", "grow", "fill"};
static const char *const STATUS_NAMES[] = {"complete", "stuck", "max_iterations", "error"};
static const char *const STEP_OUTCOMES[] = {"applied", "no_operator", "all_rejected"};

void fill_record_destroy(FillRecord *record) {
    if (!record) {
        return;
    }
    for (size_t i = 0; i < record->step_count; i++) {
        free(record->steps[i].considered);
    }
    free(record->steps);
    generation_record_destroy(record->generation);
    free(record);
}

FillResult fill_record_begin(FillRecord **out, unsigned long seed) {
    FillRecord *record = calloc(1, sizeof(FillRecord));
    if (!record) {
        return FILL_ERR_ALLOC;
    }
    char text[64];
    snprintf(text, sizeof(text), "composer-fill-%lu", seed);
    EntityID *id = NULL;
    ProvenanceReference *provenance = NULL;
    if (entity_id_create(text, &id) != ENTITY_ID_OK || provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK ||
        generation_record_create(id, seed, composer_version(), NULL, NULL, provenance, &record->generation) !=
            GENERATION_RECORD_OK) {
        entity_id_destroy(id);
        provenance_reference_destroy(provenance);
        free(record);
        return FILL_ERR_ALLOC;
    }
    record->seed = seed;
    *out = record;
    return FILL_OK;
}

FillResult fill_record_add_step(FillRecord *record, FillTarget target, FillStepOutcome outcome,
                                const FillEntries *entries, const FillOperatorRegistry *registry, long chosen,
                                uint64_t rng_state_before, uint64_t rng_state_after) {
    FillStep *steps = realloc(record->steps, (record->step_count + 1) * sizeof(FillStep));
    if (!steps) {
        return FILL_ERR_ALLOC;
    }
    record->steps = steps;
    FillStep *step = &steps[record->step_count];
    memset(step, 0, sizeof(*step));
    step->considered = entries->count ? calloc(entries->count, sizeof(FillConsidered)) : NULL;
    if (entries->count && !step->considered) {
        return FILL_ERR_ALLOC;
    }
    for (size_t i = 0; i < entries->count; i++) {
        const FillEntry *entry = &entries->items[i];
        const FillOperator *operator_ = fill_registry_at(registry, entry->operator_index);
        FillConsidered *considered = &step->considered[i];
        considered->operator_index = entry->operator_index;
        snprintf(considered->operator_name, FILL_NAME_MAX, "%s", operator_->generator.name);
        snprintf(considered->operator_version, FILL_NAME_MAX, "%s", operator_->generator.version);
        considered->kind = operator_->kind;
        snprintf(considered->label, FILL_LABEL_MAX, "%s", entry->candidate.label);
        considered->score_numerator = entry->check.score_numerator;
        considered->score_denominator = entry->check.score_denominator;
        considered->styled = entry->check.style_denominator != 0;
        considered->style_numerator = considered->styled ? entry->check.style_numerator : 0;
        considered->style_denominator = considered->styled ? entry->check.style_denominator : 1;
        considered->rejected = entry->check.hard_violation;
        snprintf(considered->reason, FILL_REASON_MAX, "%s", entry->check.reason);
    }
    step->target = target;
    step->outcome = outcome;
    step->considered_count = entries->count;
    step->chosen = chosen;
    step->rng_state_before = rng_state_before;
    step->rng_state_after = rng_state_after;
    record->step_count++;
    return FILL_OK;
}

/* ------------------------------------------------------------------- JSON */

static cJSON *decimal_string(unsigned long long value) {
    char text[32];
    snprintf(text, sizeof(text), "%llu", value);
    return cJSON_CreateString(text);
}

static cJSON *rational_json(unsigned long numerator, unsigned long denominator) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "num", json_number((long)numerator)) &&
                    json_add(object, "den", json_number((long)denominator)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *considered_json(const FillConsidered *considered) {
    cJSON *object = cJSON_CreateObject();
    cJSON *operator_ = cJSON_CreateObject();
    bool ok = object && operator_ && json_add(operator_, "name", cJSON_CreateString(considered->operator_name)) &&
              json_add(operator_, "version", cJSON_CreateString(considered->operator_version)) &&
              json_add(operator_, "kind", cJSON_CreateString(OPERATOR_KINDS[considered->kind]));
    if (ok) {
        ok = json_add(object, "operator", operator_);
        operator_ = NULL;
    }
    ok = ok && json_add(object, "label", cJSON_CreateString(considered->label)) &&
         json_add(object, "score", rational_json(considered->score_numerator, considered->score_denominator)) &&
         (!considered->styled ||
          json_add(object, "style", rational_json(considered->style_numerator, considered->style_denominator))) &&
         json_add(object, "rejected", cJSON_CreateBool(considered->rejected)) &&
         json_add(object, "reason", cJSON_CreateString(considered->reason));
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(operator_);
        return NULL;
    }
    return object;
}

static cJSON *step_json(const FillStep *step) {
    cJSON *object = cJSON_CreateObject();
    cJSON *target = cJSON_CreateObject();
    cJSON *considered = cJSON_CreateArray();
    bool ok = object && target && considered &&
              json_add(target, "kind", cJSON_CreateString(step->target.kind == FILL_TARGET_NODE ? "node" : "lane_span")) &&
              json_add(target, "id", json_number((long)step->target.id));
    if (ok) {
        ok = json_add(object, "target", target);
        target = NULL;
    }
    ok = ok && json_add(object, "outcome", cJSON_CreateString(STEP_OUTCOMES[step->outcome])) &&
         json_add(object, "rng_state_before", decimal_string(step->rng_state_before)) &&
         json_add(object, "rng_state_after", decimal_string(step->rng_state_after));
    for (size_t i = 0; ok && i < step->considered_count; i++) {
        ok = json_append(considered, considered_json(&step->considered[i]));
    }
    if (ok) {
        ok = json_add(object, "considered", considered);
        considered = NULL;
    }
    ok = ok && json_add(object, "chosen", step->chosen >= 0 ? json_number(step->chosen) : cJSON_CreateNull());
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(target);
        cJSON_Delete(considered);
        return NULL;
    }
    return object;
}

char *fill_record_to_json(const FillRecord *record) {
    if (!record) {
        return NULL;
    }
    cJSON *document = cJSON_CreateObject();
    cJSON *steps = cJSON_CreateArray();
    bool ok = document && steps;
    ok = ok && json_add(document, "schema_version", json_number(1)) &&
         json_add(document, "record_id", cJSON_CreateString(entity_id_value(generation_record_id(record->generation)))) &&
         json_add(document, "random_seed", decimal_string(generation_record_random_seed(record->generation))) &&
         json_add(document, "generator_version", cJSON_CreateString(generation_record_generator_version(record->generation))) &&
         json_add(document, "framework_version", cJSON_CreateNull()) &&
         json_add(document, "status", cJSON_CreateString(STATUS_NAMES[record->status])) &&
         json_add(document, "iterations", json_number((long)record->iterations));
    for (size_t i = 0; ok && i < record->step_count; i++) {
        ok = json_append(steps, step_json(&record->steps[i]));
    }
    if (ok) {
        ok = json_add(document, "steps", steps);
        steps = NULL;
    }
    char *printed = ok ? cJSON_Print(document) : NULL;
    cJSON_Delete(document);
    cJSON_Delete(steps);
    char *text = printed ? json_indent_with_spaces(printed) : NULL;
    cJSON_free(printed);
    return text;
}
