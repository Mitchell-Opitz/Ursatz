#include "ledger_line_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_LEDGER_RULE_COUNT] = {"ledger-extends-staff", "ledger-no-extra"};

static StandardNotationRulePair g_pairs[STANDARD_NOTATION_LEDGER_RULE_COUNT];
static bool g_built[STANDARD_NOTATION_LEDGER_RULE_COUNT];

static bool valid_rule(StandardNotationLedgerRule which) {
    return which >= 0 && which < STANDARD_NOTATION_LEDGER_RULE_COUNT;
}

static void ensure_built(StandardNotationLedgerRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_notation_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_notation_ledger_rule(StandardNotationLedgerRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_notation_ledger_constraint(StandardNotationLedgerRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

int standard_notation_ledger_lines_required(const StaffPosition *position) {
    if (position == NULL) {
        return 0;
    }
    return staff_position_ledger_line_count(position);
}

bool standard_notation_ledger_lines_valid(const StaffPosition *position, int drawn_count) {
    if (position == NULL) {
        return false;
    }
    return drawn_count == staff_position_ledger_line_count(position);
}
