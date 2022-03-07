#include "fugue_entry_rules.h"

#include "fugue_rule_table.h"

static const char *const NAMES[FUGUE_ENTRY_RULE_COUNT] = {"first-is-subject",     "entries-alternate",
                                                          "each-voice-enters-once", "entries-aligned",
                                                          "new-voice-at-extreme",  "countersubject-follows"};

static bool table_is_hard(int which) {
    return which <= FUGUE_ENTRY_ENTRIES_ALIGNED;
}

static FugueRulePair g_pairs[FUGUE_ENTRY_RULE_COUNT];
static bool g_built[FUGUE_ENTRY_RULE_COUNT];
static const FugueRuleTable TABLE = {"entry", NAMES, FUGUE_ENTRY_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *fugue_entry_rule(FugueEntryRule which) {
    return fugue_rule_table_rule(&TABLE, (int)which);
}

const Constraint *fugue_entry_constraint(FugueEntryRule which) {
    return fugue_rule_table_constraint(&TABLE, (int)which);
}

const char *fugue_entry_rule_name(FugueEntryRule which) {
    return fugue_rule_table_name(&TABLE, (int)which);
}

/* The indexes of the statements that are entries, in order. */
static size_t entries_of(const FugueExposition *exposition, size_t *out) {
    size_t n = 0;
    for (size_t i = 0; i < exposition->statement_count; i++) {
        if (fugue_role_is_entry(exposition->statements[i].role)) {
            out[n++] = i;
        }
    }
    return n;
}

size_t fugue_exposition_entry_count(const FugueExposition *exposition) {
    if (!fugue_exposition_valid(exposition)) {
        return 0;
    }
    size_t entries[FUGUE_MAX_STATEMENTS];
    return entries_of(exposition, entries);
}

size_t fugue_exposition_redundant_entries(const FugueExposition *exposition) {
    size_t entries = fugue_exposition_entry_count(exposition);
    return entries > exposition->voice_count ? entries - exposition->voice_count : 0;
}

/* The number of entries before any redundant one. */
static size_t first_round(const FugueExposition *exposition, size_t entry_count) {
    return entry_count < exposition->voice_count ? entry_count : exposition->voice_count;
}

static bool check_first_is_subject(const FugueExposition *exposition, const size_t *entries, size_t count) {
    return count > 0 && exposition->statements[entries[0]].role == FUGUE_ROLE_SUBJECT;
}

static bool check_entries_alternate(const FugueExposition *exposition, const size_t *entries, size_t count) {
    size_t round = first_round(exposition, count);
    for (size_t k = 0; k < round; k++) {
        FugueRole expected = k % 2 == 0 ? FUGUE_ROLE_SUBJECT : FUGUE_ROLE_ANSWER;
        if (exposition->statements[entries[k]].role != expected) {
            return false;
        }
    }
    return true;
}

static bool check_each_voice_enters_once(const FugueExposition *exposition, const size_t *entries, size_t count) {
    size_t round = first_round(exposition, count);
    for (size_t a = 0; a < round; a++) {
        for (size_t b = a + 1; b < round; b++) {
            if (exposition->statements[entries[a]].voice == exposition->statements[entries[b]].voice) {
                return false;
            }
        }
    }
    return true;
}

static bool check_entries_aligned(const FugueExposition *exposition, const size_t *entries, size_t count) {
    for (size_t k = 1; k < count; k++) {
        if (exposition->statements[entries[k]].start % exposition->slots_per_bar !=
            exposition->statements[entries[0]].start % exposition->slots_per_bar) {
            return false;
        }
    }
    return true;
}

static bool check_new_voice_at_extreme(const FugueExposition *exposition, const size_t *entries, size_t count) {
    size_t round = first_round(exposition, count);
    for (size_t k = 1; k < round; k++) {
        size_t voice = exposition->statements[entries[k]].voice;
        bool highest = true;
        bool lowest = true;
        for (size_t j = 0; j < k; j++) {
            size_t earlier = exposition->statements[entries[j]].voice;
            highest = highest && voice < earlier;
            lowest = lowest && voice > earlier;
        }
        if (!highest && !lowest) {
            return false;
        }
    }
    return true;
}

/* The slot where statement `index` ends: after its last note. */
static bool statement_end(const FugueExposition *exposition, size_t index, size_t *end) {
    FugueNote notes[FUGUE_MAX_NOTES];
    size_t count;
    if (!fugue_statement_notes(exposition, index, notes, FUGUE_MAX_NOTES, &count)) {
        return false;
    }
    *end = notes[count - 1].start + notes[count - 1].length;
    return true;
}

static bool check_countersubject_follows(const FugueExposition *exposition, const size_t *entries, size_t count) {
    size_t round = first_round(exposition, count);
    for (size_t k = 0; k + 1 < round; k++) {
        size_t end;
        if (!statement_end(exposition, entries[k], &end)) {
            return false;
        }
        size_t voice = exposition->statements[entries[k]].voice;
        size_t next_start = exposition->statements[entries[k + 1]].start;
        bool found = false;
        for (size_t i = 0; i < exposition->statement_count && !found; i++) {
            const FugueStatement *s = &exposition->statements[i];
            found = s->role == FUGUE_ROLE_COUNTERSUBJECT && s->voice == voice && s->start >= end &&
                    s->start <= next_start;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

typedef bool (*EntryCheck)(const FugueExposition *exposition, const size_t *entries, size_t count);

static const EntryCheck CHECKS[FUGUE_ENTRY_RULE_COUNT] = {
    check_first_is_subject,      check_entries_alternate,    check_each_voice_enters_once,
    check_entries_aligned,       check_new_voice_at_extreme, check_countersubject_follows};

bool fugue_entry_rule_holds(FugueEntryRule which, const FugueExposition *exposition) {
    if (!fugue_rule_table_valid(&TABLE, (int)which) || !fugue_exposition_valid(exposition)) {
        return false;
    }
    size_t entries[FUGUE_MAX_STATEMENTS];
    size_t count = entries_of(exposition, entries);
    return CHECKS[which](exposition, entries, count);
}

static bool holds_adapter(int which, const void *subject) {
    return fugue_entry_rule_holds((FugueEntryRule)which, subject);
}

bool fugue_entry_check(const FugueExposition *exposition, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !fugue_exposition_valid(exposition)) {
        return false;
    }
    fugue_rule_table_check(&TABLE, holds_adapter, exposition, hard_violations, soft_violations);
    return true;
}
