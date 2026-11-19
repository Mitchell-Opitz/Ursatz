#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer.h>
#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/form_figure.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/piece_reopen.h>
#include <plan/plan_json.h>
#include <plan/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>

#include "api_internal.h"
#include "api_construction.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"
#include <ursatz_composer/piece_plan_keys.h>

/*
 * The piece entry (docs/api.md): the piece request composed, or its loop plan regenerated in one section, with the ending
 * version built from the loop's. The seeds are tried one after another; the plans, the result document, the MIDI file and the
 * MusicXML of the loop version are made in memory.
 */

/* ------------------------------------------------------------- the entry */

/* The piece request members of the API request that are not the piece request: what to make besides the document. */
static UrsatzStatus read_outputs(const cJSON *request, ApiPieceOutputs *outputs, bool *plain_midi, UrsatzError *error) {
    bool present;
    const char *explain = NULL;
    memset(outputs, 0, sizeof(*outputs));
    *plain_midi = false;
    if (!api_member_bool(request, "midi", &outputs->midi, &present, error) ||
        !api_member_bool(request, "musicxml", &outputs->musicxml, &present, error) ||
        !api_member_bool(request, "ending_musicxml", &outputs->ending_musicxml, &present, error) ||
        !api_member_bool(request, "musicxml_versions", &outputs->musicxml_versions, &present, error) ||
        !api_member_bool(request, "layers", &outputs->layers, &present, error) ||
        !api_member_bool(request, "plain_midi", plain_midi, &present, error) ||
        !api_member_string(request, "explain", &explain, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            outputs->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            outputs->explain = 2;
        } else {
            return api_fail(error, URSATZ_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    return URSATZ_OK;
}

/* The loop plan to work on, as JSON text, and the section to write again: "from" and "regenerate". */
static UrsatzStatus read_edit(const cJSON *request, char **text, PieceEdit *edit, const cJSON **section, UrsatzError *error) {
    const cJSON *from = cJSON_GetObjectItemCaseSensitive(request, "from");
    const cJSON *regenerate = cJSON_GetObjectItemCaseSensitive(request, "regenerate");
    memset(edit, 0, sizeof(*edit));
    edit->path = "/regenerate";
    *text = NULL;
    *section = NULL;
    bool has_regenerate = regenerate && !cJSON_IsNull(regenerate);
    if (!from || cJSON_IsNull(from)) {
        return has_regenerate ? api_fail(error, URSATZ_ERR_REQUEST, "/regenerate", "\"regenerate\" goes with \"from\", the loop plan")
                              : URSATZ_OK;
    }
    if (!cJSON_IsObject(from)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/from", "\"from\" must be a plan object");
    }
    *text = cJSON_PrintUnformatted(from);
    if (!*text) {
        return api_fail_alloc(error);
    }
    edit->text = *text;
    if (!has_regenerate) {
        return URSATZ_OK;
    }
    if (!cJSON_IsObject(regenerate)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/regenerate", "\"regenerate\" must be {\"section\": <id>, \"lanes\": [...]}");
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(regenerate, "section");
    if (item && !cJSON_IsNull(item)) {
        if (!cJSON_IsString(item)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/regenerate/section", "\"section\" must be a section id");
        }
        edit->section = item->valuestring;
        *section = item;
    }
    return api_piece_read_lanes(cJSON_GetObjectItemCaseSensitive(regenerate, "lanes"), "/regenerate/lanes", edit, error);
}

UrsatzStatus api_piece_fail_compose(const PieceSpec *request, PieceResult result, const PieceMade *last,
                                      unsigned long first_seed, unsigned long last_seed, UrsatzError *error) {
    if (result == PIECE_ERR_MOTIVE_TOO_LONG) {
        size_t theme_bars = 8;
        size_t idea_bars = 2;
        (void)piece_basic_idea(&request->request, &theme_bars, &idea_bars);
        return api_fail_motif_long(error, "/motive", request->request.motive, &request->request.meter, theme_bars, idea_bars);
    }
    if (result == PIECE_ERR_BAD_REQUEST) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/request",
                        "this piece cannot be built as asked: a theme fill must be as long as its section, in the same meter and "
                        "mode; a progression must be readable and fit its section");
    }
    if (result == PIECE_ERR_STUCK) {
        const PieceVersion *stuck = &last->versions[0];
        for (size_t i = last->version_count; i > 1; i--) {
            if (last->versions[i - 1].composed && last->versions[i - 1].outcome.stuck) {
                stuck = &last->versions[i - 1];
                break;
            }
        }
        const ComposeRejection *rejection =
            stuck->outcome.rejection_count > 0 ? &stuck->outcome.rejections[stuck->outcome.rejection_count - 1] : NULL;
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "",
                        "no piece could be composed from seeds %lu to %lu (the last was stuck in the %s version, %s stage%s%s)%s",
                        first_seed, last_seed, api_piece_version_name(request, (size_t)(stuck - last->versions)),
                        rejection && rejection->stage[0] ? rejection->stage : "?", rejection ? ", " : "",
                        rejection ? rejection->reason : "",
                        rejection && (strcmp(rejection->stage, "seam") == 0 || strcmp(rejection->stage, "structure") == 0 ||
                                      !rejection->stage[0])
                            ? "; a section of whole bars needs 2 bars at least to hold its cadence"
                            : "");
    }
    return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "", "%s", piece_result_name(result));
}

UrsatzStatus composer_api_piece(const ComposerContext *context, const char *request_json,
                                  const UrsatzJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    UrsatzStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != URSATZ_OK) {
        return context_status;
    }
    cJSON *root = NULL;
    PieceSpec *request = calloc(1, sizeof(*request));
    PieceMade piece;
    PieceEdit edit;
    ApiPieceOutputs outputs;
    char *plan_text = NULL;
    const cJSON *section = NULL;
    bool plain_midi = false;
    bool check = false;
    bool present = false;
    bool style_present = false;
    bool seeded = false;
    bool made = false;
    unsigned long seed = 0;
    const char *style = NULL;
    memset(&piece, 0, sizeof(piece));
    memset(&edit, 0, sizeof(edit));
    UrsatzStatus status = request ? api_parse_request(request_json, &root, &out->error) : api_fail_alloc(&out->error);
    if (status == URSATZ_OK) {
        const cJSON *corpus = cJSON_GetObjectItemCaseSensitive(root, "corpus");
        if (corpus && !cJSON_IsNull(corpus)) {
            status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/corpus", "a piece takes no corpus");
        }
    }
    if (status == URSATZ_OK) {
        status = read_outputs(root, &outputs, &plain_midi, &out->error);
    }
    if (status == URSATZ_OK) {
        if (!api_member_bool(root, "check", &check, &present, &out->error) ||
            !api_member_whole(root, "seed", 0, PIECE_SEED_MAX, &seed, &seeded, &out->error) ||
            !api_member_string(root, "style", &style, &style_present, &out->error)) {
            status = URSATZ_ERR_REQUEST;
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == URSATZ_OK && !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "request"))) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/request", "the request needs \"request\": the piece request");
    }
    if (status == URSATZ_OK) {
        status = read_edit(root, &plan_text, &edit, &section, &out->error);
    }
    if (status == URSATZ_OK) {
        status = api_piece_read_request(request, context->knowledge, &materials, cJSON_GetObjectItemCaseSensitive(root, "request"), "/request",
                                        style, plain_midi, job, &out->error);
    }
    if (status == URSATZ_OK && edit.text) {
        status = api_piece_refuse_performed(request, &out->error);
    }
    if (status == URSATZ_OK && outputs.ending_musicxml && api_piece_version_count(request) < 2) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/ending_musicxml", "the %s form has no ending version",
                          request->form->name);
    }
    PieceResult result = PIECE_ERR_STUCK;
    unsigned long last_seed = seed;
    /* The seeds are tried one after another; a stuck run is passed over, and a request no seed can mend ends the search. */
    for (unsigned attempt = 0; status == URSATZ_OK && attempt < PIECE_SEED_TRIES && result == PIECE_ERR_STUCK; attempt++) {
        if (job_step(job, "seed", attempt, PIECE_SEED_TRIES)) {
            status = api_piece_fail_cancelled(&out->error);
            break;
        }
        api_piece_made_destroy(&piece);
        last_seed = (seed + attempt) & PIECE_SEED_MAX;
        made = true;
        if (edit.text) {
            status = api_piece_regenerate(request, &edit, check, last_seed, &piece, &result, &out->error);
        } else {
            result = api_piece_compose(request, check, last_seed, &piece);
        }
        if (status == URSATZ_OK && api_piece_cancelled(&piece)) {
            status = api_piece_fail_cancelled(&out->error);
        }
    }
    if (status == URSATZ_OK && result != PIECE_OK) {
        status = api_piece_fail_compose(request, result, &piece, seed, last_seed, &out->error);
    }
    if (status == URSATZ_OK) {
        cJSON *document = api_piece_document(request, &piece, &edit, section, edit.text != NULL);
        status = document ? api_piece_add_outputs(context->knowledge, document, request, &piece, &outputs, out) : api_fail_alloc(&out->error);
        if (status == URSATZ_OK) {
            status = api_piece_render_loop(context->knowledge, request, &piece, &outputs, out);
        }
        if (status == URSATZ_OK) {
            status = api_respond_json(out, document);
        } else {
            cJSON_Delete(document);
        }
    }
    if (made) {
        api_piece_made_destroy(&piece);
    }
    if (request) {
        api_piece_request_destroy(request);
    }
    free(request);
    cJSON_free(plan_text);
    cJSON_Delete(root);
    if (status != URSATZ_OK) {
        UrsatzError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
