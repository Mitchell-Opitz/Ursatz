#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Architecture test for the test sources: tests/<module>/ and frameworks/<name>/tests/ follow the same
 * layer rules as the code they test (docs/layers-and-dependency-rules.md).
 *
 *  - A kernel test under tests/<module>/ may include its own module and modules in lower layers, and
 *    siblings only where the code allows it (the L9 modules, and rules on theory). It never includes a
 *    framework or ursatz_chain header. tests/integration/ and tests/accuracy/ run the whole system and
 *    are not restricted.
 *  - A framework test under frameworks/<name>/tests/ may include L0 to L7 headers, its own framework,
 *    and nothing else.
 *
 * Every edge beyond that is in ALLOWED_TEST_EDGES with a reason; a new edge is added there in the commit
 * that creates it. Local includes without a directory are not checked. Needs SOURCE_DIR, the repo root.
 */

typedef struct {
    const char *module;
    int layer;
} ModuleLayer;

static const ModuleLayer KERNEL_MODULES[] = {
    {"identity", 0},  {"core", 1},          {"time", 2},     {"pitch", 2},       {"events", 3},       {"containers", 4},
    {"structure", 5}, {"context", 6},       {"semantics", 6}, {"relationship", 6}, {"interpretation", 6}, {"theory", 7},
    {"rules", 7},     {"analysis", 8},      {"generation", 8}, {"io", 9},        {"export", 9},       {"persistence", 9},
    {"corpus", 9},    {"corpus_stats", 9},  {"query", 9},
};

static const char *const FRAMEWORKS[] = {
    "baroque-ground-bass", "common-practice-minimal", "galant-schemas", "high-baroque-fugue", "scale-harmonization",
    "sixteenth-century-style", "standard-counterpoint", "standard-form", "standard-harmony", "standard-markings",
    "standard-meter", "standard-notation", "standard-rhythm", "standard-texture", "standard-tonality",
    "standard-voicing", "western-periods",
};

typedef struct {
    const char *from;    /* path prefix of the including file, relative to the repo root */
    const char *include; /* include path prefix */
    const char *reason;
} TestEdge;

static const TestEdge ALLOWED_TEST_EDGES[] = {
    {"tests/analysis/", "common-practice-minimal/", "analysis tests run the chain and the framework adapters end to end"},
    {"tests/analysis/", "standard-meter/", "analysis tests run the chain and the framework adapters end to end"},
    {"tests/analysis/", "standard-tonality/", "analysis tests run the chain and the framework adapters end to end"},
    {"tests/analysis/", "ursatz_chain/", "analysis tests run the chain end to end"},
    {"tests/analysis/", "io/", "analysis tests import the corpus MIDI files"},
    {"tests/persistence/", "common-practice-minimal/", "persistence tests store framework chord and key labels"},
    {"frameworks/common-practice-minimal/tests/", "standard-tonality/", "declared dependency (the scale degree owner)"},
    {"frameworks/common-practice-minimal/tests/", "analysis/outer_voice.h",
     "the test builds kernel outer lines and hands them to the framework in its own input shape"},
    {"frameworks/baroque-ground-bass/tests/", "standard-counterpoint/", "declared dependency"},
    {"frameworks/high-baroque-fugue/tests/", "standard-counterpoint/", "declared dependency"},
    {"frameworks/sixteenth-century-style/tests/", "standard-counterpoint/", "declared dependency"},
    {"frameworks/scale-harmonization/tests/", "galant-schemas/", "declared dependency"},
    {"frameworks/standard-harmony/tests/", "standard-tonality/", "test-only edge: Roman numeral grammar cases"},
    {"frameworks/standard-harmony/tests/", "analysis/period_detection.h",
     "test-only edge: compares the period ranking with the kernel period detection"},
    {"frameworks/standard-form/tests/", "ursatz_chain/", "test-only edge: form_test_util.h builds the chain's form input"},
    {"frameworks/standard-meter/tests/", "io/", "test-only edge: tests import MIDI to feed the meter analysis"},
};

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int ends_with(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    return suffix_len <= name_len && strcmp(name + (name_len - suffix_len), suffix) == 0;
}

static int kernel_layer(const char *module) {
    for (size_t i = 0; i < sizeof(KERNEL_MODULES) / sizeof(KERNEL_MODULES[0]); i++) {
        if (strcmp(KERNEL_MODULES[i].module, module) == 0) {
            return KERNEL_MODULES[i].layer;
        }
    }
    return -1;
}

static int is_framework(const char *name) {
    for (size_t i = 0; i < sizeof(FRAMEWORKS) / sizeof(FRAMEWORKS[0]); i++) {
        if (strcmp(FRAMEWORKS[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int edge_allowed(const char *relative_path, const char *include) {
    for (size_t i = 0; i < sizeof(ALLOWED_TEST_EDGES) / sizeof(ALLOWED_TEST_EDGES[0]); i++) {
        if (starts_with(relative_path, ALLOWED_TEST_EDGES[i].from) && starts_with(include, ALLOWED_TEST_EDGES[i].include)) {
            return 1;
        }
    }
    return 0;
}

/* owner_framework is NULL for a kernel test; owner_module is the tests/<module> name for a kernel test. */
static int violates(const char *owner_framework, const char *owner_module, const char *include) {
    const char *slash = strchr(include, '/');
    if (slash == NULL) {
        return 0; /* local include */
    }
    char top[128];
    size_t length = (size_t)(slash - include);
    if (length >= sizeof(top)) {
        return 0;
    }
    memcpy(top, include, length);
    top[length] = '\0';

    int is_chain = strcmp(top, "ursatz_chain") == 0;
    int top_framework = is_framework(top);
    int top_layer = kernel_layer(top);

    if (owner_framework != NULL) {
        if (top_framework) {
            return strcmp(top, owner_framework) != 0;
        }
        if (is_chain) {
            return 1;
        }
        /* A framework's own subdirectories (for example analysis/) are included by their full path, so a
           bare kernel module name is the kernel's. */
        return top_layer >= 8;
    }

    int owner_layer = kernel_layer(owner_module);
    if (owner_layer < 0) {
        return 0; /* integration and accuracy run the whole system */
    }
    if (top_framework || is_chain) {
        return 1;
    }
    if (top_layer < 0) {
        return 0;
    }
    if (top_layer > owner_layer) {
        return 1;
    }
    if (top_layer == owner_layer && strcmp(top, owner_module) != 0) {
        if (owner_layer == 9) {
            return 0; /* the L9 modules sit side by side and import each other's types */
        }
        if (strcmp(owner_module, "rules") == 0 && strcmp(top, "theory") == 0) {
            return 0; /* documented: rules depends on theory */
        }
        return 1;
    }
    return 0;
}

static int check_file(const char *path, const char *relative_path, const char *owner_framework, const char *owner_module) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open %s\n", path);
        return 1;
    }
    char line[1024];
    int violations = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *start = strstr(line, "#include \"");
        if (start == NULL) {
            continue;
        }
        start += strlen("#include \"");
        char *end = strchr(start, '"');
        if (end == NULL) {
            continue;
        }
        *end = '\0';
        if (violates(owner_framework, owner_module, start) && !edge_allowed(relative_path, start)) {
            fprintf(stderr, "violation: %s includes \"%s\" (add it to ALLOWED_TEST_EDGES with a reason, or remove it)\n",
                    relative_path, start);
            violations++;
        }
    }
    fclose(f);
    return violations;
}

static int scan_dir(const char *root, const char *relative_dir, const char *owner_framework, const char *owner_module,
                    int *files_checked) {
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", root, relative_dir);
    DIR *d = opendir(dir_path);
    if (d == NULL) {
        return 0;
    }
    int violations = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char relative[1024];
        char path[2048];
        snprintf(relative, sizeof(relative), "%s/%s", relative_dir, entry->d_name);
        snprintf(path, sizeof(path), "%s/%s", root, relative);
        struct stat info;
        if (stat(path, &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            violations += scan_dir(root, relative, owner_framework, owner_module, files_checked);
        } else if (ends_with(entry->d_name, ".c") || ends_with(entry->d_name, ".h")) {
            violations += check_file(path, relative, owner_framework, owner_module);
            (*files_checked)++;
        }
    }
    closedir(d);
    return violations;
}

int main(void) {
    const char *root = SOURCE_DIR;
    int violations = 0;
    int files_checked = 0;

    char tests_dir[1024];
    snprintf(tests_dir, sizeof(tests_dir), "%s/tests", root);
    DIR *d = opendir(tests_dir);
    if (d == NULL) {
        fprintf(stderr, "could not open %s\n", tests_dir);
        return 1;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char relative[512];
        snprintf(relative, sizeof(relative), "tests/%s", entry->d_name);
        violations += scan_dir(root, relative, NULL, entry->d_name, &files_checked);
    }
    closedir(d);

    for (size_t i = 0; i < sizeof(FRAMEWORKS) / sizeof(FRAMEWORKS[0]); i++) {
        char relative[512];
        snprintf(relative, sizeof(relative), "frameworks/%s/tests", FRAMEWORKS[i]);
        violations += scan_dir(root, relative, FRAMEWORKS[i], NULL, &files_checked);
    }

    if (violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in the test sources\n", violations);
        return 1;
    }
    printf("test sources: no layer violations (%d file(s) checked)\n", files_checked);
    return 0;
}
