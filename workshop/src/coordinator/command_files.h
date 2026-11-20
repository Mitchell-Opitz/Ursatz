#ifndef URSATZ_WORKSHOP_COORDINATOR_COMMAND_FILES_H
#define URSATZ_WORKSHOP_COORDINATOR_COMMAND_FILES_H

#include <stddef.h>

/* The files of workshop/commands/ as the build embedded them (cmake/embed_json.py): one per command family, sorted by path. The last byte
 * after `size` is a NUL. command_schemas.c parses them once. */
typedef struct {
    const char *path;
    const char *data;
    size_t size;
} WorkshopCommandFile;

extern const WorkshopCommandFile workshop_command_files[];
extern const size_t workshop_command_file_count;

#endif
