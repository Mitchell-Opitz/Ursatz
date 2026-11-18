#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <io/imported_score.h>
#include <ursatz_analyzer/motive_links.h>

#include "cJSON.h"
#include "links/links_internal.h"
#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * Motive links over the four pieces of tests/fixtures/links: a (C major) and b (G major, a's motive transposed and augmented 2:1)
 * link by one step across keys, d (a's motive inverted) links to both, c (unrelated) is in no link. Checks the chain
 * replays from one prototype sketch to the other, the cache (a second run compares nothing; adding a piece compares only
 * the pairs with its families; another analyzer version empties it), the scope's errors and the JSON text.
 */

#define PIECES 4
static const char *const piece_ids[PIECES] = {"a.musicxml", "b.musicxml", "c.musicxml", "d.musicxml"};
static char *documents[PIECES];

static bool analyze_fixtures(void) {
    for (int i = 0; i < PIECES; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", URSATZ_LINKS_DIR, piece_ids[i]);
        ImportedScore notes;
        ChainResult report;
        if (imported_score_import_file(path, IMPORTED_FORMAT_MUSICXML, &notes, NULL) != IMPORTED_SCORE_OK) {
            fprintf(stderr, "cannot import %s\n", path);
            return false;
        }
        if (!run_analysis_pipeline(&notes, test_knowledge(), &report)) {
            fprintf(stderr, "cannot analyze %s\n", path);
            imported_score_destroy(&notes);
            return false;
        }
        documents[i] = test_render_document(path, &notes, &report);
        chain_result_destroy(&report);
        imported_score_destroy(&notes);
        if (!documents[i]) {
            return false;
        }
    }
    return true;
}

static MotiveLinkScope *scope_of(size_t count) {
    MotiveLinkScope *scope = motive_link_scope_create(test_knowledge());
    for (size_t i = 0; scope && i < count; i++) {
        CHECK(motive_link_scope_add_document(scope, piece_ids[i], documents[i]) == MOTIVE_LINKS_OK);
    }
    return scope;
}

static const MotiveLinkFound *link_between(const MotiveLinks *links, const char *piece_a, const char *piece_b) {
    for (size_t i = 0; i < links->link_count; i++) {
        const MotiveLinkFound *link = &links->links[i];
        if (strcmp(link->a.piece, piece_a) == 0 && strcmp(link->b.piece, piece_b) == 0) {
            return link;
        }
    }
    return NULL;
}

static bool in_a_link(const MotiveLinks *links, const char *piece) {
    for (size_t i = 0; i < links->link_count; i++) {
        if (strcmp(links->links[i].a.piece, piece) == 0 || strcmp(links->links[i].b.piece, piece) == 0) {
            return true;
        }
    }
    return false;
}

static bool in_a_group(const MotiveLinks *links, const char *piece) {
    for (size_t g = 0; g < links->group_count; g++) {
        for (size_t m = 0; m < links->groups[g].member_count; m++) {
            if (strcmp(links->groups[g].members[m].piece, piece) == 0) {
                return true;
            }
        }
    }
    return false;
}

/* The sketch a family is linked as, from the scope's own list. */
static bool scope_sketch(const MotiveLinkScope *scope, const char *piece_id, size_t family, MotiveSketch *out) {
    for (size_t i = 0; i < scope->family_count; i++) {
        if (scope->families[i].family == family && strcmp(scope->pieces[scope->families[i].piece].id, piece_id) == 0) {
            *out = scope->families[i].sketch;
            return true;
        }
    }
    return false;
}

/* Replaying a link's steps (the structs and the text) from the source family's sketch gives the target's. */
static void check_replay(const MotiveLinkScope *scope, const MotiveLinkFound *link) {
    const MotiveLinkEnd *source = link->reversed ? &link->b : &link->a;
    const MotiveLinkEnd *target = link->reversed ? &link->a : &link->b;
    MotiveSketch from;
    MotiveSketch to;
    CHECK(scope_sketch(scope, source->piece, source->family, &from));
    CHECK(scope_sketch(scope, target->piece, target->family, &to));
    CHECK(link->step_count >= 1 && link->step_count <= 2);
    MotiveSketch by_struct = from;
    MotiveSketch by_text = from;
    for (size_t s = 0; s < link->step_count; s++) {
        MotiveSketch next;
        CHECK(motive_relation_apply(&by_struct, &link->steps[s], &next) == MOTIVE_RELATION_OK);
        by_struct = next;
        MotiveRelationStep parsed;
        CHECK(motive_relation_step_parse(link->step_text[s], &parsed));
        CHECK(motive_relation_apply(&by_text, &parsed, &next) == MOTIVE_RELATION_OK);
        by_text = next;
    }
    CHECK(motive_sketch_equal(&by_struct, &to));
    CHECK(motive_sketch_equal(&by_text, &to));
}

static size_t progress_calls;
static size_t progress_last_done;
static size_t progress_last_total;

static void on_progress(size_t done, size_t total, void *context) {
    CHECK(context == &progress_calls);
    progress_calls++;
    progress_last_done = done;
    progress_last_total = total;
}

static void check_scope_errors(void) {
    MotiveLinkScope *scope = motive_link_scope_create(test_knowledge());
    CHECK(motive_link_scope_add_document(scope, "a.musicxml", documents[0]) == MOTIVE_LINKS_OK);
    CHECK(motive_link_scope_add_document(scope, "a.musicxml", documents[0]) == MOTIVE_LINKS_ERR_ARGUMENT);
    CHECK(motive_link_scope_add_document(scope, "x", "not json") == MOTIVE_LINKS_ERR_FORMAT);
    CHECK(motive_link_scope_add_document(scope, "x", "{\"schema_version\": 8, \"motives\": []}") ==
          MOTIVE_LINKS_ERR_FORMAT);
    CHECK(motive_link_scope_add_document(NULL, "x", documents[0]) == MOTIVE_LINKS_ERR_ARGUMENT);
    CHECK(scope->piece_count == 1);

    /* a document written before features: the family has none */
    cJSON *document = cJSON_Parse(documents[1]);
    cJSON *motives = cJSON_GetObjectItemCaseSensitive(document, "motives");
    CHECK(cJSON_GetArraySize(motives) > 0);
    cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(motives, 0), "features");
    char *stripped = cJSON_PrintUnformatted(document);
    cJSON_Delete(document);
    CHECK(motive_link_scope_add_document(scope, "b.musicxml", stripped) == MOTIVE_LINKS_ERR_NO_FEATURES);
    CHECK(strcmp(motive_link_scope_failed_piece(scope), "b.musicxml") == 0);
    CHECK(scope->piece_count == 1);
    CHECK(motive_link_scope_add_document(scope, "b.musicxml", documents[1]) == MOTIVE_LINKS_OK);
    cJSON_free(stripped);

    /* a piece without motives is a piece without families */
    CHECK(motive_link_scope_add_document(scope, "empty", "{\"schema_version\": 13, \"motives\": []}") ==
          MOTIVE_LINKS_OK);
    CHECK(scope->piece_count == 3);
    CHECK(scope->family_count == 2);

    MotiveLinksOptions options;
    motive_links_options_default(&options);
    options.candidates = 0;
    MotiveLinks links;
    CHECK(motive_links_run(scope, NULL, &options, &links) == MOTIVE_LINKS_ERR_ARGUMENT);
    motive_link_scope_destroy(scope);
}

int main(void) {
    if (!analyze_fixtures()) {
        return 1;
    }
    char cache_path[600];
    snprintf(cache_path, sizeof cache_path, "%s/cache.json", URSATZ_LINKS_WORK_DIR);
    char command[700];
    snprintf(command, sizeof command, "mkdir -p '%s'", URSATZ_LINKS_WORK_DIR);
    if (system(command) != 0) {
        return 1;
    }
    remove(cache_path);

    check_scope_errors();

    /* a, b, c: a and b link by one step across keys, c is in no link */
    MotiveLinkScope *scope = scope_of(3);
    CHECK(scope->piece_count == 3);
    CHECK(scope->family_count == 3);
    MotiveLinkCache *cache = NULL;
    CHECK(motive_link_cache_load(cache_path, &cache) == MOTIVE_LINKS_OK);
    CHECK(cache->count == 0);
    MotiveLinksOptions options;
    motive_links_options_default(&options);
    options.progress = on_progress;
    options.progress_context = &progress_calls;
    MotiveLinks first;
    CHECK(motive_links_run(scope, cache, &options, &first) == MOTIVE_LINKS_OK);
    CHECK(first.pieces == 3);
    CHECK(first.pairs == 1 && first.compared == 1 && first.cached == 0 && !first.cache_reset);
    CHECK(progress_calls == 2 && progress_last_done == 1 && progress_last_total == 1);
    CHECK(first.link_count == 1);
    const MotiveLinkFound *ab = link_between(&first, "a.musicxml", "b.musicxml");
    CHECK(ab != NULL);
    if (ab) {
        CHECK(ab->step_count == 1 && strcmp(ab->step_text[0], "augment ratio=2:1") == 0);
        CHECK(!ab->reversed);
        CHECK(ab->a.first_measure == 1 && ab->a.last_measure == 1 && ab->a.beat == 1.0);
        CHECK(ab->b.first_measure == 1 && ab->b.last_measure == 2);
        CHECK(strcmp(ab->a.degrees, "1 3 5 4 3") == 0 && strcmp(ab->b.degrees, "1 3 5 4 3") == 0);
        CHECK(ab->distance_numerator == 1 && ab->distance_denominator == 5);
        CHECK(ab->similarity[MOTIVE_DIMENSION_INTERVALS] == 1.0);
        check_replay(scope, ab);
    }
    CHECK(!in_a_link(&first, "c.musicxml") && !in_a_group(&first, "c.musicxml"));
    CHECK(first.group_count == 1 && first.groups[0].member_count == 2);
    CHECK(strcmp(first.groups[0].members[0].piece, "a.musicxml") == 0);
    CHECK(strcmp(first.groups[0].members[1].piece, "b.musicxml") == 0);

    char *first_json = NULL;
    CHECK(motive_links_render(&first, &first_json) == MOTIVE_LINKS_OK);
    char *first_json_again = NULL;
    CHECK(motive_links_render(&first, &first_json_again) == MOTIVE_LINKS_OK);
    CHECK(first_json && first_json_again && strcmp(first_json, first_json_again) == 0);
    cJSON *rendered = first_json ? cJSON_Parse(first_json) : NULL;
    CHECK(rendered != NULL);
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(rendered, "links")) == 1);
    CHECK(cJSON_GetObjectItemCaseSensitive(rendered, "compared")->valuedouble == 1);
    cJSON_Delete(rendered);
    free(first_json);
    free(first_json_again);

    /* a second run with the saved cache compares nothing and gives the same links */
    CHECK(motive_link_cache_save(cache, cache_path) == MOTIVE_LINKS_OK);
    motive_link_cache_destroy(cache);
    cache = NULL;
    CHECK(motive_link_cache_load(cache_path, &cache) == MOTIVE_LINKS_OK);
    CHECK(cache->count == 1);
    MotiveLinks second;
    CHECK(motive_links_run(scope, cache, &options, &second) == MOTIVE_LINKS_OK);
    CHECK(second.pairs == 1 && second.compared == 0 && second.cached == 1 && !second.cache_reset);
    CHECK(second.link_count == 1 && second.group_count == 1);
    const MotiveLinkFound *again = link_between(&second, "a.musicxml", "b.musicxml");
    CHECK(again && ab && again->step_count == ab->step_count && strcmp(again->step_text[0], ab->step_text[0]) == 0 &&
          again->distance_numerator == ab->distance_numerator && again->reversed == ab->reversed);
    motive_links_destroy(&second);
    motive_links_destroy(&first);
    motive_link_scope_destroy(scope);

    /* adding d compares only the pairs with d's family, and links it into the group */
    scope = scope_of(4);
    MotiveLinks third;
    CHECK(motive_links_run(scope, cache, &options, &third) == MOTIVE_LINKS_OK);
    CHECK(third.pairs == 3 && third.cached == 1 && third.compared == 2);
    CHECK(third.link_count == 3);
    const MotiveLinkFound *ad = link_between(&third, "a.musicxml", "d.musicxml");
    CHECK(ad != NULL);
    if (ad) {
        CHECK(ad->step_count == 1 && strcmp(ad->step_text[0], "invert tonal=1") == 0);
        check_replay(scope, ad);
    }
    const MotiveLinkFound *bd = link_between(&third, "b.musicxml", "d.musicxml");
    CHECK(bd != NULL);
    if (bd) {
        check_replay(scope, bd);
    }
    CHECK(third.group_count == 1 && third.groups[0].member_count == 3);
    CHECK(in_a_group(&third, "d.musicxml") && !in_a_group(&third, "c.musicxml"));
    motive_links_destroy(&third);

    /* another analyzer version empties the cache */
    CHECK(motive_link_cache_save(cache, cache_path) == MOTIVE_LINKS_OK);
    motive_link_cache_destroy(cache);
    cache = NULL;
    char *saved = test_read_file(cache_path, NULL);
    CHECK(saved != NULL);
    char *at = saved ? strstr(saved, "\"analyzer_version\":\t\"") : NULL;
    CHECK(at != NULL);
    if (at) {
        char *value = at + strlen("\"analyzer_version\":\t\"");
        value[0] = '9';
        CHECK(test_write_file(cache_path, saved, strlen(saved)));
    }
    free(saved);
    CHECK(motive_link_cache_load(cache_path, &cache) == MOTIVE_LINKS_OK);
    MotiveLinks reset;
    CHECK(motive_links_run(scope, cache, &options, &reset) == MOTIVE_LINKS_OK);
    CHECK(reset.cache_reset && reset.compared == reset.pairs && reset.cached == 0 && reset.pairs == 3);
    MotiveLinks settled;
    CHECK(motive_links_run(scope, cache, &options, &settled) == MOTIVE_LINKS_OK);
    CHECK(!settled.cache_reset && settled.compared == 0);
    motive_links_destroy(&settled);
    motive_links_destroy(&reset);

    /* options that differ empty it too */
    MotiveLinksOptions shallower = options;
    shallower.link.max_depth = 1;
    MotiveLinks shallow;
    CHECK(motive_links_run(scope, cache, &shallower, &shallow) == MOTIVE_LINKS_OK);
    CHECK(shallow.cache_reset && shallow.compared == shallow.pairs);
    CHECK(shallow.link_count == 2); /* a-b (augment) and a-d (invert) are single steps; b-d needs two */
    motive_links_destroy(&shallow);

    /* a cache of schema version 1 (keyed by ursatz_commit) loads as an empty cache and is emptied by the next run */
    CHECK(motive_link_cache_save(cache, cache_path) == MOTIVE_LINKS_OK);
    char *current = test_read_file(cache_path, NULL);
    char *version_at = current ? strstr(current, "\"schema_version\":\t2") : NULL;
    CHECK(version_at != NULL);
    if (version_at) {
        version_at[strlen("\"schema_version\":\t")] = '1';
        CHECK(test_write_file(cache_path, current, strlen(current)));
    }
    free(current);
    MotiveLinkCache *older = NULL;
    CHECK(motive_link_cache_load(cache_path, &older) == MOTIVE_LINKS_OK && older != NULL);
    if (older) {
        MotiveLinks restarted;
        CHECK(motive_links_run(scope, older, &options, &restarted) == MOTIVE_LINKS_OK);
        CHECK(restarted.cache_reset && restarted.compared == restarted.pairs && restarted.cached == 0);
        motive_links_destroy(&restarted);
        motive_link_cache_destroy(older);
    }

    /* a file that is not a cache is refused */
    const char *other_schema = "{\"schema\": \"other\"}";
    CHECK(test_write_file(cache_path, other_schema, strlen(other_schema)));
    MotiveLinkCache *refused = NULL;
    CHECK(motive_link_cache_load(cache_path, &refused) == MOTIVE_LINKS_ERR_FORMAT && refused == NULL);

    motive_link_cache_destroy(cache);
    motive_link_scope_destroy(scope);
    remove(cache_path);
    for (int i = 0; i < PIECES; i++) {
        free(documents[i]);
    }
    return test_finish("test_motive_links");
}
