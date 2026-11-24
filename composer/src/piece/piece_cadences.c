#include <ursatz_composer/piece_plan_keys.h>

#include <stdio.h>
#include <string.h>

#include <theory/harmony/harmony_cadence_rules.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/harmony_phrase.h>
#include "structure/form_record.h"
#include "piece/piece_internal.h"
#include "structure/plan_util.h"

#define MAX_CADENCES 2

/* The cadences of the section in order: the one it ends in, then an antecedent's weak one. Returns how many. */
static size_t section_cadences(const Plan *plan, NodeId section, CadenceType *out) {
    const SectionDescription *described = plan_section_or_empty(plan, section);
    size_t count = 0;
    if (described->has_cadence) {
        out[count++] = described->cadence;
    }
    if (described->has_first_cadence && count < MAX_CADENCES) {
        out[count++] = described->first_cadence;
    }
    return count;
}

/* The key the section ends in: its KEY constraint. False when it has none. */
static bool section_key(const Plan *plan, NodeId section, SpelledKey *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_REGION) {
            *out = constraint->params.region;
            return true;
        }
    }
    return false;
}

/* Whether the section is a transition: it ends on the new key's V in a half cadence, not a PAC (R-503, R-512). */
static bool is_transition(const Plan *plan, NodeId section) {
    SectionFunction function;
    return plan_section_function_of(plan, section, &function) && function.function == SECTION_FUNCTION_TRANSITION;
}

/* The cadences of the section, replaced by these: the one it ends in, then an antecedent's weak one. */
static PieceResult set_section_cadences(Plan *plan, NodeId section, const CadenceType *cadences, size_t count) {
    SectionDescription description = *plan_section_or_empty(plan, section);
    description.has_cadence = count > 0;
    description.cadence = count > 0 ? cadences[0] : CADENCE_TYPE_PERFECT_AUTHENTIC;
    description.has_first_cadence = count > 1;
    description.first_cadence = count > 1 ? cadences[1] : CADENCE_TYPE_PERFECT_AUTHENTIC;
    return piece_result_of_plan(plan_node_set_section(plan, section, &description));
}

PieceResult piece_plan_cadences(Plan *plan, const FormSpec *form, const FormSpecSize *size, const PieceKeyPlan *keys) {
    if (!plan || !form || !size || !keys) {
        return PIECE_ERR_NULL;
    }
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t count = form_record_sections(plan, sections);
    if (count != keys->count || count != size->section_count || count == 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (!form->closing) {
        return PIECE_OK;
    }
    SpelledKey home = keys->sections[0].start;
    for (size_t i = 0; i < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        size_t have = section_cadences(plan, sections[i], cadences);
        if (have == 0) {
            return PIECE_ERR_BAD_REQUEST;
        }
        CadenceType wanted = cadences[0];
        if (is_transition(plan, sections[i]) && i + 1 < count) {
            continue; /* a transition keeps its half cadence in the key it modulates to */
        }
        if (i + 1 == count || !spelled_key_equals(&keys->sections[i].end, &home)) {
            wanted = CADENCE_TYPE_PERFECT_AUTHENTIC;
        } else if (wanted == CADENCE_TYPE_PERFECT_AUTHENTIC && !size->sections[i].closed) {
            wanted = CADENCE_TYPE_HALF;
        }
        if (wanted != cadences[0]) {
            cadences[0] = wanted;
            PieceResult result = set_section_cadences(plan, sections[i], cadences, have);
            if (result != PIECE_OK) {
                return result;
            }
        }
    }
    return PIECE_OK;
}

static void fail(PieceCadenceCheck *out, const char *what, size_t section) {
    out->ok = false;
    snprintf(out->reason, sizeof(out->reason), "%s (section %zu)", what, section);
}

bool piece_check_cadences(const Plan *plan, PieceCadenceCheck *out) {
    if (!plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    const PlanFormRecord *record = plan_form_record(plan);
    if (!record || !record->closing) {
        return true; /* nothing to check: a plan of no form, or a form that does not close (loop, layered, game loop) */
    }
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t count = form_record_sections(plan, sections);
    if (count == 0) {
        return false;
    }
    LaneSpanId first_key;
    if (plan_lane_find(plan, PLAN_LANE_REGION, 0, plan_node_span(plan, sections[0]), &first_key, 1) == 0) {
        return false;
    }
    SpelledKey home = plan_lane_span_payload(plan, first_key)->region;
    int last_strength = -1;
    for (size_t i = 0; i < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        SpelledKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        bool away = !spelled_key_equals(&ends_in, &home);
        if (away && cadences[0] != CADENCE_TYPE_PERFECT_AUTHENTIC && cadences[0] != CADENCE_TYPE_IMPERFECT_AUTHENTIC &&
            !is_transition(plan, sections[i])) {
            fail(out, "a section ends in another key without an authentic cadence", i);
        } else if (!away && i + 1 < count && cadences[0] == CADENCE_TYPE_PERFECT_AUTHENTIC && !plan_section_or_empty(plan, sections[i])->closed) {
            fail(out, "a section closes in the home key before the piece ends", i);
        }
        if (i + 1 == count) {
            last_strength = harmony_cadence_strength(cadences[0]);
        }
    }
    /* The last cadence is the strongest of the piece in the home key. */
    for (size_t i = 0; i + 1 < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        SpelledKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        if (spelled_key_equals(&ends_in, &home) &&
            harmony_cadence_strength(cadences[0]) > last_strength) {
            fail(out, "a cadence in the home key is stronger than the last one", i);
        }
    }
    CadenceType last[MAX_CADENCES];
    if (section_cadences(plan, sections[count - 1], last) == 0) {
        return false;
    }
    if (last[0] != CADENCE_TYPE_PERFECT_AUTHENTIC) {
        fail(out, "the last section does not end in a PAC", count - 1);
    }
    return true;
}

/* The Composer composes in the major, the natural minor and the church modes (R-725). */
static bool plan_material_supported(const PitchMaterial *material, void *data) {
    (void)data;
    return material != NULL && (strcmp(material->id, "omt.material.major") == 0 || strcmp(material->id, "omt.material.natural-minor") == 0 ||
                                pitch_material_is_modal_key(NULL, material));
}

static PlanResult plan_form_record_migrate(Plan *plan, void *data) {
    (void)data;
    return piece_form_record_migrate(plan) ? PLAN_OK : PLAN_ERR_ALLOC;
}

const PlanReadHooks *composer_plan_read_hooks(void) {
    static const PlanReadHooks hooks = {plan_material_supported, plan_form_record_migrate, NULL};
    return &hooks;
}
