#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "preferences/preferences_internal.h"

/*
 * The presets (P69c B5, R-754): named settings documents built in, each the preferences a style of her listening sets and the accompaniment
 * pattern it travels with. They are the files in presets/ embedded at build time (cmake/EmbedPresets.cmake); a request names one with
 * `preset`, and the settings of its own `preferences` apply over it.
 */

extern const char *const COMPOSER_PRESET_DOCUMENTS[];
extern const char *const COMPOSER_PRESET_NAMES[];
extern const char *const COMPOSER_PRESET_LABELS[];
extern const char *const COMPOSER_PRESET_SOURCES[];
extern const size_t COMPOSER_PRESET_DOCUMENT_COUNT;

size_t composer_preset_count(void) {
    return COMPOSER_PRESET_DOCUMENT_COUNT;
}

const char *composer_preset_name(size_t index) {
    return index < COMPOSER_PRESET_DOCUMENT_COUNT ? COMPOSER_PRESET_NAMES[index] : NULL;
}

const char *composer_preset_label(size_t index) {
    return index < COMPOSER_PRESET_DOCUMENT_COUNT ? COMPOSER_PRESET_LABELS[index] : NULL;
}

const char *composer_preset_source(size_t index) {
    return index < COMPOSER_PRESET_DOCUMENT_COUNT ? COMPOSER_PRESET_SOURCES[index] : NULL;
}

static void say(char *message, size_t size, const char *format, const char *text) {
    if (message && size > 0) {
        snprintf(message, size, format, text);
    }
}

bool composer_preset_apply(const char *name, ComposerPreferences *out, char *accompaniment, size_t accompaniment_size, char *message,
                           size_t message_size) {
    if (accompaniment && accompaniment_size > 0) {
        accompaniment[0] = '\0';
    }
    size_t at = 0;
    while (name && at < COMPOSER_PRESET_DOCUMENT_COUNT && strcmp(name, COMPOSER_PRESET_NAMES[at]) != 0) {
        at++;
    }
    if (at == COMPOSER_PRESET_DOCUMENT_COUNT) {
        say(message, message_size, "'%s' is not a preset", name ? name : "");
        return false;
    }
    cJSON *document = cJSON_Parse(COMPOSER_PRESET_DOCUMENTS[at]);
    const cJSON *preferences = document ? cJSON_GetObjectItemCaseSensitive(document, "preferences") : NULL;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    bool ok = cJSON_IsObject(preferences) && composer_preferences_apply(preferences, out, path, sizeof path, message, message_size);
    const cJSON *pattern = ok ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "accompaniment"), "pattern") : NULL;
    if (ok && cJSON_IsString(pattern) && accompaniment && accompaniment_size > 0) {
        snprintf(accompaniment, accompaniment_size, "%s", pattern->valuestring);
    }
    if (!ok && !cJSON_IsObject(preferences)) {
        say(message, message_size, "the preset '%s' holds no preferences", name);
    }
    cJSON_Delete(document);
    return ok;
}
