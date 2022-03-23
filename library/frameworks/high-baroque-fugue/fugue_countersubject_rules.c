#include "fugue_countersubject_rules.h"

#include <stdint.h>

#include "fugue_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"

static const char *const NAMES[FUGUE_COUNTERSUBJECT_RULE_COUNT] = {
    "rhythm-constant", "invertible-intervals", "complementary-rhythm", "changes-only-with-answer"};

static bool table_is_hard(int which) {
    return which == FUGUE_COUNTERSUBJECT_RHYTHM_CONSTANT;
}

static FugueRulePair g_pairs[FUGUE_COUNTERSUBJECT_RULE_COUNT];
static bool g_built[FUGUE_COUNTERSUBJECT_RULE_COUNT];
static const FugueRuleTable TABLE = {"countersubject", NAMES, FUGUE_COUNTERSUBJECT_RULE_COUNT, table_is_hard,
                                     g_pairs, g_built};

const Rule *fugue_countersubject_rule(FugueCountersubjectRule which) {
    return fugue_rule_table_rule(&TABLE, (int)which);
}

const Constraint *fugue_countersubject_constraint(FugueCountersubjectRule which) {
    return fugue_rule_table_constraint(&TABLE, (int)which);
}

const char *fugue_countersubject_rule_name(FugueCountersubjectRule which) {
    return fugue_rule_table_name(&TABLE, (int)which);
}

static bool statement_span(const FugueExposition *exposition, size_t index, size_t *start, size_t *end) {
    FugueNote notes[FUGUE_MAX_NOTES];
    size_t count;
    if (!fugue_statement_notes(exposition, index, notes, FUGUE_MAX_NOTES, &count)) {
        return false;
    }
    *start = notes[0].start;
    *end = notes[count - 1].start + notes[count - 1].length;
    return true;
}

/* The first statement of countersubject `number`. */
static bool first_of_number(const FugueExposition *exposition, size_t number, size_t *index) {
    for (size_t i = 0; i < exposition->statement_count; i++) {
        if (exposition->statements[i].role == FUGUE_ROLE_COUNTERSUBJECT && exposition->statements[i].number == number) {
            *index = i;
            return true;
        }
    }
    return false;
}

static bool check_rhythm_constant(const FugueExposition *exposition) {
    for (size_t i = 0; i < exposition->statement_count; i++) {
        const FugueStatement *s = &exposition->statements[i];
        size_t first;
        if (s->role != FUGUE_ROLE_COUNTERSUBJECT || !first_of_number(exposition, s->number, &first) || first == i) {
            continue;
        }
        FugueNote a[FUGUE_MAX_NOTES], b[FUGUE_MAX_NOTES];
        size_t na, nb;
        if (!fugue_statement_notes(exposition, first, a, FUGUE_MAX_NOTES, &na) ||
            !fugue_statement_notes(exposition, i, b, FUGUE_MAX_NOTES, &nb) || na != nb) {
            return false;
        }
        for (size_t n = 0; n < na; n++) {
            if (a[n].length != b[n].length) {
                return false;
            }
        }
    }
    return true;
}

/* The entry that countersubject statement `index` accompanies: the first entry
   in another voice beginning at or after its start. False when there is
   none. */
static bool accompanying_entry(const FugueExposition *exposition, size_t index, size_t *entry) {
    const FugueStatement *cs = &exposition->statements[index];
    for (size_t i = 0; i < exposition->statement_count; i++) {
        const FugueStatement *s = &exposition->statements[i];
        if (fugue_role_is_entry(s->role) && s->voice != cs->voice && s->start >= cs->start) {
            *entry = i;
            return true;
        }
    }
    return false;
}

/* Calls `visit` for each slot where the countersubject and its entry both
   sound and either begins a note; stops and returns false when it returns
   false. Statements with no entry to accompany are skipped. */
typedef bool (*SimultaneityVisit)(const FugueExposition *exposition, size_t cs_voice, size_t entry_voice, size_t slot,
                                  void *state);

static bool each_simultaneity(const FugueExposition *exposition, SimultaneityVisit visit, void *state) {
    for (size_t i = 0; i < exposition->statement_count; i++) {
        size_t entry, cs_start, cs_end, entry_start, entry_end;
        if (exposition->statements[i].role != FUGUE_ROLE_COUNTERSUBJECT ||
            !accompanying_entry(exposition, i, &entry)) {
            continue;
        }
        if (!statement_span(exposition, i, &cs_start, &cs_end) ||
            !statement_span(exposition, entry, &entry_start, &entry_end)) {
            return false;
        }
        size_t lo = cs_start > entry_start ? cs_start : entry_start;
        size_t hi = cs_end < entry_end ? cs_end : entry_end;
        size_t cs_voice = exposition->statements[i].voice;
        size_t entry_voice = exposition->statements[entry].voice;
        for (size_t slot = lo; slot < hi; slot++) {
            if (fugue_pitch_at(exposition, cs_voice, slot) == NULL ||
                fugue_pitch_at(exposition, entry_voice, slot) == NULL ||
                !(fugue_attacks_at(exposition, cs_voice, slot) || fugue_attacks_at(exposition, entry_voice, slot))) {
                continue;
            }
            if (!visit(exposition, cs_voice, entry_voice, slot, state)) {
                return false;
            }
        }
    }
    return true;
}

static bool visit_interval(const FugueExposition *exposition, size_t cs_voice, size_t entry_voice, size_t slot,
                           void *state) {
    (void)state;
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(fugue_pitch_at(exposition, cs_voice, slot),
                                                fugue_pitch_at(exposition, entry_voice, slot), &interval)) {
        return false;
    }
    int simple = interval.simple_size;
    return simple == 1 || simple == 2 || simple == 3 || simple == 6 || simple == 7 || simple == 8;
}

static bool check_invertible_intervals(const FugueExposition *exposition) {
    return !exposition->invertible || each_simultaneity(exposition, visit_interval, NULL);
}

typedef struct {
    size_t total;
    size_t both;
} Counts;

static bool visit_count(const FugueExposition *exposition, size_t cs_voice, size_t entry_voice, size_t slot,
                        void *state) {
    Counts *counts = state;
    counts->total++;
    counts->both += fugue_attacks_at(exposition, cs_voice, slot) && fugue_attacks_at(exposition, entry_voice, slot);
    return true;
}

static bool check_complementary_rhythm(const FugueExposition *exposition) {
    Counts counts = {0, 0};
    return each_simultaneity(exposition, visit_count, &counts) && counts.both * 2 <= counts.total;
}

/* The number of intervals whose generic size differs between two note lists
   of the same length; SIZE_MAX when the lengths differ. */
static size_t altered_between(const FugueNote *a, size_t na, const FugueNote *b, size_t nb) {
    if (na != nb) {
        return SIZE_MAX;
    }
    size_t altered = 0;
    for (size_t i = 0; i + 1 < na; i++) {
        StandardCounterpointInterval ia, ib;
        if (!standard_counterpoint_interval_between(a[i].pitch, a[i + 1].pitch, &ia) ||
            !standard_counterpoint_interval_between(b[i].pitch, b[i + 1].pitch, &ib)) {
            return SIZE_MAX;
        }
        altered += ia.size != ib.size ? 1 : 0;
    }
    return altered;
}

static bool first_with_role(const FugueExposition *exposition, FugueRole role, size_t *index) {
    for (size_t i = 0; i < exposition->statement_count; i++) {
        if (exposition->statements[i].role == role) {
            *index = i;
            return true;
        }
    }
    return false;
}

static bool check_changes_only_with_answer(const FugueExposition *exposition) {
    size_t allowed = 0;
    size_t subject, answer;
    if (first_with_role(exposition, FUGUE_ROLE_SUBJECT, &subject) &&
        first_with_role(exposition, FUGUE_ROLE_ANSWER, &answer)) {
        FugueNote a[FUGUE_MAX_NOTES], b[FUGUE_MAX_NOTES];
        size_t na, nb;
        if (!fugue_statement_notes(exposition, subject, a, FUGUE_MAX_NOTES, &na) ||
            !fugue_statement_notes(exposition, answer, b, FUGUE_MAX_NOTES, &nb)) {
            return false;
        }
        allowed = altered_between(a, na, b, nb);
    }
    for (size_t i = 0; i < exposition->statement_count; i++) {
        const FugueStatement *s = &exposition->statements[i];
        size_t first;
        if (s->role != FUGUE_ROLE_COUNTERSUBJECT || !first_of_number(exposition, s->number, &first) || first == i) {
            continue;
        }
        FugueNote a[FUGUE_MAX_NOTES], b[FUGUE_MAX_NOTES];
        size_t na, nb;
        if (!fugue_statement_notes(exposition, first, a, FUGUE_MAX_NOTES, &na) ||
            !fugue_statement_notes(exposition, i, b, FUGUE_MAX_NOTES, &nb)) {
            return false;
        }
        if (altered_between(a, na, b, nb) > allowed) {
            return false;
        }
    }
    return true;
}

bool fugue_countersubject_rule_holds(FugueCountersubjectRule which, const FugueExposition *exposition) {
    if (!fugue_rule_table_valid(&TABLE, (int)which) || !fugue_exposition_valid(exposition)) {
        return false;
    }
    switch (which) {
        case FUGUE_COUNTERSUBJECT_RHYTHM_CONSTANT:
            return check_rhythm_constant(exposition);
        case FUGUE_COUNTERSUBJECT_INVERTIBLE_INTERVALS:
            return check_invertible_intervals(exposition);
        case FUGUE_COUNTERSUBJECT_COMPLEMENTARY_RHYTHM:
            return check_complementary_rhythm(exposition);
        case FUGUE_COUNTERSUBJECT_CHANGES_ONLY_WITH_ANSWER:
            return check_changes_only_with_answer(exposition);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return fugue_countersubject_rule_holds((FugueCountersubjectRule)which, subject);
}

bool fugue_countersubject_check(const FugueExposition *exposition, unsigned *hard_violations,
                                unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !fugue_exposition_valid(exposition)) {
        return false;
    }
    fugue_rule_table_check(&TABLE, holds_adapter, exposition, hard_violations, soft_violations);
    return true;
}
