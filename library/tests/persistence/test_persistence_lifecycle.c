#include "persistence/persistence.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * persistence_open/persistence_close lifecycle tests: a valid path
 * opens and creates the schema; a bad path (unwritable directory) and
 * a corrupt (non-SQLite) file both fail fast with no handle produced.
 */

static void test_open_valid_path_succeeds(void) {
    const char *path = "test_persistence_lifecycle_valid.sqlite";
    remove(path);

    PersistenceHandle *handle = NULL;
    PersistenceResult result = persistence_open(path, &handle);
    assert(result == PERSISTENCE_OK);
    assert(handle != NULL);

    persistence_close(handle);
    remove(path);
}

static void test_open_unwritable_path_fails(void) {
    const char *path = "/nonexistent-directory/persistence.sqlite";

    PersistenceHandle *handle = NULL;
    PersistenceResult result = persistence_open(path, &handle);
    assert(result == PERSISTENCE_ERR_OPEN_FAILED);
    assert(handle == NULL);
}

static void test_open_corrupt_file_fails(void) {
    const char *path = "test_persistence_lifecycle_corrupt.sqlite";
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fprintf(f, "not a sqlite database");
    fclose(f);

    PersistenceHandle *handle = NULL;
    PersistenceResult result = persistence_open(path, &handle);
    assert(result == PERSISTENCE_ERR_SCHEMA_FAILED);
    assert(handle == NULL);

    remove(path);
}

static void test_open_null_args_fails(void) {
    PersistenceHandle *handle = NULL;
    assert(persistence_open(NULL, &handle) == PERSISTENCE_ERR_NULL_ARG);
    assert(persistence_open("test_persistence_lifecycle_nullarg.sqlite", NULL) == PERSISTENCE_ERR_NULL_ARG);
}

int main(void) {
    test_open_valid_path_succeeds();
    test_open_unwritable_path_fails();
    test_open_corrupt_file_fails();
    test_open_null_args_fails();
    printf("persistence lifecycle: all tests passed\n");
    return 0;
}
