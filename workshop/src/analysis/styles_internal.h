#ifndef URSATZ_WORKSHOP_ANALYSIS_STYLES_INTERNAL_H
#define URSATZ_WORKSHOP_ANALYSIS_STYLES_INTERNAL_H

/* What styles.c (list, get and the request), styles_build.c (briefs, sources, the files and the build), styles_corpus.c (the pieces a
   source names) and styles_rebuild.c (stored styles, rebuild and delete) share: private to analysis. */

#include "items/formats.h"
#include "items/layout.h"
#include "items/txn.h"
#include "base/last_error.h"
#include "items/lock.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <cJSON.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <theory/style/style_compare.h>
#include <theory/style/style_features.h>
#include <theory/style/style_labels.h>
#include <theory/style/style_model.h>
#include <theory/style/style_model_file.h>
#include <theory/style/style_profile.h>
#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include "analysis.h"
#include "items/collections.h"
#include "items/ids.h"
#include "items/library_metadata.h"
#include "analysis/style_model_io.h"

#define PATH_BYTES 400

#define NAME_MAX_BYTES WORKSHOP_META_MAX_VALUE_BYTES

#define ID_MAX_TRIES 8

#define LOCK() workshop_lock_acquire(&styles_g_lock)

#define UNLOCK() workshop_lock_release(&styles_g_lock)

typedef struct {
    WorkshopStyleSkipped *items;
    size_t count;
    size_t capacity;
} SkippedList;

/* A source with its filter values normalized, ready to resolve and to store. */
typedef struct {
    WorkshopStyleSourceKind kind;
    char *composer; /* "" when unset */
    char *source;
    char *tag;
    char *collection_id;
    CommonNames piece_ids;
    WorkshopStyleBrief brief;
} NormalizedSource;

/* --- reading pieces into a profile ---------------------------------------- */

/* The pieces a build used and left out, with the profile they made. */
typedef struct {
    StyleProfile *profile;
    CommonNames used;
    SkippedList skipped;
    char default_name[NAME_MAX_BYTES + 1];
} Collected;

extern const char *styles_kPiecesDir;
extern const char *styles_kStylesDir;
extern WorkshopLock styles_g_lock;
bool styles_skipped_add(SkippedList *list, const char *id, const char *reason);
void styles_free_strings(char **items, size_t count);
bool styles_scope_kind_from_label(const char *label, WorkshopLinkScopeKind *kind);
void styles_clip_utf8(char *text, size_t max_bytes);
void styles_normalized_source_free(NormalizedSource *source);
UrsatzStatus styles_normalized_source_make(const WorkshopStyleSource *input, NormalizedSource *out);
bool styles_write_style_files(const char *dir, const char *id, const char *name, const char *created_at, const char *updated_at, const NormalizedSource *source, const Collected *collected);
UrsatzStatus styles_load_meta(const char *id, cJSON **out);
void styles_collected_free(Collected *collected);
UrsatzStatus styles_collect_pieces(const NormalizedSource *source, const UrsatzJob *job, Collected *collected);
bool styles_style_dir_path(const char *id, char *out, size_t out_size);
cJSON *styles_entry_of(cJSON *meta);

#endif
