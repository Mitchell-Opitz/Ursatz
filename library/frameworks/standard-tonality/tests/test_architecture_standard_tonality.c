#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Architecture test: standard-tonality (Framework extension
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
 * itself is never called here -- see rule_pair.c).
 *
 * Scans every .h/.c file directly under frameworks/standard-tonality/ and under its analysis/
 * subdirectory (the L3 adapter; not its tests/ subdirectory) and flags any local include ("...")
 * that does not point into an allowed prefix. This also keeps the module
 * independent of the other framework modules: standard-notation's
 * headers are not on the allowed list. Directory-driven rather
 * than a fixed file list, same pattern as
 * tests/rules/test_architecture_rules.c.
 */

/* True when a bare include names a header that lives in this module (its top directory or analysis/). */
static int is_module_header(const char *name) {
    if (strchr(name, '/') != NULL) {
        return 0;
    }
    const char *dirs[] = {"..", "../analysis"};
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char path[512];
        struct stat info;
        snprintf(path, sizeof(path), "%s/%s", dirs[i], name);
        if (stat(path, &info) == 0) {
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

        if (strncmp(start, "standard-tonality/", 18) == 0) {
            continue; /* same-framework header, by its public path */
        }

        if (is_module_header(start)) {
            continue;
        }

        if (strcmp(start, "framework.h") == 0 || strcmp(start, "theory/framework.h") == 0 ||
            strcmp(start, "theory/framework_singleton.h") == 0 ||
            strcmp(start, "rules/rule.h") == 0 || strcmp(start, "rules/constraint.h") == 0 ||
            strcmp(start, "rules/rule_pair.h") == 0 ||
            strcmp(start, "rule_pair.h") == 0 || strcmp(start, "tonal_pitch.h") == 0 ||
            strcmp(start, "major_scale_rules.h") == 0 || strcmp(start, "scale_degree_rules.h") == 0 ||
            strcmp(start, "key_signature_rules.h") == 0 || strcmp(start, "circle_of_fifths_rules.h") == 0 ||
            strcmp(start, "scale_pattern.h") == 0 || strcmp(start, "minor_scale_rules.h") == 0 ||
            strcmp(start, "minor_scale_degree_rules.h") == 0 || strcmp(start, "key_relationship_rules.h") == 0 ||
            strcmp(start, "minor_key_rules.h") == 0 || strcmp(start, "solfege_rules.h") == 0 ||
            strcmp(start, "mode_rules.h") == 0 || strcmp(start, "modal_solfege_rules.h") == 0 ||
            strcmp(start, "pentatonic_rules.h") == 0 || strcmp(start, "chromatic_scale_rules.h") == 0 ||
            strcmp(start, "interval_size_rules.h") == 0 || strcmp(start, "interval_quality_rules.h") == 0 ||
            strcmp(start, "interval_inversion_rules.h") == 0 || strcmp(start, "interval_consonance_rules.h") == 0 ||
            strcmp(start, "interval_white_key_rules.h") == 0 || strcmp(start, "interval_enharmonic_rules.h") == 0 ||
            strcmp(start, "interval_spelling_rules.h") == 0 || strcmp(start, "triad_rules.h") == 0 ||
            strcmp(start, "triad_spelling_rules.h") == 0 || strcmp(start, "triad_identification_rules.h") == 0 ||
            strcmp(start, "chord_symbol_rules.h") == 0 || strcmp(start, "triad_in_key_rules.h") == 0 ||
            strcmp(start, "seventh_rules.h") == 0 || strcmp(start, "seventh_spelling_rules.h") == 0 ||
            strcmp(start, "seventh_identification_rules.h") == 0 || strcmp(start, "seventh_in_key_rules.h") == 0 ||
            strcmp(start, "roman_numeral_rules.h") == 0 || strcmp(start, "roman_numeral_analysis_rules.h") == 0) {
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
                    "theory/framework.h, theory/framework_singleton.h, rules/rule.h, rules/constraint.h, or rules/rule_pair.h)\n",
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
    const char *module_dirs[] = {"..", "../analysis"};
    int total_violations = 0;
    int files_checked = 0;

    for (size_t i = 0; i < sizeof(module_dirs) / sizeof(module_dirs[0]); i++) {
        DIR *d = opendir(module_dirs[i]);
        if (d == NULL) {
            if (i == 0) {
                printf("standard-tonality: directory not found, nothing to check\n");
                return 0;
            }
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (!has_suffix(entry->d_name, ".h") && !has_suffix(entry->d_name, ".c")) {
                continue;
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/%s", module_dirs[i], entry->d_name);
            total_violations += check_file(path);
            files_checked++;
        }
        closedir(d);
    }

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in standard-tonality\n", total_violations);
        return 1;
    }

    printf("standard-tonality: no forward/outside dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
