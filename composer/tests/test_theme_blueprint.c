#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"

/*
 * Theme blueprints (R-743, docs/blueprints.md): every refusal of a blueprint with its path and message; a blueprint that gives
 * every bar's melody, bass and chords renders exactly those notes; one of rhythms, jobs, anchors and a climax composes on seed 1
 * with the anchors held, a running bar on a K11 running cell, a motif bar on the basic idea's rhythm and the climax the highest note.
 * Everything is a C major period of eight bars in 4/4.
 */

#define BARS 8

/* The two-voice period written out: the chords, the melody and the bass of each bar. */
static const char *const FULL[BARS][3] = {
    {"I", "E5:q G5:q E5:q C5:q", "C3:h E3:h"},       {"IV", "F5:q A5:q F5:h", "F2:h A2:h"},
    {"II", "D5:q F5:q A5:q F5:q", "D3:h F3:h"},      {"V", "G5:h D5:h", "G2:w"},
    {"I", "E5:q G5:q E5:q C5:q", "C3:h E3:h"},       {"IV", "F5:q A5:q F5:h", "F2:h A2:h"},
    {"II6 V@2", "D5:q F5:q D5:q B4:q", "F2:h G2:h"}, {"I", "C5:e D5:e E5:q r:h", "C3:w"},
};

/* The skeleton: rhythms, jobs, an anchor a bar in the antecedent's basic idea, and the climax in bar 6. */
static const char *const SKELETON =
    "{\"schema\": \"ursatz-theme-request\", \"schema_version\": 2, \"into\": \"period\", \"key\": \"C\", \"material\": \"major\","
    " \"meter\": \"4/4\", \"bars\": 8, \"cadence\": \"PAC\", \"seed\": 1, \"blueprint\": {\"bars\": ["
    "{\"chords\": \"I\", \"rhythm\": \"x:q x:q x:h\", \"anchors\": [{\"at\": \"0\", \"pitch\": \"E5\"}]},"
    "{\"chords\": \"V7\", \"rhythm\": \"x:q. x:e x:h\", \"anchors\": [{\"at\": \"2\", \"pitch\": \"D5\"}]},"
    "{\"chords\": \"IV\", \"job\": \"running\"},"
    "{\"chords\": \"V\", \"job\": \"cadence\"},"
    "{\"chords\": \"I\"},"
    "{\"chords\": \"V7\"},"
    "{\"chords\": \"II6 V7@2\", \"job\": \"motif\"},"
    "{\"chords\": \"I\", \"rhythm\": \"x:w\"}],"
    " \"climax\": {\"bar\": 6, \"at\": \"0\", \"pitch\": \"A5\"}}}";

typedef struct {
    int midi;
    long num; /* the onset in the theme, num / den whole notes */
    long den;
    long dnum; /* the duration */
    long dden;
} Note;

#define NOTES_MAX 256

/* --- requests ------------------------------------------------------------------------------------- */

static cJSON *skeleton(void) {
    cJSON *request = cJSON_Parse(SKELETON);
    assert(request);
    return request;
}

/* The full period: every bar's melody, bass and chords. */
static cJSON *full(void) {
    cJSON *request = skeleton();
    cJSON *blueprint = cJSON_GetObjectItemCaseSensitive(request, "blueprint");
    cJSON_DeleteItemFromObjectCaseSensitive(blueprint, "climax");
    cJSON *bars = cJSON_CreateArray();
    for (size_t bar = 0; bar < BARS; bar++) {
        cJSON *item = cJSON_CreateObject();
        assert(cJSON_AddStringToObject(item, "chords", FULL[bar][0]) && cJSON_AddStringToObject(item, "melody", FULL[bar][1]) &&
               cJSON_AddStringToObject(item, "bass", FULL[bar][2]));
        assert(cJSON_AddItemToArray(bars, item));
    }
    cJSON_ReplaceItemInObjectCaseSensitive(blueprint, "bars", bars);
    return request;
}

static cJSON *bar_of(cJSON *request, size_t bar) {
    cJSON *bars = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "blueprint"), "bars");
    cJSON *item = cJSON_GetArrayItem(bars, (int)bar);
    assert(item);
    return item;
}

/* Sets (or, with NULL, removes) a member of an object to a JSON text. */
static void set(cJSON *object, const char *name, const char *json) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    if (json) {
        cJSON *item = cJSON_Parse(json);
        assert(item && cJSON_AddItemToObject(object, name, item));
    }
}

static UrsatzStatus run(cJSON *request, ComposerResponse *response) {
    char *text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    UrsatzStatus status = composer_api_theme(test_context(), text, NULL, response);
    free(text);
    return status;
}

static void expect_refused(cJSON *request, const char *path, const char *message) {
    ComposerResponse response;
    UrsatzStatus status = run(request, &response);
    if (status == URSATZ_OK || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "%s '%s' at '%s', expected '%s' at '%s'\n", test_status_name(status), response.error.message,
                response.error.path, message, path);
    }
    assert(status != URSATZ_OK);
    assert(strcmp(response.error.path, path) == 0);
    assert(strstr(response.error.message, message));
    composer_response_destroy(&response);
}

/* --- the notes of a plan -------------------------------------------------------------------------- */

static long fraction_part(const cJSON *fraction, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(fraction, name);
    assert(cJSON_IsNumber(item));
    return (long)item->valuedouble;
}

static int by_onset(const void *a, const void *b) {
    const Note *x = a;
    const Note *y = b;
    long left = x->num * y->den;
    long right = y->num * x->den;
    return left < right ? -1 : left > right;
}

/* The notes of a lane (voice 0) in the theme's eight bars, in time order. */
static size_t lane_notes(const char *plan_json, const char *lane, Note *out) {
    cJSON *plan = cJSON_Parse(plan_json);
    size_t count = 0;
    assert(plan);
    static const char *const GROUPS[] = {"melody", "texture"};
    for (size_t g = 0; g < 2; g++) {
        const cJSON *span = NULL;
        cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, GROUPS[g])) {
            const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
            const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "span"), "start");
            const cJSON *voice = cJSON_GetObjectItemCaseSensitive(composer, "voice");
            long num = fraction_part(start, "num");
            long den = fraction_part(start, "den");
            if (strcmp(cJSON_GetObjectItemCaseSensitive(composer, "lane")->valuestring, lane) != 0 || voice->valuedouble != 0 ||
                num >= BARS * den) {
                continue;
            }
            const cJSON *note = NULL;
            cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "notes")) {
                const cJSON *offset = cJSON_GetObjectItemCaseSensitive(note, "offset");
                const cJSON *duration = cJSON_GetObjectItemCaseSensitive(note, "duration");
                long onum = fraction_part(offset, "num");
                long oden = fraction_part(offset, "den");
                assert(count < NOTES_MAX);
                out[count].midi = (int)cJSON_GetObjectItemCaseSensitive(note, "midi_pitch")->valuedouble;
                out[count].num = num * oden + onum * den;
                out[count].den = den * oden;
                out[count].dnum = fraction_part(duration, "num");
                out[count].dden = fraction_part(duration, "den");
                count++;
            }
        }
    }
    cJSON_Delete(plan);
    qsort(out, count, sizeof(Note), by_onset);
    return count;
}

/* The note starting at bar + num / den whole notes, or NULL. */
static const Note *note_at(const Note *notes, size_t count, size_t bar, long num, long den) {
    for (size_t i = 0; i < count; i++) {
        if (notes[i].num * den == ((long)bar * den + num) * notes[i].den) {
            return &notes[i];
        }
    }
    return NULL;
}

/* The onsets of a bar's notes in sixteenths from the bar's start, into out; their count. */
static size_t bar_onsets(const Note *notes, size_t count, size_t bar, long *out) {
    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        long sixteenths = notes[i].num * 16 / notes[i].den;
        if (notes[i].num * 16 % notes[i].den == 0 && sixteenths >= (long)bar * 16 && sixteenths < ((long)bar + 1) * 16) {
            out[found++] = sixteenths - (long)bar * 16;
        }
    }
    return found;
}

/* The onsets of a rhythm text of one bar, in sixteenths. */
static size_t rhythm_onsets(const char *text, long *out) {
    size_t found = 0;
    long at = 0;
    char copy[128];
    snprintf(copy, sizeof copy, "%s", text);
    for (char *token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        char *colon = strchr(token, ':');
        assert(colon);
        long length = colon[1] == 'w' ? 16 : colon[1] == 'h' ? 8 : colon[1] == 'q' ? 4 : colon[1] == 'e' ? 2 : 1;
        if (colon[2] == '.') {
            length += length / 2;
        }
        if (token[0] == 'x') {
            out[found++] = at;
        }
        at += length;
    }
    return found;
}

/* --- the refusals --------------------------------------------------------------------------------- */

static void test_refusals(void) {
    cJSON *request;
    /* Members the blueprint gives itself. */
    request = skeleton();
    set(request, "chords", "[{\"bar\": 0, \"text\": \"I\"}]");
    expect_refused(request, "/chords", "a blueprint gives its chords per bar");
    request = skeleton();
    set(request, "accompaniment", "{\"pattern\": \"block\"}");
    expect_refused(request, "/accompaniment", "a blueprint gives its accompaniment per bar");
    request = skeleton();
    set(request, "ideas", "{\"contrasting idea\": \"a\"}");
    expect_refused(request, "/ideas", "a blueprint gives its idea motives per bar");
    /* The bars. */
    request = skeleton();
    cJSON_DeleteItemFromArray(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "blueprint"), "bars"), 7);
    expect_refused(request, "/blueprint/bars", "the blueprint has 7 bars; the theme has 8");
    /* One of melody, rhythm and job a bar. */
    request = skeleton();
    set(bar_of(request, 0), "melody", "\"E5:w\"");
    expect_refused(request, "/blueprint/bars/0/rhythm", "bar 0 gives a melody and a rhythm");
    request = skeleton();
    set(bar_of(request, 2), "melody", "\"E5:w\"");
    expect_refused(request, "/blueprint/bars/2/job", "bar 2 gives a melody and a job");
    request = skeleton();
    set(bar_of(request, 2), "rhythm", "\"x:w\"");
    expect_refused(request, "/blueprint/bars/2/job", "bar 2 gives a rhythm and a job");
    /* Anchors and events go on a composed melody. */
    request = full();
    set(bar_of(request, 1), "anchors", "[{\"at\": \"0\", \"pitch\": \"F5\"}]");
    expect_refused(request, "/blueprint/bars/1/anchors", "bar 1 gives its melody: anchors and events");
    request = full();
    set(bar_of(request, 1), "events", "[{\"at\": \"0\", \"type\": \"passing\"}]");
    expect_refused(request, "/blueprint/bars/1/events", "bar 1 gives its melody: anchors and events");
    /* Jobs: a word or a K11 rhythm id of the meter. */
    request = skeleton();
    set(bar_of(request, 2), "job", "\"walking\"");
    expect_refused(request, "/blueprint/bars/2/job", "'walking' is not a rhythm job");
    request = skeleton();
    set(bar_of(request, 2), "job", "\"michelle.pattern.run-68\"");
    expect_refused(request, "/blueprint/bars/2/job", "the rhythm michelle.pattern.run-68 is in 6/8, not 4/4");
    request = skeleton();
    set(bar_of(request, 0), "job", "\"motif\"");
    set(bar_of(request, 0), "rhythm", NULL);
    set(bar_of(request, 1), "job", "\"motif\"");
    set(bar_of(request, 1), "rhythm", NULL);
    expect_refused(request, "/blueprint/bars/0/job", "the job motif is made from the basic idea's rhythm");
    /* An idea's bars give a melody, a rhythm or a job in all of them or none. */
    request = full();
    set(bar_of(request, 3), "melody", NULL);
    expect_refused(request, "/blueprint/bars/3", "bars 2 and 3 are one idea: give a melody in both or neither");
    request = skeleton();
    set(bar_of(request, 3), "job", NULL);
    expect_refused(request, "/blueprint/bars/3", "bars 2 and 3 are one idea: give a rhythm or a job in both or neither");
    /* The basic idea: its own bars, or slots.basic and then none of them. */
    request = skeleton();
    set(request, "motives", "[{\"id\": \"a\", \"degrees\": \"1:q 3:q 5:h\"}]");
    set(request, "slots", "{\"basic\": \"a\"}");
    expect_refused(request, "/blueprint/bars/0/rhythm", "slots.basic gives the basic idea");
    request = skeleton();
    for (size_t bar = 0; bar < 2; bar++) {
        set(bar_of(request, bar), "rhythm", NULL);
        set(bar_of(request, bar), "anchors", NULL);
    }
    expect_refused(request, "/blueprint/bars/0", "the basic idea needs a melody, a rhythm or a job in its bars, or slots.basic");
    request = skeleton();
    set(request, "motives", "[{\"id\": \"a\", \"degrees\": \"1:q 3:q 5:h\"}]");
    set(request, "slots", "{\"basic\": \"a\", \"contrasting\": \"a\"}");
    expect_refused(request, "/slots/contrasting", "only slots.basic is read beside it");
    /* Bass and inner voices: every bar or none, as many voices, a rest only under a given melody, no inner voices with an accompaniment. */
    request = full();
    set(bar_of(request, 5), "bass", NULL);
    expect_refused(request, "/blueprint/bars/5", "a blueprint gives a bass in every bar or in none: bar 5 has none");
    request = full();
    for (size_t bar = 0; bar < BARS; bar++) {
        set(bar_of(request, bar), "inner", bar == 4 ? "[\"G4:w\", \"C4:w\"]" : "[\"G4:w\"]");
    }
    expect_refused(request, "/blueprint/bars/4/inner", "bar 4 has 2 inner voices and bar 0 has 1");
    request = skeleton();
    for (size_t bar = 0; bar < BARS; bar++) {
        set(bar_of(request, bar), "bass", bar == 2 ? "\"F2:h r:h\"" : "\"C3:w\"");
    }
    expect_refused(request, "/blueprint/bars/2/bass", "bar 2's bass rests and its melody is composed");
    request = full();
    for (size_t bar = 0; bar < BARS; bar++) {
        set(bar_of(request, bar), "inner", "[\"G4:w\"]");
    }
    set(cJSON_GetObjectItemCaseSensitive(request, "blueprint"), "accompaniment", "{\"pattern\": \"block\"}");
    expect_refused(request, "/blueprint/bars/0/inner", "the accompaniment composes the inner voices");
    /* Note lists fill their bar. */
    request = skeleton();
    set(bar_of(request, 7), "rhythm", "\"x:h\"");
    expect_refused(request, "/blueprint/bars/7/rhythm", "bar 7 rhythm: the notes fill 2 beats; a bar has 4");
    /* Refused when the plan is built: an anchor on no onset, the climax under a given note. */
    request = skeleton();
    set(bar_of(request, 1), "anchors", "[{\"at\": \"1\", \"pitch\": \"D5\"}]");
    expect_refused(request, "/blueprint/bars/1/anchors/0/at", "bar 1 anchor at 1: no note starts there");
    request = skeleton();
    set(bar_of(request, 5), "events", "[{\"at\": \"1\", \"type\": \"passing\"}]");
    expect_refused(request, "/blueprint/bars/5/events/0/at", "bar 5 event at 1: no note starts there");
    request = full();
    set(cJSON_GetObjectItemCaseSensitive(request, "blueprint"), "climax", "{\"bar\": 3, \"at\": \"0\", \"pitch\": \"G5\"}");
    expect_refused(request, "/blueprint/climax/pitch", "the climax G5 is not the theme's highest note: bar 0 has G5");
}

/* --- what a blueprint makes ----------------------------------------------------------------------- */

/* Every note of the written period, melody and bass, and nothing else in the theme. */
static void test_full(void) {
    ComposerResponse response;
    UrsatzStatus status = run(full(), &response);
    if (status != URSATZ_OK) {
        fprintf(stderr, "full: %s at %s\n", response.error.message, response.error.path);
    }
    assert(status == URSATZ_OK && response.plan_json);
    static Note melody[NOTES_MAX];
    static Note bass[NOTES_MAX];
    size_t melody_count = lane_notes(response.plan_json, "melody", melody);
    size_t bass_count = lane_notes(response.plan_json, "bass", bass);
    static const int MELODY[] = {76, 79, 76, 72, 77, 81, 77, 74, 77, 81, 77, 79, 74, 76, 79, 76, 72, 77, 81, 77, 74, 77, 74, 71, 72, 74, 76};
    static const int BASS[] = {48, 52, 41, 45, 50, 53, 43, 48, 52, 41, 45, 41, 43, 48};
    assert(melody_count == sizeof(MELODY) / sizeof(MELODY[0]));
    assert(bass_count == sizeof(BASS) / sizeof(BASS[0]));
    for (size_t i = 0; i < melody_count; i++) {
        assert(melody[i].midi == MELODY[i]);
    }
    for (size_t i = 0; i < bass_count; i++) {
        assert(bass[i].midi == BASS[i]);
    }
    /* The rhythm too: bar 3's two halves, and the last bar's eighths and the rest after its quarter. */
    const Note *last = note_at(melody, melody_count, 7, 1, 4);
    assert(note_at(melody, melody_count, 3, 1, 2) && last && last->dnum * 4 == last->dden);
    assert(melody[melody_count - 1].midi == 76);
    composer_response_destroy(&response);
}

/* The skeleton on seed 1: the anchors held, a running cell, the motif's rhythm, the climax highest. */
static void test_skeleton(void) {
    ComposerResponse response;
    UrsatzStatus status = run(skeleton(), &response);
    if (status != URSATZ_OK) {
        fprintf(stderr, "skeleton: %s at %s\n", response.error.message, response.error.path);
    }
    assert(status == URSATZ_OK && response.plan_json);
    static Note melody[NOTES_MAX];
    size_t count = lane_notes(response.plan_json, "melody", melody);
    /* The anchors: E5 on bar 0's downbeat, D5 on bar 1's third beat. */
    const Note *anchor = note_at(melody, count, 0, 0, 1);
    assert(anchor && anchor->midi == 76);
    anchor = note_at(melody, count, 1, 1, 2);
    assert(anchor && anchor->midi == 74);
    /* The rhythms given: bar 0's and bar 7's. */
    long onsets[32];
    long want[32];
    size_t found = bar_onsets(melody, count, 0, onsets);
    assert(found == rhythm_onsets("x:q x:q x:h", want) && memcmp(onsets, want, found * sizeof(long)) == 0);
    assert(bar_onsets(melody, count, 7, onsets) == 1 && onsets[0] == 0);
    /* The running bar is one of the 4/4 running cells of K11. */
    static const char *const RUNNING[] = {"x:e x:e x:e x:e x:q. x:e", "x:e x:e x:q x:e x:e x:q", "x:e x:e x:e x:e x:e x:e x:q"};
    found = bar_onsets(melody, count, 2, onsets);
    bool running = false;
    for (size_t r = 0; r < 3; r++) {
        running = running || (found == rhythm_onsets(RUNNING[r], want) && memcmp(onsets, want, found * sizeof(long)) == 0);
    }
    assert(running);
    /* The motif bar (the consequent's contrasting idea, at its first bar) is the basic idea's first bar's rhythm. */
    found = bar_onsets(melody, count, 6, onsets);
    assert(found == rhythm_onsets("x:q x:q x:h", want) && memcmp(onsets, want, found * sizeof(long)) == 0);
    /* The climax: A5 on bar 6's downbeat, and no other note as high. */
    const Note *climax = note_at(melody, count, 6, 0, 1);
    assert(climax && climax->midi == 81);
    for (size_t i = 0; i < count; i++) {
        assert(&melody[i] == climax || melody[i].midi < 81);
    }
    composer_response_destroy(&response);
}

int main(void) {
    test_refusals();
    test_full();
    test_skeleton();
    printf("test_theme_blueprint: ok\n");
    return 0;
}
