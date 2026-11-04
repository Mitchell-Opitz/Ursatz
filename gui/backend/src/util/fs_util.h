#ifndef URSATZ_GUI_FS_UTIL_H
#define URSATZ_GUI_FS_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/* A malloc'd copy of value (caller frees); NULL when value is NULL or memory is out. */
char *ursatz_gui_dup_string(const char *value);

/* The current UTC time as "YYYY-MM-DDTHH:MM:SSZ". */
void ursatz_gui_iso_utc_now(char *out, size_t out_size);

/* True when path is an existing directory. */
bool ursatz_gui_path_is_dir(const char *path);

/* Creates one directory; true only when this call created it. */
bool ursatz_gui_make_dir(const char *path);

/* Creates the directory when it is absent; true when it exists afterwards. */
bool ursatz_gui_ensure_dir(const char *path);

/* Writes data to "<path>.tmp", then renames it over path. */
bool ursatz_gui_write_file_atomic(const char *path, const void *data, size_t size);

/* The whole file as a NUL-terminated buffer (caller frees), or NULL. *size_or_null gets the byte count. */
char *ursatz_gui_read_file(const char *path, size_t *size_or_null);

/* Parses the JSON object in path (caller frees), or NULL when absent or not an object. */
cJSON *ursatz_gui_read_json_object(const char *path);

#endif
