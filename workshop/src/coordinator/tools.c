#include <string.h>

#include "analysis/passages.h"
#include "base/names.h"
#include "composition/composer_tool.h"
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
    if (status == URSATZ_OK) {
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
