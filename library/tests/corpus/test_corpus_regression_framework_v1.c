#include "corpus/corpus_api.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * CorpusRegression: registers "framework-v1-reference-set" and one
 * piece_ref per .mid file currently in tests/fixtures/framework-v1-corpus/,
 * then asserts the Corpus reflects exactly that set. Runs against whatever
 * fixture files are present -- zero files (the pre-fixture state) is
 * expected, not a failure.
 */

#define FIXTURE_DIR "../fixtures/framework-v1-corpus"
#define MAX_FIXTURES 256

static int has_mid_suffix(const char *name) {
    size_t len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".mid") == 0;
}

static size_t scan_mid_filenames(char **out_names, size_t max_names) {
    DIR *d = opendir(FIXTURE_DIR);
    assert(d != NULL);

    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (!has_mid_suffix(entry->d_name)) {
            continue;
        }
        assert(count < max_names);
        out_names[count] = strdup(entry->d_name);
        assert(out_names[count] != NULL);
        count++;
    }
    closedir(d);
    return count;
}

int main(void) {
    const char *path = "test_corpus_regression_framework_v1.sqlite";
    remove(path);

    CorpusStoreHandle *handle = NULL;
    assert(corpus_api_open(path, &handle) == CORPUS_API_OK);

    const char *corpus_id = "framework-v1-reference-set";
    assert(corpus_api_create_corpus(handle, corpus_id, "framework-v1-reference-set") == CORPUS_API_OK);

    char *fixture_names[MAX_FIXTURES];
    size_t fixture_count = scan_mid_filenames(fixture_names, MAX_FIXTURES);

    for (size_t i = 0; i < fixture_count; i++) {
        assert(corpus_api_add_piece_ref(handle, corpus_id, fixture_names[i]) == CORPUS_API_OK);
    }

    CorpusApiEntry *entries = NULL;
    size_t entry_count = 0;
    assert(corpus_api_list_corpora(handle, &entries, &entry_count) == CORPUS_API_OK);
    int found = 0;
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].id, corpus_id) == 0) {
            assert(strcmp(entries[i].name, "framework-v1-reference-set") == 0);
            found = 1;
        }
    }
    assert(found);
    corpus_api_free_corpus_list(entries, entry_count);

    char **refs = NULL;
    size_t ref_count = 0;
    assert(corpus_api_list_piece_refs(handle, corpus_id, &refs, &ref_count) == CORPUS_API_OK);
    assert(ref_count == fixture_count);

    for (size_t i = 0; i < fixture_count; i++) {
        int matched = 0;
        for (size_t j = 0; j < ref_count; j++) {
            if (strcmp(fixture_names[i], refs[j]) == 0) {
                matched = 1;
                break;
            }
        }
        assert(matched);
        free(fixture_names[i]);
    }

    for (size_t i = 0; i < ref_count; i++) {
        free(refs[i]);
    }
    free(refs);

    corpus_api_close(handle);
    remove(path);

    printf("corpus_regression_framework_v1: all tests passed (%zu fixture(s))\n", fixture_count);
    return 0;
}
