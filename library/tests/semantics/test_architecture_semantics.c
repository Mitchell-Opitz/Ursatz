#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-semantics (L6) may depend on music-identity
 * (L0), music-core (L1), music-time, music-pitch (L2), music-events
 * (L3), music-containers (L4), and music-structure (L5) only. It MUST
 * NOT depend on music-theory or music-rules (L7) or anything above --
 * and, per the cross-cutting L6 requirement, MUST NOT depend sideways
 * on the other L6 modules (music-context, music-relationship,
 * music-interpretation) either. Dependency & Layer Spec v2 §2, §3.
 *
 * Scans every .h/.c file under src/semantics/ and flags any local
 * include ("...") that does not point into identity/, core/, time/,
 * pitch/, events/, containers/, structure/, or semantics/ itself. The
 * scan is directory-driven rather than a fixed file list, so this test
 * does not need editing as semantics/ grows.
 */

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
            strncmp(start, "structure/", 10) != 0 && strncmp(start, "semantics/", 10) != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, core/, time/, pitch/, events/, "
                    "containers/, structure/, or semantics/)\n",
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
    const char *semantics_dir = "../../src/semantics";
    DIR *d = opendir(semantics_dir);
    if (d == NULL) {
        printf("music-semantics: src/semantics/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", semantics_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-semantics\n", total_violations);
        return 1;
    }

    printf("music-semantics: no forward/outside dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}