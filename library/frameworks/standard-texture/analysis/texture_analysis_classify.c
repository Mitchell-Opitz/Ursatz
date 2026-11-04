#include "texture_analysis_classify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "mixed_texture_rules.h"
#include "texture_analysis_claim.h"

static double clip(double x) {
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

static double least(double a, double b) {
    return a < b ? a : b;
}

bool standard_texture_features_of_window(const StandardTextureWindowFeatures *window, const StandardTextureConfig *config,
                                         StandardTextureFeatures *out) {
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->voice_count = 1;
    if (window == NULL) {
        return false;
    }
    StandardTextureConfig fallback = standard_texture_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    out->voice_count = window->voices_max < 1 ? 1 : window->voices_max;
    out->unison = window->unison;
    out->rhythmic_unison = out->voice_count >= 2 && window->synchrony >= config->rhythmic_unison_synchrony;
    bool above_a_figure = window->pattern_support >= config->pattern_min && window->pattern_top < window->pattern_support;
    out->predominant_melody = !out->unison && out->voice_count >= 2 && window->top_gap >= config->prominence_gap_semitones &&
                              (window->prominence >= config->prominence_min || above_a_figure);
    out->independent_lines = !out->unison && !out->predominant_melody && window->pair_count >= 1 &&
                             window->independence_mean >= config->independence_min &&
                             window->synchrony < config->independence_max_synchrony;
    out->shared_melody = false;
    return true;
}

/* How far the measurements that decided `texture` are from their thresholds, 0 to 1. */
static double margin_of(StandardTexture texture, const StandardTextureWindowFeatures *w, const StandardTextureConfig *c,
                        const StandardTextureFeatures *f) {
    switch (texture) {
    case STANDARD_TEXTURE_MONOPHONY:
        return 1.0;
    case STANDARD_TEXTURE_HOMORHYTHM:
        return clip((w->synchrony - c->rhythmic_unison_synchrony) / (1.0 - c->rhythmic_unison_synchrony + 1e-12));
    case STANDARD_TEXTURE_POLYPHONY: {
        double independence = clip((w->independence_mean - c->independence_min) / (1.0 - c->independence_min + 1e-12));
        double apart = clip((c->independence_max_synchrony - w->synchrony) / c->independence_max_synchrony);
        double by_prominence = clip((c->prominence_min - w->prominence) / (c->prominence_min - 1.0));
        double by_gap = c->prominence_gap_semitones > 0.0 ? clip((c->prominence_gap_semitones - w->top_gap) / c->prominence_gap_semitones)
                                                          : 0.0;
        double equal = by_prominence > by_gap ? by_prominence : by_gap;
        return least(least(independence, apart), equal);
    }
    case STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT: {
        double room = c->prominence_gap_semitones > 1.0 ? c->prominence_gap_semitones : 1.0;
        double by_prominence = clip((w->prominence - c->prominence_min) / c->prominence_min);
        double by_figure = clip((w->pattern_support - c->pattern_min) / (1.0 - c->pattern_min + 1e-12));
        double evidence = by_prominence > by_figure ? by_prominence : by_figure;
        return least(evidence, clip((w->top_gap - c->prominence_gap_semitones) / room));
    }
    default:
        (void)f;
        return 0.0;
    }
}

bool standard_texture_classify_window(const StandardTextureWindowFeatures *window, const StandardTextureConfig *config,
                                      StandardTextureWindowClass *out) {
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    StandardTextureConfig fallback = standard_texture_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (!standard_texture_features_of_window(window, config, &out->features)) {
        return false;
    }
    if (window->events == 0) {
        return true; /* nothing sounds: no texture */
    }
    StandardTexture texture;
    if (!standard_texture_classify(&out->features, &texture)) {
        return true;
    }
    out->has_texture = true;
    out->texture = texture;
    out->margin = clip(margin_of(texture, window, config, &out->features));
    out->texture_confidence = 0.5 + 0.5 * out->margin;
    out->confidence = out->texture_confidence * clip(window->voice_confidence);
    return true;
}

static void passage_ids(int index, char *out, size_t size) {
    snprintf(out, size, "standard-texture-passage-%d", index);
}

static StandardTextureAnalysisStatus build_passage(const StandardTextureAnalysis *analysis, int index, size_t first, size_t last,
                                                   StandardTexturePassage *out) {
    const StandardTextureWindowFeatures *windows = analysis->windows.items;
    StandardTexturePassageClaim *claim = &out->claim;
    memset(claim, 0, sizeof(*claim));
    claim->index = index;
    claim->first_measure = windows[first].measure;
    claim->last_measure = windows[last].measure;
    claim->texture = analysis->window_texture[first];
    StandardTexture category = STANDARD_TEXTURE_MONOPHONY;
    if (!standard_texture_category((StandardTexture)claim->texture, &category)) {
        category = (StandardTexture)claim->texture;
    }
    claim->category = (int)category;
    claim->start_n = windows[first].start.n;
    claim->start_d = windows[first].start.d;
    claim->end_n = windows[last].end.n;
    claim->end_d = windows[last].end.d;

    size_t classified = 0;
    size_t with_notes = 0;
    for (size_t w = first; w <= last; w++) {
        claim->windows++;
        const StandardTextureWindowClass *c = &analysis->classes[w];
        if (c->has_texture) {
            classified++;
            claim->confidence += c->confidence;
            claim->texture_confidence += c->texture_confidence;
            claim->voice_confidence += windows[w].voice_confidence;
        } else {
            claim->unclassified_windows++;
        }
        if (windows[w].events > 0) {
            with_notes++;
            claim->voices_max = windows[w].voices_max > claim->voices_max ? windows[w].voices_max : claim->voices_max;
            claim->voices_mean += windows[w].voices_mean;
            claim->synchrony += windows[w].synchrony;
            claim->independence += windows[w].independence_mean;
            claim->prominence += windows[w].prominence;
            claim->top_gap += windows[w].top_gap;
            claim->register_span += (double)windows[w].register_span;
            claim->spacing += windows[w].spacing;
            claim->density += windows[w].density;
            claim->pattern += windows[w].pattern_mean;
        }
    }
    if (classified > 0) {
        claim->confidence /= (double)classified;
        claim->texture_confidence /= (double)classified;
        claim->voice_confidence /= (double)classified;
    }
    if (with_notes > 0) {
        double n = (double)with_notes;
        claim->voices_mean /= n;
        claim->synchrony /= n;
        claim->independence /= n;
        claim->prominence /= n;
        claim->top_gap /= n;
        claim->register_span /= n;
        claim->spacing /= n;
        claim->density /= n;
        claim->pattern /= n;
    }
    out->uncertainty = 1.0 - clip(claim->confidence);
    out->span = standard_texture_make_span(windows[first].start, windows[last].end);

    char id[96];
    char interpretation_id[128];
    passage_ids(index, id, sizeof(id));
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-texture-passage-claim-%d", index);
    Interpretation *interpretation = standard_texture_analysis_interpretation(
        interpretation_id, id, STANDARD_TEXTURE_PASSAGE_CLAIM_TYPE, claim, sizeof(*claim), out->uncertainty, "texture-classification");
    if (out->span == NULL || interpretation == NULL) {
        interpretation_destroy(interpretation);
        return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    if (texture_create(interpretation, standard_texture_name((StandardTexture)claim->texture), &out->texture) != TEXTURE_OK) {
        interpretation_destroy(interpretation);
        out->texture = NULL;
        return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    return STANDARD_TEXTURE_ANALYSIS_OK;
}

static StandardTextureAnalysisStatus build_change(const StandardTextureAnalysis *analysis, int from, StandardTextureChange *out) {
    const StandardTexturePassageClaim *before = &analysis->passages[from].claim;
    const StandardTexturePassageClaim *after = &analysis->passages[from + 1].claim;
    memset(out, 0, sizeof(*out));
    out->claim.from = before->index;
    out->claim.to = after->index;
    out->claim.boundary_measure = after->first_measure;
    out->claim.from_texture = before->texture;
    out->claim.to_texture = after->texture;
    out->claim.category_changed = before->category != after->category ? 1 : 0;
    double confidence = least(before->confidence, after->confidence);
    out->uncertainty = 1.0 - clip(confidence);
    char source[96];
    char target[96];
    char key[48];
    char interpretation_id[128];
    passage_ids(before->index, source, sizeof(source));
    passage_ids(after->index, target, sizeof(target));
    snprintf(key, sizeof(key), "%d-%d", before->index, after->index);
    out->relationship = standard_texture_analysis_relationship("change", key, source, target, "changes-to");
    if (out->relationship == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-texture-change-claim-%s", key);
    out->interpretation = standard_texture_analysis_interpretation(
        interpretation_id, entity_id_value(relationship_id(out->relationship)), STANDARD_TEXTURE_CHANGE_CLAIM_TYPE, &out->claim,
        sizeof(out->claim), out->uncertainty, "texture-classification");
    return out->interpretation == NULL ? STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC : STANDARD_TEXTURE_ANALYSIS_OK;
}

static StandardTextureAnalysisStatus standard_texture_analyze_windows(const StandardTextureWindows *windows,
                                                               const StandardTextureConfig *config, StandardTextureAnalysis *out) {
    if (windows == NULL || out == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardTextureConfig fallback = standard_texture_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (!standard_texture_config_valid(config)) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_INVALID;
    }
    StandardTextureAnalysis found;
    memset(&found, 0, sizeof(found));
    size_t n = windows->count;
    if (n == 0) {
        *out = found;
        return STANDARD_TEXTURE_ANALYSIS_OK;
    }
    found.windows.items = (StandardTextureWindowFeatures *)malloc(n * sizeof(StandardTextureWindowFeatures));
    found.classes = (StandardTextureWindowClass *)calloc(n, sizeof(StandardTextureWindowClass));
    found.window_texture = (int *)malloc(n * sizeof(int));
    found.passages = (StandardTexturePassage *)calloc(n, sizeof(StandardTexturePassage));
    if (found.windows.items == NULL || found.classes == NULL || found.window_texture == NULL || found.passages == NULL) {
        standard_texture_analysis_destroy(&found);
        return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    memcpy(found.windows.items, windows->items, n * sizeof(StandardTextureWindowFeatures));
    found.windows.count = n;

    long first_texture = -1;
    for (size_t i = 0; i < n; i++) {
        standard_texture_classify_window(&found.windows.items[i], config, &found.classes[i]);
        found.window_texture[i] = found.classes[i].has_texture ? (int)found.classes[i].texture : -1;
        if (found.classes[i].has_texture && first_texture < 0) {
            first_texture = (long)found.classes[i].texture;
        }
    }
    if (first_texture < 0) {
        *out = found; /* no window has a texture: no passage */
        return STANDARD_TEXTURE_ANALYSIS_OK;
    }
    int carried = (int)first_texture;
    for (size_t i = 0; i < n; i++) {
        if (found.window_texture[i] < 0) {
            found.window_texture[i] = carried;
        } else {
            carried = found.window_texture[i];
        }
    }

    StandardTextureAnalysisStatus status = STANDARD_TEXTURE_ANALYSIS_OK;
    size_t start = 0;
    for (size_t i = 1; status == STANDARD_TEXTURE_ANALYSIS_OK && i <= n; i++) {
        if (i == n || found.window_texture[i] != found.window_texture[start]) {
            status = build_passage(&found, (int)found.passage_count, start, i - 1, &found.passages[found.passage_count]);
            found.passage_count++;
            start = i;
        }
    }

    /* the change points by standard-texture's rule, over the passages' ids */
    if (status == STANDARD_TEXTURE_ANALYSIS_OK && found.passage_count > 1) {
        EntityID **ids = (EntityID **)calloc(found.passage_count, sizeof(EntityID *));
        StandardTextureSegment *segments = (StandardTextureSegment *)calloc(found.passage_count, sizeof(StandardTextureSegment));
        size_t *changes = (size_t *)calloc(found.passage_count, sizeof(size_t));
        found.changes = (StandardTextureChange *)calloc(found.passage_count, sizeof(StandardTextureChange));
        if (ids == NULL || segments == NULL || changes == NULL || found.changes == NULL) {
            status = STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
        }
        for (size_t p = 0; status == STANDARD_TEXTURE_ANALYSIS_OK && p < found.passage_count; p++) {
            char id[96];
            passage_ids((int)p, id, sizeof(id));
            if (entity_id_create(id, &ids[p]) != ENTITY_ID_OK) {
                status = STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
                break;
            }
            segments[p].passage_id = ids[p];
            segments[p].texture = (StandardTexture)found.passages[p].claim.texture;
        }
        size_t change_count = 0;
        if (status == STANDARD_TEXTURE_ANALYSIS_OK) {
            change_count = standard_texture_change_points(segments, found.passage_count, false, changes, found.passage_count);
        }
        for (size_t c = 0; status == STANDARD_TEXTURE_ANALYSIS_OK && c < change_count && c < found.passage_count; c++) {
            status = build_change(&found, (int)changes[c] - 1, &found.changes[found.change_count]);
            found.change_count++;
        }
        if (ids != NULL) {
            for (size_t p = 0; p < found.passage_count; p++) {
                entity_id_destroy(ids[p]);
            }
        }
        free(ids);
        free(segments);
        free(changes);
    }
    if (status != STANDARD_TEXTURE_ANALYSIS_OK) {
        standard_texture_analysis_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_TEXTURE_ANALYSIS_OK;
}

StandardTextureAnalysisStatus standard_texture_analyze(const StandardTextureInput *input, const StandardTextureConfig *config,
                                                       StandardTextureAnalysis *out) {
    if (input == NULL || out == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardTextureWindows windows = {NULL, 0};
    StandardTextureAnalysisStatus status = standard_texture_measure_windows(input, config, &windows);
    if (status != STANDARD_TEXTURE_ANALYSIS_OK) {
        return status;
    }
    status = standard_texture_analyze_windows(&windows, config, out);
    standard_texture_windows_destroy(&windows);
    return status;
}

void standard_texture_analysis_destroy(StandardTextureAnalysis *analysis) {
    if (analysis == NULL) {
        return;
    }
    for (size_t i = 0; i < analysis->passage_count; i++) {
        time_span_destroy(analysis->passages[i].span);
        texture_destroy(analysis->passages[i].texture);
    }
    for (size_t i = 0; i < analysis->change_count; i++) {
        interpretation_destroy(analysis->changes[i].interpretation);
        relationship_destroy(analysis->changes[i].relationship);
    }
    free(analysis->windows.items);
    free(analysis->classes);
    free(analysis->window_texture);
    free(analysis->passages);
    free(analysis->changes);
    memset(analysis, 0, sizeof(*analysis));
}
