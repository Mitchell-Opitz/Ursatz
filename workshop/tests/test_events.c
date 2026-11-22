/* The workshop's events (R-648): a command publishes the events of what it changed after its transaction committed, a job publishes job-progress and
 * job-done, in order, and a subscriber that does not read gets one "reset" when its queue overflows. Runs in its own working directory. */

#include "api_harness.h"

#include <ursatz_test.h>

#ifndef URSATZ_WORKSHOP_TEST_LINK_FIXTURES
#error "URSATZ_WORKSHOP_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

/* The next event within a second: its name and data (the caller frees both), false when there is none. */
static bool next_event(WorkshopEvents *events, char **name, cJSON **data) {
    char *text = NULL;
    if (!workshop_events_next(events, 1000, name, &text)) {
        return false;
    }
    *data = cJSON_Parse(text);
    free(text);
    return true;
}

/* Reads events until one named name arrives (the names that passed are appended to seen, comma separated); the data of it is the caller's. */
static cJSON *read_until(WorkshopEvents *events, const char *name, char *seen, size_t size) {
    for (int i = 0; i < 400; i++) {
        char *got = NULL;
        cJSON *data = NULL;
        if (!next_event(events, &got, &data)) {
            return NULL;
        }
        size_t length = strlen(seen);
        snprintf(seen + length, size - length, "%s%s", length ? "," : "", got);
        bool found = strcmp(got, name) == 0;
        free(got);
        if (found) {
            return data;
        }
        cJSON_Delete(data);
    }
    return NULL;
}

int main(void) {
    if (!harness_open(18994)) {
        return 1;
    }
    WorkshopEvents *events = workshop_events_subscribe(g_workshop);
    WorkshopEvents *sleeper = workshop_events_subscribe(g_workshop);
    check(events && sleeper, "a started workshop gives subscriptions");

    WorkshopReply reply;
    UrsatzError error;
    char seen[512] = "";
    check(command("subject.create", NULL, "{\"name\":\"Anna\",\"kind\":\"idea\"}", -1, &reply, &error) == URSATZ_OK, "a subject is made");
    char subject[64];
    snprintf(subject, sizeof(subject), "%s", str(member(reply.json, "subject"), "id"));
    workshop_reply_destroy(&reply);
    cJSON *data = read_until(events, "subject-changed", seen, sizeof(seen));
    check(data != NULL, "a command publishes subject-changed after its transaction committed");
    cJSON_Delete(data);

    check(command("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply, &error) == URSATZ_OK, "a project is made");
    char project[64];
    snprintf(project, sizeof(project), "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    char params[2500];
    snprintf(params, sizeof(params),
             "{\"type\":\"motif\",\"name\":\"A\",\"project_id\":\"%s\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e "
             "Eb4:e C4:e Eb4:e G4:e C5:e\",\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}",
             project);
    check(command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK, "a motif is made");
    char motif[64];
    snprintf(motif, sizeof(motif), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    data = read_until(events, "item-changed", seen, sizeof(seen));
    check(data && strcmp(str(data, "item_id"), motif) == 0 && num(data, "version") == 1, "item-changed names the item and its version");
    cJSON_Delete(data);

    check(command("item.delete", motif, "{}", 1, &reply, &error) == URSATZ_OK, "the motif is deleted");
    workshop_reply_destroy(&reply);
    data = read_until(events, "item-removed", seen, sizeof(seen));
    check(data && strcmp(str(data, "item_id"), motif) == 0, "item-removed names the item");
    cJSON_Delete(data);

    /* A job: the import's transaction is published before its job is done, and job-done names the job and how it ended. */
    char path[600];
    snprintf(path, sizeof(path), "%s/a.musicxml", URSATZ_WORKSHOP_TEST_LINK_FIXTURES);
    size_t size = 0;
    char *file = test_read_file(path, &size);
    WorkshopBytes bytes = {(const uint8_t *)file, size, "a.musicxml"};
    check(command_as("test", "library.import", NULL, "{\"filename\":\"a.musicxml\"}", -1, &bytes, &reply, &error) == URSATZ_OK, "an import is queued");
    char job_id[64];
    snprintf(job_id, sizeof(job_id), "%s", str(member(reply.json, "job"), "id"));
    workshop_reply_destroy(&reply);
    free(file);
    seen[0] = '\0';
    data = read_until(events, "job-done", seen, sizeof(seen));
    check(data && strcmp(str(data, "id"), job_id) == 0 && strcmp(str(data, "state"), "done") == 0 && num(data, "http_status") == 200,
          "job-done names the job, its state and its status");
    cJSON_Delete(data);
    const char *library = strstr(seen, "library-changed");
    const char *done = strstr(seen, "job-done");
    check(library && done && library < done, "the change the job committed was published before the job was done");

    /* A subscriber that does not read gets one reset when its queue (256 events) overflows. */
    char update[96];
    snprintf(update, sizeof(update), "{\"note\":\"n\"}");
    for (int i = 0; i < 270; i++) {
        snprintf(update, sizeof(update), "{\"note\":\"n%d\"}", i);
        if (command("subject.update", subject, update, -1, &reply, &error) != URSATZ_OK) {
            break;
        }
        workshop_reply_destroy(&reply);
    }
    bool reset = false;
    int total = 0;
    char *name = NULL;
    char *text = NULL;
    while (workshop_events_next(sleeper, 0, &name, &text)) {
        reset = reset || strcmp(name, "reset") == 0;
        total++;
        free(name);
        free(text);
    }
    check(reset && total < 256, "a subscriber that did not read got a reset in place of its overflowing queue");

    workshop_events_unsubscribe(events);
    workshop_events_unsubscribe(sleeper);
    workshop_events_close(g_workshop);

    /* Stopped, the workshop has no bus: nothing subscribes; a second stop does nothing; started again it publishes to a new subscriber;
       stopping NULL is safe. */
    workshop_stop(g_workshop);
    check(workshop_events_subscribe(g_workshop) == NULL, "a stopped workshop gives no subscription");
    workshop_stop(g_workshop);
    check(workshop_events_subscribe(g_workshop) == NULL, "a second stop changes nothing");
    check(workshop_start(g_workshop, &error) == URSATZ_OK, "a stopped workshop starts again");
    WorkshopEvents *again = workshop_events_subscribe(g_workshop);
    check(again != NULL, "a restarted workshop gives subscriptions");
    workshop_events_unsubscribe(again);
    workshop_stop(NULL);
    harness_close();
    return test_finish("workshop_events");
}
