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

/* ------------------------------------------------------------- regenerating a section */

static const char *lane_word(PlanLaneKind lane) {
    return lane == PLAN_LANE_HARMONY ? "harmony" : lane == PLAN_LANE_BASS ? "bass" : lane == PLAN_LANE_MELODY ? "melody" : "inner";
}

ComposerStatus api_piece_read_lanes(const cJSON *lanes, const char *path, PieceEdit *edit, ComposerError *error) {
    edit->lane_count = 0;
    if (!lanes || cJSON_IsNull(lanes)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsArray(lanes)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "\"lanes\" must be a list of harmony, bass, melody or inner");
    }
    const cJSON *item = NULL;
    size_t at = 0;
    cJSON_ArrayForEach(item, lanes) {
        char member[128];
        snprintf(member, sizeof member, "%s/%zu", path, at++);
        PlanLaneKind lane;
        if (!cJSON_IsString(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, member, "a lane must be harmony, bass, melody, inner or all");
        }
        if (request_equals_ignore_case(item->valuestring, "all")) {
            edit->lane_count = 0;
            return COMPOSER_OK;
        }
        if (!api_lane_from_name(item->valuestring, &lane)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, member, "lane must be harmony, bass, melody, inner or all, not '%s'",
                            item->valuestring);
        }
        bool known = false;
        for (size_t i = 0; i < edit->lane_count; i++) {
            known = known || edit->lanes[i] == lane;
        }
        if (!known) {
            edit->lanes[edit->lane_count++] = lane;
        }
    }
    return COMPOSER_OK;
}

void api_piece_lanes_name(const PieceEdit *edit, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    if (edit->lane_count == 0) {
        snprintf(out, size, "all");
        return;
    }
    for (size_t i = 0; i < edit->lane_count && used + 1 < size; i++) {
        int written = snprintf(out + used, size - used, "%s%s", i ? "+" : "", lane_word(edit->lanes[i]));
        used += written > 0 ? (size_t)written : 0;
    }
}

/* A section's fill in the request, or PIECE_FILL_EMPTY for an id the request does not have. */
static PieceFillKind request_fill(const PieceSpec *request, const char *id) {
    for (size_t s = 0; s < request->request.section_count; s++) {
        if (strcmp(request->sections[s].description.id, id) == 0) {
            return request->sections[s].fill;
        }
    }
    return PIECE_FILL_EMPTY;
}

/* The section of a plan a form with sizes has, by the name of its node: "A", "B'", "variation 2". */
NodeId api_piece_sized_section_find(const Plan *plan, const char *name) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    for (size_t i = 0; i < count && i < API_MAX_SECTIONS; i++) {
        if (strcmp(sections[i].name, name) == 0) {
            return sections[i].node;
        }
    }
    return PLAN_NODE_NONE;
}

static bool is_node_target(const FillTarget *targets, size_t count, NodeId node) {
    for (size_t i = 0; i < count; i++) {
        if (targets[i].kind == FILL_TARGET_NODE && targets[i].id == node) {
            return true;
        }
    }
    return false;
}

/*
 * The loop plan read again, with what regenerating the section reopens: its lanes (a lane the section was given, by a theme
 * or a progression, is reopened for the lanes asked and no other), and, when the section holds the body's first bar, the
 * seam's copy of that bar dropped and the sections the loop's end holds back reopened, so the seam stage copies the new bar
 * and writes the end against it.
 */
static ComposerStatus open_edit(const PieceSpec *request, const PieceEdit *edit, Plan **out, FillTarget **targets,
                                size_t *count, char *what, size_t what_size, ComposerError *error) {
    PlanJsonError plan_error;
    Plan *plan = NULL;
    bool sized = !request->form->request_sections;
    *out = NULL;
    *targets = NULL;
    *count = 0;
    if (plan_from_json_resolved(request->knowledge, edit->text, composer_plan_read_hooks(), &plan, &plan_error) != PLAN_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the %s: %s", sized ? "plan" : "loop plan",
                        plan_error.message);
    }
    char member[64];
    NodeId node = sized ? api_piece_sized_section_find(plan, edit->section) : piece_section_find(plan, edit->section);
    if (node == PLAN_NODE_NONE) {
        plan_destroy(plan);
        snprintf(member, sizeof member, "%s/section", edit->path);
        return api_fail(error, COMPOSER_ERR_REQUEST, member, "the %s has no section '%s'", sized ? "plan" : "loop plan",
                        edit->section);
    }
    snprintf(member, sizeof member, "%s/lanes", edit->path);
    bool theme = !sized && request_fill(request, edit->section) == PIECE_FILL_THEME;
    if (theme && edit->lane_count == 0) {
        plan_destroy(plan);
        return api_fail(error, COMPOSER_ERR_REQUEST, member,
                        "section '%s' is filled with a theme: regenerate it with lanes harmony, bass, melody or inner",
                        edit->section);
    }
    for (size_t l = 0; theme && l < edit->lane_count; l++) {
        if (edit->lanes[l] == PLAN_LANE_MELODY || edit->lanes[l] == PLAN_LANE_INNER_VOICE) {
            plan_destroy(plan);
            return api_fail(error, COMPOSER_ERR_REQUEST, member,
                            "section '%s' is filled with a theme: its melody and inner voices are the theme's own; regenerate its "
                            "harmony or bass, or fill the section another way",
                            edit->section);
        }
    }
    bool reopened[PLAN_LANE_INNER_VOICE + 1] = {false};
    for (size_t l = 0; l < edit->lane_count; l++) {
        PlanLaneKind lane = edit->lanes[l];
        if (lane == PLAN_LANE_HARMONY) {
            /* New chords need a bass and inner voices written from them (the melody stays), so these are reopened first. */
            if (!reopened[PLAN_LANE_BASS]) {
                piece_reopen_lane(plan, node, PLAN_LANE_BASS);
                reopened[PLAN_LANE_BASS] = true;
            }
            if (!reopened[PLAN_LANE_INNER_VOICE]) {
                piece_reopen_lane(plan, node, PLAN_LANE_INNER_VOICE);
                reopened[PLAN_LANE_INNER_VOICE] = true;
            }
        }
        if (!reopened[lane]) {
            piece_reopen_lane(plan, node, lane);
            reopened[lane] = true;
        }
    }
    FillTarget *found = NULL;
    size_t found_count = 0;
    if (edit->lane_count == 0) {
        ComposerStatus status = api_lane_targets(plan, node, false, PLAN_LANE_HARMONY, &found, &found_count, error);
        if (status != COMPOSER_OK) {
            plan_destroy(plan);
            return status;
        }
    }
    for (size_t l = 0; l < edit->lane_count; l++) {
        FillTarget *lane_targets = NULL;
        size_t lane_count = 0;
        ComposerStatus status = api_lane_targets(plan, node, true, edit->lanes[l], &lane_targets, &lane_count, error);
        FillTarget *grown = status == COMPOSER_OK ? realloc(found, (found_count + lane_count + 1) * sizeof(FillTarget)) : NULL;
        if (status != COMPOSER_OK || !grown) {
            free(lane_targets);
            free(found);
            plan_destroy(plan);
            return status != COMPOSER_OK ? status : api_fail_alloc(error);
        }
        found = grown;
        memcpy(found + found_count, lane_targets, lane_count * sizeof(FillTarget));
        found_count += lane_count;
        free(lane_targets);
    }
    char lanes[64];
    api_piece_lanes_name(edit, lanes, sizeof lanes);
    snprintf(what, what_size, "%s%s section %s", edit->lane_count ? "the " : "", edit->lane_count ? lanes : "", edit->section);
    NodeId held[PIECE_STRUCTURE_MAX_SECTIONS];
    size_t held_count = 0;
    bool refresh = !sized && piece_section_holds_body_start(plan, node);
    if (refresh) {
        held_count = piece_held_sections(plan, held, PIECE_STRUCTURE_MAX_SECTIONS);
        held_count = held_count > PIECE_STRUCTURE_MAX_SECTIONS ? PIECE_STRUCTURE_MAX_SECTIONS : held_count;
    }
    FillTarget *all = calloc(found_count + held_count + 1, sizeof(FillTarget));
    if (!all || (refresh && piece_seam_reset(plan) != PIECE_OK)) {
        free(all);
        free(found);
        plan_destroy(plan);
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot reopen the seam");
    }
    memcpy(all, found, found_count * sizeof(FillTarget));
    size_t total = found_count;
    for (size_t h = 0; h < held_count; h++) {
        if (!is_node_target(all, total, held[h])) {
            all[total++] = (FillTarget){FILL_TARGET_NODE, held[h]};
        }
    }
    free(found);
    *out = plan;
    *targets = all;
    *count = total;
    return COMPOSER_OK;
}

ComposerStatus api_piece_regenerate_loop(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                         PieceVersion *loop, PieceResult *result, ComposerError *error) {
    memset(loop, 0, sizeof(*loop));
    *result = PIECE_OK;
    if (!edit->section) {
        /* The plan is complete: nothing is composed, so the loop version has no outcome to report. */
        PlanJsonError plan_error;
        if (plan_from_json_resolved(request->knowledge, edit->text, composer_plan_read_hooks(), &loop->plan, &plan_error) != PLAN_OK) {
            *result = PIECE_ERR_BAD_REQUEST;
            return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the %s: %s",
                            request->form->request_sections ? "loop plan" : "plan", plan_error.message);
        }
        return COMPOSER_OK;
    }
    Plan *plan = NULL;
    FillTarget *targets = NULL;
    size_t count = 0;
    char what[96];
    ComposeConfig config;
    api_piece_make_config(request, check, seed, &config);
    ComposerStatus status = open_edit(request, edit, &plan, &targets, &count, what, sizeof(what), error);
    if (status != COMPOSER_OK) {
        *result = PIECE_ERR_BAD_REQUEST;
        return status;
    }
    RegenerateResult ran = plan_regenerate_all(plan, targets, count, &config, &loop->outcome);
    free(targets);
    loop->plan = plan;
    loop->composed = true;
    if (loop->outcome.cancelled) {
        return api_piece_fail_cancelled(error);
    }
    if (ran == REGENERATE_ERR_STUCK) {
        *result = PIECE_ERR_STUCK;
        return COMPOSER_OK;
    }
    if (ran != REGENERATE_OK) {
        *result = PIECE_ERR_BAD_REQUEST;
        return api_refused(ran, what, error);
    }
    return COMPOSER_OK;
}

/* One try from a seed: the first plan written again, then the ending version built from it with seed + 20 when the form has one. */
ComposerStatus api_piece_regenerate(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                       PieceMade *piece, PieceResult *result, ComposerError *error) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    piece->version_count = api_piece_version_count(request);
    ComposerStatus status = api_piece_regenerate_loop(request, edit, check, seed, &piece->versions[0], result, error);
    if (status == COMPOSER_OK && *result == PIECE_OK && piece->version_count == 2) {
        *result = api_piece_compose_ending(request, check, seed, piece);
    }
    return status;
}

