#ifndef URSATZ_GUI_COMPOSER_CLIENT_H
#define URSATZ_GUI_COMPOSER_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ursatz_composer/composer_api.h"

/* What a Composer API call (R-219) answers, owned by the route that made the call. The members are moved out of the
 * ComposerResponse, so the reply outlives it. The refusal text is message and the request member at fault is path
 * ("" when none); both are empty on success. The CLI's exit 1 is COMPOSER_ERR_REQUEST, its exit 2
 * COMPOSER_ERR_UNSATISFIABLE; anything else (ALLOC, INTERNAL, CANCELLED) is a failure of the run. */
typedef struct {
  ComposerStatus status;
  char *json;
  char *plan_json;
  uint8_t *midi;
  size_t midi_size;
  char *musicxml;
  char message[512];
  char path[128];
} UrsatzGuiComposerReply;

/* Moves the members of response into reply, sets reply->status to status and copies the error (message and path),
 * then destroys response. reply is overwritten, not freed first. */
void ursatz_gui_composer_reply_take(ComposerStatus status, ComposerResponse *response, UrsatzGuiComposerReply *reply);

/* Frees the members and zeroes the reply. Safe on a zeroed reply and when called twice. */
void ursatz_gui_composer_reply_destroy(UrsatzGuiComposerReply *reply);

/* The Composer's vocabulary (composer_api_vocabulary), built on the first call and kept for the process. NULL when the
 * Composer could not give it. */
const char *ursatz_gui_composer_vocabulary(void);

/* The linked Composer's build as composer_api_versions gives it, read on the first call and kept for the process. Each
 * pointer is a string owned by the client; all are "" when the Composer could not give its versions. */
typedef struct {
  const char *api_version;
  const char *composer_version;
  const char *ursatz_commit;
} UrsatzGuiComposerBuild;

UrsatzGuiComposerBuild ursatz_gui_composer_build(void);

/* The spelling of the tonic of pitch_class (0 to 11) in mode ("major" or "minor") in the vocabulary's pitch_class_names;
 * false when it has none. */
bool ursatz_gui_composer_tonic_name(int pitch_class, const char *mode, char *out, size_t out_size);

#endif /* URSATZ_GUI_COMPOSER_CLIENT_H */
