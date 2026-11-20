#ifndef WORKSHOP_ITEMS_MIGRATE_H
#define WORKSHOP_ITEMS_MIGRATE_H

/*
 * The migration of the GUI's data folder (layout 0) to layout 1 is workshop_migrate in <ursatz_workshop/workshop_api.h>. What
 * workshop_open asks of the folder is below.
 */

#include <stdbool.h>

#include <common/status.h>

/* *layout is the format_version of data-folder.json (1 or 2) when it is there and readable (a newer version or another format is FORMAT naming the file), else 0. */
UrsatzStatus workshop_data_folder_layout(const char *data_dir, int *layout, UrsatzError *error);

/*
 * What workshop_open asks of the folder: a current layout folder passes, an empty one gets its data-folder.json, and one with GUI entries (or the
 * rest of an interrupted migration) and no data-folder.json is FORMAT: "the data folder <path> is in the GUI's old layout; back it up and
 * migrate it: ursatz --data <path> migrate --dry-run, then ursatz --data <path> migrate".
 */
UrsatzStatus workshop_data_folder_prepare(const char *data_dir, UrsatzError *error);

#endif
