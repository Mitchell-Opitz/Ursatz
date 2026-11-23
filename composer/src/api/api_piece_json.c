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

#include "api_style.h"
#include "api_construction.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"
#include <ursatz_composer/piece_plan_keys.h>
#include <common/json.h>

/*
 * The piece entry (docs/api.md): the piece request composed, or its loop plan regenerated in one section, with the ending
 * version built from the loop's. The seeds are tried one after another; the plans, the result document, the MIDI file and the
 * MusicXML of the loop version are made in memory.
 */

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
    /* the four rules with the sentence each is shown as (API 4.2) */
    static const char *const kRuleIds[] = {"progression", "no_parallels", "step", "leading_tone"};
    static const char *const kRuleLabels[] = {"The chords lead back", "No parallel perfect intervals across the seam",
                                              "The melody leaps no further than it can recover", "The leading tone resolves"};
    const bool held[] = {outcome->seam.progression, outcome->seam.no_parallels, outcome->seam.step, outcome->seam.leading_tone};
    cJSON *rules = cJSON_AddArrayToObject(object, "rules");
    for (size_t i = 0; i < 4; i++) {
        cJSON *rule = rules ? cJSON_CreateObject() : NULL;
        if (!rule || !cJSON_AddStringToObject(rule, "id", kRuleIds[i]) || !cJSON_AddStringToObject(rule, "label", kRuleLabels[i]) ||
            !cJSON_AddBoolToObject(rule, "ok", held[i]) || !cJSON_AddItemToArray(rules, rule)) {
            cJSON_Delete(rule);
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}

/*
 * Adds why a theme section is as long as it is (R-469), the sentence grow_construction_describe writes, as the row's
 * construction_text. A section without a construction has no member, so a piece of 4 and 8-bar themes reads as before.
 */
static bool add_construction_text(cJSON *entry, const ApiPieceRow *row) {
    char text[192];
    return !row->plan || grow_section_describe(row->plan, row->node, text, sizeof text) == 0 ||
           cJSON_AddStringToObject(entry, "construction_text", text) != NULL;
}

/* The key the plan's key lane gives near the end of a section, as "C major"; "" when the lane has none there. */
static void planned_key_near_end(const Plan *plan, PlanSpan section, char *out, size_t size) {
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
    api_piece_planned_key_at(plan, last, out, size, &flats);
}

/*
 * The function of a row (R-510): {kind, independent, lyric, text}, null for a section with no SECTION_FUNCTION. A performed row
 * has the plan and node of the section it performs, so it takes that section's function.
 */
static cJSON *function_json(const ApiPieceRow *row) {
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
        api_piece_planned_key_at(row->plan, plan_node_span(row->plan, row->node), start, sizeof start, &flats);
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

/* "bar 3" at a bar's first beat, else "bar 3 beat 2": where a section starts, in the meter's beats (counted from 0). */
static void position_text(double beats, int per_bar, char *out, size_t size) {
    int bar = per_bar > 0 ? (int)(beats / per_bar) + 1 : 1;
    double beat = beats - (double)(bar - 1) * per_bar;
    if (beat < 1e-9) {
        snprintf(out, size, "bar %d", bar);
    } else {
        snprintf(out, size, "bar %d beat %.15g", bar, (double)(long long)((beat + 1) * 100 + 0.5) / 100);
    }
}

/* "2 bars", "6 beats", "1 bar 2 beats": a length in the meter's beats. */
static void length_text(double beats, int per_bar, char *out, size_t size) {
    int bars = per_bar > 0 ? (int)(beats / per_bar) : 0;
    double rest = beats - (double)bars * per_bar;
    size_t used = 0;
    out[0] = '\0';
    if (bars > 0) {
        used += (size_t)snprintf(out, size, "%d %s", bars, bars == 1 ? "bar" : "bars");
    }
    if ((rest > 1e-9 || bars == 0) && used < size) {
        snprintf(out + used, size - used, "%s%.15g %s", used ? " " : "", rest, rest == 1 ? "beat" : "beats");
    }
}

/* start_text, end_text and length_text of the loop region {"start": "n/d", "end": "n/d"} (whole notes), in the request's meter. */
bool api_piece_loop_texts(const PieceSpec *request, cJSON *loop) {
    long long start_num = 0;
    long long start_den = 0;
    long long end_num = 0;
    long long end_den = 0;
    const cJSON *start = cJSON_GetObjectItemCaseSensitive(loop, "start");
    const cJSON *end = cJSON_GetObjectItemCaseSensitive(loop, "end");
    /* "n/d" with a slash and nothing after it ("1/4x" is refused: R-689) */
    if (!cJSON_IsString(start) || !cJSON_IsString(end) || strchr(start->valuestring, '/') == NULL ||
        strchr(end->valuestring, '/') == NULL ||
        !common_json_rational_read(start, COMMON_RATIONAL_TEXT, &start_num, &start_den) ||
        !common_json_rational_read(end, COMMON_RATIONAL_TEXT, &end_num, &end_den)) {
        return false;
    }
    int per_bar = (int)request->request.meter.beats_per_bar;
    double unit = (double)request->request.meter.beat_unit;
    double from = (double)start_num * unit / (double)start_den;
    double to = (double)end_num * unit / (double)end_den;
    char first[48];
    char last[48];
    char length[48];
    position_text(from, per_bar, first, sizeof first);
    position_text(to, per_bar, last, sizeof last);
    length_text(to - from, per_bar, length, sizeof length);
    char text[112];
    snprintf(text, sizeof text, "%s to %s", first, last);
    return cJSON_AddStringToObject(loop, "text", text) && cJSON_AddStringToObject(loop, "length_text", length);
}

/* start_text and length_text of a row, in the request's meter. */
static bool add_time_texts(cJSON *entry, const PieceSpec *request, const ApiPieceRow *row, double beats) {
    char start[48];
    char length[48];
    int per_bar = (int)request->request.meter.beats_per_bar;
    double start_beats = row->start.den != 0 ? (double)row->start.num * (double)request->request.meter.beat_unit / (double)row->start.den : 0;
    position_text(start_beats, per_bar, start, sizeof start);
    length_text(beats, per_bar, length, sizeof length);
    return cJSON_AddStringToObject(entry, "start_text", start) && cJSON_AddStringToObject(entry, "length_text", length);
}

/* A row of a form whose sections the request gives: {id, part, function, letter, start, beats, cadence, heard, [in_scope], fill, source, [construction_text]}. */
static cJSON *loop_row_json(const PieceSpec *request, const ApiPieceRow *row) {
    cJSON *entry = cJSON_CreateObject();
    bool ok = entry && cJSON_AddStringToObject(entry, "id", row->section->description.id) &&
              cJSON_AddStringToObject(entry, "part", piece_part_name(piece_section_part(row->section))) &&
              api_add(entry, "function", function_json(row)) &&
              cJSON_AddStringToObject(entry, "letter", row->section->description.letter) && api_add(entry, "start", api_piece_time_json(row->start)) &&
              cJSON_AddNumberToObject(entry, "beats", (double)row->section->description.beats) &&
              api_add(entry, "cadence", string_or_null(row->has_cadence ? plan_cadence_name(row->cadence) : NULL)) &&
              api_add(entry, "heard", string_or_null(row->heard)) && (!row->kept || cJSON_AddBoolToObject(entry, "in_scope", false)) &&
              cJSON_AddStringToObject(entry, "fill", piece_fill_name(row->section->fill)) &&
              cJSON_AddStringToObject(entry, "source", row->source) &&
              api_add(entry, "region", string_or_null(row->region[0] ? row->region : NULL)) &&
              api_add(entry, "harmony", string_or_null(row->harmony[0] ? row->harmony : NULL)) && add_construction_text(entry, row) &&
              add_time_texts(entry, request, row, (double)row->section->description.beats);
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

/* The length of a span in beats of the request's meter. */
double api_piece_span_beats(const PieceSpec *request, Rational length) {
    return length.den != 0 ? (double)length.num * (double)request->request.meter.beat_unit / (double)length.den : 0;
}

/* The melody notes written over a section. */
static size_t melody_notes(const Plan *plan, NodeId section) {
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
static const char *return_setting_name(const ApiPieceRow *row) {
    const SectionDescription *section = plan_node_section(row->plan, row->node);
    return section && section->returns[0] != '\0' && section->variation == SECTION_RETURN_SETTING
               ? api_accompaniment_name((PlanAccompanimentPattern)section->setting)
               : NULL;
}

/* How a section that returns another differs from it: null for a section that returns none. */
static cJSON *changes_json(const ApiPieceRow *row) {
    const SectionDescription *spec = row->spec;
    bool returns = form_spec_section_index(row->size, spec->returns) != FORM_SPEC_NO_RETURN;
    NodeId source = returns ? api_piece_sized_section_find(row->plan, spec->returns) : PLAN_NODE_NONE;
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
    const char *mode = mapped ? (pitch_material_is_modal_key(NULL, to_material) ? to_material->key_word : composer_material_is_minor(to_material) ? "minor" : "major") : NULL; /* the text keeps its word */
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
static bool melody_note_at(const Plan *plan, NodeId section, Rational onset, int *midi) {
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
static cJSON *figure_json(const ApiPieceRow *row) {
    const SectionDescription *spec = row->spec;
    FormSkeleton *skeleton = (spec->has_figure && form_return_copies(row->plan, row->node)) ? calloc(1, sizeof(*skeleton)) : NULL;
    if (!skeleton || !form_figure_skeleton(row->plan, NULL, row->node, skeleton) /* only the masks are read: the fallback tables give the same */) {
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
static cJSON *upbeat_json(const ApiPieceRow *row) {
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
static cJSON *size_row_json(const PieceSpec *request, const ApiPieceRow *row) {
    const SectionDescription *spec = row->spec;
    char motive[96];
    MotiveRelationStep step;
    bool has_motive = form_spec_section_motive(spec, &step) && motive_relation_step_text(&step, motive, sizeof motive);
    bool returns = form_spec_section_index(row->size, spec->returns) != FORM_SPEC_NO_RETURN;
    cJSON *entry = cJSON_CreateObject();
    bool ok = entry && cJSON_AddStringToObject(entry, "id", spec->name) &&
              cJSON_AddStringToObject(entry, "role", section_role_name(spec->role)) &&
              cJSON_AddStringToObject(entry, "letter", spec->letter) && api_add(entry, "start", api_piece_time_json(row->start)) &&
              cJSON_AddNumberToObject(entry, "beats", api_piece_span_beats(request, row->length)) &&
              api_add(entry, "cadence", string_or_null(row->has_cadence ? plan_cadence_name(row->cadence) : NULL)) &&
              api_add(entry, "heard", string_or_null(row->heard)) && (!row->kept || cJSON_AddBoolToObject(entry, "in_scope", false)) &&
              api_add(entry, "returns", string_or_null(returns ? spec->returns : NULL)) &&
              api_add(entry, "motive", string_or_null(has_motive ? motive : NULL)) &&
              api_add(entry, "key_heard", string_or_null(row->key_heard[0] ? row->key_heard : NULL)) &&
              api_add(entry, "key", string_or_null(row->key[0] ? row->key : NULL)) &&
              api_add(entry, "region", string_or_null(row->region[0] ? row->region : NULL)) &&
              api_add(entry, "harmony", string_or_null(row->harmony[0] ? row->harmony : NULL)) &&
              api_add(entry, "pedal", string_or_null(row->pedal ? "dominant" : NULL)) &&
              api_add(entry, "changes", changes_json(row)) && api_add(entry, "figure", figure_json(row)) &&
              api_add(entry, "function", function_json(row)) && api_add(entry, "part", string_or_null(row->part)) &&
              api_add(entry, "repeat_of", string_or_null(row->repeat_of)) && api_add(entry, "upbeat", upbeat_json(row)) &&
              api_add(entry, "connective", row->connective < 0 ? cJSON_CreateNull() : cJSON_CreateBool(row->connective > 0)) &&
              add_construction_text(entry, row) && add_time_texts(entry, request, row, api_piece_span_beats(request, row->length));
    if (!ok) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

cJSON *api_piece_version_json(const PieceSpec *request, const PieceVersion *version) {
    ApiPieceRow rows[PIECE_ROWS_MAX];
    size_t count = api_piece_collect_rows(request, version, rows);
    const ComposeOutcome *outcome = &version->outcome;
    cJSON *object = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    bool ok = api_add(object, "plan", cJSON_CreateNull()) && api_add(object, "sections", sections);
    for (size_t i = 0; ok && i < count; i++) {
        cJSON *entry = rows[i].spec ? size_row_json(request, &rows[i]) : loop_row_json(request, &rows[i]);
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

