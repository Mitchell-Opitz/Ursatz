#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-query (L9, a sibling of music-corpus-stats) may
 * depend on music-identity (L0), music-core (for ProvenanceReference), music-
 * semantics (for ContourDirection), music-persistence (L9, for
 * PersistenceHandle and its context queries) and music-corpus (for
 * CorpusStoreHandle and its piece-ref listing) only. It MUST NOT depend on
 * music-time, music-pitch, music-events, music-containers, music-structure,
 * music-context, music-interpretation, music-relationship, music-theory,
 * music-rules, music-transform, music-analysis, music-generation, music-io,
 * music-corpus-stats, or frameworks/.
 *
 * In particular it does not import music-analysis: a search can only return
 * what analysis has already persisted, never run analysis itself.
 *
 * Scans every .h/.c file under src/query/ and flags any local include ("...")
 * that does not point into identity/, core/, semantics/, persistence/,
 * corpus/, or query/ itself.
 *
 * Also scans every .h/.c file under src/ outside src/query/ and flags any
 * local include of a query/ internal header -- the similarity model, scoring,
 * search result and corpus search headers are the public surface of
 * music-query; query_fraction.h is internal.
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
            strncmp(start, "semantics/", 10) != 0 && strncmp(start, "persistence/", 12) != 0 &&
            strncmp(start, "corpus/", 7) != 0 && strncmp(start, "query/", 6) != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, core/, semantics/, persistence/, corpus/, or "
                    "query/)\n",
                    path, start);
            violations++;
        }
    }

    fclose(f);
    return violations;
}

static int check_file_external(const char *path) {
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

        if (strncmp(start, "query/", 6) == 0 && strcmp(start, "query/similarity_model.h") != 0 &&
            strcmp(start, "query/similarity_scoring.h") != 0 && strcmp(start, "query/search_result.h") != 0 &&
            strcmp(start, "query/corpus_search.h") != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (only the similarity model, scoring, search result and corpus "
                    "search headers may be included from outside query/)\n",
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

static int scan_external_dir(const char *dir_path, int *files_checked) {
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        return 0;
    }

    int violations = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "query") == 0) {
            continue;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        if (has_suffix(entry->d_name, ".h") || has_suffix(entry->d_name, ".c")) {
            violations += check_file_external(path);
            (*files_checked)++;
            continue;
        }

        DIR *subdir = opendir(path);
        if (subdir != NULL) {
            closedir(subdir);
            violations += scan_external_dir(path, files_checked);
        }
    }

    closedir(d);
    return violations;
}

int main(void) {
    const char *query_dir = "../../src/query";
    DIR *d = opendir(query_dir);
    if (d == NULL) {
        printf("music-query: src/query/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", query_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    const char *src_dir = "../../src";
    int external_files_checked = 0;
    total_violations += scan_external_dir(src_dir, &external_files_checked);
    files_checked += external_files_checked;

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-query\n", total_violations);
        return 1;
    }

    printf("music-query: no forward/outside/sibling dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
