#ifndef WORKSHOP_ITEMS_STORE_INTERNAL_H
#define WORKSHOP_ITEMS_STORE_INTERNAL_H

/*
 * What the files of the material store share (store.c, references.c, body_rules.c, body_migrate.c, op_checks.c, version_files.c).
 * Callers outside the store include store.h.
 */

#include "store.h"
#include "txn.h"
#include "base/last_error.h"
#include "lock.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#define NAME_MAX_CHARS 200
#define NOTE_MAX_CHARS 500
#define DESCRIPTION_MAX_CHARS 2000
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400
#define PIECE_ID_MAX 15

/* Whether the store is ready, and the one lock every operation runs under (store.c). */
extern bool store_ready;
extern WorkshopLock store_lock;
/* Calls the forget hook (workshop_store_set_forget_item) for item_id, if there is one: it stages what the item's delete drops from the
 * subjects into txn. */
void store_forget_item(WorkshopTxn *txn, const char *item_id);

/* Begins the transaction of a writer (before the store's lock, which is taken inside it); NULL when the journal cannot be read. */
WorkshopTxn *store_txn_begin(const char *command, const char *target);

/* Commits the transaction when result is OK and aborts it otherwise; the status to answer (a failed commit is INTERNAL). */
UrsatzStatus store_txn_end(WorkshopTxn *txn, UrsatzStatus result);

#define LOCK() workshop_lock_acquire(&store_lock)
#define UNLOCK() workshop_lock_release(&store_lock)

bool store_is_valid_id(const char *id);

/* The folder of an item that exists (found across the projects), its file, and the folder of one of its versions; false for an id that
 * is not an item. */
bool store_item_dir_path(const char *id, char *out, size_t out_size);

bool store_item_file_path(const char *id, const char *file, char *out, size_t out_size);

bool store_version_dir_path(const char *id, int number, char *out, size_t out_size);

/* The project an item is in (the folder it is under). */
bool store_find_item_project(const char *id, char *project, size_t project_size);

/* Adds the id of every item, across the projects, to out (sorted). */
UrsatzStatus store_item_ids_locked(CommonNames *out);

UrsatzStatus store_invalid_body(UrsatzError *error, const char *message);

/* A member that is absent or null. */
bool store_is_absent_or_null(const cJSON *object, const char *name);

/* True when the body of a theme or a piece is a placeholder: its request is absent or has no member (a new item). */
bool store_request_is_placeholder(const cJSON *body);

/* An id like s0, s12 or end. */
bool store_piece_section_id_ok(const char *id);

UrsatzStatus store_load_item_locked(const char *id, cJSON **out);

int store_current_version_of(const cJSON *item);

UrsatzStatus store_read_version_locked(const char *id, int number, cJSON **out_body);

/* prepare_uses_locked, then fill_uses_locked: *out is body with the uses a theme's slots imply and a fresh copy in each. */
UrsatzStatus store_fill_body_uses_locked(const char *self_id, const cJSON *body, cJSON **out);

/* Sets "variant_of" on an item object: the source item of a variant, else null. The member is added to answers only;
 * item.json never carries it. known_body is the item's current version when the caller has it, else NULL and a motif
 * reads it. */
bool store_annotate_variant_of_locked(cJSON *item, const cJSON *known_body);

/* Stages body as the next version of item and its header with it made current, in txn; stage (NULL for none) is a folder of the version's
 * files that becomes the version folder. */
UrsatzStatus store_commit_version_locked(WorkshopTxn *txn, const char *id, cJSON *item, const cJSON *body, const char *note, const char *stage);

const char *store_prefix_for_type(const char *type);

#endif
