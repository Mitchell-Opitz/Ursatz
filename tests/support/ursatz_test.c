#include "ursatz_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_failure_count = 0;

bool test_check(bool ok, const char *file, int line, const char *text) {
    if (!ok) {
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, text);
        test_failure_count++;
    }
    return ok;
}

bool test_checkf(bool ok, const char *file, int line, const char *format, ...) {
    if (!ok) {
        fprintf(stderr, "FAIL %s:%d: ", file, line);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputc('\n', stderr);
        test_failure_count++;
    }
    return ok;
}

void test_pass(const char *what) {
    printf("PASS: %s\n", what);
}

int test_finish(const char *name) {
    if (test_failure_count == 0) {
        printf("%s: ok\n", name);
        return 0;
    }
    printf("%s: %d failure(s)\n", name, test_failure_count);
    return 1;
}

bool test_update_goldens(void) {
    const char *value = getenv("URSATZ_UPDATE_GOLDENS");
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

int test_goldens_updated(const char *name) {
    if (test_failure_count != 0) {
        return test_finish(name);
    }
    printf("%s: goldens updated (URSATZ_UPDATE_GOLDENS)\n", name);
    return 2;
}

char *test_read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *data = malloc((size_t)length + 1);
    if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(file);
    if (data) {
        data[length] = '\0';
        if (size) {
            *size = (size_t)length;
        }
    }
    return data;
}

char *test_must_read_file(const char *path, size_t *size) {
    char *data = test_read_file(path, size);
    if (!data) {
        fprintf(stderr, "cannot read %s\n", path);
        abort();
    }
    return data;
}

bool test_write_file(const char *path, const char *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    bool ok = size == 0 || fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

static void smf_append(unsigned char *buf, size_t *pos, const void *bytes, size_t length) {
    memcpy(buf + *pos, bytes, length);
    *pos += length;
}

static void smf_append_u32(unsigned char *buf, size_t *pos, uint32_t value) {
    const unsigned char bytes[4] = {(unsigned char)(value >> 24), (unsigned char)(value >> 16), (unsigned char)(value >> 8),
                                    (unsigned char)value};
    smf_append(buf, pos, bytes, 4);
}

static void smf_append_u16(unsigned char *buf, size_t *pos, uint16_t value) {
    const unsigned char bytes[2] = {(unsigned char)(value >> 8), (unsigned char)value};
    smf_append(buf, pos, bytes, 2);
}

size_t test_smf(unsigned char *buf, uint16_t format, uint16_t division, const unsigned char *const *tracks,
                const size_t *sizes, uint16_t count) {
    size_t pos = 0;
    smf_append(buf, &pos, "MThd", 4);
    smf_append_u32(buf, &pos, 6);
    smf_append_u16(buf, &pos, format);
    smf_append_u16(buf, &pos, count);
    smf_append_u16(buf, &pos, division);
    for (uint16_t t = 0; t < count; t++) {
        smf_append(buf, &pos, "MTrk", 4);
        smf_append_u32(buf, &pos, (uint32_t)sizes[t]);
        smf_append(buf, &pos, tracks[t], sizes[t]);
    }
    return pos;
}

size_t test_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body, size_t track_body_size) {
    const unsigned char *tracks[1] = {track_body};
    const size_t sizes[1] = {track_body_size};
    return test_smf(buf, 0, division, tracks, sizes, 1);
}

bool test_file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

char *test_temp_path(const char *name) {
    const char *dir = getenv("TMPDIR");
    if (dir == NULL || dir[0] == '\0') {
        dir = "/tmp";
    }
    size_t size = strlen(dir) + strlen(name) + 48;
    char *path = malloc(size);
    if (path) {
        snprintf(path, size, "%s/ursatz_test_%ld_%s", dir, (long)getpid(), name);
    }
    return path;
}

cJSON *test_read_json(const char *path) {
    char *text = test_read_file(path, NULL);
    if (!text) {
        return NULL;
    }
    cJSON *json = cJSON_Parse(text);
    free(text);
    return json;
}

const cJSON *test_json_member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

double test_json_number(const cJSON *object, const char *name, double fallback) {
    const cJSON *item = test_json_member(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

const char *test_json_string(const cJSON *object, const char *name) {
    const cJSON *item = test_json_member(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static void append_where(char *where, size_t where_size, const char *text) {
    size_t used = strlen(where);
    if (used + 1 < where_size) {
        snprintf(where + used, where_size - used, "%s", text);
    }
}

/* Finds the first difference under expected and actual, appending its path to where. True when they are equal. */
static bool compare(const cJSON *expected, const cJSON *actual, char *where, size_t where_size) {
    if (cJSON_Compare(expected, actual, true)) {
        return true;
    }
    if (cJSON_IsObject(expected) && cJSON_IsObject(actual)) {
        const cJSON *member = NULL;
        cJSON_ArrayForEach(member, expected) {
            const cJSON *other = cJSON_GetObjectItemCaseSensitive(actual, member->string);
            size_t mark = strlen(where);
            append_where(where, where_size, where[0] ? "." : "");
            append_where(where, where_size, member->string);
            if (!other || !compare(member, other, where, where_size)) {
                return false;
            }
            where[mark] = '\0';
        }
        const cJSON *extra = NULL;
        cJSON_ArrayForEach(extra, actual) {
            if (!cJSON_GetObjectItemCaseSensitive(expected, extra->string)) {
                append_where(where, where_size, where[0] ? "." : "");
                append_where(where, where_size, extra->string);
                return false;
            }
        }
        return false;
    }
    if (cJSON_IsArray(expected) && cJSON_IsArray(actual)) {
        int count = cJSON_GetArraySize(expected);
        int other_count = cJSON_GetArraySize(actual);
        for (int i = 0; i < count && i < other_count; i++) {
            size_t mark = strlen(where);
            char index[24];
            snprintf(index, sizeof index, "[%d]", i);
            append_where(where, where_size, index);
            if (!compare(cJSON_GetArrayItem(expected, i), cJSON_GetArrayItem(actual, i), where, where_size)) {
                return false;
            }
            where[mark] = '\0';
        }
        char index[24];
        snprintf(index, sizeof index, "[%d]", count < other_count ? count : other_count);
        append_where(where, where_size, index);
        return false;
    }
    return false;
}

bool test_json_equal(const cJSON *expected, const cJSON *actual, char *where, size_t where_size) {
    if (!expected || !actual) {
        return expected == actual;
    }
    if (where_size == 0) {
        return cJSON_Compare(expected, actual, true);
    }
    where[0] = '\0';
    return compare(expected, actual, where, where_size);
}
