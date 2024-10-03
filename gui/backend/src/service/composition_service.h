#ifndef URSATZ_GUI_COMPOSITION_SERVICE_H
#define URSATZ_GUI_COMPOSITION_SERVICE_H

#include <stddef.h>

#include <cJSON.h>

/* The composition store and the commands that fill it (R-086). A composition
 * lives in "<compositions_dir>/<id>/": meta.json (name, kind, the request, the
 * versions) and v<n>/plan.json, piece.mid and piece.musicxml for each version.
 * Every command runs the Composer through composer_service.h with absolute
 * paths; a request field is checked against a fixed pattern or range before
 * it reaches an argument list, and a failed command leaves no version. */

typedef struct {
  int status;  /* HTTP status */
  char *body;  /* JSON text, caller frees; NULL only on allocation failure */
} UrsatzGuiCompositionResponse;

typedef enum {
  URSATZ_GUI_COMPOSITION_FILE_PLAN,
  URSATZ_GUI_COMPOSITION_FILE_MUSICXML,
  URSATZ_GUI_COMPOSITION_FILE_MIDI
} UrsatzGuiCompositionFile;

void ursatz_gui_composition_response_destroy(UrsatzGuiCompositionResponse *response);

/* Grows a theme from a motive: composition kind "grow", version 1. */
UrsatzGuiCompositionResponse ursatz_gui_composition_grow(const char *compositions_dir, const cJSON *request);

/* Plans a piece and fills it: composition kind "plan", version 1. */
UrsatzGuiCompositionResponse ursatz_gui_composition_plan(const char *compositions_dir, const cJSON *request);

/* {"ok":true,"compositions":[{id,name,kind,created_at,versions}]}, newest first. */
UrsatzGuiCompositionResponse ursatz_gui_composition_list(const char *compositions_dir);

/* meta.json of one composition with its id: {"ok":true,"id",...}. */
UrsatzGuiCompositionResponse ursatz_gui_composition_get(const char *compositions_dir, const char *id);

/* request: {"name"}. */
UrsatzGuiCompositionResponse ursatz_gui_composition_rename(const char *compositions_dir, const char *id,
                                                           const cJSON *request);

UrsatzGuiCompositionResponse ursatz_gui_composition_delete(const char *compositions_dir, const char *id);

/* The four commands below work on one version of a composition, named by
 * request "version", and write the next one (the others leave v<version>
 * as it is). The target is "node" (a plan node id) or "section" (a form
 * letter), and optionally "lane" (harmony, bass, melody, inner). */

/* request: {version, node|section, lane?}. Only the plan changes, so the new
 * version copies the piece files of the one it locks. */
UrsatzGuiCompositionResponse ursatz_gui_composition_lock(const char *compositions_dir, const char *id,
                                                         const cJSON *request);

/* request: {version, node|section, lane?, seed?}. */
UrsatzGuiCompositionResponse ursatz_gui_composition_regenerate(const char *compositions_dir, const char *id,
                                                               const cJSON *request);

/* request: {version, node|section, lane?, count, seed?, harmonize?}. Writes
 * v<version>/variations-<k>/variation-<i>.{json,mid,musicxml}; harmonize
 * takes a section and no lane. Returns k and the variations that were kept. */
UrsatzGuiCompositionResponse ursatz_gui_composition_variations(const char *compositions_dir, const char *id,
                                                               const cJSON *request);

/* Makes variation i of variations set k the next version. */
UrsatzGuiCompositionResponse ursatz_gui_composition_choose(const char *compositions_dir, const char *id,
                                                           const char *set, const char *index);

/* The bytes of one file of variation i of set k (caller frees), or NULL. */
char *ursatz_gui_composition_read_variation_file(const char *compositions_dir, const char *id, const char *set,
                                                 const char *index, UrsatzGuiCompositionFile file, size_t *out_size);

/* The bytes of one file of version n (caller frees), or NULL when the
 * composition, the version or the file does not exist. */
char *ursatz_gui_composition_read_version_file(const char *compositions_dir, const char *id, const char *version,
                                               UrsatzGuiCompositionFile file, size_t *out_size);

#endif /* URSATZ_GUI_COMPOSITION_SERVICE_H */
