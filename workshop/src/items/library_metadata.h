#ifndef WORKSHOP_PIECE_METADATA_H
#define WORKSHOP_PIECE_METADATA_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include "analysis/analysis_service.h"
#include "base/last_error.h"

/*
 * Piece metadata rules: normalizing composer, source and tag text, the tag
 * list helpers, grouping pieces for the Tags panel and filtering them.
 * Pure functions: no I/O. analysis_service.c stores the values, routes/analyzer.c
 * serves them (R-103).
 */

#define WORKSHOP_META_MAX_VALUE_BYTES 120
#define WORKSHOP_META_MAX_TAG_BYTES 48
#define WORKSHOP_META_MAX_TAGS 32

/*
 * Trims the text, collapses every run of whitespace to one space and drops
 * control characters. "" is valid and means unset. Returns a newly allocated,
 * caller-owned string. Longer than WORKSHOP_META_MAX_VALUE_BYTES after
 * normalizing: NULL with *too_long set. NULL with *too_long clear is an
 * allocation failure. A NULL text normalizes to "".
 */
char *workshop_meta_normalize_value(const char *text, bool *too_long);

/*
 * As workshop_meta_normalize_value, then ASCII lowercase. NULL with
 * *invalid set when the result is empty, longer than
 * WORKSHOP_META_MAX_TAG_BYTES, or contains a comma. NULL with *invalid
 * clear is an allocation failure.
 */
char *workshop_meta_normalize_tag(const char *text, bool *invalid);

/* True when both texts are equal after normalizing, ASCII case-insensitive.
   NULL counts as "". Length limits are not applied. */
bool workshop_meta_values_equal(const char *a, const char *b);

/*
 * Tag lists: *tags is a heap array of *count heap strings in insertion order
 * (NULL and 0 when empty). The tag given is expected to be normalized;
 * duplicates are found with workshop_meta_values_equal.
 *
 * add copies the tag and appends it; a duplicate is skipped (URSATZ_OK).
 * Adding when WORKSHOP_META_MAX_TAGS are already held and the tag is new:
 * URSATZ_ERR_REQUEST. remove drops the tag when present and is
 * not an error when absent.
 */
UrsatzStatus workshop_meta_tags_add(char ***tags, size_t *count, const char *tag);
UrsatzStatus workshop_meta_tags_remove(char ***tags, size_t *count, const char *tag);
void workshop_meta_tags_free(char **tags, size_t count);

/* One distinct value with the number of pieces that have it. */
typedef struct {
    char *value; /* the spelling used by most pieces; ties broken by byte order */
    size_t count;
} WorkshopMetaGroup;

typedef struct {
    WorkshopMetaGroup *composers;
    size_t composer_count;
    WorkshopMetaGroup *sources;
    size_t source_count;
    WorkshopMetaGroup *tags;
    size_t tag_count;
    size_t unassigned_composer; /* pieces with an empty composer */
    size_t unassigned_source;   /* pieces with an empty source */
    size_t untagged;            /* pieces without tags */
    size_t total;
} WorkshopMetaGroups;

/*
 * Groups pieces by composer, source and tag: distinct values (compared
 * case-insensitively) with their piece counts, sorted by count descending
 * then case-insensitive value. A piece counts once per distinct tag. Empty
 * composers and sources and pieces without tags are counted in the
 * unassigned and untagged fields instead of a group.
 */
UrsatzStatus workshop_meta_group(const WorkshopPieceSummary *pieces, size_t count,
                                          WorkshopMetaGroups *out);
void workshop_meta_groups_destroy(WorkshopMetaGroups *groups);

/* NULL string fields match any value; every field given must match. */
typedef struct {
    const char *composer;
    const char *source;
    const char *tag;
    bool composer_unassigned; /* match an empty composer */
    bool source_unassigned;   /* match an empty source */
    bool untagged;            /* match a piece without tags */
} WorkshopMetaFilter;

bool workshop_meta_matches(const WorkshopPieceSummary *piece, const WorkshopMetaFilter *filter);

#endif /* WORKSHOP_PIECE_METADATA_H */
