#define _XOPEN_SOURCE 700
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Architecture test: vendored third-party code (third_party/) is private to one module. The XML parser ezxml
 * may be included by src/io/musicxml_importer.c and by nothing else: no other file under src/, frameworks/ or
 * tests/ may include "ezxml.h" (third_party/ itself is not scanned). Its types must not leak through a public
 * header either, which this enforces as a consequence: a header that included it would be a second includer.
 */

static int violations = 0;
static int files_checked = 0;
static int allowed_includers = 0;

static int ends_with(const char *name, const char *suffix) {
    size_t n = strlen(name);
    size_t m = strlen(suffix);
    return m <= n && strcmp(name + n - m, suffix) == 0;
}

static int visit(const char *path, const struct stat *info, int type, struct FTW *where) {
    (void)info;
    (void)where;
    if (type != FTW_F || (!ends_with(path, ".c") && !ends_with(path, ".h"))) {
        return 0;
    }
    /* this test names the header in its own text */
    if (ends_with(path, "test_architecture_third_party.c")) {
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "could not open %s\n", path);
        violations++;
        return 0;
    }
    files_checked++;
    char line[1024];
    while (fgets(line, sizeof(line), f) != NULL) {
        const char *marker = strstr(line, "#include \"ezxml.h\"");
        if (marker == NULL) {
            continue;
        }
        if (ends_with(path, "src/io/musicxml_importer.c")) {
            allowed_includers++;
        } else {
            fprintf(stderr, "violation: %s includes \"ezxml.h\" (only src/io/musicxml_importer.c may)\n", path);
            violations++;
        }
    }
    fclose(f);
    return 0;
}

int main(void) {
    const char *roots[3] = {"../src", "../frameworks", "../tests"};
    for (int i = 0; i < 3; i++) {
        if (nftw(roots[i], visit, 16, FTW_PHYS) != 0) {
            fprintf(stderr, "could not walk %s\n", roots[i]);
            return 1;
        }
    }
    if (allowed_includers != 1) {
        fprintf(stderr, "expected exactly one includer of ezxml.h (src/io/musicxml_importer.c), found %d\n",
                allowed_includers);
        return 1;
    }
    if (violations > 0) {
        fprintf(stderr, "%d third-party architecture violation(s)\n", violations);
        return 1;
    }
    printf("third_party: ezxml.h is included by src/io/musicxml_importer.c only (%d file(s) checked)\n", files_checked);
    return 0;
}
