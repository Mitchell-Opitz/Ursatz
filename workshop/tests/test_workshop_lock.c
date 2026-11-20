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
    assert(workshop_settings_resolve(&flags, &resolved, storage, sizeof(storage), error) == URSATZ_OK);
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

    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    return 0;
}
