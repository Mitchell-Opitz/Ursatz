#include "standard-texture/analysis/texture_analysis_classify.h"
#include "standard-texture/analysis/texture_analysis_input.h"
#include "ursatz_chain/chain_inputs.h"
#include "weighted_pipeline.h"

/*
 * The texture changes of Field 1 (Nocturne No. 1 in E-flat major, in 12/8), against tests/fixtures/annotations/field1_texture.txt: a DRAFT
 * hand reading, not yet checked against the score (see its header), made from the notes of the file and not from this code's output.
 *
 * The adapter runs on the rule-based voice separation (per track, so per hand) of the imported file, with the library's defaults. Its windows
 * are measures; the texture of each is read at the level of the CATEGORY (monophony, heterophony, homophony, polyphony), the annotation's level:
 * homorhythm and melody and accompaniment are both homophony.
 *
 *   A MEASURE is RIGHT when the category of its window's texture (after a window with no texture takes its neighbour's, as passages are
 *   built) is the annotated one. Only the annotated measures are scored.
 *   A CHANGE POINT is FOUND when the adapter has a passage that starts within one measure of it. A FALSE change point is a passage start (a change of
 *   TEXTURE, finer than the annotated change of category) with no annotated change within one measure; they are reported by whether the category changes.
 *
 * Asserted: at least 80% of the annotated measures right, and every annotated change point found. The misses, the false change points and the
 * confusion between the annotated and the found category are reported. Nothing is tuned to the fixture: every default is the library's.
 */

#define MEASURE_TARGET 0.80

typedef struct {
    int first;
    int last;
    StandardTexture category;
} AnnotatedPassage;

typedef struct {
    int range_first;
    int range_last;
    AnnotatedPassage passages[32];
    size_t passage_count;
    int changes[32];
    size_t change_count;
} TextureAnnotation;

static TextureAnnotation load_texture_annotation(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/field1_texture.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    TextureAnnotation a;
    memset(&a, 0, sizeof(a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[32];
        int x = 0;
        int y = 0;
        if (sscanf(line, "range %d %d", &x, &y) == 2) {
            a.range_first = x;
            a.range_last = y;
        } else if (sscanf(line, "passage %d %d %31s", &x, &y, word) == 3) {
            StandardTexture category;
            assert(standard_texture_from_name(word, &category) && standard_texture_is_category(category));
            a.passages[a.passage_count].first = x;
            a.passages[a.passage_count].last = y;
            a.passages[a.passage_count++].category = category;
        } else if (sscanf(line, "change %d", &x) == 1) {
            a.changes[a.change_count++] = x;
        }
    }
    free(text);
    assert(a.range_first > 0 && a.range_last >= a.range_first && a.passage_count > 0);
    return a;
}

static bool annotated_category(const TextureAnnotation *a, int measure, StandardTexture *category) {
    for (size_t i = 0; i < a->passage_count; i++) {
        if (measure >= a->passages[i].first && measure <= a->passages[i].last) {
            *category = a->passages[i].category;
            return true;
        }
    }
    return false;
}

int main(void) {
    TextureAnnotation annotation = load_texture_annotation();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Field 1.mid", NULL);
    const MeterMap *map = p.meter.meter_map;
    ChainPitches pitches;
    assert(chain_build_pitches(&p.set, &pitches) == CHAIN_INPUT_OK);
    ChainTextureInput build;
    assert(chain_build_texture_input(&p.set, &pitches, &build) == CHAIN_INPUT_OK);
    printf("Field 1, measures %d-%d (draft hand analysis): %zu notes in %zu track(s)\n", annotation.range_first, annotation.range_last,
           build.input.note_count, build.stream_count);
    size_t per_track[8] = {0};
    for (size_t s = 0; s < build.assignment.stream_count && s < 8; s++) {
        per_track[s] = build.assignment.streams[s].voice_count;
    }
    printf("  voices per track after the separation:");
    for (size_t s = 0; s < build.assignment.stream_count && s < 8; s++) {
        printf(" track %zu: %zu", build.assignment.streams[s].stream, per_track[s]);
    }
    printf("\n");

    StandardTextureAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    assert(standard_texture_analyze(&build.input, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK);
    printf("  %zu window(s), %zu passage(s), %zu change(s)\n", analysis.windows.count, analysis.passage_count, analysis.change_count);

    /* the measures */
    size_t scored = 0;
    size_t right = 0;
    size_t confusion[STANDARD_TEXTURE_COUNT][STANDARD_TEXTURE_COUNT];
    memset(confusion, 0, sizeof(confusion));
    for (size_t w = 0; w < analysis.windows.count; w++) {
        int measure = analysis.windows.items[w].measure;
        StandardTexture want;
        if (measure < annotation.range_first || measure > annotation.range_last || !annotated_category(&annotation, measure, &want)) {
            continue;
        }
        scored++;
        StandardTexture found_category = STANDARD_TEXTURE_MONOPHONY;
        assert(analysis.window_texture[w] >= 0);
        assert(standard_texture_category((StandardTexture)analysis.window_texture[w], &found_category));
        confusion[want][found_category]++;
        if (found_category == want) {
            right++;
        } else {
            const StandardTextureWindowFeatures *f = &analysis.windows.items[w];
            printf("    measure %d: annotated %s, found %s (%s%s); voices %d, synchrony %.2f, independence %.2f, prominence %.2f, gap %.1f, pattern %.2f\n",
                   measure, standard_texture_name(want), standard_texture_name(found_category),
                   standard_texture_name((StandardTexture)analysis.window_texture[w]),
                   analysis.classes[w].has_texture ? "" : ", taken from a neighbour", f->voices_max, f->synchrony, f->independence_mean,
                   f->prominence, f->top_gap, f->pattern_max);
        }
    }
    double rate = scored == 0 ? 0.0 : (double)right / (double)scored;
    printf("  measures with the right category: %zu of %zu (%.1f%%)\n", right, scored, 100.0 * rate);
    printf("  annotated -> found (category):\n");
    for (int want = 0; want < STANDARD_TEXTURE_COUNT; want++) {
        for (int got = 0; got < STANDARD_TEXTURE_COUNT; got++) {
            if (confusion[want][got] > 0) {
                printf("    %s -> %s: %zu\n", standard_texture_name((StandardTexture)want), standard_texture_name((StandardTexture)got),
                       confusion[want][got]);
            }
        }
    }

    /* the change points */
    size_t found_changes = 0;
    for (size_t c = 0; c < annotation.change_count; c++) {
        bool found = false;
        for (size_t i = 1; i < analysis.passage_count; i++) {
            int start = analysis.passages[i].claim.first_measure;
            found = found || (start >= annotation.changes[c] - 1 && start <= annotation.changes[c] + 1);
        }
        found_changes += found ? 1 : 0;
        if (!found) {
            printf("    missed change point at measure %d\n", annotation.changes[c]);
        }
    }
    size_t false_changes = 0;
    size_t false_by_category = 0;
    for (size_t i = 0; i < analysis.change_count; i++) {
        const StandardTextureChangeClaim *change = &analysis.changes[i].claim;
        bool annotated = false;
        for (size_t c = 0; c < annotation.change_count; c++) {
            annotated = annotated || (change->boundary_measure >= annotation.changes[c] - 1 &&
                                      change->boundary_measure <= annotation.changes[c] + 1);
        }
        if (!annotated) {
            false_changes++;
            false_by_category += (size_t)change->category_changed;
            printf("    false change point at measure %d: %s -> %s%s\n", change->boundary_measure,
                   standard_texture_name((StandardTexture)change->from_texture), standard_texture_name((StandardTexture)change->to_texture),
                   change->category_changed ? " (the category changes)" : "");
        }
    }
    printf("  change points found: %zu of %zu; false change points: %zu (%zu of them change the category)\n", found_changes,
           annotation.change_count, false_changes, false_by_category);

    double confidence = 0.0;
    for (size_t i = 0; i < analysis.passage_count; i++) {
        confidence += analysis.passages[i].claim.confidence;
    }
    printf("  mean passage confidence %.2f\n", analysis.passage_count == 0 ? 0.0 : confidence / (double)analysis.passage_count);

    assert(rate >= MEASURE_TARGET);
    assert(found_changes == annotation.change_count);

    standard_texture_analysis_destroy(&analysis);
    chain_texture_input_destroy(&build);
    chain_pitches_destroy(&pitches);
    pipeline_destroy(&p);
    printf("test_texture_field1: ok\n");
    return 0;
}
