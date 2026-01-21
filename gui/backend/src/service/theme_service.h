#ifndef URSATZ_GUI_THEME_SERVICE_H
#define URSATZ_GUI_THEME_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * Theme service: composes a theme item into a new version with the Composer's theme command (R-166, R-167).
 *
 * A theme version keeps its files beside its body, in the material store's items/<id>/v<n>/ (see
 * material_service.h): request.json (what the Composer read), brief.style.json (when the theme has a brief),
 * plan.json, theme.mid and theme.musicxml. The Composer runs through composer_service.c and nothing here changes
 * the working directory: every path handed to it is absolute.
 */

typedef enum {
    URSATZ_GUI_THEME_OK = 0,
    URSATZ_GUI_THEME_ERR_NOT_FOUND,      /* no such item (404) */
    URSATZ_GUI_THEME_ERR_INVALID,        /* not a theme, not composable, a bad option or a Composer usage error (400) */
    URSATZ_GUI_THEME_ERR_UNPROCESSABLE,  /* the Composer is stuck, or the brief names nothing it can use (422) */
    URSATZ_GUI_THEME_ERR_NO_BINARY,      /* the Composer binary is missing (503) */
    URSATZ_GUI_THEME_ERR_TIMEOUT,        /* the Composer took too long (504) */
    URSATZ_GUI_THEME_ERR_FAILED,         /* any other failure (500) */
    URSATZ_GUI_THEME_ERR_ALLOC
} UrsatzGuiThemeResult;

/*
 * Composes the theme item id into a new version.
 *
 * The request is built from the current version: each slot's
 * motif from its use's copy, the pool of its saved forms (a motif item whose derived chain, followed at most 32 steps
 * through derived.source_item, leads to the slot's item; flag approved, and unflagged with the default pool; at most
 * 64 for each motive; rejected forms never), and the brief built into the version's folder as brief.style.json.
 * seed (when has_seed) wins over the body's seed.
 *
 * On OK, *out_item is the new item.json, caller-owned. A new version holds the body as it was plus
 * result {seed, summary (the command's JSON), brief {piece_count, skipped}, files [names]}, with the note
 * "composed seed <n>". On any other result no version is written and no folder is left; message (when not NULL)
 * receives a one-line reason.
 */
UrsatzGuiThemeResult ursatz_gui_theme_compose(const char *id, bool has_seed, long long seed, cJSON **out_item,
                                              char *message, size_t message_size);

#endif /* URSATZ_GUI_THEME_SERVICE_H */
