#include <ursatz_analyzer/markdown_report.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool sb_appendf(StringBuilder *sb, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return sb_append(sb, buffer);
}

char *markdown_report_render(const char *source_filename, const AnalysisReport *report) {
    StringBuilder sb;
    if (!sb_init(&sb)) {
        return NULL;
    }

    bool ok = true;
    ok = ok && sb_appendf(&sb, "# Analysis Report: %s\n\n", source_filename);

    if (report->key_found && report->entry_count > 0) {
        ok = ok && sb_appendf(&sb, "**Detected key:** %s\n\n", report->entries[0].key_label);
    } else {
        ok = ok && sb_append(&sb, "**Detected key:** No key detected\n\n");
    }

    ok = ok && sb_append(&sb, "## Chord Regions\n\n");
    ok = ok && sb_append(&sb, "Times are beats from the start of the file.\n\n");

    if (report->entry_count == 0) {
        ok = ok && sb_append(&sb, "No chord regions were identified.\n");
    } else {
        for (size_t i = 0; i < report->entry_count && ok; i++) {
            const AnalysisReportEntry *entry = &report->entries[i];
            double beats = (double)(2 * entry->time_numerator) / (double)entry->time_denominator;
            ok = sb_appendf(&sb, "- Beat %.2f: %s\n", beats, entry->chord_label);
        }
    }

    if (!ok) {
        free(sb.data);
        return NULL;
    }
    return sb.data;
}
