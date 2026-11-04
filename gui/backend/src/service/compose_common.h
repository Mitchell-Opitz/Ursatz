#ifndef URSATZ_GUI_COMPOSE_COMMON_H
#define URSATZ_GUI_COMPOSE_COMMON_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "theme_service.h"

/* What the theme and the piece service share when they compose an item into a new version: the small JSON helpers and
 * the steps around the Composer run. The caller holds its own compose lock. */

/* Copies text into message when there is room for it. */
void ursatz_gui_compose_say(char *message, size_t size, const char *text);

/* The string member name of object, or "" when it is missing or not a string. */
const char *ursatz_gui_compose_text_of(const cJSON *object, const char *name);

/* Adds child to parent as name, or frees it. false on allocation failure. */
bool ursatz_gui_compose_attach(cJSON *parent, const char *name, cJSON *child);

/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
bool ursatz_gui_compose_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name);

/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
bool ursatz_gui_compose_add_copy_if(cJSON *dst, const cJSON *src, const char *name);

/* What differs between composing a theme and composing a piece. */
typedef struct {
    const char *member;         /* the body's member and the item type: "theme" or "piece" */
    const char *not_kind_text;  /* the reason when the item has another type */
    /* True when the member can be composed; else it says why in message. */
    bool (*composable)(const cJSON *member, bool has_brief, char *message, size_t message_size);
    /* Builds the Composer request from the body; style_path is the brief's style file or NULL. */
    UrsatzGuiThemeResult (*build_request)(const cJSON *body, const char *dir, const char *style_path, cJSON **out_request,
                                          char *message, size_t message_size);
    /* Runs the Composer in dir (the request is already there). */
    UrsatzGuiThemeResult (*run)(const char *dir, const cJSON *member, bool use_seed, long long seed, cJSON **out_summary,
                                char *message, size_t message_size);
    /* The note of the new version. */
    void (*note)(const cJSON *summary, char *note, size_t note_size);
    /* The body of the new version; takes ownership of summary and brief_summary. */
    cJSON *(*body_with_result)(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief);
} UrsatzGuiComposeKind;

/*
 * Composes the item id into a new version: updates the uses (when update_uses), reads the item, makes the version
 * folder, builds the brief and the request, runs the Composer, adds the version and moves the folder when another
 * writer added a version meanwhile. On OK *out_item is the new item.json (caller frees); otherwise no folder is left.
 */
UrsatzGuiThemeResult ursatz_gui_compose_item(const UrsatzGuiComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, cJSON **out_item, char *message,
                                             size_t message_size);

#endif
