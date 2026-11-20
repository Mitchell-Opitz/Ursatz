#ifndef URSATZ_WORKSHOP_COMPOSITION_RESTORE_H
#define URSATZ_WORKSHOP_COMPOSITION_RESTORE_H

#include <common/status.h>

/*
 * Restore a version (R-667): the next version of item id is a copy of version `version`, which must be older than the current one. Every file of the
 * old version folder but its envelope is copied into a stage, the paths of the body's `result` are rebased from the old folder to the new one, and
 * the stage becomes the new version's folder through workshop_material_item_add_version_restored: the uses stay as they were in the old version and
 * the envelope's lineage says `restored_from`. `note` is the version note (NULL: "restore v<version>"). *out_number is the number of the new version.
 *
 * `version` the current one is URSATZ_ERR_REQUEST at "/params/version", "version <n> is the current one"; a version that is not there is
 * URSATZ_ERR_NOT_FOUND at "/params/version", "no version <n>"; an item that is not there is NOT_FOUND at "/target".
 */
UrsatzStatus workshop_material_item_restore(const char *id, int version, const char *note, int *out_number, UrsatzError *error);

#endif
