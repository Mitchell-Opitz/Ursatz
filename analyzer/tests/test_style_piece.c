#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/analysis_result.h>
#include <ursatz_analyzer_internal/style_profile.h>
#include "result/analysis_result_internal.h"

#include <chain/chain_style.h>

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The two sources of a style piece (R-617): for Bach 114, Mozart 545, Clementi 36, Field 1 and Field 5 the statistics of a
 * profile counted from the analysis document's JSON equal those of a profile counted from chain_style_piece on the chain
 * that made the document.
 *
 *   test_style_piece          the five pieces
 *   test_style_piece smoke    Bach 114 only
 */

static const char *const PIECES[] = {"Bach 114.mid", "Mozart 545.mid", "Clementi 36.mid", "Field 1.mid", "Field 5.mid"};

static void check_piece(const char *fixture) {
    char path[512];
    snprintf(path, sizeof path, "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedScore notes;
    if (imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) != IMPORTED_SCORE_OK) {
        CHECKF(false, "%s: cannot import", fixture);
        return;
    }
    AnalyzerResult *result = NULL;
    AnalyzerOptions options = test_analyzer_options();
    CHECKF(analyzer_analyze(&notes, &options, &result) == URSATZ_OK && result, "%s: analyze", fixture);
    if (result) {
        char *json = analysis_document_render_result(fixture, result);
        StyleProfile *from_json = style_profile_create();
        StyleProfile *from_chain = style_profile_create();
        StylePiece piece = {0};
        CHECKF(json && from_json && from_chain && analyzer_style_add_document_json(from_json, json) == STYLE_PROFILE_OK &&
                   chain_style_piece(analyzer_result_run_chain(result), &piece) &&
                   style_profile_add_piece(from_chain, &piece) == STYLE_PROFILE_OK,
               "%s: profiles", fixture);
        StyleStatistic *left = NULL, *right = NULL;
        size_t left_count = 0, right_count = 0;
        bool read = from_json && from_chain && style_profile_statistics(from_json, &left, &left_count) == STYLE_PROFILE_OK &&
                    style_profile_statistics(from_chain, &right, &right_count) == STYLE_PROFILE_OK;
        CHECKF(read && left_count > 0 && left_count == right_count, "%s: %zu statistics from the document, %zu from the chain",
               fixture, left_count, right_count);
        for (size_t i = 0; read && i < left_count && i < right_count; i++) {
            CHECKF(strcmp(left[i].category, right[i].category) == 0 && strcmp(left[i].label, right[i].label) == 0 &&
                       left[i].count == right[i].count && left[i].total == right[i].total,
                   "%s: %s '%s' %lu/%lu from the document, %s '%s' %lu/%lu from the chain", fixture, left[i].category,
                   left[i].label, left[i].count, left[i].total, right[i].category, right[i].label, right[i].count,
                   right[i].total);
        }
        CHECKF(style_profile_piece_count(from_json) == 1 && style_profile_piece_count(from_chain) == 1, "%s: piece count", fixture);
        free(left);
        free(right);
        style_piece_destroy(&piece);
        style_profile_destroy(from_json);
        style_profile_destroy(from_chain);
        free(json);
    }
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    for (size_t i = 0; i < (smoke ? 1 : sizeof PIECES / sizeof PIECES[0]); i++) {
        check_piece(PIECES[i]);
    }
    return test_finish("test_style_piece");
}
