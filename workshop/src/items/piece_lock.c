#include "piece_lock.h"

#include "lock.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct PieceLockEntry {
    struct PieceLockEntry *next;
    char *id;
    WorkshopLock mutex;
    unsigned refs;
} PieceLockEntry;

static WorkshopLock g_table_lock = WORKSHOP_LOCK_INIT;
static PieceLockEntry *g_entries = NULL;

static PieceLockEntry *find_entry(const char *id) {
    for (PieceLockEntry *entry = g_entries; entry; entry = entry->next) {
        if (strcmp(entry->id, id) == 0) {
            return entry;
        }
    }
    return NULL;
}

bool workshop_piece_lock(const char *id) {
    if (!id) {
        return false;
    }
    workshop_lock_acquire(&g_table_lock);
    PieceLockEntry *entry = find_entry(id);
    if (!entry) {
        entry = calloc(1, sizeof(PieceLockEntry));
        size_t length = strlen(id) + 1;
        char *copy = entry ? malloc(length) : NULL;
        if (!copy) {
            free(entry);
            workshop_lock_release(&g_table_lock);
            return false;
        }
        memcpy(copy, id, length);
        entry->id = copy;
        entry->mutex = (WorkshopLock)WORKSHOP_LOCK_INIT;
        entry->next = g_entries;
        g_entries = entry;
    }
    entry->refs++;
    workshop_lock_release(&g_table_lock);
    workshop_lock_acquire(&entry->mutex);
    return true;
}

void workshop_piece_unlock(const char *id) {
    if (!id) {
        return;
    }
    workshop_lock_acquire(&g_table_lock);
    PieceLockEntry **link = &g_entries;
    while (*link && strcmp((*link)->id, id) != 0) {
        link = &(*link)->next;
    }
    PieceLockEntry *entry = *link;
    if (entry) {
        workshop_lock_release(&entry->mutex);
        if (--entry->refs == 0) {
            *link = entry->next;
            free(entry->id);
            free(entry);
        }
    }
    workshop_lock_release(&g_table_lock);
}

unsigned workshop_piece_lock_entries(void) {
    unsigned count = 0;
    workshop_lock_acquire(&g_table_lock);
    for (PieceLockEntry *entry = g_entries; entry; entry = entry->next) {
        count++;
    }
    workshop_lock_release(&g_table_lock);
    return count;
}
