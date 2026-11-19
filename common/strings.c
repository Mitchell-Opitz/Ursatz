#include "common/strings.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

static void *(*g_realloc)(void *, size_t) = NULL;

void common_text_set_realloc(void *(*fn)(void *, size_t)) {
    g_realloc = fn;
}

void common_text_init(CommonText *text) {
    text->data = NULL;
    text->length = 0;
    text->capacity = 0;
    text->failed = false;
}

static bool text_reserve(CommonText *text, size_t extra) {
    if (text->failed) {
        return false;
    }
    if (text->length + extra + 1 <= text->capacity) {
        return true;
    }
    size_t capacity = text->capacity ? text->capacity : 256;
    while (capacity < text->length + extra + 1) {
        capacity *= 2;
    }
    char *grown = g_realloc ? g_realloc(text->data, capacity) : realloc(text->data, capacity);
    if (!grown) {
        text->failed = true;
        return false;
    }
    text->data = grown;
    text->capacity = capacity;
    return true;
}

bool common_text_append(CommonText *text, const char *value) {
    size_t size = strlen(value);
    if (!text_reserve(text, size)) {
        return false;
    }
    memcpy(text->data + text->length, value, size + 1);
    text->length += size;
    return true;
}

bool common_text_appendf(CommonText *text, const char *format, ...) {
    if (text->failed) {
        return false;
    }
    va_list args;
    va_start(args, format);
    va_list again;
    va_copy(again, args);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (size < 0 || !text_reserve(text, (size_t)size)) {
        text->failed = true;
        va_end(again);
        return false;
    }
    vsnprintf(text->data + text->length, (size_t)size + 1, format, again);
    va_end(again);
    text->length += (size_t)size;
    return true;
}

bool common_text_append_json_string(CommonText *text, const char *value) {
    if (text->failed) {
        return false;
    }
    cJSON *string = cJSON_CreateString(value);
    char *printed = string ? cJSON_PrintUnformatted(string) : NULL;
    cJSON_Delete(string);
    if (!printed) {
        text->failed = true;
        return false;
    }
    bool ok = common_text_append(text, printed);
    free(printed);
    return ok;
}

char *common_text_take(CommonText *text) {
    if (text->failed) {
        return NULL;
    }
    if (!text->data && !text_reserve(text, 0)) {
        return NULL;
    }
    if (!text->data) {
        return NULL;
    }
    text->data[text->length] = '\0';
    char *data = text->data;
    text->data = NULL;
    text->length = 0;
    text->capacity = 0;
    return data;
}

void common_text_free(CommonText *text) {
    free(text->data);
    common_text_init(text);
}

bool common_names_add(CommonNames *names, const char *name) {
    if (names->count == names->capacity) {
        size_t capacity = names->capacity ? names->capacity * 2 : 8;
        char **grown = realloc(names->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        names->items = grown;
        names->capacity = capacity;
    }
    char *copy = common_strdup(name);
    if (!copy) {
        return false;
    }
    names->items[names->count++] = copy;
    return true;
}

bool common_names_contains(const CommonNames *names, const char *name) {
    for (size_t i = 0; i < names->count; i++) {
        if (strcmp(names->items[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static int compare_names(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

void common_names_sort(CommonNames *names) {
    if (names->count > 1) {
        qsort(names->items, names->count, sizeof(*names->items), compare_names);
    }
}

void common_names_free(CommonNames *names) {
    for (size_t i = 0; i < names->count; i++) {
        free(names->items[i]);
    }
    free(names->items);
    names->items = NULL;
    names->count = 0;
    names->capacity = 0;
}

char *common_strdup(const char *value) {
    if (!value) {
        return NULL;
    }
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, value, size);
    }
    return copy;
}

size_t common_trim(char *text) {
    size_t start = 0;
    while (text[start] && isspace((unsigned char)text[start])) {
        start++;
    }
    size_t end = strlen(text);
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end--;
    }
    memmove(text, text + start, end - start);
    text[end - start] = '\0';
    return end - start;
}

void common_copy_text(char *out, size_t size, const char *text) {
    if (out && size > 0) {
        snprintf(out, size, "%s", text);
    }
}

int common_compare_ci(const char *left, const char *right) {
    while (*left && *right) {
        int a = tolower((unsigned char)*left);
        int b = tolower((unsigned char)*right);
        if (a != b) {
            return a < b ? -1 : 1;
        }
        left++;
        right++;
    }
    return (unsigned char)*left == (unsigned char)*right ? 0 : (*left ? 1 : -1);
}

void common_utc_now(char out[32]) {
    time_t now = time(NULL);
    struct tm parts;
    gmtime_r(&now, &parts);
    strftime(out, 32, "%Y-%m-%dT%H:%M:%SZ", &parts);
}
