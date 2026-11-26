#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The blueprint generator (P69c B3, R-751, R-752): for the four motif fixtures as sentences and periods, seeds 1 to 10, the drawn chords and
 * rhythms are a blueprint the form takes, with no tritone between neighbouring basses, the climax in its bars and every anchor under it
 * and inside the run's range; the weights of the preferences steer the draw (a fragment that can only contrast, a climax that cannot be in
 * bar 5, a sound of weight 0 never heard); through composer_api_theme a sentence with development blueprint composes, says so, returns its
 * blueprint, and that blueprint sent back with the same motif renders the same notes; the requests the generator does not take are refused at
 * /development.
 */

#define SEEDS 10
#define FIXTURES 4

static const char *const FILES[FIXTURES] = {"/continuation/s77-chords.json", "/continuation/u51-chords.json", "/rates/two-bar-3-4.json",
                                            "/motifs/motive-d-minor-arpeggio.json"};
static Motive MOTIVE_OF[FIXTURES];

/* The motif of a fixture in key C, in its mode, as `listen --motif` sends it (R-757): each note keeps its step. */
static Motive load_motive(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Motive motive;
    Motive in_c;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    SpelledKey source = motive_key(&motive);
    SpelledKey key = test_key(0, pitch_material_third(source.material) == 3 ? "minor" : "major");
    assert(motive_in_key(&motive, &key, &in_c));
    return in_c;
}

typedef struct {
    GrowBlueprint *blueprint;
    GrowBlueprintDraw *drawn;
    bool made;
    int low;
    int high;
} Generated;

/* The generator for a fixture, a pairing and a seed, as grow_one runs it; made is false for a seed whose draw is stuck. */
static Generated generate(size_t which, bool period, unsigned long seed, const BlueprintPreferences *preferences, GrowRequest *request) {
    Generated out;
    memset(&out, 0, sizeof out);
    memset(request, 0, sizeof *request);
    request->motive = &MOTIVE_OF[which];
    request->key = motive_key(&MOTIVE_OF[which]);
    request->meter = motive_meter(&MOTIVE_OF[which]);
    request->bars = 8;
    request->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request->first_cadence = CADENCE_TYPE_HALF;
    request->pairing.beginning = period ? SECTION_BEGINNING_ANTECEDENT : SECTION_BEGINNING_PRESENTATION;
    request->pairing.ending = period ? SECTION_ENDING_CONSEQUENT : SECTION_ENDING_CONTINUATION;
    request->knowledge = test_knowledge();
    request->seed = seed;
    out.blueprint = calloc(1, sizeof(*out.blueprint));
    out.drawn = calloc(1, sizeof(*out.drawn));
    assert(out.blueprint && out.drawn);
    FillRng rng;
    GrowRefusal refusal;
    memset(&refusal, 0, sizeof refusal);
    fill_rng_seed(&rng, seed + 11);
    GrowResult result = grow_blueprint_generate(request, preferences, &rng, out.blueprint, &refusal);
    if (result == GROW_ERR_STUCK) {
        return out;
    }
    assert(result == GROW_OK);
    UrsatzError error;
    memset(&error, 0, sizeof error);
    assert(grow_blueprint_check_ideas(out.blueprint, &request->pairing, &request->meter, 8, NULL, true, &error) == URSATZ_OK);
    request->blueprint = out.blueprint;
    result = grow_blueprint_draw(request, out.drawn, &refusal);
    assert(result == GROW_OK);
    request->drawn = out.drawn;
    MelodyFillParams melody = melody_fill_params_default();
    out.low = melody.range_low;
    out.high = melody.range_high;
    result = grow_blueprint_generate_marks(request, preferences, out.drawn, &rng, out.blueprint, &out.low, &out.high, &refusal);
    assert(result == GROW_OK || result == GROW_ERR_STUCK);
    out.made = result == GROW_OK;
    return out;
}

static void release(Generated *generated) {
    free(generated->blueprint);
    free(generated->drawn);
}

/* The text with the suffixes the generator adds taken off and its numerals in upper case, the form the plain own-harmony parse reads. */
static void plain_text(const char *text, char *out, size_t size) {
    static const char *const SUFFIXES[] = {"add9", "maj7", "sus4"};
    size_t used = 0;
    for (const char *c = text; *c && used + 1 < size;) {
        size_t skip = 0;
        for (size_t i = 0; i < 3; i++) {
            skip = strncmp(c, SUFFIXES[i], 4) == 0 ? 4 : skip;
        }
        if (skip) {
            c += skip;
            continue;
        }
        out[used++] = (char)((*c >= 'a' && *c <= 'z' && (c == text || c[-1] == ' ')) ? *c - ('a' - 'A') : *c);
        c++;
    }
    out[used] = '\0';
}

/* The bass pitch class of every chord of the blueprint's texts, in order. */
static size_t basses(const GrowRequest *request, const GrowBlueprint *blueprint, int out[], size_t capacity) {
    Motive keyed;
    size_t count = 0;
    motive_init(&keyed, request->meter);
    motive_set_key(&keyed, &request->key, MOTIVE_SKETCH_KEY_GIVEN);
    for (size_t bar = 0; bar < blueprint->bar_count; bar++) {
        MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
        size_t chord_count = 0;
        MotiveError error;
        char plain[96];
        plain_text(blueprint->bars[bar].chords, plain, sizeof plain);
        assert(motive_harmony_parse(plain, &request->meter, chords, &chord_count, &error) == MOTIVE_OK);
        for (size_t c = 0; c < chord_count && count < capacity; c++) {
            int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
            size_t members = motive_sketch_chord_tones(&keyed, &chords[c], pcs);
            assert(members > (size_t)chords[c].inversion);
            out[count++] = pcs[chords[c].inversion];
        }
    }
    return count;
}

static void test_drawn_blueprints(void) {
    for (size_t which = 0; which < FIXTURES; which++) {
        for (int period = 0; period < 2; period++) {
            unsigned made = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                GrowRequest request;
                Generated g = generate(which, period != 0, seed, NULL, &request);
                if (g.made) {
                    made++;
                    ComposerChords tones;
                    GivenChord given[GROW_EXT_CHORDS];
                    char numerals[GROW_EXT_CHORDS][PLAN_ROMAN_NUMERAL_MAX];
                    size_t given_count = 0;
                    UrsatzError error;
                    memset(&error, 0, sizeof error);
                    assert(composer_chords_init(&tones, test_knowledge(), NULL, 0));
                    UrsatzStatus read = api_blueprint_chords(&tones, g.blueprint, &request.meter, &request.key, 8, given, numerals, GROW_EXT_CHORDS,
                                                             &given_count, &error);
                    if (read != URSATZ_OK) {
                        fprintf(stderr, "fixture %zu seed %lu: %s at %s\n", which, seed, error.message, error.path);
                    }
                    assert(read == URSATZ_OK && given_count >= 8);
                    int bass[64];
                    size_t count = basses(&request, g.blueprint, bass, 64);
                    for (size_t i = 0; i + 1 < count; i++) {
                        assert(((bass[i + 1] - bass[i]) % 12 + 12) % 12 != 6);
                    }
                    assert(g.blueprint->has_climax && g.blueprint->climax_midi == g.high);
                    assert(period ? g.blueprint->climax_bar == 6 : (g.blueprint->climax_bar == 5 || g.blueprint->climax_bar == 6));
                    for (size_t bar = 0; bar < g.blueprint->bar_count; bar++) {
                        for (size_t a = 0; a < g.blueprint->bars[bar].anchor_count; a++) {
                            int midi = g.blueprint->bars[bar].anchors[a].midi;
                            assert(midi < g.blueprint->climax_midi && midi >= g.low);
                        }
                    }
                }
                release(&g);
            }
            if (made < 5) {
                fprintf(stderr, "fixture %zu %s: only %u of %d seeds drew a blueprint\n", which, period ? "period" : "sentence", made, SEEDS);
            }
            assert(made >= 5);
        }
    }
}

static void test_weights(void) {
    BlueprintPreferences contrast;
    BlueprintPreferences late;
    BlueprintPreferences silent;
    blueprint_preferences_default(&contrast);
    memset(contrast.jobs[BLUEPRINT_FUNCTION_FRAGMENT], 0, sizeof contrast.jobs[BLUEPRINT_FUNCTION_FRAGMENT]);
    contrast.jobs[BLUEPRINT_FUNCTION_FRAGMENT][BLUEPRINT_JOB_CONTRAST] = 1;
    blueprint_preferences_default(&late);
    late.climax[0] = 0;
    blueprint_preferences_default(&silent);
    memset(silent.sounds, 0, sizeof silent.sounds);
    for (size_t which = 0; which < FIXTURES; which++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            GrowRequest request;
            Generated g = generate(which, false, seed, &contrast, &request);
            assert(g.blueprint->bars[4].job == GROW_JOB_CONTRAST || g.blueprint->bar_count == 0);
            release(&g);
            g = generate(which, false, seed, &late, &request);
            assert(!g.made || g.blueprint->climax_bar == 6);
            release(&g);
            for (int period = 0; period < 2; period++) {
                g = generate(which, period != 0, seed, &silent, &request);
                for (size_t bar = 0; g.made && bar < g.blueprint->bar_count; bar++) {
                    const char *text = g.blueprint->bars[bar].chords;
                    assert(!strstr(text, "add9") && !strstr(text, "maj7") && !strstr(text, "sus4"));
                    assert(g.blueprint->bars[bar].anchor_count == 0 && g.blueprint->bars[bar].event_count == 0);
                }
                release(&g);
            }
        }
    }
}

/* --- through the API ----------------------------------------------------------------------------- */

static cJSON *read_request(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    return request;
}

static void set_string(cJSON *request, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(request, name);
    assert(cJSON_AddStringToObject(request, name, value));
}

/* The MIDI number of a pitch name ("F#5", "Bb4"). */
static int midi_of_name(const char *name) {
    static const int LETTERS[7] = {9, 11, 0, 2, 4, 5, 7}; /* A to G */
    int pitch = LETTERS[name[0] - 'A'];
    const char *at = name + 1;
    for (; *at == '#' || *at == 'b'; at++) {
        pitch += *at == '#' ? 1 : -1;
    }
    return pitch + 12 * (atoi(at) + 1);
}

static void test_through_the_api(void) {
    cJSON *request = read_request();
    set_string(request, "development", "blueprint");
    assert(cJSON_AddNumberToObject(request, "seed", 1) && cJSON_AddBoolToObject(request, "midi", true));
    assert(cJSON_AddStringToObject(request, "explain", "text"));
    char *text = cJSON_PrintUnformatted(request);
    ComposerResponse first;
    UrsatzStatus status = composer_api_theme(test_context(), text, NULL, &first);
    if (status != URSATZ_OK) {
        fprintf(stderr, "development blueprint: %s at %s\n", first.error.message, first.error.path);
    }
    assert(status == URSATZ_OK);
    test_write_text("api_theme_request_generated.json", text);
    test_write_text("api_theme_response_generated.json", first.json);
    cJSON *document = cJSON_Parse(first.json);
    assert(document);
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(document, "brief");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(brief, "development")->valuestring, "blueprint") == 0);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(theme, "explanation");
    assert(!explanation || strstr(explanation->valuestring, "blueprint"));
    const cJSON *blueprint = cJSON_GetObjectItemCaseSensitive(theme, "blueprint");
    assert(cJSON_IsObject(blueprint) && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(blueprint, "bars")) == 8);
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(blueprint, "climax")));
    assert(strstr(first.plan_json ? first.plan_json : "", "\"climax\""));

    /* The blueprint sent back with the same motif, development balanced and the theme's seed, renders the same notes. */
    cJSON *back = read_request();
    set_string(back, "development", "balanced");
    /* The run's melody range is narrowed to the climax (R-751): the top is the climax, the bottom the least of range 16 and its distance above
       the motif's lowest note (D4, 62 in the fixture), not wider than the default 60 to 84; sent back, the blueprint needs that range. */
    int climax = midi_of_name(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(blueprint, "climax"), "pitch")->valuestring);
    int low = climax - (16 > climax - 62 ? 16 : climax - 62);
    cJSON *preferences = cJSON_AddObjectToObject(back, "preferences");
    cJSON *melody = preferences ? cJSON_AddObjectToObject(preferences, "melody") : NULL;
    cJSON *range = melody ? cJSON_AddObjectToObject(melody, "range") : NULL;
    assert(range && cJSON_AddNumberToObject(range, "low", low > 60 ? low : 60) && cJSON_AddNumberToObject(range, "high", climax < 84 ? climax : 84));
    cJSON_DeleteItemFromObjectCaseSensitive(back, "seed");
    assert(cJSON_AddNumberToObject(back, "seed", cJSON_GetObjectItemCaseSensitive(theme, "seed")->valuedouble));
    assert(cJSON_AddBoolToObject(back, "midi", true));
    assert(cJSON_AddItemToObject(back, "blueprint", cJSON_Duplicate(blueprint, true)));
    char *back_text = cJSON_PrintUnformatted(back);
    ComposerResponse second;
    status = composer_api_theme(test_context(), back_text, NULL, &second);
    if (status != URSATZ_OK) {
        fprintf(stderr, "the blueprint sent back: %s at %s\n", second.error.message, second.error.path);
    }
    assert(status == URSATZ_OK);
    assert(first.midi_size == second.midi_size && memcmp(first.midi, second.midi, first.midi_size) == 0);
    composer_response_destroy(&second);
    free(back_text);
    cJSON_Delete(back);

    /* The same request twice gives the same bytes. */
    ComposerResponse again;
    assert(composer_api_theme(test_context(), text, NULL, &again) == URSATZ_OK);
    assert(strcmp(first.json, again.json) == 0 && first.midi_size == again.midi_size && memcmp(first.midi, again.midi, first.midi_size) == 0);
    composer_response_destroy(&again);
    composer_response_destroy(&first);
    cJSON_Delete(document);
    cJSON_Delete(request);
    free(text);
}

static double ratio_of(const cJSON *object) {
    return cJSON_GetObjectItemCaseSensitive(object, "numerator")->valuedouble / cJSON_GetObjectItemCaseSensitive(object, "denominator")->valuedouble;
}

/* The candidates of the generator are ranked by the critic (R-755): with blueprint.candidates 3 the response has `rejected` and the kept theme
   scores at least each rejected one; with 1 it has neither `rejected` nor `critic`. */
static void test_ranking(void) {
    for (int candidates = 1; candidates <= 3; candidates += 2) {
        cJSON *request = read_request();
        set_string(request, "development", "blueprint");
        assert(cJSON_AddNumberToObject(request, "seed", 1));
        cJSON *preferences = cJSON_AddObjectToObject(request, "preferences");
        cJSON *blueprint = preferences ? cJSON_AddObjectToObject(preferences, "blueprint") : NULL;
        assert(blueprint && cJSON_AddNumberToObject(blueprint, "candidates", candidates));
        char *text = cJSON_PrintUnformatted(request);
        ComposerResponse response;
        assert(composer_api_theme(test_context(), text, NULL, &response) == URSATZ_OK);
        cJSON *document = cJSON_Parse(response.json);
        assert(document);
        const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
        const cJSON *rejected = cJSON_GetObjectItemCaseSensitive(document, "rejected");
        if (candidates == 1) {
            assert(!rejected && !cJSON_GetObjectItemCaseSensitive(theme, "critic"));
        } else {
            const cJSON *critic = cJSON_GetObjectItemCaseSensitive(theme, "critic");
            assert(cJSON_IsArray(rejected) && cJSON_GetArraySize(rejected) >= 1 && cJSON_IsObject(critic));
            double kept = ratio_of(cJSON_GetObjectItemCaseSensitive(critic, "score"));
            const cJSON *candidate;
            cJSON_ArrayForEach(candidate, rejected) {
                assert(kept >= ratio_of(cJSON_GetObjectItemCaseSensitive(candidate, "score")));
            }
        }
        cJSON_Delete(document);
        composer_response_destroy(&response);
        cJSON_Delete(request);
        free(text);
    }
}

/* The request with one or two members set (their values parsed as JSON) and development blueprint is refused at /development. */
static void refused_with(const char *member, const char *value, const char *second, const char *second_value) {
    cJSON *request = read_request();
    set_string(request, "development", "blueprint");
    const char *members[2] = {member, second};
    const char *values[2] = {value, second_value};
    for (int i = 0; i < 2; i++) {
        if (members[i]) {
            cJSON_DeleteItemFromObjectCaseSensitive(request, members[i]);
            cJSON *parsed = cJSON_Parse(values[i]);
            assert(parsed && cJSON_AddItemToObject(request, members[i], parsed));
        }
    }
    char *text = cJSON_PrintUnformatted(request);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), text, NULL, &response);
    if (status != URSATZ_ERR_REQUEST || strcmp(response.error.path, "/development") != 0 ||
        !strstr(response.error.message, "development blueprint writes an 8-bar sentence or period")) {
        fprintf(stderr, "%s=%s: %d at %s: %s\n", member ? member : "(none)", value ? value : "", (int)status, response.error.path,
                response.error.message);
    }
    assert(status == URSATZ_ERR_REQUEST && strcmp(response.error.path, "/development") == 0);
    assert(strstr(response.error.message, "development blueprint writes an 8-bar sentence or period"));
    composer_response_destroy(&response);
    cJSON_Delete(request);
    free(text);
}

static void refused(const char *member, const char *value) {
    refused_with(member, value, NULL, NULL);
}

static void test_refusals(void) {
    refused("bars", "4");
    refused_with("into", "\"hybrid\"", "hybrid", "\"antecedent+continuation\"");
    refused("chords", "[{\"bar\": 0, \"text\": \"I\"}]");
    refused("style", "\"" COMPOSER_FIXTURE_DIR "/style-fast-minor.json\"");
    refused("meter", "\"2/4\"");
    refused_with("mode", "null", "material", "\"dorian\"");
}

int main(void) {
    for (size_t i = 0; i < FIXTURES; i++) {
        char path[256];
        snprintf(path, sizeof path, "%s%s", COMPOSER_FIXTURE_DIR, FILES[i]);
        MOTIVE_OF[i] = load_motive(path);
    }
    test_drawn_blueprints();
    test_weights();
    test_through_the_api();
    test_ranking();
    test_refusals();
    printf("test_blueprint_generate: ok\n");
    return 0;
}
