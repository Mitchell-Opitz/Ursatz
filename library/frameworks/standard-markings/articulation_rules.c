#include "articulation_rules.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_MARKINGS_ARTICULATION_RULE_COUNT] = {
    "articulation-connection-classes", "articulation-accent-force", "articulation-sforzando-accent"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_markings_framework, "standard-markings",
    NAMES, STANDARD_MARKINGS_ARTICULATION_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_markings_articulation, StandardMarkingsArticulationRule, STANDARD_MARKINGS_ARTICULATION_RULE_COUNT, g_table)

#define MAX_KIND 32

static bool normalize(const char *kind, char *buffer) {
    if (kind == NULL) {
        return false;
    }
    size_t length = strlen(kind);
    if (length == 0 || length >= MAX_KIND) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (char)tolower((unsigned char)kind[i]);
    }
    buffer[length] = '\0';
    return true;
}

bool standard_markings_is_sforzando(const char *kind) {
    char text[MAX_KIND];
    if (!normalize(kind, text)) {
        return false;
    }
    return strcmp(text, "sfz") == 0 || strcmp(text, "sf") == 0 || strcmp(text, "fz") == 0 ||
           strcmp(text, "sforzando") == 0 || strcmp(text, "forzando") == 0 || strcmp(text, "forzato") == 0;
}

ArticulationClass standard_markings_articulation_class(const char *kind) {
    char text[MAX_KIND];
    if (!normalize(kind, text)) {
        return ARTICULATION_CLASS_UNKNOWN;
    }
    if (strcmp(text, "legato") == 0 || strcmp(text, "slur") == 0 || strcmp(text, "tenuto") == 0) {
        return ARTICULATION_CLASS_CONNECTED;
    }
    if (strcmp(text, "staccato") == 0) {
        return ARTICULATION_CLASS_SEPARATED;
    }
    if (strcmp(text, "accent") == 0 || strcmp(text, "marcato") == 0 || standard_markings_is_sforzando(kind)) {
        return ARTICULATION_CLASS_ACCENTED;
    }
    return ARTICULATION_CLASS_UNKNOWN;
}

bool standard_markings_articulation_more_forceful(const char *stronger, const char *weaker) {
    char strong[MAX_KIND], weak[MAX_KIND];
    if (!normalize(stronger, strong) || !normalize(weaker, weak)) {
        return false;
    }
    if (strcmp(weak, "accent") != 0) {
        return false;
    }
    return strcmp(strong, "marcato") == 0 || standard_markings_is_sforzando(stronger);
}

bool standard_markings_articulation_is_dynamic_like(const char *kind) {
    return standard_markings_is_sforzando(kind);
}
