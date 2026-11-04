#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: the analysis chain (an extension above the numbered
 * stack, physically separate from src/) may include kernel L0 to L8 and the
 * framework headers named in FRAMEWORK_HEADERS below. It MUST NOT include
 * L9 (io/, export/, persistence/, corpus/, corpus_stats/, query/). Framework
 * headers are an exact allowlist of file names, not a directory prefix.
 *
 * Scans every .h/.c file under chain/ursatz_chain/ and flags any local
 * include ("...") that is not a kernel L0-L8 prefix, a chain header, or a
 * listed framework header. Directory-driven, so it needs editing only when
 * a new framework header is first included.
 */

static const char *KERNEL_PREFIXES[] = {
    "identity/",    "core/",     "time/",           "pitch/",     "events/", "containers/",
    "structure/",   "context/",  "semantics/",      "relationship/", "interpretation/",
    "theory/",      "rules/",    "analysis/",       "transform/", "generation/",
};

static const char *FRAMEWORK_HEADERS[] = {
    "common-practice-minimal/triad_identification.h",
    "common-practice-minimal/seventh_identification.h",
    "common-practice-minimal/key_identification.h",
    "standard-form/analysis/analysis_motive.h",
    "standard-form/analysis/analysis_phrase.h",
    "common-practice-minimal/key_regions.h",
    "common-practice-minimal/weighted_chord_identification.h",
    "standard-meter/metric_position_rules.h",
    "standard-meter/segmentation_beat_grid.h",
    "standard-harmony/analysis/analysis_cadence.h",
    "standard-tonality/analysis/chord_input.h",
    "standard-tonality/analysis/roman_numeral_adapter.h",
    "standard-tonality/seventh_rules.h",
    "standard-tonality/triad_rules.h",
    "standard-tonality/tonal_key.h",
    "standard-harmony/harmony_key_change.h",
    "standard-harmony/harmony_cadence_rules.h",
    "common-practice-minimal/outer_scale_degrees.h",
    "standard-form/analysis/analysis_expansion.h",
    "standard-counterpoint/analysis/analysis_grid.h",
    "standard-form/analysis/analysis_ideas.h",
    "standard-counterpoint/analysis/analysis_melody_climax.h",
    "standard-counterpoint/analysis/analysis_melody_leaps.h",
    "standard-counterpoint/analysis/analysis_melody_nct.h",
    "standard-counterpoint/analysis/analysis_motion.h",
    "standard-counterpoint/analysis/analysis_perfects.h",
    "standard-harmony/analysis/analysis_resolution.h",
    "standard-form/analysis/analysis_theme.h",
    "galant-schemas/analysis/galant_analysis_match.h",
    "standard-meter/analysis/meter_analysis_density.h",
    "standard-meter/analysis/meter_analysis_hemiola.h",
    "standard-meter/analysis/meter_analysis_hypermeter.h",
    "standard-meter/analysis/meter_analysis_syncopation.h",
    "scale-harmonization/analysis/scale_analysis_detect.h",
    "standard-texture/analysis/texture_analysis_classify.h",
    "standard-counterpoint/analysis/analysis_melody.h",
    "galant-schemas/analysis/galant_analysis_input.h",
    "standard-form/analysis/analysis_time.h",
    "standard-form/analysis/analysis_input.h",
    "standard-form/analysis/analysis_motive_shape.h",
    "standard-meter/analysis/meter_analysis_input.h",
    "standard-texture/analysis/texture_analysis_input.h",
};

static int has_prefix(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int is_allowed(const char *include_path) {
    for (size_t i = 0; i < sizeof(KERNEL_PREFIXES) / sizeof(KERNEL_PREFIXES[0]); i++) {
        if (has_prefix(include_path, KERNEL_PREFIXES[i])) {
            return 1;
        }
    }
    if (has_prefix(include_path, "ursatz_chain/")) {
        return 1;
    }
    for (size_t i = 0; i < sizeof(FRAMEWORK_HEADERS) / sizeof(FRAMEWORK_HEADERS[0]); i++) {
        if (strcmp(include_path, FRAMEWORK_HEADERS[i]) == 0) {
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

        if (!is_allowed(start)) {
            fprintf(stderr,
                    "violation: %s includes \"%s\" (outside kernel L0-L8, ursatz_chain/, or the "
                    "framework header allowlist; L9 is forbidden)\n",
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
    const char *module_dir = "../ursatz_chain";
    DIR *d = opendir(module_dir);
    if (d == NULL) {
        printf("chain: directory not found, nothing to check\n");
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
        fprintf(stderr, "%d architecture violation(s) found in chain\n", total_violations);
        return 1;
    }

    printf("chain: no forbidden dependencies found (%d file(s) checked)\n", files_checked);
    return 0;
}
