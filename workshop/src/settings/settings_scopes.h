#ifndef WORKSHOP_SETTINGS_SCOPES_H
#define WORKSHOP_SETTINGS_SCOPES_H

/*
 * The scope files of the settings (R-671, R-676): a taste profile (settings/taste/<name>.json), a style's, a project's and an item's
 * (format ursatz-settings). A scope is named "taste:<name>", "style:<id>", "project:<id>" or "item:<id>"; an item's file is in its
 * project's folder, so the project is looked up. A missing file is an empty scope: {scope, values: {}}.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

#include "items/txn.h"

typedef enum { WORKSHOP_SCOPE_TASTE, WORKSHOP_SCOPE_STYLE, WORKSHOP_SCOPE_PROJECT, WORKSHOP_SCOPE_ITEM } WorkshopScopeKind;

/* Splits "kind:id": the kind and the id (at most size - 1 bytes). False for text that is none or an id the layout refuses. */
bool workshop_scope_parse(const char *scope, WorkshopScopeKind *kind, char *id, size_t size);

/* The path (relative to the data folder) of the scope's file. NOT_FOUND when the style, project or item the scope names does not exist
 * (a taste profile need not); REQUEST for text that is no scope. */
UrsatzStatus workshop_scope_path(const char *scope, char *path, size_t size, UrsatzError *error);

/* The scope's file, stripped of its format members, or the empty scope when the file is absent. *out is the caller's. */
UrsatzStatus workshop_scope_read(const char *scope, cJSON **out, UrsatzError *error);

/*
 * The rules of a scope file (R-676): overrides and off only in a project or item file, `project` in off only in an item file, taste never
 * in a taste file, kept only in a taste file. REQUEST at where (the path of the member) otherwise.
 */
UrsatzStatus workshop_scope_check(const cJSON *file, WorkshopScopeKind kind, const char *where, UrsatzError *error);

/* Checks file (the scope's content, stripped) and stages it as the scope's file in the open transaction. */
UrsatzStatus workshop_scope_stage(WorkshopTxn *txn, const char *scope, cJSON *file, UrsatzError *error);

/* {tastes: [names], styles: [ids], projects: [ids], items: [ids]} of the scope files present, each list sorted. */
UrsatzStatus workshop_scope_list(cJSON **out, UrsatzError *error);

#endif
