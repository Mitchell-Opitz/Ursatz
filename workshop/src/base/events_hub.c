#include "base/events_hub.h"

#include <stdlib.h>
#include <string.h>

static CommonEventBus *g_bus = NULL;

void workshop_events_hub_set(CommonEventBus *bus) {
    g_bus = bus;
}

CommonEventBus *workshop_events_hub(void) {
    return g_bus;
}

void workshop_event(const char *name, cJSON *object) {
    CommonEventBus *bus = g_bus;
    if (bus) {
        char *text = object ? cJSON_PrintUnformatted(object) : NULL;
        common_event_publish(bus, name, text);
        free(text);
    }
    cJSON_Delete(object);
}

static void event_with_id(const char *name, const char *member, const char *id) {
    cJSON *object = cJSON_CreateObject();
    if (object && cJSON_AddStringToObject(object, member, id ? id : "")) {
        workshop_event(name, object);
    } else {
        cJSON_Delete(object);
    }
}

void workshop_events_after_commit(const char *command, const char *target, const cJSON *headers) {
    if (!g_bus) {
        return;
    }
    const cJSON *header = NULL;
    cJSON_ArrayForEach(header, headers) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(header, "id");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(header, "current_version");
        cJSON *object = cJSON_CreateObject();
        if (object && cJSON_IsString(id) && cJSON_AddStringToObject(object, "item_id", id->valuestring) &&
            cJSON_AddNumberToObject(object, "version", cJSON_IsNumber(version) ? version->valuedouble : 0)) {
            workshop_event("item-changed", object);
        } else {
            cJSON_Delete(object);
        }
    }
    if (strcmp(command, "item.delete") == 0) {
        event_with_id("item-removed", "item_id", target);
    } else if (strncmp(command, "library.", 8) == 0) {
        event_with_id("library-changed", "piece_id", target);
    } else if (strncmp(command, "collection.", 11) == 0) {
        event_with_id("collection-changed", "collection_id", target);
    } else if (strncmp(command, "subject.", 8) == 0) {
        event_with_id("subject-changed", "subject_id", target);
    } else if (strncmp(command, "style.", 6) == 0) {
        event_with_id("style-changed", "style_id", target);
    } else if (strncmp(command, "links.", 6) == 0) {
        event_with_id("links-changed", "run_id", target);
    } else if (strncmp(command, "settings.", 9) == 0) {
        workshop_event("settings-changed", cJSON_CreateObject());
    }
}
