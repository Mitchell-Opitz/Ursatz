#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <common/files.h>

#include <ursatz_workshop/workshop_api.h>

static char g_root[64];

static Workshop *open_on(const char *data, UrsatzStatus *status, UrsatzError *error) {
    WorkshopConfig flags = {data, NULL, 0, 0, 0};
    WorkshopConfig resolved;
    char storage[512];
    assert(workshop_config_resolve(&flags, &resolved, storage, sizeof(storage), error) == URSATZ_OK);
    Workshop *workshop = NULL;
    *status = workshop_open(&resolved, &workshop, error);
    return workshop;
}

/* In a child process: the open is refused as CONFLICT and the message names the parent's pid. Exit 0 when it is. */
static void child_open_is_refused(const char *data, long parent) {
    UrsatzStatus status;
    UrsatzError error;
    Workshop *workshop = open_on(data, &status, &error);
    char pid[32];
    snprintf(pid, sizeof(pid), "(pid %ld)", parent);
    int ok = workshop == NULL && status == URSATZ_ERR_CONFLICT && strstr(error.message, pid) != NULL;
    if (workshop) {
        workshop_close(workshop);
    }
    _exit(ok ? 0 : 1);
}

static int run_child(const char *data, long parent) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        child_open_is_refused(data, parent);
    }
    int wait_status = 0;
    assert(waitpid(child, &wait_status, 0) == child);
    return WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : 99;
}

/* A child that holds the folder through workshop_open, tells the parent (pipe `ready`), and on each byte of `go` announces the port it reads
 * from the byte's value times 1000 (a byte 0 ends the child). */
static void holder_child(const char *data, int ready, int go) {
    UrsatzStatus status;
    UrsatzError error;
    Workshop *workshop = open_on(data, &status, &error);
    char byte = workshop ? 1 : 0;
    if (write(ready, &byte, 1) != 1 || !workshop) {
        _exit(1);
    }
    while (read(go, &byte, 1) == 1 && byte != 0) {
        if (workshop_announce(workshop, (int)byte * 1000) != URSATZ_OK || write(ready, &byte, 1) != 1) {
            _exit(2);
        }
    }
    workshop_close(workshop);
    _exit(0);
}

/* The lock holder: NOT_FOUND for a free folder, the pid and no port for a child that holds it, the port once the child announced it. */
static void test_lock_holder(void) {
    char folder[96];
    snprintf(folder, sizeof(folder), "%s/holder", g_root);
    assert(common_dir_make(folder, NULL) == URSATZ_OK);
    long pid = -1;
    int port = -1;
    UrsatzError error;
    assert(workshop_lock_holder(folder, &pid, &port, &error) == URSATZ_ERR_NOT_FOUND); /* no lock file yet */
    UrsatzStatus status;
    Workshop *once = open_on(folder, &status, &error);
    assert(once);
    workshop_close(once);
    assert(workshop_lock_holder(folder, &pid, &port, &error) == URSATZ_ERR_NOT_FOUND); /* the file stays, nobody holds it */
    /* a tools workshop and a bad port cannot announce */
    Workshop *tools = NULL;
    assert(workshop_tools_open(&tools, &error) == URSATZ_OK);
    assert(workshop_announce(tools, 8000) == URSATZ_ERR_REQUEST);
    workshop_close(tools);

    int ready[2];
    int go[2];
    assert(pipe(ready) == 0 && pipe(go) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(ready[0]);
        close(go[1]);
        holder_child(folder, ready[1], go[0]);
    }
    close(ready[1]);
    close(go[0]);
    char byte = 0;
    assert(read(ready[0], &byte, 1) == 1 && byte == 1);
    assert(workshop_lock_holder(folder, &pid, &port, &error) == URSATZ_OK && pid == (long)child && port == 0);
    byte = 7; /* the child announces port 7000 */
    assert(write(go[1], &byte, 1) == 1 && read(ready[0], &byte, 1) == 1);
    assert(workshop_lock_holder(folder, &pid, &port, &error) == URSATZ_OK && pid == (long)child && port == 7000);
    byte = 0;
    assert(write(go[1], &byte, 1) == 1);
    int wait_status = 0;
    assert(waitpid(child, &wait_status, 0) == child && WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0);
    assert(workshop_lock_holder(folder, &pid, &port, &error) == URSATZ_ERR_NOT_FOUND); /* the child closed it */
    close(ready[0]);
    close(go[1]);
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "workshop-lock-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    UrsatzStatus status;
    UrsatzError error;

    Workshop *first = open_on(g_root, &status, &error);
    assert(first && status == URSATZ_OK);

    /* A second open, in this process and in another, is refused while the first is open. */
    Workshop *second = open_on(g_root, &status, &error);
    assert(second == NULL && status == URSATZ_ERR_CONFLICT && strstr(error.message, g_root));
    assert(run_child(g_root, (long)getpid()) == 0);

    /* After close the data folder opens again. */
    workshop_close(first);
    second = open_on(g_root, &status, &error);
    assert(second && status == URSATZ_OK);
    workshop_close(second);

    test_lock_holder();

    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    return 0;
}
