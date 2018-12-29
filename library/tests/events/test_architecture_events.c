#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-events (L3) may depend on music-identity (L0),
 * music-core (L1), music-time, and music-pitch (L2) only. It MUST NOT
 * depend on music-containers (L4) or anything else L4+. Dependency &
 * Layer Spec v2 §2, §3.
 *
 * The music-containers non-dependency is the main thing this layer's
 * design protects: where a lower object needs to reference Voice (L4),
 * it must use VoiceReference (L1, music-core), never a resolved Voice
 * object or a music-containers import.
 *
 * Scans every .h/.c file under src/events/ and flags any local include
 * ("...") that does not point into identity/, core/, time/, pitch/, or
 * events/ itself. The scan is directory-driven rather than a fixed file
 * list, so this test does not need editing as events/ grows.
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
            strncmp(start, "events/", 7) != 0) {
            fprintf(stderr, "violation: %s includes \"%s\" (outside identity/, core/, time/, pitch/, or events/)\n",
                    path, start);
            violations++;
        }

        if (strncmp(start, "containers/", 11) == 0) {
            fprintf(stderr, "violation: %s includes \"%s\" (music-containers is L4, forbidden from L3)\n", path,
                    start);
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
    const char *events_dir = "../../src/events";
    DIR *d = opendir(events_dir);
    if (d == NULL) {
        printf("music-events: src/events/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", events_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-events\n", total_violations);
        return 1;
    }

    printf("music-events: no forward/outside/containers dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}