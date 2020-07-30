#include "structural_rules.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT] = {
    "structural-fermata-hold", "structural-caesura-break", "structural-breath-mark", "structural-repeat-signs",
    "structural-numbered-endings", "structural-measure-repeats", "structural-jump-markers"};

static StandardMarkingsRulePair g_pairs[STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT];
static bool g_built[STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT];

static bool valid_rule(StandardMarkingsStructuralRule which) {
    return which >= 0 && which < STANDARD_MARKINGS_STRUCTURAL_RULE_COUNT;
}

static void ensure_built(StandardMarkingsStructuralRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_markings_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_markings_structural_rule(StandardMarkingsStructuralRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_markings_structural_constraint(StandardMarkingsStructuralRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

typedef struct {
    const char *name;
    StructuralKind kind;
} KindName;

static const KindName KIND_NAMES[] = {
    {"fermata", STRUCTURAL_KIND_FERMATA},
    {"caesura", STRUCTURAL_KIND_CAESURA},
    {"breath-mark", STRUCTURAL_KIND_BREATH_MARK},
    {"repeat-start", STRUCTURAL_KIND_REPEAT_START},
    {"repeat-end", STRUCTURAL_KIND_REPEAT_END},
    {"ending", STRUCTURAL_KIND_ENDING},
    {"one-measure-repeat", STRUCTURAL_KIND_ONE_MEASURE_REPEAT},
    {"two-measure-repeat", STRUCTURAL_KIND_TWO_MEASURE_REPEAT},
    {"segno", STRUCTURAL_KIND_SEGNO},
    {"coda", STRUCTURAL_KIND_CODA},
    {"to-coda", STRUCTURAL_KIND_TO_CODA},
    {"fine", STRUCTURAL_KIND_FINE},
    {"dc-al-coda", STRUCTURAL_KIND_DC_AL_CODA},
    {"dc-al-fine", STRUCTURAL_KIND_DC_AL_FINE},
    {"ds-al-coda", STRUCTURAL_KIND_DS_AL_CODA},
    {"ds-al-fine", STRUCTURAL_KIND_DS_AL_FINE},
};

#define MAX_KIND 32

StructuralKind standard_markings_structural_kind(const char *kind) {
    if (kind == NULL) {
        return STRUCTURAL_KIND_UNKNOWN;
    }
    size_t length = strlen(kind);
    if (length == 0 || length >= MAX_KIND) {
        return STRUCTURAL_KIND_UNKNOWN;
    }
    char text[MAX_KIND];
    for (size_t i = 0; i < length; i++) {
        text[i] = (char)tolower((unsigned char)kind[i]);
    }
    text[length] = '\0';

    for (size_t i = 0; i < sizeof(KIND_NAMES) / sizeof(KIND_NAMES[0]); i++) {
        if (strcmp(text, KIND_NAMES[i].name) == 0) {
            return KIND_NAMES[i].kind;
        }
    }
    return STRUCTURAL_KIND_UNKNOWN;
}

bool standard_markings_extends_duration(StructuralKind kind) {
    return kind == STRUCTURAL_KIND_FERMATA;
}

bool standard_markings_interrupts_flow(StructuralKind kind) {
    return kind == STRUCTURAL_KIND_CAESURA || kind == STRUCTURAL_KIND_BREATH_MARK;
}

bool standard_markings_marker_well_formed(StructuralKind kind, unsigned int parameter) {
    if (kind == STRUCTURAL_KIND_UNKNOWN) {
        return false;
    }
    if (kind == STRUCTURAL_KIND_ENDING) {
        return parameter >= 1;
    }
    return true;
}

unsigned int standard_markings_measure_repeat_span(StructuralKind kind) {
    if (kind == STRUCTURAL_KIND_ONE_MEASURE_REPEAT) {
        return 1;
    }
    if (kind == STRUCTURAL_KIND_TWO_MEASURE_REPEAT) {
        return 2;
    }
    return 0;
}

StructuralJump standard_markings_jump_target(StructuralKind kind) {
    if (kind == STRUCTURAL_KIND_DC_AL_CODA || kind == STRUCTURAL_KIND_DC_AL_FINE) {
        return STRUCTURAL_JUMP_TO_START;
    }
    if (kind == STRUCTURAL_KIND_DS_AL_CODA || kind == STRUCTURAL_KIND_DS_AL_FINE) {
        return STRUCTURAL_JUMP_TO_SEGNO;
    }
    return STRUCTURAL_JUMP_NONE;
}

StructuralStop standard_markings_jump_stop(StructuralKind kind) {
    if (kind == STRUCTURAL_KIND_DC_AL_CODA || kind == STRUCTURAL_KIND_DS_AL_CODA) {
        return STRUCTURAL_STOP_AT_CODA;
    }
    if (kind == STRUCTURAL_KIND_DC_AL_FINE || kind == STRUCTURAL_KIND_DS_AL_FINE) {
        return STRUCTURAL_STOP_AT_FINE;
    }
    return STRUCTURAL_STOP_NONE;
}
