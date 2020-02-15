#include <ursatz_analyzer/markdown_report.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "pipeline/analysis_document_internal.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static bool sb_init(StringBuilder *sb) {
    sb->capacity = 256;
    sb->length = 0;
    sb->data = malloc(sb->capacity);
    if (!sb->data) {
        return false;
    }
    sb->data[0] = '\0';
    return true;
}

static bool sb_append(StringBuilder *sb, const char *text) {
    size_t text_len = strlen(text);
    size_t needed = sb->length + text_len + 1;
    if (needed > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        char *grown = realloc(sb->data, new_capacity);
        if (!grown) {
            return false;
        }
        sb->data = grown;
        sb->capacity = new_capacity;
    }
    memcpy(sb->data + sb->length, text, text_len + 1);
    sb->length += text_len;
    return true;
}

/* Formats into the builder, sized by a first vsnprintf pass so no line
   is ever truncated. */
static bool sb_appendf(StringBuilder *sb, const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list sizing_args;
    va_copy(sizing_args, args);
    int length = vsnprintf(NULL, 0, format, sizing_args);
    va_end(sizing_args);
    if (length < 0) {
        va_end(args);
        return false;
    }

    char *text = malloc((size_t)length + 1);
    if (!text) {
        va_end(args);
        return false;
    }
    vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);

    bool ok = sb_append(sb, text);
    free(text);
    return ok;
}

/* ---- Document access ---- */

static const char *string_member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static double number_member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : 0.0;
}

/* ---- Sections ---- */

/* Renders one report section from the whole document and that
   section's own data (never NULL, null, or empty). */
typedef bool (*SectionRenderer)(StringBuilder *sb, const cJSON *document, const cJSON *section);

static bool render_title(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    (void)document;
    const char *source_file = string_member(metadata, "source_file");
    return sb_appendf(sb, "# Analysis Report: %s\n\n", source_file ? source_file : "");
}

static bool render_key(StringBuilder *sb, const cJSON *document, const cJSON *keys) {
    (void)document;
    const char *label = string_member(cJSON_GetArrayItem(keys, 0), "label");
    return sb_appendf(sb, "**Detected key:** %s\n\n", label ? label : "");
}

static bool render_chords(StringBuilder *sb, const cJSON *document, const cJSON *chords) {
    (void)document;
    bool ok = sb_append(sb, "## Chord Regions\n\n") &&
              sb_append(sb, "Times are beats from the start of the file.\n\n");
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (!ok) {
            break;
        }
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(chord, "span"), "start");
        double beats = 2.0 * number_member(start, "num") / number_member(start, "den");
        const char *label = string_member(chord, "label");
        ok = sb_appendf(sb, "- Beat %.2f: %s\n", beats, label ? label : "");
    }
    return ok;
}

/* Report sections in document section order. Adding a section means
   adding one row and one render function. */
static const struct {
    const char *document_key;
    SectionRenderer render;
} SECTIONS[] = {
    {"metadata", render_title},
    {"keys", render_key},
    {"chords", render_chords},
};

static bool section_is_empty(const cJSON *section) {
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    return (cJSON_IsArray(section) || cJSON_IsObject(section)) && section->child == NULL;
}

char *markdown_report_render(const AnalysisDocument *document) {
    StringBuilder sb;
    if (!sb_init(&sb)) {
        return NULL;
    }

    const cJSON *root = document->root;
    bool ok = true;
    for (size_t i = 0; i < sizeof(SECTIONS) / sizeof(SECTIONS[0]) && ok; i++) {
        const cJSON *section = cJSON_GetObjectItemCaseSensitive(root, SECTIONS[i].document_key);
        if (!section_is_empty(section)) {
            ok = SECTIONS[i].render(&sb, root, section);
        }
    }

    if (!ok) {
        free(sb.data);
        return NULL;
    }
    return sb.data;
}
