#include <ursatz_analyzer/report_writer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/path_util.h"

char *derive_report_path(const char *input_path) {
    return path_with_suffix(input_path, ".report.md");
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
