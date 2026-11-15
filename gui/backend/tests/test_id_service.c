#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "service/id_service.h"

#define ID_COUNT 10000

static int failures = 0;

#define CHECK(cond, msg)                                         \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                          \
        }                                                        \
    } while (0)

static int matches_piece_pattern(const char *id) {
    if (strlen(id) != strlen("piece-") + 16 || strncmp(id, "piece-", 6) != 0) {
        return 0;
    }
    for (const char *p = id + 6; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static int compare_ids(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void test_ids_distinct_and_well_formed(void) {
    char **ids = calloc(ID_COUNT, sizeof(char *));
    CHECK(ids != NULL, "allocate id list");
    if (!ids) {
        return;
    }
    int malformed = 0;
    for (int i = 0; i < ID_COUNT; i++) {
        ids[i] = ursatz_gui_new_id("piece");
        if (!ids[i] || !matches_piece_pattern(ids[i])) {
            malformed++;
        }
    }
    CHECK(malformed == 0, "every id matches ^piece-[0-9a-f]{16}$");

    int duplicates = 0;
    if (malformed == 0) {
        qsort(ids, ID_COUNT, sizeof(char *), compare_ids);
        for (int i = 1; i < ID_COUNT; i++) {
            if (strcmp(ids[i - 1], ids[i]) == 0) {
                duplicates++;
            }
        }
    }
    CHECK(duplicates == 0, "10,000 ids are all distinct");

    for (int i = 0; i < ID_COUNT; i++) {
        free(ids[i]);
    }
    free(ids);
}

static void test_fork_differs_from_parent(void) {
    int fds[2];
    CHECK(pipe(fds) == 0, "create pipe");
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        char *id = ursatz_gui_new_id("piece");
        if (id) {
            ssize_t ignored = write(fds[1], id, strlen(id));
            (void)ignored;
        }
        _exit(id ? 0 : 1);
    }
    close(fds[1]);
    char *parent_id = ursatz_gui_new_id("piece");
    char child_id[64] = {0};
    ssize_t got = read(fds[0], child_id, sizeof(child_id) - 1);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(parent_id != NULL, "parent id allocated");
    CHECK(got > 0 && status == 0, "child produced an id");
    if (parent_id && got > 0) {
        CHECK(strcmp(parent_id, child_id) != 0, "forked child and parent ids differ");
    }
    free(parent_id);
}

static void test_prefix_and_null(void) {
    char *id = ursatz_gui_new_id("comp");
    CHECK(id != NULL && strncmp(id, "comp-", 5) == 0 && strlen(id) == 5 + 16, "prefix is kept");
    free(id);
    CHECK(ursatz_gui_new_id(NULL) == NULL, "NULL prefix gives NULL");
}

int main(void) {
    test_ids_distinct_and_well_formed();
    test_fork_differs_from_parent();
    test_prefix_and_null();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("test_id_service: ok\n");
    return 0;
}
