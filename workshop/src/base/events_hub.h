#ifndef WORKSHOP_BASE_EVENTS_HUB_H
#define WORKSHOP_BASE_EVENTS_HUB_H

#include <cJSON.h>

#include <common/events.h>

/*
 * The workshop's one event bus (R-648) and what publishes into it. The coordinator makes the bus when the workshop starts and removes it when it stops;
 * the services publish through workshop_event, which does nothing while there is no bus (a test that uses a store without a workshop).
 */

void workshop_events_hub_set(CommonEventBus *bus);
CommonEventBus *workshop_events_hub(void);

/* Publishes name with object as its data (freed; NULL for none). */
void workshop_event(const char *name, cJSON *object);

/* After a transaction committed (items/txn.c): the events its command stands for: item-changed for every item header it wrote, item-removed for an
 * item delete, and library-, collection-, subject-, style- or links-changed for the commands of those kinds, naming the target when it has one. */
void workshop_events_after_commit(const char *command, const char *target, const cJSON *headers);

#endif
