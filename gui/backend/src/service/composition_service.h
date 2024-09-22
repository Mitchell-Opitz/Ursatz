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

/* The bytes of one file of version n (caller frees), or NULL when the
 * composition, the version or the file does not exist. */
char *ursatz_gui_composition_read_version_file(const char *compositions_dir, const char *id, const char *version,
                                               UrsatzGuiCompositionFile file, size_t *out_size);

#endif /* URSATZ_GUI_COMPOSITION_SERVICE_H */
