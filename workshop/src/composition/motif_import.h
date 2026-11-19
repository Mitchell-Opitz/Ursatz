#ifndef WORKSHOP_MOTIF_IMPORT_H
#define WORKSHOP_MOTIF_IMPORT_H

#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The import of a motif from a MIDI file (R-179): the Composer reads the file, and a motif found on another tonic moves to C in the same
 * mode. Nothing is saved.
 */

/* Reads bytes (a MIDI file) with the optional meter and key ("D minor"; NULL for none). The answer {"ok": true, notes_text, meter, key,
 * material, key_found, moved_from, pickup, analysis} is in *answer. On a failure *http_status is the status to answer and error holds
 * the message (and the request path a Composer refusal names). */
UrsatzStatus workshop_motif_import(const unsigned char *bytes, size_t size, const char *meter, const char *key, cJSON **answer,
                                   int *http_status, UrsatzError *error);

#endif /* WORKSHOP_MOTIF_IMPORT_H */
