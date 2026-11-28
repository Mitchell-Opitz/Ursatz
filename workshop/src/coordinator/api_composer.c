#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"

/* The Composer queries (R-646): what the Composer knows and renders without saving anything. */

/* The versions this build holds: members available, version ("ursatz-composer <n>", the text the command printed), api_version,
 * analyzer_version and producer_key. */
WORKSHOP_HANDLER(workshop_q_composer_status) {
    cJSON *versions = NULL;
    UrsatzError failure = {{0}, {0}};
    if (workshop_status(call->workshop, &versions, &failure) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "composer failed");
    }
    const cJSON *composer_version = cJSON_GetObjectItemCaseSensitive(versions, "composer_version");
    const cJSON *api_version = cJSON_GetObjectItemCaseSensitive(versions, "api_version");
    const cJSON *producer_key = cJSON_GetObjectItemCaseSensitive(versions, "producer_key");
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(versions, "analyzer_version");
    char version[128];
    snprintf(version, sizeof(version), "ursatz-composer %s", cJSON_IsString(composer_version) ? composer_version->valuestring : "");
    cJSON *object = workshop_reply_object(reply);
    bool built = object && cJSON_IsString(composer_version) && cJSON_IsString(api_version) && cJSON_IsString(producer_key) &&
                 cJSON_IsString(analyzer) && cJSON_AddTrueToObject(object, "available") &&
                 cJSON_AddStringToObject(object, "version", version) &&
                 cJSON_AddStringToObject(object, "api_version", api_version->valuestring) &&
                 cJSON_AddStringToObject(object, "analyzer_version", analyzer->valuestring) &&
                 cJSON_AddStringToObject(object, "producer_key", producer_key->valuestring);
    cJSON_Delete(versions);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "composer failed");
}

/* The words the Composer reads, as composer_api_vocabulary gives them (built once and kept, R-270). */
WORKSHOP_HANDLER(workshop_q_composer_vocabulary) {
    (void)call;
    const char *vocabulary = workshop_composer_vocabulary();
    cJSON *tree = vocabulary ? cJSON_Parse(vocabulary) : NULL;
    return tree ? workshop_reply_tree(reply, tree, error) : workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "composer failed");
}

/* The forms the Composer builds and their sections, sizes and slots, as composer_api_forms gives them (built once and kept). */
WORKSHOP_HANDLER(workshop_q_composer_forms) {
    (void)call;
    const char *forms = workshop_composer_forms();
    cJSON *tree = forms ? cJSON_Parse(forms) : NULL;
    return tree ? workshop_reply_tree(reply, tree, error) : workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "composer failed");
}

/* The Composer's variant operation list, as the Composer API gives it. */
WORKSHOP_HANDLER(workshop_q_composer_variant_operations) {
    (void)call;
    WorkshopComposerReply composer;
    workshop_motif_variant_operations(&composer);
    cJSON *document = composer.status == URSATZ_OK && composer.json ? cJSON_Parse(composer.json) : NULL;
    if (!document) {
        UrsatzStatus status = composer.status != URSATZ_OK ? composer.status : URSATZ_ERR_INTERNAL;
        workshop_call_fail(error, status, composer.path, "%s", composer.message[0] ? composer.message : "composer could not list the operations");
        workshop_composer_reply_destroy(&composer);
        return status;
    }
    workshop_composer_reply_destroy(&composer);
    return workshop_reply_tree(reply, document, error);
}

/* The answer of a Composer call that keeps an HTTP status for its refusal: the document, or the failure with that status as a status. */
static UrsatzStatus composer_answer(WorkshopReply *reply, cJSON *document, UrsatzStatus result, int http_status, UrsatzError *error) {
    if (result != URSATZ_OK) {
        cJSON_Delete(document);
        return workshop_status_of_http(http_status);
    }
    return workshop_reply_tree(reply, document, error);
}

/* A motif rendered from the request in params (form, degrees, meter, key?, pickup?, render_key?, tempo?, role?); nothing is saved. */
WORKSHOP_HANDLER(workshop_q_composer_render) {
    cJSON *answer = NULL;
    int status = 200;
    UrsatzStatus result = workshop_motif_render(call->params, &answer, &status, error);
    return composer_answer(reply, answer, result, status, error);
}

/* A progression request written with its MIDI and MusicXML (R-769): the document, its MusicXML and the playback; nothing is saved. */
WORKSHOP_HANDLER(workshop_q_composer_progression) {
    cJSON *answer = NULL;
    int status = 200;
    UrsatzStatus result = workshop_progression_preview(call->params, &answer, &status, error);
    return composer_answer(reply, answer, result, status, error);
}

/* Random motives from the Composer (material, meter, key?, form?, style_id?, count?, seed?); nothing is saved. */
WORKSHOP_HANDLER(workshop_q_composer_random_motifs) {
    cJSON *document = NULL;
    int status = 200;
    UrsatzStatus result = workshop_motif_random(call->params, &document, &status, error);
    return composer_answer(reply, document, result, status, error);
}

/* A MIDI file read as a motif (bytes; params meter and key optional); nothing is saved. */
WORKSHOP_HANDLER(workshop_q_composer_import_motif) {
    if (!call->bytes || call->bytes->size == 0) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/bytes", "a MIDI file is required");
    }
    bool bad = false;
    const char *meter = workshop_param_string(call->params, "meter", &bad);
    const char *key = workshop_param_string(call->params, "key", &bad);
    if (bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "meter and key are strings");
    }
    cJSON *answer = NULL;
    int status = 200;
    UrsatzStatus result = workshop_motif_import(call->bytes->data, call->bytes->size, meter, key, &answer, &status, error);
    return composer_answer(reply, answer, result, status, error);
}

/* How the target derives from the source: params source {item, version} and target {item, version}. */
WORKSHOP_HANDLER(workshop_q_composer_recognize) {
    cJSON *document = NULL;
    int status = 200;
    UrsatzStatus result = workshop_variants_recognize(call->params, &document, &status, error);
    return composer_answer(reply, document, result, status, error);
}
