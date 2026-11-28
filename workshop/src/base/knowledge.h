#ifndef URSATZ_WORKSHOP_COORDINATOR_KNOWLEDGE_H
#define URSATZ_WORKSHOP_COORDINATOR_KNOWLEDGE_H

#include <stdbool.h>

#include <common/status.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/composer_api.h>

/*
 * The one knowledge snapshot of the workshop (the embedded entries, no user files), built when the workshop opens and freed when it
 * closes. The services that run the Analyzer pass it on (analysis, material export); it outlives every analysis made from it. The
 * snapshot is read only, so the worker threads share it.
 */

/* Builds the snapshot; true when it is built (also when it already was). False with a message on stderr when the knowledge cannot be
   read. workshop_open calls it; so does a test that uses the services without opening a workshop. */
bool workshop_knowledge_init(void);

/* As workshop_knowledge_init, with every .json file of the data folder's knowledge folder as a user file, in name order (a folder with none, or none
   at all, adds nothing; the files are read, never written). False with a message on stderr when a file cannot be read or a user entry is
   broken: the message names the file, the entry and the field. NULL `data_dir` is workshop_knowledge_init. */
bool workshop_knowledge_init_folder(const char *data_dir);

/* Builds the snapshot again from the embedded entries and the files of the data folder's knowledge folder as they are now, and swaps it in (the Composer's
   context follows). The snapshot it replaces is kept until the shutdown, since a running job may hold it. URSATZ_ERR_INTERNAL when the workshop has no data folder
   or the files no longer load (the snapshot is then as it was); the commands that write her entries check the candidate first, so this does not happen after
   a good commit. */
UrsatzStatus workshop_knowledge_rebuild(void);

/* Frees the snapshot. A no-op when there is none. */
void workshop_knowledge_shutdown(void);

/* The snapshot, or NULL before workshop_knowledge_init and after the shutdown. */
const KnowledgeSnapshot *workshop_knowledge_snapshot(void);

/* The call context every Composer entry takes (composer_api.h): over the same snapshot, valid as long as it. Before the init and after
   the shutdown it holds no snapshot, so an entry refuses. */
const ComposerContext *workshop_composer_context_global(void);

/* Whether `document_text` (a knowledge document) loads beside the embedded entries and the data folder's knowledge files (NULL `data_dir`: none): a
   snapshot is built and freed, the running one untouched. False with the loader's message, which names the document as "file". */
bool workshop_knowledge_check(const char *data_dir, const char *document_text, char *message, size_t message_size);

#endif
