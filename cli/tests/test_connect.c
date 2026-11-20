#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <common/files.h>

#include <ursatz_workshop/workshop_api.h>

#include "test_http.h"

/*
 * Connect or run (R-659): ursatz --data <dir> status opens the folder under the lock when nobody holds it, says so when another process holds it and
 * serves nothing, and connects to the GUI backend when that serves it. The programs are run as processes; the environment variable URSATZ_DATA is
 * taken out so that each case names the folder (or does not) itself.
 */

#define PORT "18781"

static char g_root[96];

/* Runs "env -u URSATZ_DATA <ursatz> <arguments> 2>&1": the output (stdout and stderr, a buffer the caller frees) and the exit status. */
static char *run(const char *arguments, int *status) {
    char command[2048];
    snprintf(command, sizeof(command), "env -u URSATZ_DATA \"%s\" %s 2>&1", URSATZ_BIN, arguments);
    FILE *pipe = popen(command, "r");
    assert(pipe);
    size_t capacity = 8192;
    size_t used = 0;
    char *text = malloc(capacity);
    size_t got;
    while ((got = fread(text + used, 1, capacity - used - 1, pipe)) > 0) {
        used += got;
        if (used + 1 >= capacity) {
            capacity *= 2;
            text = realloc(text, capacity);
        }
    }
    text[used] = '\0';
    int raw = pclose(pipe);
    *status = WIFEXITED(raw) ? WEXITSTATUS(raw) : 99;
    return text;
}

static Workshop *open_on(const char *data) {
    WorkshopConfig flags = {data, NULL, 0, 0, 0};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    assert(workshop_settings_resolve(&flags, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK);
    return workshop;
}

static void test_a_free_folder_is_opened_here(void) {
    char data[160];
    char arguments[512];
    snprintf(data, sizeof(data), "%s/fresh", g_root);
    snprintf(arguments, sizeof(arguments), "--data \"%s\" status", data);
    int status = 0;
    char *out = run(arguments, &status);
    assert(status == 0 && strstr(out, "opened by this command") && strstr(out, "composer ") && strstr(out, "analyzer "));
    free(out);
    snprintf(arguments, sizeof(arguments), "--data \"%s\" status --format json", data);
    out = run(arguments, &status);
    assert(status == 0 && strstr(out, "\"mode\": \"local\""));
    free(out);
    /* the same folder through the environment */
    snprintf(arguments, sizeof(arguments), "URSATZ_DATA=\"%s\" \"%s\" status 2>&1", data, URSATZ_BIN);
    char command[1024];
    snprintf(command, sizeof(command), "%s", arguments);
    FILE *pipe = popen(command, "r");
    assert(pipe);
    char line[256];
    bool opened = false;
    while (fgets(line, sizeof(line), pipe)) {
        opened = opened || strstr(line, "opened by this command") != NULL;
    }
    assert(pclose(pipe) == 0 && opened);
}

static void test_no_folder_named(void) {
    int status = 0;
    char *out = run("status", &status);
    assert(status == 1 && strstr(out, "name the data folder with --data <dir> or URSATZ_DATA"));
    free(out);
    out = run("--data", &status);
    assert(status == 1 && strstr(out, "--data needs a value"));
    free(out);
    out = run("--port 99999 --data x status", &status);
    assert(status == 1 && strstr(out, "--port takes a whole number"));
    free(out);
}

static void test_a_folder_held_without_a_port(void) {
    char data[160];
    char arguments[512];
    snprintf(data, sizeof(data), "%s/held", g_root);
    assert(common_dir_make(data, NULL) == URSATZ_OK);
    Workshop *holder = open_on(data); /* this process holds it and announces no port */
    snprintf(arguments, sizeof(arguments), "--data \"%s\" status", data);
    int status = 0;
    char *out = run(arguments, &status);
    assert(status == 2 && strstr(out, "in use by another Ursatz process (pid") && strstr(out, "it takes no connections, so run again when it ends"));
    free(out);
    workshop_close(holder);
}

static void test_the_backend_serves_the_folder(void) {
    char data[160];
    char arguments[512];
    snprintf(data, sizeof(data), "%s/served", g_root);
    assert(common_dir_make(data, NULL) == URSATZ_OK);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl(URSATZ_BACKEND, URSATZ_BACKEND, "--data", data, "--port", PORT, (char *)NULL);
        _exit(127);
    }
    int answered = 0;
    for (int attempt = 0; attempt < 200 && !answered; attempt++) {
        int http = 0;
        cJSON *json = test_http_json(PORT, "GET", "/api/workshop/status", NULL, &http);
        answered = http == 200 && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "data_dir"));
        cJSON_Delete(json);
        if (!answered) {
            test_sleep_ms(50);
        }
    }
    assert(answered);
    /* the backend writes its port into the lock file just after it starts serving */
    int announced = 0;
    for (int attempt = 0; attempt < 200 && !announced; attempt++) {
        long holder = 0;
        int port = 0;
        UrsatzError holder_error;
        announced = workshop_lock_holder(data, &holder, &port, &holder_error) == URSATZ_OK && port == atoi(PORT);
        if (!announced) {
            test_sleep_ms(50);
        }
    }
    assert(announced);

    int status = 0;
    snprintf(arguments, sizeof(arguments), "--data \"%s\" status", data);
    char *out = run(arguments, &status);
    assert(status == 0 && strstr(out, "served by the Ursatz process") && strstr(out, "on port " PORT));
    free(out);
    snprintf(arguments, sizeof(arguments), "--data \"%s\" query project.list", data);
    out = run(arguments, &status);
    assert(status == 0 && strstr(out, "\"projects\""));
    free(out);
    /* a refusal comes back as the workshop's message, exit 1 for a request */
    snprintf(arguments, sizeof(arguments), "--data \"%s\" query no.such.query", data);
    out = run(arguments, &status);
    assert(status == 1 && strstr(out, "Error: unknown query"));
    free(out);

    kill(child, SIGTERM);
    int wait_status = 0;
    assert(waitpid(child, &wait_status, 0) == child);
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "cli-connect-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    test_no_folder_named();
    test_a_free_folder_is_opened_here();
    test_a_folder_held_without_a_port();
    test_the_backend_serves_the_folder();
    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    puts("ok");
    return 0;
}
