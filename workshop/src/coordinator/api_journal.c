#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"
#include "items/journal.h"

/* The journal query (R-646, R-658): what the commands changed, newest first. */

/* A copy of the "final" member of each entry of the array `name` of record, as an array of paths. */
static cJSON *final_paths(const cJSON *record, const char *name) {
    cJSON *paths = cJSON_CreateArray();
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(record, name)) {
        const cJSON *path = cJSON_GetObjectItemCaseSensitive(entry, "final");
        cJSON *copy = cJSON_IsString(path) && paths ? cJSON_CreateString(path->valuestring) : NULL;
        if (copy) {
            cJSON_AddItemToArray(paths, copy);
        }
    }
    return paths;
}

/* {seq, id, command, actor, at, target, base_version, writes: [<final path>], removes: [<final path>]}: the record without its headers. */
static cJSON *summary(const cJSON *record) {
    static const char *const kMembers[] = {"seq", "id", "command", "actor", "at", "target", "base_version"};
    cJSON *object = cJSON_CreateObject();
    bool ok = object != NULL;
    for (size_t i = 0; ok && i < sizeof(kMembers) / sizeof(kMembers[0]); i++) {
        const cJSON *member = cJSON_GetObjectItemCaseSensitive(record, kMembers[i]);
        cJSON *copy = member ? cJSON_Duplicate(member, true) : cJSON_CreateNull();
        ok = copy && cJSON_AddItemToObject(object, kMembers[i], copy);
        if (!ok) {
            cJSON_Delete(copy);
        }
    }
    cJSON *writes = ok ? final_paths(record, "writes") : NULL;
    cJSON *removes = ok ? final_paths(record, "removes") : NULL;
    if (!ok || !writes || !removes || !cJSON_AddItemToObject(object, "writes", writes) || !cJSON_AddItemToObject(object, "removes", removes)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* {"records": [...]} for params target (a record's target, optional) and limit (1 to 1000, default 20). */
WORKSHOP_HANDLER(workshop_q_journal_list) {
    bool bad = false;
    const char *target = workshop_param_string(call->params, "target", &bad);
    if (bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/target", "target is a text");
    }
    long limit = 20;
    const cJSON *limit_item = cJSON_GetObjectItemCaseSensitive(call->params, "limit");
    if (limit_item && !cJSON_IsNull(limit_item)) {
        if (!cJSON_IsNumber(limit_item) || limit_item->valuedouble < 1 || limit_item->valuedouble > 1000 ||
            limit_item->valuedouble != (double)(long)limit_item->valuedouble) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/limit", "limit is a whole number from 1 to 1000");
        }
        limit = (long)limit_item->valuedouble;
    }
    WorkshopJournal journal;
    UrsatzError read_error = {{0}, {0}};
    if (workshop_journal_read(WORKSHOP_JOURNAL_FILE, &journal, &read_error) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_FORMAT, "", "%s", read_error.message);
    }
    cJSON *records = cJSON_CreateArray();
    bool ok = records != NULL;
    long taken = 0;
    for (size_t i = journal.count; ok && i > 0 && taken < limit; i--) {
        const cJSON *record = journal.records[i - 1];
        const cJSON *record_target = cJSON_GetObjectItemCaseSensitive(record, "target");
        if (target && !(cJSON_IsString(record_target) && strcmp(record_target->valuestring, target) == 0)) {
            continue;
        }
        cJSON *item = summary(record);
        ok = item != NULL;
        if (ok) {
            cJSON_AddItemToArray(records, item);
            taken++;
        }
    }
    workshop_journal_free(&journal);
    if (!ok) {
        cJSON_Delete(records);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_member(reply, "records", records, error);
}
