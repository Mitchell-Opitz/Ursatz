#include <math.h>
#include <string.h>

#include "analysis_test_util.h"
#include "interpretation/texture.h"
#include "texture_analysis_classify.h"

static bool near(double a, double b) {
    return fabs(a - b) < 1e-9;
}

/* Measure m (1-based, 4/4) spans [m-1, m); a note from eighth a to eighth b of it. */
static void note(Piece *piece, int measure, size_t track, size_t voice, long pitch, int from_eighth, int to_eighth, double confidence) {
    long base = (long)(measure - 1) * 8;
    piece_note(piece, track, voice, pitch, base + from_eighth, 8, base + to_eighth, 8, confidence);
}

/* Block chords: three voices on the half notes. */
static void block_chords(Piece *piece, int measure) {
    for (int chord = 0; chord < 2; chord++) {
        note(piece, measure, 0, 0, 72 - 2 * chord, 4 * chord, 4 * chord + 4, 1.0);
        note(piece, measure, 0, 1, 64 - 2 * chord, 4 * chord, 4 * chord + 4, 1.0);
        note(piece, measure, 1, 0, 48 - 2 * chord, 4 * chord, 4 * chord + 4, 1.0);
    }
}

/* A melody of two half notes over an arpeggio of eighth notes: 48 55 64 55 in the left hand. */
static void melody_over_arpeggio(Piece *piece, int measure, double confidence) {
    note(piece, measure, 0, 0, 72, 0, 4, confidence);
    note(piece, measure, 0, 0, 74, 4, 8, confidence);
    const long arpeggio[4] = {48, 55, 64, 55};
    for (int i = 0; i < 8; i++) {
        note(piece, measure, 1, 0, arpeggio[i % 4], i, i + 1, confidence);
    }
}

/* One line of quarter notes. */
static void single_line(Piece *piece, int measure, double confidence) {
    const long walk[4] = {60, 62, 64, 65};
    for (int i = 0; i < 4; i++) {
        note(piece, measure, 0, 0, walk[i], 2 * i, 2 * i + 2, confidence);
    }
}

/* Two lines of the same density that never attack together: quarter notes, and quarter notes an eighth later. */
static void two_lines(Piece *piece, int measure) {
    const long upper[4] = {72, 71, 72, 74};
    const long lower[4] = {55, 53, 52, 55};
    for (int i = 0; i < 4; i++) {
        note(piece, measure, 0, 0, upper[i], 2 * i, 2 * i + 2, 1.0);
        note(piece, measure, 1, 0, lower[i], 2 * i + 1, 2 * i + 3 > 8 ? 8 : 2 * i + 3, 1.0);
    }
}

static StandardTextureAnalysis analyze(Piece *piece, const StandardTextureConfig *config) {
    StandardTextureAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    assert(standard_texture_analyze(&piece->input, config, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK);
    return analysis;
}

static StandardTexture only_texture(Piece *piece) {
    StandardTextureAnalysis analysis = analyze(piece, NULL);
    assert(analysis.passage_count == 1);
    StandardTexture texture = (StandardTexture)analysis.passages[0].claim.texture;
    standard_texture_analysis_destroy(&analysis);
    return texture;
}

static void test_monophony(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    single_line(&piece, 1, 1.0);
    assert(only_texture(&piece) == STANDARD_TEXTURE_MONOPHONY);
    piece_destroy(&piece);
}

static void test_homophony_of_block_chords(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    block_chords(&piece, 1);
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(analysis.passage_count == 1 && analysis.passages[0].claim.texture == STANDARD_TEXTURE_HOMORHYTHM);
    assert(analysis.passages[0].claim.category == STANDARD_TEXTURE_HOMOPHONY);
    assert(near(analysis.windows.items[0].synchrony, 1.0) && analysis.classes[0].features.rhythmic_unison);
    assert(near(analysis.classes[0].margin, 1.0) && near(analysis.passages[0].claim.confidence, 1.0));
    assert(analysis.passages[0].texture != NULL && strcmp(texture_label(analysis.passages[0].texture), "homorhythm") == 0);
    assert(analysis.passages[0].span != NULL && analysis.change_count == 0);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_melody_and_accompaniment(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    melody_over_arpeggio(&piece, 1, 1.0);
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(analysis.passage_count == 1 && analysis.passages[0].claim.texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT);
    const StandardTextureWindowFeatures *w = &analysis.windows.items[0];
    assert(w->voices_max == 2 && near(w->synchrony, 0.25) && near(w->prominence, 4.0) && near(w->top_gap, 25.0));
    assert(w->pattern_max > 0.5); /* the arpeggio figure repeats */
    assert(analysis.classes[0].features.predominant_melody && !analysis.classes[0].features.independent_lines);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_two_independent_lines(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    two_lines(&piece, 1);
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(analysis.passage_count == 1 && analysis.passages[0].claim.texture == STANDARD_TEXTURE_POLYPHONY);
    assert(analysis.passages[0].claim.category == STANDARD_TEXTURE_POLYPHONY);
    assert(near(analysis.windows.items[0].synchrony, 0.0) && near(analysis.windows.items[0].independence_mean, 1.0));
    assert(analysis.classes[0].features.independent_lines && !analysis.classes[0].features.predominant_melody);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_a_change_from_homophony_to_melody_and_accompaniment(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    block_chords(&piece, 1);
    melody_over_arpeggio(&piece, 2, 1.0);
    melody_over_arpeggio(&piece, 3, 1.0);
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(analysis.windows.count == 3 && analysis.passage_count == 2 && analysis.change_count == 1);
    assert(analysis.passages[0].claim.first_measure == 1 && analysis.passages[0].claim.last_measure == 1 &&
           analysis.passages[0].claim.texture == STANDARD_TEXTURE_HOMORHYTHM);
    assert(analysis.passages[1].claim.first_measure == 2 && analysis.passages[1].claim.last_measure == 3 &&
           analysis.passages[1].claim.texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT);
    /* the boundary is the barline between measures 1 and 2: one whole note */
    assert(analysis.passages[1].claim.start_n == 1 && analysis.passages[1].claim.start_d == 1);
    assert(analysis.passages[0].claim.end_n == 1 && analysis.passages[0].claim.end_d == 1);
    const StandardTextureChangeClaim *change = &analysis.changes[0].claim;
    assert(change->from == 0 && change->to == 1 && change->boundary_measure == 2);
    assert(change->from_texture == STANDARD_TEXTURE_HOMORHYTHM && change->to_texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT);
    assert(change->category_changed == 0); /* both are homophony */
    assert(analysis.changes[0].relationship != NULL && analysis.changes[0].interpretation != NULL);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);

    /* from a single line to block chords the category changes too */
    piece_init(&piece, 4, 2);
    single_line(&piece, 1, 1.0);
    block_chords(&piece, 2);
    analysis = analyze(&piece, NULL);
    assert(analysis.passage_count == 2 && analysis.changes[0].claim.category_changed == 1);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_confidence_follows_the_voice_confidence(void) {
    Piece sure;
    piece_init(&sure, 4, 2);
    single_line(&sure, 1, 1.0);
    StandardTextureAnalysis a = analyze(&sure, NULL);
    Piece unsure;
    piece_init(&unsure, 4, 2);
    single_line(&unsure, 1, 0.4);
    StandardTextureAnalysis b = analyze(&unsure, NULL);
    assert(near(a.passages[0].claim.confidence, 1.0) && near(b.passages[0].claim.confidence, 0.4));
    assert(near(b.passages[0].claim.voice_confidence, 0.4) && near(b.passages[0].uncertainty, 0.6));
    standard_texture_analysis_destroy(&a);
    standard_texture_analysis_destroy(&b);
    piece_destroy(&sure);
    piece_destroy(&unsure);
}

static void test_confidence_follows_the_margin(void) {
    /* synchrony exactly on the threshold (three of four attack times shared): a coin flip, 0.5 */
    Piece piece;
    piece_init(&piece, 4, 2);
    for (int i = 0; i < 4; i++) {
        note(&piece, 1, 0, 0, 72, 2 * i, 2 * i + 2, 1.0);
    }
    for (int i = 0; i < 3; i++) {
        note(&piece, 1, 1, 0, 48, 2 * i, 2 * i + 2, 1.0);
    }
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(near(analysis.windows.items[0].synchrony, 0.75) && analysis.classes[0].has_texture &&
           analysis.classes[0].texture == STANDARD_TEXTURE_HOMORHYTHM);
    assert(near(analysis.classes[0].margin, 0.0) && near(analysis.classes[0].texture_confidence, 0.5));
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_a_window_without_notes_does_not_break_a_passage(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    single_line(&piece, 1, 1.0);
    single_line(&piece, 3, 1.0); /* nothing in measure 2 */
    StandardTextureAnalysis analysis = analyze(&piece, NULL);
    assert(analysis.windows.count == 3 && analysis.passage_count == 1 && analysis.change_count == 0);
    assert(analysis.passages[0].claim.first_measure == 1 && analysis.passages[0].claim.last_measure == 3 &&
           analysis.passages[0].claim.windows == 3 && analysis.passages[0].claim.unclassified_windows == 1);
    assert(analysis.window_texture[1] == (int)STANDARD_TEXTURE_MONOPHONY && !analysis.classes[1].has_texture);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);

    /* an empty window between two textures goes with the one before it */
    piece_init(&piece, 4, 2);
    single_line(&piece, 1, 1.0);
    block_chords(&piece, 3);
    analysis = analyze(&piece, NULL);
    assert(analysis.passage_count == 2 && analysis.passages[0].claim.last_measure == 2 && analysis.passages[1].claim.first_measure == 3);
    standard_texture_analysis_destroy(&analysis);
    piece_destroy(&piece);
}

static void test_the_features_of_a_window_by_the_thresholds(void) {
    StandardTextureConfig config = standard_texture_config_default();
    StandardTextureWindowFeatures w;
    memset(&w, 0, sizeof(w));
    StandardTextureFeatures f;
    w.events = 4;
    w.voices_max = 2;
    w.pair_count = 1;
    w.synchrony = 0.4;
    w.independence_mean = 0.8;
    w.prominence = 1.0;
    assert(standard_texture_features_of_window(&w, &config, &f));
    assert(f.voice_count == 2 && f.independent_lines && !f.rhythmic_unison && !f.predominant_melody && !f.shared_melody);
    w.prominence = 2.0; /* a clear difference of density, but no gap: not a melody yet */
    w.top_gap = 2.0;
    standard_texture_features_of_window(&w, &config, &f);
    assert(f.independent_lines && !f.predominant_melody);
    w.top_gap = 5.0;
    standard_texture_features_of_window(&w, &config, &f);
    assert(f.predominant_melody && !f.independent_lines); /* a melody stands out: the lines are not of equal prominence */
    w.synchrony = 0.9;
    w.prominence = 1.0;
    standard_texture_features_of_window(&w, &config, &f);
    assert(f.rhythmic_unison && !f.independent_lines);
    w.synchrony = 0.6; /* between the thresholds: no texture */
    w.independence_mean = 0.4;
    StandardTextureWindowClass c;
    assert(standard_texture_classify_window(&w, &config, &c) && !c.has_texture && near(c.confidence, 0.0));
    memset(&w, 0, sizeof(w));
    assert(standard_texture_classify_window(&w, &config, &c) && !c.has_texture);
    assert(!standard_texture_features_of_window(NULL, &config, &f) && f.voice_count == 1);
}

static void test_no_notes_and_bad_calls(void) {
    StandardTextureAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    Piece piece;
    piece_init(&piece, 4, 2);
    assert(standard_texture_analyze(&piece.input, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK && analysis.passage_count == 0);
    standard_texture_analysis_destroy(&analysis);
    assert(standard_texture_analyze(NULL, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_texture_analyze(&piece.input, NULL, NULL) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardTextureConfig bad = standard_texture_config_default();
    bad.independence_min = 0.0;
    assert(standard_texture_analyze(&piece.input, &bad, &analysis) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    piece_destroy(&piece);
}

int main(void) {
    test_monophony();
    test_homophony_of_block_chords();
    test_melody_and_accompaniment();
    test_two_independent_lines();
    test_a_change_from_homophony_to_melody_and_accompaniment();
    test_confidence_follows_the_voice_confidence();
    test_confidence_follows_the_margin();
    test_a_window_without_notes_does_not_break_a_passage();
    test_the_features_of_a_window_by_the_thresholds();
    test_no_notes_and_bad_calls();
    printf("test_analysis_classify (standard-texture): ok\n");
    return 0;
}
