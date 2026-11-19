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

#include "api_grow.h"
#include "api_internal.h"
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

ComposerStatus api_piece_fail_cancelled(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
}

/* ------------------------------------------------------------- composing */

size_t api_piece_version_names(const FormSpec *form, const char **names, const char **labels, size_t capacity) {
    size_t count = 0;
    if (form->request_sections && form->loop_role_count > 0) {
        if (count < capacity) {
            names[count] = "loop";
            labels[count++] = "Loop version";
        }
        if (count < capacity) {
            names[count] = "ending";
            labels[count++] = "Ending version";
        }
    } else if (count < capacity) {
        names[count] = "piece";
        labels[count++] = "Piece";
    }
    return count;
}

void api_piece_version_destroy(PieceVersion *version) {
    if (version->composed) {
        compose_outcome_destroy(&version->outcome);
    }
    piece_performance_destroy(&version->performance);
    plan_destroy(version->plan);
    memset(version, 0, sizeof(*version));
}

ComposerStatus api_piece_refuse_performed(const PieceSpec *request, ComposerError *error) {
    if (request->form && form_spec_performed(request->form)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/request/form",
                        "regenerate and variations of the %s form are Later: it is composed whole", request->form->label);
    }
    return COMPOSER_OK;
}

const Plan *api_piece_version_output_plan(const PieceVersion *version) {
    return version->performed ? version->performance.plan : version->plan;
}

void api_piece_made_destroy(PieceMade *piece) {
    for (size_t i = 0; i < API_PIECE_VERSIONS_MAX; i++) {
        api_piece_version_destroy(&piece->versions[i]);
    }
    piece_parts_destroy(&piece->parts);
}

/* How many versions the form of the request has, and the name of one in the document: "loop" and "ending", or "piece". */
size_t api_piece_version_count(const PieceSpec *request) {
    const char *names[API_PIECE_VERSIONS_MAX];
    const char *labels[API_PIECE_VERSIONS_MAX];
    return api_piece_version_names(request->form, names, labels, API_PIECE_VERSIONS_MAX);
}

const char *api_piece_version_name(const PieceSpec *request, size_t index) {
    const char *names[API_PIECE_VERSIONS_MAX];
    const char *labels[API_PIECE_VERSIONS_MAX];
    return api_piece_version_names(request->form, names, labels, API_PIECE_VERSIONS_MAX) > index ? names[index] : "";
}

const char *api_piece_first_version_name(const PieceSpec *request) {
    return api_piece_version_name(request, 0);
}

void api_piece_make_config(const PieceSpec *request, bool check, unsigned long seed, ComposeConfig *config) {
    memset(config, 0, sizeof(*config));
    config->seed = seed;
    config->analyzer_check = check;
    config->style = request->style;
    config->style_weight = request->weight;
    config->basic = request->basic;
    config->tempo = request->tempo;
    config->pool = request->pool;
    config->pool_count = request->pool_count;
    config->job = request->job;
    config->knowledge = request->knowledge;
}

static PieceResult compose_version(const PieceSpec *request, bool check, unsigned long seed, PieceVersion *version) {
    ComposeConfig config;
    api_piece_make_config(request, check, seed, &config);
    PieceResult result = compose_run(version->plan, &config, &version->outcome);
    version->composed = true;
    return result;
}

PieceResult api_piece_compose_ending(const PieceSpec *request, bool check, unsigned long seed, PieceMade *piece) {
    PieceResult result = piece_build_ending(piece->versions[0].plan, &request->request, &piece->versions[1].plan);
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed + PIECE_ENDING_SEED_OFFSET, &piece->versions[1]);
    }
    return result;
}

/*
 * One try from a seed: the first version (the loop of a game loop), then the ending version built from it with seed + 20. A
 * sized form has the one version, and its plan is built again with the seed, which plans the keys of a modulating piece.
 */
PieceResult api_piece_compose(PieceSpec *request, bool check, unsigned long seed, PieceMade *piece) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    piece->version_count = api_piece_version_count(request);
    if (!request->form->request_sections) {
        request->request.seed = seed;
    }
    /*
     * A compound form composes each part as a piece of its own and performs them in order. The version has no plan and no
     * outcome of its own: the rows read the parts (R-485).
     */
    if (!request->form->request_sections && request->form->part_count > 0) {
        PieceVersion *version = &piece->versions[0];
        ComposeConfig config;
        api_piece_make_config(request, check, seed, &config);
        PieceResult composed = piece_compose_parts(request->form, &request->request, &config, &piece->parts);
        if (composed == PIECE_OK) {
            version->composed = true;
            version->parts = &piece->parts;
            composed = piece_perform_compound(request->form, request->request.bars, request->request.count, &piece->parts,
                                              &version->performance);
            version->performed = composed == PIECE_OK;
        }
        return composed;
    }
    PieceResult result = piece_build_plan(&request->request, &piece->versions[0].plan);
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed, &piece->versions[0]);
    }
    /* A form with repeats is composed once and performed in order; the performance is what is rendered (R-485). */
    if (result == PIECE_OK && !request->form->request_sections && form_spec_performed(request->form)) {
        PieceVersion *version = &piece->versions[0];
        PiecePerformPart part = {request->form, form_spec_size(request->form, request->request.bars, request->request.count),
                                 version->plan, true, NULL};
        result = piece_perform(&part, 1, &version->performance);
        version->performed = result == PIECE_OK;
    }
    if (result == PIECE_OK && piece->version_count == 2) {
        result = api_piece_compose_ending(request, check, seed, piece);
    }
    return result;
}

bool api_piece_cancelled(const PieceMade *piece) {
    for (size_t i = 0; i < piece->parts.count; i++) {
        if (piece->parts.outcomes[i].cancelled) {
            return true;
        }
    }
    for (size_t i = 0; i < piece->version_count; i++) {
        if (piece->versions[i].composed && piece->versions[i].outcome.cancelled) {
            return true;
        }
    }
    return false;
}

