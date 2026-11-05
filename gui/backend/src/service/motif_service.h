#ifndef URSATZ_GUI_MOTIF_SERVICE_H
#define URSATZ_GUI_MOTIF_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "composer_client.h"

/* The Composer calls of the motif area and the requests they take. The routes parse a request, call here and answer;
 * no route calls composer_api_motive*, _recognize, _random_motive, _validate or _variants itself. Each call that takes
 * a request prints it and moves the answer into reply (owned by the caller); it returns false when the request could
 * not be printed (reply is then empty). */

/* composer_api_motive: the analysis of a motive request. */
bool ursatz_gui_motif_motive(const cJSON *request, UrsatzGuiComposerReply *reply);

/* composer_api_motive_render: the MIDI and MusicXML of a motive request. */
bool ursatz_gui_motif_motive_render(const cJSON *request, UrsatzGuiComposerReply *reply);

/* composer_api_recognize: how a target motive relates to a source. */
bool ursatz_gui_motif_recognize(const cJSON *request, UrsatzGuiComposerReply *reply);

/* composer_api_random_motive: drawn motives. */
bool ursatz_gui_motif_random_motive(const cJSON *request, UrsatzGuiComposerReply *reply);

/* composer_api_motive_from_midi: the motive of a MIDI file; request holds meter and key when given. */
bool ursatz_gui_motif_motive_from_midi(const unsigned char *bytes, size_t size, const cJSON *request,
                                       UrsatzGuiComposerReply *reply);

/* composer_api_variant_operations: the operation list. */
void ursatz_gui_motif_variant_operations(UrsatzGuiComposerReply *reply);

/* composer_api_validate for kind ("motif", "theme" or "piece") on request. */
bool ursatz_gui_motif_validate(const char *kind, const cJSON *request, UrsatzGuiComposerReply *reply);

/* Runs composer_api_variants on the motif member of a version body (its degrees, form, meter, key, pickup, harmony,
 * features and role); basic_body is the version body of the lineage root or NULL. chain (an object {"steps": [...]}) is
 * optional (random chains when NULL); seed negative means the clock. job reports progress and asks for cancel. On
 * success returns the Composer's document (caller-owned); otherwise NULL with *status (400 when the Composer refuses
 * the input, 499 for a cancel, else 500) and message set. */
cJSON *ursatz_gui_motif_run_variants(const cJSON *motif, const cJSON *basic_body, const cJSON *functions,
                                     const char *distance, bool each_op, const cJSON *chain, int count, double seed,
                                     const ComposerJob *job, int *status, char *message, size_t message_size);

/* The request member at fault of the last refused ursatz_gui_motif_run_variants on this thread, "" when none. */
const char *ursatz_gui_motif_variants_path(void);

/* Request building. */

/* The motive request of the Composer API: input.text, and input.format / input.form when given, meter, key (when not
 * NULL) and pickup. NULL on allocation failure. */
cJSON *ursatz_gui_motif_motive_request(const char *format, const char *form, const char *text, const char *meter,
                                       const char *key, const char *pickup);

/* The harmony, features and role of a motif as request members; harmony and role may be NULL. False on allocation
 * failure. */
bool ursatz_gui_motif_add_extras(cJSON *request, const char *harmony, const cJSON *features, const char *role);

/* The motive request of a motif body member (form, notes, meter, key, pickup, harmony, features, role, tones). NULL on
 * allocation failure or when the motif lacks the members of a request. */
cJSON *ursatz_gui_motif_request(const cJSON *motif);

/* The harmony text of a motif member, NULL when it has none. */
const char *ursatz_gui_motif_harmony(const cJSON *motif);

/* The role as a Composer option value, NULL for anything but melodic or accompaniment. */
const char *ursatz_gui_motif_role_option(const char *role);

/* The result of ursatz_gui_motif_analyze_body. */
#define URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE 400

typedef enum {
  URSATZ_GUI_MOTIF_ANALYSIS_DONE,
  URSATZ_GUI_MOTIF_ANALYSIS_REFUSED,
  URSATZ_GUI_MOTIF_ANALYSIS_ALLOC
} UrsatzGuiMotifAnalysis;

/* Runs the Composer's motive analysis on a motif body (full, rhythm or contour) and stores the result as
 * motif.analysis and its degree, rhythm or contour list as motif.degrees. A body without a valid motif is left alone
 * for the material service to judge. When the Composer cannot run, the analysis and degrees are stored as null and
 * analysis_error (URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE bytes, empty when analyzed) names the reason; the reason is also
 * stored as motif.analysis_error, null when analyzed. When the Composer reads the input and refuses it, or a full motif
 * has no key to write its degrees in, the result is REFUSED and refusal holds its message. */
UrsatzGuiMotifAnalysis ursatz_gui_motif_analyze_body(cJSON *version_body, char *analysis_error, char *refusal,
                                                     size_t refusal_size, char *refusal_path, size_t refusal_path_size);

#endif /* URSATZ_GUI_MOTIF_SERVICE_H */
