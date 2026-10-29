#include "dynamics_rules.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_MARKINGS_DYNAMICS_RULE_COUNT] = {
    "dynamics-loudness-order", "dynamics-issimo-stacking", "dynamics-mezzo-moderate", "dynamics-change"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_markings_framework, "standard-markings",
    NAMES, STANDARD_MARKINGS_DYNAMICS_RULE_COUNT);

static bool valid_rule(StandardMarkingsDynamicsRule which) {
    return which >= 0 && which < STANDARD_MARKINGS_DYNAMICS_RULE_COUNT;
}

const Rule *standard_markings_dynamics_rule(StandardMarkingsDynamicsRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_markings_dynamics_constraint(StandardMarkingsDynamicsRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

#define MAX_MARKING 64

/* Lowercased copy with surrounding spaces and one trailing '.' removed. */
static bool normalize(const char *marking, char *buffer) {
    if (marking == NULL) {
        return false;
    }
    while (*marking == ' ') {
        marking++;
    }
    size_t length = strlen(marking);
    while (length > 0 && (marking[length - 1] == ' ' || marking[length - 1] == '.')) {
        length--;
    }
    if (length == 0 || length >= MAX_MARKING) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (char)tolower((unsigned char)marking[i]);
    }
    buffer[length] = '\0';
    return true;
}

static bool is_all(const char *text, char letter, size_t min_count) {
    size_t length = strlen(text);
    if (length < min_count) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (text[i] != letter) {
            return false;
        }
    }
    return true;
}

/* The plain word: "piano" or "forte" alone is one letter (p or f). */
static bool is_word(const char *text, const char *word) {
    return strcmp(text, word) == 0;
}

/*
 * Stacked -issimo words are the stem ("pian", "fort") followed by
 * ("iss")^k then "imo", for k >= 1, naming k + 1 letters:
 *
 *   pianissimo = pian + iss + imo       -> pp  (2)
 *   pianississimo = pian + iss iss + imo -> ppp (3)
 *
 * Returns the letter count through out_count.
 */
static bool parse_issimo(const char *text, const char *stem, int *out_count) {
    size_t stem_length = strlen(stem);
    if (strncmp(text, stem, stem_length) != 0) {
        return false;
    }
    const char *rest = text + stem_length;
    int stacks = 0;
    while (strncmp(rest, "iss", 3) == 0) {
        rest += 3;
        stacks++;
    }
    if (stacks == 0 || strcmp(rest, "imo") != 0) {
        return false;
    }
    *out_count = stacks + 1;
    return true;
}

bool standard_markings_dynamic_rank(const char *marking, int *out_rank) {
    char text[MAX_MARKING];
    if (!normalize(marking, text)) {
        return false;
    }

    int count;
    int rank;
    if (strcmp(text, "mp") == 0 || strcmp(text, "mezzo piano") == 0 || strcmp(text, "mezzo-piano") == 0) {
        rank = -1;
    } else if (strcmp(text, "mf") == 0 || strcmp(text, "mezzo forte") == 0 || strcmp(text, "mezzo-forte") == 0) {
        rank = 1;
    } else if (is_all(text, 'p', 1)) {
        rank = -((int)strlen(text) + 1);
    } else if (is_all(text, 'f', 1)) {
        rank = (int)strlen(text) + 1;
    } else if (is_word(text, "piano")) {
        rank = -2;
    } else if (is_word(text, "forte")) {
        rank = 2;
    } else if (parse_issimo(text, "pian", &count)) {
        rank = -(count + 1);
    } else if (parse_issimo(text, "fort", &count)) {
        rank = count + 1;
    } else {
        return false;
    }

    *out_rank = rank;
    return true;
}

bool standard_markings_is_dynamic_level(const char *marking) {
    int rank;
    return standard_markings_dynamic_rank(marking, &rank);
}

bool standard_markings_dynamic_is_quieter(const char *quieter, const char *louder) {
    int quieter_rank, louder_rank;
    if (!standard_markings_dynamic_rank(quieter, &quieter_rank) ||
        !standard_markings_dynamic_rank(louder, &louder_rank)) {
        return false;
    }
    return quieter_rank < louder_rank;
}

bool standard_markings_dynamic_abbreviation(const char *marking, char *buffer, unsigned long buffer_size) {
    int rank;
    if (buffer == NULL || !standard_markings_dynamic_rank(marking, &rank)) {
        return false;
    }

    char abbreviation[MAX_MARKING];
    size_t length = 0;
    if (rank == -1) {
        memcpy(abbreviation, "mp", 3);
        length = 2;
    } else if (rank == 1) {
        memcpy(abbreviation, "mf", 3);
        length = 2;
    } else {
        char letter = rank < 0 ? 'p' : 'f';
        int count = (rank < 0 ? -rank : rank) - 1;
        if (count >= MAX_MARKING) {
            return false;
        }
        for (int i = 0; i < count; i++) {
            abbreviation[length++] = letter;
        }
        abbreviation[length] = '\0';
    }

    if (length + 1 > buffer_size) {
        return false;
    }
    memcpy(buffer, abbreviation, length + 1);
    return true;
}

DynamicChange standard_markings_dynamic_change(const char *marking) {
    char text[MAX_MARKING];
    if (!normalize(marking, text)) {
        return DYNAMIC_CHANGE_NONE;
    }

    if (strcmp(text, "<") == 0 || strcmp(text, "cresc") == 0 || strcmp(text, "crescendo") == 0) {
        return DYNAMIC_CHANGE_LOUDER;
    }
    if (strcmp(text, ">") == 0 || strcmp(text, "decresc") == 0 || strcmp(text, "decrescendo") == 0 ||
        strcmp(text, "dim") == 0 || strcmp(text, "diminuendo") == 0) {
        return DYNAMIC_CHANGE_QUIETER;
    }
    return DYNAMIC_CHANGE_NONE;
}
