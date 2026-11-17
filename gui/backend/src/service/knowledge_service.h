#ifndef URSATZ_GUI_KNOWLEDGE_SERVICE_H
#define URSATZ_GUI_KNOWLEDGE_SERVICE_H

#include <stdbool.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/composer_api.h>

/*
 * Knowledge service: the one knowledge snapshot of the backend (the embedded entries, no user files), built when the server starts and
 * freed when it stops. The services that run the Analyzer pass it on (analysis_service.c, material_export_service.c); it outlives
 * every analysis made from it. The snapshot is read only, so the worker threads share it.
 */

/* Builds the snapshot; true when it is built (also when it already was). False with a message on stderr when the knowledge cannot be
   read. Called once before the first request. */
bool ursatz_gui_knowledge_init(void);

/* Frees the snapshot. A no-op when there is none. Called after the last request is answered. */
void ursatz_gui_knowledge_shutdown(void);

/* The snapshot, or NULL before ursatz_gui_knowledge_init and after the shutdown. */
const KnowledgeSnapshot *ursatz_gui_knowledge(void);

/* The call context every Composer entry takes (composer_api.h): over the same snapshot, valid as long as it. Before the init and after
   the shutdown it holds no snapshot, so an entry refuses. */
const ComposerContext *ursatz_gui_composer_context(void);

#endif
