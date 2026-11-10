#include <ursatz_composer/piece_plan_keys.h>

#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_phrase.h>
#include "util/plan_util.h"

#define MAX_CADENCES 2

/* The sections of the plan in order, and the material's id (not a section of the piece). */
static size_t sections_of(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = false;
        for (size_t c = 0; c < plan_node_constraint_count(plan, child); c++) {
            PlanConstraintKind kind = plan_node_constraint_at(plan, child, c)->kind;
            material = material || kind == PLAN_CONSTRAINT_MATERIAL || kind == PLAN_CONSTRAINT_SEAM;
        }
        if (!material && count < FORM_SPEC_MAX_SECTIONS) {
            sections[count++] = child;
        }
    }
    return count;
}

/*
 * The size of the form whose sections the plan has: as many, in the same proportions (a size of the form at another
 * scale has the same sections in the same places). The first such size; NULL for none.
 */
static const FormSpecSize *size_of_plan(const FormSpec *form, const PiecePlan *plan, const NodeId *sections, size_t count) {
    for (size_t z = 0; z < form->size_count; z++) {
        const FormSpecSize *size = &form->sizes[z];
        bool match = size->section_count == count && size->sections[0].bars > 0;
        Rational first = plan_node_span(plan, sections[0]).length;
        for (size_t i = 1; match && i < count; i++) {
            Rational left;
            Rational right;
            match = size->sections[i].bars > 0 &&
                    rational_mul_int(plan_node_span(plan, sections[i]).length, (long long)size->sections[0].bars, &left) ==
                        RATIONAL_OK &&
                    rational_mul_int(first, (long long)size->sections[i].bars, &right) == RATIONAL_OK &&
                    rational_cmp(left, right) == 0;
        }
        if (match) {
            return size;
        }
    }
    return NULL;
}

/* The cadences of the section in order: the one it ends in, then an antecedent's weak one. Returns how many. */
static size_t section_cadences(const PiecePlan *plan, NodeId section, CadenceType *out) {
    size_t count = 0;
    for (size_t i = 0; i < plan_node_constraint_count(plan, section) && count < MAX_CADENCES; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            out[count++] = constraint->params.cadence;
        }
    }
    return count;
}

/* The key the section ends in: its KEY constraint. False when it has none. */
static bool section_key(const PiecePlan *plan, NodeId section, AnalysisKey *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_KEY) {
            *out = constraint->params.key;
            return true;
        }
    }
    return false;
}

/* The CADENCE constraints of the section, replaced by these. */
static PieceResult set_section_cadences(PiecePlan *plan, NodeId section, const CadenceType *cadences, size_t count) {
    for (size_t i = plan_node_constraint_count(plan, section); i > 0; i--) {
        if (plan_node_constraint_at(plan, section, i - 1)->kind == PLAN_CONSTRAINT_CADENCE) {
            PieceResult result = piece_result_of_plan(plan_node_remove_constraint(plan, section, i - 1));
            if (result != PIECE_OK) {
                return result;
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        PlanConstraint constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = cadences[i];
        PieceResult result = piece_result_of_plan(plan_node_add_constraint(plan, section, &constraint));
        if (result != PIECE_OK) {
            return result;
        }
    }
    return PIECE_OK;
}

PieceResult piece_plan_cadences(PiecePlan *plan, const FormSpec *form, const FormSpecSize *size, const PieceKeyPlan *keys) {
    if (!plan || !form || !size || !keys) {
        return PIECE_ERR_NULL;
    }
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    if (count != keys->count || count != size->section_count || count == 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (!form->closing) {
        return PIECE_OK;
    }
    AnalysisKey home = keys->sections[0].start;
    for (size_t i = 0; i < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        size_t have = section_cadences(plan, sections[i], cadences);
        if (have == 0) {
            return PIECE_ERR_BAD_REQUEST;
        }
        CadenceType wanted = cadences[0];
        if (i + 1 == count || !analysis_key_equals(&keys->sections[i].end, &home)) {
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

static bool check_cadences(const PiecePlan *plan, const FormSpec *form, PieceCadenceCheck *out);

static void fail(PieceCadenceCheck *out, const char *what, size_t section) {
    out->ok = false;
    snprintf(out->reason, sizeof(out->reason), "%s (section %zu)", what, section);
}

bool piece_check_cadences(const PiecePlan *plan, const FormSpec *form, PieceCadenceCheck *out) {
    if (!plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    FormSpec *owned = NULL;
    if (!form) {
        if (form_spec_builtin_for_plan(plan, &owned) != FORM_SPEC_OK) {
            return false;
        }
        form = owned;
    }
    bool read = check_cadences(plan, form, out);
    form_spec_destroy(owned);
    return read;
}

static bool check_cadences(const PiecePlan *plan, const FormSpec *form, PieceCadenceCheck *out) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    if (count == 0) {
        return false;
    }
    if (!form->closing) {
        return true;
    }
    const FormSpecSize *size = size_of_plan(form, plan, sections, count);
    if (!size) {
        return false;
    }
    LaneSpanId first_key;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, plan_node_span(plan, sections[0]), &first_key, 1) == 0) {
        return false;
    }
    AnalysisKey home = plan_lane_span_payload(plan, first_key)->key;
    int last_strength = -1;
    for (size_t i = 0; i < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        AnalysisKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        bool away = !analysis_key_equals(&ends_in, &home);
        if (away && cadences[0] != CADENCE_TYPE_PERFECT_AUTHENTIC && cadences[0] != CADENCE_TYPE_IMPERFECT_AUTHENTIC) {
            fail(out, "a section ends in another key without an authentic cadence", i);
        } else if (!away && i + 1 < count && cadences[0] == CADENCE_TYPE_PERFECT_AUTHENTIC && !size->sections[i].closed) {
            fail(out, "a section closes in the home key before the piece ends", i);
        }
        if (i + 1 == count) {
            last_strength = harmony_cadence_strength(cadences[0]);
        }
    }
    /* The last cadence is the strongest of the piece in the home key. */
    for (size_t i = 0; i + 1 < count; i++) {
        CadenceType cadences[MAX_CADENCES];
        AnalysisKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        if (analysis_key_equals(&ends_in, &home) &&
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
