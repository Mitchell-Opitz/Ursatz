#ifndef URSATZ_ANALYZER_STYLE_PROFILE_INTERNAL_H
#define URSATZ_ANALYZER_STYLE_PROFILE_INTERNAL_H

#include <cJSON.h>

#include <ursatz_analyzer_internal/style_profile.h>

/*
 * Fills *piece from one analysis document tree (schema version 13): the members a style profile counts, read as
 * the profile has always read them (R-617). *piece must be empty (`StylePiece piece = {0}`); the caller releases it with
 * style_piece_destroy. STYLE_PROFILE_ERR_FORMAT for a tree that is not an analysis document of that version,
 * STYLE_PROFILE_ERR_ALLOC on allocation failure (*piece is then empty).
 */
StyleProfileResult style_piece_from_document(const cJSON *document, StylePiece *piece);

#endif
