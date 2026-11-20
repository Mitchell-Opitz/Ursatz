#include "common/events.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/strings.h"

typedef struct {
    char *name;
    char *json;
} Event;

struct CommonEventSubscriber {
    CommonEventBus *bus;
    Event *queue;
    size_t capacity;
    size_t head;
    size_t count;
    struct CommonEventSubscriber *next;
};

struct CommonEventBus {
    pthread_mutex_t lock;
    pthread_cond_t wake;
    struct CommonEventSubscriber *subscribers;
    bool closed;
};

CommonEventBus *common_event_bus_create(void) {
    CommonEventBus *bus = calloc(1, sizeof(*bus));
    if (!bus) {
        return NULL;
    }
    pthread_mutex_init(&bus->lock, NULL);
    pthread_condattr_t attribute;
    pthread_condattr_init(&attribute);
    pthread_condattr_setclock(&attribute, CLOCK_MONOTONIC);
    pthread_cond_init(&bus->wake, &attribute);
    pthread_condattr_destroy(&attribute);
    return bus;
}

static void clear_queue(CommonEventSubscriber *subscriber) {
    for (size_t i = 0; i < subscriber->count; i++) {
        Event *event = &subscriber->queue[(subscriber->head + i) % subscriber->capacity];
        free(event->name);
        free(event->json);
    }
    subscriber->head = 0;
    subscriber->count = 0;
}

static void free_subscriber(CommonEventSubscriber *subscriber) {
    clear_queue(subscriber);
    free(subscriber->queue);
    free(subscriber);
}

void common_event_bus_close(CommonEventBus *bus) {
    if (!bus) {
        return;
    }
    pthread_mutex_lock(&bus->lock);
    bus->closed = true;
    pthread_cond_broadcast(&bus->wake);
    pthread_mutex_unlock(&bus->lock);
}

void common_event_bus_destroy(CommonEventBus *bus) {
    if (!bus) {
        return;
    }
    common_event_bus_close(bus);
    pthread_mutex_lock(&bus->lock);
    CommonEventSubscriber *subscriber = bus->subscribers;
    while (subscriber) {
        CommonEventSubscriber *next = subscriber->next;
        free_subscriber(subscriber);
        subscriber = next;
    }
    bus->subscribers = NULL;
    pthread_mutex_unlock(&bus->lock);
    pthread_cond_destroy(&bus->wake);
    pthread_mutex_destroy(&bus->lock);
    free(bus);
}

CommonEventSubscriber *common_event_subscribe(CommonEventBus *bus, size_t queue_capacity) {
    if (!bus || queue_capacity < 2) {
        return NULL;
    }
    CommonEventSubscriber *subscriber = calloc(1, sizeof(*subscriber));
    Event *queue = subscriber ? calloc(queue_capacity, sizeof(*queue)) : NULL;
    if (!queue) {
        free(subscriber);
        return NULL;
    }
    subscriber->bus = bus;
    subscriber->queue = queue;
    subscriber->capacity = queue_capacity;
    pthread_mutex_lock(&bus->lock);
    if (bus->closed) {
        pthread_mutex_unlock(&bus->lock);
        free_subscriber(subscriber);
        return NULL;
    }
    subscriber->next = bus->subscribers;
    bus->subscribers = subscriber;
    pthread_mutex_unlock(&bus->lock);
    return subscriber;
}

void common_event_unsubscribe(CommonEventSubscriber *subscriber) {
    if (!subscriber) {
        return;
    }
    CommonEventBus *bus = subscriber->bus;
    pthread_mutex_lock(&bus->lock);
    for (CommonEventSubscriber **link = &bus->subscribers; *link; link = &(*link)->next) {
        if (*link == subscriber) {
            *link = subscriber->next;
            break;
        }
    }
    pthread_mutex_unlock(&bus->lock);
    free_subscriber(subscriber);
}

/* Appends a copy to the queue; a full queue is cleared and gets one "reset" instead. */
static void push(CommonEventSubscriber *subscriber, const char *name, const char *json) {
    if (subscriber->count == subscriber->capacity) {
        clear_queue(subscriber);
        name = "reset";
        json = "{}";
    }
    Event *event = &subscriber->queue[(subscriber->head + subscriber->count) % subscriber->capacity];
    event->name = common_strdup(name);
    event->json = common_strdup(json);
    if (!event->name || !event->json) {
        free(event->name);
        free(event->json);
        return;
    }
    subscriber->count++;
}

void common_event_publish(CommonEventBus *bus, const char *name, const char *json) {
    if (!bus || !name) {
        return;
    }
    pthread_mutex_lock(&bus->lock);
    if (!bus->closed) {
        for (CommonEventSubscriber *subscriber = bus->subscribers; subscriber; subscriber = subscriber->next) {
            push(subscriber, name, json ? json : "{}");
        }
        pthread_cond_broadcast(&bus->wake);
    }
    pthread_mutex_unlock(&bus->lock);
}

bool common_event_next(CommonEventSubscriber *subscriber, int timeout_ms, char **name, char **json) {
    CommonEventBus *bus = subscriber->bus;
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    if (timeout_ms > 0) {
        deadline.tv_sec += timeout_ms / 1000;
        deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&bus->lock);
    while (subscriber->count == 0 && !bus->closed) {
        if (timeout_ms == 0) {
            break;
        }
        if (timeout_ms < 0) {
            pthread_cond_wait(&bus->wake, &bus->lock);
        } else if (pthread_cond_timedwait(&bus->wake, &bus->lock, &deadline) != 0) {
            break;
        }
    }
    bool got = subscriber->count > 0;
    if (got) {
        Event *event = &subscriber->queue[subscriber->head];
        *name = event->name;
        *json = event->json;
        event->name = NULL;
        event->json = NULL;
        subscriber->head = (subscriber->head + 1) % subscriber->capacity;
        subscriber->count--;
    }
    pthread_mutex_unlock(&bus->lock);
    return got;
}
