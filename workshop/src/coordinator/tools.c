#include <stdlib.h>
#include <string.h>

#include "analysis/analyze_tool.h"
#include "analysis/corpus_tools.h"
#include "analysis/passages.h"
#include "base/names.h"
#include "composition/composer_tool.h"
#include "coordinator/command_schemas.h"
#include "coordinator/command_text.h"
#include "coordinator/command_sheet.h"
#include "items/views.h"
#include "coordinator/dispatch.h"

/* The tools (R-656): the calls that work on the files their params name and never on the data folder or the journal. dispatch.c lists them. */

/* The versions this build holds: what workshop_status answers, without the data folder. */
WORKSHOP_HANDLER(workshop_t_versions) {
    cJSON *status = NULL;
    UrsatzStatus result = workshop_status(call->workshop, &status, error);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(status, "data_dir");
    return workshop_reply_tree(reply, status, error);
}

static bool add_text(cJSON *object, const char *name, const char *text) {
    return text ? cJSON_AddStringToObject(object, name, text) != NULL : cJSON_AddNullToObject(object, name) != NULL;
}

/* One Composer API entry (composition/composer_tool.h): params entry, request and kind; the reply holds the response's three texts and its
 * MIDI file as bytes. */
WORKSHOP_HANDLER(workshop_t_composer) {
    bool bad = false;
    const char *entry = workshop_param_string(call->params, "entry", &bad);
    if (!entry || bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/entry", "entry is required and a string");
    }
    const char *kind = workshop_param_string(call->params, "kind", &bad);
    if (bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/kind", "kind is a string");
    }
    ComposerResponse response;
    UrsatzStatus status = workshop_composer_tool(entry, cJSON_GetObjectItemCaseSensitive(call->params, "request"), kind,
                                                 call->bytes ? call->bytes->data : NULL, call->bytes ? call->bytes->size : 0, &response,
                                                 error);
    /* A refusal may still carry an answer (a plan that could not be completed lists what is stuck): the reply holds it too. */
    if (status == URSATZ_OK || response.json || response.plan_json || response.musicxml) {
        cJSON *object = workshop_reply_object(reply);
        if (!object || !add_text(object, "json", response.json) || !add_text(object, "plan_json", response.plan_json) ||
            !add_text(object, "musicxml", response.musicxml)) {
            status = workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        } else if (response.midi) {
            workshop_reply_take_bytes(reply, response.midi, response.midi_size, "audio/midi");
            response.midi = NULL;
            response.midi_size = 0;
        }
    }
    composer_response_destroy(&response);
    return status;
}

/* The passages of a corpus that look like a file or a progression (analysis/passages.h). */
WORKSHOP_HANDLER(workshop_t_corpus_passages) {
    bool bad = false;
    const char *dir = workshop_param_string(call->params, "dir", &bad);
    if (!dir || bad || dir[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/dir", "dir is required and a string");
    }
    bool bad_like = false;
    bool bad_numerals = false;
    const char *like = workshop_param_string(call->params, "like", &bad_like);
    const char *numerals = workshop_param_string(call->params, "numerals", &bad_numerals);
    if (bad_like || bad_numerals || (like == NULL) == (numerals == NULL)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "give exactly one of like (a file) and numerals (a text)");
    }
    long long limit = 10;
    const cJSON *limit_item = cJSON_GetObjectItemCaseSensitive(call->params, "limit");
    if (limit_item && !cJSON_IsNull(limit_item)) {
        if (!cJSON_IsNumber(limit_item) || limit_item->valuedouble < 1 || limit_item->valuedouble > 100 ||
            limit_item->valuedouble != (double)(long long)limit_item->valuedouble) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/limit", "limit is a whole number from 1 to 100");
        }
        limit = (long long)limit_item->valuedouble;
    }
    cJSON *array = NULL;
    UrsatzStatus status = workshop_corpus_passages(dir, like, numerals, (size_t)limit, &array, error);
    return status == URSATZ_OK ? workshop_reply_member(reply, "passages", array, error) : status;
}

/* MIDI numbers as note names (base/names.h). */
WORKSHOP_HANDLER(workshop_t_pitch_names) {
    cJSON *names = NULL;
    UrsatzStatus status = workshop_pitch_names(cJSON_GetObjectItemCaseSensitive(call->params, "midi"), &names, error);
    return status == URSATZ_OK ? workshop_reply_member(reply, "names", names, error) : status;
}

/* Reads the optional string member name of params into *out (NULL when absent); false with the failure filled when it is not a string. */
static bool optional_string(const cJSON *params, const char *name, const char **out, UrsatzError *error) {
    bool bad = false;
    *out = workshop_param_string(params, name, &bad);
    if (bad) {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "%s is a string", name);
    }
    return !bad;
}

static bool optional_flag(const cJSON *params, const char *name, bool *out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    *out = cJSON_IsTrue(item);
    if (item && !cJSON_IsNull(item) && !cJSON_IsBool(item)) {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "%s is true or false", name);
        return false;
    }
    return true;
}

/* A file analyzed as the Analyzer's command line did (analysis/analyze_tool.h). */
WORKSHOP_HANDLER(workshop_t_analyze) {
    WorkshopAnalyzeRequest request;
    memset(&request, 0, sizeof(request));
    bool bad = false;
    request.path = workshop_param_string(call->params, "path", &bad);
    if (!request.path || bad || request.path[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/path", "path is required and a string");
    }
    if (!optional_string(call->params, "input_format", &request.input_format, error) ||
        !optional_string(call->params, "meter", &request.meter, error) || !optional_string(call->params, "value_scale", &request.value_scale, error) ||
        !optional_string(call->params, "pickup", &request.pickup, error) || !optional_flag(call->params, "layers", &request.layers, error) ||
        !optional_flag(call->params, "model_plan", &request.model_plan, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(call->params, "text");
    if (text && !cJSON_IsNull(text)) {
        if (!cJSON_IsObject(text)) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/text", "text is an object {first_measure, last_measure, title}");
        }
        request.text = true;
        const char *const names[2] = {"first_measure", "last_measure"};
        int *const targets[2] = {&request.first_measure, &request.last_measure};
        for (int i = 0; i < 2; i++) {
            const cJSON *number = cJSON_GetObjectItemCaseSensitive(text, names[i]);
            if (number && !cJSON_IsNull(number)) {
                if (!cJSON_IsNumber(number) || number->valuedouble < 0 || number->valuedouble > 2147483647.0 ||
                    number->valuedouble != (double)(int)number->valuedouble) {
                    return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/text", "%s is a whole number from 0", names[i]);
                }
                *targets[i] = (int)number->valuedouble;
            }
        }
        if (!optional_string(text, "title", &request.title, error)) {
            return URSATZ_ERR_REQUEST;
        }
    }
    cJSON *answer = NULL;
    UrsatzStatus status = workshop_analyze(&request, &answer, error);
    return status == URSATZ_OK ? workshop_reply_tree(reply, answer, error) : status;
}

/* A folder of pieces analyzed into a corpus output (analysis/corpus_tools.h). */
WORKSHOP_HANDLER(workshop_t_corpus) {
    bool bad = false;
    const char *dir = workshop_param_string(call->params, "dir", &bad);
    if (!dir || bad || dir[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/dir", "dir is required and a string");
    }
    const char *out = NULL;
    bool resume = false;
    if (!optional_string(call->params, "out", &out, error) || !optional_flag(call->params, "resume", &resume, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *answer = NULL;
    UrsatzStatus status = workshop_corpus_run(dir, out, resume, &answer, error);
    return status == URSATZ_OK ? workshop_reply_tree(reply, answer, error) : status;
}

/* The style model of a corpus output (analysis/corpus_tools.h). */
WORKSHOP_HANDLER(workshop_t_style_profile) {
    bool bad = false;
    const char *dir = workshop_param_string(call->params, "dir", &bad);
    if (!dir || bad || dir[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/dir", "dir is required and a string");
    }
    const char *name = workshop_param_string(call->params, "name", &bad);
    if (!name || bad || name[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/name", "name is required and a string that is not empty");
    }
    const char *out = NULL;
    if (!optional_string(call->params, "out", &out, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *answer = NULL;
    UrsatzStatus status = workshop_style_profile(dir, name, out, &answer, error);
    return status == URSATZ_OK ? workshop_reply_tree(reply, answer, error) : status;
}

/* A file or a text of any layer kind as Ursatz Text, a view of it or its JSON (items/views.h). */
WORKSHOP_HANDLER(workshop_t_text) {
    const char *path = NULL;
    const char *text = NULL;
    const char *view = NULL;
    if (!optional_string(call->params, "path", &path, error) || !optional_string(call->params, "text", &text, error) ||
        !optional_string(call->params, "view", &view, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if ((path == NULL) == (text == NULL)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "give exactly one of path (a file) and text");
    }
    TextViewLimits limits;
    UrsatzStatus status = workshop_view_limits(call->params, &limits, error);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *answer = NULL;
    status = workshop_view_input(path, text, view && view[0] ? view : "full", &limits, &answer, error);
    return status == URSATZ_OK ? workshop_reply_tree(reply, answer, error) : status;
}

/* The command reference generated from the schemas (coordinator/command_sheet.h): params format (sheet, markdown, grammar, json or help) and name (for help). */
WORKSHOP_HANDLER(workshop_t_commands) {
    const char *format = NULL;
    const char *name = NULL;
    if (!optional_string(call->params, "format", &format, error) || !optional_string(call->params, "name", &name, error)) {
        return URSATZ_ERR_REQUEST;
    }
    format = format && format[0] ? format : "sheet";
    char *text = NULL;
    if (strcmp(format, "sheet") == 0) {
        text = workshop_command_sheet_text();
    } else if (strcmp(format, "markdown") == 0) {
        text = workshop_command_sheet_markdown();
    } else if (strcmp(format, "grammar") == 0) {
        text = workshop_command_sheet_grammar();
    } else if (strcmp(format, "json") == 0) {
        text = workshop_command_sheet_json();
    } else if (strcmp(format, "help") == 0) {
        if (!name || !name[0]) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/name", "help needs the name of a command");
        }
        const cJSON *schema = workshop_command_schema(name);
        if (!schema) {
            return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/name", "unknown command '%s'", name);
        }
        text = workshop_command_sheet_help(schema);
    } else {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/format", "format is sheet, markdown, grammar, json or help");
    }
    if (!text) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON *answer = cJSON_CreateObject();
    bool ok = answer && cJSON_AddStringToObject(answer, "format", format) && cJSON_AddStringToObject(answer, "text", text);
    free(text);
    if (!ok) {
        cJSON_Delete(answer);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_tree(reply, answer, error);
}

/* Commands written in Ursatz Text as the command objects `ursatz apply` sends (coordinator/command_text.h): param text. */
WORKSHOP_HANDLER(workshop_t_commands_parse) {
    bool bad = false;
    const char *text = workshop_param_string(call->params, "text", &bad);
    if (!text || bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/text", "text is required and a string");
    }
    cJSON *answer = NULL;
    UrsatzStatus status = workshop_command_text_parse(text, &answer, error);
    return status == URSATZ_OK ? workshop_reply_tree(reply, answer, error) : status;
}
