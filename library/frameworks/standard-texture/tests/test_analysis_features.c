#include <math.h>
#include <string.h>

#include "analysis_test_util.h"

static bool near(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static void test_the_window(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent inside = event(1, 4, 1, 2, 60, 0, 1.0);
    StandardTextureEvent before = event(-1, 2, 0, 1, 60, 0, 1.0); /* released at the window's start: not sounding in it */
    StandardTextureEvent across = event(-1, 2, 1, 4, 60, 0, 1.0);
    StandardTextureEvent after = event(1, 1, 5, 4, 60, 0, 1.0);
    assert(standard_texture_event_in_window(&inside, &window) && standard_texture_event_in_window(&across, &window));
    assert(!standard_texture_event_in_window(&before, &window) && !standard_texture_event_in_window(&after, &window));
    assert(near(standard_texture_window_beats(&window), 4.0));
}

static void test_voice_counts(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent two[2] = {event(0, 1, 1, 1, 72, 0, 1.0), event(0, 1, 1, 2, 60, 1, 1.0)};
    int most = 0;
    double mean = 0.0;
    standard_texture_feature_voice_counts(two, 2, &window, &most, &mean);
    assert(most == 2 && near(mean, 1.5)); /* two for half the measure, one for the rest */
    StandardTextureEvent rest[1] = {event(0, 1, 1, 2, 72, 0, 1.0)};
    standard_texture_feature_voice_counts(rest, 1, &window, &most, &mean);
    assert(most == 1 && near(mean, 0.5)); /* the rest counts 0 */
    standard_texture_feature_voice_counts(NULL, 0, &window, &most, &mean);
    assert(most == 0 && near(mean, 0.0));
}

static void test_synchrony(void) {
    StandardTextureWindow window = window_four_four();
    /* block chords: three voices attack together twice */
    StandardTextureEvent chords[6] = {event(0, 1, 1, 2, 72, 0, 1.0), event(0, 1, 1, 2, 64, 1, 1.0), event(0, 1, 1, 2, 48, 2, 1.0),
                                      event(1, 2, 1, 1, 71, 0, 1.0), event(1, 2, 1, 1, 62, 1, 1.0), event(1, 2, 1, 1, 43, 2, 1.0)};
    size_t times = 0;
    assert(near(standard_texture_feature_synchrony(chords, 6, &window, &times), 1.0) && times == 2);
    /* a held melody note over four accompaniment attacks: one of four attack times is shared */
    StandardTextureEvent melody[5] = {event(0, 1, 1, 1, 72, 0, 1.0), event(0, 1, 1, 4, 48, 1, 1.0), event(1, 4, 1, 2, 55, 1, 1.0),
                                      event(1, 2, 3, 4, 52, 1, 1.0), event(3, 4, 1, 1, 55, 1, 1.0)};
    assert(near(standard_texture_feature_synchrony(melody, 5, &window, &times), 0.25) && times == 4);
    /* one voice, or none, shares nothing */
    assert(near(standard_texture_feature_synchrony(melody + 1, 4, &window, &times), 0.0) && times == 4);
    assert(near(standard_texture_feature_synchrony(NULL, 0, &window, &times), 0.0) && times == 0);
}

static void test_independence(void) {
    StandardTextureWindow window = window_four_four();
    /* a attacks at 0 and 1/2, b at 1/4 and 3/4: no shared attack, independence 1 */
    StandardTextureEvent apart[4] = {event(0, 1, 1, 4, 60, 0, 1.0), event(1, 2, 3, 4, 60, 0, 1.0), event(1, 4, 1, 2, 48, 1, 1.0),
                                     event(3, 4, 1, 1, 48, 1, 1.0)};
    StandardTexturePair pairs[4];
    double mean = -1.0;
    double least = -1.0;
    assert(standard_texture_feature_independence(apart, 4, &window, pairs, 4, &mean, &least) == 1);
    assert(pairs[0].first == 0 && pairs[0].second == 1 && near(pairs[0].independence, 1.0) && near(mean, 1.0) && near(least, 1.0));
    /* the same attacks: 0 */
    StandardTextureEvent together[4] = {event(0, 1, 1, 2, 60, 0, 1.0), event(1, 2, 1, 1, 60, 0, 1.0), event(0, 1, 1, 2, 48, 1, 1.0),
                                        event(1, 2, 1, 1, 48, 1, 1.0)};
    assert(standard_texture_feature_independence(together, 4, &window, pairs, 4, &mean, &least) == 1 && near(mean, 0.0));
    /* a at {0, 1/2}, b at {0, 1/4}: one shared of three attack times, 1 - 1/3 */
    StandardTextureEvent partial[4] = {event(0, 1, 1, 4, 60, 0, 1.0), event(1, 2, 1, 1, 60, 0, 1.0), event(0, 1, 1, 4, 48, 1, 1.0),
                                       event(1, 4, 1, 2, 48, 1, 1.0)};
    assert(standard_texture_feature_independence(partial, 4, &window, pairs, 4, &mean, &least) == 1 && near(mean, 2.0 / 3.0));
    /* three voices: three pairs, the mean and the least */
    StandardTextureEvent three[6] = {event(0, 1, 1, 4, 72, 0, 1.0), event(1, 2, 3, 4, 72, 0, 1.0), event(1, 4, 1, 2, 60, 1, 1.0),
                                     event(3, 4, 1, 1, 60, 1, 1.0), event(0, 1, 1, 4, 48, 2, 1.0), event(1, 2, 3, 4, 48, 2, 1.0)};
    assert(standard_texture_feature_independence(three, 6, &window, pairs, 4, &mean, &least) == 3);
    assert(near(least, 0.0) && near(mean, 2.0 / 3.0)); /* voices 0 and 2 attack together; each is apart from voice 1 */
    /* a voice that only sustains is not a pair */
    StandardTextureEvent held[3] = {event(0, 1, 1, 1, 72, 0, 1.0), event(0, 1, 1, 4, 48, 1, 1.0), event(1, 4, 1, 2, 48, 1, 1.0)};
    assert(standard_texture_feature_independence(held, 3, &window, pairs, 4, &mean, &least) == 1 && near(mean, 0.5));
    assert(standard_texture_feature_independence(held + 1, 2, &window, pairs, 4, &mean, &least) == 0 && near(mean, 0.0));
}

static void test_prominence(void) {
    StandardTextureWindow window = window_four_four();
    /* a held top note over four accompaniment attacks: densities 1/4 and 1, ratio 1/4, prominence 4, a gap of 12 */
    StandardTextureEvent melody[5] = {event(0, 1, 1, 1, 72, 0, 1.0), event(0, 1, 1, 4, 60, 1, 1.0), event(1, 4, 1, 2, 60, 1, 1.0),
                                      event(1, 2, 3, 4, 60, 1, 1.0), event(3, 4, 1, 1, 60, 1, 1.0)};
    double ratio = 0.0;
    double gap = 0.0;
    size_t top = 9;
    double prominence = standard_texture_feature_prominence(melody, 5, &window, &ratio, &gap, &top);
    assert(near(prominence, 4.0) && near(ratio, 0.25) && near(gap, 12.0) && top == 0);
    /* the same rhythm in both: prominence 1 */
    StandardTextureEvent equal[4] = {event(0, 1, 1, 2, 72, 0, 1.0), event(1, 2, 1, 1, 72, 0, 1.0), event(0, 1, 1, 2, 60, 1, 1.0),
                                     event(1, 2, 1, 1, 60, 1, 1.0)};
    assert(near(standard_texture_feature_prominence(equal, 4, &window, &ratio, &gap, &top), 1.0) && near(gap, 12.0));
    /* one voice has no prominence; no voice neither */
    assert(near(standard_texture_feature_prominence(equal, 2, &window, &ratio, &gap, &top), 0.0) && near(gap, 0.0));
    assert(near(standard_texture_feature_prominence(NULL, 0, &window, &ratio, &gap, &top), 0.0));
    /* a top voice that moves over a bass held from before the window: the capped ratio */
    StandardTextureEvent moving[3] = {event(0, 1, 1, 2, 72, 0, 1.0), event(1, 2, 1, 1, 74, 0, 1.0), event(-1, 1, 1, 1, 48, 1, 1.0)};
    assert(near(standard_texture_feature_prominence(moving, 3, &window, &ratio, &gap, &top), STANDARD_TEXTURE_PROMINENCE_CAP));
}

static void test_register(void) {
    StandardTextureWindow window = window_four_four();
    /* 72 held, 60 until 1/2, then 67: a span of 12; the gaps at 0 (12) and 1/2 (5) */
    StandardTextureEvent voices[3] = {event(0, 1, 1, 1, 72, 0, 1.0), event(0, 1, 1, 2, 60, 1, 1.0), event(1, 2, 1, 1, 67, 1, 1.0)};
    long span = 0;
    double spacing = standard_texture_feature_register(voices, 3, &window, &span);
    assert(span == 12 && near(spacing, 8.5));
    /* a note that began before the window counts at its start */
    StandardTextureEvent carried[2] = {event(-1, 2, 1, 2, 64, 0, 1.0), event(1, 4, 1, 1, 55, 1, 1.0)};
    spacing = standard_texture_feature_register(carried, 2, &window, &span);
    assert(span == 9 && near(spacing, 9.0));
    assert(near(standard_texture_feature_register(NULL, 0, &window, &span), 0.0) && span == 0);
}

static void test_density(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent quarters[4] = {event(0, 1, 1, 4, 60, 0, 1.0), event(1, 4, 1, 2, 60, 0, 1.0), event(1, 2, 3, 4, 60, 0, 1.0),
                                        event(3, 4, 1, 1, 60, 0, 1.0)};
    assert(near(standard_texture_feature_density(quarters, 4, &window), 1.0));
    /* a second voice on the same attack times adds none */
    StandardTextureEvent doubled[8];
    for (size_t i = 0; i < 4; i++) {
        doubled[i] = quarters[i];
        doubled[i + 4] = quarters[i];
        doubled[i + 4].voice = 1;
    }
    assert(near(standard_texture_feature_density(doubled, 8, &window), 1.0));
    assert(near(standard_texture_feature_density(NULL, 0, &window), 0.0));
}

static void test_pattern(void) {
    StandardTextureWindow window = window_four_four();
    window.beat = tq(1, 2);
    /* eight eighths of a repeated figure 60 64 67 64 | 60 64 67 64: the cell (+4 +3 -3) repeats, covering 6 of 7 pairs */
    const long pitches[8] = {60, 64, 67, 64, 60, 64, 67, 64};
    StandardTextureEvent figure[8];
    for (long i = 0; i < 8; i++) {
        figure[i] = event(i, 8, i + 1, 8, pitches[i], 0, 1.0);
    }
    double most = 0.0;
    size_t voices = 0;
    double mean = standard_texture_feature_pattern(figure, 8, &window, &most, &voices);
    assert(near(mean, 6.0 / 7.0) && near(most, 6.0 / 7.0) && voices == 1);
    /* a cell may not be longer than a beat: with a beat of one eighth only the one-pair cells can repeat (+4 twice, +3, -3, -4 once more) */
    window.beat = tq(1, 8);
    mean = standard_texture_feature_pattern(figure, 8, &window, &most, &voices);
    assert(near(mean, 2.0 / 7.0));
    /* a line that never repeats a step scores 0 */
    window.beat = tq(1, 2);
    const long walk[5] = {60, 61, 63, 66, 70};
    StandardTextureEvent line[5];
    for (long i = 0; i < 5; i++) {
        line[i] = event(i, 8, i + 1, 8, walk[i], 0, 1.0);
    }
    assert(near(standard_texture_feature_pattern(line, 5, &window, &most, &voices), 0.0) && voices == 1);
    /* a voice with fewer than three attacks is not scored */
    assert(near(standard_texture_feature_pattern(figure, 2, &window, &most, &voices), 0.0) && voices == 0);
    /* two voices: the mean and the maximum */
    StandardTextureEvent both[13];
    memcpy(both, figure, sizeof(figure));
    for (long i = 0; i < 5; i++) {
        both[8 + i] = line[i];
        both[8 + i].voice = 1;
    }
    mean = standard_texture_feature_pattern(both, 13, &window, &most, &voices);
    assert(voices == 2 && near(mean, 3.0 / 7.0) && near(most, 6.0 / 7.0));
}

static void test_unison(void) {
    StandardTextureWindow window = window_four_four();
    /* two voices an octave apart in the same rhythm: unison */
    StandardTextureEvent octave[4] = {event(0, 1, 1, 2, 72, 0, 1.0), event(1, 2, 1, 1, 74, 0, 1.0), event(0, 1, 1, 2, 60, 1, 1.0),
                                      event(1, 2, 1, 1, 62, 1, 1.0)};
    bool unison = false;
    double share = standard_texture_feature_unison(octave, 4, &window, 1.0, &unison);
    assert(near(share, 1.0) && unison);
    /* a fifth apart: not */
    StandardTextureEvent fifth[4] = {event(0, 1, 1, 2, 67, 0, 1.0), event(1, 2, 1, 1, 69, 0, 1.0), event(0, 1, 1, 2, 60, 1, 1.0),
                                     event(1, 2, 1, 1, 62, 1, 1.0)};
    share = standard_texture_feature_unison(fifth, 4, &window, 1.0, &unison);
    assert(near(share, 0.0) && !unison);
    /* the same pitches but not attacking together: not a unison window */
    share = standard_texture_feature_unison(octave, 4, &window, 0.5, &unison);
    assert(near(share, 1.0) && !unison);
    /* one voice has no unison */
    share = standard_texture_feature_unison(octave, 2, &window, 0.0, &unison);
    assert(near(share, 0.0) && !unison);
}

static void test_voice_confidence(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent events[3] = {event(0, 1, 1, 2, 72, 0, 0.8), event(1, 2, 1, 1, 72, 0, 0.6), event(-1, 2, 1, 4, 60, 1, 0.1)};
    assert(near(standard_texture_feature_confidence(events, 3, &window), 0.7)); /* the carried note does not attack here */
    assert(near(standard_texture_feature_confidence(NULL, 0, &window), 1.0));
}

static void test_all_features_of_a_window(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent line[4] = {event(0, 1, 1, 4, 60, 0, 0.9), event(1, 4, 1, 2, 62, 0, 0.9), event(1, 2, 3, 4, 64, 0, 0.9),
                                    event(3, 4, 1, 1, 65, 0, 0.9)};
    StandardTextureWindowFeatures f;
    standard_texture_measure_features(line, 4, &window, 7, &f);
    assert(f.measure == 7 && f.events == 4 && f.attacks == 4 && f.voices_max == 1 && near(f.voices_mean, 1.0));
    assert(near(f.synchrony, 0.0) && f.pair_count == 0 && near(f.prominence, 0.0) && near(f.density, 1.0) && f.register_span == 5);
    assert(near(f.voice_confidence, 0.9) && !f.unison);
}

static void test_voices_that_attack_in_turn_are_one_line(void) {
    StandardTextureWindow window = window_four_four();
    StandardTextureEvent merged[4];
    /* a melody that overlaps itself a little (the separation put it in two voices): one line */
    StandardTextureEvent split[3] = {event_in_track(0, 1, 1, 2, 72, 0, 0, 1.0), event_in_track(1, 2, 3, 4, 74, 1, 0, 1.0),
                                     event_in_track(3, 4, 1, 1, 71, 0, 0, 1.0)};
    assert(standard_texture_consolidate_voices(split, 3, &window, merged) == 3);
    assert(merged[0].voice == merged[1].voice && merged[1].voice == merged[2].voice);
    StandardTextureWindowFeatures f;
    standard_texture_measure_features(split, 3, &window, 1, &f);
    assert(f.voices_max == 1 && near(f.synchrony, 0.0) && f.pair_count == 0);
    /* an arpeggio whose notes ring into each other: attacks in turn, one line */
    StandardTextureEvent ringing[3] = {event_in_track(0, 1, 1, 2, 48, 0, 1, 1.0), event_in_track(1, 8, 1, 2, 55, 1, 1, 1.0),
                                       event_in_track(1, 4, 1, 2, 64, 2, 1, 1.0)};
    standard_texture_measure_features(ringing, 3, &window, 1, &f);
    assert(f.voices_max == 1);
    /* two voices of a hand that ever attack together stay two (a chord) */
    StandardTextureEvent chord[4] = {event_in_track(0, 1, 1, 2, 72, 0, 0, 1.0), event_in_track(0, 1, 1, 2, 64, 1, 0, 1.0),
                                     event_in_track(1, 2, 1, 1, 74, 0, 0, 1.0), event_in_track(1, 2, 1, 1, 65, 1, 0, 1.0)};
    standard_texture_measure_features(chord, 4, &window, 1, &f);
    assert(f.voices_max == 2 && near(f.synchrony, 1.0));
    /* voices of different tracks are never joined, however they alternate */
    StandardTextureEvent hands[2] = {event_in_track(0, 1, 1, 2, 72, 0, 0, 1.0), event_in_track(1, 2, 1, 1, 48, 1, 1, 1.0)};
    standard_texture_measure_features(hands, 2, &window, 1, &f);
    assert(f.voices_max == 1 && f.attack_times == 2); /* never sound together here, but two lines: no consolidation across tracks */
    assert(standard_texture_consolidate_voices(hands, 2, &window, merged) == 2 && merged[0].voice != merged[1].voice);
    /* a voice that does not attack in the window (a held note) is left alone */
    StandardTextureEvent held[2] = {event_in_track(-1, 2, 1, 2, 72, 0, 0, 1.0), event_in_track(0, 1, 1, 1, 60, 1, 0, 1.0)};
    standard_texture_measure_features(held, 2, &window, 1, &f);
    assert(f.voices_max == 2);
}

static void test_the_pattern_of_the_top_voice_and_of_the_others(void) {
    StandardTextureWindow window = window_four_four();
    /* a melody that repeats nothing over an arpeggio that repeats a cell */
    StandardTextureEvent events[12];
    const long melody[4] = {72, 74, 71, 76};
    for (long i = 0; i < 4; i++) {
        events[i] = event_in_track(i, 4, i + 1, 4, melody[i], 0, 0, 1.0);
    }
    const long arpeggio[8] = {48, 55, 64, 55, 48, 55, 64, 55};
    for (long i = 0; i < 8; i++) {
        events[4 + i] = event_in_track(i, 8, i + 1, 8, arpeggio[i], 1, 1, 1.0);
    }
    StandardTextureWindowFeatures f;
    standard_texture_measure_features(events, 12, &window, 1, &f);
    assert(f.voices_max == 2 && near(f.pattern_top, 0.0) && near(f.pattern_support, 4.0 / 7.0));
    assert(near(standard_texture_feature_voice_pattern(events, 12, &window, 0), 0.0));
    assert(near(standard_texture_feature_voice_pattern(events, 12, &window, 1), 4.0 / 7.0));
    /* a single line has no support */
    standard_texture_measure_features(events, 4, &window, 1, &f);
    assert(near(f.pattern_support, 0.0));
}

static void test_the_windows_of_an_input(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    /* measure 1: block chords of three voices on the half notes; measure 2: one line of quarter notes (a rest in measure 3's place) */
    for (int chord = 0; chord < 2; chord++) {
        piece_note(&piece, 0, 0, 72 - chord, chord, 2, chord + 1, 2, 0.9);
        piece_note(&piece, 0, 1, 64 - chord, chord, 2, chord + 1, 2, 0.9);
        piece_note(&piece, 1, 0, 48 - chord, chord, 2, chord + 1, 2, 0.9);
    }
    for (int i = 0; i < 4; i++) {
        piece_note(&piece, 0, 0, 60 + i, 4 + i, 4, 5 + i, 4, 0.5);
    }
    StandardTextureWindows windows = {NULL, 0};
    assert(standard_texture_measure_windows(&piece.input, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_OK);
    assert(windows.count == 2);
    assert(windows.items[0].measure == 1 && windows.items[0].voices_max == 3 && near(windows.items[0].synchrony, 1.0) &&
           windows.items[0].attack_times == 2 && near(windows.items[0].voice_confidence, 0.9));
    assert(windows.items[1].measure == 2 && windows.items[1].voices_max == 1 && near(windows.items[1].density, 1.0) &&
           near(windows.items[1].voice_confidence, 0.5));
    standard_texture_windows_destroy(&windows);
    piece_destroy(&piece);
}

static void test_a_performed_chord_is_one_attack(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    /* a chord rolled by 1/512 of a whole note: the snap reads it as one attack */
    piece_note(&piece, 0, 0, 72, 0, 1, 1, 2, 1.0);
    piece_note(&piece, 0, 1, 60, 1, 512, 1, 2, 1.0);
    StandardTextureWindows windows = {NULL, 0};
    assert(standard_texture_measure_windows(&piece.input, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_OK);
    assert(windows.count == 1 && windows.items[0].attack_times == 1 && near(windows.items[0].synchrony, 1.0));
    standard_texture_windows_destroy(&windows);
    piece_destroy(&piece);
}

static void test_bad_calls(void) {
    StandardTextureWindows windows = {NULL, 0};
    assert(standard_texture_measure_windows(NULL, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    Piece piece;
    piece_init(&piece, 4, 2);
    assert(standard_texture_measure_windows(&piece.input, NULL, NULL) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    StandardTextureConfig bad = standard_texture_config_default();
    bad.prominence_min = 1.0;
    assert(standard_texture_measure_windows(&piece.input, &bad, &windows) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    bad = standard_texture_config_default();
    bad.release_tolerance_denominator = 16; /* wider than half a grid step */
    assert(!standard_texture_config_valid(&bad));
    assert(standard_texture_input_add_note(&piece.input, "", piece_span(&piece, 0, 1, 1, 4), 60, 0, 0, 1.0) ==
           STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    assert(standard_texture_input_add_note(&piece.input, "x", NULL, 60, 0, 0, 1.0) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    assert(standard_texture_input_add_note(&piece.input, "x", piece_span(&piece, 0, 1, 1, 4), 60, 0, 0, 1.5) ==
           STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    assert(standard_texture_measure_windows(&piece.input, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_OK && windows.count == 0);
    standard_texture_windows_destroy(&windows);
    piece_destroy(&piece);
}

int main(void) {
    test_the_window();
    test_voice_counts();
    test_synchrony();
    test_independence();
    test_prominence();
    test_register();
    test_density();
    test_pattern();
    test_unison();
    test_voice_confidence();
    test_all_features_of_a_window();
    test_voices_that_attack_in_turn_are_one_line();
    test_the_pattern_of_the_top_voice_and_of_the_others();
    test_the_windows_of_an_input();
    test_a_performed_chord_is_one_attack();
    test_bad_calls();
    printf("test_analysis_features (standard-texture): ok\n");
    return 0;
}
