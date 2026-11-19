#ifndef WORKSHOP_MOTIF_SERVICE_H
#define WORKSHOP_MOTIF_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "composer_client.h"

/* The Composer calls of the motif area and the requests they take. The routes parse a request, call here and answer;
 * no route calls composer_api_motive*, _recognize, _random_motive, _validate or _variants itself. Each call that takes
 * a request prints it and moves the answer into reply (owned by the caller); it returns false when the request could
 * not be printed (reply is then empty). */

/* composer_api_motive: the analysis of a motive request. */
bool workshop_motif_motive(const cJSON *request, WorkshopComposerReply *reply);

/* composer_api_motive_render: the MIDI and MusicXML of a motive request. */
bool workshop_motif_motive_render(const cJSON *request, WorkshopComposerReply *reply);

/* composer_api_recognize: how a target motive relates to a source. */
bool workshop_motif_recognize(const cJSON *request, WorkshopComposerReply *reply);

/* composer_api_random_motive: drawn motives. */
bool workshop_motif_random_motive(const cJSON *request, WorkshopComposerReply *reply);

/* composer_api_motive_from_midi: the motive of a MIDI file; request holds meter and key when given. */
bool workshop_motif_motive_from_midi(const unsigned char *bytes, size_t size, const cJSON *request,
                                       WorkshopComposerReply *reply);

/* composer_api_variant_operations: the operation list. */
void workshop_motif_variant_operations(WorkshopComposerReply *reply);

/* composer_api_validate for kind ("motif", "theme" or "piece") on request. */
bool workshop_motif_validate(const char *kind, const cJSON *request, WorkshopComposerReply *reply);

/* Runs composer_api_variants on the motif member of a version body (its degrees, form, meter, key, pickup, harmony,
 * features and role); basic_body is the version body of the lineage root or NULL. chain (an object {"steps": [...]}) is
 * optional (random chains when NULL); seed negative means the clock. job reports progress and asks for cancel. On
 * success returns the Composer's document (caller-owned); otherwise NULL with *status (400 when the Composer refuses
 * the input, 499 for a cancel, else 500) and error set (its path names the request member at fault). */
cJSON *workshop_motif_run_variants(const cJSON *motif, const cJSON *basic_body, const cJSON *functions,
                                     const char *distance, bool each_op, const cJSON *chain, int count, double seed,
                                     const UrsatzJob *job, int *status, UrsatzError *error);

/* Request building. */

/* The motive request of the Composer API: input.text, and input.format / input.form when given, meter, key (when not
 * NULL) and pickup. NULL on allocation failure. */
cJSON *workshop_motif_motive_request(const char *format, const char *form, const char *text, const char *meter,
                                       const char *key, const char *pickup);

/* The harmony, features and role of a motif as request members; harmony and role may be NULL. False on allocation
 * failure. */
bool workshop_motif_add_extras(cJSON *request, const char *harmony, const cJSON *features, const char *role);

/* The motive request of a motif body member (form, notes, meter, key, pickup, harmony, features, role, tones). NULL on
 * allocation failure or when the motif lacks the members of a request. */
cJSON *workshop_motif_request(const cJSON *motif);

/* The harmony text of a motif member, NULL when it has none. */
const char *workshop_motif_harmony(const cJSON *motif);

/* The key text a motif writes its notes in ("C major"): C and the key word of its material, or the old key text of a body not
 * migrated yet; NULL when it has neither. The text is written to out. */
const char *workshop_motif_key_text(const cJSON *motif, char *out, size_t out_size);

/* The role as a Composer option value, NULL for anything but melodic or accompaniment. */
const char *workshop_motif_role_option(const char *role);

/* The result of workshop_motif_analyze_body. */
#define WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE 400

typedef enum {
  WORKSHOP_MOTIF_ANALYSIS_DONE,
  WORKSHOP_MOTIF_ANALYSIS_REFUSED,
  WORKSHOP_MOTIF_ANALYSIS_ALLOC
} WorkshopMotifAnalysis;

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
 * motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
 * for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
 * analysis_error (WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE bytes, empty when analyzed) names the reason; the reason is also
 * stored as motif.analysis_error, null when analyzed. When the Composer reads the input and refuses it, or a full motif
 * has no key to write its degrees in, the result is REFUSED and refusal holds its message.
 * Segments (R-312): when the motif has no features, or segments_proposed is true and its notes, pickup, meter or form
 * differ from previous_body (the previous version's body; NULL for none), the request sends no features and the
 * Composer's proposal is stored as motif.features with segments_proposed true. Otherwise the features are kept and sent
 * as they are, and segments_proposed is kept (false when absent). */
WorkshopMotifAnalysis workshop_motif_analyze_body(cJSON *version_body, const cJSON *previous_body, char *analysis_error,
                                                     char *refusal, size_t refusal_size, char *refusal_path,
                                                     size_t refusal_path_size);

/* The rules of the motif requests the routes get. */

/* A motif body is written with C as its tonic (R-179), so it names a material the Composer offers (the old key text "C major" or
 * "C minor" is read as before). Bodies without a motif pass. */
bool workshop_motif_tonic_ok(const cJSON *version_body);

/* True when a new version of a body that holds a motif or a theme has a version note (every such version says what changed); other
 * bodies need none. */
bool workshop_motif_version_note_ok(const cJSON *version_body, const char *note);

/* The note of an item's first version: note when it has text, "Created" for a motif or a theme, else NULL. */
const char *workshop_motif_first_note(const char *type, const char *note);

/* Random motives from the Composer: body {material | mode, meter, key?, form?, role?, style_id?, count?, seed?}; nothing is saved. The
 * Composer's document plus "ok" in *document. On a failure *http_status is the status to answer and error holds the message and path. */
UrsatzStatus workshop_motif_random(const cJSON *body, cJSON **document, int *http_status, UrsatzError *error);

#endif /* WORKSHOP_MOTIF_SERVICE_H */
