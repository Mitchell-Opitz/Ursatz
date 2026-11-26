#include "support/test_blueprint_check.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/harmony_phrase.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"

/* Moved unchanged from test_reference_skeletons.c (P69c B8): reading a composed theme and the checks of the fixed items of a blueprint. */




/* --- reading ---------------------------------------------------------------------------------------- */

static long ticks_of(const cJSON *fraction) {
    const cJSON *num = cJSON_GetObjectItemCaseSensitive(fraction, "num");
    const cJSON *den = cJSON_GetObjectItemCaseSensitive(fraction, "den");
    assert(cJSON_IsNumber(num) && cJSON_IsNumber(den) && den->valuedouble > 0);
    long n = (long)num->valuedouble * WHOLE;
    long d = (long)den->valuedouble;
    assert(n % d == 0);
    return n / d;
}

/* "n" or "n/d" beats of the meter's unit, in ticks. */
static long beats_of(const char *text, long beat) {
    const char *slash = strchr(text, '/');
    long num = strtol(text, NULL, 10);
    long den = slash ? strtol(slash + 1, NULL, 10) : 1;
    assert(den > 0 && (num * beat) % den == 0);
    return num * beat / den;
}

static int pitch_class_of(const char *name, const char **after) {
    static const int LETTERS[] = {9, 11, 0, 2, 4, 5, 7}; /* A to G */
    char letter = (char)toupper((unsigned char)name[0]);
    assert(letter >= 'A' && letter <= 'G');
    int pc = LETTERS[letter - 'A'];
    const char *at = name + 1;
    for (; *at == '#' || *at == 'b'; at++) {
        pc += *at == '#' ? 1 : -1;
    }
    *after = at;
    return (pc % 12 + 12) % 12;
}

/* "C5", "F#4", "Bb5": the MIDI pitch (C4 is 60). */
static int midi_of(const char *name) {
    const char *octave;
    int pc = pitch_class_of(name, &octave);
    char *end;
    long value = strtol(octave, &end, 10);
    assert(end != octave && *end == '\0');
    /* A flat or sharp across C keeps the written octave: Cb5 is B4, B#4 is C5. */
    int letter = toupper((unsigned char)name[0]);
    int natural = pitch_class_of((char[]){(char)letter, '\0'}, &octave);
    int shift = pc - natural;
    shift = shift > 6 ? shift - 12 : shift < -6 ? shift + 12 : shift;
    return (int)(value + 1) * 12 + natural + shift;
}

/* Lower case, without the quality signs (o, the degree sign) and with maj7 read as the plan writes it (7). */
static void normalized(const char *label, char *out, size_t size) {
    size_t length = 0;
    for (const char *at = label; *at && length + 1 < size; at++) {
        if (strncmp(at, "maj", 3) == 0 && (at[3] == '7' || at[3] == '9')) {
            at += 2;
            continue;
        }
        if (*at == 'o' || *at == '\xc2' || *at == '\xb0') {
            continue;
        }
        out[length++] = (char)tolower((unsigned char)*at);
    }
    out[length] = '\0';
}

void test_blueprint_read_theme(const char *plan_json, const cJSON *request, Theme *theme) {
    memset(theme, 0, sizeof(*theme));
    const char *meter = cJSON_GetObjectItemCaseSensitive(request, "meter")->valuestring;
    long beats = strtol(meter, NULL, 10);
    long unit = strtol(strchr(meter, '/') + 1, NULL, 10);
    theme->beat = WHOLE / unit;
    theme->bar = beats * theme->beat;
    theme->bars = (long)cJSON_GetObjectItemCaseSensitive(request, "bars")->valuedouble;
    const char *after;
    int tonic = pitch_class_of(cJSON_GetObjectItemCaseSensitive(request, "key")->valuestring, &after);
    theme->key = test_key(tonic, cJSON_GetObjectItemCaseSensitive(request, "material")->valuestring);
    long end = theme->bars * theme->bar;
    cJSON *plan = cJSON_Parse(plan_json);
    assert(plan);
    const cJSON *span = NULL;
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "melody")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
        long start = ticks_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "span"), "start"));
        if (strcmp(cJSON_GetObjectItemCaseSensitive(composer, "lane")->valuestring, "melody") != 0 ||
            cJSON_GetObjectItemCaseSensitive(composer, "voice")->valuedouble != 0 || start >= end) {
            continue;
        }
        const cJSON *note = NULL;
        cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "notes")) {
            assert(theme->note_count < NOTES_MAX);
            Note *out = &theme->notes[theme->note_count++];
            out->onset = start + ticks_of(cJSON_GetObjectItemCaseSensitive(note, "offset"));
            out->duration = ticks_of(cJSON_GetObjectItemCaseSensitive(note, "duration"));
            out->pitch = (int)cJSON_GetObjectItemCaseSensitive(note, "midi_pitch")->valuedouble;
        }
    }
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "chords")) {
        const cJSON *times = cJSON_GetObjectItemCaseSensitive(span, "span");
        long start = ticks_of(cJSON_GetObjectItemCaseSensitive(times, "start"));
        if (start >= end) {
            continue;
        }
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(composer, "state")->valuestring, "given") == 0);
        assert(theme->chord_count < CHORDS_MAX);
        Chord *out = &theme->chords[theme->chord_count++];
        out->onset = start;
        out->end = ticks_of(cJSON_GetObjectItemCaseSensitive(times, "end"));
        snprintf(out->label, sizeof out->label, "%s",
                 cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "roman_numeral")->valuestring);
    }
    cJSON_Delete(plan);
    /* In time order (insertion sort: a theme has a few hundred notes at most). */
    for (size_t i = 1; i < theme->note_count; i++) {
        for (size_t j = i; j > 0 && theme->notes[j - 1].onset > theme->notes[j].onset; j--) {
            Note swap = theme->notes[j];
            theme->notes[j] = theme->notes[j - 1];
            theme->notes[j - 1] = swap;
        }
    }
    for (size_t i = 1; i < theme->note_count; i++) {
        assert(theme->notes[i].onset > theme->notes[i - 1].onset);
    }
}

static long note_at(const Theme *theme, long onset) {
    for (size_t i = 0; i < theme->note_count; i++) {
        if (theme->notes[i].onset == onset) {
            return (long)i;
        }
    }
    return -1;
}

static const Chord *chord_at(const Theme *theme, long onset) {
    for (size_t i = 0; i < theme->chord_count; i++) {
        if (theme->chords[i].onset <= onset && onset < theme->chords[i].end) {
            return &theme->chords[i];
        }
    }
    return NULL;
}

/* --- the fixed items ------------------------------------------------------------------------------- */


#define FAIL_IF(condition, ...)                                                                                                            \
    do {                                                                                                                                   \
        if (condition) {                                                                                                                   \
            fprintf(stderr, "%s seed %u: ", where->name, where->seed);                                                                     \
            fprintf(stderr, __VA_ARGS__);                                                                                                  \
            fprintf(stderr, "\n");                                                                                                         \
            assert(!(condition));                                                                                                          \
        }                                                                                                                                  \
    } while (0)

/* Each chord of the bar's text ("ii6 V7@2") starts at its beat with that numeral, and no other chord starts in the bar. */
static void check_chords(const Where *where, const Theme *theme, long bar, const char *text) {
    char copy[128];
    snprintf(copy, sizeof copy, "%s", text);
    size_t given = 0;
    for (char *token = strtok(copy, " "); token; token = strtok(NULL, " "), given++) {
        char *at = strchr(token, '@');
        long onset = bar * theme->bar;
        if (at) {
            *at = '\0';
            onset += beats_of(at + 1, theme->beat);
        }
        const Chord *chord = NULL;
        for (size_t i = 0; i < theme->chord_count; i++) {
            chord = theme->chords[i].onset == onset ? &theme->chords[i] : chord;
        }
        FAIL_IF(!chord, "bar %ld: no chord starts where %s is given", bar, token);
        char want[32];
        char have[32];
        normalized(token, want, sizeof want);
        normalized(chord->label, have, sizeof have);
        FAIL_IF(strcmp(want, have) != 0, "bar %ld: the chord is %s, the blueprint gives %s", bar, chord->label, token);
    }
    size_t found = 0;
    for (size_t i = 0; i < theme->chord_count; i++) {
        found += theme->chords[i].onset >= bar * theme->bar && theme->chords[i].onset < (bar + 1) * theme->bar;
    }
    FAIL_IF(found != given, "bar %ld: %zu chords start, the blueprint gives %zu", bar, found, given);
}

static long length_of(char value) {
    return value == 'w' ? WHOLE : value == 'h' ? WHOLE / 2 : value == 'q' ? WHOLE / 4 : value == 'e' ? WHOLE / 8 : WHOLE / 16;
}

/* The bar's notes start and last as the rhythm text says ("x:q. x:e r:q", a tie "x:h~ x:e" is one note). */
static void check_rhythm(const Where *where, const Theme *theme, long bar, const char *text) {
    long onsets[ITEMS_MAX];
    long durations[ITEMS_MAX];
    size_t count = 0;
    bool tied = false;
    bool tied_out = false;
    long at = bar * theme->bar;
    char copy[256];
    snprintf(copy, sizeof copy, "%s", text);
    for (char *token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        assert((token[0] == 'x' || token[0] == 'r') && token[1] == ':' && strchr("whqes", token[2]));
        long length = length_of(token[2]);
        const char *rest = token + 3;
        if (*rest == '.') {
            length += length / 2;
            rest++;
        }
        bool tie = *rest == '~';
        if (token[0] == 'x' && tied) {
            durations[count - 1] += length;
        } else if (token[0] == 'x') {
            assert(count < ITEMS_MAX);
            onsets[count] = at;
            durations[count++] = length;
        }
        tied = tie && token[0] == 'x';
        tied_out = tied;
        at += length;
    }
    assert(at == (bar + 1) * theme->bar);
    size_t found = 0;
    for (size_t i = 0; i < theme->note_count; i++) {
        const Note *note = &theme->notes[i];
        if (note->onset < bar * theme->bar || note->onset >= (bar + 1) * theme->bar) {
            continue;
        }
        FAIL_IF(found >= count || note->onset != onsets[found], "bar %ld: a note at tick %ld is not in the rhythm %s", bar,
                note->onset - bar * theme->bar, text);
        bool last = found + 1 == count;
        FAIL_IF(last && tied_out ? note->duration < durations[found] : note->duration != durations[found],
                "bar %ld: the note at tick %ld lasts %ld ticks, the rhythm %s gives %ld", bar, note->onset - bar * theme->bar,
                note->duration, text, durations[found]);
        found++;
    }
    FAIL_IF(found != count, "bar %ld: %zu notes, the rhythm %s has %zu", bar, found, text, count);
}

static void check_anchor(const Where *where, const Theme *theme, long bar, const cJSON *anchor) {
    long onset = bar * theme->bar + beats_of(cJSON_GetObjectItemCaseSensitive(anchor, "at")->valuestring, theme->beat);
    const char *pitch = cJSON_GetObjectItemCaseSensitive(anchor, "pitch")->valuestring;
    long i = note_at(theme, onset);
    FAIL_IF(i < 0, "bar %ld: no note at the anchor (tick %ld)", bar, onset - bar * theme->bar);
    FAIL_IF(theme->notes[i].pitch != midi_of(pitch), "bar %ld: the anchor's note is %d, not %s", bar, theme->notes[i].pitch, pitch);
}

/* The pitch classes of a chord label in the key: as it sounds (composer_chord_tones: a sus chord's fourth or second for its third, a
 * seventh or a ninth chord's members), except that an added ninth or sixth (add9, add6) is a color, not a chord tone: the chord's
 * basic triad (composer_chord_intervals). The references sound their added ninths as appoggiaturas on downbeats. */
static unsigned chord_mask(const Theme *theme, const char *label) {
    const ComposerChords *chords = test_composer_chords();
    HarmonyKind kind = harmony_kind(chords, &theme->key);
    HarmonyChord parsed;
    int intervals[COMPOSER_CHORD_TONES_MAX];
    size_t count = 0;
    int root = 0;
    assert(harmony_roman_parse_in(&kind, label, &parsed));
    int tonic = spelled_key_tonic_pitch_class(&theme->key);
    if (strstr(label, "add")) {
        assert(composer_chord_intervals(chords, kind.mode, kind.modal, &parsed, &root, intervals, &count));
        root += tonic;
    } else {
        assert(composer_chord_tones(chords, &theme->key, &parsed, &root, intervals, &count));
    }
    unsigned mask = 0;
    for (size_t k = 0; k < count; k++) {
        mask |= 1u << ((root + intervals[k]) % 12);
    }
    return mask;
}

static bool is_step(int from, int to) {
    int interval = abs(to - from);
    return interval == 1 || interval == 2;
}

static void check_event(const Where *where, const Theme *theme, long bar, const cJSON *event) {
    long onset = bar * theme->bar + beats_of(cJSON_GetObjectItemCaseSensitive(event, "at")->valuestring, theme->beat);
    const char *type = cJSON_GetObjectItemCaseSensitive(event, "type")->valuestring;
    long i = note_at(theme, onset);
    FAIL_IF(i < 0, "bar %ld: no note at the %s (tick %ld)", bar, type, onset - bar * theme->bar);
    const Note *note = &theme->notes[i];
    const Chord *chord = chord_at(theme, onset);
    assert(chord);
    FAIL_IF((chord_mask(theme, chord->label) >> (note->pitch % 12)) & 1u, "bar %ld: the %s %d is a tone of %s", bar, type, note->pitch,
            chord->label);
    bool has_next = (size_t)i + 1 < theme->note_count;
    int next = has_next ? theme->notes[i + 1].pitch : 0;
    int previous = i > 0 ? theme->notes[i - 1].pitch : -1;
    if (strcmp(type, "anticipation") == 0) {
        /* A tone of the chord that follows, struck again or held into it. */
        const Chord *following = chord_at(theme, chord->end);
        FAIL_IF(!following || !((chord_mask(theme, following->label) >> (note->pitch % 12)) & 1u),
                "bar %ld: the anticipation %d is no tone of the chord after %s", bar, note->pitch, chord->label);
        return;
    }
    if (strcmp(type, "escape") == 0) {
        FAIL_IF(i > 0 && !is_step(previous, note->pitch), "bar %ld: the escape tone %d is no step from %d", bar, note->pitch, previous);
        return;
    }
    if (strcmp(type, "suspension") == 0 || strcmp(type, "retardation") == 0) {
        FAIL_IF(previous != note->pitch, "bar %ld: the %s %d does not keep the note before (%d)", bar, type, note->pitch, previous);
    }
    FAIL_IF(has_next && !is_step(note->pitch, next), "bar %ld: the %s %d does not move by step to %d", bar, type, note->pitch, next);
}

static void check_climax(const Where *where, const Theme *theme, const cJSON *climax) {
    long bar = (long)cJSON_GetObjectItemCaseSensitive(climax, "bar")->valuedouble;
    long onset = bar * theme->bar + beats_of(cJSON_GetObjectItemCaseSensitive(climax, "at")->valuestring, theme->beat);
    int pitch = midi_of(cJSON_GetObjectItemCaseSensitive(climax, "pitch")->valuestring);
    long at = note_at(theme, onset);
    FAIL_IF(at < 0, "no note at the climax");
    FAIL_IF(theme->notes[at].pitch != pitch, "the climax note is %d, not %d", theme->notes[at].pitch, pitch);
    for (size_t i = 0; i < theme->note_count; i++) {
        FAIL_IF((long)i != at && theme->notes[i].pitch >= pitch, "note %d at tick %ld reaches the climax %d", theme->notes[i].pitch,
                theme->notes[i].onset, pitch);
    }
}

/* Every accompaniment constraint of the texture names the given pattern (an entry of K11 by its id), and there is one. */
static void check_accompaniment(const Where *where, const char *plan_json, const char *pattern) {
    cJSON *plan = cJSON_Parse(plan_json);
    size_t found = 0;
    const cJSON *span = NULL;
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "texture")) {
        const cJSON *constraint = NULL;
        cJSON_ArrayForEach(constraint, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "composer"), "constraints")) {
            if (strcmp(cJSON_GetObjectItemCaseSensitive(constraint, "kind")->valuestring, "accompaniment") != 0) {
                continue;
            }
            const char *named = cJSON_GetObjectItemCaseSensitive(constraint, "pattern")->valuestring;
            const cJSON *id = cJSON_GetObjectItemCaseSensitive(constraint, "pattern_id");
            if (strcmp(named, "entry") == 0 && cJSON_IsString(id)) {
                named = id->valuestring;
            }
            FAIL_IF(strcmp(named, pattern) != 0, "the accompaniment is %s, not %s", named, pattern);
            found++;
        }
    }
    cJSON_Delete(plan);
    FAIL_IF(found == 0, "no accompaniment %s in the texture", pattern);
}

void test_blueprint_check_fixed(const Where *where, const char *plan_json, const cJSON *request) {
    static Theme theme;
    test_blueprint_read_theme(plan_json, request, &theme);
    const cJSON *blueprint = cJSON_GetObjectItemCaseSensitive(request, "blueprint");
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(blueprint, "bars");
    assert(cJSON_GetArraySize(bars) == theme.bars);
    for (long bar = 0; bar < theme.bars; bar++) {
        const cJSON *item = cJSON_GetArrayItem(bars, (int)bar);
        check_chords(where, &theme, bar, cJSON_GetObjectItemCaseSensitive(item, "chords")->valuestring);
        const cJSON *rhythm = cJSON_GetObjectItemCaseSensitive(item, "rhythm");
        if (cJSON_IsString(rhythm)) {
            check_rhythm(where, &theme, bar, rhythm->valuestring);
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(item, "anchors")) {
            check_anchor(where, &theme, bar, entry);
        }
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(item, "events")) {
            check_event(where, &theme, bar, entry);
        }
    }
    const cJSON *climax = cJSON_GetObjectItemCaseSensitive(blueprint, "climax");
    if (climax) {
        check_climax(where, &theme, climax);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(blueprint, "accompaniment");
    if (accompaniment) {
        check_accompaniment(where, plan_json, cJSON_GetObjectItemCaseSensitive(accompaniment, "pattern")->valuestring);
    }
}
