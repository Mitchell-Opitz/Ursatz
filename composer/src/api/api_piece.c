#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/form_figure.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/piece_reopen.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "grow/grow_internal.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

/*
 * The piece entry (docs/api.md): the piece request composed, or its loop plan regenerated in one section, with the ending
 * version built from the loop's. The seeds are tried one after another; the plans, the result document, the MIDI file and the
 * MusicXML of the loop version are made in memory.
 */

static ComposerStatus fail_cancelled(ComposerError *error) {
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

const PiecePlan *api_piece_version_output_plan(const PieceVersion *version) {
    return version->performed ? version->performance.plan : version->plan;
}

void api_piece_made_destroy(PieceMade *piece) {
    for (size_t i = 0; i < API_PIECE_VERSIONS_MAX; i++) {
        api_piece_version_destroy(&piece->versions[i]);
    }
    piece_parts_destroy(&piece->parts);
}

/* How many versions the form of the request has, and the name of one in the document: "loop" and "ending", or "piece". */
static size_t version_count(const PieceSpec *request) {
    const char *names[API_PIECE_VERSIONS_MAX];
    const char *labels[API_PIECE_VERSIONS_MAX];
    return api_piece_version_names(request->form, names, labels, API_PIECE_VERSIONS_MAX);
}

static const char *version_name(const PieceSpec *request, size_t index) {
    const char *names[API_PIECE_VERSIONS_MAX];
    const char *labels[API_PIECE_VERSIONS_MAX];
    return api_piece_version_names(request->form, names, labels, API_PIECE_VERSIONS_MAX) > index ? names[index] : "";
}

const char *api_piece_first_version_name(const PieceSpec *request) {
    return version_name(request, 0);
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
    config->knowledge = request->knowledge;
}

static PieceResult compose_version(const PieceSpec *request, bool check, unsigned long seed, PieceVersion *version) {
    ComposeConfig config;
    make_config(request, check, seed, &config);
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
static PieceResult compose_piece(PieceSpec *request, bool check, unsigned long seed, PieceMade *piece) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    piece->version_count = version_count(request);
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
        make_config(request, check, seed, &config);
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

static bool piece_cancelled(const PieceMade *piece) {
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
static NodeId sized_section_find(const PiecePlan *plan, const char *name) {
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
static ComposerStatus open_edit(const PieceSpec *request, const PieceEdit *edit, PiecePlan **out, FillTarget **targets,
                                size_t *count, char *what, size_t what_size, ComposerError *error) {
    PlanJsonError plan_error;
    PiecePlan *plan = NULL;
    bool sized = !request->form->request_sections;
    *out = NULL;
    *targets = NULL;
    *count = 0;
    if (plan_from_json_resolved(request->knowledge, edit->text, &plan, &plan_error) != PLAN_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the %s: %s", sized ? "plan" : "loop plan",
                        plan_error.message);
    }
    char member[64];
    NodeId node = sized ? sized_section_find(plan, edit->section) : piece_section_find(plan, edit->section);
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
        if (plan_from_json_resolved(request->knowledge, edit->text, &loop->plan, &plan_error) != PLAN_OK) {
            *result = PIECE_ERR_BAD_REQUEST;
            return api_fail(error, COMPOSER_ERR_REQUEST, "/from", "cannot read the %s: %s",
                            request->form->request_sections ? "loop plan" : "plan", plan_error.message);
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

/* One try from a seed: the first plan written again, then the ending version built from it with seed + 20 when the form has one. */
static ComposerStatus regenerate_piece(const PieceSpec *request, const PieceEdit *edit, bool check, unsigned long seed,
                                       PieceMade *piece, PieceResult *result, ComposerError *error) {
    memset(piece, 0, sizeof(*piece));
    piece->seed = seed;
    piece->version_count = version_count(request);
    ComposerStatus status = api_piece_regenerate_loop(request, edit, check, seed, &piece->versions[0], result, error);
    if (status == COMPOSER_OK && *result == PIECE_OK && piece->version_count == 2) {
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
        const SectionDescription *section = plan_node_section(plan, id);
        if (!section->has_role || section->role < SECTION_ROLE_INTRO || section->role > SECTION_ROLE_ENDING) {
            continue;
        }
        {
            int part = (int)(section->role - SECTION_ROLE_INTRO);
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
    const PieceSection *section;     /* the section the request gave, for a form whose sections it gives */
    const SectionDescription *spec;     /* the section of the form's size, for a form with sizes */
    const FormSpecSize *size;
    const char *source;
    Rational start;
    Rational length;
    bool has_cadence;
    CadenceType cadence;
    const char *heard; /* the cadence the Analyzer heard, NULL without a check */
    const PiecePlan *plan; /* a form with sizes: the plan and the section's node, for the pedal and the changes */
    NodeId node;
    char key_heard[16]; /* the key the Analyzer heard at the section's start over the whole piece, "" when none */
    char key[16];       /* the key the plan gives the section's start (R-493), "" when the plan has none */
    char region[128];   /* the region as the request asked for it or the form names it (R-610), "" for none */
    bool key_flats;     /* that key is spelled with flats: the figure's skeleton names its tones so */
    bool pedal;        /* a harmony span in the section's first half was written over a pedal */
    bool kept;         /* a regenerate left the section as it was: it was neither heard nor checked */
    const char *part;      /* a performed compound form: the name of the part the section is in, else NULL */
    const char *repeat_of; /* a performed section that writes another out again: the first time's name, else NULL */
    bool has_upbeat;       /* the section leads into the next: its last chord and the next section's first (R-486) */
    char upbeat_chord[PLAN_ROMAN_NUMERAL_MAX];
    char upbeat_next[PLAN_ROMAN_NUMERAL_MAX];
    int connective;        /* -1 the section has no CONNECTIVE constraint, else whether it was written */
} Row;

/* The cadence heard for the next cadence the plan has, from the Analyzer check; NULL without a check. */
static const char *heard_cadence(const AnalyzerCheck *check, size_t next_item) {
    size_t seen = 0;
    for (size_t i = 0; check && i < check->item_count; i++) {
        if (check->items[i].kind == ANALYZER_CHECK_CADENCE && seen++ == next_item) {
            return check->items[i].heard;
        }
    }
    return NULL;
}

static bool section_kept(const PieceVersion *version, NodeId id) {
    for (size_t h = 0; h < version->outcome.section_count; h++) {
        if (version->outcome.sections[h].section == id && !version->outcome.sections[h].in_scope) {
            return true;
        }
    }
    return false;
}

/*
 * Whether a regenerate left a section of a form with sizes as it was: the run composed something, and the section was not one
 * of the open ones it heard (a section already written is not open) or was heard out of scope.
 */
static bool size_section_kept(const PieceVersion *version, NodeId id) {
    if (!version->composed) {
        return false;
    }
    for (size_t h = 0; h < version->outcome.section_count; h++) {
        if (version->outcome.sections[h].section == id) {
            return !version->outcome.sections[h].in_scope;
        }
    }
    return true;
}

/* The type of the last cadence the compose run heard in a section, when it arrives in the section's last bar; else NULL. */
static const char *last_bar_cadence(const PieceSpec *request, const PieceVersion *version, NodeId id, PlanSpan span) {
    Rational bar;
    Rational end;
    Rational last;
    unsigned top = request->request.meter.beats_per_bar;
    unsigned bottom = request->request.meter.beat_unit;
    if (top == 0 || bottom == 0 || rational_make((long long)top, (long long)bottom, &bar) != RATIONAL_OK ||
        plan_span_end(span, &end) != PLAN_OK || rational_sub(end, bar, &last) != RATIONAL_OK) {
        return NULL;
    }
    for (size_t h = 0; h < version->outcome.section_count; h++) {
        const ComposeHeard *section = &version->outcome.sections[h];
        if (section->section != id || !section->in_scope) {
            continue;
        }
        const char *type = NULL;
        for (size_t c = 0; c < section->heard.cadence_count; c++) {
            Rational arrival = section->heard.cadences[c].arrival;
            if (rational_cmp(arrival, last) >= 0 && rational_cmp(arrival, end) < 0) {
                type = plan_cadence_name((CadenceType)section->heard.cadences[c].type);
            }
        }
        return type;
    }
    return NULL;
}

/* Whether a harmony span in the first half of the section was written over a pedal. */
static bool section_pedal(const PiecePlan *plan, PlanSpan span) {
    Rational half;
    if (rational_make(span.length.num, span.length.den * 2, &half) != RATIONAL_OK) {
        return false;
    }
    PlanSpan first = {span.start, half};
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, first, NULL, 0);
    LaneSpanId *ids = found ? calloc(found, sizeof(*ids)) : NULL;
    bool pedal = false;
    if (ids) {
        plan_lane_find(plan, PLAN_LANE_HARMONY, 0, first, ids, found);
    }
    for (size_t i = 0; ids && i < found && !pedal; i++) {
        for (size_t c = 0; c < plan_lane_span_constraint_count(plan, ids[i]); c++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, ids[i], c);
            pedal = pedal || (constraint->kind == PLAN_CONSTRAINT_HARMONY_SHAPE && constraint->params.harmony_shape.pedal);
        }
    }
    free(ids);
    return pedal;
}

/* The key heard at the start of a span in the keys of the whole piece, as "C major"; "" when none. */
static void key_heard_at(const FormHeardKey *keys, size_t key_count, Rational start, char *out, size_t size) {
    const FormHeardKey *at = NULL;
    for (size_t i = 0; i < key_count; i++) {
        if (rational_cmp(keys[i].start, start) <= 0) {
            at = &keys[i];
        }
    }
    out[0] = '\0';
    if (at) {
        util_key_name(analysis_key_tonic_pitch_class(&at->key), at->key.material, out, size);
    }
}

/* The key the plan's key lane gives at the start of a span, as "C major"; "" when the lane has none there. */
static void planned_key_at(const PiecePlan *plan, PlanSpan section, char *out, size_t size, bool *flats) {
    PlanSpan up_to = {{0, 1}, {0, 1}};
    Rational start = section.start;
    Rational end_of_section;
    out[0] = '\0';
    if (plan_span_end(section, &end_of_section) != PLAN_OK ||
        rational_make(end_of_section.num, end_of_section.den, &up_to.length) != RATIONAL_OK) {
        return;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, up_to, NULL, 0);
    LaneSpanId *ids = found ? calloc(found, sizeof(*ids)) : NULL;
    if (!ids) {
        return;
    }
    plan_lane_find(plan, PLAN_LANE_KEY, 0, up_to, ids, found);
    const PlanLanePayload *at = NULL;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        if (rational_cmp(span.start, start) <= 0 && plan_span_end(span, &end) == PLAN_OK && rational_cmp(start, end) < 0) {
            at = plan_lane_span_payload(plan, ids[i]);
        }
    }
    if (at) {
        util_key_name(analysis_key_tonic_pitch_class(&at->key), at->key.material, out, size);
        *flats = util_key_prefers_flats(analysis_key_tonic_pitch_class(&at->key), at->key.material);
    }
    free(ids);
}

/* The sections of a size of a form with sizes, each matched to the plan's section of its name, in order. */
static size_t collect_composed_rows(const PieceSpec *request, const PieceVersion *version, const FormSpecSize *size, Row *rows) {
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    const PiecePlan *plan = version->plan;
    size_t next_item = 0;
    size_t count = 0;
    FormHeardKey *keys = NULL;
    size_t key_count = 0;
    if (version->plan && form_hear_keys(plan, request->knowledge, plan_node_span(plan, plan_root(plan)), &keys,
                                       &key_count) != FORM_HEAR_OK) {
        keys = NULL;
        key_count = 0;
    }
    PieceRegion regions[FORM_SPEC_MAX_SECTIONS];
    bool regional = false;
    size_t refused = 0;
    char why[64];
    if (!size || piece_resolve_overlay_regions(&request->request, size, request->knowledge, regions, &regional, &refused, why, sizeof why) !=
                     PIECE_OK) {
        regional = false;
    }
    for (size_t i = 0; size && i < size->section_count && count < PIECE_ROWS_MAX; i++) {
        NodeId id = sized_section_find(plan, size->sections[i].name);
        if (id == PLAN_NODE_NONE) {
            continue;
        }
        const SectionDescription *described = plan_node_section(plan, id);
        Row *row = &rows[count++];
        memset(row, 0, sizeof(*row));
        row->connective = -1;
        row->spec = &size->sections[i];
        row->size = size;
        row->start = plan_node_span(plan, id).start;
        row->length = plan_node_span(plan, id).length;
        row->kept = size_section_kept(version, id);
        row->has_cadence = described->has_cadence;
        row->cadence = described->has_cadence ? described->cadence : CADENCE_TYPE_PERFECT_AUTHENTIC;
        row->plan = plan;
        row->node = id;
        row->pedal = section_pedal(plan, plan_node_span(plan, id));
        row->connective = described->connective != SECTION_CONNECTIVE_NONE ? (described->connective_written ? 1 : 0) : -1;
        if (size->sections[i].leads_in && i + 1 < size->section_count) {
            NodeId next = sized_section_find(plan, size->sections[i + 1].name);
            char ignored[PLAN_ROMAN_NUMERAL_MAX];
            row->has_upbeat = next != PLAN_NODE_NONE &&
                              form_return_edge_numerals(plan, id, ignored, row->upbeat_chord, sizeof row->upbeat_chord) &&
                              form_return_edge_numerals(plan, next, row->upbeat_next, ignored, sizeof row->upbeat_next);
        }
        key_heard_at(keys, key_count, row->start, row->key_heard, sizeof row->key_heard);
        planned_key_at(plan, plan_node_span(plan, id), row->key, sizeof row->key, &row->key_flats);
        if (regional && regions[i].has_region) {
            snprintf(row->region, sizeof row->region, "%s", regions[i].text);
        } else if (size->sections[i].key_end.kind == SECTION_REGION_FORM || size->sections[i].key_start.kind == SECTION_REGION_FORM) {
            snprintf(row->region, sizeof row->region, "%s",
                     size->sections[i].key_end.kind == SECTION_REGION_FORM ? size->sections[i].key_end.form_region
                                                                              : size->sections[i].key_start.form_region);
        }
        if (described->has_cadence) {
            row->heard = heard_cadence(check, next_item++);
        }
        if (!check) {
            row->heard = last_bar_cadence(request, version, id, plan_node_span(plan, id));
        }
    }
    free(keys);
    return count;
}

/*
 * The rows of a form with sizes. A performed version has one row for each performed section: its start and length from the
 * performed plan, every other member from the source section in the composed plan, so a repeat's row is its source's at its
 * own start. A compound form's rows come from the part each performed section belongs to: its composed plan, its outcome
 * and the size of its form (an again part from the part it performs again).
 */
static size_t collect_size_rows(const PieceSpec *request, const PieceVersion *version, Row *rows) {
    if (!version->performed) {
        return collect_composed_rows(request, version, form_spec_size(request->form, request->request.bars, request->request.count), rows);
    }
    const PieceParts *parts = version->parts;
    const FormSpecSize *whole = form_spec_size(request->form, request->request.bars, request->request.count);
    Row composed[FORM_SPEC_MAX_PARTS_OF][PIECE_ROWS_MAX];
    size_t composed_count[FORM_SPEC_MAX_PARTS_OF] = {0};
    if (parts && whole) {
        for (size_t i = 0; i < request->form->part_count; i++) {
            if (request->form->parts[i].again != FORM_SPEC_NO_RETURN || !parts->forms[i] || !parts->plans[i]) {
                continue;
            }
            PieceVersion view;
            memset(&view, 0, sizeof(view));
            view.plan = parts->plans[i];
            view.outcome = parts->outcomes[i]; /* a shallow copy for reading: never destroyed */
            view.composed = true;
            const size_t part_bars = request->form->parts[i].bars ? request->form->parts[i].bars : whole->bars;
            composed_count[i] = collect_composed_rows(request, &view, form_spec_size(parts->forms[i], part_bars, 0), composed[i]);
        }
    } else if (!parts) {
        composed_count[0] = collect_composed_rows(request, version, whole, composed[0]);
    } else {
        return 0;
    }
    const PiecePerformance *performance = &version->performance;
    size_t performed = 0;
    for (size_t i = 0; i < performance->section_count && performed < PIECE_ROWS_MAX; i++) {
        size_t part = parts ? piece_parts_source(request->form, performance->sections[i].part) : 0;
        for (size_t j = 0; j < composed_count[part]; j++) {
            if (composed[part][j].node == performance->sections[i].source) {
                PlanSpan span = plan_node_span(performance->plan, performance->sections[i].node);
                rows[performed] = composed[part][j];
                rows[performed].start = span.start;
                rows[performed].length = span.length;
                if (parts && request->form->part_count > 0) {
                    rows[performed].part = request->form->parts[performance->sections[i].part].name;
                }
                /* The first time the same section of the same composed part was performed. */
                for (size_t earlier = 0; earlier < i; earlier++) {
                    size_t earlier_part = parts ? piece_parts_source(request->form, performance->sections[earlier].part) : 0;
                    if (earlier_part == part && performance->sections[earlier].source == performance->sections[i].source) {
                        rows[performed].repeat_of = performance->sections[earlier].name;
                        break;
                    }
                }
                performed++;
                break;
            }
        }
    }
    return performed;
}

/* The sections of a version in time order, with the cadence each planned and, with a check, the one heard. */
static size_t collect_rows(const PieceSpec *request, const PieceVersion *version, Row *rows) {
    if (!request->form->request_sections) {
        return collect_size_rows(request, version, rows);
    }
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    size_t next_item = 0;
    size_t count = 0;
    const PiecePlan *plan = version->plan;
    PieceRegion loop_regions[PIECE_STRUCTURE_MAX_SECTIONS];
    size_t loop_refused = 0;
    char loop_why[64];
    bool loop_regional = request->request.section_count > 0 &&
                         piece_resolve_regions(&request->request, request->knowledge, loop_regions, &loop_refused, loop_why, sizeof loop_why) ==
                             PIECE_OK;
    for (NodeId id = 1; id <= plan_node_count(plan) && count < PIECE_ROWS_MAX; id++) {
        if (!plan_has_node(plan, id) || plan_node_kind(plan, id) != PLAN_NODE_SECTION ||
            plan_section_has_role(plan, id, SECTION_ROLE_SEAM)) {
            continue;
        }
        const SectionDescription *described = plan_node_section(plan, id);
        const bool has_part = described->has_role && described->role >= SECTION_ROLE_INTRO && described->role <= SECTION_ROLE_ENDING;
        if (!has_part) {
            continue;
        }
        Row *row = &rows[count];
        memset(row, 0, sizeof(*row));
        for (size_t s = 0; s < request->request.section_count; s++) {
            if (strcmp(request->sections[s].description.id, described->id) == 0) {
                row->section = &request->sections[s];
                row->source = request->source[s];
                if (loop_regional && loop_regions[s].has_region) {
                    snprintf(row->region, sizeof row->region, "%s", loop_regions[s].text);
                }
            }
        }
        if (!row->section) {
            continue;
        }
        row->plan = plan;
        row->node = id;
        row->start = plan_node_span(plan, id).start;
        row->kept = section_kept(version, id);
        row->has_cadence = described->has_cadence;
        row->cadence = described->has_cadence ? described->cadence : CADENCE_TYPE_PERFECT_AUTHENTIC;
        if (described->has_cadence && check) {
            row->heard = heard_cadence(check, next_item);
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
              cJSON_AddBoolToObject(object, "contradicts", check->contradicts) && api_add(object, "items", items);
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

/*
 * Adds why a theme section is as long as it is (R-469), the sentence grow_construction_describe writes, as the row's
 * construction_text. A section without a construction has no member, so a piece of 4 and 8-bar themes reads as before.
 */
static bool add_construction_text(cJSON *entry, const Row *row) {
    char text[192];
    return !row->plan || grow_section_describe(row->plan, row->node, text, sizeof text) == 0 ||
           cJSON_AddStringToObject(entry, "construction_text", text) != NULL;
}

/* The key the plan's key lane gives near the end of a section, as "C major"; "" when the lane has none there. */
static void planned_key_near_end(const PiecePlan *plan, PlanSpan section, char *out, size_t size) {
    Rational offset;
    Rational at;
    PlanSpan last = section;
    bool flats = false;
    out[0] = '\0';
    if (rational_make(section.length.num * 3, section.length.den * 4, &offset) != RATIONAL_OK ||
        rational_add(section.start, offset, &at) != RATIONAL_OK || rational_sub(section.length, offset, &last.length) != RATIONAL_OK) {
        return;
    }
    last.start = at;
    planned_key_at(plan, last, out, size, &flats);
}

/*
 * The function of a row (R-510): {kind, independent, lyric, text}, null for a section with no SECTION_FUNCTION. A performed row
 * has the plan and node of the section it performs, so it takes that section's function.
 */
static cJSON *function_json(const Row *row) {
    SectionFunction function;
    if (!row->plan || !plan_section_function_of(row->plan, row->node, &function)) {
        return cJSON_CreateNull();
    }
    char text[192];
    switch (function.function) {
    case SECTION_FUNCTION_TRANSITION: {
        char start[16];
        char end[16];
        bool flats;
        planned_key_at(row->plan, plan_node_span(row->plan, row->node), start, sizeof start, &flats);
        planned_key_near_end(row->plan, plan_node_span(row->plan, row->node), end, sizeof end);
        const bool modulates = start[0] && end[0] && strcmp(start, end) != 0;
        snprintf(text, sizeof text, "%s%s%s, liquidating to the upbeat chord",
                 function.independent ? "an independent transition with its own motif" : "a transition grown from the main theme",
                 modulates ? ", modulating to " : "", modulates ? end : "");
        break;
    }
    case SECTION_FUNCTION_RETRANSITION:
        snprintf(text, sizeof text, "a retransition over the dominant pedal, ending on the main theme's head");
        break;
    case SECTION_FUNCTION_SUBORDINATE:
        snprintf(text, sizeof text, "a subordinate theme%s", function.lyric ? ", lyric" : "");
        break;
    default:
        snprintf(text, sizeof text, "a coda of 4, 2 and 2 bars, each closing on the tonic");
        break;
    }
    cJSON *entry = cJSON_CreateObject();
    if (!entry || !cJSON_AddStringToObject(entry, "kind", section_function_kind_name(function.function)) ||
        !cJSON_AddBoolToObject(entry, "independent", function.independent) || !cJSON_AddBoolToObject(entry, "lyric", function.lyric) ||
        !cJSON_AddStringToObject(entry, "text", text)) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

/* A row of a form whose sections the request gives: {id, part, function, letter, start, beats, cadence, heard, [in_scope], fill, source, [construction_text]}. */
static cJSON *loop_row_json(const Row *row) {
    cJSON *entry = cJSON_CreateObject();
    bool ok = entry && cJSON_AddStringToObject(entry, "id", row->section->description.id) &&
              cJSON_AddStringToObject(entry, "part", piece_part_name(piece_section_part(row->section))) &&
              api_add(entry, "function", function_json(row)) &&
              cJSON_AddStringToObject(entry, "letter", row->section->description.letter) && api_add(entry, "start", time_json(row->start)) &&
              cJSON_AddNumberToObject(entry, "beats", (double)row->section->description.beats) &&
              api_add(entry, "cadence", string_or_null(row->has_cadence ? plan_cadence_name(row->cadence) : NULL)) &&
              api_add(entry, "heard", string_or_null(row->heard)) && (!row->kept || cJSON_AddBoolToObject(entry, "in_scope", false)) &&
              cJSON_AddStringToObject(entry, "fill", piece_fill_name(row->section->fill)) &&
              cJSON_AddStringToObject(entry, "source", row->source) &&
              api_add(entry, "region", string_or_null(row->region[0] ? row->region : NULL)) && add_construction_text(entry, row);
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

/* The length of a span in beats of the request's meter. */
static double span_beats(const PieceSpec *request, Rational length) {
    return length.den != 0 ? (double)length.num * (double)request->request.meter.beat_unit / (double)length.den : 0;
}

/* The melody notes written over a section. */
static size_t melody_notes(const PiecePlan *plan, NodeId section) {
    PlanSpan span = plan_node_span(plan, section);
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = found ? calloc(found, sizeof(*ids)) : NULL;
    size_t notes = 0;
    if (ids) {
        plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, found);
    }
    for (size_t i = 0; ids && i < found; i++) {
        notes += plan_lane_span_payload(plan, ids[i])->notes.note_count;
    }
    free(ids);
    return notes;
}

static void append_part(char *text, size_t size, const char *part) {
    size_t used = strlen(text);
    snprintf(text + used, size - used, "%s%s", used ? "; " : "", part);
}

/* The pattern of the new accompaniment of a section whose RETURN is a setting (R-518, after the plan's substitution); NULL otherwise. */
static const char *return_setting_name(const Row *row) {
    const SectionDescription *section = plan_node_section(row->plan, row->node);
    return section && section->returns[0] != '\0' && section->variation == SECTION_RETURN_SETTING
               ? api_accompaniment_name((PlanAccompanimentPattern)section->setting)
               : NULL;
}

/* How a section that returns another differs from it: null for a section that returns none. */
static cJSON *changes_json(const Row *row) {
    const SectionDescription *spec = row->spec;
    bool returns = form_spec_section_index(row->size, spec->returns) != FORM_SPEC_NO_RETURN;
    NodeId source = returns ? sized_section_find(row->plan, spec->returns) : PLAN_NODE_NONE;
    if (!returns || source == PLAN_NODE_NONE) {
        return cJSON_CreateNull();
    }
    const char *name = spec->returns;
    bool copied = form_return_copies(row->plan, row->node);
    cJSON *entry = cJSON_CreateObject();
    const char *setting = return_setting_name(row);
    const PitchMaterial *to_material = NULL;
    bool mapped = form_return_mode_change(row->plan, row->node, &to_material);
    const char *material_id = mapped && to_material ? to_material->id : NULL;
    const char *mode = mapped ? (composer_material_is_minor(to_material) ? "minor" : "major") : NULL; /* the text keeps its word */
    bool ok = entry && cJSON_AddBoolToObject(entry, "copied", copied) &&
              cJSON_AddStringToObject(entry, "variation", section_return_variation_name(spec->variation)) &&
              api_add(entry, "setting", string_or_null(setting)) && api_add(entry, "material", string_or_null(material_id));
    char text[256] = "";
    size_t changed = 0;
    size_t chords = 0;
    size_t added = 0;
    char from[64] = "";
    char to[64] = "";
    bool read = copied && form_return_chords_changed(row->plan, row->node, &changed, &chords) &&
                form_return_cadence_numerals(row->plan, row->node, from, to, sizeof from);
    if (read) {
        size_t had = melody_notes(row->plan, source);
        size_t has = melody_notes(row->plan, row->node);
        added = has > had ? has - had : 0;
        char part[160];
        if (spec->has_figure) {
            append_part(text, sizeof text, "melody: figured over the skeleton");
        } else if (added > 0) {
            snprintf(part, sizeof part, "melody: %zu note%s added", added, added == 1 ? "" : "s");
            append_part(text, sizeof text, part);
        }
        if (setting) {
            snprintf(part, sizeof part, "accompaniment: %s (new setting)", setting);
            append_part(text, sizeof text, part);
        }
        if (mode) {
            snprintf(part, sizeof part, "mode: mapped into %s", mode);
            append_part(text, sizeof text, part);
        }
        if (changed > 0) {
            snprintf(part, sizeof part, "harmony: %zu of %zu chords changed", changed, chords);
            append_part(text, sizeof text, part);
        }
        if (strcmp(from, to) != 0) {
            snprintf(part, sizeof part, "cadence: %s for %s", to, from);
            append_part(text, sizeof text, part);
        }
    }
    if (!copied) {
        bool shorter = rational_cmp(plan_node_span(row->plan, source).length, row->length) != 0;
        snprintf(text, sizeof text, "composed anew from %s's shape and motive (its %s differ%s)", name,
                 shorter ? "length" : "keys", shorter ? "s" : "");
    } else if (text[0] == '\0') {
        snprintf(text, sizeof text, "the same as %s", name);
    }
    ok = ok && api_add(entry, "melody_added", read ? cJSON_CreateNumber((double)added) : cJSON_CreateNull()) &&
         api_add(entry, "chords_changed", read ? cJSON_CreateNumber((double)changed) : cJSON_CreateNull()) &&
         api_add(entry, "chords", read ? cJSON_CreateNumber((double)chords) : cJSON_CreateNull()) &&
         api_add(entry, "cadence_from", read ? cJSON_CreateString(from) : cJSON_CreateNull()) &&
         api_add(entry, "cadence_to", read ? cJSON_CreateString(to) : cJSON_CreateNull()) &&
         cJSON_AddStringToObject(entry, "text", text);
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

/* The midi pitch of the melody note that starts at onset (from the start of the section's span); false when none does. */
static bool melody_note_at(const PiecePlan *plan, NodeId section, Rational onset, int *midi) {
    PlanSpan span = plan_node_span(plan, section);
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(*ids)) : NULL;
    bool found = false;
    if (ids) {
        plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, spans);
    }
    for (size_t s = 0; ids && !found && s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        const PlanNotes *held = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; !found && n < held->note_count; n++) {
            Rational at;
            Rational from_start;
            if (rational_add(lane_span.start, held->notes[n].offset, &at) == RATIONAL_OK &&
                rational_sub(at, span.start, &from_start) == RATIONAL_OK && rational_cmp(from_start, onset) == 0) {
                *midi = held->notes[n].midi;
                found = true;
            }
        }
    }
    free(ids);
    return found;
}

/* The sentence of a figure: "neighbor tones in sixteenths over the skeleton E5 D5 C5 ..." with what changes the harmony, the mode and the octave. */
static void figure_text(const SectionDescription *spec, bool raised, const char *skeleton, bool more, char *text, size_t size) {
    const SectionFigure *figure = &spec->figure;
    const char *kind = figure->kind == SECTION_FIGURE_NEIGHBOR ? "neighbor tones" : (figure->kind == SECTION_FIGURE_SCALE ? "a scale line" : "a broken chord");
    size_t used = (size_t)snprintf(text, size, "%s in %s", kind, section_figure_rhythm_name(figure->rhythm));
    if (figure->reharmonize && used < size) {
        used += (size_t)snprintf(text + used, size - used, ", reharmonized");
    }
    if (figure->change_at > 0 && used < size) {
        used += (size_t)snprintf(text + used, size - used, ", root changed at bar %u", figure->change_at);
    }
    if (spec->mode == SECTION_MODE_PARALLEL && used < size) {
        used += (size_t)snprintf(text + used, size - used, ", in the parallel mode");
    }
    if (raised && used < size) {
        used += (size_t)snprintf(text + used, size - used, ", an octave up");
    }
    if (used < size) {
        snprintf(text + used, size - used, " over the skeleton %s%s", skeleton, more ? " ..." : "");
    }
}

/* How many tones of the skeleton the text and the row's list name. The row's list holds them all. */
#define FIGURE_TEXT_TONES 8

/*
 * The figure of a row (R-501): {kind, rhythm, harmony, change_at, climax, raised, mode, skeleton, text}, null for a section with
 * no figure or one that was not copied. skeleton is form_figure_skeleton of the composed section, each tone as the library
 * names a pitch (C4 = middle C), spelled with flats in a flat key; raised is whether the climax octave was applied.
 */
static cJSON *figure_json(const Row *row) {
    const SectionDescription *spec = row->spec;
    FormSkeleton *skeleton = (spec->has_figure && form_return_copies(row->plan, row->node)) ? calloc(1, sizeof(*skeleton)) : NULL;
    if (!skeleton || !form_figure_skeleton(row->plan, row->node, skeleton)) {
        free(skeleton);
        return cJSON_CreateNull();
    }
    const SectionFigure *figure = &spec->figure;
    bool raised = false;
    for (size_t k = 0; figure->climax && k < skeleton->count; k++) {
        int first;
        if (!skeleton->kept[k]) {
            raised = melody_note_at(row->plan, row->node, skeleton->onset[k], &first) && first == skeleton->pitch[k] + 12;
            break;
        }
    }
    cJSON *tones = cJSON_CreateArray();
    char head[FIGURE_TEXT_TONES * 8 + 8] = "";
    bool ok = tones != NULL;
    for (size_t k = 0; ok && k < skeleton->count; k++) {
        char name[16];
        util_pitch_name(skeleton->pitch[k], row->key_flats, name, sizeof name);
        ok = cJSON_AddItemToArray(tones, cJSON_CreateString(name));
        if (k < FIGURE_TEXT_TONES) {
            snprintf(head + strlen(head), sizeof head - strlen(head), "%s%s", k ? " " : "", name);
        }
    }
    char text[256];
    figure_text(spec, raised, head, skeleton->count > FIGURE_TEXT_TONES, text, sizeof text);
    cJSON *entry = ok ? cJSON_CreateObject() : NULL;
    ok = entry && cJSON_AddStringToObject(entry, "kind", section_figure_kind_name(figure->kind)) &&
         cJSON_AddStringToObject(entry, "rhythm", section_figure_rhythm_name(figure->rhythm)) &&
         cJSON_AddStringToObject(entry, "harmony", figure->reharmonize ? "reharmonized" : "kept") &&
         api_add(entry, "change_at", figure->change_at > 0 ? cJSON_CreateNumber((double)figure->change_at) : cJSON_CreateNull()) &&
         cJSON_AddBoolToObject(entry, "climax", figure->climax) && cJSON_AddBoolToObject(entry, "raised", raised) &&
         api_add(entry, "mode", string_or_null(spec->mode == SECTION_MODE_PARALLEL ? "parallel" : NULL)) && api_add(entry, "skeleton", tones) &&
         cJSON_AddStringToObject(entry, "text", text);
    if (!ok) {
        cJSON_Delete(entry);
        cJSON_Delete(tones);
        entry = NULL;
    }
    free(skeleton);
    return entry;
}

/* {"chord", "next"} of a section that leads into the next, else null. */
static cJSON *upbeat_json(const Row *row) {
    if (!row->has_upbeat) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddStringToObject(object, "chord", row->upbeat_chord) ||
        !cJSON_AddStringToObject(object, "next", row->upbeat_next)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* A row of a form with sizes: {id, role, letter, start, beats, cadence, heard, [in_scope], returns, motive, key_heard, key, pedal, changes, figure, function, part, repeat_of, upbeat, connective, [construction_text]}. */
static cJSON *size_row_json(const PieceSpec *request, const Row *row) {
    const SectionDescription *spec = row->spec;
    char motive[96];
    MotiveRelationStep step;
    bool has_motive = form_spec_section_motive(spec, &step) && motive_relation_step_text(&step, motive, sizeof motive);
    bool returns = form_spec_section_index(row->size, spec->returns) != FORM_SPEC_NO_RETURN;
    cJSON *entry = cJSON_CreateObject();
    bool ok = entry && cJSON_AddStringToObject(entry, "id", spec->name) &&
              cJSON_AddStringToObject(entry, "role", section_role_name(spec->role)) &&
              cJSON_AddStringToObject(entry, "letter", spec->letter) && api_add(entry, "start", time_json(row->start)) &&
              cJSON_AddNumberToObject(entry, "beats", span_beats(request, row->length)) &&
              api_add(entry, "cadence", string_or_null(row->has_cadence ? plan_cadence_name(row->cadence) : NULL)) &&
              api_add(entry, "heard", string_or_null(row->heard)) && (!row->kept || cJSON_AddBoolToObject(entry, "in_scope", false)) &&
              api_add(entry, "returns", string_or_null(returns ? spec->returns : NULL)) &&
              api_add(entry, "motive", string_or_null(has_motive ? motive : NULL)) &&
              api_add(entry, "key_heard", string_or_null(row->key_heard[0] ? row->key_heard : NULL)) &&
              api_add(entry, "key", string_or_null(row->key[0] ? row->key : NULL)) &&
              api_add(entry, "region", string_or_null(row->region[0] ? row->region : NULL)) &&
              api_add(entry, "pedal", string_or_null(row->pedal ? "dominant" : NULL)) &&
              api_add(entry, "changes", changes_json(row)) && api_add(entry, "figure", figure_json(row)) &&
              api_add(entry, "function", function_json(row)) && api_add(entry, "part", string_or_null(row->part)) &&
              api_add(entry, "repeat_of", string_or_null(row->repeat_of)) && api_add(entry, "upbeat", upbeat_json(row)) &&
              api_add(entry, "connective", row->connective < 0 ? cJSON_CreateNull() : cJSON_CreateBool(row->connective > 0)) &&
              add_construction_text(entry, row);
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

cJSON *api_piece_version_json(const PieceSpec *request, const PieceVersion *version) {
    Row rows[PIECE_ROWS_MAX];
    size_t count = collect_rows(request, version, rows);
    const ComposeOutcome *outcome = &version->outcome;
    cJSON *object = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    bool ok = api_add(object, "plan", cJSON_CreateNull()) && api_add(object, "sections", sections);
    for (size_t i = 0; ok && i < count; i++) {
        cJSON *entry = rows[i].spec ? size_row_json(request, &rows[i]) : loop_row_json(&rows[i]);
        if (!entry || !cJSON_AddItemToArray(sections, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    ok = ok && api_add(object, "score", ratio_object(outcome->score.numerator, outcome->score.denominator)) &&
         api_add(object, "seam", seam_json(outcome)) && api_add(object, "analyzer_check", analyzer_check_json(outcome)) &&
         api_add(object, "style_distance", outcome->style_measured ? api_style_distance_json(&outcome->style_distance) : cJSON_CreateNull());
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
    brief.material = request->request.key.material;
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
    bool musicxml_versions;
    int explain; /* 0 none, 1 text, 2 json */
} PieceOutputs;

/* The explanation of the versions as the command writes it: text with a heading for each, or JSON {"<version>": ...}. */
static char *explanation_text(const PieceSpec *request, const PieceMade *piece, bool json) {
    Explanation explanations[API_PIECE_VERSIONS_MAX];
    char *texts[API_PIECE_VERSIONS_MAX] = {NULL};
    size_t built = 0;
    size_t size = json ? 3 : 1;
    char *joined = NULL;
    bool ok = true;
    for (size_t i = 0; ok && i < piece->version_count; i++) {
        /* A compound form is explained from its first composed part. */
        const PieceParts *parts = piece->versions[i].parts;
        ok = (parts ? explain_build(parts->plans[0], &parts->outcomes[0], &explanations[i])
                    : explain_build(piece->versions[i].plan, &piece->versions[i].outcome, &explanations[i])) == EXPLAIN_OK;
        if (ok) {
            built++;
            texts[i] = json ? explain_to_json(&explanations[i]) : explain_to_text(&explanations[i]);
            ok = texts[i] != NULL;
        }
        if (ok) {
            size += strlen(version_name(request, i)) + strlen(texts[i]) + 24;
        }
    }
    joined = ok ? malloc(size) : NULL;
    if (joined) {
        size_t used = 0;
        if (json) {
            joined[used++] = '{';
        }
        for (size_t i = 0; i < piece->version_count; i++) {
            const char *name = version_name(request, i);
            used += (size_t)(json ? snprintf(joined + used, size - used, "%s\"%s\":%s", i > 0 ? "," : "", name, texts[i])
                                  : snprintf(joined + used, size - used, "== %s version ==\n%s\n", name, texts[i]));
        }
        snprintf(joined + used, size - used, "%s", json ? "}\n" : "");
    }
    for (size_t i = 0; i < piece->version_count; i++) {
        free(texts[i]);
    }
    for (size_t i = 0; i < built; i++) {
        explain_destroy(&explanations[i]);
    }
    return joined;
}

/* {"bars": n} of the performed piece (R-485), or null for a piece of a form with sizes that is not performed. */
static cJSON *performed_json(const PieceSpec *request, const PieceMade *piece) {
    const PieceVersion *version = &piece->versions[0];
    if (!version->performed || !version->performance.plan || request->request.meter.beats_per_bar <= 0) {
        return cJSON_CreateNull();
    }
    PlanSpan span = plan_node_span(version->performance.plan, plan_root(version->performance.plan));
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddNumberToObject(object, "bars", span_beats(request, span.length) / request->request.meter.beats_per_bar)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/*
 * The result document: {schema, seed, key, meter, brief, loop, performed, [regenerated,] versions: {<name>: ...}}, the versions named as
 * api_piece_version_names has them ("loop" and "ending", or "piece"), as the command prints it, with no plan paths (the plans
 * are in "plans") and no files.
 */
static cJSON *piece_document(const PieceSpec *request, const PieceMade *piece, const PieceEdit *edit,
                             const cJSON *regenerate_section, bool from) {
    char key[32];
    char lanes[64];
    char meter[24];
    GrowBrief brief = make_brief(request);
    util_key_name(analysis_key_tonic_pitch_class(&request->request.key), request->request.key.material,
                  key, sizeof key);
    snprintf(meter, sizeof meter, "%d/%d", request->request.meter.beats_per_bar, request->request.meter.beat_unit);
    cJSON *document = cJSON_CreateObject();
    cJSON *loop_region = api_piece_loop_json(piece->versions[0].plan);
    bool ok = document && cJSON_AddStringToObject(document, "schema", "ursatz-piece") &&
              cJSON_AddNumberToObject(document, "seed", (double)piece->seed) && cJSON_AddStringToObject(document, "key", key) &&
              cJSON_AddStringToObject(document, "meter", meter) && api_add(document, "brief", api_brief_json(&brief)) &&
              api_add(document, "loop", loop_region ? loop_region : cJSON_CreateNull()) &&
              /* The game loop's document is as it was: only a form with sizes says whether it is performed. */
              (request->form->request_sections || api_add(document, "performed", performed_json(request, piece)));
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
        ok = api_add(document, "regenerated", regenerated);
    }
    cJSON *versions = cJSON_CreateObject();
    for (size_t i = 0; ok && i < piece->version_count; i++) {
        ok = api_add(versions, version_name(request, i), api_piece_version_json(request, &piece->versions[i]));
    }
    ok = ok && api_add(document, "versions", versions);
    if (!ok) {
        cJSON_Delete(versions);
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* The loop version's MIDI file and MusicXML: the MIDI file has the loopStart and loopEnd markers. */
static ComposerStatus render_loop(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const PieceMade *piece, const PieceOutputs *outputs,
                                  ComposerResponse *out) {
    if (!outputs->midi && !outputs->musicxml) {
        return COMPOSER_OK;
    }
    PlanExportOptions loop_options = request->export_options;
    Rational start;
    Rational end;
    const PiecePlan *output = api_piece_version_output_plan(&piece->versions[0]);
    if (api_piece_loop_region(output, &start, &end)) {
        loop_options.loop_markers = true;
        loop_options.loop_start = start;
        loop_options.loop_end = end;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(output, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, outputs->midi ? "/midi" : "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(knowledge, &rendered, &loop_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == COMPOSER_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(knowledge, &rendered, &request->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

ComposerStatus api_piece_musicxml(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const PiecePlan *plan, char **out, ComposerError *error) {
    RenderedPlan rendered;
    PlanExportError detail;
    *out = NULL;
    if (plan_export_render(plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (rendered_plan_musicxml_text(knowledge, &rendered, &request->export_options, out, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

/*
 * Adds "plans": {<name>: ...} for each version, with an explanation "explanation", with ending_musicxml the ending version's
 * MusicXML as "ending_musicxml", and with musicxml_versions the MusicXML of every version after the first as
 * "musicxml_versions": {<name>: ...} to the document.
 */
static ComposerStatus add_outputs(const KnowledgeSnapshot *knowledge, cJSON *document, const PieceSpec *request, const PieceMade *piece, const PieceOutputs *outputs,
                                  ComposerResponse *out) {
    cJSON *plans = cJSON_CreateObject();
    bool plans_ok = plans != NULL;
    for (size_t i = 0; plans_ok && i < piece->version_count; i++) {
        plans_ok = api_add(plans, version_name(request, i), api_piece_plan_object(api_piece_version_output_plan(&piece->versions[i])));
    }
    if (!plans_ok || !api_add(document, "plans", plans)) {
        cJSON_Delete(plans);
        return api_fail_alloc(&out->error);
    }
    if (outputs->explain) {
        char *text = explanation_text(request, piece, outputs->explain == 2);
        if (!text || !cJSON_AddStringToObject(document, "explanation", text)) {
            free(text);
            return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the explanation");
        }
        free(text);
    }
    if (outputs->musicxml_versions) {
        cJSON *musicxml = cJSON_CreateObject();
        for (size_t i = 1; musicxml && i < piece->version_count; i++) {
            char *text = NULL;
            ComposerStatus status = api_piece_musicxml(knowledge, request, api_piece_version_output_plan(&piece->versions[i]), &text, &out->error);
            if (status != COMPOSER_OK) {
                cJSON_Delete(musicxml);
                return status;
            }
            bool added = cJSON_AddStringToObject(musicxml, version_name(request, i), text) != NULL;
            free(text);
            if (!added) {
                cJSON_Delete(musicxml);
                musicxml = NULL;
            }
        }
        if (!api_add(document, "musicxml_versions", musicxml)) {
            return api_fail_alloc(&out->error);
        }
    }
    if (outputs->ending_musicxml) {
        char *text = NULL;
        ComposerStatus status = api_piece_musicxml(knowledge, request, api_piece_version_output_plan(&piece->versions[1]), &text, &out->error);
        if (status != COMPOSER_OK) {
            return status;
        }
        bool added = cJSON_AddStringToObject(document, "ending_musicxml", text) != NULL;
        free(text);
        if (!added) {
            return api_fail_alloc(&out->error);
        }
    }
    PlanJsonError detail;
    out->plan_json = plan_to_json(api_piece_version_output_plan(&piece->versions[0]), &detail);
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
        !api_member_bool(request, "musicxml_versions", &outputs->musicxml_versions, &present, error) ||
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
        return api_fail_alloc(error);
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
        const PieceVersion *stuck = &last->versions[0];
        for (size_t i = last->version_count; i > 1; i--) {
            if (last->versions[i - 1].composed && last->versions[i - 1].outcome.stuck) {
                stuck = &last->versions[i - 1];
                break;
            }
        }
        const ComposeRejection *rejection =
            stuck->outcome.rejection_count > 0 ? &stuck->outcome.rejections[stuck->outcome.rejection_count - 1] : NULL;
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                        "no piece could be composed from seeds %lu to %lu (the last was stuck in the %s version, %s stage%s%s)%s",
                        first_seed, last_seed, version_name(request, (size_t)(stuck - last->versions)),
                        rejection && rejection->stage[0] ? rejection->stage : "?", rejection ? ", " : "",
                        rejection ? rejection->reason : "",
                        rejection && (strcmp(rejection->stage, "seam") == 0 || strcmp(rejection->stage, "structure") == 0 ||
                                      !rejection->stage[0])
                            ? "; a section of whole bars needs 2 bars at least to hold its cadence"
                            : "");
    }
    return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", piece_result_name(result));
}

ComposerStatus composer_api_piece(const ComposerContext *context, const char *request_json,
                                  const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    ComposerStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != COMPOSER_OK) {
        return context_status;
    }
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
    ComposerStatus status = request ? api_parse_request(request_json, &root, &out->error) : api_fail_alloc(&out->error);
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
        status = api_piece_read_request(request, context->knowledge, &materials, cJSON_GetObjectItemCaseSensitive(root, "request"), "/request",
                                        style, plain_midi, job, &out->error);
    }
    if (status == COMPOSER_OK && edit.text) {
        status = api_piece_refuse_performed(request, &out->error);
    }
    if (status == COMPOSER_OK && outputs.ending_musicxml && version_count(request) < 2) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/ending_musicxml", "the %s form has no ending version",
                          request->form->name);
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
        status = document ? add_outputs(context->knowledge, document, request, &piece, &outputs, out) : api_fail_alloc(&out->error);
        if (status == COMPOSER_OK) {
            status = render_loop(context->knowledge, request, &piece, &outputs, out);
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
