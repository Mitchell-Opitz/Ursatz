#ifndef URSATZ_ANALYSIS_NOTES_NOTE_GROUP_H
#define URSATZ_ANALYSIS_NOTES_NOTE_GROUP_H

#include <stddef.h>

#include "model/structure/structure.h"

/*
 * A Structure shell whose event refs are the given note ids (duplicates removed) under the id `shell_id`; the shell's type is
 * "<framework_name>/note-group". The stages that read a group of notes back (a theme's soprano notes, a hemiola's notes) hold one.
 * NULL on a NULL or empty argument, an empty note id or allocation failure. Nothing the caller passed is retained or freed.
 */
Structure *analysis_note_group(const char *framework_name, const char *shell_id, const char *const *note_ids, size_t count);

#endif
