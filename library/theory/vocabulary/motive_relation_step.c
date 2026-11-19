#include "theory/vocabulary/motive_relation_step.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/vocabulary/non_chord_tone.h"

typedef struct {
    const char *name;
    size_t params;
} Entry;

static const Entry TABLE[MOTIVE_RELATION_KIND_COUNT] = {
    {"exact", 0},
    {"transpose", 3},
    {"invert", 2},
    {"retrograde", 0},
    {"interval-change", 3},
    {"fragment", 3},
    {"liquidate", 3},
    {"augment", 1},
    {"diminish", 1},
    {"note-repeat", 2},
    {"rhythm-repeat", 3},
    {"rhythm-shift", 1},
    {"add-upbeat", 2},
    {"change-meter", 2},
    {"add-interval", 2},
    {"omit-interval", 1},
    {"ancillary-fill", 2},
    {"condense", 3},
    {"repeat-feature", 3},
    {"shift-feature", 2},
    {"harmony-inversion", 2},
    {"harmony-append", 2},
    {"harmony-insert", 2},
    {"harmony-substitute", 3},
    {"passing-harmony", 1},
    {"counter-line", 0},
    {"dominant-form", 0},
    {"liquidate-features", 2},
    {"reduce-to-frame", 0},
    {"add-non-chord-tone", 2},
    {"remove-non-chord-tone", 1},
    {"change-non-chord-tone", 2},
    {"reharmonize-frame", 2},
};

const char *motive_relation_name(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].name : NULL;
}

bool motive_relation_find(const char *name, MotiveRelationKind *kind) {
    if (!name || !kind) {
        return false;
    }
    for (size_t i = 0; i < MOTIVE_RELATION_KIND_COUNT; i++) {
        if (strcmp(TABLE[i].name, name) == 0) {
            *kind = (MotiveRelationKind)i;
            return true;
        }
    }
    return false;
}

size_t motive_relation_param_count(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].params : 0;
}

/* --- steps as text ------------------------------------------------------------------------------ */

static const char *const PARAM_NAMES[MOTIVE_RELATION_KIND_COUNT][MOTIVE_RELATION_MAX_PARAMS] = {
    [MOTIVE_RELATION_EXACT] = {NULL},
    [MOTIVE_RELATION_TRANSPOSE] = {"tonal", "by", "degree"},
    [MOTIVE_RELATION_INVERT] = {"axis", "tonal"},
    [MOTIVE_RELATION_RETROGRADE] = {NULL},
    [MOTIVE_RELATION_INTERVAL_CHANGE] = {"interval", "amount", "keep-contour"},
    [MOTIVE_RELATION_FRAGMENT] = {"position", "length", "min"},
    [MOTIVE_RELATION_LIQUIDATE] = {"drop", "keep-rhythm", "min"},
    [MOTIVE_RELATION_AUGMENT] = {"ratio"},
    [MOTIVE_RELATION_DIMINISH] = {"ratio"},
    [MOTIVE_RELATION_NOTE_REPEAT] = {"note", "count"},
    [MOTIVE_RELATION_RHYTHM_REPEAT] = {"first", "length", "target"},
    [MOTIVE_RELATION_RHYTHM_SHIFT] = {"offset"},
    [MOTIVE_RELATION_ADD_UPBEAT] = {"count", "approach"},
    [MOTIVE_RELATION_CHANGE_METER] = {"num", "den"},
    [MOTIVE_RELATION_ADD_INTERVAL] = {"interval", "steps"},
    [MOTIVE_RELATION_OMIT_INTERVAL] = {"interval"},
    [MOTIVE_RELATION_ANCILLARY_FILL] = {"interval", "kind"},
    [MOTIVE_RELATION_CONDENSE] = {"first", "last", "ratio"},
    [MOTIVE_RELATION_REPEAT_FEATURE] = {"feature", "times", "steps"},
    [MOTIVE_RELATION_SHIFT_FEATURE] = {"feature", "offset"},
    [MOTIVE_RELATION_HARMONY_INVERSION] = {"chord", "inversion"},
    [MOTIVE_RELATION_HARMONY_APPEND] = {"degree", "seventh"},
    [MOTIVE_RELATION_HARMONY_INSERT] = {"chord", "degree"},
    [MOTIVE_RELATION_HARMONY_SUBSTITUTE] = {"chord", "degree", "then"},
    [MOTIVE_RELATION_PASSING_HARMONY] = {"chord"},
    [MOTIVE_RELATION_COUNTER_LINE] = {NULL},
    [MOTIVE_RELATION_DOMINANT_FORM] = {NULL},
    [MOTIVE_RELATION_LIQUIDATE_FEATURES] = {"steps", "shorten"},
    [MOTIVE_RELATION_REDUCE_TO_FRAME] = {NULL},
    [MOTIVE_RELATION_ADD_NON_CHORD_TONE] = {"after", "type"},
    [MOTIVE_RELATION_REMOVE_NON_CHORD_TONE] = {"note"},
    [MOTIVE_RELATION_CHANGE_NON_CHORD_TONE] = {"note", "type"},
    [MOTIVE_RELATION_REHARMONIZE_FRAME] = {"degree", "seventh"},
};

/* --- parameter metadata --------------------------------------------------------------------------- */

#define NOTE_MAX (MOTIVE_RELATION_NOTE_LIMIT - 1)
#define INT_P(name, lo, hi, optional, text) \
    {name, MOTIVE_RELATION_PARAM_INT, lo, hi, 0, 0, optional, NULL, 0, text}
#define BOOL_P(name, text) {name, MOTIVE_RELATION_PARAM_BOOL, 0, 1, 0, 0, true, NULL, 0, text}
#define RATIO_P(name, optional, text) {name, MOTIVE_RELATION_PARAM_RATIO, 1, LONG_MAX, 0, 0, optional, NULL, 0, text}
#define CHOICE_P(name, lo, count, choices, optional, text) \
    {name, MOTIVE_RELATION_PARAM_CHOICE, lo, (lo) + (long)(count) - 1, 0, 0, optional, choices, count, text}

static const char *const POSITION_CHOICES[] = {"leading", "trailing", "middle"};
static const char *const FILL_CHOICES[] = {"passing", "neighbor"};
static const char *const TONE_CHOICES[NON_CHORD_TONE_COUNT] = {
    "passing",      "neighbor",    "double-neighbor", "changing-tone", "suspension",
    "anticipation", "appoggiatura", "escape-tone",    "retardation",   "pedal"};

#define TONE_P(name, text) CHOICE_P(name, 0, NON_CHORD_TONE_COUNT, TONE_CHOICES, false, text)
#define CHORD_P(name, text) INT_P(name, 0, MOTIVE_RELATION_CHORD_LIMIT - 1, false, text)
#define FEATURE_P(name, text) INT_P(name, 0, MOTIVE_RELATION_FEATURE_LIMIT - 1, false, text)
#define DEGREE_P(name, text) INT_P(name, 1, 7, false, text)

static const MotiveRelationParamInfo TRANSPOSE_INFO[] = {
    BOOL_P("tonal", "move in steps of the key's scale instead of semitones"),
    INT_P("by", -127, 127, false, "how far to move: semitones, or steps when tonal; not 0"),
    {"degree", MOTIVE_RELATION_PARAM_INT, 1, 7, 0, 0, true, NULL, 0,
     "move the first note to this scale degree instead of using by; tonal"}};
static const MotiveRelationParamInfo INVERT_INFO[] = {
    {"axis", MOTIVE_RELATION_PARAM_INT, 0, 127, -1, 0, true, NULL, 0,
     "the pitch mirrored about, a MIDI note number; the first note when -1"},
    BOOL_P("tonal", "mirror in steps of the key's scale instead of semitones")};
static const MotiveRelationParamInfo INTERVAL_CHANGE_INFO[] = {
    INT_P("interval", 0, NOTE_MAX, true, "the 1-based interval changed; 0 for every one"),
    INT_P("amount", -127, 127, false, "semitones to widen by, negative to narrow; not 0"),
    BOOL_P("keep-contour", "refuse a result that changes a direction")};
static const MotiveRelationParamInfo FRAGMENT_INFO[] = {
    CHOICE_P("position", 1, 3, POSITION_CHOICES, false, "where the fragment is taken from: leading, trailing or middle"),
    INT_P("length", 1, NOTE_MAX, false, "how many notes the fragment has"),
    INT_P("min", 0, NOTE_MAX, true, "the fewest notes the result may have; 0 for 2")};
static const MotiveRelationParamInfo LIQUIDATE_INFO[] = {
    INT_P("drop", 1, NOTE_MAX, false, "how many notes to drop, the weakest first"),
    BOOL_P("keep-rhythm", "leave a dropped note's time as a rest"),
    INT_P("min", 0, NOTE_MAX, true, "the fewest notes the result may have; 0 for 2")};
static const MotiveRelationParamInfo AUGMENT_INFO[] = {
    RATIO_P("ratio", true, "the factor times and durations are multiplied by, n:m above 1; 0:0 for 2:1")};
static const MotiveRelationParamInfo DIMINISH_INFO[] = {
    RATIO_P("ratio", true, "the factor times and durations are multiplied by, n:m below 1; 0:0 for 1:2")};
static const MotiveRelationParamInfo NOTE_REPEAT_INFO[] = {
    INT_P("note", 0, NOTE_MAX, false, "the 0-based note repeated"),
    INT_P("count", 2, 4, false, "how many notes it becomes")};
static const MotiveRelationParamInfo RHYTHM_REPEAT_INFO[] = {
    INT_P("first", 0, NOTE_MAX, false, "the first note whose rhythm is copied"),
    INT_P("length", 1, NOTE_MAX, false, "how many notes of rhythm are copied"),
    INT_P("target", 0, NOTE_MAX, false, "the note where the copied rhythm starts")};
static const MotiveRelationParamInfo RHYTHM_SHIFT_INFO[] = {
    RATIO_P("offset", false, "how far later in the bar the rhythm moves, as a fraction of a whole note")};
static const MotiveRelationParamInfo ADD_UPBEAT_INFO[] = {
    INT_P("count", 1, 3, false, "how many upbeat notes"),
    INT_P("approach", -1, 1, false, "+1 approach from below, -1 from above; not 0")};
static const MotiveRelationParamInfo CHANGE_METER_INFO[] = {
    INT_P("num", 1, 64, false, "beats in the new meter"),
    INT_P("den", 1, 16, false, "the new beat value: 1, 2, 4, 8 or 16")};
static const MotiveRelationParamInfo ADD_INTERVAL_INFO[] = {
    INT_P("interval", 1, NOTE_MAX, false, "the 1-based interval a note is added in"),
    INT_P("steps", -NOTE_MAX, NOTE_MAX, false, "scale steps from the note before the interval; not 0")};
static const MotiveRelationParamInfo OMIT_INTERVAL_INFO[] = {
    INT_P("interval", 1, NOTE_MAX, false, "the 1-based interval; the note after it goes")};
static const MotiveRelationParamInfo ANCILLARY_FILL_INFO[] = {
    INT_P("interval", 1, NOTE_MAX, false, "the 1-based interval filled"),
    CHOICE_P("kind", 0, 2, FILL_CHOICES, true, "passing notes or a neighbor")};
static const MotiveRelationParamInfo CONDENSE_INFO[] = {
    INT_P("first", 0, NOTE_MAX, false, "the first note condensed"),
    INT_P("last", 0, NOTE_MAX, false, "the last note condensed"),
    RATIO_P("ratio", false, "the factor the span is shortened by, below 1")};
static const MotiveRelationParamInfo REPEAT_FEATURE_INFO[] = {
    FEATURE_P("feature", "the 0-based labelled feature repeated"),
    INT_P("times", 1, 2, false, "how many repeats"),
    INT_P("steps", -NOTE_MAX, NOTE_MAX, false, "scale steps each repeat moves by")};
static const MotiveRelationParamInfo SHIFT_FEATURE_INFO[] = {
    FEATURE_P("feature", "the 0-based labelled feature moved"),
    RATIO_P("offset", false, "how far later the feature moves, as a fraction of a whole note")};
static const MotiveRelationParamInfo HARMONY_INVERSION_INFO[] = {
    CHORD_P("chord", "the 0-based chord"),
    INT_P("inversion", 0, 3, false, "the new inversion; 3 needs a seventh chord")};
static const MotiveRelationParamInfo HARMONY_APPEND_INFO[] = {
    DEGREE_P("degree", "the scale degree of the chord added at the end"),
    BOOL_P("seventh", "make it a seventh chord")};
static const MotiveRelationParamInfo HARMONY_INSERT_INFO[] = {
    CHORD_P("chord", "the 0-based chord the new one goes before"), DEGREE_P("degree", "the scale degree of the new chord")};
static const MotiveRelationParamInfo HARMONY_SUBSTITUTE_INFO[] = {
    CHORD_P("chord", "the 0-based chord replaced"), DEGREE_P("degree", "the scale degree of the new chord"),
    INT_P("then", 0, 7, true, "a second degree that follows it; 0 for none")};
static const MotiveRelationParamInfo PASSING_HARMONY_INFO[] = {
    CHORD_P("chord", "the 0-based chord a passing chord goes before")};
static const MotiveRelationParamInfo LIQUIDATE_FEATURES_INFO[] = {
    INT_P("steps", 1, 3, false, "how many liquidation steps"), BOOL_P("shorten", "also diminish the result 1:2")};
static const MotiveRelationParamInfo ADD_TONE_INFO[] = {
    INT_P("after", 0, NOTE_MAX, false, "the 0-based chord tone the figure follows"),
    TONE_P("type", "the non-chord tone added")};
static const MotiveRelationParamInfo REMOVE_TONE_INFO[] = {
    INT_P("note", 0, NOTE_MAX, false, "the 0-based note that is not a chord tone")};
static const MotiveRelationParamInfo CHANGE_TONE_INFO[] = {
    INT_P("note", 0, NOTE_MAX, false, "the 0-based note that is not a chord tone"),
    TONE_P("type", "the type it becomes")};
static const MotiveRelationParamInfo REHARMONIZE_INFO[] = {
    DEGREE_P("degree", "the scale degree of the one chord"), BOOL_P("seventh", "make it a seventh chord")};

typedef struct {
    const MotiveRelationParamInfo *info;
} InfoRow;

#define ROW(kind, array) [kind] = {array}
static const InfoRow INFO[MOTIVE_RELATION_KIND_COUNT] = {
    ROW(MOTIVE_RELATION_TRANSPOSE, TRANSPOSE_INFO),
    ROW(MOTIVE_RELATION_INVERT, INVERT_INFO),
    ROW(MOTIVE_RELATION_INTERVAL_CHANGE, INTERVAL_CHANGE_INFO),
    ROW(MOTIVE_RELATION_FRAGMENT, FRAGMENT_INFO),
    ROW(MOTIVE_RELATION_LIQUIDATE, LIQUIDATE_INFO),
    ROW(MOTIVE_RELATION_AUGMENT, AUGMENT_INFO),
    ROW(MOTIVE_RELATION_DIMINISH, DIMINISH_INFO),
    ROW(MOTIVE_RELATION_NOTE_REPEAT, NOTE_REPEAT_INFO),
    ROW(MOTIVE_RELATION_RHYTHM_REPEAT, RHYTHM_REPEAT_INFO),
    ROW(MOTIVE_RELATION_RHYTHM_SHIFT, RHYTHM_SHIFT_INFO),
    ROW(MOTIVE_RELATION_ADD_UPBEAT, ADD_UPBEAT_INFO),
    ROW(MOTIVE_RELATION_CHANGE_METER, CHANGE_METER_INFO),
    ROW(MOTIVE_RELATION_ADD_INTERVAL, ADD_INTERVAL_INFO),
    ROW(MOTIVE_RELATION_OMIT_INTERVAL, OMIT_INTERVAL_INFO),
    ROW(MOTIVE_RELATION_ANCILLARY_FILL, ANCILLARY_FILL_INFO),
    ROW(MOTIVE_RELATION_CONDENSE, CONDENSE_INFO),
    ROW(MOTIVE_RELATION_REPEAT_FEATURE, REPEAT_FEATURE_INFO),
    ROW(MOTIVE_RELATION_SHIFT_FEATURE, SHIFT_FEATURE_INFO),
    ROW(MOTIVE_RELATION_HARMONY_INVERSION, HARMONY_INVERSION_INFO),
    ROW(MOTIVE_RELATION_HARMONY_APPEND, HARMONY_APPEND_INFO),
    ROW(MOTIVE_RELATION_HARMONY_INSERT, HARMONY_INSERT_INFO),
    ROW(MOTIVE_RELATION_HARMONY_SUBSTITUTE, HARMONY_SUBSTITUTE_INFO),
    ROW(MOTIVE_RELATION_PASSING_HARMONY, PASSING_HARMONY_INFO),
    ROW(MOTIVE_RELATION_LIQUIDATE_FEATURES, LIQUIDATE_FEATURES_INFO),
    ROW(MOTIVE_RELATION_ADD_NON_CHORD_TONE, ADD_TONE_INFO),
    ROW(MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, REMOVE_TONE_INFO),
    ROW(MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, CHANGE_TONE_INFO),
    ROW(MOTIVE_RELATION_REHARMONIZE_FRAME, REHARMONIZE_INFO),
};

bool motive_relation_param_info(MotiveRelationKind kind, size_t index, MotiveRelationParamInfo *out) {
    if (!out || (unsigned)kind >= MOTIVE_RELATION_KIND_COUNT || index >= motive_relation_param_count(kind) ||
        !INFO[kind].info) {
        return false;
    }
    *out = INFO[kind].info[index];
    return true;
}

static bool param_is_ratio(MotiveRelationKind kind, size_t index) {
    switch (kind) {
    case MOTIVE_RELATION_AUGMENT:
    case MOTIVE_RELATION_DIMINISH:
    case MOTIVE_RELATION_RHYTHM_SHIFT:
        return index == 0;
    case MOTIVE_RELATION_CONDENSE:
        return index == 2;
    case MOTIVE_RELATION_SHIFT_FEATURE:
        return index == 1;
    default:
        return false;
    }
}

static long param_default(MotiveRelationKind kind, size_t index) {
    return kind == MOTIVE_RELATION_INVERT && index == 0 ? -1 : 0;
}

bool motive_relation_step_text(const MotiveRelationStep *step, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (!step || (unsigned)step->kind >= MOTIVE_RELATION_KIND_COUNT) {
        return false;
    }
    size_t used = 0;
    int written = snprintf(buffer, size, "%s", TABLE[step->kind].name);
    if (written < 0 || (size_t)written >= size) {
        buffer[0] = '\0';
        return false;
    }
    used = (size_t)written;
    for (size_t i = 0; i < TABLE[step->kind].params; i++) {
        if (step->value[i] == param_default(step->kind, i)) {
            continue;
        }
        if (param_is_ratio(step->kind, i)) {
            written = snprintf(buffer + used, size - used, " %s=%ld:%ld", PARAM_NAMES[step->kind][i], step->value[i],
                               step->den[i]);
        } else {
            written = snprintf(buffer + used, size - used, " %s=%ld", PARAM_NAMES[step->kind][i], step->value[i]);
        }
        if (written < 0 || (size_t)written >= size - used) {
            buffer[used] = '\0';
            return false;
        }
        used += (size_t)written;
    }
    return true;
}

/* The next space-separated token of *cursor, cut in place; NULL at the end. */
static char *next_token(char **cursor) {
    char *start = *cursor;
    while (*start == ' ') {
        start++;
    }
    if (*start == '\0') {
        *cursor = start;
        return NULL;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    if (*end != '\0') {
        *end++ = '\0';
    }
    *cursor = end;
    return start;
}

static bool parse_whole(const char *text, long *out) {
    if (*text == '\0') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

bool motive_relation_step_parse(const char *text, MotiveRelationStep *out) {
    if (!text || !out) {
        return false;
    }
    char copy[160];
    if (strlen(text) >= sizeof(copy)) {
        return false;
    }
    strcpy(copy, text);
    char *save = copy;
    char *name = next_token(&save);
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    if (!name || !motive_relation_find(name, &step.kind)) {
        return false;
    }
    for (size_t i = 0; i < MOTIVE_RELATION_MAX_PARAMS; i++) {
        step.value[i] = param_default(step.kind, i);
    }
    bool seen[MOTIVE_RELATION_MAX_PARAMS] = {false};
    for (char *token = next_token(&save); token; token = next_token(&save)) {
        char *equals = strchr(token, '=');
        if (!equals) {
            return false;
        }
        *equals = '\0';
        const char *value = equals + 1;
        size_t index = TABLE[step.kind].params;
        for (size_t i = 0; i < TABLE[step.kind].params; i++) {
            if (strcmp(token, PARAM_NAMES[step.kind][i]) == 0) {
                index = i;
                break;
            }
        }
        if (index == TABLE[step.kind].params || seen[index]) {
            return false;
        }
        seen[index] = true;
        if (param_is_ratio(step.kind, index)) {
            char *colon = strchr(value, ':');
            long n = 0;
            long d = 0;
            if (!colon) {
                return false;
            }
            *colon = '\0';
            if (!parse_whole(value, &n) || !parse_whole(colon + 1, &d) || n <= 0 || d <= 0) {
                return false;
            }
            step.value[index] = n;
            step.den[index] = d;
        } else if (!parse_whole(value, &step.value[index])) {
            return false;
        }
    }
    *out = step;
    return true;
}
