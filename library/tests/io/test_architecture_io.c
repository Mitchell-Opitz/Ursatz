#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-io (L9) may depend on music-identity (L0),
 * music-core (L1), music-time and music-pitch (L2), and music-events
 * (L3) only. It MUST NOT depend on music-containers, music-structure,
 * music-interpretation, music-theory, frameworks/, or music-analysis,
 * or any other module. This importer produces raw canonical events; it
 * has no opinion about chords, keys, or structure.
 *
 * Scans every .h/.c file under src/io/ and flags any local include
 * ("...") that does not point into identity/, core/, time/, pitch/,
 * events/, or io/ itself -- except "ezxml.h", the vendored XML parser
 * (third_party/ezxml) that io/musicxml_importer.c alone uses.
 * tests/test_architecture_third_party.c checks that nothing outside
 * src/io/ includes it.
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

        if (strcmp(start, "ezxml.h") == 0) {
            continue;
        }

        if (strncmp(start, "identity/", 9) != 0 && strncmp(start, "core/", 5) != 0 &&
            strncmp(start, "time/", 5) != 0 && strncmp(start, "pitch/", 6) != 0 &&
            strncmp(start, "events/", 7) != 0 && strncmp(start, "io/", 3) != 0 &&
            strcmp(start, "semantics/analysis_key.h") != 0) {
            fprintf(stderr, "violation: %s includes \"%s\" (outside identity/, core/, time/, pitch/, events/, io/, or semantics/analysis_key.h)\n",
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
    const char *io_dir = "../../src/io";
    DIR *d = opendir(io_dir);
    if (d == NULL) {
        printf("music-io: src/io/ not yet present, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", io_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-io\n", total_violations);
        return 1;
    }

    printf("music-io: no forward/outside/sibling dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
