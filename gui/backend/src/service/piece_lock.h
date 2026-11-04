#ifndef URSATZ_GUI_PIECE_LOCK_H
#define URSATZ_GUI_PIECE_LOCK_H

#include <stdbool.h>

/*
 * One lock per piece directory (pieces/<id>/). Every writer of a piece and
 * every read that must not see half a write holds it:
 *   upload (import_and_save), PATCH (update_piece: metadata, re-analyze and
 *   undo as one hold), re-analyze (also the style build's and the link
 *   worker's), delete (the directory removal), and the reads of analysis.json
 *   plus the source (detail, musicxml, playback).
 * All of these are in analysis_service.c and take the lock themselves, so a
 * caller never holds it.
 *
 * Lock order:
 *   1. a store lock (style, link, collection, subject), if one is held;
 *   2. then at most one piece lock;
 *   never the reverse (no store lock is taken while a piece lock is held, and
 *   the collection and subject cleanups of a delete run after the piece lock
 *   is released), and never two piece locks.
 * The piece lock is not recursive: code under it calls only the unlocked
 * helpers of analysis_service.c.
 *
 * The table keeps one mutex per id that is locked or waited for, with a
 * reference count; an entry is freed when its count reaches 0.
 */

/* Blocks until the lock of piece id is held. id is copied. false (lock not held, do not unlock) only when the
   table cannot allocate. */
bool ursatz_gui_piece_lock(const char *id);

/* Releases the lock of piece id. */
void ursatz_gui_piece_unlock(const char *id);

/* Number of ids with an entry in the table (locked or waited for); for tests. */
unsigned ursatz_gui_piece_lock_entries(void);

#endif
