#ifndef URSATZ_COMMON_EVENTS_H
#define URSATZ_COMMON_EVENTS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The event bus (R-648): a publisher gives an event a name and a JSON text, every subscriber gets a copy in a queue of its own, and a reader waits
 * for the next one with a timeout. A subscriber whose queue is full loses its queue and gets one "reset" event ("{}"), the sign to read the state again.
 * Thread safe. Closing the bus wakes every waiting reader, which then gets false.
 */

typedef struct CommonEventBus CommonEventBus;
typedef struct CommonEventSubscriber CommonEventSubscriber;

/* NULL when memory is out. Destroying a bus frees every subscriber's queue (a subscriber that is still held answers false afterwards: close the bus and unsubscribe first). */
CommonEventBus *common_event_bus_create(void);
void common_event_bus_destroy(CommonEventBus *bus);

/* Makes every waiting reader return false and every later one too; publishing after it is a no-op. */
void common_event_bus_close(CommonEventBus *bus);

/* A queue of queue_capacity events (at least 2). NULL when memory is out or the bus is closed. */
CommonEventSubscriber *common_event_subscribe(CommonEventBus *bus, size_t queue_capacity);
void common_event_unsubscribe(CommonEventSubscriber *subscriber);

/* Copies name and json (NULL for "{}") into the queue of every subscriber. */
void common_event_publish(CommonEventBus *bus, const char *name, const char *json);

/* Waits up to timeout_ms (0 for none, negative for as long as it takes) for the next event; *name and *json are the caller's (free them). False on a timeout
 * and when the bus is closed. */
bool common_event_next(CommonEventSubscriber *subscriber, int timeout_ms, char **name, char **json);

#endif
