#ifndef WORKSHOP_ITEMS_LAYERS_H
#define WORKSHOP_ITEMS_LAYERS_H

/*
 * The layers beside a piece or a composed version (R-644, R-530): realization.json (the work as the Analyzer or the Composer wrote it,
 * library/realization/realization_json.h) and readings/<name>.json (what one reader says about it, library/reading/reading_json.h), whose target
 * names the realization by its digest. The transaction reads both before it commits (items/txn.h) and the answers describe them.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

#define WORKSHOP_FILE_REALIZATION "realization.json"
#define WORKSHOP_DIR_READINGS "readings"

/* True when a file at final_path is a layer file: realization.json, or a .json file in a readings folder. */
bool workshop_layers_is_realization_path(const char *final_path);
bool workshop_layers_is_reading_path(const char *final_path);

/* Reads the realization file at actual with realization_work_from_json: FORMAT naming final_path when it does not read. */
UrsatzStatus workshop_layers_check_realization(const char *actual, const char *final_path, UrsatzError *error);

/* Reads the reading file at actual with reading_from_json and checks its target digest against the realization file at realization_actual
 * (which must read): FORMAT naming final_path otherwise. */
UrsatzStatus workshop_layers_check_reading(const char *actual, const char *final_path, const char *realization_actual, UrsatzError *error);

/*
 * The layers of the folder as an answer: {"realization": {"digest", "movements"}, "readings": [{"source", "name"}]} (the readings in name order,
 * source the kind of the reading's source and name its source name), or JSON null when the folder holds no realization. *out is caller-owned.
 */
UrsatzStatus workshop_layers_describe(const char *folder, cJSON **out);

#endif
