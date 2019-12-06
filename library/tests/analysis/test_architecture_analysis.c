#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-analysis (L8) may depend on music-identity
 * (L0), music-core (L1), music-time, music-pitch (L2), music-events
 * (L3), music-containers (L4), music-structure (L5), music-context,
 * music-semantics, music-relationship, music-interpretation (L6), and
 * music-theory, music-rules (L7) only. It MUST NOT depend on
 * music-corpus/music-io/music-notation/music-performance (L9) or its
 * L8 siblings music-transform/music-generation.
 * Dependency & Layer Spec v2 §2, §3.
 *
 * ChordIdentification and KeyEstimation are also legitimately compiled
 * into frameworks/common-practice-minimal/ (see the comment in
 * src/CMakeLists.txt on why -- a circular library dependency otherwise)
 * and so include that module's own flat, prefix-less headers directly
 * (e.g. "triad_identification.h", not "frameworks/.../triad_
 * identification.h"). COMMON_PRACTICE_MINIMAL_HEADERS below is an exact
 * allowlist of those specific filenames -- not a directory prefix --
 * so this does not open the door to depending on any other
 * framework-shaped module beyond what's explicitly named here.
 *
 * Scans every .h/.c file under src/analysis/ and flags any local
 * include ("...") that does not point into identity/, core/, time/,
 * pitch/, events/, containers/, structure/, context/, semantics/,
 * relationship/, interpretation/, theory/, rules/, or analysis/
 * itself, and is not one of COMMON_PRACTICE_MINIMAL_HEADERS. The scan
 * is directory-driven rather than a fixed file list, so this test does
 * not need editing as analysis/ grows.
 */

static const char *COMMON_PRACTICE_MINIMAL_HEADERS[] = {
    "chord_vocabulary.h",
    "key_vocabulary.h",
    "triad_identification.h",
    "key_identification.h",
};
#define COMMON_PRACTICE_MINIMAL_HEADER_COUNT \
    (sizeof(COMMON_PRACTICE_MINIMAL_HEADERS) / sizeof(COMMON_PRACTICE_MINIMAL_HEADERS[0]))

static int is_common_practice_minimal_header(const char *include_path) {
    for (size_t i = 0; i < COMMON_PRACTICE_MINIMAL_HEADER_COUNT; i++) {
        if (strcmp(include_path, COMMON_PRACTICE_MINIMAL_HEADERS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int check_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open %s\n", path);
        return 1;
    }

    char line[1024];
    int violations = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        const char *marker = "#include \"";
        char *start = strstr(line, marker);
        if (start == NULL) {
            continue;
        }
        start += strlen(marker);

        char *end = strchr(start, '"');
        if (end == NULL) {
            continue;
        }
        *end = '\0';

        if (strncmp(start, "identity/", 9) != 0 && strncmp(start, "core/", 5) != 0 &&
            strncmp(start, "time/", 5) != 0 && strncmp(start, "pitch/", 6) != 0 &&
            strncmp(start, "events/", 7) != 0 && strncmp(start, "containers/", 11) != 0 &&
            strncmp(start, "structure/", 10) != 0 && strncmp(start, "context/", 8) != 0 &&
            strncmp(start, "semantics/", 10) != 0 && strncmp(start, "relationship/", 13) != 0 &&
            strncmp(start, "interpretation/", 15) != 0 && strncmp(start, "theory/", 7) != 0 &&
            strncmp(start, "rules/", 6) != 0 && strncmp(start, "analysis/", 9) != 0 &&
            !is_common_practice_minimal_header(start)) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, core/, time/, pitch/, events/, "
                    "containers/, structure/, context/, semantics/, relationship/, interpretation/, "
                    "theory/, rules/, analysis/, or the common-practice-minimal header allowlist)\n",
                    path, start);
            violations++;
        }
    }

    fclose(f);
    return violations;
}

static int has_suffix(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > name_len) {
        return 0;
    }
    return strcmp(name + (name_len - suffix_len), suffix) == 0;
}

int main(void) {
    const char *analysis_dir = "../../src/analysis";
    DIR *d = opendir(analysis_dir);
    if (d == NULL) {
        printf("music-analysis: src/analysis/ not yet present, nothing to check\n");
        return 0;
    }

    int total_violations = 0;
    int files_checked = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (!has_suffix(entry->d_name, ".h") && !has_suffix(entry->d_name, ".c")) {
            continue;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", analysis_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-analysis\n", total_violations);
        return 1;
    }

    printf("music-analysis: no forward/outside dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}