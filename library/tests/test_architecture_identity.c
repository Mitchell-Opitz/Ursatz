#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: music-identity (L0) MUST NOT import anything
 * outside identity/ or the standard library. Dependency & Layer Spec v2 §2, §3.
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

        if (strncmp(start, "identity/", 9) != 0) {
            fprintf(stderr, "violation: %s includes \"%s\" (outside identity/)\n", path, start);
            violations++;
        }
    }

    fclose(f);
    return violations;
}

int main(void) {
    const char *files[] = {
        "../src/identity/entity_id.h",
        "../src/identity/entity_id.c",
        "../src/identity/version.h",
        "../src/identity/version.c",
        "../src/identity/version_model.h",
        "../src/identity/version_model.c",
        "../src/identity/schema_version.h",
        "../src/identity/schema_version.c"
    };
    size_t count = sizeof(files) / sizeof(files[0]);
    int total_violations = 0;

    for (size_t i = 0; i < count; i++) {
        total_violations += check_file(files[i]);
    }

    if (total_violations > 0) {
        fprintf(stderr, "%d architecture violation(s) found in music-identity\n", total_violations);
        return 1;
    }

    printf("music-identity: no forward/outside dependencies found\n");
    return 0;
}