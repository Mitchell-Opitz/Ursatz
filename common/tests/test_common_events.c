#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <common/events.h>

static int g_failures = 0;
#define check(cond, what)                                    \
    do {                                                     \
        if (!(cond)) {                                       \
            fprintf(stderr, "FAIL: %s (line %d)\n", what, __LINE__); \
            g_failures++;                                    \
        }                                                    \
    } while (0)

/* The event bus (R-648): copies for every subscriber, order kept, a timeout, a reset for a full queue, and a close that wakes a reader. */

static void *publish_later(void *bus) {
    usleep(50 * 1000);
    common_event_publish(bus, "late", "{\"n\":1}");
    return NULL;
}

static void *close_later(void *bus) {
    usleep(50 * 1000);
    common_event_bus_close(bus);
    return NULL;
}

int main(void) {
    CommonEventBus *bus = common_event_bus_create();
    CommonEventSubscriber *a = common_event_subscribe(bus, 4);
    CommonEventSubscriber *b = common_event_subscribe(bus, 4);
    check(bus && a && b && !common_event_subscribe(bus, 1), "subscribers need a queue of at least two");
    char *name = NULL;
    char *json = NULL;
    check(!common_event_next(a, 0, &name, &json), "no event is no event");
    common_event_publish(bus, "one", "{\"a\":1}");
    common_event_publish(bus, "two", NULL);
    check(common_event_next(a, 0, &name, &json) && strcmp(name, "one") == 0 && strcmp(json, "{\"a\":1}") == 0, "a subscriber gets the first event");
    free(name);
    free(json);
    check(common_event_next(a, 0, &name, &json) && strcmp(name, "two") == 0 && strcmp(json, "{}") == 0, "then the second, with {} for no data");
    free(name);
    free(json);
    check(common_event_next(b, 0, &name, &json) && strcmp(name, "one") == 0, "every subscriber has its own copy");
    free(name);
    free(json);
    common_event_unsubscribe(b);

    for (int i = 0; i < 6; i++) {
        common_event_publish(bus, "x", "{}");
    }
    int count = 0;
    int resets = 0;
    while (common_event_next(a, 0, &name, &json)) {
        count++;
        resets += strcmp(name, "reset") == 0;
        free(name);
        free(json);
    }
    check(resets == 1 && count < 4, "a full queue is cleared and gets one reset");

    pthread_t thread;
    pthread_create(&thread, NULL, publish_later, bus);
    check(common_event_next(a, 2000, &name, &json) && strcmp(name, "late") == 0, "a reader waits for the next event");
    free(name);
    free(json);
    pthread_join(thread, NULL);
    check(!common_event_next(a, 50, &name, &json), "and gives up after its timeout");
    pthread_create(&thread, NULL, close_later, bus);
    check(!common_event_next(a, -1, &name, &json), "closing the bus wakes a reader that waits for ever");
    pthread_join(thread, NULL);
    common_event_publish(bus, "after", "{}");
    check(!common_event_next(a, 0, &name, &json) && !common_event_subscribe(bus, 4), "a closed bus takes no events and no subscribers");
    common_event_unsubscribe(a);
    common_event_bus_destroy(bus);
    return g_failures == 0 ? 0 : 1;
}
