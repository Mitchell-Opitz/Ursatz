#include "ledger_line_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_LEDGER_RULE_COUNT] = {"ledger-extends-staff", "ledger-no-extra"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_LEDGER_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_ledger, StandardNotationLedgerRule, STANDARD_NOTATION_LEDGER_RULE_COUNT, g_table)

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
