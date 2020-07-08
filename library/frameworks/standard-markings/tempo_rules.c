#include "tempo_rules.h"

#include <ctype.h>
#include <string.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_MARKINGS_TEMPO_RULE_COUNT] = {
    "tempo-groups", "tempo-order", "tempo-modifiers", "tempo-metronome", "tempo-change", "tempo-a-tempo"};

static StandardMarkingsRulePair g_pairs[STANDARD_MARKINGS_TEMPO_RULE_COUNT];
static bool g_built[STANDARD_MARKINGS_TEMPO_RULE_COUNT];

static bool valid_rule(StandardMarkingsTempoRule which) {
    return which >= 0 && which < STANDARD_MARKINGS_TEMPO_RULE_COUNT;
}

static void ensure_built(StandardMarkingsTempoRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_markings_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_markings_tempo_rule(StandardMarkingsTempoRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_markings_tempo_constraint(StandardMarkingsTempoRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

/* Slowest to fastest. */
static const char *const WORDS[STANDARD_MARKINGS_TEMPO_WORD_COUNT] = {
    "grave", "largo", "lento", "adagio", "andante", "moderato", "allegretto", "allegro", "vivace", "presto"};

#define MAX_TEXT 96

/* Lowercased copy, with surrounding spaces and trailing dots removed. */
static bool normalize(const char *text, char *buffer) {
    if (text == NULL) {
        return false;
    }
    while (*text == ' ') {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '.')) {
        length--;
    }
    if (length == 0 || length >= MAX_TEXT) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (char)tolower((unsigned char)text[i]);
    }
    buffer[length] = '\0';
    return true;
}

bool standard_markings_tempo_word_rank(const char *word, int *out_rank) {
    char text[MAX_TEXT];
    if (!normalize(word, text)) {
        return false;
    }
    for (int i = 0; i < STANDARD_MARKINGS_TEMPO_WORD_COUNT; i++) {
        if (strcmp(text, WORDS[i]) == 0) {
            *out_rank = i;
            return true;
        }
    }
    return false;
}

TempoGroup standard_markings_tempo_group(const char *word) {
    int rank;
    if (!standard_markings_tempo_word_rank(word, &rank)) {
        return TEMPO_GROUP_UNKNOWN;
    }
    if (rank <= 3) { /* grave, largo, lento, adagio */
        return TEMPO_GROUP_SLOW;
    }
    if (rank <= 5) { /* andante, moderato */
        return TEMPO_GROUP_MEDIUM;
    }
    return TEMPO_GROUP_FAST; /* allegretto, allegro, vivace, presto */
}

bool standard_markings_tempo_is_slower(const char *slower, const char *faster) {
    int slower_rank, faster_rank;
    if (!standard_markings_tempo_word_rank(slower, &slower_rank) ||
        !standard_markings_tempo_word_rank(faster, &faster_rank)) {
        return false;
    }
    return slower_rank < faster_rank;
}

bool standard_markings_is_tempo_modifier(const char *word) {
    char text[MAX_TEXT];
    if (!normalize(word, text)) {
        return false;
    }
    return strcmp(text, "assai") == 0 || strcmp(text, "espressivo") == 0 || strcmp(text, "cantabile") == 0;
}

bool standard_markings_tempo_text_rank(const char *text, int *out_rank) {
    char buffer[MAX_TEXT];
    if (!normalize(text, buffer)) {
        return false;
    }

    char *saveptr = NULL;
    char *token = strtok_r(buffer, " ", &saveptr);
    if (token == NULL) {
        return false;
    }
    int rank;
    if (!standard_markings_tempo_word_rank(token, &rank)) {
        return false;
    }
    while ((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
        if (!standard_markings_is_tempo_modifier(token)) {
            return false;
        }
    }
    *out_rank = rank;
    return true;
}

void standard_markings_metronome_seconds_per_beat(const MetronomeMarking *marking, unsigned long *out_numerator,
                                                  unsigned long *out_denominator) {
    metronome_marking_seconds_per_beat_unit(marking, out_numerator, out_denominator);
}

TempoChange standard_markings_tempo_change(const char *text) {
    char buffer[MAX_TEXT];
    if (!normalize(text, buffer)) {
        return TEMPO_CHANGE_NONE;
    }
    if (strcmp(buffer, "rit") == 0 || strcmp(buffer, "ritardando") == 0) {
        return TEMPO_CHANGE_GRADUALLY_SLOWER;
    }
    if (strcmp(buffer, "accel") == 0 || strcmp(buffer, "accelerando") == 0) {
        return TEMPO_CHANGE_GRADUALLY_FASTER;
    }
    if (strcmp(buffer, "a tempo") == 0) {
        return TEMPO_CHANGE_RETURN;
    }
    return TEMPO_CHANGE_NONE;
}

bool standard_markings_tempo_source_index(const char *const *texts, size_t count, size_t index,
                                          size_t *out_source) {
    if (texts == NULL || index >= count) {
        return false;
    }
    if (texts[index] == NULL) {
        return false;
    }

    if (standard_markings_tempo_change(texts[index]) == TEMPO_CHANGE_NONE) {
        *out_source = index; /* a setting is its own source */
        return true;
    }

    for (size_t i = index; i > 0; i--) {
        const char *earlier = texts[i - 1];
        if (earlier != NULL && standard_markings_tempo_change(earlier) == TEMPO_CHANGE_NONE) {
            *out_source = i - 1;
            return true;
        }
    }
    return false;
}
