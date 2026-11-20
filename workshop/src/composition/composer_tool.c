#include "composition/composer_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/knowledge.h"
#include "composition/composer_client.h"

typedef UrsatzStatus (*ComposerEntry)(const ComposerContext *context, const char *request_json, const UrsatzJob *job,
                                      ComposerResponse *out);

typedef struct {
    const char *name;
    ComposerEntry entry; /* NULL for the entries with their own signature, which workshop_composer_tool handles by name */
    bool needs_request;
} EntryRow;

static const EntryRow kEntries[] = {
    {"export", composer_api_export, true},
    {"motive", composer_api_motive, true},
    {"motive_from_midi", NULL, true},
    {"motive_render", composer_api_motive_render, true},
    {"variant_operations", NULL, false},
    {"variants", composer_api_variants, true},
    {"recognize", composer_api_recognize, true},
    {"random_motive", composer_api_random_motive, true},
    {"theme", composer_api_theme, true},
    {"piece", composer_api_piece, true},
    {"piece_variations", composer_api_piece_variations, true},
    {"plan", composer_api_plan, true},
    {"fill", composer_api_fill, true},
    {"lock", composer_api_lock, true},
    {"regenerate", composer_api_regenerate, true},
    {"variations", composer_api_variations, true},
    {"harmonize", composer_api_harmonize, true},
    {"progression", composer_api_progression, true},
    {"counterpoint", composer_api_counterpoint, true},
    {"develop", composer_api_develop, true},
    {"grow", composer_api_grow, true},
    {"vocabulary", NULL, false},
    {"versions", NULL, false},
    {"validate", NULL, true},
};

UrsatzStatus workshop_composer_tool(const char *name, const cJSON *request, const char *kind, const uint8_t *midi, size_t midi_size,
                                    ComposerResponse *out, UrsatzError *error) {
    memset(out, 0, sizeof(*out));
    const EntryRow *row = NULL;
    for (size_t i = 0; name && i < sizeof(kEntries) / sizeof(kEntries[0]); i++) {
        if (strcmp(kEntries[i].name, name) == 0) {
            row = &kEntries[i];
            break;
        }
    }
    if (!row) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/entry", "unknown composer entry '%s'", name ? name : "");
    }
    if (row->needs_request && !cJSON_IsObject(request)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/request", "request is required and an object");
    }
    if (strcmp(name, "validate") == 0 && !kind) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/kind", "validate needs kind, a string");
    }
    if (strcmp(name, "motive_from_midi") == 0 && (!midi || midi_size == 0)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/bytes", "a MIDI file is required");
    }
    if (strcmp(name, "vocabulary") == 0) {
        /* The words are built once and kept (R-270): the cached text answers. */
        const char *text = workshop_composer_vocabulary();
        out->json = text ? strdup(text) : NULL;
        return out->json ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the Composer gave no vocabulary");
    }
    char *text = row->needs_request ? cJSON_PrintUnformatted(request) : NULL;
    if (row->needs_request && !text) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    const ComposerContext *context = workshop_composer_context_global();
    UrsatzStatus status;
    if (row->entry) {
        status = row->entry(context, text, NULL, out);
    } else if (strcmp(name, "motive_from_midi") == 0) {
        status = composer_api_motive_from_midi(context, midi, midi_size, text, NULL, out);
    } else if (strcmp(name, "variant_operations") == 0) {
        status = composer_api_variant_operations(out);
    } else if (strcmp(name, "versions") == 0) {
        status = composer_api_versions(out);
    } else {
        status = composer_api_validate(context, kind, text, out);
    }
    cJSON_free(text);
    if (status != URSATZ_OK && error) {
        /* The Composer's refusal, unchanged: its path points into the request. */
        snprintf(error->message, sizeof(error->message), "%s", out->error.message);
        snprintf(error->path, sizeof(error->path), "%s", out->error.path);
    }
    return status;
}
