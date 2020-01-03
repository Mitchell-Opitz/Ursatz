#include <ursatz_analyzer/report_writer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_last_path_separator(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (slash && backslash) {
        return slash > backslash ? slash : backslash;
    }
    return slash ? slash : backslash;
}

char *derive_report_path(const char *input_path) {
    size_t input_len = strlen(input_path);

    const char *last_segment_start = find_last_path_separator(input_path);
    last_segment_start = last_segment_start ? last_segment_start + 1 : input_path;

    const char *dot = strrchr(last_segment_start, '.');
    size_t stem_len = dot ? (size_t)(dot - input_path) : input_len;

    static const char SUFFIX[] = ".report.md";
    size_t total_len = stem_len + sizeof(SUFFIX) - 1;

    char *result = malloc(total_len + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, input_path, stem_len);
    memcpy(result + stem_len, SUFFIX, sizeof(SUFFIX));
    return result;
}

bool write_report_file(const char *path, const char *markdown) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        fprintf(stderr, "Error: cannot write report to '%s'\n", path);
        return false;
    }

    size_t length = strlen(markdown);
    size_t written = fwrite(markdown, 1, length, file);
    fclose(file);

    if (written != length) {
        fprintf(stderr, "Error: failed to write report to '%s'\n", path);
        return false;
    }
    return true;
}
