#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/piece_reopen.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

/*
 * The piece entry (docs/api.md): the piece request composed, or its loop plan regenerated in one section, with the ending
 * version built from the loop's. The seeds are tried one after another; the plans, the result document, the MIDI file and the
 * MusicXML of the loop version are made in memory.
 */

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

static ComposerStatus fail_cancelled(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
}

/* Adds item to object under name and returns true; the item is deleted when it is NULL or cannot be added. */
static bool add(cJSON *object, const char *name, cJSON *item) {
    if (!object || !item || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------- composing */

void api_piece_version_destroy(PieceVersion *version) {
    if (version->composed) {
        compose_outcome_destroy(&version->outcome);
    }
    plan_destroy(version->plan);
    memset(version, 0, sizeof(*version));
}

void api_piece_made_destroy(PieceMade *piece) {
    api_piece_version_destroy(&piece->loop);
    api_piece_version_destroy(&piece->ending);
}

static void make_config(const PieceSpec *request, bool check, unsigned long seed, ComposeConfig *config) {
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
}

static PieceResult compose_version(const PieceSpec *request, bool check, unsigned long seed, PieceVersion *version) {
    ComposeConfig config;
    make_config(request, check, seed, &config);
    PieceResult result = compose_run(version->plan, &config, &version->outcome);
    version->composed = true;
    return result;
}

PieceResult api_piece_compose_ending(const PieceSpec *request, bool check, unsigned long seed, PieceMade *piece) {
    PieceResult result = piece_build_ending(piece->loop.plan, &request->request, &piece->ending.plan);
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed + PIECE_ENDING_SEED_OFFSET, &piece->ending);
    }
    return result;
}

/* One try from a seed: the loop version, then the ending version built from it with seed + 20. */
static PieceResult compose_piece(const PieceSpec *request, bool check, unsigned long seed, PieceMade *piece) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    PieceResult result = piece_build_plan(&request->request, &piece->loop.plan);
    if (result == PIECE_OK) {
        result = compose_version(request, check, seed, &piece->loop);
    }
    if (result == PIECE_OK) {
        result = api_piece_compose_ending(request, check, seed, piece);
    }
    return result;
}

static bool piece_cancelled(const PieceMade *piece) {
    return (piece->loop.composed && piece->loop.outcome.cancelled) || (piece->ending.composed && piece->ending.outcome.cancelled);
}

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
        if (strcmp(request->sections[s].id, id) == 0) {
            return request->sections[s].fill;
        }
    }
    return PIECE_FILL_EMPTY;
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
static ComposerStatus open_edit(const PieceSpec *request, const PieceEdit *edit, PiecePlan **out, FillTarget **targets,
                                size_t *count, char *what, size_t what_size, ComposerError *error) {
    PlanJsonError plan_error;
    PiecePlan *plan = NULL;
    *out = NULL;
    *targets = NULL;
    *count = 0;
    if (plan_from_json(edit->text, &plan, &plan_error) != PLAN_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the loop plan: %s", plan_error.message);
    }
    char member[64];
    NodeId node = piece_section_find(plan, edit->section);
    if (node == PLAN_NODE_NONE) {
        plan_destroy(plan);
        snprintf(member, sizeof member, "%s/section", edit->path);
        return api_fail(error, COMPOSER_ERR_REQUEST, member, "the loop plan has no section '%s'", edit->section);
    }
    snprintf(member, sizeof member, "%s/lanes", edit->path);
    bool theme = request_fill(request, edit->section) == PIECE_FILL_THEME;
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
            return status != COMPOSER_OK ? status : fail_alloc(error);
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
    bool refresh = piece_section_holds_body_start(plan, node);
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
        if (plan_from_json(edit->text, &loop->plan, &plan_error) != PLAN_OK) {
            *result = PIECE_ERR_BAD_REQUEST;
            return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the loop plan: %s", plan_error.message);
        }
        return COMPOSER_OK;
    }
    PiecePlan *plan = NULL;
    FillTarget *targets = NULL;
    size_t count = 0;
    char what[96];
    ComposeConfig config;
    make_config(request, check, seed, &config);
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
        return fail_cancelled(error);
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

/* One try from a seed: the loop plan written again, then the ending version built from it with seed + 20. */
static ComposerStatus regenerate_piece(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                       PieceMade *piece, PieceResult *result, ComposerError *error) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    ComposerStatus status = api_piece_regenerate_loop(request, edit, check, seed, &piece->loop, result, error);
    if (status == COMPOSER_OK && *result == PIECE_OK) {
        *result = api_piece_compose_ending(request, check, seed, piece);
    }
    return status;
}

/* ------------------------------------------------------------- reading the plans */

bool api_piece_loop_region(const PiecePlan *plan, Rational *start, Rational *end) {
    bool found = false;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            if (constraint->kind != PLAN_CONSTRAINT_PIECE_PART) {
                continue;
            }
            int part = constraint->params.piece_part.part;
            PlanSpan span = plan_node_span(plan, id);
            Rational span_end;
            if ((part != (int)PIECE_PART_BODY && part != (int)PIECE_PART_TURNAROUND) ||
                plan_span_end(span, &span_end) != PLAN_OK) {
                continue;
            }
            if (!found || (part == (int)PIECE_PART_BODY && rational_cmp(span.start, *start) < 0)) {
                *start = span.start;
            }
            if (!found || rational_cmp(span_end, *end) > 0) {
                *end = span_end;
            }
            found = true;
        }
    }
    return found;
}

static cJSON *time_json(Rational time) {
    char text[48];
    snprintf(text, sizeof text, "%lld/%lld", time.num, time.den);
    return cJSON_CreateString(text);
}

cJSON *api_piece_loop_json(const PiecePlan *plan) {
    Rational start;
    Rational end;
    if (!api_piece_loop_region(plan, &start, &end)) {
        return NULL;
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddItemToObject(object, "start", time_json(start)) ||
        !cJSON_AddItemToObject(object, "end", time_json(end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_piece_plan_object(const PiecePlan *plan) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    cJSON *object = text ? cJSON_Parse(text) : NULL;
    free(text);
    return object;
}

typedef struct {
    const PieceSection *section;
    const char *source;
    Rational start;
    bool has_cadence;
    CadenceType cadence;
    const char *heard; /* the cadence the Analyzer heard, NULL without a check */
    bool kept;         /* a regenerate left the section as it was: it was neither heard nor checked */
} Row;

/* The sections of a version in time order, with the cadence each planned and, with a check, the one heard. */
static size_t collect_rows(const PieceSpec *request, const PieceVersion *version, Row *rows) {
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    size_t next_item = 0;
    size_t count = 0;
    const PiecePlan *plan = version->plan;
    for (NodeId id = 1; id <= plan_node_count(plan) && count < PIECE_ROWS_MAX; id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION ||
            plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_SEAM)) {
            continue;
        }
        const PlanConstraint *part = NULL;
        const PlanConstraint *cadence = NULL;
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            part = constraint->kind == PLAN_CONSTRAINT_PIECE_PART ? constraint : part;
            cadence = constraint->kind == PLAN_CONSTRAINT_CADENCE && !cadence ? constraint : cadence;
        }
        if (!part) {
            continue;
        }
        Row *row = &rows[count];
        memset(row, 0, sizeof(*row));
        for (size_t s = 0; s < request->request.section_count; s++) {
            if (strcmp(request->sections[s].id, part->params.piece_part.id) == 0) {
                row->section = &request->sections[s];
                row->source = request->source[s];
            }
        }
        if (!row->section) {
            continue;
        }
        row->start = plan_node_span(plan, id).start;
        for (size_t h = 0; h < version->outcome.section_count; h++) {
            if (version->outcome.sections[h].section == id && !version->outcome.sections[h].in_scope) {
                row->kept = true;
            }
        }
        row->has_cadence = cadence != NULL;
        row->cadence = cadence ? cadence->params.cadence : CADENCE_TYPE_PERFECT_AUTHENTIC;
        if (cadence && check) {
            size_t seen = 0;
            for (size_t i = 0; i < check->item_count; i++) {
                if (check->items[i].kind == ANALYZER_CHECK_CADENCE && seen++ == next_item) {
                    row->heard = check->items[i].heard;
                }
            }
            next_item++;
        }
        count++;
    }
    return count;
}

static cJSON *ratio_object(unsigned long numerator, unsigned long denominator) {
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddNumberToObject(object, "numerator", (double)numerator) ||
        !cJSON_AddNumberToObject(object, "denominator", (double)denominator)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *string_or_null(const char *text) {
    return text ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *analyzer_check_json(const ComposeOutcome *outcome) {
    if (outcome->analyzer_check_count == 0) {
        return cJSON_CreateNull();
    }
    const AnalyzerCheck *check = &outcome->analyzer_checks[0];
    cJSON *object = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && cJSON_AddNumberToObject(object, "planned", (double)check->planned) &&
              cJSON_AddNumberToObject(object, "agreed", (double)check->agreed) &&
              cJSON_AddBoolToObject(object, "contradicts", check->contradicts) && add(object, "items", items);
    for (size_t i = 0; ok && i < check->item_count; i++) {
        const AnalyzerCheckItem *item = &check->items[i];
        cJSON *entry = cJSON_CreateObject();
        ok = entry &&
             cJSON_AddStringToObject(entry, "kind",
                                     item->kind == ANALYZER_CHECK_CADENCE ? "cadence"
                                     : item->kind == ANALYZER_CHECK_KEY   ? "key"
                                                                          : "violation") &&
             cJSON_AddNumberToObject(entry, "bar", (double)item->bar) && cJSON_AddStringToObject(entry, "planned", item->planned) &&
             cJSON_AddStringToObject(entry, "heard", item->heard) && cJSON_AddBoolToObject(entry, "agreed", item->agreed);
        if (!ok || !cJSON_AddItemToArray(items, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *seam_json(const ComposeOutcome *outcome) {
    if (!outcome->seam_planned) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddBoolToObject(object, "progression", outcome->seam.progression) ||
        !cJSON_AddBoolToObject(object, "no_parallels", outcome->seam.no_parallels) ||
        !cJSON_AddBoolToObject(object, "step", outcome->seam.step) ||
        !cJSON_AddBoolToObject(object, "leading_tone", outcome->seam.leading_tone) ||
        !cJSON_AddNumberToObject(object, "held", (double)outcome->seam.held)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_piece_version_json(const PieceSpec *request, const PieceVersion *version) {
    Row rows[PIECE_ROWS_MAX];
    size_t count = collect_rows(request, version, rows);
    const ComposeOutcome *outcome = &version->outcome;
    cJSON *object = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    bool ok = add(object, "plan", cJSON_CreateNull()) && add(object, "sections", sections);
    for (size_t i = 0; ok && i < count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry && cJSON_AddStringToObject(entry, "id", rows[i].section->id) &&
             cJSON_AddStringToObject(entry, "part", piece_part_name(rows[i].section->part)) &&
             cJSON_AddStringToObject(entry, "letter", rows[i].section->letter) && add(entry, "start", time_json(rows[i].start)) &&
             cJSON_AddNumberToObject(entry, "beats", (double)rows[i].section->beats) &&
             add(entry, "cadence", string_or_null(rows[i].has_cadence ? plan_cadence_name(rows[i].cadence) : NULL)) &&
             add(entry, "heard", string_or_null(rows[i].heard)) && (!rows[i].kept || cJSON_AddBoolToObject(entry, "in_scope", false)) &&
             cJSON_AddStringToObject(entry, "fill", piece_fill_name(rows[i].section->fill)) &&
             cJSON_AddStringToObject(entry, "source", rows[i].source);
        if (!ok || !cJSON_AddItemToArray(sections, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    ok = ok && add(object, "score", ratio_object(outcome->score.numerator, outcome->score.denominator)) &&
         add(object, "seam", seam_json(outcome)) && add(object, "analyzer_check", analyzer_check_json(outcome)) &&
         add(object, "style_distance", outcome->style_measured ? api_style_distance_json(&outcome->style_distance) : cJSON_CreateNull());
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* ------------------------------------------------------------- the result */

static GrowBrief make_brief(const PieceSpec *request) {
    GrowBrief brief;
    memset(&brief, 0, sizeof(brief));
    brief.tempo = request->tempo;
    brief.tempo_from = request->tempo_from;
    brief.minor = request->request.key.mode == KEY_STRUCTURE_MODE_MINOR;
    brief.mode_from = request->mode_from;
    brief.has_density = composer_style_density(request->style, &brief.density);
    brief.without = request->without;
    brief.without_count = request->without_count;
    return brief;
}

typedef struct {
    bool midi;
    bool musicxml;
    bool ending_musicxml;
    int explain; /* 0 none, 1 text, 2 json */
} PieceOutputs;

/* The explanation of both versions as the command writes it: text with a heading for each, or JSON {"loop", "ending"}. */
static char *explanation_text(const PieceMade *piece, bool json) {
    Explanation loop;
    Explanation ending;
    if (explain_build(piece->loop.plan, &piece->loop.outcome, &loop) != EXPLAIN_OK) {
        return NULL;
    }
    if (explain_build(piece->ending.plan, &piece->ending.outcome, &ending) != EXPLAIN_OK) {
        explain_destroy(&loop);
        return NULL;
    }
    char *loop_text = json ? explain_to_json(&loop) : explain_to_text(&loop);
    char *ending_text = json ? explain_to_json(&ending) : explain_to_text(&ending);
    char *joined = NULL;
    if (loop_text && ending_text) {
        const char *format = json ? "{\"loop\":%s,\"ending\":%s}\n" : "== loop version ==\n%s\n== ending version ==\n%s\n";
        int size = snprintf(NULL, 0, format, loop_text, ending_text);
        joined = size > 0 ? malloc((size_t)size + 1) : NULL;
        if (joined) {
            snprintf(joined, (size_t)size + 1, format, loop_text, ending_text);
        }
    }
    free(loop_text);
    free(ending_text);
    explain_destroy(&loop);
    explain_destroy(&ending);
    return joined;
}

/*
 * The result document: {schema, seed, key, meter, brief, loop, [regenerated,] versions: {loop, ending}}, as the command prints
 * it, with no plan paths (the plans are in "plans") and no files.
 */
static cJSON *piece_document(const PieceSpec *request, const PieceMade *piece, const PieceEdit *edit,
                             const cJSON *regenerate_section, bool from) {
    char key[32];
    char lanes[64];
    char meter[24];
    GrowBrief brief = make_brief(request);
    util_key_name(analysis_key_tonic_pitch_class(&request->request.key), request->request.key.mode == KEY_STRUCTURE_MODE_MINOR,
                  key, sizeof key);
    snprintf(meter, sizeof meter, "%d/%d", request->request.meter.beats_per_bar, request->request.meter.beat_unit);
    cJSON *document = cJSON_CreateObject();
    cJSON *loop_region = api_piece_loop_json(piece->loop.plan);
    bool ok = document && cJSON_AddStringToObject(document, "schema", "ursatz-piece") &&
              cJSON_AddNumberToObject(document, "seed", (double)piece->seed) && cJSON_AddStringToObject(document, "key", key) &&
              cJSON_AddStringToObject(document, "meter", meter) && add(document, "brief", api_brief_json(&brief)) &&
              add(document, "loop", loop_region ? loop_region : cJSON_CreateNull());
    if (ok && from) {
        cJSON *regenerated = cJSON_CreateNull();
        if (regenerate_section) {
            api_piece_lanes_name(edit, lanes, sizeof lanes);
            cJSON_Delete(regenerated);
            regenerated = cJSON_CreateObject();
            if (!regenerated || !cJSON_AddStringToObject(regenerated, "section", regenerate_section->valuestring) ||
                !cJSON_AddStringToObject(regenerated, "lane", lanes)) {
                cJSON_Delete(regenerated);
                regenerated = NULL;
            }
        }
        ok = add(document, "regenerated", regenerated);
    }
    cJSON *versions = cJSON_CreateObject();
    ok = ok && add(versions, "loop", api_piece_version_json(request, &piece->loop)) &&
         add(versions, "ending", api_piece_version_json(request, &piece->ending)) && add(document, "versions", versions);
    if (!ok) {
        cJSON_Delete(versions);
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* The loop version's MIDI file and MusicXML: the MIDI file has the loopStart and loopEnd markers. */
static ComposerStatus render_loop(const PieceSpec *request, const PieceMade *piece, const PieceOutputs *outputs,
                                  ComposerResponse *out) {
    if (!outputs->midi && !outputs->musicxml) {
        return COMPOSER_OK;
    }
    PlanExportOptions loop_options = request->export_options;
    Rational start;
    Rational end;
    if (api_piece_loop_region(piece->loop.plan, &start, &end)) {
        loop_options.loop_markers = true;
        loop_options.loop_start = start;
        loop_options.loop_end = end;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(piece->loop.plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, outputs->midi ? "/midi" : "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(&rendered, &loop_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == COMPOSER_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(&rendered, &request->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

ComposerStatus api_piece_musicxml(const PieceSpec *request, const PiecePlan *plan, char **out, ComposerError *error) {
    RenderedPlan rendered;
    PlanExportError detail;
    *out = NULL;
    if (plan_export_render(plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (rendered_plan_musicxml_text(&rendered, &request->export_options, out, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

/*
 * Adds "plans": {"loop", "ending"}, with an explanation "explanation", and with ending_musicxml the ending version's MusicXML
 * as "ending_musicxml" to the document.
 */
static ComposerStatus add_outputs(cJSON *document, const PieceSpec *request, const PieceMade *piece, const PieceOutputs *outputs,
                                  ComposerResponse *out) {
    cJSON *plans = cJSON_CreateObject();
    if (!add(plans, "loop", api_piece_plan_object(piece->loop.plan)) ||
        !add(plans, "ending", api_piece_plan_object(piece->ending.plan)) || !add(document, "plans", plans)) {
        cJSON_Delete(plans);
        return fail_alloc(&out->error);
    }
    if (outputs->explain) {
        char *text = explanation_text(piece, outputs->explain == 2);
        if (!text || !cJSON_AddStringToObject(document, "explanation", text)) {
            free(text);
            return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the explanation");
        }
        free(text);
    }
    if (outputs->ending_musicxml) {
        char *text = NULL;
        ComposerStatus status = api_piece_musicxml(request, piece->ending.plan, &text, &out->error);
        if (status != COMPOSER_OK) {
            return status;
        }
        bool added = cJSON_AddStringToObject(document, "ending_musicxml", text) != NULL;
        free(text);
        if (!added) {
            return fail_alloc(&out->error);
        }
    }
    PlanJsonError detail;
    out->plan_json = plan_to_json(piece->loop.plan, &detail);
    if (!out->plan_json) {
        return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the plan: %s", detail.message);
    }
    return COMPOSER_OK;
}

/* ------------------------------------------------------------- the entry */

/* The piece request members of the API request that are not the piece request: what to make besides the document. */
static ComposerStatus read_outputs(const cJSON *request, PieceOutputs *outputs, bool *plain_midi, ComposerError *error) {
    bool present;
    const char *explain = NULL;
    memset(outputs, 0, sizeof(*outputs));
    *plain_midi = false;
    if (!api_member_bool(request, "midi", &outputs->midi, &present, error) ||
        !api_member_bool(request, "musicxml", &outputs->musicxml, &present, error) ||
        !api_member_bool(request, "ending_musicxml", &outputs->ending_musicxml, &present, error) ||
        !api_member_bool(request, "plain_midi", plain_midi, &present, error) ||
        !api_member_string(request, "explain", &explain, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            outputs->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            outputs->explain = 2;
        } else {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    return COMPOSER_OK;
}

/* The loop plan to work on, as JSON text, and the section to write again: "from" and "regenerate". */
static ComposerStatus read_edit(const cJSON *request, char **text, PieceEdit *edit, const cJSON **section, ComposerError *error) {
    const cJSON *from = cJSON_GetObjectItemCaseSensitive(request, "from");
    const cJSON *regenerate = cJSON_GetObjectItemCaseSensitive(request, "regenerate");
    memset(edit, 0, sizeof(*edit));
    edit->path = "/regenerate";
    *text = NULL;
    *section = NULL;
    bool has_regenerate = regenerate && !cJSON_IsNull(regenerate);
    if (!from || cJSON_IsNull(from)) {
        return has_regenerate ? api_fail(error, COMPOSER_ERR_REQUEST, "/regenerate", "\"regenerate\" goes with \"from\", the loop plan")
                              : COMPOSER_OK;
    }
    if (!cJSON_IsObject(from)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "\"from\" must be a plan object");
    }
    *text = cJSON_PrintUnformatted(from);
    if (!*text) {
        return fail_alloc(error);
    }
    edit->text = *text;
    if (!has_regenerate) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsObject(regenerate)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/regenerate", "\"regenerate\" must be {\"section\": <id>, \"lanes\": [...]}");
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(regenerate, "section");
    if (item && !cJSON_IsNull(item)) {
        if (!cJSON_IsString(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/regenerate/section", "\"section\" must be a section id");
        }
        edit->section = item->valuestring;
        *section = item;
    }
    return api_piece_read_lanes(cJSON_GetObjectItemCaseSensitive(regenerate, "lanes"), "/regenerate/lanes", edit, error);
}

ComposerStatus api_piece_fail_compose(const PieceSpec *request, PieceResult result, const PieceMade *last,
                                      unsigned long first_seed, unsigned long last_seed, ComposerError *error) {
    if (result == PIECE_ERR_MOTIVE_TOO_LONG) {
        size_t theme_bars = 8;
        size_t idea_bars = 2;
        (void)piece_basic_idea(&request->request, &theme_bars, &idea_bars);
        return api_fail_motif_long(error, "/motive", request->request.motive, &request->request.meter, theme_bars, idea_bars);
    }
    if (result == PIECE_ERR_BAD_REQUEST) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/request",
                        "this piece cannot be built as asked: a theme fill must be as long as its section, in the same meter and "
                        "mode; a progression must be readable and fit its section");
    }
    if (result == PIECE_ERR_STUCK) {
        const PieceVersion *stuck = last->ending.composed && last->ending.outcome.stuck ? &last->ending : &last->loop;
        const ComposeRejection *rejection =
            stuck->outcome.rejection_count > 0 ? &stuck->outcome.rejections[stuck->outcome.rejection_count - 1] : NULL;
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                        "no piece could be composed from seeds %lu to %lu (the last was stuck in the %s version, %s stage%s%s)%s",
                        first_seed, last_seed, stuck == &last->ending ? "ending" : "loop",
                        rejection && rejection->stage[0] ? rejection->stage : "?", rejection ? ", " : "",
                        rejection ? rejection->reason : "",
                        rejection && (strcmp(rejection->stage, "seam") == 0 || strcmp(rejection->stage, "structure") == 0 ||
                                      !rejection->stage[0])
                            ? "; a section of whole bars needs 2 bars at least to hold its cadence"
                            : "");
    }
    return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", piece_result_name(result));
}

ComposerStatus composer_api_piece(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    PieceSpec *request = calloc(1, sizeof(*request));
    PieceMade piece;
    PieceEdit edit;
    PieceOutputs outputs;
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
    ComposerStatus status = request ? api_parse_request(request_json, &root, &out->error) : fail_alloc(&out->error);
    if (status == COMPOSER_OK) {
        const cJSON *corpus = cJSON_GetObjectItemCaseSensitive(root, "corpus");
        if (corpus && !cJSON_IsNull(corpus)) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/corpus", "a piece takes no corpus");
        }
    }
    if (status == COMPOSER_OK) {
        status = read_outputs(root, &outputs, &plain_midi, &out->error);
    }
    if (status == COMPOSER_OK) {
        if (!api_member_bool(root, "check", &check, &present, &out->error) ||
            !api_member_whole(root, "seed", 0, PIECE_SEED_MAX, &seed, &seeded, &out->error) ||
            !api_member_string(root, "style", &style, &style_present, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == COMPOSER_OK && !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "request"))) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/request", "the request needs \"request\": the piece request");
    }
    if (status == COMPOSER_OK) {
        status = read_edit(root, &plan_text, &edit, &section, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_piece_read_request(request, cJSON_GetObjectItemCaseSensitive(root, "request"), "/request", style, plain_midi,
                                        job, &out->error);
    }
    PieceResult result = PIECE_ERR_STUCK;
    unsigned long last_seed = seed;
    /* The seeds are tried one after another; a stuck run is passed over, and a request no seed can mend ends the search. */
    for (unsigned attempt = 0; status == COMPOSER_OK && attempt < PIECE_SEED_TRIES && result == PIECE_ERR_STUCK; attempt++) {
        if (job_step(job, "seed", attempt, PIECE_SEED_TRIES)) {
            status = fail_cancelled(&out->error);
            break;
        }
        api_piece_made_destroy(&piece);
        last_seed = (seed + attempt) & PIECE_SEED_MAX;
        made = true;
        if (edit.text) {
            status = regenerate_piece(request, &edit, check, last_seed, &piece, &result, &out->error);
        } else {
            result = compose_piece(request, check, last_seed, &piece);
        }
        if (status == COMPOSER_OK && piece_cancelled(&piece)) {
            status = fail_cancelled(&out->error);
        }
    }
    if (status == COMPOSER_OK && result != PIECE_OK) {
        status = api_piece_fail_compose(request, result, &piece, seed, last_seed, &out->error);
    }
    if (status == COMPOSER_OK) {
        cJSON *document = piece_document(request, &piece, &edit, section, edit.text != NULL);
        status = document ? add_outputs(document, request, &piece, &outputs, out) : fail_alloc(&out->error);
        if (status == COMPOSER_OK) {
            status = render_loop(request, &piece, &outputs, out);
        }
        if (status == COMPOSER_OK) {
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
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
