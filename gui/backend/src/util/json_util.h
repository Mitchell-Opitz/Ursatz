#ifndef URSATZ_GUI_JSON_UTIL_H
#define URSATZ_GUI_JSON_UTIL_H

#include <stdbool.h>

#include <cJSON.h>

/* The small cJSON helpers the services share. */

/* The string member name of object, or "" when it is missing or not a string. Owned by object. */
const char *ursatz_gui_json_string(const cJSON *object, const char *name);

/* A malloc'd copy of the string member name of object, "" when it is missing or not a string; NULL on allocation
 * failure. */
char *ursatz_gui_json_string_copy(const cJSON *object, const char *name);

/* Replaces the member name of object with the string value. false on allocation failure. */
bool ursatz_gui_json_set_string(cJSON *object, const char *name, const char *value);

/* Adds child to parent as name, or frees it. false on allocation failure. */
bool ursatz_gui_json_attach(cJSON *parent, const char *name, cJSON *child);

/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
bool ursatz_gui_json_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name);

/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
bool ursatz_gui_json_add_copy_if(cJSON *dst, const cJSON *src, const char *name);

#endif
