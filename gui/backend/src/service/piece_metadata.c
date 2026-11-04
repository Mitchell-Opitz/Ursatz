#include "piece_metadata.h"
#include "util/fs_util.h"

#include <stdlib.h>
#include <string.h>

static bool is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static bool is_control(unsigned char c) {
    return c < 0x20 || c == 0x7f;
}

static unsigned char lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

/* Trim, collapse whitespace runs to one space, drop control characters. No
   length limit. NULL on allocation failure. */
static char *normalize_text(const char *text) {
    if (!text) {
        text = "";
    }
    char *out = malloc(strlen(text) + 1);
    if (!out) {
        return NULL;
    }
    size_t length = 0;
    bool pending_space = false;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (is_space(*c)) {
            pending_space = true;
        } else if (!is_control(*c)) {
            if (pending_space && length > 0) {
                out[length++] = ' ';
            }
            pending_space = false;
            out[length++] = (char)*c;
        }
    }
    out[length] = '\0';
    return out;
}

/* Byte-wise comparison ignoring ASCII case. */
static int compare_ci(const char *a, const char *b) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (*x && *y && lower(*x) == lower(*y)) {
        x++;
        y++;
    }
    return (int)lower(*x) - (int)lower(*y);
}

char *ursatz_gui_meta_normalize_value(const char *text, bool *too_long) {
    *too_long = false;
    char *out = normalize_text(text);
    if (out && strlen(out) > URSATZ_GUI_META_MAX_VALUE_BYTES) {
        free(out);
        *too_long = true;
        return NULL;
    }
    return out;
}

char *ursatz_gui_meta_normalize_tag(const char *text, bool *invalid) {
    *invalid = false;
    char *out = normalize_text(text);
    if (!out) {
        return NULL;
    }
    size_t length = strlen(out);
    if (length == 0 || length > URSATZ_GUI_META_MAX_TAG_BYTES || strchr(out, ',')) {
        free(out);
        *invalid = true;
        return NULL;
    }
    for (size_t i = 0; i < length; i++) {
        out[i] = (char)lower((unsigned char)out[i]);
    }
    return out;
}

bool ursatz_gui_meta_values_equal(const char *a, const char *b) {
    char *left = normalize_text(a);
    char *right = normalize_text(b);
    bool equal = left && right && compare_ci(left, right) == 0;
    free(left);
    free(right);
    return equal;
}

void ursatz_gui_meta_tags_free(char **tags, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(tags[i]);
    }
    free(tags);
}

/* Index of the tag equal to tag in the list, or count when absent. */
static size_t find_tag(char *const *tags, size_t count, const char *tag) {
    for (size_t i = 0; i < count; i++) {
        if (ursatz_gui_meta_values_equal(tags[i], tag)) {
            return i;
        }
    }
    return count;
}

UrsatzGuiMetaResult ursatz_gui_meta_tags_add(char ***tags, size_t *count, const char *tag) {
    if (find_tag(*tags, *count, tag) < *count) {
        return URSATZ_GUI_META_OK;
    }
    if (*count >= URSATZ_GUI_META_MAX_TAGS) {
        return URSATZ_GUI_META_ERR_TOO_MANY_TAGS;
    }
    char *copy = ursatz_gui_dup_string(tag);
    char **grown = copy ? realloc(*tags, (*count + 1) * sizeof(char *)) : NULL;
    if (!grown) {
        free(copy);
        return URSATZ_GUI_META_ERR_ALLOC;
    }
    grown[*count] = copy;
    *tags = grown;
    (*count)++;
    return URSATZ_GUI_META_OK;
}

UrsatzGuiMetaResult ursatz_gui_meta_tags_remove(char ***tags, size_t *count, const char *tag) {
    size_t at = find_tag(*tags, *count, tag);
    if (at == *count) {
        return URSATZ_GUI_META_OK;
    }
    free((*tags)[at]);
    memmove(&(*tags)[at], &(*tags)[at + 1], (*count - at - 1) * sizeof(char *));
    (*count)--;
    if (*count == 0) {
        free(*tags);
        *tags = NULL;
    }
    return URSATZ_GUI_META_OK;
}

/* One distinct value (case-insensitive) while grouping, with the spellings
   seen and how many pieces used each. */
typedef struct {
    char **spellings;
    size_t *spelling_counts;
    size_t spelling_count;
    size_t count;
} Bucket;

typedef struct {
    Bucket *items;
    size_t count;
} BucketList;

static void bucket_list_destroy(BucketList *list) {
    for (size_t i = 0; i < list->count; i++) {
        for (size_t j = 0; j < list->items[i].spelling_count; j++) {
            free(list->items[i].spellings[j]);
        }
        free(list->items[i].spellings);
        free(list->items[i].spelling_counts);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* Counts one piece for a normalized, non-empty value. */
static bool bucket_list_add(BucketList *list, const char *value) {
    Bucket *bucket = NULL;
    for (size_t i = 0; i < list->count && !bucket; i++) {
        if (compare_ci(list->items[i].spellings[0], value) == 0) {
            bucket = &list->items[i];
        }
    }
    if (!bucket) {
        Bucket *grown = realloc(list->items, (list->count + 1) * sizeof(Bucket));
        if (!grown) {
            return false;
        }
        list->items = grown;
        bucket = &list->items[list->count];
        memset(bucket, 0, sizeof(*bucket));
        list->count++;
    }
    for (size_t j = 0; j < bucket->spelling_count; j++) {
        if (strcmp(bucket->spellings[j], value) == 0) {
            bucket->spelling_counts[j]++;
            bucket->count++;
            return true;
        }
    }
    char *copy = ursatz_gui_dup_string(value);
    char **spellings = copy ? realloc(bucket->spellings, (bucket->spelling_count + 1) * sizeof(char *)) : NULL;
    if (spellings) {
        bucket->spellings = spellings;
    }
    size_t *counts =
        spellings ? realloc(bucket->spelling_counts, (bucket->spelling_count + 1) * sizeof(size_t)) : NULL;
    if (!counts) {
        free(copy);
        /* A bucket created above with no spelling must not stay in the list. */
        if (bucket->spelling_count == 0) {
            free(bucket->spellings);
            list->count--;
        }
        return false;
    }
    bucket->spelling_counts = counts;
    bucket->spellings[bucket->spelling_count] = copy;
    bucket->spelling_counts[bucket->spelling_count] = 1;
    bucket->spelling_count++;
    bucket->count++;
    return true;
}

static int compare_groups(const void *left, const void *right) {
    const UrsatzGuiMetaGroup *a = left;
    const UrsatzGuiMetaGroup *b = right;
    if (a->count != b->count) {
        return a->count > b->count ? -1 : 1;
    }
    int by_value = compare_ci(a->value, b->value);
    return by_value != 0 ? by_value : strcmp(a->value, b->value);
}

/* Moves the buckets into a sorted group array, choosing each shown spelling. */
static bool buckets_to_groups(BucketList *list, UrsatzGuiMetaGroup **out_groups, size_t *out_count) {
    *out_groups = NULL;
    *out_count = list->count;
    if (list->count == 0) {
        return true;
    }
    UrsatzGuiMetaGroup *groups = calloc(list->count, sizeof(UrsatzGuiMetaGroup));
    if (!groups) {
        return false;
    }
    for (size_t i = 0; i < list->count; i++) {
        const Bucket *bucket = &list->items[i];
        size_t best = 0;
        for (size_t j = 1; j < bucket->spelling_count; j++) {
            if (bucket->spelling_counts[j] > bucket->spelling_counts[best] ||
                (bucket->spelling_counts[j] == bucket->spelling_counts[best] &&
                 strcmp(bucket->spellings[j], bucket->spellings[best]) < 0)) {
                best = j;
            }
        }
        groups[i].value = ursatz_gui_dup_string(bucket->spellings[best]);
        groups[i].count = bucket->count;
        if (!groups[i].value) {
            for (size_t k = 0; k < i; k++) {
                free(groups[k].value);
            }
            free(groups);
            return false;
        }
    }
    qsort(groups, list->count, sizeof(UrsatzGuiMetaGroup), compare_groups);
    *out_groups = groups;
    return true;
}

/* Counts a composer or source: an empty value goes to *unassigned. */
static bool count_single(BucketList *list, size_t *unassigned, const char *text) {
    char *value = normalize_text(text);
    if (!value) {
        return false;
    }
    bool ok = true;
    if (value[0] == '\0') {
        (*unassigned)++;
    } else {
        ok = bucket_list_add(list, value);
    }
    free(value);
    return ok;
}

UrsatzGuiMetaResult ursatz_gui_meta_group(const UrsatzGuiPieceSummary *pieces, size_t count,
                                          UrsatzGuiMetaGroups *out) {
    memset(out, 0, sizeof(*out));
    BucketList composers = {0};
    BucketList sources = {0};
    BucketList tags = {0};
    bool ok = true;

    for (size_t i = 0; i < count && ok; i++) {
        const UrsatzGuiPieceSummary *piece = &pieces[i];
        out->total++;
        ok = count_single(&composers, &out->unassigned_composer, piece->composer) &&
             count_single(&sources, &out->unassigned_source, piece->source);
        size_t counted = 0;
        for (size_t t = 0; t < piece->tag_count && ok; t++) {
            char *tag = normalize_text(piece->tags[t]);
            if (!tag) {
                ok = false;
                break;
            }
            /* A tag repeated within one piece counts once. */
            bool repeated = false;
            for (size_t earlier = 0; earlier < t && !repeated; earlier++) {
                repeated = ursatz_gui_meta_values_equal(piece->tags[earlier], tag);
            }
            if (tag[0] != '\0' && !repeated) {
                ok = bucket_list_add(&tags, tag);
                counted++;
            }
            free(tag);
        }
        if (ok && counted == 0) {
            out->untagged++;
        }
    }

    ok = ok && buckets_to_groups(&composers, &out->composers, &out->composer_count) &&
         buckets_to_groups(&sources, &out->sources, &out->source_count) &&
         buckets_to_groups(&tags, &out->tags, &out->tag_count);
    bucket_list_destroy(&composers);
    bucket_list_destroy(&sources);
    bucket_list_destroy(&tags);
    if (!ok) {
        ursatz_gui_meta_groups_destroy(out);
        return URSATZ_GUI_META_ERR_ALLOC;
    }
    return URSATZ_GUI_META_OK;
}

static void groups_free(UrsatzGuiMetaGroup *groups, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(groups[i].value);
    }
    free(groups);
}

void ursatz_gui_meta_groups_destroy(UrsatzGuiMetaGroups *groups) {
    groups_free(groups->composers, groups->composer_count);
    groups_free(groups->sources, groups->source_count);
    groups_free(groups->tags, groups->tag_count);
    memset(groups, 0, sizeof(*groups));
}

static bool is_blank(const char *text) {
    char *value = normalize_text(text);
    bool blank = value && value[0] == '\0';
    free(value);
    return blank;
}

bool ursatz_gui_meta_matches(const UrsatzGuiPieceSummary *piece, const UrsatzGuiMetaFilter *filter) {
    if (filter->composer && !ursatz_gui_meta_values_equal(piece->composer, filter->composer)) {
        return false;
    }
    if (filter->source && !ursatz_gui_meta_values_equal(piece->source, filter->source)) {
        return false;
    }
    if (filter->composer_unassigned && !is_blank(piece->composer)) {
        return false;
    }
    if (filter->source_unassigned && !is_blank(piece->source)) {
        return false;
    }
    if (filter->untagged) {
        for (size_t i = 0; i < piece->tag_count; i++) {
            if (!is_blank(piece->tags[i])) {
                return false;
            }
        }
    }
    if (filter->tag) {
        bool found = false;
        for (size_t i = 0; i < piece->tag_count && !found; i++) {
            found = ursatz_gui_meta_values_equal(piece->tags[i], filter->tag);
        }
        if (!found) {
            return false;
        }
    }
    return true;
}
