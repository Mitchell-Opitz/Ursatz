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

/* ------------------------------------------------------------- reading the plans */

bool api_piece_loop_region(const Plan *plan, Rational *start, Rational *end) {
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

cJSON *api_piece_time_json(Rational time) {
    char text[48];
    snprintf(text, sizeof text, "%lld/%lld", time.num, time.den);
    return cJSON_CreateString(text);
}

cJSON *api_piece_loop_json(const Plan *plan) {
    Rational start;
    Rational end;
    if (!api_piece_loop_region(plan, &start, &end)) {
        return NULL;
    }
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddItemToObject(object, "start", api_piece_time_json(start)) ||
        !cJSON_AddItemToObject(object, "end", api_piece_time_json(end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_piece_plan_object(const KnowledgeSnapshot *knowledge, const Plan *plan) {
    PlanJsonError error;
    PlanProducer producer = composer_plan_producer(knowledge);
    char *text = plan_to_json(plan, &producer, &error);
    cJSON *object = text ? cJSON_Parse(text) : NULL;
    free(text);
    return object;
}


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
static bool section_pedal(const Plan *plan, PlanSpan span) {
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
        util_key_name(spelled_key_tonic_pitch_class(&at->key), at->key.material, out, size);
    }
}

/* The key the plan's key lane gives at the start of a span, as "C major"; "" when the lane has none there. */
void api_piece_planned_key_at(const Plan *plan, PlanSpan section, char *out, size_t size, bool *flats) {
    PlanSpan up_to = {{0, 1}, {0, 1}};
    Rational start = section.start;
    Rational end_of_section;
    out[0] = '\0';
    if (plan_span_end(section, &end_of_section) != PLAN_OK ||
        rational_make(end_of_section.num, end_of_section.den, &up_to.length) != RATIONAL_OK) {
        return;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_REGION, 0, up_to, NULL, 0);
    LaneSpanId *ids = found ? calloc(found, sizeof(*ids)) : NULL;
    if (!ids) {
        return;
    }
    plan_lane_find(plan, PLAN_LANE_REGION, 0, up_to, ids, found);
    const PlanLanePayload *at = NULL;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        if (rational_cmp(span.start, start) <= 0 && plan_span_end(span, &end) == PLAN_OK && rational_cmp(start, end) < 0) {
            at = plan_lane_span_payload(plan, ids[i]);
        }
    }
    if (at) {
        util_key_name(spelled_key_tonic_pitch_class(&at->region), at->region.material, out, size);
        *flats = util_key_prefers_flats(spelled_key_tonic_pitch_class(&at->region), at->region.material);
    }
    free(ids);
}

/* The harmony of a section as the request gave it: its progression's numerals (and cells), "schema <name>" or "bar 2: V, bar 3: I V". */
static void harmony_text(const SectionHarmony *harmony, char *out, size_t size) {
    out[0] = '\0';
    if (harmony->kind == SECTION_HARMONY_PROGRESSION) {
        if (harmony->cells) {
            snprintf(out, size, "%s (%u cells each)", harmony->numerals, harmony->cells);
        } else {
            snprintf(out, size, "%s", harmony->numerals);
        }
    } else if (harmony->kind == SECTION_HARMONY_SCHEMA) {
        snprintf(out, size, "schema %s", harmony->schema);
    } else if (harmony->kind == SECTION_HARMONY_CHORDS) {
        size_t used = 0;
        for (size_t i = 0; i < harmony->chord_count && used + 1 < size; i++) {
            used += (size_t)snprintf(out + used, size - used, "%sbar %u: %s", i ? ", " : "", harmony->chords[i].bar, harmony->chords[i].text);
        }
    }
}

/* The sections of a size of a form with sizes, each matched to the plan's section of its name, in order. */
static size_t collect_composed_rows(const PieceSpec *request, const PieceVersion *version, const FormSpecSize *size, ApiPieceRow *rows) {
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    const Plan *plan = version->plan;
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
        NodeId id = api_piece_sized_section_find(plan, size->sections[i].name);
        if (id == PLAN_NODE_NONE) {
            continue;
        }
        const SectionDescription *described = plan_node_section(plan, id);
        ApiPieceRow *row = &rows[count++];
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
            NodeId next = api_piece_sized_section_find(plan, size->sections[i + 1].name);
            char ignored[PLAN_ROMAN_NUMERAL_MAX];
            row->has_upbeat = next != PLAN_NODE_NONE &&
                              form_return_edge_numerals(plan, id, ignored, row->upbeat_chord, sizeof row->upbeat_chord) &&
                              form_return_edge_numerals(plan, next, row->upbeat_next, ignored, sizeof row->upbeat_next);
        }
        key_heard_at(keys, key_count, row->start, row->key_heard, sizeof row->key_heard);
        api_piece_planned_key_at(plan, plan_node_span(plan, id), row->key, sizeof row->key, &row->key_flats);
        for (size_t o = 0; o < request->request.overlay_count; o++) {
            if (strcmp(request->request.overlays[o].description.name, size->sections[i].name) == 0) {
                harmony_text(&request->request.overlays[o].description.harmony, row->harmony, sizeof row->harmony);
            }
        }
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
static size_t collect_size_rows(const PieceSpec *request, const PieceVersion *version, ApiPieceRow *rows) {
    if (!version->performed) {
        return collect_composed_rows(request, version, form_spec_size(request->form, request->request.bars, request->request.count), rows);
    }
    const PieceParts *parts = version->parts;
    const FormSpecSize *whole = form_spec_size(request->form, request->request.bars, request->request.count);
    ApiPieceRow composed[FORM_SPEC_MAX_PARTS_OF][PIECE_ROWS_MAX];
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
size_t api_piece_collect_rows(const PieceSpec *request, const PieceVersion *version, ApiPieceRow *rows) {
    if (!request->form->request_sections) {
        return collect_size_rows(request, version, rows);
    }
    const AnalyzerCheck *check = version->outcome.analyzer_check_count > 0 ? &version->outcome.analyzer_checks[0] : NULL;
    size_t next_item = 0;
    size_t count = 0;
    const Plan *plan = version->plan;
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
        ApiPieceRow *row = &rows[count];
        memset(row, 0, sizeof(*row));
        for (size_t s = 0; s < request->request.section_count; s++) {
            if (strcmp(request->sections[s].description.id, described->id) == 0) {
                row->section = &request->sections[s];
                row->source = request->source[s];
                harmony_text(&request->sections[s].description.harmony, row->harmony, sizeof row->harmony);
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

