#ifndef URSATZ_GUI_PIECE_METADATA_H
#define URSATZ_GUI_PIECE_METADATA_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_service.h"

/*
 * Piece metadata rules: normalizing composer, source and tag text, the tag
 * list helpers, grouping pieces for the Tags panel and filtering them.
 * Pure functions: no I/O. analysis_service.c stores the values, routes/analyzer.c
 * serves them (R-103).
 */

#define URSATZ_GUI_META_MAX_VALUE_BYTES 120
#define URSATZ_GUI_META_MAX_TAG_BYTES 48
#define URSATZ_GUI_META_MAX_TAGS 32

typedef enum {
    URSATZ_GUI_META_OK = 0,
    URSATZ_GUI_META_ERR_ALLOC,
    URSATZ_GUI_META_ERR_TOO_MANY_TAGS
} UrsatzGuiMetaResult;

/*
 * Trims the text, collapses every run of whitespace to one space and drops
 * control characters. "" is valid and means unset. Returns a newly allocated,
 * caller-owned string. Longer than URSATZ_GUI_META_MAX_VALUE_BYTES after
 * normalizing: NULL with *too_long set. NULL with *too_long clear is an
 * allocation failure. A NULL text normalizes to "".
 */
char *ursatz_gui_meta_normalize_value(const char *text, bool *too_long);

/*
 * As ursatz_gui_meta_normalize_value, then ASCII lowercase. NULL with
 * *invalid set when the result is empty, longer than
 * URSATZ_GUI_META_MAX_TAG_BYTES, or contains a comma. NULL with *invalid
 * clear is an allocation failure.
 */
char *ursatz_gui_meta_normalize_tag(const char *text, bool *invalid);

/* True when both texts are equal after normalizing, ASCII case-insensitive.
   NULL counts as "". Length limits are not applied. */
bool ursatz_gui_meta_values_equal(const char *a, const char *b);

/*
 * Tag lists: *tags is a heap array of *count heap strings in insertion order
 * (NULL and 0 when empty). The tag given is expected to be normalized;
 * duplicates are found with ursatz_gui_meta_values_equal.
 *
 * add copies the tag and appends it; a duplicate is skipped (URSATZ_GUI_META_OK).
 * Adding when URSATZ_GUI_META_MAX_TAGS are already held and the tag is new:
 * URSATZ_GUI_META_ERR_TOO_MANY_TAGS. remove drops the tag when present and is
 * not an error when absent.
 */
UrsatzGuiMetaResult ursatz_gui_meta_tags_add(char ***tags, size_t *count, const char *tag);
UrsatzGuiMetaResult ursatz_gui_meta_tags_remove(char ***tags, size_t *count, const char *tag);
void ursatz_gui_meta_tags_free(char **tags, size_t count);

/* One distinct value with the number of pieces that have it. */
typedef struct {
    char *value; /* the spelling used by most pieces; ties broken by byte order */
    size_t count;
} UrsatzGuiMetaGroup;

typedef struct {
    UrsatzGuiMetaGroup *composers;
    size_t composer_count;
    UrsatzGuiMetaGroup *sources;
    size_t source_count;
    UrsatzGuiMetaGroup *tags;
    size_t tag_count;
    size_t unassigned_composer; /* pieces with an empty composer */
    size_t unassigned_source;   /* pieces with an empty source */
    size_t untagged;            /* pieces without tags */
    size_t total;
} UrsatzGuiMetaGroups;

/*
 * Groups pieces by composer, source and tag: distinct values (compared
 * case-insensitively) with their piece counts, sorted by count descending
 * then case-insensitive value. A piece counts once per distinct tag. Empty
 * composers and sources and pieces without tags are counted in the
 * unassigned and untagged fields instead of a group.
 */
UrsatzGuiMetaResult ursatz_gui_meta_group(const UrsatzGuiPieceSummary *pieces, size_t count,
                                          UrsatzGuiMetaGroups *out);
void ursatz_gui_meta_groups_destroy(UrsatzGuiMetaGroups *groups);

/* NULL string fields match any value; every field given must match. */
typedef struct {
    const char *composer;
    const char *source;
    const char *tag;
    bool composer_unassigned; /* match an empty composer */
    bool source_unassigned;   /* match an empty source */
    bool untagged;            /* match a piece without tags */
} UrsatzGuiMetaFilter;

bool ursatz_gui_meta_matches(const UrsatzGuiPieceSummary *piece, const UrsatzGuiMetaFilter *filter);

#endif /* URSATZ_GUI_PIECE_METADATA_H */
