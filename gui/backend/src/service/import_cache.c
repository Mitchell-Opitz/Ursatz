#include "import_cache.h"

#include "util/store_lock.h"

#include <stdlib.h>
#include <string.h>

#define IMPORT_CACHE_MAX 8

struct UrsatzGuiImportEntry {
    struct UrsatzGuiImportEntry *next;
    char *piece_id;
    char *version;
    ImportedNotes notes;
    char *xml;
    bool meter_defaulted;
    unsigned long used;
    unsigned refs;
    bool stale;
};

static UrsatzGuiLock g_lock = URSATZ_GUI_LOCK_INIT;
static UrsatzGuiImportEntry *g_entries = NULL;
static unsigned g_count = 0;
static unsigned long g_tick = 0;
static unsigned g_imports = 0;

static void entry_free(UrsatzGuiImportEntry *entry) {
    imported_notes_destroy(&entry->notes);
    free(entry->piece_id);
    free(entry->version);
    free(entry->xml);
    free(entry);
}

/* Unlinks entry from the table; frees it unless somebody holds it (then release frees it). */
static void entry_drop(UrsatzGuiImportEntry *entry) {
    for (UrsatzGuiImportEntry **link = &g_entries; *link; link = &(*link)->next) {
        if (*link == entry) {
            *link = entry->next;
            g_count--;
            break;
        }
    }
    if (entry->refs == 0) {
        entry_free(entry);
    } else {
        entry->stale = true;
    }
}

static UrsatzGuiImportEntry *find_entry(const char *piece_id) {
    for (UrsatzGuiImportEntry *entry = g_entries; entry; entry = entry->next) {
        if (strcmp(entry->piece_id, piece_id) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void evict_to_fit(void) {
    while (g_count >= IMPORT_CACHE_MAX) {
        UrsatzGuiImportEntry *oldest = NULL;
        for (UrsatzGuiImportEntry *entry = g_entries; entry; entry = entry->next) {
            if (entry->refs == 0 && (!oldest || entry->used < oldest->used)) {
                oldest = entry;
            }
        }
        if (!oldest) {
            return;
        }
        entry_drop(oldest);
    }
}

static char *copy_text(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) {
        memcpy(copy, text, length);
    }
    return copy;
}

UrsatzGuiImportEntry *ursatz_gui_import_cache_acquire(const char *piece_id, const char *version,
                                                       UrsatzGuiImportFn import, void *context,
                                                       bool *out_import_failed) {
    if (out_import_failed) {
        *out_import_failed = false;
    }
    ursatz_gui_lock_acquire(&g_lock);
    UrsatzGuiImportEntry *entry = find_entry(piece_id);
    if (entry && strcmp(entry->version, version) == 0) {
        entry->refs++;
        entry->used = ++g_tick;
        ursatz_gui_lock_release(&g_lock);
        return entry;
    }
    if (entry) {
        entry_drop(entry);
    }
    ursatz_gui_lock_release(&g_lock);

    /* The import runs outside the table lock: the caller's piece lock keeps anyone else off this piece. */
    entry = calloc(1, sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    entry->piece_id = copy_text(piece_id);
    entry->version = copy_text(version);
    if (!entry->piece_id || !entry->version) {
        free(entry->piece_id);
        free(entry->version);
        free(entry);
        return NULL;
    }
    ursatz_gui_lock_acquire(&g_lock);
    g_imports++;
    ursatz_gui_lock_release(&g_lock);
    if (!import(context, &entry->notes)) {
        free(entry->piece_id);
        free(entry->version);
        free(entry);
        if (out_import_failed) {
            *out_import_failed = true;
        }
        return NULL;
    }

    ursatz_gui_lock_acquire(&g_lock);
    evict_to_fit();
    entry->refs = 1;
    entry->used = ++g_tick;
    entry->next = g_entries;
    g_entries = entry;
    g_count++;
    ursatz_gui_lock_release(&g_lock);
    return entry;
}

void ursatz_gui_import_cache_release(UrsatzGuiImportEntry *entry) {
    if (!entry) {
        return;
    }
    ursatz_gui_lock_acquire(&g_lock);
    entry->refs--;
    bool free_now = entry->stale && entry->refs == 0;
    ursatz_gui_lock_release(&g_lock);
    if (free_now) {
        entry_free(entry);
    }
}

const ImportedNotes *ursatz_gui_import_entry_notes(const UrsatzGuiImportEntry *entry) {
    return &entry->notes;
}

const char *ursatz_gui_import_entry_score(const UrsatzGuiImportEntry *entry, bool *out_meter_defaulted) {
    if (entry->xml && out_meter_defaulted) {
        *out_meter_defaulted = entry->meter_defaulted;
    }
    return entry->xml;
}

void ursatz_gui_import_entry_set_score(UrsatzGuiImportEntry *entry, char *xml, bool meter_defaulted) {
    free(entry->xml);
    entry->xml = xml;
    entry->meter_defaulted = meter_defaulted;
}

void ursatz_gui_import_cache_invalidate(const char *piece_id) {
    ursatz_gui_lock_acquire(&g_lock);
    UrsatzGuiImportEntry *entry = find_entry(piece_id);
    if (entry) {
        entry_drop(entry);
    }
    ursatz_gui_lock_release(&g_lock);
}

unsigned ursatz_gui_import_cache_imports(void) {
    ursatz_gui_lock_acquire(&g_lock);
    unsigned count = g_imports;
    ursatz_gui_lock_release(&g_lock);
    return count;
}

unsigned ursatz_gui_import_cache_entries(void) {
    ursatz_gui_lock_acquire(&g_lock);
    unsigned count = g_count;
    ursatz_gui_lock_release(&g_lock);
    return count;
}

void ursatz_gui_import_cache_reset(void) {
    ursatz_gui_lock_acquire(&g_lock);
    while (g_entries) {
        entry_drop(g_entries);
    }
    g_imports = 0;
    ursatz_gui_lock_release(&g_lock);
}
