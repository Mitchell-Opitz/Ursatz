#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_LEDGER_LINE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_LEDGER_LINE_RULES_H

#include <stdbool.h>

#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Ledger line rules for standard-notation-v1:
 *
 *   EXTENDS_STAFF  notes too high or low for the staff are written on
 *                  ledger lines, which extend the staff
 *   NO_EXTRA       a note takes exactly the ledger lines between it and
 *                  the staff -- never an extra one above or below it
 */

typedef enum {
    STANDARD_NOTATION_LEDGER_RULE_EXTENDS_STAFF = 0,
    STANDARD_NOTATION_LEDGER_RULE_NO_EXTRA,
    STANDARD_NOTATION_LEDGER_RULE_COUNT
} StandardNotationLedgerRule;

const Rule *standard_notation_ledger_rule(StandardNotationLedgerRule which);
const Constraint *standard_notation_ledger_constraint(StandardNotationLedgerRule which);

/* EXTENDS_STAFF: ledger lines required to notate position (0 on or
   adjacent to the staff). */
int standard_notation_ledger_lines_required(const StaffPosition *position);

/* NO_EXTRA: true when drawn_count is exactly the required count. */
bool standard_notation_ledger_lines_valid(const StaffPosition *position, int drawn_count);

#endif
