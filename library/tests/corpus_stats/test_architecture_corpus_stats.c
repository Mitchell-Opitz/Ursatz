#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-corpus-stats (sibling to music-corpus) may
 * depend on music-identity (L0), music-persistence (L9, for
 * PersistenceHandle/Chord persistence), music-interpretation (for
 * Chord/chord_label), and music-corpus (for CorpusStoreHandle and its
 * piece-ref listing) only. It MUST NOT depend on music-core, music-
 * time, music-pitch, music-events, music-containers, music-structure,
 * music-context, music-semantics, music-relationship, music-theory,
 * music-rules, music-transform, music-analysis, music-generation,
 * music-io, or frameworks/.
 *
 * Note this module depends on interpretation/ specifically so that
 * corpus/ itself can stay dependency-pure (identity + persistence
 * only, per docs/modules/corpus.md) -- Corpus Statistics is a distinct
 * Registry Concept from Corpus's own membership operations, kept as
 * its own module rather than widening corpus/'s allowed dependencies.
 *
 * Scans every .h/.c file under src/corpus_stats/ and flags any local
 * include ("...") that does not point into identity/, persistence/,
 * interpretation/, corpus/, or corpus_stats/ itself.
 *
 * Also scans every .h/.c file under src/ outside src/corpus_stats/ and
 * flags any local include of a corpus_stats/ internal header --
 * corpus_stats_api.h is the only public surface of music-corpus-stats.
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

        if (strncmp(start, "identity/", 9) != 0 && strncmp(start, "persistence/", 12) != 0 &&
            strncmp(start, "interpretation/", 15) != 0 && strncmp(start, "corpus/", 7) != 0 &&
            strncmp(start, "corpus_stats/", 13) != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, persistence/, interpretation/, corpus/, or "
                    "corpus_stats/)\n",
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

        if (strncmp(start, "corpus_stats/", 13) == 0 && strcmp(start, "corpus_stats/corpus_stats_api.h") != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (only corpus_stats/corpus_stats_api.h may be included from "
                    "outside corpus_stats/)\n",
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
            strcmp(entry->d_name, "corpus_stats") == 0) {
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
    const char *corpus_stats_dir = "../../src/corpus_stats";
    DIR *d = opendir(corpus_stats_dir);
    if (d == NULL) {
        printf("music-corpus-stats: src/corpus_stats/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", corpus_stats_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    const char *src_dir = "../../src";
    int external_files_checked = 0;
    total_violations += scan_external_dir(src_dir, &external_files_checked);
    files_checked += external_files_checked;

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-corpus-stats\n", total_violations);
        return 1;
    }

    printf("music-corpus-stats: no forward/outside/sibling dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
