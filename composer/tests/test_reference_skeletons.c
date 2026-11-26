#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/harmony_phrase.h>

#include "support/test_blueprint_check.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Gate 2 of P69b (R-740, docs/blueprints.md): each reference skeleton (fixtures/references/<name>.skeleton.json) through the theme API
 * on seeds 1 to 10. At least 9 of the 10 compose (each skeleton's measured count is pinned below and may only rise), and in each
 * theme that composes every fixed item of the blueprint holds, read from the plan: each bar's chords are the given ones at their
 * beats; a bar with a rhythm has exactly its onsets and durations; an anchor's note has its pitch; an event's note is not a tone
 * of its chord (chord_mask) and moves by step to the next note (a suspension or a retardation keeps the pitch of the note before, an
 * anticipation is a tone of the chord after, an escape tone steps from the note before); the climax note has its pitch and no other
 * note reaches it; the accompaniment is the given pattern. With `smoke`, seed 1 of hand-A and ref-4-hollowknight; with a skeleton's
 * name, that skeleton alone.
 */

typedef struct {
    const char *name;
    int composed; /* of seeds 1 to 10, measured: the test fails below it */
} Skeleton;

static const Skeleton SKELETONS[] = {
    {"hand-A", 10}, {"hand-B", 10}, {"ref-1-megaman", 10}, {"ref-2-ff7", 10}, {"ref-3-banjo", 10}, {"ref-4-hollowknight", 10},
};
#define SKELETON_COUNT (sizeof SKELETONS / sizeof SKELETONS[0])

#define SEEDS 10

/* --- the runs --------------------------------------------------------------------------------------- */

static cJSON *skeleton_request(const char *name) {
    char path[512];
    snprintf(path, sizeof path, COMPOSER_FIXTURE_DIR "/references/%s.skeleton.json", name);
    char *text = test_must_read_file(path, NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_IsObject(request));
    return request;
}

/* Composes the skeleton on one seed as `ursatz theme <file> --seed <n>` does; true when it composed (and every fixed item held). */
static bool compose(const char *name, const cJSON *request, unsigned seed) {
    cJSON *sent = cJSON_Duplicate(request, true);
    char text[16];
    snprintf(text, sizeof text, "%u", seed);
    assert(sent && cJSON_AddStringToObject(sent, "seed", text) && cJSON_AddBoolToObject(sent, "check", false) &&
           cJSON_AddBoolToObject(sent, "midi", false) && cJSON_AddBoolToObject(sent, "musicxml", false) &&
           cJSON_AddBoolToObject(sent, "plain_midi", false) && cJSON_AddBoolToObject(sent, "plans", false));
    char *printed = cJSON_PrintUnformatted(sent);
    cJSON_Delete(sent);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), printed, NULL, &response);
    free(printed);
    Where where = {name, seed};
    if (status != URSATZ_OK) {
        /* Only a theme no seed tried could grow is a seed that does not compose; a refused request is a broken fixture. */
        printf("%s seed %u: %s: %s\n", name, seed, test_status_name(status), response.error.message);
        assert(status == URSATZ_ERR_UNSATISFIABLE);
        composer_response_destroy(&response);
        return false;
    }
    assert(response.plan_json);
    test_blueprint_check_fixed(&where, response.plan_json, request);
    composer_response_destroy(&response);
    return true;
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    if (smoke) {
        static const char *const SMOKE[] = {"hand-A", "ref-4-hollowknight"};
        for (size_t i = 0; i < 2; i++) {
            cJSON *request = skeleton_request(SMOKE[i]);
            assert(compose(SMOKE[i], request, 1));
            cJSON_Delete(request);
        }
        printf("test_reference_skeletons smoke: ok\n");
        return 0;
    }
    /* A skeleton's name as the argument runs that one alone (to look at one while working on it). */
    const char *only = argc > 1 ? argv[1] : NULL;
    for (size_t i = 0; i < SKELETON_COUNT; i++) {
        const Skeleton *skeleton = &SKELETONS[i];
        if (only && strcmp(only, skeleton->name) != 0) {
            continue;
        }
        cJSON *request = skeleton_request(skeleton->name);
        int composed = 0;
        for (unsigned seed = 1; seed <= SEEDS; seed++) {
            composed += compose(skeleton->name, request, seed);
        }
        cJSON_Delete(request);
        printf("%s: %d of %d composed (pinned %d)\n", skeleton->name, composed, SEEDS, skeleton->composed);
        assert(skeleton->composed >= 9);
        assert(composed >= skeleton->composed);
    }
    printf("test_reference_skeletons: ok\n");
    return 0;
}
