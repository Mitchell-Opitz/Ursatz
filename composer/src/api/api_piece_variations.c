#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/variations.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"

/*
 * Variations of one section of a loop plan (docs/api.md): the section is written again from one seed after another (as a
 * regenerate does, the seam and the loop's end included) and a version is kept when it sounds different from the plan and
 * from the ones kept before.
 */

typedef struct {
    PieceVersion version;
    unsigned long seed;
    uint64_t signature;
} Variation;

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

static void variations_destroy(Variation *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        api_piece_version_destroy(&items[i].version);
    }
}

/* Adds item to object under name and returns true; the item is deleted when it is NULL or cannot be added. */
static bool add(cJSON *object, const char *name, cJSON *item) {
    if (!object || !item || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

/*
 * {"schema": "ursatz-piece-variations", "section", "lane", "brief", "variations": [{"index", "seed", "loop": <version, its
 * "plan" the plan object>, "loop_markers": {"start", "end"}}], "kept", "requested", "seeds_tried"}.
 */
static ComposerStatus variations_document(const PieceSpec *request, const PieceEdit *edit, const Variation *items, size_t kept,
                                  size_t tried, unsigned long requested, bool musicxml, cJSON **result, ComposerError *error) {
    char lanes[64];
    GrowBrief brief;
    memset(&brief, 0, sizeof(brief));
    brief.tempo = request->tempo;
    brief.tempo_from = request->tempo_from;
    brief.minor = request->structure.key.mode == KEY_STRUCTURE_MODE_MINOR;
    brief.mode_from = request->mode_from;
    brief.has_density = composer_style_density(request->style, &brief.density);
    brief.without = request->without;
    brief.without_count = request->without_count;
    api_piece_lanes_name(edit, lanes, sizeof lanes);
    cJSON *document = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    bool ok = document && cJSON_AddStringToObject(document, "schema", "ursatz-piece-variations") &&
              cJSON_AddStringToObject(document, "section", edit->section) && cJSON_AddStringToObject(document, "lane", lanes) &&
              add(document, "brief", api_brief_json(&brief)) && add(document, "variations", list);
    for (size_t i = 0; ok && i < kept; i++) {
        const Variation *variation = &items[i];
        cJSON *entry = cJSON_CreateObject();
        cJSON *loop = api_piece_version_json(request, &variation->version);
        cJSON *plan = api_piece_plan_object(variation->version.plan);
        ok = entry && loop && plan && cJSON_ReplaceItemInObject(loop, "plan", plan);
        if (!ok) {
            cJSON_Delete(loop && plan ? NULL : plan);
        }
        ok = ok && cJSON_AddNumberToObject(entry, "index", (double)(i + 1)) &&
             cJSON_AddNumberToObject(entry, "seed", (double)variation->seed) && add(entry, "loop", loop);
        if (ok) {
            cJSON *markers = api_piece_loop_json(variation->version.plan);
            ok = !markers || add(entry, "loop_markers", markers);
        }
        if (ok && musicxml) {
            char *text = NULL;
            ComposerStatus rendered = api_piece_musicxml(request, variation->version.plan, &text, error);
            if (rendered != COMPOSER_OK) {
                cJSON_Delete(entry);
                cJSON_Delete(document);
                return rendered;
            }
            ok = cJSON_AddStringToObject(entry, "musicxml", text) != NULL;
            free(text);
        }
        if (!ok || !cJSON_AddItemToArray(list, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    ok = ok && cJSON_AddNumberToObject(document, "kept", (double)kept) &&
         cJSON_AddNumberToObject(document, "requested", (double)requested) &&
         cJSON_AddNumberToObject(document, "seeds_tried", (double)tried);
    if (!ok) {
        cJSON_Delete(document);
        return fail_alloc(error);
    }
    *result = document;
    return COMPOSER_OK;
}

/* "variations": {"section", "lanes", "count"}. */
static ComposerStatus read_variations(const cJSON *root, PieceEdit *edit, unsigned long *count, ComposerError *error) {
    const cJSON *variations = cJSON_GetObjectItemCaseSensitive(root, "variations");
    const char *section = NULL;
    bool present;
    if (!cJSON_IsObject(variations)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/variations", "the request needs \"variations\": {\"section\", \"lanes\", \"count\"}");
    }
    if (!api_request_string(variations, "/variations", "section", true, &section, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (!api_member_whole(variations, "count", 1, PIECE_VARIATIONS_MAX, count, &present, error)) {
        snprintf(error->path, sizeof error->path, "/variations/count");
        return COMPOSER_ERR_REQUEST;
    }
    if (!present) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/variations/count", "the request needs \"count\": 1 to %d",
                        PIECE_VARIATIONS_MAX);
    }
    edit->section = section;
    return api_piece_read_lanes(cJSON_GetObjectItemCaseSensitive(variations, "lanes"), "/variations/lanes", edit, error);
}

ComposerStatus composer_api_piece_variations(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    PieceSpec *request = calloc(1, sizeof(*request));
    PieceEdit edit;
    Variation items[PIECE_VARIATIONS_MAX];
    char *plan_text = NULL;
    bool plain_midi = false;
    bool musicxml = false;
    bool check = false;
    bool present = false;
    bool seeded = false;
    unsigned long seed = 0;
    unsigned long requested = 0;
    const char *style = NULL;
    size_t kept = 0;
    size_t tried = 0;
    memset(&edit, 0, sizeof(edit));
    edit.path = "/variations";
    memset(items, 0, sizeof(items));
    ComposerStatus status = request ? api_parse_request(request_json, &root, &out->error) : fail_alloc(&out->error);
    if (status == COMPOSER_OK) {
        if (!api_member_bool(root, "check", &check, &present, &out->error) ||
            !api_member_bool(root, "plain_midi", &plain_midi, &present, &out->error) ||
            !api_member_bool(root, "musicxml", &musicxml, &present, &out->error) ||
            !api_member_whole(root, "seed", 0, PIECE_SEED_MAX, &seed, &seeded, &out->error) ||
            !api_member_string(root, "style", &style, &present, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == COMPOSER_OK) {
        status = read_variations(root, &edit, &requested, &out->error);
    }
    if (status == COMPOSER_OK && !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "request"))) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/request", "the request needs \"request\": the piece request");
    }
    if (status == COMPOSER_OK && !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(root, "from"))) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/from", "the request needs \"from\": the loop plan to vary");
    }
    uint64_t original_signature = 0;
    if (status == COMPOSER_OK) {
        plan_text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(root, "from"));
        edit.text = plan_text;
        PiecePlan *original = NULL;
        PlanJsonError plan_error;
        if (!plan_text) {
            status = fail_alloc(&out->error);
        } else if (plan_from_json(plan_text, &original, &plan_error) != PLAN_OK) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/from", "cannot read the loop plan: %s", plan_error.message);
        } else {
            original_signature = plan_signature(original);
            plan_destroy(original);
        }
    }
    if (status == COMPOSER_OK) {
        status = api_piece_read_request(request, cJSON_GetObjectItemCaseSensitive(root, "request"), "/request", style, plain_midi,
                                        job, &out->error);
    }
    unsigned total = (unsigned)(PIECE_SEEDS_PER_VARIATION * requested);
    for (size_t attempt = 0; status == COMPOSER_OK && kept < requested && attempt < total; attempt++) {
        if (composer_job_step(job, "variations", (unsigned)attempt, total)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
            break;
        }
        Variation *next = &items[kept];
        unsigned long used = (seed + attempt) & PIECE_SEED_MAX;
        PieceResult result;
        status = api_piece_regenerate_loop(request, &edit, check, used, &next->version, &result, &out->error);
        tried++;
        bool keep = status == COMPOSER_OK && result == PIECE_OK;
        if (keep) {
            next->signature = plan_signature(next->version.plan);
            keep = next->signature != original_signature;
            for (size_t i = 0; keep && i < kept; i++) {
                keep = items[i].signature != next->signature;
            }
        }
        if (keep) {
            next->seed = used;
            kept++;
        } else {
            api_piece_version_destroy(&next->version);
        }
    }
    if (status == COMPOSER_OK && kept == 0) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "",
                          "no version of section '%s' that differs from the plan was found in %zu seeds from %lu", edit.section,
                          tried, seed);
    }
    if (status == COMPOSER_OK) {
        cJSON *document = NULL;
        status = variations_document(request, &edit, items, kept, tried, requested, musicxml, &document, &out->error);
        if (status == COMPOSER_OK) {
            status = api_respond_json(out, document);
        }
    }
    variations_destroy(items, kept);
    if (request) {
        api_piece_request_destroy(request);
    }
    free(request);
    cJSON_free(plan_text);
    cJSON_Delete(root);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
