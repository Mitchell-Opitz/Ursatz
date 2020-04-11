#include "persistence/persistence.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Reproduces the ursatz-gui repro: two threads within one process open
 * separate handles to the same .db file concurrently (e.g. a
 * meta-fetch and a full-export request landing at the same time). One
 * thread holds an exclusive write lock briefly while the other opens
 * and reads; both must succeed once persistence_open sets a busy
 * timeout, instead of the second call failing immediately with
 * SQLITE_BUSY.
 */

static const char *DB_PATH = "test_persistence_concurrency.sqlite";

static pthread_barrier_t barrier;
static PersistenceResult writer_result = PERSISTENCE_ERR_NULL_ARG;
static PersistenceResult reader_result = PERSISTENCE_ERR_NULL_ARG;

static void *writer_thread(void *arg) {
    (void)arg;
    PersistenceHandle *handle = NULL;
    writer_result = persistence_open(DB_PATH, &handle);
    if (writer_result != PERSISTENCE_OK) {
        return NULL;
    }

    sqlite3 *db = persistence_db(handle);
    char *errmsg = NULL;
    sqlite3_exec(db, "BEGIN EXCLUSIVE;", NULL, NULL, &errmsg);
    sqlite3_free(errmsg);

    pthread_barrier_wait(&barrier);
    usleep(200 * 1000);

    sqlite3_exec(db, "COMMIT;", NULL, NULL, &errmsg);
    sqlite3_free(errmsg);
    persistence_close(handle);
    return NULL;
}

static void *reader_thread(void *arg) {
    (void)arg;
    pthread_barrier_wait(&barrier);

    PersistenceHandle *handle = NULL;
    reader_result = persistence_open(DB_PATH, &handle);
    if (reader_result == PERSISTENCE_OK) {
        persistence_close(handle);
    }
    return NULL;
}

static void test_concurrent_open_waits_instead_of_busy(void) {
    remove(DB_PATH);

    PersistenceHandle *setup = NULL;
    assert(persistence_open(DB_PATH, &setup) == PERSISTENCE_OK);
    persistence_close(setup);

    pthread_barrier_init(&barrier, NULL, 2);

    pthread_t writer;
    pthread_t reader;
    pthread_create(&writer, NULL, writer_thread, NULL);
    pthread_create(&reader, NULL, reader_thread, NULL);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);
    pthread_barrier_destroy(&barrier);

    assert(writer_result == PERSISTENCE_OK);
    assert(reader_result == PERSISTENCE_OK);

    remove(DB_PATH);
}

int main(void) {
    test_concurrent_open_waits_instead_of_busy();
    printf("persistence concurrency: all tests passed\n");
    return 0;
}
