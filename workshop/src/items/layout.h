#ifndef WORKSHOP_ITEMS_LAYOUT_H
#define WORKSHOP_ITEMS_LAYOUT_H

/*
 * Data folder layout 1 (R-642): every path of the workshop's files is built here, relative to the data folder (the workshop's working
 * directory once a server is started), and nowhere else.
 *
 *   data-folder.json                                   the layout marker, written last by a migration
 *   ursatz.json                                        the settings file (optional)
 *   projects/<prj>/project.json
 *   projects/<prj>/items/<id>/item.json                the header (name, current version)
 *   projects/<prj>/items/<id>/op-checks.json           a motif's operation marks
 *   projects/<prj>/items/<id>/v<n>/envelope.json       what the version is (written last: a version exists once it has one)
 *   projects/<prj>/items/<id>/v<n>/body.json           the body
 *   projects/<prj>/items/<id>/v<n>/...                 a composed version's files, as before (request.json, plan.json, theme.mid...)
 *   subjects/<sub>.json
 *   library/pieces/<id>/{source.<ext>, piece.json, analysis.json}
 *   library/collections/<id>.json
 *   library/links/<lnk>/run.json
 *   settings/styles/<sty>/{style-model.json, style.json, settings.json}
 *   settings/taste/<name>.json                         a taste profile (R-671)
 *   ratings/sessions/<ses-id>.json                     a listening session: batches, ratings, picks (R-674)
 *   ratings/items/<item id>.json                       the ratings of the versions of an item
 *   ratings/critiques/<item id>.json                   the critic's judgments of the versions of an item and her verdicts on their observations (R-723)
 *   projects/<prj>/settings.json                       the project's settings, overrides and switches
 *   projects/<prj>/items/<id>/settings.json            the item's
 *   projects/<prj>/items/<id>/v<n>/settings.json       how the settings resolved for the compose that made the version
 *   cache/motive-link-cache.json
 *   cache/renders/<item>/v<n>/{score.musicxml, motif.mid, render.json}
 *   cache/index/                                       rebuilt on open, never read as a source of truth
 *
 * Every function writes the path into out and returns false when an id is not a valid id or the path does not fit.
 */

#include <stdbool.h>
#include <stddef.h>

#define WORKSHOP_DIR_PROJECTS "projects"
#define WORKSHOP_DIR_SUBJECTS "subjects"
#define WORKSHOP_DIR_PIECES "library/pieces"
#define WORKSHOP_DIR_COLLECTIONS "library/collections"
#define WORKSHOP_DIR_LINKS "library/links"
#define WORKSHOP_DIR_STYLES "settings/styles"
#define WORKSHOP_DIR_TASTE "settings/taste"
#define WORKSHOP_DIR_SESSIONS "ratings/sessions"
#define WORKSHOP_DIR_ITEM_RATINGS "ratings/items"
#define WORKSHOP_DIR_ITEM_CRITIQUES "ratings/critiques"
#define WORKSHOP_DIR_CACHE "cache"
#define WORKSHOP_DIR_RENDERS "cache/renders"
#define WORKSHOP_DIR_INDEX "cache/index"
#define WORKSHOP_FILE_DATA_FOLDER "data-folder.json"
#define WORKSHOP_FILE_LINK_CACHE "cache/motive-link-cache.json"

/* The file names inside a folder of the layout. */
#define WORKSHOP_FILE_PROJECT "project.json"
#define WORKSHOP_FILE_ITEM "item.json"
#define WORKSHOP_FILE_ENVELOPE "envelope.json"
#define WORKSHOP_FILE_BODY "body.json"
#define WORKSHOP_FILE_OP_CHECKS "op-checks.json"
#define WORKSHOP_FILE_PIECE "piece.json"
#define WORKSHOP_FILE_ANALYSIS "analysis.json"
#define WORKSHOP_FILE_RUN "run.json"
#define WORKSHOP_FILE_STYLE "style.json"
#define WORKSHOP_FILE_STYLE_MODEL "style-model.json"
#define WORKSHOP_FILE_SETTINGS "settings.json"

/* True for an id the layout may name: 1 to 64 characters of letters, digits, hyphens and underscores. */
bool workshop_layout_id_ok(const char *id);

bool workshop_layout_project_dir(const char *project, char *out, size_t size);
bool workshop_layout_project_file(const char *project, char *out, size_t size);
bool workshop_layout_items_dir(const char *project, char *out, size_t size);
bool workshop_layout_item_dir(const char *project, const char *item, char *out, size_t size);
bool workshop_layout_item_file(const char *project, const char *item, const char *name, char *out, size_t size);
bool workshop_layout_version_dir(const char *project, const char *item, int version, char *out, size_t size);
bool workshop_layout_version_file(const char *project, const char *item, int version, const char *name, char *out, size_t size);

bool workshop_layout_subject_file(const char *subject, char *out, size_t size);
bool workshop_layout_collection_file(const char *collection, char *out, size_t size);
bool workshop_layout_link_dir(const char *run, char *out, size_t size);
bool workshop_layout_link_file(const char *run, char *out, size_t size);
bool workshop_layout_style_dir(const char *style, char *out, size_t size);
bool workshop_layout_session_file(const char *session, char *out, size_t size);
bool workshop_layout_item_ratings_file(const char *item, char *out, size_t size);
bool workshop_layout_item_critiques_file(const char *item, char *out, size_t size);
bool workshop_layout_taste_file(const char *name, char *out, size_t size);
bool workshop_layout_style_settings_file(const char *style, char *out, size_t size);
bool workshop_layout_project_settings_file(const char *project, char *out, size_t size);
bool workshop_layout_item_settings_file(const char *project, const char *item, char *out, size_t size);
bool workshop_layout_render_dir(const char *item, int version, char *out, size_t size);
bool workshop_layout_render_tmp_dir(char *out, size_t size);

#endif
