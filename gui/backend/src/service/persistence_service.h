#ifndef URSATZ_GUI_PERSISTENCE_SERVICE_H
#define URSATZ_GUI_PERSISTENCE_SERVICE_H

#include <stdbool.h>

/*
 * Shared persistence/query service.
 *
 * This is the ONLY place backend code is allowed to call into Ursatz's
 * persistence layer. Tool route handlers call through this service; they
 * never open or query the Ursatz store directly. This keeps tool modules
 * decoupled from persistence details and keeps the store access path in
 * one auditable place.
 */

/* Opens the configured Ursatz store and confirms it is reachable.
 * Returns true on success. On success, *out_message is a short
 * human-readable status string owned by the caller (must be freed). */
bool ursatz_gui_persistence_check(const char *db_path, char **out_message);

#endif /* URSATZ_GUI_PERSISTENCE_SERVICE_H */
