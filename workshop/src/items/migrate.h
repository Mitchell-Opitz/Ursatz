#ifndef WORKSHOP_ITEMS_MIGRATE_H
#define WORKSHOP_ITEMS_MIGRATE_H

/*
 * The migration of the GUI's data folder (layout 0) to layout 1 is workshop_migrate in <ursatz_workshop/workshop_api.h>. These two
 * questions are the open's: workshop_open refuses a folder with GUI entries and no data-folder.json, and makes an empty folder layout 1.
 */

#include <stdbool.h>

#include <common/status.h>

/* True when the folder holds a top-level entry of the GUI's layout (pieces/, materials/, subjects/, collections/, styles/, links/). */
bool workshop_data_folder_has_old_entries(const char *data_dir);

/* *layout is 1 when data-folder.json is there and readable (a newer version or another format is FORMAT naming the file), else 0. */
UrsatzStatus workshop_data_folder_layout(const char *data_dir, int *layout, UrsatzError *error);

#endif
