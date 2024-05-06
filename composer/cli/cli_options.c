#include "cli_options.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int cli_fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

bool cli_equals_ignore_case(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

bool cli_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out) {
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

bool cli_parse_key(const char *text, PlanKey *key) {
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
    if (cli_equals_ignore_case(mode, "major")) {
        key->mode = PLAN_MODE_MAJOR;
    } else if (cli_equals_ignore_case(mode, "minor")) {
        key->mode = PLAN_MODE_MINOR;
    } else {
        return false;
    }
    key->tonic_pitch_class = (pc % 12 + 12) % 12;
    return true;
}

const char *cli_key_name(const PlanKey *key, char *out, size_t size) {
    static const char *const MAJOR[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MINOR[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    int pc = key->tonic_pitch_class;
    snprintf(out, size, "%s %s", key->mode == PLAN_MODE_MINOR ? MINOR[pc] : MAJOR[pc],
             key->mode == PLAN_MODE_MINOR ? "minor" : "major");
    return out;
}

bool cli_parse_meter(const char *text, PlanMeter *meter) {
    unsigned long beats;
    unsigned long unit;
    char first[16];
    const char *slash = strchr(text, '/');
    if (!slash || (size_t)(slash - text) >= sizeof(first)) {
        return false;
    }
    memcpy(first, text, (size_t)(slash - text));
    first[slash - text] = '\0';
    if (!cli_parse_whole_number(first, 64, &beats) || !cli_parse_whole_number(slash + 1, 64, &unit) || beats == 0 ||
        unit == 0) {
        return false;
    }
    meter->beats_per_bar = (int)beats;
    meter->beat_unit = (int)unit;
    meter->bars_per_hyperbar = 0;
    return true;
}

static const char *const CADENCE_NAMES[5] = {"PAC", "IAC", "HC", "DC", "PC"};

bool cli_parse_cadence(const char *text, PlanCadenceType *cadence) {
    for (int i = 0; i < 5; i++) {
        if (cli_equals_ignore_case(text, CADENCE_NAMES[i])) {
            *cadence = (PlanCadenceType)i;
            return true;
        }
    }
    return false;
}

const char *cli_cadence_name(PlanCadenceType cadence) {
    return CADENCE_NAMES[cadence];
}

unsigned long cli_default_seed(void) {
    return ((unsigned long)time(NULL) * 2654435761UL + (unsigned long)clock()) & CLI_SEED_MAX;
}

bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size) {
    if (!several) {
        return snprintf(out, size, "%s", path) < (int)size;
    }
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return snprintf(out, size, "%s-%zu", path, k) < (int)size;
    }
    return snprintf(out, size, "%.*s-%zu%s", (int)(dot - path), path, k, dot) < (int)size;
}
