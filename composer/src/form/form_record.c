#include "form/form_record.h"

#include <stdio.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece_plan_keys.h>

#include "util/plan_util.h"

/* The form a plan was built from (R-608): its record, the migration of plans that have none, and what the checks read of the plan's sections. */

/* The sections of the plan in order, and the material's id (not a section of the piece). */
size_t form_record_sections(const Plan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; form != PLAN_NODE_NONE && i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        bool material = plan_section_has_role(plan, child, SECTION_ROLE_MATERIAL) || plan_section_has_role(plan, child, SECTION_ROLE_SEAM);
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
const FormSpecSize *form_record_size_of_plan(const FormSpec *form, const Plan *plan, const NodeId *sections, size_t count) {
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

PieceResult piece_set_form_record(Plan *plan, const FormSpec *form) {
    PlanFormRecord record;
    memset(&record, 0, sizeof(record));
    snprintf(record.name, sizeof(record.name), "%s", form->name);
    record.version = FORM_SPEC_VERSION;
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        if (strcmp(form_spec_builtin_name(i), form->name) == 0) {
            const char *hash = form_spec_builtin_hash(i);
            snprintf(record.hash, sizeof(record.hash), "%s", hash ? hash : "");
        }
    }
    record.checks = form->checks;
    record.closing = form->closing;
    return piece_result_of_plan(plan_set_form_record(plan, &record));
}

bool piece_form_record_migrate(Plan *plan) {
    NodeId root = plan_root(plan);
    const char *root_name = root == PLAN_NODE_NONE ? NULL : plan_node_name(plan, root);
    for (size_t i = 0; root_name && !plan_form_record(plan) && i < form_spec_builtin_count(); i++) {
        if (strcmp(root_name, form_spec_builtin_plan_name(i)) != 0) {
            continue;
        }
        FormSpec *form = NULL;
        FormSpecResult parsed = form_spec_builtin(form_spec_builtin_name(i), &form);
        if (parsed != FORM_SPEC_OK) {
            return parsed != FORM_SPEC_ERR_ALLOC;
        }
        bool ok = piece_set_form_record(plan, form) == PIECE_OK;
        NodeId sections[FORM_SPEC_MAX_SECTIONS];
        size_t count = form_record_sections(plan, sections);
        const FormSpecSize *size = ok && form->closing && count > 0 ? form_record_size_of_plan(form, plan, sections, count) : NULL;
        for (size_t s = 0; ok && size && s < count; s++) {
            SectionDescription described = *plan_section_or_empty(plan, sections[s]);
            described.closed = size->sections[s].closed;
            ok = plan_node_set_section(plan, sections[s], &described) == PLAN_OK;
        }
        form_spec_destroy(form);
        return ok;
    }
    return true;
}
