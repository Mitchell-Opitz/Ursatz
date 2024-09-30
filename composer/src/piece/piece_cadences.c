#include <ursatz_composer/piece_plan_keys.h>

#include <stdio.h>
#include <string.h>

#include <harmony_cadence_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_phrase.h>

#define MAX_CADENCES 2

static PieceResult from_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

static bool same_key(PlanKey a, PlanKey b) {
    return a.tonic_pitch_class == b.tonic_pitch_class && a.mode == b.mode;
}

/* The sections of the plan in order, and the material's id (not a section of the piece). */
static size_t sections_of(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = false;
        for (size_t c = 0; c < plan_node_constraint_count(plan, child); c++) {
            material = material || plan_node_constraint_at(plan, child, c)->kind == PLAN_CONSTRAINT_MATERIAL;
        }
        if (!material && count < FORM_TEMPLATE_MAX_SECTIONS) {
            sections[count++] = child;
        }
    }
    return count;
}

/* Whether the template ends the piece with a cadence (the forms the plan sets cadences for). */
static bool closing_form(FormTemplateKind kind) {
    return kind == FORM_TEMPLATE_BINARY || kind == FORM_TEMPLATE_ROUNDED_BINARY || kind == FORM_TEMPLATE_TERNARY ||
           kind == FORM_TEMPLATE_VARIATIONS;
}

/* Whether the template lets the section close in the home key with a PAC before the piece ends. */
static bool closed_section(FormTemplateKind kind, size_t index) {
    switch (kind) {
    case FORM_TEMPLATE_TERNARY:
    case FORM_TEMPLATE_ROUNDED_BINARY:
        return index == 0;
    case FORM_TEMPLATE_VARIATIONS:
        return true;
    default:
        return false;
    }
}

/* The cadences of the section in order: the one it ends in, then an antecedent's weak one. Returns how many. */
static size_t section_cadences(const PiecePlan *plan, NodeId section, PlanCadenceType *out) {
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
static bool section_key(const PiecePlan *plan, NodeId section, PlanKey *out) {
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
static PieceResult set_section_cadences(PiecePlan *plan, NodeId section, const PlanCadenceType *cadences, size_t count) {
    for (size_t i = plan_node_constraint_count(plan, section); i > 0; i--) {
        if (plan_node_constraint_at(plan, section, i - 1)->kind == PLAN_CONSTRAINT_CADENCE) {
            PieceResult result = from_plan(plan_node_remove_constraint(plan, section, i - 1));
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
        PieceResult result = from_plan(plan_node_add_constraint(plan, section, &constraint));
        if (result != PIECE_OK) {
            return result;
        }
    }
    return PIECE_OK;
}

PieceResult piece_plan_cadences(PiecePlan *plan, const PieceKeyPlan *keys) {
    if (!plan || !keys) {
        return PIECE_ERR_NULL;
    }
    FormTemplateKind kind;
    const char *name = plan_node_name(plan, plan_root(plan));
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    if (!name || !form_template_from_name(name, &kind) || count != keys->count || count == 0) {
        return PIECE_ERR_BAD_REQUEST;
    }
    if (!closing_form(kind)) {
        return PIECE_OK;
    }
    PlanKey home = keys->sections[0].start;
    for (size_t i = 0; i < count; i++) {
        PlanCadenceType cadences[MAX_CADENCES];
        size_t have = section_cadences(plan, sections[i], cadences);
        if (have == 0) {
            return PIECE_ERR_BAD_REQUEST;
        }
        PlanCadenceType wanted = cadences[0];
        if (i + 1 == count || !same_key(keys->sections[i].end, home)) {
            wanted = PLAN_CADENCE_PAC;
        } else if (wanted == PLAN_CADENCE_PAC && !closed_section(kind, i)) {
            wanted = PLAN_CADENCE_HC;
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

bool piece_check_cadences(const PiecePlan *plan, PieceCadenceCheck *out) {
    if (!plan || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = true;
    FormTemplateKind kind;
    const char *name = plan_node_name(plan, plan_root(plan));
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    if (!name || !form_template_from_name(name, &kind) || count == 0) {
        return false;
    }
    if (!closing_form(kind)) {
        return true;
    }
    LaneSpanId first_key;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, plan_node_span(plan, sections[0]), &first_key, 1) == 0) {
        return false;
    }
    PlanKey home = plan_lane_span_payload(plan, first_key)->key;
    int last_strength = -1;
    for (size_t i = 0; i < count; i++) {
        PlanCadenceType cadences[MAX_CADENCES];
        PlanKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        bool away = !same_key(ends_in, home);
        if (away && cadences[0] != PLAN_CADENCE_PAC && cadences[0] != PLAN_CADENCE_IAC) {
            fail(out, "a section ends in another key without an authentic cadence", i);
        } else if (!away && i + 1 < count && cadences[0] == PLAN_CADENCE_PAC && !closed_section(kind, i)) {
            fail(out, "a section closes in the home key before the piece ends", i);
        }
        if (i + 1 == count) {
            last_strength = harmony_cadence_strength(harmony_cadence_type_of(cadences[0]));
        }
    }
    /* The last cadence is the strongest of the piece in the home key. */
    for (size_t i = 0; i + 1 < count; i++) {
        PlanCadenceType cadences[MAX_CADENCES];
        PlanKey ends_in;
        if (section_cadences(plan, sections[i], cadences) == 0 || !section_key(plan, sections[i], &ends_in)) {
            return false;
        }
        if (same_key(ends_in, home) &&
            harmony_cadence_strength(harmony_cadence_type_of(cadences[0])) > last_strength) {
            fail(out, "a cadence in the home key is stronger than the last one", i);
        }
    }
    PlanCadenceType last[MAX_CADENCES];
    if (section_cadences(plan, sections[count - 1], last) == 0) {
        return false;
    }
    if (last[0] != PLAN_CADENCE_PAC) {
        fail(out, "the last section does not end in a PAC", count - 1);
    }
    return true;
}
