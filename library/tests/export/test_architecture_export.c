#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-export may depend on music-identity (L0),
 * music-time and music-pitch (L2), music-events (L3), music-persistence
 * (L9), and music-export itself only. It MUST NOT depend on music-core,
 * music-containers, music-structure, music-context, music-semantics,
 * music-relationship, music-theory, music-rules, music-transform,
 * music-analysis, music-generation, music-io, music-corpus,
 * music-corpus-stats, or frameworks/ -- this module reads persisted
 * NoteEvents and their PitchSpelling only, the same dependency shape
 * corpus_stats/ uses toward persistence (see docs/modules/
 * corpus_stats.md), applied here to export instead of corpus.
 *
 * Scans every .h/.c file under src/export/ and flags any local include
 * ("...") that does not point into identity/, time/, pitch/, events/,
 * persistence/, or export/ itself.
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

        if (strncmp(start, "identity/", 9) != 0 && strncmp(start, "time/", 5) != 0 &&
            strncmp(start, "pitch/", 6) != 0 && strncmp(start, "events/", 7) != 0 &&
            strncmp(start, "persistence/", 12) != 0 && strncmp(start, "export/", 7) != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, time/, pitch/, events/, "
                    "persistence/, or export/)\n",
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
    const char *export_dir = "../../src/export";
    DIR *d = opendir(export_dir);
    if (d == NULL) {
        printf("music-export: src/export/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", export_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-export\n", total_violations);
        return 1;
    }

    printf("music-export: no forward/outside/sibling dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
