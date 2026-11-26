#include "api_internal.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <ursatz_composer/preferences.h>

#include <realization/realization_json.h>
#include <reading/reading_json.h>

#include "api_vocabulary.h"
#include "check/hearing.h"
#include "check/hearing_result.h"
#include "render/movement_build.h"
#include "structure/request_parse.h"
#include "util/util.h"

UrsatzStatus api_fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s", path ? path : "");
    }
    return status;
}

UrsatzStatus api_seed_failure(CorpusSeedResult result, const char *what, const char *path, UrsatzError *error) {
    switch (result) {
    case CORPUS_SEED_ERR_NO_MATCH:
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, path, "no passage of corpus_passages gives %s", what);
    default:
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, path, "%s", corpus_seed_result_name(result));
    }
}

UrsatzStatus api_export_failure(UrsatzError *error, PlanExportResult result, const char *path, const char *what,
                                const PlanExportError *detail) {
    UrsatzStatus status = result == PLAN_EXPORT_ERR_ALLOC ? URSATZ_ERR_ALLOC
                            : result == PLAN_EXPORT_ERR_NO_CONTENT || result == PLAN_EXPORT_ERR_NOT_REPRESENTABLE ||
                                    result == PLAN_EXPORT_ERR_TOO_MANY_VOICES
                                ? URSATZ_ERR_REQUEST
                                : URSATZ_ERR_INTERNAL;
    return api_fail(error, status, status == URSATZ_ERR_REQUEST ? path : "", "%s%s%s", what,
                    detail && detail->message[0] ? ": " : "", detail ? detail->message : "");
}

bool api_fail_unsupported_key(UrsatzError *error, const char *path, const KnowledgeSnapshot *knowledge, const char *key_text) {
    char message[REQUEST_KEY_WORD_MAX + 96];
    if (!request_key_refusal(key_text, knowledge, message, sizeof message)) {
        return false;
    }
    api_fail(error, URSATZ_ERR_REQUEST, path, "%s", message);
    return true;
}

bool api_read_material(const cJSON *object, const char *base, bool required, const char **text, const char **member,
                       UrsatzError *error) {
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(object, "material");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(object, "mode");
    bool has_material = material && !cJSON_IsNull(material);
    bool has_mode = mode && !cJSON_IsNull(mode);
    char path[128];
    *text = NULL;
    *member = has_mode && !has_material ? "mode" : "material";
    if (has_material && has_mode) {
        snprintf(path, sizeof path, "%s/mode", base ? base : "");
        api_fail(error, URSATZ_ERR_REQUEST, path, "give \"material\" or the old \"mode\", not both");
        return false;
    }
    if (base) {
        return api_request_string(object, base, *member, required, text, error);
    }
    bool present = false;
    if (!api_member_string(object, *member, text, &present, error)) {
        return false;
    }
    if (!present && required) {
        snprintf(path, sizeof path, "/%s", *member);
        api_fail(error, URSATZ_ERR_REQUEST, path, "material is required: major or minor");
        return false;
    }
    return true;
}

UrsatzStatus api_resolve_material(const KnowledgeSnapshot *knowledge, const char *member, const char *path, const char *text,
                                    unsigned allow, const PitchMaterial **material, ApiMaterialKind *kind, UrsatzError *error) {
    CommonPracticeMaterials materials;
    char message[256];
    *material = NULL;
    *kind = API_MATERIAL_PICKED;
    if (!knowledge || !common_practice_materials_resolve(knowledge, &materials, message, sizeof message)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "a knowledge snapshot is required");
    }
    if ((allow & API_MATERIAL_ALLOW_STYLE) && request_equals_ignore_case(text, "style")) {
        *kind = API_MATERIAL_STYLE;
        return URSATZ_OK;
    }
    if ((allow & API_MATERIAL_ALLOW_RANDOM) && request_equals_ignore_case(text, "random")) {
        *kind = API_MATERIAL_RANDOM;
        return URSATZ_OK;
    }
    bool old = strcmp(member, "mode") == 0;
    if (old) {
        if (request_equals_ignore_case(text, "major")) {
            *material = materials.major;
        } else if (request_equals_ignore_case(text, "minor")) {
            *material = materials.natural_minor;
        } else {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "mode must be %s, not '%s'",
                            (allow & API_MATERIAL_ALLOW_STYLE) ? "major, minor or style" : "major or minor", text);
        }
        return URSATZ_OK;
    }
    const PitchMaterial *named = knowledge_snapshot_material_named(knowledge, text);
    if (!named) {
        return api_fail(error, URSATZ_ERR_REQUEST, path,
                        "material must be a material id, name, key word or alias%s, not '%s'",
                        (allow & API_MATERIAL_ALLOW_STYLE) ? " (or style)" : "", text);
    }
    if (!composer_material_supported(&materials, named)) {
        return api_fail(error, URSATZ_ERR_REQUEST, path,
                        "the Composer writes major, minor and modal keys; '%s' is not one of them", text);
    }
    *material = named;
    return URSATZ_OK;
}

UrsatzStatus api_require_knowledge(const ComposerContext *context, CommonPracticeMaterials *materials, UrsatzError *error) {
    if (!context || !context->knowledge) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "a knowledge snapshot is required");
    }
    char message[256];
    if (materials && !common_practice_materials_resolve(context->knowledge, materials, message, sizeof(message))) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "%s", message);
    }
    return URSATZ_OK;
}

UrsatzStatus api_fail_alloc(UrsatzError *error) {
    return api_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

bool api_add(cJSON *object, const char *name, cJSON *item) {
    if (!object || !item || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

UrsatzStatus api_parse_request(const char *text, cJSON **out, UrsatzError *error) {
    *out = NULL;
    if (!text) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "the request is missing");
    }
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        return api_fail(error, URSATZ_ERR_REQUEST, "", "the request is not valid JSON");
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return api_fail(error, URSATZ_ERR_REQUEST, "", "the request must be a JSON object");
    }
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "api_version");
    if (version && (!cJSON_IsNumber(version) || version->valuedouble != (double)COMPOSER_API_VERSION_MAJOR)) {
        cJSON_Delete(root);
        return api_fail(error, URSATZ_ERR_REQUEST, "/api_version", "api_version must be %d", COMPOSER_API_VERSION_MAJOR);
    }
    *out = root;
    return URSATZ_OK;
}

UrsatzStatus api_respond_json(ComposerResponse *out, cJSON *value) {
    char *text = value ? cJSON_PrintUnformatted(value) : NULL;
    cJSON_Delete(value);
    if (!text) {
        return api_fail(&out->error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    out->json = text;
    return URSATZ_OK;
}

/* The member, or NULL (with *present false) when it is absent or null. */
static const cJSON *member(const cJSON *object, const char *name, bool *present) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    bool found = item && !cJSON_IsNull(item);
    if (present) {
        *present = found;
    }
    return found ? item : NULL;
}

static bool member_fail(UrsatzError *error, const char *name, const char *what) {
    char path[sizeof error->path];
    snprintf(path, sizeof path, "/%s", name);
    api_fail(error, URSATZ_ERR_REQUEST, path, "%s must be %s", name, what);
    return false;
}

bool api_member_string(const cJSON *object, const char *name, const char **out, bool *present, UrsatzError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsString(item)) {
        return member_fail(error, name, "a string");
    }
    *out = item->valuestring;
    return true;
}

bool api_member_bool(const cJSON *object, const char *name, bool *out, bool *present, UrsatzError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        return member_fail(error, name, "true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool api_member_whole(const cJSON *object, const char *name, unsigned long minimum, unsigned long maximum,
                      unsigned long *out, bool *present, UrsatzError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    unsigned long value = 0;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 4294967295.0 &&
               item->valuedouble == (double)(unsigned long)item->valuedouble) {
        snprintf(text, sizeof text, "%lu", (unsigned long)item->valuedouble);
        source = text;
    } else if (cJSON_IsNumber(item)) {
        snprintf(text, sizeof text, "%g", item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_whole_number(source, maximum, &value) || value < minimum) {
        char path[sizeof error->path];
        snprintf(path, sizeof path, "/%s", name);
        api_fail(error, URSATZ_ERR_REQUEST, path, "%s must be a whole number from %lu to %lu, not '%s'", name, minimum,
                 maximum, source ? source : "?");
        return false;
    }
    *out = value;
    return true;
}

unsigned long api_default_seed(void) {
    return ((unsigned long)time(NULL) * 2654435761UL + (unsigned long)clock()) & 4294967295UL;
}

bool api_member_rational(const cJSON *object, const char *name, Rational *out, bool *present, UrsatzError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 4294967295.0 &&
               item->valuedouble == (double)(unsigned long)item->valuedouble) {
        snprintf(text, sizeof text, "%lu", (unsigned long)item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_ratio(source, out)) {
        return member_fail(error, name, "a whole number or a fraction like \"3/2\"");
    }
    return true;
}

UrsatzStatus api_read_tempo(const cJSON *request, PlanExportOptions *options, UrsatzError *error) {
    Rational tempo;
    bool has_tempo = false;
    if (!api_member_rational(request, "tempo", &tempo, &has_tempo, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (has_tempo && (tempo.den == 0 || tempo.num < (long long)API_TEMPO_MIN * tempo.den ||
                      tempo.num > (long long)API_TEMPO_MAX * tempo.den)) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, "tempo");
        char text[48] = "";
        if (cJSON_IsString(item)) {
            snprintf(text, sizeof text, "%s", item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            snprintf(text, sizeof text, "%g", item->valuedouble);
        }
        return api_fail(error, URSATZ_ERR_REQUEST, "/tempo",
                        "tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like 195/2, not '%s'",
                        text);
    }
    if (has_tempo) {
        options->tempo_numerator = tempo.num;
        options->tempo_denominator = tempo.den;
    }
    return URSATZ_OK;
}

UrsatzStatus api_load_style(const char *path, const char *const *without, size_t without_count, const char *path_file,
                              const char *path_without, ComposerStyle **out, UrsatzError *error) {
    ComposerStyleResult loaded = composer_style_load_without(path, without, without_count, out);
    if (loaded == COMPOSER_STYLE_OK) {
        return URSATZ_OK;
    }
    bool argument = loaded == COMPOSER_STYLE_ERR_ARGUMENT;
    return api_fail(error, argument ? URSATZ_ERR_REQUEST : loaded == COMPOSER_STYLE_ERR_ALLOC ? URSATZ_ERR_ALLOC
                                                                                                  : URSATZ_ERR_UNSATISFIABLE,
                    argument ? path_without : path_file, "cannot read style '%s': %s", path,
                    composer_style_result_name(loaded));
}

UrsatzStatus api_read_output_options(const cJSON *request, ApiOutputOptions *out, UrsatzError *error) {
    bool present;
    bool plain = false;
    const char *explain = NULL;
    memset(out, 0, sizeof(*out));
    out->export_options = plan_export_options_default();
    if (!api_member_bool(request, "midi", &out->midi, &present, error) ||
        !api_member_bool(request, "musicxml", &out->musicxml, &present, error) ||
        !api_member_bool(request, "plain_midi", &plain, &present, error) ||
        !api_member_bool(request, "plans", &out->plans, &present, error) ||
        !api_member_bool(request, "layers", &out->layers, &present, error) ||
        !api_member_string(request, "explain", &explain, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            out->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            out->explain = 2;
        } else {
            return api_fail(error, URSATZ_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    if (plain) {
        out->export_options.track_names = false;
        out->export_options.section_markers = false;
    }
    return URSATZ_OK;
}

/* Adds the text (one JSON document) parsed as member `name` of document; the text is freed. */
static bool add_parsed(cJSON *document, const char *name, char *text) {
    cJSON *item = text ? cJSON_Parse(text) : NULL;
    free(text);
    if (!item || !api_add(document, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

UrsatzStatus api_layers_json(const KnowledgeSnapshot *knowledge, const Plan *plan, const PlanExportOptions *options, cJSON *document,
                               UrsatzError *error) {
    ComposerHearing *hearing = NULL;
    HearingResult heard = hearing_create(plan, knowledge, &hearing);
    if (heard != HEARING_OK) {
        return heard == HEARING_ERR_ALLOC ? api_fail(error, URSATZ_ERR_ALLOC, "", "out of memory")
                                          : api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/layers", "cannot render the plan");
    }
    UrsatzStatus status = URSATZ_OK;
    Work *work = NULL;
    PlanExportError detail;
    char digest[17];
    const Reading *reading = NULL;
    RealizationJsonError realization_error;
    ReadingJsonError reading_error;
    if (rendered_plan_work(hearing_rendered(hearing), options, knowledge, &work, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/layers", "cannot write the realization: %s", detail.message);
    } else if (!realization_digest(work, digest)) {
        status = api_fail(error, URSATZ_ERR_INTERNAL, "/layers", "cannot name the realization");
    } else if (hearing_piece_reading_of(hearing, digest, &reading) != HEARING_OK) {
        status = api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/layers", "cannot read the piece");
    } else if (!add_parsed(document, "realization", realization_work_to_json(work, &realization_error)) ||
               !add_parsed(document, "reading", reading_to_json(reading, &reading_error))) {
        status = api_fail_alloc(error);
    }
    work_destroy(work);
    hearing_destroy(hearing);
    return status;
}

bool api_read_preferences(const cJSON *request, ComposerPreferences *out, bool *present, UrsatzError *error) {
    composer_preferences_default(out);
    const cJSON *item = member(request, "preferences", present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return member_fail(error, "preferences", "an object");
    }
    char path[COMPOSER_PREFERENCES_PATH_MAX / 2]; /* the reader's paths are short; "/preferences" + path fits error->path */
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (!composer_preferences_read(item, out, path, sizeof path, message, sizeof message)) {
        char full[sizeof error->path];
        snprintf(full, sizeof full, "/preferences%s", path);
        api_fail(error, URSATZ_ERR_REQUEST, full, "%s", message);
        return false;
    }
    return true;
}

bool api_read_preset_name(const cJSON *request, const char **name, UrsatzError *error) {
    bool given;
    const cJSON *item = member(request, "preset", &given);
    *name = NULL;
    if (!given) {
        return true;
    }
    if (!cJSON_IsString(item)) {
        return member_fail(error, "preset", "the name of a preset");
    }
    for (size_t i = 0; i < composer_preset_count(); i++) {
        if (strcmp(composer_preset_name(i), item->valuestring) == 0) {
            *name = item->valuestring;
            return true;
        }
    }
    api_fail(error, URSATZ_ERR_REQUEST, "/preset", "'%.40s' is not a preset; see the vocabulary's presets", item->valuestring);
    return false;
}

bool api_read_preferences_with_preset(const cJSON *request, ComposerPreferences *out, bool *present, char *accompaniment,
                                      size_t accompaniment_size, UrsatzError *error) {
    const char *preset = NULL;
    composer_preferences_default(out);
    if (accompaniment && accompaniment_size > 0) {
        accompaniment[0] = '\0';
    }
    if (!api_read_preset_name(request, &preset, error)) {
        return false;
    }
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (preset && !composer_preset_apply(preset, out, accompaniment, accompaniment_size, message, sizeof message)) {
        api_fail(error, URSATZ_ERR_INTERNAL, "/preset", "%s", message);
        return false;
    }
    bool given = false;
    const cJSON *item = member(request, "preferences", &given);
    if (present) {
        *present = given || preset != NULL;
    }
    if (!item) {
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return member_fail(error, "preferences", "an object");
    }
    char path[COMPOSER_PREFERENCES_PATH_MAX / 2];
    if (!composer_preferences_apply(item, out, path, sizeof path, message, sizeof message)) {
        char full[sizeof error->path];
        snprintf(full, sizeof full, "/preferences%s", path);
        api_fail(error, URSATZ_ERR_REQUEST, full, "%s", message);
        return false;
    }
    return true;
}

NodeId api_find_node(const Plan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* A number written with %g and read back, so a double in a result has no float noise. */
cJSON *api_number_g(double value) {
    char text[64];
    snprintf(text, sizeof text, "%g", value);
    return cJSON_CreateNumber(strtod(text, NULL));
}

cJSON *api_ratio_number(const Rational *ratio) {
    return api_number_g((double)ratio->num / (double)ratio->den);
}

/* True when the object has the member and it is not null. */
bool api_has_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return item && !cJSON_IsNull(item);
}
