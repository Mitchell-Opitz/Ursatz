#include "items/views.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <plan/plan_json.h>
#include <reading/reading_json.h>
#include <realization/realization_json.h>
#include <text/text_layers.h>
#include <text/text_tree.h>

#include "base/knowledge.h"
#include "items/layers.h"
#include "items/store.h"

UrsatzStatus workshop_view_fail(UrsatzError *error, const TextError *text_error) {
    const char *path = "/params/view";
    if (strncmp(text_error->message, "unknown section", 15) == 0) {
        path = "/params/section";
    } else if (strncmp(text_error->message, "measures ", 9) == 0) {
        path = "/params/measures";
    }
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, path, "%s", text_error->message);
}

/* The layers a source holds, read with the codecs from the codec's JSON. */
typedef struct {
    Plan *plan;
    Work *work;
    Reading *reading;
} Layer;

static void layer_free(Layer *layer) {
    plan_destroy(layer->plan);
    work_destroy(layer->work);
    reading_destroy(layer->reading);
}

static UrsatzStatus layer_read(TextKind kind, const char *json, const char *where, Layer *layer, UrsatzError *error) {
    const KnowledgeSnapshot *knowledge = workshop_knowledge_snapshot();
    if (kind == TEXT_KIND_PLAN) {
        PlanJsonError failure = {{0}, {0}, 0};
        if (plan_from_json(knowledge, json, NULL, &layer->plan, &failure) != PLAN_OK) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s%s%s", failure.path, failure.path[0] ? ": " : "", failure.message);
        }
    } else if (kind == TEXT_KIND_READING) {
        ReadingJsonError failure = {{0}, {0}};
        if (reading_from_json(knowledge, json, &layer->reading, &failure) != REALIZATION_OK) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s%s%s", failure.path, failure.path[0] ? ": " : "", failure.message);
        }
    } else if (kind == TEXT_KIND_REALIZATION) {
        RealizationJsonError failure = {{0}, {0}};
        if (realization_work_from_json(knowledge, json, &layer->work, &failure) != REALIZATION_OK) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s%s%s", failure.path, failure.path[0] ? ": " : "", failure.message);
        }
    }
    return URSATZ_OK;
}

static bool add_pair(cJSON *object, const char *kind, const char *name, const char *text) {
    return cJSON_AddStringToObject(object, "kind", kind) && cJSON_AddStringToObject(object, name, text);
}

static bool member_is(const cJSON *document, const char *name, const char *value) {
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(document, name);
    return cJSON_IsString(m) && m->valuestring && strcmp(m->valuestring, value) == 0;
}

/* The kind a JSON object is, from its "format" or "schema" (B1's checks); json for any other object. */
static TextKind json_kind(const cJSON *document) {
    if (member_is(document, "format", "ursatz-plan") || member_is(document, "format", "ursatz-composer-plan")) {
        return TEXT_KIND_PLAN;
    }
    if (member_is(document, "format", "ursatz-reading")) {
        return TEXT_KIND_READING;
    }
    if (member_is(document, "format", "ursatz-realization")) {
        return TEXT_KIND_REALIZATION;
    }
    return member_is(document, "schema", "ursatz-knowledge") ? TEXT_KIND_KNOWLEDGE : TEXT_KIND_JSON;
}

UrsatzStatus workshop_view_limits(const cJSON *params, TextViewLimits *out, UrsatzError *error) {
    out->first_measure = 0;
    out->last_measure = 0;
    out->section = NULL;
    const cJSON *measures = cJSON_GetObjectItemCaseSensitive(params, "measures");
    if (measures && !cJSON_IsNull(measures)) {
        const cJSON *first = cJSON_IsObject(measures) ? cJSON_GetObjectItemCaseSensitive(measures, "first") : NULL;
        const cJSON *last = cJSON_IsObject(measures) ? cJSON_GetObjectItemCaseSensitive(measures, "last") : NULL;
        bool ok = cJSON_IsObject(measures);
        const cJSON *both[2] = {first, last};
        int *targets[2] = {&out->first_measure, &out->last_measure};
        for (int i = 0; ok && i < 2; i++) {
            if (both[i] && !cJSON_IsNull(both[i])) {
                ok = cJSON_IsNumber(both[i]) && both[i]->valuedouble >= 0 && both[i]->valuedouble <= 999999 &&
                     both[i]->valuedouble == (double)(int)both[i]->valuedouble;
                *targets[i] = ok ? (int)both[i]->valuedouble : 0;
            }
        }
        if (!ok) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/measures", "measures is an object {first, last} of whole numbers from 0");
        }
    }
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(params, "section");
    if (section && !cJSON_IsNull(section)) {
        if (!cJSON_IsString(section)) {
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/section", "section is a string");
        }
        out->section = section->valuestring[0] ? section->valuestring : NULL;
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_view_input(const char *path, const char *text, const char *view, const TextViewLimits *limits, cJSON **out,
                                 UrsatzError *error) {
    const char *where = path ? "/params/path" : "/params/text";
    char *source = NULL;
    if (path) {
        UrsatzStatus read = common_file_read(path, &source, NULL, NULL);
        if (read == URSATZ_ERR_ALLOC) {
            return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        if (read != URSATZ_OK) {
            return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/path", "cannot read %s", path);
        }
        text = source;
    }
    const char *first = text;
    while (first && isspace((unsigned char)*first)) {
        first++;
    }
    UrsatzStatus status = URSATZ_OK;
    TextKind kind = TEXT_KIND_JSON;
    char *json = NULL;    /* the layer or object as JSON text: the input, or what the codec wrote for a text input */
    char *printed = NULL; /* the input as Ursatz Text, made when asked for */
    if (first && *first == '{') {
        cJSON *document = cJSON_Parse(text);
        if (!document) {
            status = ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "the JSON does not parse");
        } else {
            kind = json_kind(document);
            json = cJSON_PrintUnformatted(document);
            cJSON_Delete(document);
        }
    } else {
        TextError failure = {0, ""};
        json = text_to_json(workshop_knowledge_snapshot(), text ? text : "", &kind, &failure);
        if (!json) {
            status = failure.line > 0 ? ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "line %d: %s", failure.line, failure.message)
                                      : ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s", failure.message);
        }
    }
    cJSON *answer = NULL;
    if (status == URSATZ_OK) {
        answer = cJSON_CreateObject();
        if (!answer || !json) {
            status = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    if (status == URSATZ_OK && strcmp(view, "json") == 0) {
        /* the codec's own text for the layers (a JSON input goes through the codec and back), the tree's for the rest */
        char *normal = NULL;
        if (kind == TEXT_KIND_PLAN || kind == TEXT_KIND_READING || kind == TEXT_KIND_REALIZATION) {
            TextError failure = {0, ""};
            printed = text_from_json(json, kind, &failure);
            normal = printed ? text_to_json(workshop_knowledge_snapshot(), printed, &kind, &failure) : NULL;
            if (!normal) {
                status = ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s", failure.message);
            }
        } else {
            normal = json;
            json = NULL;
        }
        if (status == URSATZ_OK && !add_pair(answer, text_kind_name(kind), "json", normal)) {
            status = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        if (normal != json) {
            free(normal);
        }
    } else if (status == URSATZ_OK && strcmp(view, "full") == 0) {
        TextError failure = {0, ""};
        printed = text_from_json(json, kind, &failure);
        if (!printed) {
            status = ursatz_error_set(error, URSATZ_ERR_REQUEST, where, "%s", failure.message);
        } else if (!add_pair(answer, text_kind_name(kind), "text", printed)) {
            status = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    } else if (status == URSATZ_OK) {
        Layer layer = {NULL, NULL, NULL};
        status = layer_read(kind, json, where, &layer, error);
        if (status == URSATZ_OK) {
            TextViewSource source_layers = {path ? path : "text", layer.plan, layer.work, layer.reading, workshop_knowledge_snapshot()};
            TextError failure = {0, ""};
            printed = text_view_print(&source_layers, view, limits, &failure);
            if (!printed) {
                status = workshop_view_fail(error, &failure);
            } else if (!add_pair(answer, text_kind_name(kind), "text", printed)) {
                status = ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
            }
        }
        layer_free(&layer);
    }
    free(printed);
    free(json);
    free(source);
    if (status != URSATZ_OK) {
        cJSON_Delete(answer);
        return status;
    }
    *out = answer;
    return URSATZ_OK;
}

/* ---- the layers of a folder ----------------------------------------------------------------------------------------------------- */

typedef struct {
    char *plan; /* the files' texts, NULL when the folder has none */
    char *realization;
    char *reading;
} Files;

static void files_free(Files *f) {
    free(f->plan);
    free(f->realization);
    free(f->reading);
}

static char *read_if(const char *dir, const char *name) {
    char path[1100];
    char *data = NULL;
    if (!common_path_join(path, sizeof path, dir, name) || !common_path_exists(path)) {
        return NULL;
    }
    return common_file_read(path, &data, NULL, NULL) == URSATZ_OK ? data : NULL;
}

static void files_read(const char *dir, bool with_plan, const char *reading_file, Files *f) {
    memset(f, 0, sizeof *f);
    f->plan = with_plan ? read_if(dir, "plan.json") : NULL;
    f->realization = read_if(dir, WORKSHOP_FILE_REALIZATION);
    f->reading = read_if(dir, reading_file);
}

/* Each layer's text, plan first, joined by a blank line; NULL with *failure set when one does not print. */
static char *files_full(const Files *f, UrsatzError *error) {
    const struct {
        const char *json;
        TextKind kind;
    } layers[3] = {{f->plan, TEXT_KIND_PLAN}, {f->realization, TEXT_KIND_REALIZATION}, {f->reading, TEXT_KIND_READING}};
    char *out = NULL;
    for (int i = 0; i < 3; i++) {
        if (!layers[i].json) {
            continue;
        }
        TextError failure = {0, ""};
        char *one = text_from_json(layers[i].json, layers[i].kind, &failure);
        if (!one) {
            free(out);
            ursatz_error_set(error, URSATZ_ERR_INTERNAL, "/params/view", "a stored %s does not print: %s", text_kind_name(layers[i].kind), failure.message);
            return NULL;
        }
        if (!out) {
            out = one;
            continue;
        }
        size_t a = strlen(out), b = strlen(one);
        char *joined = malloc(a + b + 2);
        if (!joined) {
            free(out);
            free(one);
            ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
            return NULL;
        }
        memcpy(joined, out, a);
        joined[a] = '\n';
        memcpy(joined + a + 1, one, b + 1);
        free(out);
        free(one);
        out = joined;
    }
    return out;
}

/* A view of the layers the files hold (every view but full and json). */
static UrsatzStatus files_view(const Files *f, const char *address, const char *view, const TextViewLimits *limits, char **text, UrsatzError *error) {
    Layer layer = {NULL, NULL, NULL};
    UrsatzStatus status = URSATZ_OK;
    if (f->plan) {
        status = layer_read(TEXT_KIND_PLAN, f->plan, "/params/view", &layer, error);
    }
    if (status == URSATZ_OK && f->realization) {
        status = layer_read(TEXT_KIND_REALIZATION, f->realization, "/params/view", &layer, error);
    }
    if (status == URSATZ_OK && f->reading) {
        status = layer_read(TEXT_KIND_READING, f->reading, "/params/view", &layer, error);
    }
    if (status == URSATZ_OK) {
        TextViewSource source = {address, layer.plan, layer.work, layer.reading, workshop_knowledge_snapshot()};
        TextError failure = {0, ""};
        *text = text_view_print(&source, view, limits, &failure);
        if (!*text) {
            status = workshop_view_fail(error, &failure);
        }
    }
    layer_free(&layer);
    return status;
}

UrsatzStatus workshop_view_folder(const char *folder, bool with_plan, const char *reading_file, const char *address, const char *view,
                                  const TextViewLimits *limits, char **text, bool *none, UrsatzError *error) {
    *text = NULL;
    Files files;
    files_read(folder, with_plan, reading_file, &files);
    *none = !files.plan && !files.realization && !files.reading;
    UrsatzStatus status = URSATZ_OK;
    if (*none) {
        status = URSATZ_OK;
    } else if (strcmp(view, "full") == 0) {
        *text = files_full(&files, error);
        status = *text ? URSATZ_OK : URSATZ_ERR_INTERNAL;
    } else {
        status = files_view(&files, address, view, limits, text, error);
    }
    files_free(&files);
    return status;
}

UrsatzStatus workshop_view_item(const char *id, int version, const char *view, const TextViewLimits *limits, char **text, UrsatzError *error) {
    *text = NULL;
    cJSON *meta = NULL;
    UrsatzStatus status = workshop_material_item_get(id, &meta, NULL);
    if (status != URSATZ_OK) {
        return ursatz_error_set(error, status == URSATZ_ERR_NOT_FOUND ? status : URSATZ_ERR_INTERNAL, "/params/id", "item not found");
    }
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
    if (version <= 0) {
        version = cJSON_IsNumber(current) ? (int)current->valuedouble : 1;
    }
    bool in_range = cJSON_IsNumber(current) && version >= 1 && version <= (int)current->valuedouble;
    cJSON_Delete(meta);
    char dir[1024];
    if (!in_range || !workshop_material_version_dir(id, version, dir, sizeof dir) || !common_path_is_dir(dir)) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/version", "version not found");
    }
    char address[160];
    snprintf(address, sizeof address, "item:%s@v%d", id, version);
    bool none = false;
    status = workshop_view_folder(dir, true, WORKSHOP_DIR_READINGS "/composer.json", address, view, limits, text, &none, error);
    if (status == URSATZ_OK && none) {
        if (strcmp(view, "full") == 0) {
            /* no layer: the version's body, as a json document */
            cJSON *body = NULL;
            TextError failure = {0, ""};
            status = workshop_material_item_get_version(id, version, &body);
            char *json = status == URSATZ_OK ? cJSON_PrintUnformatted(body) : NULL;
            cJSON_Delete(body);
            *text = json ? text_from_json(json, TEXT_KIND_JSON, &failure) : NULL;
            free(json);
            status = *text ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/version", "version not found");
        } else {
            status = ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/view", "this version holds no plan, realization or reading");
        }
    }
    return status;
}
