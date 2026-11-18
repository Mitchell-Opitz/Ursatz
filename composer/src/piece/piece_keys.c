#include <ursatz_composer/piece_plan_keys.h>

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

#include <theory/harmony/harmony_chord.h>
#include <theory/harmony/harmony_key_change.h>
#include <theory/harmony/harmony_related_key.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/tonality/region_relation.h>
#include <theory/vocabulary/cadence_type.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_state.h>
#include "check/hearing.h"
#include "form/form_internal.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

/* A key lane span before it is added: its key, the relation that reached it from home (NULL for home) and where it lies. */
typedef struct {
    PlanSpan span;
    AnalysisKey key;
    const char *relation;
} KeySegment;

#define MAX_SEGMENTS (2 * FORM_SPEC_MAX_SECTIONS + 1)

/* The relation that reaches the home key's parallel key: the other mode on the same tonic (R-596). */
#define PARALLEL_RELATION "omt.relation.parallel"

/* The key a relation of the snapshot reaches from the home key, resolved against the home key's region. */
static bool resolve_relation(const KnowledgeSnapshot *knowledge, AnalysisKey home, const char *relation_id, AnalysisKey *out,
                             const char **resolved_id) {
    if (!knowledge || !relation_id || home.material == NULL) {
        return false;
    }
    const Relation *relation = knowledge_snapshot_relation(knowledge, relation_id);
    Region anchor = region_tonal(analysis_key_tonic_pitch_class(&home), home.material);
    Region found;
    if (!relation || region_resolve(knowledge, relation, &anchor, &found, NULL, 0) != REGION_RELATION_OK ||
        found.kind != REGION_TONAL_MODAL) {
        return false;
    }
    *out = plan_key_of_pitch_class(found.center, found.material);
    *resolved_id = relation->id;
    return true;
}

/*
 * The key of a region of the home key (R-485): one of the region's relations for the home key's mode, drawn from the list
 * (one draw of rng whatever the relation is), resolved through its entry in the snapshot (R-596). The parallel relation is
 * the home key's parallel key, the other mode on the same tonic; any other is the key the relation reaches, closely related
 * for the relations the forms list. False when the relation refuses.
 */
static bool region_key(const KnowledgeSnapshot *knowledge, AnalysisKey home, const FormRegion *region, FillRng *rng, AnalysisKey *out,
                       const char **relation_id) {
    bool is_minor = composer_key_is_minor(&home);
    const char(*relations)[SECTION_WORD_MAX] = is_minor ? region->minor : region->major;
    size_t count = is_minor ? region->minor_count : region->major_count;
    return resolve_relation(knowledge, home, relations[fill_rng_below(rng, count)], out, relation_id);
}

bool piece_region_key(const KnowledgeSnapshot *knowledge, AnalysisKey home, const FormRegion *region, FillRng *rng, AnalysisKey *out) {
    const char *relation_id;
    return region_key(knowledge, home, region, rng, out, &relation_id);
}

/* A region a section's keys name has no parallel relation: the parallel key is for the parts of a compound form. */
static bool region_has_parallel(const FormRegion *region, bool is_minor) {
    const char(*relations)[SECTION_WORD_MAX] = is_minor ? region->minor : region->major;
    size_t count = is_minor ? region->minor_count : region->major_count;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(relations[i], PARALLEL_RELATION) == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Draws the keys each section starts and ends in; every section is home when keys is left as set. A form that can leave
 * home (a section of the size starts or ends in a region) modulates in two pieces of three, as the templates always did:
 * the draw is fill_rng_below(rng, 3), 0 staying home unless the form always modulates (R-493), then one pick per
 * region in the order the form lists them.
 */
static bool draw_keys(const KnowledgeSnapshot *knowledge, const FormSpec *form, const FormSpecSize *size, AnalysisKey home, FillRng *rng,
                      PieceSectionKeys *keys,
                      bool *modulates) {
    AnalysisKey away[FORM_SPEC_MAX_REGIONS];
    const char *away_relation[FORM_SPEC_MAX_REGIONS];
    bool can = false;
    *modulates = false;
    for (size_t i = 0; i < size->section_count; i++) {
        keys[i].start = home;
        keys[i].end = home;
        keys[i].start_relation = NULL;
        keys[i].end_relation = NULL;
        can = can || form_spec_region_of(form, &size->sections[i].key_start) != FORM_SPEC_HOME ||
              form_spec_region_of(form, &size->sections[i].key_end) != FORM_SPEC_HOME;
        /* A section in the parallel mode (R-499) is on the home tonic in the other mode: no draw, and modulates stays as it is. */
        if (size->sections[i].mode == SECTION_MODE_PARALLEL) {
            if (!resolve_relation(knowledge, home, PARALLEL_RELATION, &keys[i].start, &keys[i].start_relation)) {
                return false;
            }
            keys[i].end = keys[i].start;
            keys[i].end_relation = keys[i].start_relation;
        }
        /* A section in the tonic major (R-517) is on the home tonic in its major key: home in a major piece, the parallel major in a
         * minor one. No draw, so the draws after it are unchanged. */
        if (size->sections[i].mode == SECTION_MODE_TONIC_MAJOR && composer_key_is_minor(&home)) {
            if (!resolve_relation(knowledge, home, PARALLEL_RELATION, &keys[i].start, &keys[i].start_relation)) {
                return false;
            }
            keys[i].end = keys[i].start;
            keys[i].end_relation = keys[i].start_relation;
        }
    }
    if (!rng || !can) {
        return true;
    }
    /* The draw is made for every form, so the region draws after it are the same; a form that modulates does not stay. */
    bool stay = fill_rng_below(rng, 3) == 0;
    if (stay && !form->modulates) {
        return true;
    }
    for (size_t r = 0; r < form->region_count; r++) {
        const FormRegion *region = &form->regions[r];
        if (region_has_parallel(region, composer_key_is_minor(&home)) || !region_key(knowledge, home, region, rng, &away[r], &away_relation[r])) {
            return false;
        }
    }
    for (size_t i = 0; i < size->section_count; i++) {
        int start = form_spec_region_of(form, &size->sections[i].key_start);
        int end = form_spec_region_of(form, &size->sections[i].key_end);
        if (start != FORM_SPEC_HOME) {
            keys[i].start = away[start];
            keys[i].start_relation = away_relation[start];
        }
        if (end != FORM_SPEC_HOME) {
            keys[i].end = away[end];
            keys[i].end_relation = away_relation[end];
        }
    }
    *modulates = true;
    return true;
}

/* Adds a segment, joined to the one before it when that is in the same key and ends where this one starts. */
static PieceResult push_segment(KeySegment *segments, size_t *count, PlanSpan span, AnalysisKey key, const char *relation) {
    if (*count > 0 && analysis_key_equals(&segments[*count - 1].key, &key)) {
        Rational end;
        PieceResult result = piece_result_of_plan(plan_span_end(segments[*count - 1].span, &end));
        if (result == PIECE_OK && rational_cmp(end, span.start) == 0) {
            return piece_result_of_plan(plan_result_of_rational(rational_add(segments[*count - 1].span.length, span.length,
                                           &segments[*count - 1].span.length)));
        }
        return PIECE_ERR_BAD_REQUEST;
    }
    if (*count >= MAX_SEGMENTS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    segments[*count].span = span;
    segments[*count].key = key;
    segments[*count].relation = relation;
    (*count)++;
    return PIECE_OK;
}

/* A section whose key changes halfway has its first half in the key it starts in and its second in the key it ends in. */
static PieceResult section_segments(KeySegment *segments, size_t *count, PlanSpan span, const PieceSectionKeys *keys) {
    if (analysis_key_equals(&keys->start, &keys->end)) {
        return push_segment(segments, count, span, keys->start, keys->start_relation);
    }
    PlanSpan first;
    PlanSpan second;
    first.start = span.start;
    PieceResult result = piece_result_of_plan(plan_result_of_rational(rational_div_int(span.length, 2, &first.length)));
    if (result != PIECE_OK) {
        return result;
    }
    second.length = first.length;
    result = piece_result_of_plan(plan_result_of_rational(rational_add(span.start, first.length, &second.start)));
    if (result == PIECE_OK) {
        result = push_segment(segments, count, first, keys->start, keys->start_relation);
    }
    return result == PIECE_OK ? push_segment(segments, count, second, keys->end, keys->end_relation) : result;
}

/* The KEY constraint of the section, replaced by the key it ends in. */
static PieceResult set_section_key(PiecePlan *plan, NodeId section, AnalysisKey key) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        if (plan_node_constraint_at(plan, section, i)->kind == PLAN_CONSTRAINT_KEY) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_KEY;
            constraint.params.key = key;
            PieceResult result = piece_result_of_plan(plan_node_remove_constraint(plan, section, i));
            return result == PIECE_OK ? piece_result_of_plan(plan_node_add_constraint(plan, section, &constraint)) : result;
        }
    }
    return PIECE_ERR_BAD_REQUEST;
}

PieceResult piece_plan_keys(PiecePlan *plan, const KnowledgeSnapshot *knowledge, const FormSpec *spec, const FormSpecSize *size,
                            FillRng *rng, PieceKeyPlan *out) {
    if (!plan || !spec || !size || !out) {
        return PIECE_ERR_NULL;
    }
    memset(out, 0, sizeof(*out));
    NodeId form = plan_root(plan);
    if (form == PLAN_NODE_NONE) {
        return PIECE_ERR_BAD_REQUEST;
    }
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    NodeId material = PLAN_NODE_NONE;
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            material = child;
        } else if (plan_section_has_role(plan, child, SECTION_ROLE_SEAM)) {
            continue; /* the seam is not a section of the piece */
        } else if (count < FORM_SPEC_MAX_SECTIONS) {
            sections[count++] = child;
        } else {
            return PIECE_ERR_BAD_REQUEST;
        }
    }
    const PlanConstraint *home_key = count > 0 ? plan_node_find_constraint(plan, sections[0], PLAN_CONSTRAINT_KEY) : NULL;
    if (!home_key || material == PLAN_NODE_NONE || count != size->section_count) {
        return PIECE_ERR_BAD_REQUEST;
    }
    AnalysisKey home = home_key->params.key;
    out->count = count;
    if (!draw_keys(knowledge, spec, size, home, rng, out->sections, &out->modulates)) {
        return PIECE_ERR_BAD_REQUEST;
    }

    KeySegment segments[MAX_SEGMENTS];
    size_t segment_count = 0;
    PieceResult result = PIECE_OK;
    for (size_t i = 0; result == PIECE_OK && i < count; i++) {
        result = section_segments(segments, &segment_count, plan_node_span(plan, sections[i]), &out->sections[i]);
    }
    if (result == PIECE_OK) {
        result = push_segment(segments, &segment_count, plan_node_span(plan, material), home, NULL);
    }
    for (size_t i = 0; result == PIECE_OK && i < segment_count; i++) {
        PlanLanePayload payload;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        payload.key = segments[i].key;
        result = piece_result_of_plan(plan_add_lane_span(plan, form, PLAN_LANE_KEY, 0, segments[i].span, &payload, &id));
        if (result == PIECE_OK && segments[i].relation) {
            result = piece_result_of_plan(
                plan_lane_span_set_key_relation(plan, id, segments[i].relation, PLAN_KEY_ANCHOR_HOME, NULL, true));
        }
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN)) : result;
    }
    for (size_t i = 0; result == PIECE_OK && i < count; i++) {
        if (!analysis_key_equals(&out->sections[i].end, &home)) {
            result = set_section_key(plan, sections[i], out->sections[i].end);
        }
    }
    return result;
}


/* ------------------------------------------------------------- the regions a request names */

static PieceResult refuse_region(size_t index, size_t *refused, char *message, size_t size, const char *format, ...) {
    va_list args;
    if (refused) {
        *refused = index;
    }
    if (message && size > 0) {
        va_start(args, format);
        vsnprintf(message, size, format, args);
        va_end(args);
    }
    return PIECE_ERR_BAD_REQUEST;
}

/* The words of a refusal for a region the Composer does not write: its material, until P69 (R-611). */
static PieceResult refuse_material(size_t index, const char *section, int center, const PitchMaterial *material, size_t *refused,
                                   char *message, size_t size) {
    static const char *const NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    return refuse_region(index, refused, message, size,
                         "section '%s' is in %s %s: the Composer writes major and natural minor keys until P69", section,
                         center >= 0 && center < 12 ? NAMES[center] : "?", material && material->name ? material->name : "no material");
}

PieceResult piece_resolve_regions(const PieceRequest *request, const KnowledgeSnapshot *knowledge, PieceRegion *out, size_t *refused,
                                  char *message, size_t message_size) {
    CommonPracticeMaterials materials;
    if (!request || !out || !knowledge || !common_practice_materials_resolve(knowledge, &materials, NULL, 0)) {
        return PIECE_ERR_NULL;
    }
    AnalysisKey keys[PIECE_STRUCTURE_MAX_SECTIONS];
    size_t count = request->section_count < PIECE_STRUCTURE_MAX_SECTIONS ? request->section_count : PIECE_STRUCTURE_MAX_SECTIONS;
    for (size_t i = 0; i < count; i++) {
        const SectionDescription *section = &request->sections[i].description;
        const SectionRegion *region = &section->key_end;
        PieceRegion *entry = &out[i];
        memset(entry, 0, sizeof(*entry));
        entry->key = request->key;
        keys[i] = request->key;
        snprintf(entry->text, sizeof(entry->text), "home");
        if (region->kind == SECTION_REGION_NONE) {
            continue;
        }
        entry->has_region = true;
        if (region->kind == SECTION_REGION_HOME) {
            entry->changes_key = i > 0 && !analysis_key_equals(&keys[i - 1], &request->key);
            continue;
        }
        if (region->kind == SECTION_REGION_KEY) {
            char text[SECTION_TONIC_MAX + SECTION_WORD_MAX + 2];
            snprintf(text, sizeof(text), "%s %s", region->tonic, region->material);
            const PitchMaterial *material = knowledge_snapshot_material_named(knowledge, region->material);
            AnalysisKey key;
            if (!material) {
                return refuse_region(i, refused, message, message_size, "section '%s': no material is named '%s'", section->id, region->material);
            }
            if (!composer_material_supported(&materials, material)) {
                char tonic[SECTION_TONIC_MAX + 8];
                snprintf(tonic, sizeof(tonic), "%s major", region->tonic);
                AnalysisKey center;
                int pc = request_parse_key(tonic, knowledge, &center) ? analysis_key_tonic_pitch_class(&center) : -1;
                return refuse_material(i, section->id, pc, material, refused, message, message_size);
            }
            if (!request_parse_key(text, knowledge, &key)) {
                return refuse_region(i, refused, message, message_size, "section '%s': '%s' is not a tonic", section->id, region->tonic);
            }
            entry->key = key;
            keys[i] = key;
            entry->changes_key = !analysis_key_equals(i > 0 ? &keys[i - 1] : &request->key, &key);
            snprintf(entry->text, sizeof(entry->text), "%s", text);
            continue;
        }
        /* a relation from an anchor: home, the section before, or an earlier section */
        const AnalysisKey *anchor_key = &request->key;
        const char *anchor_name = "home";
        if (region->anchor == SECTION_ANCHOR_PREVIOUS) {
            if (i == 0) {
                return refuse_region(i, refused, message, message_size, "the first section has no previous region");
            }
            anchor_key = &keys[i - 1];
            anchor_name = "previous";
        } else if (region->anchor == SECTION_ANCHOR_SECTION) {
            size_t found = i;
            for (size_t k = 0; k < i; k++) {
                if (strcmp(request->sections[k].description.id, region->anchor_section) == 0) {
                    found = k;
                }
            }
            if (found == i) {
                return refuse_region(i, refused, message, message_size, "anchor '%s' must be an earlier section", region->anchor_section);
            }
            anchor_key = &keys[found];
            anchor_name = region->anchor_section;
        }
        const Relation *relation = knowledge_snapshot_relation_named(knowledge, region->relation);
        Region from = region_tonal(analysis_key_tonic_pitch_class(anchor_key), anchor_key->material);
        Region reached;
        char why[256];
        if (!relation) {
            return refuse_region(i, refused, message, message_size, "section '%s': no relation is named '%s'", section->id, region->relation);
        }
        if (region_resolve_named(knowledge, region->relation, &from, &reached, why, sizeof(why)) != REGION_RELATION_OK) {
            return refuse_region(i, refused, message, message_size, "section '%s': %s", section->id, why);
        }
        if (reached.kind != REGION_TONAL_MODAL || !composer_material_supported(&materials, reached.material)) {
            return refuse_material(i, section->id, reached.center, reached.material, refused, message, message_size);
        }
        entry->key = plan_key_of_pitch_class(reached.center, reached.material);
        keys[i] = entry->key;
        /* A region equal to the section before's (home for the first) asks for no key change. */
        const AnalysisKey *before = i > 0 ? &keys[i - 1] : &request->key;
        Region before_region = region_tonal(analysis_key_tonic_pitch_class(before), before->material);
        entry->changes_key = !region_equals(&reached, &before_region);
        snprintf(entry->relation, sizeof(entry->relation), "%s", relation->id);
        entry->anchor = region->anchor == SECTION_ANCHOR_PREVIOUS  ? PLAN_KEY_ANCHOR_PREVIOUS
                        : region->anchor == SECTION_ANCHOR_SECTION ? PLAN_KEY_ANCHOR_SECTION
                                                                   : PLAN_KEY_ANCHOR_HOME;
        if (entry->anchor == PLAN_KEY_ANCHOR_SECTION) {
            snprintf(entry->anchor_section, sizeof(entry->anchor_section), "%s", region->anchor_section);
        }
        snprintf(entry->text, sizeof(entry->text), "%s of %s", region->relation, anchor_name);
    }
    return PIECE_OK;
}

/* ------------------------------------------------------------- the check */

#define MAX_ARRIVALS 256

/* The key lane over the whole plan, in order. */
typedef struct {
    PlanSpan span[MAX_SEGMENTS];
    AnalysisKey key[MAX_SEGMENTS];
    size_t count;
} KeyLane;

static bool read_key_lane(const PiecePlan *plan, KeyLane *lane) {
    LaneSpanId ids[MAX_SEGMENTS];
    PlanSpan everything = {{0, 1}, {1000000, 1}};
    lane->count = plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, ids, MAX_SEGMENTS);
    if (lane->count == 0 || lane->count > MAX_SEGMENTS) {
        return false;
    }
    for (size_t i = 0; i < lane->count; i++) {
        lane->span[i] = plan_lane_span_span(plan, ids[i]);
        lane->key[i] = plan_lane_span_payload(plan, ids[i])->key;
    }
    return true;
}

/* Whether the key is the home key's or one of its closely related keys. */
static bool related_to_home(AnalysisKey home, AnalysisKey key) {
    HarmonyMode home_mode = harmony_key_mode(&home);
    HarmonyMode mode = harmony_key_mode(&key);
    if (analysis_key_equals(&home, &key)) {
        return true;
    }
    /* The home key's parallel key, on the same tonic (R-499). */
    if (analysis_key_tonic_pitch_class(&home) == analysis_key_tonic_pitch_class(&key)) {
        return true;
    }
    int semitone = util_pitch_class(analysis_key_tonic_pitch_class(&key) - analysis_key_tonic_pitch_class(&home));
    for (int i = 0; i < harmony_related_key_count(home_mode); i++) {
        HarmonyRelatedKey related;
        if (harmony_related_key_at(home_mode, i, &related) && related.mode == mode &&
            harmony_related_key_semitone(home_mode, &related) == semitone) {
            return harmony_key_is_closely_related(home_mode, &related);
        }
    }
    return false;
}

/* Adds the authentic arrivals heard in `span`. False when the span is not readable. */
static bool hear_arrivals(ComposerHearing *hearing, const PiecePlan *plan, PlanSpan span, Rational *arrivals,
                          size_t *arrival_count) {
    FormHeard heard;
    if (form_hear_in(hearing, plan, span, &heard) != FORM_HEAR_OK) {
        return false;
    }
    for (size_t i = 0; i < heard.cadence_count && *arrival_count < MAX_ARRIVALS; i++) {
        int type = heard.cadences[i].type;
        if (type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
            type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) {
            arrivals[(*arrival_count)++] = heard.cadences[i].arrival;
        }
    }
    form_heard_destroy(&heard);
    return true;
}

/*
 * The arrival of every authentic cadence heard in the sections, in whole notes. Each section is heard whole; the hearing
 * gives the Analyzer the key at the span's start, so the part of a section after a key change inside it is heard again
 * from the change, in its new key. False when a section is not readable.
 */
static bool authentic_arrivals(ComposerHearing *hearing, const PiecePlan *plan, const KeyLane *lane, const NodeId *sections,
                               size_t count, Rational *arrivals, size_t *arrival_count) {
    *arrival_count = 0;
    for (size_t s = 0; s < count; s++) {
        PlanSpan whole = plan_node_span(plan, sections[s]);
        Rational end;
        if (!hear_arrivals(hearing, plan, whole, arrivals, arrival_count) || plan_span_end(whole, &end) != PLAN_OK) {
            return false;
        }
        for (size_t k = 0; k < lane->count; k++) {
            Rational at = lane->span[k].start;
            Rational negative = {-at.num, at.den};
            PlanSpan part = {at, {0, 1}};
            if (rational_cmp(at, whole.start) > 0 && rational_cmp(at, end) < 0 &&
                rational_add(end, negative, &part.length) == RATIONAL_OK &&
                !hear_arrivals(hearing, plan, part, arrivals, arrival_count)) {
                return false;
            }
        }
    }
    return true;
}

static bool constraint_key(const PiecePlan *plan, NodeId section, AnalysisKey *out) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, section, PLAN_CONSTRAINT_KEY);
    if (constraint) {
        *out = constraint->params.key;
    }
    return constraint != NULL;
}

static size_t chords_between(const PiecePlan *plan, Rational start, Rational end) {
    PlanSpan range = {start, {0, 1}};
    Rational negative = {-start.num, start.den};
    if (rational_cmp(end, start) <= 0 || rational_add(end, negative, &range.length) != RATIONAL_OK) {
        return 0;
    }
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
}

bool piece_check_keys_in(ComposerHearing *hearing, const PiecePlan *plan, PieceKeyCheck *out) {
    if (!hearing || !plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    NodeId form = plan_root(plan);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t section_count = 0;
    Rational piece_end = {0, 1};
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL)) {
            piece_end = plan_node_span(plan, child).start;
        } else if (plan_section_has_role(plan, child, SECTION_ROLE_SEAM)) {
            continue;
        } else if (section_count < FORM_SPEC_MAX_SECTIONS) {
            sections[section_count++] = child;
        }
    }
    KeyLane lane;
    Rational arrivals[MAX_ARRIVALS + 1];
    size_t arrival_count;
    if (section_count == 0 || piece_end.num == 0 || !read_key_lane(plan, &lane) ||
        !authentic_arrivals(hearing, plan, &lane, sections, section_count, arrivals, &arrival_count)) {
        return false;
    }
    AnalysisKey home = lane.key[0];
    for (size_t k = 0; k < lane.count; k++) {
        if (!related_to_home(home, lane.key[k]) && out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "key span %zu is not closely related to the home key", k);
        }
    }
    for (size_t k = 1; k < lane.count; k++) {
        /* A change of mode on one tonic is neither counted nor classified (R-499). */
        if (analysis_key_equals(&lane.key[k], &lane.key[k - 1]) ||
            analysis_key_tonic_pitch_class(&lane.key[k]) == analysis_key_tonic_pitch_class(&lane.key[k - 1])) {
            continue;
        }
        out->changes++;
        Rational span_end;
        Rational region_end = piece_end;
        Rational cadence_end = {0, 1};
        bool planned = false;
        if (plan_span_end(lane.span[k], &span_end) != PLAN_OK) {
            return false;
        }
        region_end = rational_cmp(span_end, piece_end) < 0 ? span_end : piece_end;
        /* A section planned to end in the new key, inside the span: the last of them is where the cadence is. */
        for (size_t s = 0; s < section_count; s++) {
            AnalysisKey ends_in;
            Rational end;
            if (!constraint_key(plan, sections[s], &ends_in) ||
                plan_span_end(plan_node_span(plan, sections[s]), &end) != PLAN_OK) {
                return false;
            }
            if (analysis_key_equals(&ends_in, &lane.key[k]) && rational_cmp(end, lane.span[k].start) > 0 &&
                rational_cmp(end, region_end) <= 0 && rational_cmp(end, cadence_end) > 0) {
                cadence_end = end;
                planned = true;
            }
        }
        if (!planned) {
            continue;
        }
        HarmonyKeyChange change;
        change.chords_in_new_key = (int)chords_between(plan, lane.span[k].start, region_end);
        change.cadence_in_new_key = true;
        change.continues_after_cadence = chords_between(plan, cadence_end, region_end) > 0;
        KeyChangeExtent length_extent;
        KeyChangeExtent extent;
        if (!harmony_key_change_classify(&change, &length_extent) ||
            !harmony_key_change_confirm(length_extent, lane.span[k].start, region_end, arrivals,
                                        arrival_count, &extent)) {
            return false;
        }
        if (key_change_extent_is_modulation(extent)) {
            out->modulations++;
        } else if (out->ok) {
            out->ok = false;
            snprintf(out->reason, sizeof(out->reason), "the key change at %lld/%lld is a %s, not a modulation",
                     lane.span[k].start.num, lane.span[k].start.den, key_change_extent_name(extent));
        }
    }
    return true;
}
