#ifndef WORKSHOP_COMPOSER_CLIENT_H
#define WORKSHOP_COMPOSER_CLIENT_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include "ursatz_composer/composer_api.h"

/* What a Composer API call (R-219) answers, owned by the route that made the call. The members are moved out of the
 * ComposerResponse, so the reply outlives it. The refusal text is message and the request member at fault is path
 * ("" when none); both are empty on success. The CLI's exit 1 is URSATZ_ERR_REQUEST, its exit 2
 * URSATZ_ERR_UNSATISFIABLE; anything else (ALLOC, INTERNAL, CANCELLED) is a failure of the run. */
typedef struct {
  UrsatzStatus status;
  char *json;
  char *plan_json;
  uint8_t *midi;
  size_t midi_size;
  char *musicxml;
  char message[512];
  char path[128];
} WorkshopComposerReply;

/* Moves the members of response into reply, sets reply->status to status and copies the error (message and path),
 * then destroys response. reply is overwritten, not freed first. */
void workshop_composer_reply_take(UrsatzStatus status, ComposerResponse *response, WorkshopComposerReply *reply);

/* The CLI's exit 1 and 2: the Composer read the request and refused it. */
bool workshop_composer_reply_refused(const WorkshopComposerReply *reply);

/* The status a Composer refusal is sent with: workshop_http_status of the reply's status, without its path (a refusal at a
 * request member is 400, and the path travels as the "path" member of the answer). */
int workshop_composer_reply_failure_status(const WorkshopComposerReply *reply);

/* Frees the members and zeroes the reply. Safe on a zeroed reply and when called twice. */
void workshop_composer_reply_destroy(WorkshopComposerReply *reply);

/* The Composer's vocabulary (composer_api_vocabulary), built on the first call and kept for the process. NULL when the
 * Composer could not give it. */
const char *workshop_composer_vocabulary(void);

/* The Composer's forms (composer_api_forms), built on the first call and kept for the process. NULL when the Composer
 * could not give them. */
const char *workshop_composer_forms(void);

/* The entry of the form called name in the forms, or NULL when there is none. Owned by the client; valid for the
 * process. */
const cJSON *workshop_composer_form(const char *name);

/* The linked Composer's build as composer_api_versions gives it, read on the first call and kept for the process. Each
 * pointer is a string owned by the client; all are "" when the Composer could not give its versions. */
typedef struct {
  const char *api_version;
  const char *composer_version;
  const char *ursatz_commit;
} WorkshopComposerBuild;

WorkshopComposerBuild workshop_composer_build(void);

/* The largest seed a request of kind ("variants", "random_motive", "theme" or "piece") reads, from the vocabulary's
 * limits.seed_max; -1 when the Composer does not give it. */
double workshop_composer_seed_max(const char *kind);

/* The number the vocabulary's limits give under name ("piece_beats", "piece_sections"...); -1 when the Composer does not
 * give it. */
double workshop_composer_limit(const char *name);

/* The position of value in the vocabulary list called list ("theme_slots", "theme_ideas", "functions"...), from 0; -1 when
 * the list has no such value or the Composer gave no vocabulary. */
int workshop_composer_vocabulary_index(const char *list, const char *value);

/* Whether value is in the vocabulary list called list. */
bool workshop_composer_vocabulary_has(const char *list, const char *value);

/* The vocabulary list called name ("theme_slots", "theme_ideas"...) as the Composer gave it, owned by the client and kept for
 * the process; NULL when the list is missing or the Composer gave no vocabulary. */
const cJSON *workshop_composer_vocabulary_list(const char *name);

/* The position, from 0, of the entry of the vocabulary list called list whose required is true (the theme_slots entry every
 * theme needs); -1 when there is none. */
int workshop_composer_vocabulary_required_index(const char *list);

/* The spelling of the tonic of pitch_class (0 to 11) in a key of the material (its id, "omt.material.major") in the vocabulary's
 * pitch_class_names; false when it has none. */
bool workshop_composer_tonic_name(int pitch_class, const char *material, char *out, size_t out_size);

/* The key word of the material (its id) in the vocabulary's materials ("minor" for "omt.material.natural-minor"); false when
 * it names none. */
bool workshop_composer_key_word(const char *material, char *out, size_t out_size);

/* The word a stored mode member used for a material: its key word ("minor" for "omt.material.natural-minor"), or "style" for
 * "style" (a brief's own); false for anything else. */
bool workshop_composer_material_word(const char *material, char *out, size_t out_size);

/* The material of an old mode word: the id of the materials entry whose key word it is ("minor" gives
 * "omt.material.natural-minor"), "style" for "style"; false for any other word. */
bool workshop_composer_material_of_word(const char *word, char *out, size_t out_size);

#endif /* WORKSHOP_COMPOSER_CLIENT_H */
