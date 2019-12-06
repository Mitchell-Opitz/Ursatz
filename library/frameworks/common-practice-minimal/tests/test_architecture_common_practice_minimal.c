#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: common-practice-minimal (Framework extension
 * module, physically separate from src/) may depend only on the same
 * canonical L0-L6 interfaces Framework itself is allowed to touch --
 * identity, core, time, pitch, events, containers, structure, context,
 * semantics, relationship, interpretation -- plus theory/framework.h,
 * rules/rule.h, and rules/constraint.h specifically, the three L7 types
 * this extension is explicitly built on top of. It MUST NOT depend on
 * music-analysis, music-generation, music-transform, or music-corpus,
 * and MUST NOT depend on any other theory/ or rules/ header
 * (Heuristic/Tendency/StatisticalModel/StyleModel, Preference,
 * RuleEvaluation are not needed by this extension; constraint_evaluate
 * itself is never called here -- see chord_vocabulary.c).
 *
 * Scans every .h/.c file directly under frameworks/common-practice-minimal/
 * (not its tests/ subdirectory) and flags any local include ("...")
 * that does not point into an allowed prefix. Directory-driven rather
 * than a fixed file list, same pattern as
 * tests/rules/test_architecture_rules.c.
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

        if (strcmp(start, "framework.h") == 0 || strcmp(start, "theory/framework.h") == 0 ||
            strcmp(start, "rules/rule.h") == 0 || strcmp(start, "rules/constraint.h") == 0 ||
            strcmp(start, "chord_vocabulary.h") == 0 || strcmp(start, "key_vocabulary.h") == 0 ||
            strcmp(start, "triad_identification.h") == 0 || strcmp(start, "key_identification.h") == 0) {
            continue;
        }

        if (strncmp(start, "identity/", 9) != 0 && strncmp(start, "core/", 5) != 0 &&
            strncmp(start, "time/", 5) != 0 && strncmp(start, "pitch/", 6) != 0 &&
            strncmp(start, "events/", 7) != 0 && strncmp(start, "containers/", 11) != 0 &&
            strncmp(start, "structure/", 10) != 0 && strncmp(start, "context/", 8) != 0 &&
            strncmp(start, "semantics/", 10) != 0 && strncmp(start, "relationship/", 13) != 0 &&
            strncmp(start, "interpretation/", 15) != 0) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside identity/, core/, time/, pitch/, events/, "
                    "containers/, structure/, context/, semantics/, relationship/, interpretation/, "
                    "theory/framework.h, rules/rule.h, or rules/constraint.h)\n",
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
    const char *module_dir = "..";
    DIR *d = opendir(module_dir);
    if (d == NULL) {
        printf("common-practice-minimal: directory not found, nothing to check\n");
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
        snprintf(path, sizeof(path), "%s/%s", module_dir, entry->d_name);
        total_violations += check_file(path);
        files_checked++;
    }

    closedir(d);

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in common-practice-minimal\n", total_violations);
        return 1;
    }

    printf("common-practice-minimal: no forward/outside dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}