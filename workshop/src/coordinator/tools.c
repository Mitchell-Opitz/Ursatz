#include <string.h>

#include "coordinator/dispatch.h"

/* The tools (R-656): the calls that work on the files their params name and never on the data folder or the journal. dispatch.c lists them. */

/* The versions this build holds: what workshop_status answers, without the data folder. */
WORKSHOP_HANDLER(workshop_t_versions) {
    cJSON *status = NULL;
    UrsatzStatus result = workshop_status(call->workshop, &status, error);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(status, "data_dir");
    return workshop_reply_tree(reply, status, error);
}
