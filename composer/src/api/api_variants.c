#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/variant_registry.h>

#include "api_internal.h"
#include "api_vocabulary.h"
#include "util/request_parse.h"

/* Variants and recognize (docs/api.md): the requests of the `variants` and `recognize` commands, and their documents. */

#define VARIANTS_COUNT_MAX API_VARIANTS_COUNT_MAX
#define VARIANTS_COUNT_DEFAULT 6
#define VARIANTS_DRAWS_PER_COUNT API_VARIANTS_DRAWS_PER_COUNT
#define SEED_MAX API_VARIANTS_SEED_MAX

ComposerStatus composer_api_variant_operations(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    out->json = variant_registry_to_json();
    return out->json ? COMPOSER_OK : api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

/* --- the chain ------------------------------------------------------------ */

static bool whole_number(const cJSON *item, long *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble) {
        return false;
    }
    *out = (long)item->valuedouble;
    return true;
}

/* "n:m" with both whole numbers. */
static bool parse_ratio(const char *text, long *numerator, long *denominator) {
    char *end = NULL;
    *numerator = strtol(text, &end, 10);
    if (end == text || *end != ':') {
        return false;
    }
    const char *rest = end + 1;
    *denominator = strtol(rest, &end, 10);
    return end != rest && *end == '\0';
}

/* Sets one parameter of a step from its JSON value. */
static ComposerStatus set_param(VariantStep *step, size_t number, const char *base, const cJSON *item,
                                ComposerError *error) {
    char path[sizeof error->path];
    snprintf(path, sizeof path, "%s/params/%s", base, item->string);
    const VariantOperation *operation = variant_registry_find(step->op);
    int index = variant_operation_param_index(operation, item->string);
    VariantParam described;
    if (index < 0 || !variant_operation_param(operation, (size_t)index, &described)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "step %zu: %s has no parameter '%s'", number, step->op,
                        item->string);
    }
    const VariantParam *param = &described;
    long value = 0;
    long den = 0;
    bool ok = false;
    switch (param->kind) {
    case VARIANT_PARAM_BOOL:
        ok = cJSON_IsBool(item);
        value = cJSON_IsTrue(item) ? 1 : 0;
        break;
    case VARIANT_PARAM_INT:
    case VARIANT_PARAM_PITCH:
        ok = whole_number(item, &value);
        break;
    case VARIANT_PARAM_RATIO:
        ok = cJSON_IsString(item) && parse_ratio(item->valuestring, &value, &den);
        break;
    case VARIANT_PARAM_POSITION:
        if (cJSON_IsString(item)) {
            ok = true;
            if (strcmp(item->valuestring, "leading") == 0) {
                value = 1;
            } else if (strcmp(item->valuestring, "trailing") == 0) {
                value = 2;
            } else if (strcmp(item->valuestring, "middle") == 0) {
                value = 3;
            } else {
                ok = false;
            }
        }
        break;
    }
    if (!ok) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "step %zu: '%s' of %s must be a %s", number, param->name,
                        step->op,
                        param->kind == VARIANT_PARAM_BOOL       ? "true or false"
                        : param->kind == VARIANT_PARAM_RATIO    ? "ratio text like \"2:1\""
                        : param->kind == VARIANT_PARAM_POSITION ? "position: \"leading\", \"trailing\" or \"middle\""
                                                                : "whole number");
    }
    if (!variant_step_set(step, param->name, value, den)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "step %zu: '%s' of %s is out of range (%ld to %ld)", number,
                        param->name, step->op, param->min, param->max);
    }
    return COMPOSER_OK;
}

/* Reads {"steps": [{"op", "params"}]} into *chain. */
static ComposerStatus read_chain(const cJSON *root, VariantChain *chain, ComposerError *error) {
    memset(chain, 0, sizeof *chain);
    const cJSON *steps = cJSON_GetObjectItemCaseSensitive(root, "steps");
    if (!cJSON_IsArray(steps)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/chain", "expected {\"steps\": [...]}");
    }
    if (cJSON_GetArraySize(steps) < 1 || cJSON_GetArraySize(steps) > VARIANT_CHAIN_MAX) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/chain/steps", "a chain has 1 to %d steps", VARIANT_CHAIN_MAX);
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, steps) {
        size_t number = chain->step_count + 1;
        char base[96];
        snprintf(base, sizeof base, "/chain/steps/%zu", chain->step_count);
        const cJSON *op = cJSON_GetObjectItemCaseSensitive(item, "op");
        const cJSON *params = cJSON_GetObjectItemCaseSensitive(item, "params");
        VariantStep *step = &chain->steps[chain->step_count];
        if (!cJSON_IsString(op)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base, "step %zu needs an \"op\" name", number);
        }
        if (!variant_step_init(step, op->valuestring)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base,
                            "step %zu: unknown operation '%s'; composer_api_variant_operations lists them", number,
                            op->valuestring);
        }
        if (params && !cJSON_IsObject(params)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base, "step %zu: \"params\" must be an object", number);
        }
        const cJSON *param = NULL;
        cJSON_ArrayForEach(param, params) {
            ComposerStatus status = set_param(step, number, base, param, error);
            if (status != COMPOSER_OK) {
                return status;
            }
        }
        chain->step_count++;
    }
    return COMPOSER_OK;
}

/* --- the motives ---------------------------------------------------------- */

static bool object_or_fail(const cJSON *item, const char *path, ComposerError *error) {
    if (!cJSON_IsObject(item)) {
        api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be a motive request with an input", path + 1);
        return false;
    }
    return true;
}

/* Replaces the member of object with a copy of the one of from (when from has it). */
static bool inherit_member(cJSON *object, const cJSON *from, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(from, name);
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    cJSON *copy = cJSON_Duplicate(item, true);
    if (!copy) {
        return false;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddItemToObject(object, name, copy);
}

/*
 * A motive request that is read as the source is: a copy of part with the source's meter, key and role, and the form of the
 * source's input. NULL when out of memory.
 */
static cJSON *read_as(const cJSON *part, const cJSON *meter_key_role_from, const cJSON *form_from) {
    cJSON *copy = cJSON_Duplicate(part, true);
    if (!copy) {
        return NULL;
    }
    bool ok = inherit_member(copy, meter_key_role_from, "meter") && inherit_member(copy, meter_key_role_from, "key") &&
              inherit_member(copy, meter_key_role_from, "role");
    const cJSON *source_input = cJSON_GetObjectItemCaseSensitive(form_from, "input");
    cJSON *input = cJSON_GetObjectItemCaseSensitive(copy, "input");
    if (ok && cJSON_IsObject(source_input) && cJSON_IsObject(input)) {
        ok = inherit_member(input, source_input, "form");
    }
    if (!ok) {
        cJSON_Delete(copy);
        return NULL;
    }
    return copy;
}

/*
 * The basic motive of "basic": read in the meter, key and role of the request and the form of form_from's input. Not an
 * error when absent.
 */
static ComposerStatus read_basic(const cJSON *request, const cJSON *form_from, Motive *basic, bool *given,
                                 ComposerError *error) {
    const cJSON *part = cJSON_GetObjectItemCaseSensitive(request, "basic");
    *given = part && !cJSON_IsNull(part);
    if (!*given) {
        return COMPOSER_OK;
    }
    if (!object_or_fail(part, "/basic", error)) {
        return COMPOSER_ERR_REQUEST;
    }
    cJSON *copy = read_as(part, request, form_from);
    if (!copy) {
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    ComposerStatus status = api_read_motive(copy, "/basic", basic, NULL, error);
    cJSON_Delete(copy);
    return status;
}

/* --- the variants --------------------------------------------------------- */

typedef struct {
    VariantChain chain;
    Motive motive;
    VariantRecord record;
} Variant;

/* An operation "each_op" could not apply to the motive, and why. */
typedef struct {
    const char *op;
    char reason[VARIANT_ERROR_MAX];
} Skipped;

typedef struct {
    size_t min_steps;
    size_t max_steps;
    /* The remoteness band as fractions: over low_num/low_den (0 for none) and at most high_num/high_den. */
    unsigned long long low_num, low_den, high_num, high_den;
} Distance;

static const Distance DISTANCE_CLOSE = {1, 1, 0, 1, 1, 3};
static const Distance DISTANCE_MEDIUM = {2, 2, 1, 3, 2, 3};
static const Distance DISTANCE_FAR = {3, VARIANT_CHAIN_MAX, 2, 3, 1, 1};

static bool in_distance(const Distance *distance, const VariantRecord *record) {
    unsigned long long num = record->remoteness_num;
    unsigned long long den = record->remoteness_den;
    return num * distance->low_den > distance->low_num * den && num * distance->high_den <= distance->high_num * den;
}

typedef struct {
    const char *functions[VARIANT_FUNCTIONS];
    size_t function_count;
    bool basic_given;
} Filter;

static bool suits_a_function(const Filter *filter, const VariantRecord *record) {
    for (size_t f = 0; f < record->function_count; f++) {
        for (size_t w = 0; w < filter->function_count; w++) {
            if (strcmp(record->functions[f], filter->functions[w]) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* Sets the variant's remoteness from the basic motive and the functions it suits. */
static void measure_variant(const Filter *filter, const Motive *basic, Variant *variant) {
    /* From the basic motive, searched as deep as the chain that made the variant unless the basic is another motive. */
    size_t depth = filter->basic_given ? VARIANT_CHAIN_MAX : variant->record.step_count;
    if (variant_set_remoteness(basic, &variant->motive, depth, &variant->record) != VARIANT_OK) {
        variant->record.remoteness_num = 1;
        variant->record.remoteness_den = 1;
    }
    variant_functions(basic, &variant->motive, &variant->record, variant->record.functions,
                      &variant->record.function_count);
}

/* The motive and its analysis as the motive entry's document, parsed. NULL on failure. */
static cJSON *motive_document(const Motive *motive) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis)) {
        return NULL;
    }
    char *text = motive_analysis_to_json(motive, &analysis);
    cJSON *document = text ? cJSON_Parse(text) : NULL;
    free(text);
    return document;
}

/* A variant: its record as variant_record_to_json writes it, then "motive". NULL on failure. */
static cJSON *variant_object(const Variant *variant, bool detail) {
    char *text = variant_record_to_json(&variant->record);
    cJSON *object = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *motive = object ? motive_document(&variant->motive) : NULL;
    if (!motive) {
        cJSON_Delete(object);
        return NULL;
    }
    /* A variant that leaves its harmony is kept and marked: the warning joins the analysis's own. */
    cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motive, "analysis");
    cJSON *warnings = cJSON_GetObjectItemCaseSensitive(analysis, "warnings");
    if (variant->record.leaves_harmony &&
        (!cJSON_IsArray(warnings) ||
         !cJSON_AddItemToArray(warnings, cJSON_CreateString(VARIANT_WARNING_LEAVES_HARMONY)))) {
        cJSON_Delete(motive);
        cJSON_Delete(object);
        return NULL;
    }
    cJSON_AddItemToObject(object, "motive", motive);
    /* The exact figures the decimals of the record come from, for a client that prints them (the command line). */
    if (detail) {
        cJSON *fraction = cJSON_AddArrayToObject(object, "remoteness_fraction");
        if (!fraction || !cJSON_AddItemToArray(fraction, cJSON_CreateNumber((double)variant->record.remoteness_num)) ||
            !cJSON_AddItemToArray(fraction, cJSON_CreateNumber((double)variant->record.remoteness_den)) ||
            !cJSON_AddBoolToObject(object, "leaves_harmony", variant->record.leaves_harmony)) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}

/* {"seed", "source", "variants"}, "skipped" for each_op and "notices" when there are any. NULL on failure. */
static cJSON *variants_document(unsigned long seed, const Motive *source, const Variant *variants, size_t count,
                                const Skipped *skipped, size_t skipped_count, bool each_op, bool detail, cJSON *notices) {
    cJSON *document = cJSON_CreateObject();
    cJSON *source_document = document ? motive_document(source) : NULL;
    cJSON *array = source_document ? cJSON_CreateArray() : NULL;
    if (!array) {
        cJSON_Delete(source_document);
        cJSON_Delete(document);
        return NULL;
    }
    cJSON_AddNumberToObject(document, "seed", (double)seed);
    cJSON_AddItemToObject(document, "source", source_document);
    cJSON_AddItemToObject(document, "variants", array);
    for (size_t i = 0; i < count; i++) {
        cJSON *object = variant_object(&variants[i], detail);
        if (!object) {
            cJSON_Delete(document);
            return NULL;
        }
        cJSON_AddItemToArray(array, object);
    }
    cJSON *skipped_array = each_op ? cJSON_AddArrayToObject(document, "skipped") : NULL;
    for (size_t i = 0; each_op && i < skipped_count; i++) {
        cJSON *entry = skipped_array ? cJSON_CreateObject() : NULL;
        if (!entry || !cJSON_AddStringToObject(entry, "op", skipped[i].op) ||
            !cJSON_AddStringToObject(entry, "reason", skipped[i].reason)) {
            cJSON_Delete(entry);
            cJSON_Delete(document);
            return NULL;
        }
        cJSON_AddItemToArray(skipped_array, entry);
    }
    if (each_op && !skipped_array) {
        cJSON_Delete(document);
        return NULL;
    }
    if (notices && cJSON_GetArraySize(notices) > 0) {
        if (!cJSON_AddItemToObject(document, "notices", notices)) {
            cJSON_Delete(document);
            return NULL;
        }
    } else {
        cJSON_Delete(notices);
    }
    return document;
}

static ComposerStatus respond_variants(ComposerResponse *out, unsigned long seed, const Motive *source,
                                       const Variant *variants, size_t count, const Skipped *skipped,
                                       size_t skipped_count, bool each_op, bool detail, cJSON *notices) {
    cJSON *document = variants_document(seed, source, variants, count, skipped, skipped_count, each_op, detail, notices);
    if (!document) {
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, document);
}

static ComposerStatus cancelled(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
}

/* each_op: one one-step variant for each operation that takes the motive's form. */
static ComposerStatus make_each_op(const Filter *filter, bool detail, unsigned long seed, const Motive *source, const Motive *basic,
                                   const ComposerJob *job, ComposerResponse *out) {
    size_t rows = variant_registry_count();
    Variant *variants = calloc(rows, sizeof(*variants));
    Skipped *skipped = calloc(rows, sizeof(*skipped));
    if (!variants || !skipped) {
        free(variants);
        free(skipped);
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    size_t made = 0;
    size_t skipped_count = 0;
    ComposerStatus status = COMPOSER_OK;
    for (size_t i = 0; i < rows; i++) {
        if (job_step(job, "variants", (unsigned)i, (unsigned)rows)) {
            status = cancelled(&out->error);
            break;
        }
        const VariantOperation *operation = variant_registry_at(i);
        if ((operation->forms & VARIANT_FORM_BIT(source->form)) == 0) {
            continue;
        }
        Variant *variant = &variants[made];
        variant_step_init(&variant->chain.steps[0], operation->name);
        variant->chain.step_count = 1;
        VariantResult result =
            variant_apply_chain(source, &variant->chain, seed + (unsigned long)i, &variant->motive, &variant->record);
        if (result == VARIANT_OK) {
            measure_variant(filter, basic, variant);
            made++;
        } else {
            skipped[skipped_count].op = operation->name;
            /* The error often starts with the operation's name; the entry has that already. */
            const char *reason = variant->record.error[0] ? variant->record.error : variant_result_name(result);
            size_t name_length = strlen(operation->name);
            if (strncmp(reason, operation->name, name_length) == 0 && reason[name_length] == ':' &&
                reason[name_length + 1] == ' ') {
                reason += name_length + 2;
            }
            snprintf(skipped[skipped_count].reason, sizeof(skipped[skipped_count].reason), "%s", reason);
            skipped_count++;
        }
    }
    if (status == COMPOSER_OK && made == 0) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "no operation could be applied to this motive");
    } else if (status == COMPOSER_OK) {
        status = respond_variants(out, seed, source, variants, made, skipped, skipped_count, true, detail, NULL);
    }
    free(variants);
    free(skipped);
    return status;
}

static ComposerStatus add_notice(cJSON *notices, ComposerError *error, const char *format, ...) __attribute__((format(printf, 3, 4)));

static ComposerStatus add_notice(cJSON *notices, ComposerError *error, const char *format, ...) {
    char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    cJSON *item = cJSON_CreateString(text);
    if (!item || !cJSON_AddItemToArray(notices, item)) {
        cJSON_Delete(item);
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return COMPOSER_OK;
}

typedef struct {
    unsigned long count;
    unsigned long seed;
    bool each_op;
    bool detail;
    const Distance *distance;
    bool has_chain;
    VariantChain chain;
    Filter filter;
} VariantsRequest;

static ComposerStatus read_variants_request(const cJSON *request, VariantsRequest *read, ComposerError *error) {
    memset(read, 0, sizeof *read);
    read->count = VARIANTS_COUNT_DEFAULT;
    read->seed = api_default_seed() % (SEED_MAX + 1);
    bool has_count = false;
    bool has_distance = false;
    const char *distance_text = NULL;
    unsigned long seed = 0;
    bool has_seed = false;
    if (!api_member_whole(request, "count", 1, VARIANTS_COUNT_MAX, &read->count, &has_count, error) ||
        !api_member_whole(request, "seed", 0, SEED_MAX, &seed, &has_seed, error) ||
        !api_member_bool(request, "each_op", &read->each_op, NULL, error) ||
        !api_member_bool(request, "detail", &read->detail, NULL, error) ||
        !api_member_string(request, "distance", &distance_text, &has_distance, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (has_seed) {
        read->seed = seed;
    }
    const cJSON *functions = cJSON_GetObjectItemCaseSensitive(request, "functions");
    if (functions && !cJSON_IsNull(functions)) {
        if (!cJSON_IsArray(functions)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/functions", "functions must be a list of function names");
        }
        if (cJSON_GetArraySize(functions) > VARIANT_FUNCTIONS) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/functions", "functions has at most %d names", VARIANT_FUNCTIONS);
        }
        for (int i = 0; i < cJSON_GetArraySize(functions); i++) {
            const cJSON *item = cJSON_GetArrayItem(functions, i);
            char path[48];
            snprintf(path, sizeof path, "/functions/%d", i);
            const char *name = cJSON_IsString(item) ? item->valuestring : "";
            if (api_choice_find(API_FUNCTIONS, name) < 0) {
                char functions[96];
                api_choice_list(API_FUNCTIONS, functions, sizeof functions);
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "function must be %s, not '%s'", functions, name);
            }
            read->filter.functions[read->filter.function_count++] = name;
        }
    }
    const cJSON *chain = cJSON_GetObjectItemCaseSensitive(request, "chain");
    read->has_chain = chain && !cJSON_IsNull(chain);
    if (read->each_op && (read->has_chain || has_count || has_distance || read->filter.function_count > 0)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/each_op", "each_op takes no chain, count, distance or functions");
    }
    if (has_distance) {
        if (read->has_chain) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/distance", "a distance takes random chains, not a chain");
        }
        static const Distance *const DISTANCES[] = {&DISTANCE_CLOSE, &DISTANCE_MEDIUM, &DISTANCE_FAR};
        int distance = api_choice_find(API_DISTANCES, distance_text);
        if (distance < 0) {
            char names[64];
            api_choice_list(API_DISTANCES, names, sizeof names);
            return api_fail(error, COMPOSER_ERR_REQUEST, "/distance", "distance must be %s, not '%s'", names, distance_text);
        }
        read->distance = DISTANCES[distance];
    }
    if (read->has_chain) {
        if (!cJSON_IsObject(chain)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/chain", "expected {\"steps\": [...]}");
        }
        return read_chain(chain, &read->chain, error);
    }
    return COMPOSER_OK;
}

/* The variants proper: load the source, make "count" variants, build the document. */
static ComposerStatus make_variants(const cJSON *request, const ComposerJob *job, ComposerResponse *out) {
    ComposerError *error = &out->error;
    VariantsRequest read;
    ComposerStatus status = read_variants_request(request, &read, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    Motive source;
    status = api_read_motive(request, "", &source, NULL, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    Motive basic = source;
    status = read_basic(request, request, &basic, &read.filter.basic_given, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (read.each_op) {
        return make_each_op(&read.filter, read.detail, read.seed, &source, &basic, job, out);
    }

    unsigned long count = read.count;
    Variant *variants = calloc(count, sizeof(*variants));
    cJSON *notices = cJSON_CreateArray();
    if (!variants || !notices) {
        free(variants);
        cJSON_Delete(notices);
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    size_t made = 0;
    bool filtered = read.filter.function_count > 0 || read.distance;
    const char *asked = read.filter.function_count > 0 && read.distance ? "function and distance"
                        : read.distance                                 ? "distance"
                                                                        : "function";
    unsigned long draws = filtered ? count * VARIANTS_DRAWS_PER_COUNT : count;
    status = COMPOSER_OK;
    for (unsigned long i = 0; i < draws && made < count && status == COMPOSER_OK; i++) {
        if (job_step(job, "variants", (unsigned)i, (unsigned)draws)) {
            status = cancelled(error);
            break;
        }
        Variant *variant = &variants[made];
        unsigned long variant_seed = read.seed + i;
        if (read.has_chain) {
            variant->chain = read.chain;
        } else if (!(read.distance ? variant_random_chain_of(&source, variant_seed, read.distance->min_steps,
                                                             read.distance->max_steps, &variant->chain)
                                   : variant_random_chain(&source, variant_seed, &variant->chain))) {
            status = api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot draw a chain for this motive");
            break;
        }
        VariantResult result =
            variant_apply_chain(&source, &variant->chain, variant_seed, &variant->motive, &variant->record);
        if (result == VARIANT_OK) {
            measure_variant(&read.filter, &basic, variant);
            if ((read.filter.function_count > 0 && !suits_a_function(&read.filter, &variant->record)) ||
                (read.distance && !in_distance(read.distance, &variant->record))) {
                continue;
            }
            made++;
        } else if (read.has_chain) {
            status = api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "step %zu of the chain: %s",
                              variant->record.failed_step + 1,
                              variant->record.error[0] ? variant->record.error : variant_result_name(result));
        } else {
            status = add_notice(notices, error, "Skipped seed %lu: %s", variant_seed,
                                variant->record.error[0] ? variant->record.error : variant_result_name(result));
        }
    }
    if (status == COMPOSER_OK && made == 0) {
        status = api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                          filtered ? "no variant that suits the %s was found in %lu draws"
                                   : "no variant could be made of this motive",
                          asked, draws);
    }
    if (status == COMPOSER_OK && filtered && made < count) {
        status = add_notice(notices, error, "Only %zu of %lu variants suit the %s within %lu draws", made, count, asked,
                            draws);
    }
    if (status == COMPOSER_OK) {
        status = respond_variants(out, read.seed, &source, variants, made, NULL, 0, false, read.detail, notices);
    } else {
        cJSON_Delete(notices);
    }
    free(variants);
    return status;
}

/* --- recognize ------------------------------------------------------------ */

/* The chain as the variants JSON writes it: {"steps": [{"op", "params"}]}, from a replay of the chain. NULL on failure. */
static cJSON *chain_object(const Motive *source, const VariantChain *chain) {
    Motive replay;
    VariantRecord record;
    if (variant_apply_chain(source, chain, 0, &replay, &record) != VARIANT_OK) {
        return NULL;
    }
    char *text = variant_record_to_json(&record);
    cJSON *document = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *steps = document ? cJSON_DetachItemFromObjectCaseSensitive(
                                  cJSON_GetObjectItemCaseSensitive(document, "chain"), "steps")
                            : NULL;
    cJSON_Delete(document);
    if (!steps) {
        return NULL;
    }
    cJSON *step = NULL;
    cJSON_ArrayForEach(step, steps) {
        cJSON_DeleteItemFromObjectCaseSensitive(step, "open");
    }
    cJSON *chain_json = cJSON_CreateObject();
    if (!chain_json) {
        cJSON_Delete(steps);
        return NULL;
    }
    cJSON_AddItemToObject(chain_json, "steps", steps);
    return chain_json;
}

static cJSON *names_array(const char *const *names, size_t count) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateString(names[i]));
    }
    return array;
}

/* Four decimals, as the record's recognizability. */
static double four_decimals(unsigned long long numerator, unsigned long long denominator) {
    return (double)(long)(((double)numerator / (double)denominator) * 10000.0 + 0.5) / 10000.0;
}

static cJSON *recognize_document(const Motive *source, const VariantChain *chain, const VariantRecognition *recognition,
                                 const VariantRecord *remoteness, bool detail) {
    cJSON *document = cJSON_CreateObject();
    if (!document) {
        return NULL;
    }
    cJSON *chain_json = recognition->found ? chain_object(source, chain) : NULL;
    if (!chain_json) {
        chain_json = cJSON_CreateObject();
        if (chain_json) {
            cJSON_AddItemToObject(chain_json, "steps", cJSON_CreateArray());
        }
    }
    cJSON_AddBoolToObject(document, "found", recognition->found);
    cJSON_AddItemToObject(document, "chain", chain_json);
    cJSON_AddNumberToObject(document, "distance", four_decimals(recognition->distance_num, recognition->distance_den));
    cJSON_AddNumberToObject(document, "remoteness", four_decimals(remoteness->remoteness_num, remoteness->remoteness_den));
    cJSON_AddItemToObject(document, "kept", names_array(recognition->kept, recognition->kept_count));
    cJSON_AddItemToObject(document, "changed", names_array(recognition->changed, recognition->changed_count));
    cJSON_AddStringToObject(document, "derivation", recognition->derivation);
    if (detail) {
        cJSON *distance = cJSON_AddArrayToObject(document, "distance_fraction");
        cJSON *remote = distance ? cJSON_AddArrayToObject(document, "remoteness_fraction") : NULL;
        if (!remote || !cJSON_AddItemToArray(distance, cJSON_CreateNumber((double)recognition->distance_num)) ||
            !cJSON_AddItemToArray(distance, cJSON_CreateNumber((double)recognition->distance_den)) ||
            !cJSON_AddItemToArray(remote, cJSON_CreateNumber((double)remoteness->remoteness_num)) ||
            !cJSON_AddItemToArray(remote, cJSON_CreateNumber((double)remoteness->remoteness_den))) {
            cJSON_Delete(document);
            return NULL;
        }
    }
    return document;
}

/* The motive request of "source" or "target": its own input, harmony and features, read in the request's meter, key and
   role with the pickup the request gives it. The errors of the members the request gives are put at their own paths. */
static ComposerStatus read_recognize_motive(const cJSON *request, const char *name, const char *pickup_name, Motive *out,
                                            ComposerError *error) {
    char base[32];
    snprintf(base, sizeof base, "/%s", name);
    const cJSON *part = cJSON_GetObjectItemCaseSensitive(request, name);
    if (!object_or_fail(part, base, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    const cJSON *source_part = cJSON_GetObjectItemCaseSensitive(request, "source");
    cJSON *copy = read_as(part, request, source_part);
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(request, pickup_name);
    if (copy && pickup && !cJSON_IsNull(pickup)) {
        cJSON_DeleteItemFromObjectCaseSensitive(copy, "pickup");
        cJSON *pickup_copy = cJSON_Duplicate(pickup, true);
        if (!pickup_copy || !cJSON_AddItemToObject(copy, "pickup", pickup_copy)) {
            cJSON_Delete(pickup_copy);
            cJSON_Delete(copy);
            copy = NULL;
        }
    }
    if (!copy) {
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    ComposerStatus status = api_read_motive(copy, base, out, NULL, error);
    cJSON_Delete(copy);
    if (status == COMPOSER_ERR_REQUEST) {
        /* The members the request gives for both motives are named at the request's own path. */
        static const char *const SHARED[] = {"meter", "key", "role"};
        char own[sizeof error->path];
        for (size_t i = 0; i < sizeof SHARED / sizeof SHARED[0]; i++) {
            snprintf(own, sizeof own, "%s/%s", base, SHARED[i]);
            if (strcmp(error->path, own) == 0) {
                snprintf(error->path, sizeof error->path, "/%s", SHARED[i]);
            }
        }
        snprintf(own, sizeof own, "%s/pickup", base);
        if (strcmp(error->path, own) == 0) {
            snprintf(error->path, sizeof error->path, "/%s", pickup_name);
        }
    }
    return status;
}

static ComposerStatus recognize_request(const cJSON *request, const ComposerJob *job, ComposerResponse *out) {
    ComposerError *error = &out->error;
    if (job_step(job, "recognize", 0, 1)) {
        return cancelled(error);
    }
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(request, "meter");
    if (!meter || cJSON_IsNull(meter)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/meter", "meter is required: both motives are read in it");
    }
    unsigned long depth = VARIANT_CHAIN_MAX;
    bool detail = false;
    if (!api_member_whole(request, "depth", 1, VARIANT_CHAIN_MAX, &depth, NULL, error) ||
        !api_member_bool(request, "detail", &detail, NULL, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    Motive source;
    Motive target;
    ComposerStatus status = read_recognize_motive(request, "source", "pickup", &source, error);
    if (status == COMPOSER_OK) {
        status = read_recognize_motive(request, "target", "target_pickup", &target, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    VariantChain chain;
    VariantRecognition recognition;
    VariantResult result = variant_recognize_depth(&source, &target, depth, &chain, &recognition);
    if (result != VARIANT_OK) {
        return api_fail(error, COMPOSER_ERR_INTERNAL, "", "cannot recognize: %s", variant_result_name(result));
    }
    /* Remoteness is the distance from the basic motive; with no "basic" the source is the basic motive. */
    VariantRecord remoteness;
    memset(&remoteness, 0, sizeof remoteness);
    Motive basic;
    bool basic_given = false;
    status = read_basic(request, cJSON_GetObjectItemCaseSensitive(request, "source"), &basic, &basic_given, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (basic_given) {
        result = variant_set_remoteness(&basic, &target, depth, &remoteness);
        if (result != VARIANT_OK) {
            return api_fail(error, COMPOSER_ERR_INTERNAL, "", "cannot recognize: %s", variant_result_name(result));
        }
    } else {
        remoteness.remoteness_num = recognition.distance_num;
        remoteness.remoteness_den = recognition.distance_den;
    }
    cJSON *document = recognize_document(&source, &chain, &recognition, &remoteness, detail);
    if (!document) {
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    return api_respond_json(out, document);
}

/* --- entries -------------------------------------------------------------- */

typedef ComposerStatus (*RequestRunner)(const cJSON *request, const ComposerJob *job, ComposerResponse *out);

static ComposerStatus run_entry(RequestRunner run, const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    status = run(request, job, out);
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}

ComposerStatus composer_api_variants(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    return run_entry(make_variants, request_json, job, out);
}

ComposerStatus composer_api_recognize(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    return run_entry(recognize_request, request_json, job, out);
}
