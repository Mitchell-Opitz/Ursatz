#include "util/request_parse.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "util/plan_util.h"
#include "util/util.h"

bool request_equals_ignore_case(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

bool request_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out) {
    char *end = NULL;
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > maximum) {
        return false;
    }
    *out = value;
    return true;
}

bool request_parse_ratio(const char *text, ComposerRatio *out) {
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    char *end = NULL;
    errno = 0;
    unsigned long numerator = strtoul(text, &end, 10);
    unsigned long denominator = 1;
    if (errno != 0 || numerator > 4294967295UL) {
        return false;
    }
    if (*end == '/') {
        if (!isdigit((unsigned char)end[1])) {
            return false;
        }
        errno = 0;
        denominator = strtoul(end + 1, &end, 10);
        if (errno != 0 || denominator == 0 || denominator > 4294967295UL) {
            return false;
        }
    }
    if (*end != '\0') {
        return false;
    }
    out->numerator = numerator;
    out->denominator = denominator;
    return true;
}

bool request_parse_key(const char *text, AnalysisKey *key) {
    static const int NATURAL[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char letter = (char)toupper((unsigned char)*text);
    if (letter < 'A' || letter > 'G') {
        return false;
    }
    int pc = NATURAL[letter - 'A'];
    text++;
    if (*text == '#') {
        pc++;
        text++;
    } else if (*text == 'b') {
        pc--;
        text++;
    }
    if (!isspace((unsigned char)*text)) {
        return false;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char mode[8];
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length]) && length + 1 < sizeof(mode)) {
        mode[length] = text[length];
        length++;
    }
    mode[length] = '\0';
    const char *rest = text + length;
    while (isspace((unsigned char)*rest)) {
        rest++;
    }
    if (*rest != '\0') {
        return false;
    }
    if (request_equals_ignore_case(mode, "major")) {
        key->mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (request_equals_ignore_case(mode, "minor")) {
        key->mode = KEY_STRUCTURE_MODE_MINOR;
    } else {
        return false;
    }
    *key = plan_key_of_pitch_class((pc % 12 + 12) % 12, key->mode);
    return true;
}

bool request_parse_meter(const char *text, PlanMeter *meter) {
    unsigned long beats;
    unsigned long unit;
    char first[16];
    const char *slash = strchr(text, '/');
    if (!slash || (size_t)(slash - text) >= sizeof(first)) {
        return false;
    }
    memcpy(first, text, (size_t)(slash - text));
    first[slash - text] = '\0';
    if (!request_parse_whole_number(first, 64, &beats) || !request_parse_whole_number(slash + 1, 64, &unit) || beats == 0 ||
        unit == 0) {
        return false;
    }
    meter->beats_per_bar = (int)beats;
    meter->beat_unit = (int)unit;
    meter->bars_per_hyperbar = 0;
    return true;
}

bool request_parse_cadence(const char *text, CadenceType *cadence) {
    for (size_t i = 0; i < PLAN_CADENCE_COUNT; i++) {
        if (request_equals_ignore_case(text, plan_cadence_names[i])) {
            *cadence = plan_cadence_types[i];
            return true;
        }
    }
    return false;
}

bool request_parse_pickup(const char *text, int beat_unit, Rational *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && rational_make(numerator, (long long)(denominator * beat_unit), out) == RATIONAL_OK;
}
