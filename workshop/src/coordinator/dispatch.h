#ifndef URSATZ_WORKSHOP_COORDINATOR_DISPATCH_H
#define URSATZ_WORKSHOP_COORDINATOR_DISPATCH_H

/*
 * The query and command API's internals (R-646): what a handler is, the helpers every handler uses to read its parameters and fill its
 * reply, and the handlers themselves, which dispatch.c lists in one table per kind. A handler reads the call's params, calls the services
 * and fills the reply; it returns the status of the call and, when it fails, puts the message (and the request path at fault) into error.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>
#include <ursatz_workshop/workshop_api.h>

#include "base/last_error.h"
#include "coordinator/services.h"

typedef struct {
    Workshop *workshop;
    const cJSON *params; /* the call's params object (an empty one when the caller gave none); never NULL */
    const WorkshopBytes *bytes;
    const char *actor;   /* "" for a query */
    const char *target;  /* "" when none */
    long base_version;   /* the version the caller read, -1 when it gave none */
} WorkshopCall;

typedef UrsatzStatus (*WorkshopHandler)(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error);

/* --- parameters ------------------------------------------------------------------------------------------------------------------ */

/* The string member of params, NULL when absent or null; *bad is set when it is present and not a string. */
const char *workshop_param_string(const cJSON *params, const char *name, bool *bad);

/* A required string member: false (and error filled: REQUEST at the member's path) when absent or not a string. */
bool workshop_param_require_string(const cJSON *params, const char *name, const char **out, UrsatzError *error);

/* The number in text (digits only, 1 to max_digits of them), false when it is not one. */
bool workshop_parse_whole(const char *text, int max_digits, long long *out);

/* --- failures -------------------------------------------------------------------------------------------------------------------- */

/* Fills error with status, an optional request path and a message; returns status. */
UrsatzStatus workshop_call_fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

/* Fills error from the failing service call's own error (workshop_last_error), or with fallback when it gave no message; returns status. */
UrsatzStatus workshop_call_fail_service(UrsatzError *error, UrsatzStatus status, const char *fallback);

/* The status a Composer call answered with as an HTTP status (the out parameter of the calls that keep one) back as a status. */
UrsatzStatus workshop_status_of_http(int http_status);

/* --- replies --------------------------------------------------------------------------------------------------------------------- */

/* The reply's json is a new object holding the members of the answer; returns it, NULL (and nothing changed) when out of memory. */
cJSON *workshop_reply_object(WorkshopReply *reply);

/* reply->json is {name: value}; value is moved in (and freed when that fails). */
UrsatzStatus workshop_reply_member(WorkshopReply *reply, const char *name, cJSON *value, UrsatzError *error);

/* reply->json is value itself (an object whose members are the answer). */
UrsatzStatus workshop_reply_tree(WorkshopReply *reply, cJSON *value, UrsatzError *error);

/* reply->bytes is a copy of text (length bytes; strlen when length is 0 and text is not empty) with its content type. */
UrsatzStatus workshop_reply_text(WorkshopReply *reply, const char *text, size_t length, const char *content_type, UrsatzError *error);

/* reply->bytes takes data (malloc'd, freed with the reply). */
void workshop_reply_take_bytes(WorkshopReply *reply, uint8_t *data, size_t size, const char *content_type);

/* Fills the reply with a queued job: {"job": {id, kind, state}} (the "job" member the answers of a job command hold; the caller adds the
 * others first through *object when it wants them: object may be NULL). Submits run with input to the queue. Returns OK, or INTERNAL when
 * the queue is not running (input is freed). */
UrsatzStatus workshop_reply_job(WorkshopReply *reply, cJSON *object, const char *kind, WorkshopJobFn run, void *input,
                                void (*free_input)(void *), UrsatzError *error);

/* --- the table ------------------------------------------------------------------------------------------------------------------- */

#define WORKSHOP_HANDLER(name) UrsatzStatus name(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error)

/* library (api_library.c) */
WORKSHOP_HANDLER(workshop_q_library_list);
WORKSHOP_HANDLER(workshop_q_library_groups);
WORKSHOP_HANDLER(workshop_q_library_piece);
WORKSHOP_HANDLER(workshop_q_library_document);
WORKSHOP_HANDLER(workshop_q_library_view);
WORKSHOP_HANDLER(workshop_q_library_musicxml);
WORKSHOP_HANDLER(workshop_q_library_meter_defaulted);
WORKSHOP_HANDLER(workshop_q_library_playback);

/* collections, styles, links, jobs (api_collections.c, api_analysis.c, api_jobs.c) */
WORKSHOP_HANDLER(workshop_q_collection_list);
WORKSHOP_HANDLER(workshop_q_collection_get);
WORKSHOP_HANDLER(workshop_q_collection_pieces);
WORKSHOP_HANDLER(workshop_q_style_list);
WORKSHOP_HANDLER(workshop_q_style_get);
WORKSHOP_HANDLER(workshop_q_links_list);
WORKSHOP_HANDLER(workshop_q_links_get);
WORKSHOP_HANDLER(workshop_q_links_piece);
WORKSHOP_HANDLER(workshop_q_job_get);
WORKSHOP_HANDLER(workshop_q_job_list);

/* composer (api_composer.c) */
WORKSHOP_HANDLER(workshop_q_composer_status);
WORKSHOP_HANDLER(workshop_q_composer_vocabulary);
WORKSHOP_HANDLER(workshop_q_composer_forms);
WORKSHOP_HANDLER(workshop_q_composer_variant_operations);
WORKSHOP_HANDLER(workshop_q_composer_render);
WORKSHOP_HANDLER(workshop_q_composer_random_motifs);
WORKSHOP_HANDLER(workshop_q_composer_import_motif);
WORKSHOP_HANDLER(workshop_q_composer_recognize);

/* projects and items (api_items.c, api_item_files.c, api_motifs.c) */
WORKSHOP_HANDLER(workshop_q_project_list);
WORKSHOP_HANDLER(workshop_q_project_get);
WORKSHOP_HANDLER(workshop_q_item_list);
WORKSHOP_HANDLER(workshop_q_item_get);
WORKSHOP_HANDLER(workshop_q_item_version);
WORKSHOP_HANDLER(workshop_q_item_view);
WORKSHOP_HANDLER(workshop_q_item_export);
WORKSHOP_HANDLER(workshop_q_item_score);
WORKSHOP_HANDLER(workshop_q_item_playback);
WORKSHOP_HANDLER(workshop_q_item_file);
WORKSHOP_HANDLER(workshop_q_item_variation_file);
WORKSHOP_HANDLER(workshop_q_item_variation_playback);
WORKSHOP_HANDLER(workshop_q_item_op_checks);
WORKSHOP_HANDLER(workshop_q_item_variant_tree);
WORKSHOP_HANDLER(workshop_q_item_validate);
WORKSHOP_HANDLER(workshop_q_item_variants);
WORKSHOP_HANDLER(workshop_q_item_each_op);

/* tools (tools.c): the third call, on files the params name (R-656) */
WORKSHOP_HANDLER(workshop_t_analyze);
WORKSHOP_HANDLER(workshop_t_commands);
WORKSHOP_HANDLER(workshop_t_commands_parse);
WORKSHOP_HANDLER(workshop_t_composer);
WORKSHOP_HANDLER(workshop_t_corpus);
WORKSHOP_HANDLER(workshop_t_corpus_passages);
WORKSHOP_HANDLER(workshop_t_pitch_names);
WORKSHOP_HANDLER(workshop_t_style_profile);
WORKSHOP_HANDLER(workshop_t_text);
WORKSHOP_HANDLER(workshop_t_versions);

/* the journal (api_journal.c) */
WORKSHOP_HANDLER(workshop_q_journal_list);

/* subjects (api_subjects.c) */
WORKSHOP_HANDLER(workshop_q_subject_list);
WORKSHOP_HANDLER(workshop_q_subject_groups);
WORKSHOP_HANDLER(workshop_q_subject_get);

/* library, collections, styles, links, jobs commands (api_commands_library.c) */
WORKSHOP_HANDLER(workshop_c_library_import);
WORKSHOP_HANDLER(workshop_c_library_update);
WORKSHOP_HANDLER(workshop_c_library_delete);
WORKSHOP_HANDLER(workshop_c_library_reanalyze);
WORKSHOP_HANDLER(workshop_c_collection_create);
WORKSHOP_HANDLER(workshop_c_collection_update);
WORKSHOP_HANDLER(workshop_c_collection_delete);
WORKSHOP_HANDLER(workshop_c_collection_add_pieces);
WORKSHOP_HANDLER(workshop_c_collection_remove_piece);
WORKSHOP_HANDLER(workshop_c_style_build);
WORKSHOP_HANDLER(workshop_c_style_rebuild);
WORKSHOP_HANDLER(workshop_c_style_delete);
WORKSHOP_HANDLER(workshop_c_links_run);
WORKSHOP_HANDLER(workshop_c_links_delete);
WORKSHOP_HANDLER(workshop_c_job_cancel);

/* projects, items, subjects commands (api_commands_items.c) */
WORKSHOP_HANDLER(workshop_c_project_create);
WORKSHOP_HANDLER(workshop_c_project_update);
WORKSHOP_HANDLER(workshop_c_project_delete);
WORKSHOP_HANDLER(workshop_c_item_create);
WORKSHOP_HANDLER(workshop_c_item_update);
WORKSHOP_HANDLER(workshop_c_item_delete);
WORKSHOP_HANDLER(workshop_c_item_save);
WORKSHOP_HANDLER(workshop_c_item_reanalyze);
WORKSHOP_HANDLER(workshop_c_item_compose);
WORKSHOP_HANDLER(workshop_c_item_regenerate);
WORKSHOP_HANDLER(workshop_c_item_variations);
WORKSHOP_HANDLER(workshop_c_item_choose);
WORKSHOP_HANDLER(workshop_c_item_set_op_checks);
WORKSHOP_HANDLER(workshop_c_item_save_variant);
WORKSHOP_HANDLER(workshop_c_item_flag);
WORKSHOP_HANDLER(workshop_c_item_promote);
WORKSHOP_HANDLER(workshop_c_item_link_form);
WORKSHOP_HANDLER(workshop_c_item_reroll);
WORKSHOP_HANDLER(workshop_c_item_update_use);
WORKSHOP_HANDLER(workshop_c_subject_create);
WORKSHOP_HANDLER(workshop_c_subject_update);
WORKSHOP_HANDLER(workshop_c_subject_delete);
WORKSHOP_HANDLER(workshop_c_subject_link_item);
WORKSHOP_HANDLER(workshop_c_subject_unlink_item);
WORKSHOP_HANDLER(workshop_c_subject_link_piece);
WORKSHOP_HANDLER(workshop_c_subject_unlink_piece);
WORKSHOP_HANDLER(workshop_c_subject_relate);
WORKSHOP_HANDLER(workshop_c_subject_unrelate);

#endif
