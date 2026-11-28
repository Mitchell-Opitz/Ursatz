#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <common/hash.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The accompaniment of the four patterns, pinned (P69b B3, R-745): 15 theme requests (pattern by meter and key, in
 * fixtures/accompaniment-golden, from `229c40f`) at seeds 1 to 3, composed as `ursatz theme --seed` composes them, and the
 * first 16 digits of the SHA-256 of the MIDI file and of the plan JSON compared with the values of `229c40f`. The plan is
 * hashed without the lines of `producer_key`, `composition_key` and `knowledge_hash`, which move with every change to the
 * knowledge or the composer and say nothing about the accompaniment. Run with no argument, all 45 are composed (slow);
 * with `smoke` and `smoke-b`, seed 1 of two patterns each in 4/4 C major (two tests, each well under the fast set's 30 s on a CI runner); with `planed`, the planed chords alone (R-763: its own test, so the smoke stays under the fast set's 30 s on a CI runner).
 */

#define HASH_DIGITS 16

typedef struct {
    const char *name; /* the request file, without .json, and the pattern, meter and key it names */
    unsigned seed;
    const char *midi;
    const char *plan;
} Golden;

static const Golden GOLDEN[] = {
    {"block-44-Cmajor-period", 1, "f58bcf1c79fb1327", "757f3aaa6522afa0"},
    {"block-44-Cmajor-period", 2, "3b9fd10c6b76861b", "55caf0b801dad225"},
    {"block-44-Cmajor-period", 3, "aba6d5e9eee897aa", "5cd2d544e3e5247e"},
    {"block-44-Dminor-sentence", 1, "480f6e352b4d7b48", "ee79cb425d39ab3a"},
    {"block-44-Dminor-sentence", 2, "a60c74f768de369b", "82734a881b4f5acd"},
    {"block-44-Dminor-sentence", 3, "a34b79059e125b98", "066a0902177f53c6"},
    {"block-34-Fmajor-period", 1, "cc59fa6c7a5a26e8", "dd2ee8d19343533f"},
    {"block-34-Fmajor-period", 2, "2181dd1ecbea4826", "ff2f905f124d6b2d"},
    {"block-34-Fmajor-period", 3, "dedef1b51010c825", "44509ec78f209bf3"},
    {"block-68-Gmajor-sentence", 1, "eb8153a1029eaa9b", "4e88b42941a9ea4e"},
    {"block-68-Gmajor-sentence", 2, "c8b4c1145831088c", "1b8f889597a70a80"},
    {"block-68-Gmajor-sentence", 3, "441fee638da13af4", "dca68e1145713608"},
    {"arpeggiated-44-Cmajor-period", 1, "0237f38c48c4fb94", "2003003256125eaf"},
    {"arpeggiated-44-Cmajor-period", 2, "2f452daceef1efd5", "ec2cdc5336eeeb51"},
    {"arpeggiated-44-Cmajor-period", 3, "bbbe823456073e7c", "70928ff683408c20"},
    {"arpeggiated-44-Dminor-sentence", 1, "fa2c97dc9534af3a", "b624c3b2375639cb"},
    {"arpeggiated-44-Dminor-sentence", 2, "42518fa973b673e2", "115e1e7f8443fa26"},
    {"arpeggiated-44-Dminor-sentence", 3, "bc387882ecf30c34", "81f037ea0d21e808"},
    {"arpeggiated-34-Fmajor-period", 1, "30f073098904b8c9", "755a47db6abb1f55"},
    {"arpeggiated-34-Fmajor-period", 2, "e9ad580fdf8f8515", "341709662a2aa029"},
    {"arpeggiated-34-Fmajor-period", 3, "68c8aead831149ce", "1e840e24b99186c0"},
    {"arpeggiated-68-Gmajor-sentence", 1, "4022636cfe7f9381", "2acfa8df752c050e"},
    {"arpeggiated-68-Gmajor-sentence", 2, "840d7e13cc870cca", "744fff944e09ef6f"},
    {"arpeggiated-68-Gmajor-sentence", 3, "723c16705adff267", "de6beeb7728e4f0d"},
    {"alberti-44-Cmajor-period", 1, "8d6e9791f7098fc9", "c423b45cea07aa8f"},
    {"alberti-44-Cmajor-period", 2, "450e9f86e407c544", "27d991a396d09fc1"},
    {"alberti-44-Cmajor-period", 3, "864b486ca4dda3f0", "8a4ea5e43c5c115b"},
    {"alberti-44-Dminor-sentence", 1, "7a499cfb3d3c195e", "59fe4eaf35507049"},
    {"alberti-44-Dminor-sentence", 2, "d432865d20779c0e", "0792b6d667d3088a"},
    {"alberti-44-Dminor-sentence", 3, "5a22a16bdd8a17ab", "3ef22bea6ab7bd16"},
    {"alberti-34-Fmajor-period", 1, "0ccbef2281b25f33", "5b7e5655e7ee9e63"},
    {"alberti-34-Fmajor-period", 2, "b9fb1058f48bce91", "bd1a409885b7a453"},
    {"alberti-34-Fmajor-period", 3, "3eecd75c17516b41", "c32017741cc95eea"},
    {"oom-pah-44-Cmajor-period", 1, "df456e8bc55e3b51", "2854e07b15af3e9c"},
    {"oom-pah-44-Cmajor-period", 2, "4de96cd54f32d255", "052291484642fb98"},
    {"oom-pah-44-Cmajor-period", 3, "5f5823a6887b5fb5", "965bfb5333884399"},
    {"oom-pah-44-Dminor-sentence", 1, "0468bce1f3ba6a98", "61eec25496b77ee5"},
    {"oom-pah-44-Dminor-sentence", 2, "a1543256fabd24cb", "2a7a0943d379217b"},
    {"oom-pah-44-Dminor-sentence", 3, "505e4567e916cc5f", "e53adaf18b1b838d"},
    {"oom-pah-34-Fmajor-period", 1, "c5821ce9bb7cd2d4", "2434f5022d58c905"},
    {"oom-pah-34-Fmajor-period", 2, "a6361efe4ad7e37a", "e7ddc7ba64caa65e"},
    {"oom-pah-34-Fmajor-period", 3, "6cae8d2bf74b8b65", "f22302077f199e34"},
    {"oom-pah-68-Gmajor-sentence", 1, "be64419baf2f77ab", "397ee06e0db83aa8"},
    {"oom-pah-68-Gmajor-sentence", 2, "7ea65b985869eb08", "5f836e57a285016d"},
    {"oom-pah-68-Gmajor-sentence", 3, "a10637e695c2d170", "dcc4bc5320a25db1"},
};
#define GOLDEN_COUNT (sizeof GOLDEN / sizeof GOLDEN[0])

static const char *const SMOKE[] = {"block-44-Cmajor-period", "arpeggiated-44-Cmajor-period", "alberti-44-Cmajor-period",
                                    "oom-pah-44-Cmajor-period"};
#define SMOKE_COUNT 2 /* of each of the two smoke tests: the first two names, then the last two */
static size_t smoke_first = 0;

static bool in_smoke(const Golden *golden) {
    for (size_t i = smoke_first; i < smoke_first + SMOKE_COUNT; i++) {
        if (golden->seed == 1 && strcmp(golden->name, SMOKE[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* The request file of a golden with the options `ursatz theme <file> --seed <n> --midi <file> --out <file>` adds. */
static char *request_text(const Golden *golden) {
    char path[512];
    snprintf(path, sizeof path, COMPOSER_FIXTURE_DIR "/accompaniment-golden/%s.json", golden->name);
    char *text = test_must_read_file(path, NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_IsObject(request));
    char seed[16];
    snprintf(seed, sizeof seed, "%u", golden->seed);
    assert(cJSON_AddStringToObject(request, "seed", seed));
    assert(cJSON_AddBoolToObject(request, "check", false) && cJSON_AddBoolToObject(request, "midi", true) &&
           cJSON_AddBoolToObject(request, "musicxml", false) && cJSON_AddBoolToObject(request, "plain_midi", false) &&
           cJSON_AddBoolToObject(request, "plans", false));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

/* The plan text without the lines that name a key or the knowledge hash of the run. */
static char *masked_plan(const char *plan) {
    static const char *const DROPPED[] = {"\"producer_key\"", "\"composition_key\"", "\"knowledge_hash\""};
    char *out = malloc(strlen(plan) + 1);
    assert(out);
    size_t length = 0;
    for (const char *line = plan; *line;) {
        const char *end = strchr(line, '\n');
        size_t size = end ? (size_t)(end - line) + 1 : strlen(line);
        bool drop = false;
        for (size_t i = 0; i < sizeof DROPPED / sizeof DROPPED[0]; i++) {
            size_t word = strlen(DROPPED[i]);
            for (size_t at = 0; !drop && at + word <= size; at++) {
                drop = memcmp(line + at, DROPPED[i], word) == 0;
            }
        }
        if (!drop) {
            memcpy(out + length, line, size);
            length += size;
        }
        line += size;
    }
    out[length] = '\0';
    return out;
}

static void expect_hash(const void *data, size_t size, const char *expected, const Golden *golden, const char *what) {
    char hex[65];
    common_sha256_hex(data, size, hex);
    if (strncmp(hex, expected, HASH_DIGITS) != 0) {
        fprintf(stderr, "%s seed %u: the %s is %.16s, expected %s\n", golden->name, golden->seed, what, hex, expected);
    }
    assert(strncmp(hex, expected, HASH_DIGITS) == 0);
}

static void check_golden(const Golden *golden) {
    char *request = request_text(golden);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), request, NULL, &response);
    if (status != URSATZ_OK) {
        fprintf(stderr, "%s seed %u: %s: %s\n", golden->name, golden->seed, test_status_name(status), response.error.message);
    }
    assert(status == URSATZ_OK);
    assert(response.midi && response.plan_json);
    expect_hash(response.midi, response.midi_size, golden->midi, golden, "MIDI file");
    char *plan = masked_plan(response.plan_json);
    expect_hash(plan, strlen(plan), golden->plan, golden, "plan");
    free(plan);
    composer_response_destroy(&response);
    free(request);
}

/*
 * The planed chords (R-763): a 4/4 sentence in C major with `IV I@1 II@2 V@3` in bar 4 and the planed-chords pattern composes on
 * seed 1 with a chord on every beat of that bar, each block the first block's member order, each later block's lowest note within a
 * tritone of the first block's (the nearest pitch of its member, R-763). Note onsets are read in sixteenths of a whole note (a beat is 4).
 */
#define PLANED_BAR_START 64
#define PLANED_BEAT 4

static long sixteenths(const cJSON *rational) {
    long num = (long)cJSON_GetObjectItemCaseSensitive(rational, "num")->valuedouble;
    long den = (long)cJSON_GetObjectItemCaseSensitive(rational, "den")->valuedouble;
    assert(den > 0 && (16 * num) % den == 0);
    return 16 * num / den;
}

/* The pitch the inner voice `voice` (1 is the top) starts on the sixteenth `at`, or -1. */
static int inner_pitch_at(const cJSON *plan, int voice, long at) {
    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(plan, "texture")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(item, "composer");
        const char *lane = cJSON_GetObjectItemCaseSensitive(composer, "lane")->valuestring;
        if (strcmp(lane, "inner_voice") != 0 || (int)cJSON_GetObjectItemCaseSensitive(composer, "voice")->valuedouble != voice) {
            continue;
        }
        long start = sixteenths(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(item, "span"), "start"));
        const cJSON *note;
        cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "notes")) {
            if (start + sixteenths(cJSON_GetObjectItemCaseSensitive(note, "offset")) == at) {
                return (int)cJSON_GetObjectItemCaseSensitive(note, "midi_pitch")->valuedouble;
            }
        }
    }
    return -1;
}

static void check_planed(void) {
    static const char REQUEST[] =
        "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"into\":\"sentence\",\"key\":\"C\",\"material\":\"major\","
        "\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"development\":\"balanced\","
        "\"motives\":[{\"id\":\"A\",\"form\":\"full\",\"degrees\":\"3:q 2:e 1:e 2:q 3:q | 5:h 4:q 3:q\",\"pickup\":\"0\",\"harmony\":null,"
        "\"features\":[],\"role\":\"melodic\"}],\"slots\":{\"basic\":\"A\"},"
        "\"chords\":[{\"bar\":4,\"text\":\"IV I@1 II@2 V@3\"}],\"accompaniment\":{\"pattern\":\"michelle.pattern.planed-chords\"},"
        "\"seed\":\"1\",\"check\":false,\"midi\":false,\"musicxml\":false,\"plain_midi\":false,\"plans\":false}";
    /* The triad of each given chord, root, third, fifth. */
    static const int TRIADS[4][3] = {{5, 9, 0}, {0, 4, 7}, {2, 5, 9}, {7, 11, 2}};
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), REQUEST, NULL, &response);
    if (status != URSATZ_OK) {
        fprintf(stderr, "planed chords: %s: %s\n", test_status_name(status), response.error.message);
    }
    assert(status == URSATZ_OK && response.plan_json);
    cJSON *plan = cJSON_Parse(response.plan_json);
    assert(plan);
    int first_order[3] = {-1, -1, -1};
    int first_low = 0;
    for (int beat = 0; beat < 4; beat++) {
        int block[3];
        for (int voice = 3; voice >= 1; voice--) {
            block[3 - voice] = inner_pitch_at(plan, voice, PLANED_BAR_START + beat * PLANED_BEAT);
            assert(block[3 - voice] >= 0);
        }
        assert(block[0] < block[1] && block[1] < block[2]);
        int order[3];
        for (int i = 0; i < 3; i++) {
            order[i] = -1;
            for (int m = 0; m < 3; m++) {
                order[i] = block[i] % 12 == TRIADS[beat][m] ? m : order[i];
            }
            assert(order[i] >= 0);
        }
        if (beat == 0) {
            memcpy(first_order, order, sizeof order);
        } else {
            assert(memcmp(order, first_order, sizeof order) == 0);
            assert(abs(block[0] - first_low) <= 6);
        }
        first_low = beat == 0 ? block[0] : first_low;
    }
    cJSON_Delete(plan);
    composer_response_destroy(&response);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && (strcmp(argv[1], "smoke") == 0 || strcmp(argv[1], "smoke-b") == 0);
    smoke_first = argc > 1 && strcmp(argv[1], "smoke-b") == 0 ? SMOKE_COUNT : 0;
    if (argc > 1 && strcmp(argv[1], "planed") == 0) {
        check_planed();
        printf("test_accompaniment_patterns passed (the planed chords)\n");
        return 0;
    }
    size_t checked = 0;
    for (size_t i = 0; i < GOLDEN_COUNT; i++) {
        if (smoke && !in_smoke(&GOLDEN[i])) {
            continue;
        }
        check_golden(&GOLDEN[i]);
        checked++;
    }
    assert(checked == (smoke ? SMOKE_COUNT : GOLDEN_COUNT));
    printf("test_accompaniment_patterns passed (%zu themes)\n", checked);
    return 0;
}
