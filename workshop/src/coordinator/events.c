#include <stdlib.h>

#include <common/events.h>

#include "base/events_hub.h"
#include "coordinator/coordinator.h"

/* The workshop's events for the programs that use it (R-648): a subscription is a queue of 256 events on the workshop's one bus. */

#define QUEUE_CAPACITY 256

struct WorkshopEvents {
    CommonEventSubscriber *subscriber;
};

WorkshopEvents *workshop_events_subscribe(Workshop *workshop) {
    CommonEventBus *bus = workshop && workshop->started ? workshop_events_hub() : NULL;
    WorkshopEvents *events = bus ? calloc(1, sizeof(*events)) : NULL;
    if (!events) {
        return NULL;
    }
    events->subscriber = common_event_subscribe(bus, QUEUE_CAPACITY);
    if (!events->subscriber) {
        free(events);
        return NULL;
    }
    return events;
}

bool workshop_events_next(WorkshopEvents *events, int timeout_ms, char **name, char **json) {
    return events && common_event_next(events->subscriber, timeout_ms, name, json);
}

void workshop_events_unsubscribe(WorkshopEvents *events) {
    if (events) {
        common_event_unsubscribe(events->subscriber);
        free(events);
    }
}

void workshop_events_close(Workshop *workshop) {
    if (workshop && workshop->started) {
        common_event_bus_close(workshop_events_hub());
    }
}
