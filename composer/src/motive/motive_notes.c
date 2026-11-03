#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "motive/motive_internal.h"

/*
 * The note-list text syntax (see motive.h): tokens of PITCH ":" DURATION ["~"], separated by white space, with "|"
 * ignored as a barline.
 */

#define TOKEN_MAX 32

typedef struct {
    char text[TOKEN_MAX];
    size_t number; /* 1-based */
    size_t column; /* 1-based, in bytes */
} Token;

static void token_location(const Token *token, char *out, size_t size) {
    snprintf(out, size, "token %zu (column %zu)", token->number, token->column);
}

/* The pitch class of a letter A-G, or -1. */
static int letter_pitch_class(char letter) {
    switch (letter) {
    case 'C':
        return 0;
    case 'D':
        return 2;
    case 'E':
        return 4;
    case 'F':
        return 5;
    case 'G':
        return 7;
    case 'A':
        return 9;
    case 'B':
        return 11;
    default:
        return -1;
    }
}

/* Parses "D4", "F#3", "Bb5", "C-1" into a MIDI note number. On failure writes the reason to message. */
static bool parse_pitch(const char *text, int *midi, char *message, size_t message_size) {
    int pitch_class = letter_pitch_class(text[0]);
    if (pitch_class < 0) {
        snprintf(message, message_size, "'%s' is not a pitch; start with a letter A-G (or r for a rest)", text);
        return false;
    }
    const char *cursor = text + 1;
    int accidental = 0;
    int accidentals = 0;
    while (*cursor == '#' || *cursor == 'b') {
        accidental += *cursor == '#' ? 1 : -1;
        accidentals++;
        cursor++;
    }
    if (accidentals > 2) {
        snprintf(message, message_size, "'%s' has more than two accidentals", text);
        return false;
    }
    const char *digits = cursor;
    if (*cursor == '-') {
        cursor++;
    }
    if (!isdigit((unsigned char)*cursor)) {
        snprintf(message, message_size, "'%s' needs an octave number, as in %c4", text, text[0]);
        return false;
    }
    char *end = NULL;
    long octave = strtol(digits, &end, 10);
    if (*end != '\0') {
        snprintf(message, message_size, "'%s' is not a pitch: '%s' is not an octave number", text, digits);
        return false;
    }
    long value = (octave + 1) * 12 + pitch_class + accidental;
    if (octave < -1 || octave > 9 || value < 0 || value > 127) {
        snprintf(message, message_size, "'%s' is outside the MIDI range (C-1 to G9)", text);
        return false;
    }
    *midi = (int)value;
    return true;
}

/* The shape of one pitched token of a list: how its pitch text becomes a MIDI note number. */
typedef bool (*PitchParser)(const char *text, const AnalysisKey *key, int *midi, char *message, size_t message_size);

static bool parse_note_pitch(const char *text, const AnalysisKey *key, int *midi, char *message, size_t message_size) {
    (void)key;
    return parse_pitch(text, midi, message, message_size);
}

/*
 * Parses "1", "b3", "#4", "5'", "1,," into a MIDI note number: an optional # or b, a degree 1 to 7 of the key's scale,
 * then octave marks (' up, , down). Octave 0 is the octave whose degree 1 is the tonic's pitch class in MIDI 60 to 71.
 */
static bool parse_degree_pitch(const char *text, const AnalysisKey *key, int *midi, char *message, size_t message_size) {
    const char *cursor = text;
    int alteration = 0;
    if (*cursor == '#' || *cursor == 'b') {
        alteration = *cursor == '#' ? 1 : -1;
        cursor++;
    }
    if (*cursor < '1' || *cursor > '7') {
        snprintf(message, message_size, "'%s' is not a scale degree; use 1 to 7, with an optional # or b before it", text);
        return false;
    }
    int degree = *cursor++ - '0';
    long octave = 0;
    for (; *cursor == '\'' || *cursor == ','; cursor++) {
        octave += *cursor == '\'' ? 1 : -1;
    }
    if (*cursor != '\0') {
        snprintf(message, message_size, "'%s' is not a scale degree: '%s' is not an octave mark (' up, , down)", text,
                 cursor);
        return false;
    }
    long value = 60 + motive_position_pitch(key, octave * 7 + (degree - 1), alteration);
    if (value < 0 || value > 127) {
        snprintf(message, message_size, "'%s' is outside the MIDI range (C-1 to G9)", text);
        return false;
    }
    *midi = (int)value;
    return true;
}

/* A rhythm-only token's pitch is "x"; it sounds on degree 1 of the key (the tonic, in MIDI 60 to 71). */
static bool parse_rhythm_pitch(const char *text, const AnalysisKey *key, int *midi, char *message, size_t message_size) {
    if (strcmp(text, "x") != 0 && strcmp(text, "X") != 0) {
        snprintf(message, message_size, "'%s' is not a note of a rhythm; write x for a note or r for a rest", text);
        return false;
    }
    *midi = 60 + motive_position_pitch(key, 0, 0);
    return true;
}

/* Parses "q", "e.", "h.." into a length in whole notes. On failure writes the reason to message. */
static bool parse_duration(const char *text, Rational *out, char *message, size_t message_size) {
    Rational base;
    switch (text[0]) {
    case 'w':
        base = motive_time(1, 1);
        break;
    case 'h':
        base = motive_time(1, 2);
        break;
    case 'q':
        base = motive_time(1, 4);
        break;
    case 'e':
        base = motive_time(1, 8);
        break;
    case 's':
        base = motive_time(1, 16);
        break;
    case 't':
        base = motive_time(1, 32);
        break;
    default:
        snprintf(message, message_size, "'%s' is not a duration; use w, h, q, e, s or t, then up to two dots", text);
        return false;
    }
    size_t dots = strlen(text + 1);
    for (size_t i = 1; i <= dots; i++) {
        if (text[i] != '.') {
            snprintf(message, message_size, "'%s' is not a duration; only dots may follow the letter", text);
            return false;
        }
    }
    if (dots > 2) {
        snprintf(message, message_size, "'%s' has more than two dots", text);
        return false;
    }
    /* A dot adds half of what it follows: 3/2, then 7/4, of the base. */
    Rational total = base;
    Rational added = base;
    for (size_t i = 0; i < dots; i++) {
        added = motive_time(added.num, added.den * 2);
        total = motive_time_plus(total, added);
    }
    *out = total;
    return true;
}

static MotiveResult parse_list(const char *text, PitchParser parse_pitch_text, const AnalysisKey *key, const char *example,
                               Motive *motive, MotiveError *error) {
    Rational cursor = motive_time(0, 1);
    size_t count = 0;
    bool tied = false;          /* the last note is tied to the next token */
    size_t tie_number = 0;
    bool last_was_rest = false;
    Token last_rest = {{0}, 0, 0};
    size_t number = 0;

    for (size_t at = 0; text[at];) {
        if (isspace((unsigned char)text[at]) || text[at] == '|') {
            at++;
            continue;
        }
        Token token;
        token.number = ++number;
        token.column = at + 1;
        size_t length = 0;
        while (text[at] && !isspace((unsigned char)text[at]) && text[at] != '|') {
            if (length + 1 >= TOKEN_MAX) {
                char location[MOTIVE_ERROR_LOCATION_MAX];
                token.text[length] = '\0';
                token_location(&token, location, sizeof(location));
                motive_set_error(error, location, "the token is longer than %d characters", TOKEN_MAX - 1);
                return MOTIVE_ERR_SYNTAX;
            }
            token.text[length++] = text[at++];
        }
        token.text[length] = '\0';

        char location[MOTIVE_ERROR_LOCATION_MAX];
        char message[MOTIVE_ERROR_MESSAGE_MAX];
        token_location(&token, location, sizeof(location));

        /* Split "PITCH:DURATION[~]". */
        char *colon = strchr(token.text, ':');
        if (!colon || colon == token.text || colon[1] == '\0') {
            motive_set_error(error, location, "'%s' must be a pitch, a colon and a duration, as in %s", token.text, example);
            return MOTIVE_ERR_SYNTAX;
        }
        *colon = '\0';
        const char *pitch_text = token.text;
        char *duration_text = colon + 1;
        size_t duration_length = strlen(duration_text);
        bool tie = duration_text[duration_length - 1] == '~';
        if (tie) {
            duration_text[--duration_length] = '\0';
            if (duration_length == 0) {
                motive_set_error(error, location, "the tie mark needs a duration before it");
                return MOTIVE_ERR_SYNTAX;
            }
        }
        bool rest = (pitch_text[0] == 'r' || pitch_text[0] == 'R') && pitch_text[1] == '\0';
        int midi = 0;
        Rational duration;
        if ((!rest && !parse_pitch_text(pitch_text, key, &midi, message, sizeof(message))) ||
            !parse_duration(duration_text, &duration, message, sizeof(message))) {
            motive_set_error(error, location, "%s", message);
            return MOTIVE_ERR_VALUE;
        }
        if (rest && tie) {
            motive_set_error(error, location, "a rest cannot be tied");
            return MOTIVE_ERR_VALUE;
        }

        if (tied) {
            MotiveSketchNote *previous = &motive->notes[count - 1];
            if (rest || previous->pitch != midi) {
                motive_set_error(error, location, "token %zu is tied to a note of a different pitch; a tie joins "
                                                  "two notes of one pitch",
                                 tie_number);
                return MOTIVE_ERR_VALUE;
            }
            previous->duration = motive_time_plus(previous->duration, duration);
            cursor = motive_time_plus(cursor, duration);
            tied = tie;
            tie_number = token.number;
            last_was_rest = false;
            continue;
        }
        if (rest) {
            cursor = motive_time_plus(cursor, duration);
            last_was_rest = true;
            last_rest = token;
            continue;
        }
        if (count == MOTIVE_MAX_NOTES) {
            motive_set_error(error, location, "the motive has more than %d notes", MOTIVE_MAX_NOTES);
            return MOTIVE_ERR_TOO_MANY_NOTES;
        }
        MotiveSketchNote *note = &motive->notes[count++];
        note->pitch = midi;
        note->onset = cursor;
        note->duration = duration;
        cursor = motive_time_plus(cursor, duration);
        tied = tie;
        tie_number = token.number;
        last_was_rest = false;
    }

    if (tied) {
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "token %zu", tie_number);
        motive_set_error(error, location, "the tie has no note after it");
        return MOTIVE_ERR_VALUE;
    }
    if (count == 0) {
        motive_set_error(error, "", "the motive has no notes");
        return MOTIVE_ERR_EMPTY;
    }
    if (last_was_rest) {
        char location[MOTIVE_ERROR_LOCATION_MAX];
        token_location(&last_rest, location, sizeof(location));
        motive_set_error(error, location, "the motive ends with a rest; it ends where its last note ends");
        return MOTIVE_ERR_VALUE;
    }
    motive->note_count = count;
    return MOTIVE_OK;
}

MotiveResult motive_parse_notes_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error) {
    (void)document;
    return parse_list(text, parse_note_pitch, NULL, "D4:q", motive, error);
}

MotiveResult motive_parse_degrees_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error) {
    AnalysisKey key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    if (document->has_key) {
        key = document->key;
    }
    return parse_list(text, parse_degree_pitch, &key, "5':q", motive, error);
}

MotiveResult motive_parse_rhythm_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error) {
    AnalysisKey key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    if (document->has_key) {
        key = document->key;
    }
    MotiveResult result = parse_list(text, parse_rhythm_pitch, &key, "x:q", motive, error);
    document->form = MOTIVE_SKETCH_FORM_RHYTHM;
    return result;
}

/* A contour is ranks separated by white space, each a whole number; every note is a quarter, rank r on degree r + 1. */
MotiveResult motive_parse_contour_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error) {
    AnalysisKey key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    if (document->has_key) {
        key = document->key;
    }
    size_t count = 0;
    size_t number = 0;
    for (size_t at = 0; text[at];) {
        if (isspace((unsigned char)text[at]) || text[at] == '|') {
            at++;
            continue;
        }
        Token token;
        token.number = ++number;
        token.column = at + 1;
        size_t length = 0;
        while (text[at] && !isspace((unsigned char)text[at]) && text[at] != '|') {
            if (length + 1 >= TOKEN_MAX) {
                char location[MOTIVE_ERROR_LOCATION_MAX];
                token.text[length] = '\0';
                token_location(&token, location, sizeof(location));
                motive_set_error(error, location, "the token is longer than %d characters", TOKEN_MAX - 1);
                return MOTIVE_ERR_SYNTAX;
            }
            token.text[length++] = text[at++];
        }
        token.text[length] = '\0';

        char location[MOTIVE_ERROR_LOCATION_MAX];
        token_location(&token, location, sizeof(location));
        for (size_t i = 0; i < length; i++) {
            if (!isdigit((unsigned char)token.text[i])) {
                motive_set_error(error, location, "'%s' is not a rank; write whole numbers, 0 the lowest, as in 0 2 1 3",
                                 token.text);
                return MOTIVE_ERR_SYNTAX;
            }
        }
        if (length > 3) {
            motive_set_error(error, location, "the rank '%s' is too high; the most is %d", token.text,
                             MOTIVE_MAX_NOTES - 1);
            return MOTIVE_ERR_VALUE;
        }
        long rank = strtol(token.text, NULL, 10);
        long pitch = 60 + motive_position_pitch(&key, rank, 0);
        if (rank > MOTIVE_MAX_NOTES - 1 || pitch > 127) {
            motive_set_error(error, location, "the rank %ld is too high; the most is %d", rank, MOTIVE_MAX_NOTES - 1);
            return MOTIVE_ERR_VALUE;
        }
        if (count == MOTIVE_MAX_NOTES) {
            motive_set_error(error, location, "the motive has more than %d notes", MOTIVE_MAX_NOTES);
            return MOTIVE_ERR_TOO_MANY_NOTES;
        }
        MotiveSketchNote *note = &motive->notes[count];
        note->pitch = (int)pitch;
        note->onset = motive_time((long)count, 4);
        note->duration = motive_time(1, 4);
        count++;
    }
    if (count == 0) {
        motive_set_error(error, "", "the motive has no notes");
        return MOTIVE_ERR_EMPTY;
    }
    motive->note_count = count;
    document->form = MOTIVE_SKETCH_FORM_CONTOUR;
    return MOTIVE_OK;
}
