#include "piece_lock.h"

#include "util/store_lock.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct PieceLockEntry {
    struct PieceLockEntry *next;
    char *id;
    UrsatzGuiLock mutex;
    unsigned refs;
} PieceLockEntry;

static UrsatzGuiLock g_table_lock = URSATZ_GUI_LOCK_INIT;
static PieceLockEntry *g_entries = NULL;

static PieceLockEntry *find_entry(const char *id) {
    for (PieceLockEntry *entry = g_entries; entry; entry = entry->next) {
        if (strcmp(entry->id, id) == 0) {
            return entry;
        }
    }
    return NULL;
}

bool ursatz_gui_piece_lock(const char *id) {
    if (!id) {
        return false;
    }
    ursatz_gui_lock_acquire(&g_table_lock);
    PieceLockEntry *entry = find_entry(id);
    if (!entry) {
        entry = calloc(1, sizeof(PieceLockEntry));
        size_t length = strlen(id) + 1;
        char *copy = entry ? malloc(length) : NULL;
        if (!copy) {
            free(entry);
            ursatz_gui_lock_release(&g_table_lock);
            return false;
        }
        memcpy(copy, id, length);
        entry->id = copy;
        entry->mutex = (UrsatzGuiLock)URSATZ_GUI_LOCK_INIT;
        entry->next = g_entries;
        g_entries = entry;
    }
    entry->refs++;
    ursatz_gui_lock_release(&g_table_lock);
    ursatz_gui_lock_acquire(&entry->mutex);
    return true;
}

void ursatz_gui_piece_unlock(const char *id) {
    if (!id) {
        return;
    }
    ursatz_gui_lock_acquire(&g_table_lock);
    PieceLockEntry **link = &g_entries;
    while (*link && strcmp((*link)->id, id) != 0) {
        link = &(*link)->next;
    }
    PieceLockEntry *entry = *link;
    if (entry) {
        ursatz_gui_lock_release(&entry->mutex);
        if (--entry->refs == 0) {
            *link = entry->next;
            free(entry->id);
            free(entry);
        }
    }
    ursatz_gui_lock_release(&g_table_lock);
}

unsigned ursatz_gui_piece_lock_entries(void) {
    unsigned count = 0;
    ursatz_gui_lock_acquire(&g_table_lock);
    for (PieceLockEntry *entry = g_entries; entry; entry = entry->next) {
        count++;
    }
    ursatz_gui_lock_release(&g_table_lock);
    return count;
}
