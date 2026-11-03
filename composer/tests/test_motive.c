#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * Motive input and analysis. The same motive is loaded from MIDI, MusicXML, a note list and JSON and must come out
 * identical; three motives are analyzed against expectations worked out by hand; bad input must give the documented
 * errors; and a loaded motive must attach to a plan as Given material and survive the plan's JSON.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name

static const PlanMeter FOUR_FOUR = {4, 4, 0};
static const PlanMeter THREE_FOUR = {3, 4, 0};
static const AnalysisKey D_MINOR = {NULL, PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};
static const AnalysisKey C_MAJOR = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
static const AnalysisKey G_MAJOR = {NULL, PITCH_LETTER_G, 0, KEY_STRUCTURE_MODE_MAJOR};

static MotiveOptions options_with(const PlanMeter *meter, const AnalysisKey *key) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    if (meter) {
        options.has_meter = true;
        options.meter = *meter;
    }
    if (key) {
        options.has_key = true;
        options.key = *key;
    }
    return options;
}

static bool time_is(Rational time, long num, long long den) {
    return time.num == num && time.den == den;
}

static bool note_is(const Motive *motive, size_t index, int pitch, long offset_num, unsigned long offset_den,
                    long duration_num, unsigned long duration_den) {
    const MotiveSketchNote *note = &motive->notes[index];
    return motive_pitch(motive, index) == pitch && time_is(note->onset, offset_num, offset_den) &&
           time_is(note->duration, duration_num, duration_den);
}

static Motive load_notes(const char *text, const PlanMeter *meter, const AnalysisKey *key) {
    Motive motive;
    MotiveOptions options = options_with(meter, key);
    MotiveError error;
    MotiveResult result = motive_from_notes_text(text, &options, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "'%s': %s at %s: %s\n", text, motive_result_name(result), error.location, error.message);
    }
    assert(result == MOTIVE_OK);
    return motive;
}

static MotiveAnalysis analyze(const Motive *motive) {
    MotiveAnalysis analysis;
    bool ok = motive_analyze(motive, &analysis);
    assert(ok);
    return analysis;
}

/* A failed load: the result, and the text that must appear in the location. */
static void expect_notes_error(const char *text, const PlanMeter *meter, MotiveResult expected, const char *location) {
    Motive motive;
    MotiveOptions options = options_with(meter, NULL);
    MotiveError error;
    MotiveResult result = motive_from_notes_text(text, &options, &motive, &error);
    if (result != expected || strstr(error.location, location) == NULL || error.message[0] == '\0') {
        fprintf(stderr, "'%s': got %s at '%s': %s\n", text, motive_result_name(result), error.location, error.message);
    }
    assert(result == expected);
    assert(strstr(error.location, location) != NULL);
    assert(error.message[0] != '\0');
}

static void expect_json_error(const char *text, const PlanMeter *meter, MotiveResult expected, const char *location) {
    Motive motive;
    MotiveOptions options = options_with(meter, NULL);
    MotiveError error;
    MotiveResult result = motive_from_json(text, &options, &motive, &error);
    if (result != expected || strstr(error.location, location) == NULL || error.message[0] == '\0') {
        fprintf(stderr, "'%s': got %s at '%s': %s\n", text, motive_result_name(result), error.location, error.message);
    }
    assert(result == expected);
    assert(strstr(error.location, location) != NULL);
    assert(error.message[0] != '\0');
}

static MotiveResult load_fixture(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                                 MotiveError *error) {
    return motive_load_file(path, format, options, out, error);
}

/* ------------------------------------------------------- the four formats */

/* A MusicXML note keeps the spelling it is written with (C sharp and D flat are one pitch); other formats write none. */
/* A motive holds pitches, not the written spelling of a MusicXML note (B6a): the sketch has no per-note spelling. */
static void test_musicxml_keeps_spelling(void) {
    Motive motive;
    Motive midi;
    MotiveError error;
    assert(load_fixture(FIXTURE("motive_spelled.musicxml"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) == MOTIVE_OK);
    assert(motive.note_count == 3 && motive.notes[0].pitch == 61 && motive.notes[1].pitch == 61);
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_AUTO, NULL, &midi, &error) == MOTIVE_OK);
}

static void test_same_motive_from_each_format(void) {
    MotiveOptions for_text = options_with(&FOUR_FOUR, &D_MINOR);
    Motive midi;
    Motive musicxml;
    Motive text;
    Motive json;
    MotiveError error;
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_AUTO, NULL, &midi, &error) == MOTIVE_OK);
    assert(load_fixture(FIXTURE("motive_d_minor.musicxml"), MOTIVE_FORMAT_AUTO, NULL, &musicxml, &error) == MOTIVE_OK);
    assert(load_fixture(FIXTURE("motive_d_minor.txt"), MOTIVE_FORMAT_AUTO, &for_text, &text, &error) == MOTIVE_OK);
    assert(load_fixture(FIXTURE("motive_d_minor.json"), MOTIVE_FORMAT_AUTO, NULL, &json, &error) == MOTIVE_OK);

    /* D4 quarter, F4 eighth, A4 eighth, D5 half, in 4/4, D minor. */
    assert(midi.note_count == 4);
    assert(note_is(&midi, 0, 62, 0, 1, 1, 4));
    assert(note_is(&midi, 1, 65, 1, 4, 1, 8));
    assert(note_is(&midi, 2, 69, 3, 8, 1, 8));
    assert(note_is(&midi, 3, 74, 1, 2, 1, 2));
    assert(time_is(midi.length, 1, 1));
    assert(motive_meter(&midi).beats_per_bar == 4 && motive_meter(&midi).beat_unit == 4);
    assert(midi.key_source == MOTIVE_SKETCH_KEY_GIVEN);
    assert(midi.tonic_pc == 2 && midi.minor);

    assert(motive_equal(&midi, &musicxml));
    assert(motive_equal(&midi, &text));
    assert(motive_equal(&midi, &json));
    assert(motive_equal(&musicxml, &text));
    assert(motive_equal(&musicxml, &json));
    assert(motive_equal(&text, &json));

    /* A different note is not equal. */
    Motive other = load_notes("D4:q F4:e A4:e C5:h", &FOUR_FOUR, &D_MINOR);
    assert(!motive_equal(&midi, &other));

    /* Each format by name, from memory, as the CLI loads it. */
    const char *text_a = "D4:q F4:e A4:e D5:h";
    Motive by_name;
    assert(motive_load(text_a, strlen(text_a), MOTIVE_FORMAT_NOTES, &for_text, &by_name, &error) == MOTIVE_OK);
    assert(motive_equal(&by_name, &midi));
    assert(motive_load(text_a, strlen(text_a), MOTIVE_FORMAT_AUTO, &for_text, &by_name, &error) == MOTIVE_OK);
    assert(motive_equal(&by_name, &midi));

    /* The format is detected from the bytes. */
    assert(motive_detect_format("MThd", 4) == MOTIVE_FORMAT_MIDI);
    assert(motive_detect_format("  <?xml", 7) == MOTIVE_FORMAT_MUSICXML);
    assert(motive_detect_format("\n{\"notes\"", 9) == MOTIVE_FORMAT_JSON);
    assert(motive_detect_format("D4:q", 4) == MOTIVE_FORMAT_NOTES);
    MotiveFormat format;
    assert(motive_format_parse("musicxml", &format) && format == MOTIVE_FORMAT_MUSICXML);
    assert(!motive_format_parse("xml", &format));
}

static void test_options(void) {
    /* A key in the options wins over the file's key signature. */
    MotiveOptions bossa = options_with(NULL, &C_MAJOR);
    Motive motive;
    MotiveError error;
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_MIDI, &bossa, &motive, &error) == MOTIVE_OK);
    assert(motive.tonic_pc == 0 && !motive.minor);
    assert(motive.key_source == MOTIVE_SKETCH_KEY_GIVEN);

    /* The same meter as the file's is fine; a different one changes the bars and is refused. */
    MotiveOptions same = options_with(&FOUR_FOUR, NULL);
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_MIDI, &same, &motive, &error) == MOTIVE_OK);
    MotiveOptions different = options_with(&THREE_FOUR, NULL);
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_MIDI, &different, &motive, &error) ==
           MOTIVE_ERR_METER);
    assert(strcmp(error.location, "meter") == 0);
    assert(strstr(error.message, "3/4") != NULL && strstr(error.message, "4/4") != NULL);
    assert(load_fixture(FIXTURE("motive_d_minor.musicxml"), MOTIVE_FORMAT_MUSICXML, &different, &motive, &error) ==
           MOTIVE_ERR_METER);
    assert(load_fixture(FIXTURE("motive_d_minor.json"), MOTIVE_FORMAT_JSON, &different, &motive, &error) ==
           MOTIVE_ERR_METER);

    /* A note list has no meter of its own. */
    MotiveOptions none = options_with(NULL, NULL);
    assert(motive_from_notes_text("D4:q", &none, &motive, &error) == MOTIVE_ERR_METER);
    assert(motive_from_notes_text("D4:q", NULL, &motive, &error) == MOTIVE_ERR_METER);
    assert(strcmp(error.location, "meter") == 0);

    AnalysisKey key;
    assert(motive_parse_key("Bb minor", &key) && analysis_key_tonic_pitch_class(&key) == 10 && key.mode == KEY_STRUCTURE_MODE_MINOR);
    assert(motive_parse_key("F# major", &key) && analysis_key_tonic_pitch_class(&key) == 6 && key.mode == KEY_STRUCTURE_MODE_MAJOR);
    assert(!motive_parse_key("H major", &key));
    assert(!motive_parse_key("D dorian", &key));
    PlanMeter meter;
    assert(motive_parse_meter("6/8", &meter) && meter.beats_per_bar == 6 && meter.beat_unit == 8);
    assert(!motive_parse_meter("4/5", &meter));
    assert(!motive_parse_meter("4", &meter));
}

/* ------------------------------------------------------------- analysis */

static bool near(double a, double b) {
    return (a > b ? a - b : b - a) < 1e-9;
}

typedef struct {
    const char *roman;
    HarmonicFunction function;
    double fit;
    size_t tones;
} Expected;

static void expect_candidates(const MotiveAnalysis *a, const Expected *expected, size_t count) {
    assert(a->harmony_known && a->candidate_count == count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(a->candidates[i].roman, expected[i].roman) != 0) {
            fprintf(stderr, "candidate %zu: '%s', expected '%s'\n", i, a->candidates[i].roman, expected[i].roman);
        }
        assert(strcmp(a->candidates[i].roman, expected[i].roman) == 0);
        assert(a->candidates[i].has_function && a->candidates[i].function == expected[i].function);
        assert(near(a->candidates[i].fit, expected[i].fit));
        assert(a->candidates[i].chord_tones == expected[i].tones);
        assert(a->candidates[i].chord_tones + a->candidates[i].non_chord_tones == a->note_count);
    }
}

/* D4:q F4:e A4:e D5:h in D minor, 4/4. */
static void test_analysis_ascending_arpeggio(void) {
    Motive motive = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    MotiveAnalysis a = analyze(&motive);

    assert(a.note_count == 4 && a.interval_count == 3);
    assert(time_is(a.length, 1, 1) && time_is(a.bars, 1, 1));
    assert(a.intervals[0].semitones == 3 && a.intervals[1].semitones == 4 && a.intervals[2].semitones == 5);
    assert(a.intervals[0].has_steps && a.intervals[0].steps == 2);
    assert(a.intervals[1].steps == 2 && a.intervals[2].steps == 3);
    assert(a.directions[0] == CONTOUR_DIRECTION_UP && a.directions[1] == CONTOUR_DIRECTION_UP &&
           a.directions[2] == CONTOUR_DIRECTION_UP);
    assert(a.shape == MOTIVE_SHAPE_ASCENDING);

    assert(a.lowest == 62 && a.highest == 74 && a.range_semitones == 12);

    assert(a.degrees_known);
    assert(a.degrees[0].degree == 1 && a.degrees[1].degree == 3 && a.degrees[2].degree == 5 &&
           a.degrees[3].degree == 1);
    assert(a.degrees[0].alteration == 0 && a.degrees[3].alteration == 0);

    /* Onsets 0, 1/4, 3/8 and 1/2 are beats 0, 1, 3/2 and 2 of the quarter-note beat. */
    assert(a.rhythm[0].bar == 0 && time_is(a.rhythm[0].beat, 0, 1) && a.rhythm[0].on_beat);
    assert(time_is(a.rhythm[1].beat, 1, 1) && a.rhythm[1].on_beat);
    assert(time_is(a.rhythm[2].beat, 3, 2) && !a.rhythm[2].on_beat);
    assert(time_is(a.rhythm[3].beat, 2, 1) && a.rhythm[3].on_beat);
    for (size_t i = 0; i < 4; i++) {
        assert(!a.rhythm[i].dotted);
        assert(time_is(a.rhythm[i].rest_before, 0, 1));
    }

    /*
     * The notes weigh 1/2 (D on the downbeat counts twice), 1/8, 1/8 and 1/2: 5/4 in all. The tonic triad D F A has
     * all of it; VI (Bb D F) has D, F and D, 9/10; iv (G Bb D) has both Ds, 4/5; III (F A C) has F and A, 1/5; V and
     * V7 have only A, 1/10, and V comes first.
     */
    static const Expected EXPECTED[] = {{"i", HARMONIC_FUNCTION_TONIC, 1.0, 4},
                                        {"VI", HARMONIC_FUNCTION_PREDOMINANT, 0.9, 3},
                                        {"iv", HARMONIC_FUNCTION_PREDOMINANT, 0.8, 2},
                                        {"III", HARMONIC_FUNCTION_PREDOMINANT, 0.2, 2},
                                        {"V", HARMONIC_FUNCTION_DOMINANT, 0.1, 1},
                                        {"V7", HARMONIC_FUNCTION_DOMINANT, 0.1, 1}};
    expect_candidates(&a, EXPECTED, 6);
}

/* An eighth rest, then G4:e C5:q. B4:e in C major, 3/4: a rest and a dotted value. */
static void test_analysis_rest_and_dotted(void) {
    Motive motive = load_notes("r:e G4:e C5:q. B4:e", &THREE_FOUR, &C_MAJOR);
    assert(motive.note_count == 3);
    assert(note_is(&motive, 0, 67, 1, 8, 1, 8));
    assert(note_is(&motive, 1, 72, 1, 4, 3, 8));
    assert(note_is(&motive, 2, 71, 5, 8, 1, 8));
    assert(time_is(motive.length, 3, 4));

    MotiveAnalysis a = analyze(&motive);
    assert(time_is(a.bars, 1, 1));
    assert(a.interval_count == 2 && a.intervals[0].semitones == 5 && a.intervals[1].semitones == -1);
    assert(a.intervals[0].steps == 3 && a.intervals[1].steps == -1);
    assert(a.directions[0] == CONTOUR_DIRECTION_UP && a.directions[1] == CONTOUR_DIRECTION_DOWN);
    assert(a.shape == MOTIVE_SHAPE_ARCH);
    assert(a.lowest == 67 && a.highest == 72 && a.range_semitones == 5);

    /* G (5) up to C (1) down to B (7): it starts on 5 and ends on 7. */
    assert(a.degrees[0].degree == 5 && a.degrees[1].degree == 1 && a.degrees[2].degree == 7);

    /* The rest is the gap before the first note; the dotted quarter is a dotted value. */
    assert(time_is(a.rhythm[0].rest_before, 1, 8) && time_is(a.rhythm[0].onset, 1, 8));
    assert(time_is(a.rhythm[0].beat, 1, 2) && !a.rhythm[0].on_beat && !a.rhythm[0].dotted);
    assert(time_is(a.rhythm[1].beat, 1, 1) && a.rhythm[1].on_beat && a.rhythm[1].dotted);
    assert(time_is(a.rhythm[1].rest_before, 0, 1));
    assert(time_is(a.rhythm[2].beat, 5, 2) && !a.rhythm[2].on_beat && !a.rhythm[2].dotted);

    char name[16];
    motive_duration_name(a.rhythm[1].duration, name, sizeof(name));
    assert(strcmp(name, "q.") == 0);
    motive_duration_name(a.rhythm[0].rest_before, name, sizeof(name));
    assert(strcmp(name, "e") == 0);
    Rational odd = {5, 16};
    motive_duration_name(odd, name, sizeof(name));
    assert(strcmp(name, "5/16") == 0);

    /*
     * No note is on a downbeat, so the notes weigh 1/8, 3/8 and 1/8: 5/8. I (C E G) has G and C, 4/5; IV (F A C) and
     * vi (A C E) have C, 3/5; V (G B D), iii (E G B) and V7 have G and B, 2/5; vii (B D F) has B, 1/5. Equal fits
     * keep the order I, V, IV, vi, ii, iii, vii, V7.
     */
    static const Expected EXPECTED[] = {{"I", HARMONIC_FUNCTION_TONIC, 0.8, 2},
                                        {"IV", HARMONIC_FUNCTION_PREDOMINANT, 0.6, 1},
                                        {"vi", HARMONIC_FUNCTION_PREDOMINANT, 0.6, 1},
                                        {"V", HARMONIC_FUNCTION_DOMINANT, 0.4, 2},
                                        {"iii", HARMONIC_FUNCTION_PREDOMINANT, 0.4, 2},
                                        {"V7", HARMONIC_FUNCTION_DOMINANT, 0.4, 2},
                                        {"viio", HARMONIC_FUNCTION_DOMINANT, 0.2, 1}};
    expect_candidates(&a, EXPECTED, 7);
}

/* G4:q B4:q~ B4:e A4:e G4:q in G major, 4/4: a tie across a quarter and an eighth. */
static void test_analysis_tie(void) {
    Motive motive = load_notes("G4:q B4:q~ B4:e A4:e G4:q", &FOUR_FOUR, &G_MAJOR);
    /* The tied pair is one note as long as both. */
    assert(motive.note_count == 4);
    assert(note_is(&motive, 0, 67, 0, 1, 1, 4));
    assert(note_is(&motive, 1, 71, 1, 4, 3, 8));
    assert(note_is(&motive, 2, 69, 5, 8, 1, 8));
    assert(note_is(&motive, 3, 67, 3, 4, 1, 4));
    assert(time_is(motive.length, 1, 1));

    MotiveAnalysis a = analyze(&motive);
    assert(a.interval_count == 3);
    assert(a.intervals[0].semitones == 4 && a.intervals[1].semitones == -2 && a.intervals[2].semitones == -2);
    assert(a.intervals[0].steps == 2 && a.intervals[1].steps == -1 && a.intervals[2].steps == -1);
    assert(a.shape == MOTIVE_SHAPE_ARCH);
    assert(a.lowest == 67 && a.highest == 71 && a.range_semitones == 4);
    assert(a.degrees[0].degree == 1 && a.degrees[1].degree == 3 && a.degrees[2].degree == 2 &&
           a.degrees[3].degree == 1);

    assert(time_is(a.rhythm[1].beat, 1, 1) && a.rhythm[1].dotted);
    assert(time_is(a.rhythm[2].beat, 5, 2) && !a.rhythm[2].on_beat);
    assert(time_is(a.rhythm[3].beat, 3, 1) && a.rhythm[3].on_beat);

    /*
     * The notes weigh 1/2 (G on the downbeat counts twice), 3/8, 1/8 and 1/4: 5/4. I (G B D) and vi (E G B) have G, B
     * and G, 9/10; IV (C E G) has both Gs, 3/5; iii (B D F#) has B, 3/10; V, ii, vii and V7 have only A, 1/10.
     */
    static const Expected EXPECTED[] = {{"I", HARMONIC_FUNCTION_TONIC, 0.9, 3},
                                        {"vi", HARMONIC_FUNCTION_PREDOMINANT, 0.9, 3},
                                        {"IV", HARMONIC_FUNCTION_PREDOMINANT, 0.6, 2},
                                        {"iii", HARMONIC_FUNCTION_PREDOMINANT, 0.3, 1},
                                        {"V", HARMONIC_FUNCTION_DOMINANT, 0.1, 1},
                                        {"ii", HARMONIC_FUNCTION_PREDOMINANT, 0.1, 1},
                                        {"viio", HARMONIC_FUNCTION_DOMINANT, 0.1, 1},
                                        {"V7", HARMONIC_FUNCTION_DOMINANT, 0.1, 1}};
    expect_candidates(&a, EXPECTED, 8);
}

static void test_analysis_without_a_key(void) {
    Motive motive = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, NULL);
    assert(motive.key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED);
    MotiveAnalysis a = analyze(&motive);
    assert(!a.key_known && !a.degrees_known && !a.harmony_known && a.candidate_count == 0);
    assert(a.interval_count == 3 && a.intervals[0].semitones == 3 && !a.intervals[0].has_steps);
    assert(a.shape == MOTIVE_SHAPE_ASCENDING && a.range_semitones == 12);

    /* The one-note motive has no intervals and no shape. */
    Motive single = load_notes("C4:w", &FOUR_FOUR, NULL);
    MotiveAnalysis one = analyze(&single);
    assert(one.interval_count == 0 && one.shape == MOTIVE_SHAPE_SINGLE);

    /* A level motive and a valley. */
    Motive level = load_notes("E4:q E4:q E4:q", &FOUR_FOUR, NULL);
    assert(analyze(&level).shape == MOTIVE_SHAPE_LEVEL);
    Motive valley = load_notes("G4:q E4:q C4:q E4:q", &FOUR_FOUR, NULL);
    assert(analyze(&valley).shape == MOTIVE_SHAPE_VALLEY);
    Motive wave = load_notes("C4:q E4:q D4:q F4:q", &FOUR_FOUR, NULL);
    assert(analyze(&wave).shape == MOTIVE_SHAPE_WAVE);

    /* A pitch outside the key is the degree below it, raised: F# in C major is a raised 4. */
    Motive chromatic = load_notes("C4:q F#4:q G4:q", &FOUR_FOUR, &C_MAJOR);
    MotiveAnalysis c = analyze(&chromatic);
    assert(c.degrees[1].degree == 4 && c.degrees[1].alteration == 1);
    assert(c.intervals[0].steps == 3 && c.intervals[1].steps == 1);
}

/* All seven degrees of D natural minor are there, and so are the degrees of its relative, F major. */
static void test_key_is_inferred_only_when_the_pitches_settle(void) {
    Motive scale = load_notes("D4:q E4:q F4:q G4:q A4:q Bb4:q C5:q D5:q", &FOUR_FOUR, NULL);
    assert(scale.key_source == MOTIVE_SKETCH_KEY_INFERRED);
    assert(scale.tonic_pc == 2 && scale.minor);
    Motive major = load_notes("F4:q G4:q A4:q Bb4:q C5:q D5:q E5:q F5:q", &FOUR_FOUR, NULL);
    assert(major.key_source == MOTIVE_SKETCH_KEY_INFERRED);
    assert(major.tonic_pc == 5 && !major.minor);
    /* A given key is never replaced. */
    Motive given = load_notes("D4:q E4:q F4:q G4:q A4:q Bb4:q C5:q D5:q", &FOUR_FOUR, &C_MAJOR);
    assert(given.key_source == MOTIVE_SKETCH_KEY_GIVEN && given.tonic_pc == 0);
    /* Seven notes of a scale with no note to settle the relative keys stay open. */
    Motive open = load_notes("C4:q D4:q E4:q F4:q G4:q A4:q B4:q C5:q", &FOUR_FOUR, NULL);
    assert(open.key_source == MOTIVE_SKETCH_KEY_INFERRED && open.tonic_pc == 0 &&
           !open.minor);
    Motive undecided = load_notes("C4:q D4:q E4:q F4:q G4:q A4:q B4:q", &FOUR_FOUR, NULL);
    assert(undecided.key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED);
}

static void test_analysis_output(void) {
    Motive motive = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    MotiveAnalysis a = analyze(&motive);
    MotiveAnalysis again = analyze(&motive);

    char *json = motive_analysis_to_json(&motive, &a);
    char *json_again = motive_analysis_to_json(&motive, &again);
    assert(json && json_again && strcmp(json, json_again) == 0);
    assert(strstr(json, "\"shape\": \"ascending\"") != NULL);
    assert(strstr(json, "\"roman_numeral\": \"i\"") != NULL);
    assert(strstr(json, "\"fit\": 0.9") != NULL);
    assert(strstr(json, "\"source\": \"given\"") != NULL);
    assert(json[strlen(json) - 1] == '\n');
    free(json);
    free(json_again);

    Motive no_key = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, NULL);
    MotiveAnalysis none = analyze(&no_key);
    char *unkeyed = motive_analysis_to_json(&no_key, &none);
    assert(unkeyed && strstr(unkeyed, "\"steps\": null") != NULL && strstr(unkeyed, "\"scale_degrees\": null") != NULL);
    free(unkeyed);

    FILE *file = tmpfile();
    assert(file);
    motive_analysis_print(&motive, &a, file);
    rewind(file);
    char text[4096];
    size_t length = fread(text, 1, sizeof(text) - 1, file);
    text[length] = '\0';
    fclose(file);
    assert(strstr(text, "Notes: D4 F4 A4 D5") != NULL);
    assert(strstr(text, "Meter: 4/4") != NULL);
    assert(strstr(text, "Key: D minor (given)") != NULL);
    assert(strstr(text, "Contour: ascending (up, up, up)") != NULL);
    assert(strstr(text, "Range: D4 to D5 (12 semitones)") != NULL);
    assert(strstr(text, "1. i (tonic) fits 100%") != NULL);

    file = tmpfile();
    assert(file);
    motive_analysis_print(&no_key, &none, file);
    rewind(file);
    length = fread(text, 1, sizeof(text) - 1, file);
    text[length] = '\0';
    fclose(file);
    assert(strstr(text, "Key: unspecified") != NULL);
    assert(strstr(text, "Implied harmony: unknown (no key)") != NULL);
}

/* ----------------------------------------------------------------- JSON */

static void test_json_round_trip(void) {
    Motive motives[5];
    motives[0] = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    motives[1] = load_notes("r:e G4:e C5:q. B4:e", &THREE_FOUR, &C_MAJOR);
    motives[2] = load_notes("G4:q B4:q~ B4:e A4:e G4:q", &FOUR_FOUR, &G_MAJOR);
    motives[3] = load_notes("D4:q E4:q F4:q G4:q A4:q Bb4:q C5:q D5:q", &FOUR_FOUR, NULL); /* inferred key */
    motives[4] = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, NULL);                    /* no key */
    for (size_t i = 0; i < 5; i++) {
        char *json = motive_to_json(&motives[i]);
        assert(json);
        Motive back;
        MotiveError error;
        assert(motive_from_json(json, NULL, &back, &error) == MOTIVE_OK);
        assert(motive_equal(&motives[i], &back));
        free(json);
    }
    /* The fixture is what the writer writes, apart from layout. */
    Motive fixture;
    MotiveError error;
    assert(load_fixture(FIXTURE("motive_d_minor.json"), MOTIVE_FORMAT_JSON, NULL, &fixture, &error) == MOTIVE_OK);
    assert(motive_equal(&fixture, &motives[0]));
}

/* ------------------------------------------------------------- bad input */

static void test_note_list_errors(void) {
    expect_notes_error("D4 F4:q", &FOUR_FOUR, MOTIVE_ERR_SYNTAX, "token 1 (column 1)");
    expect_notes_error("D4:q Z4:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2 (column 6)");
    expect_notes_error("D4:q E4:x", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2");
    expect_notes_error("D4:q E:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2");
    expect_notes_error("D4:q:e", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 1");
    expect_notes_error("D4:q...", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 1");
    expect_notes_error("D4:q Ebbb4:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2");
    expect_notes_error("G10:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 1");
    expect_notes_error("D4:q :q", &FOUR_FOUR, MOTIVE_ERR_SYNTAX, "token 2");
    expect_notes_error("D4:q~", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 1");
    expect_notes_error("D4:q~ E4:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2");
    expect_notes_error("D4:q~ r:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2");
    expect_notes_error("r:q~ D4:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 1");
    expect_notes_error("D4:q r:q", &FOUR_FOUR, MOTIVE_ERR_VALUE, "token 2 (column 6)");
    expect_notes_error("", &FOUR_FOUR, MOTIVE_ERR_EMPTY, "");
    expect_notes_error(" | | ", &FOUR_FOUR, MOTIVE_ERR_EMPTY, "");
    expect_notes_error("r:q", &FOUR_FOUR, MOTIVE_ERR_EMPTY, "");
    expect_notes_error("C4:q", NULL, MOTIVE_ERR_METER, "meter");
    expect_notes_error("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:q", &FOUR_FOUR, MOTIVE_ERR_SYNTAX, "token 1");

    char many[65 * 6 + 1];
    many[0] = '\0';
    for (int i = 0; i < 65; i++) {
        strcat(many, "C4:s ");
    }
    expect_notes_error(many, &FOUR_FOUR, MOTIVE_ERR_TOO_MANY_NOTES, "token 65");

    /* Barlines are ignored, a tie joins across one, and dots and accidentals read as written. */
    Motive motive = load_notes("C4:h | D4:q~ | D4:q Bb4:e. C##4:s", &FOUR_FOUR, NULL);
    assert(motive.note_count == 4);
    assert(note_is(&motive, 0, 60, 0, 1, 1, 2));
    assert(note_is(&motive, 1, 62, 1, 2, 1, 2));
    assert(note_is(&motive, 2, 70, 1, 1, 3, 16));
    assert(note_is(&motive, 3, 62, 19, 16, 1, 16));
}

static void test_json_errors(void) {
    const char *meter = "\"meter\":{\"beats_per_bar\":4,\"beat_unit\":4},";
    char text[512];

    expect_json_error("{", &FOUR_FOUR, MOTIVE_ERR_SYNTAX, "byte");
    expect_json_error("not json", &FOUR_FOUR, MOTIVE_ERR_SYNTAX, "byte");
    expect_json_error("[]", &FOUR_FOUR, MOTIVE_ERR_VALUE, "");
    snprintf(text, sizeof(text), "{%s\"notes\":[]}", meter);
    expect_json_error(text, NULL, MOTIVE_ERR_VALUE, "notes");
    expect_json_error("{\"meter\":{\"beats_per_bar\":4,\"beat_unit\":4}}", NULL, MOTIVE_ERR_VALUE, "notes");
    snprintf(text, sizeof(text),
             "{%s\"notes\":[{\"midi_pitch\":60,\"offset\":{\"num\":0,\"den\":1},\"duration\":{\"num\":1,\"den\":4}},"
             "{\"midi_pitch\":62,\"offset\":{\"num\":1,\"den\":8},\"duration\":{\"num\":1,\"den\":4}}]}",
             meter);
    expect_json_error(text, NULL, MOTIVE_ERR_VALUE, "notes[1].offset");
    snprintf(text, sizeof(text),
             "{%s\"notes\":[{\"midi_pitch\":200,\"offset\":{\"num\":0,\"den\":1},\"duration\":{\"num\":1,\"den\":4}}]}",
             meter);
    expect_json_error(text, NULL, MOTIVE_ERR_VALUE, "notes[0].midi_pitch");
    snprintf(text, sizeof(text),
             "{%s\"notes\":[{\"midi_pitch\":60,\"offset\":{\"num\":0,\"den\":1},\"duration\":{\"num\":1,\"den\":0}}]}",
             meter);
    expect_json_error(text, NULL, MOTIVE_ERR_VALUE, "notes[0].duration.den");
    snprintf(text, sizeof(text),
             "{%s\"key\":{\"tonic_pitch_class\":2,\"mode\":\"dorian\"},\"notes\":[{\"midi_pitch\":60,"
             "\"offset\":{\"num\":0,\"den\":1},\"duration\":{\"num\":1,\"den\":4}}]}",
             meter);
    expect_json_error(text, NULL, MOTIVE_ERR_VALUE, "key.mode");
    snprintf(text, sizeof(text),
             "{\"notes\":[{\"midi_pitch\":60,\"offset\":{\"num\":0,\"den\":1},\"duration\":{\"num\":1,\"den\":4}}]}");
    expect_json_error(text, NULL, MOTIVE_ERR_METER, "meter");
}

static void test_file_errors(void) {
    Motive motive;
    MotiveError error;

    /* Notes sounding together are polyphony, whatever the format. */
    assert(load_fixture(FIXTURE("motive_polyphonic.mid"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) ==
           MOTIVE_ERR_POLYPHONIC);
    assert(strstr(error.location, "note 2") != NULL && strstr(error.message, "one voice") != NULL);
    assert(load_fixture(FIXTURE("motive_polyphonic.musicxml"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) ==
           MOTIVE_ERR_POLYPHONIC);
    assert(strstr(error.location, "note 2") != NULL);

    /* So is more than one part: none is chosen for the user and none dropped. */
    assert(load_fixture(FIXTURE("motive_two_parts.musicxml"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) ==
           MOTIVE_ERR_POLYPHONIC);
    assert(strstr(error.message, "more than one part") != NULL);

    /* A file the importer refuses. */
    assert(load_fixture(FIXTURE("motive_truncated.mid"), MOTIVE_FORMAT_MIDI, NULL, &motive, &error) ==
           MOTIVE_ERR_IMPORT);
    assert(strcmp(error.location, "midi") == 0 && error.message[0] != '\0');
    assert(load_fixture(FIXTURE("motive_malformed.musicxml"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) ==
           MOTIVE_ERR_IMPORT);
    assert(strcmp(error.location, "musicxml") == 0 && error.message[0] != '\0');

    /* A format the bytes are not. */
    assert(load_fixture(FIXTURE("motive_d_minor.txt"), MOTIVE_FORMAT_MIDI, NULL, &motive, &error) == MOTIVE_ERR_IMPORT);
    assert(load_fixture(FIXTURE("motive_d_minor.txt"), MOTIVE_FORMAT_MUSICXML, NULL, &motive, &error) ==
           MOTIVE_ERR_IMPORT);
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_JSON, NULL, &motive, &error) != MOTIVE_OK);
    assert(load_fixture(FIXTURE("motive_d_minor.mid"), MOTIVE_FORMAT_NOTES, NULL, &motive, &error) != MOTIVE_OK);

    assert(load_fixture(FIXTURE("no_such_file.mid"), MOTIVE_FORMAT_AUTO, NULL, &motive, &error) == MOTIVE_ERR_IO);
    assert(motive_load_file(NULL, MOTIVE_FORMAT_AUTO, NULL, &motive, &error) == MOTIVE_ERR_NULL);

    /* Nothing crashes on any bytes in any format, and nothing is accepted that is not a motive. */
    static const char *const GARBAGE[] = {"", "\xFF\xFF\xFF", "MThd", "MThd\0\0\0\6\0\0\0\1\1\340MTrk\0\0\0\4\0\0\0\0",
                                          "<score-partwise>", "<?xml", "{\"notes\": 5}", "{{{{", "~~~~", ":::", "D4:q~~"};
    for (size_t i = 0; i < sizeof(GARBAGE) / sizeof(GARBAGE[0]); i++) {
        size_t size = i == 3 ? 26 : strlen(GARBAGE[i]);
        for (int format = MOTIVE_FORMAT_AUTO; format <= MOTIVE_FORMAT_JSON; format++) {
            MotiveResult result = motive_load(GARBAGE[i], size, (MotiveFormat)format, NULL, &motive, &error);
            assert(result != MOTIVE_OK);
            assert(error.message[0] != '\0');
        }
    }
}

/* ---------------------------------------------------------------- attach */

static Rational whole(long num, long long den) {
    Rational time;
    RationalResult result = rational_make(num, den, &time);
    assert(result == RATIONAL_OK);
    return time;
}

static void test_attach_as_given(void) {
    PiecePlan *plan = plan_create();
    NodeId form;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanSpan four = {whole(0, 1), whole(4, 1)};
    PlanSpan two = {whole(0, 1), whole(2, 1)};
    PlanSpan one = {whole(0, 1), whole(1, 1)};
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, four, "piece", &form) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, four, "A", &section) == PLAN_OK);
    assert(plan_add_node(plan, section, PLAN_NODE_PHRASE, two, NULL, &phrase) == PLAN_OK);
    assert(plan_add_node(plan, phrase, PLAN_NODE_IDEA, one, "basic idea", &idea) == PLAN_OK);

    Motive motive = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    size_t nodes = plan_node_count(plan);
    size_t spans = plan_lane_span_count(plan);

    /* Refused: nothing is added. */
    assert(motive_attach(NULL, idea, whole(0, 1), &motive, NULL, NULL, NULL) == PLAN_ERR_NULL);
    assert(motive_attach(plan, 999, whole(0, 1), &motive, NULL, NULL, NULL) == PLAN_ERR_NOT_FOUND);
    assert(motive_attach(plan, phrase, whole(0, 1), &motive, NULL, NULL, NULL) == PLAN_ERR_BAD_KIND);
    assert(motive_attach(plan, idea, whole(1, 2), &motive, NULL, NULL, NULL) == PLAN_ERR_BAD_SPAN);
    Motive empty;
    memset(&empty, 0, sizeof(empty));
    assert(motive_attach(plan, idea, whole(0, 1), &empty, NULL, NULL, NULL) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_node_count(plan) == nodes && plan_lane_span_count(plan) == spans);

    NodeId node = PLAN_NODE_NONE;
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(motive_attach(plan, idea, whole(0, 1), &motive, "motive", &node, &span) == PLAN_OK);
    assert(plan_node_count(plan) == nodes + 1 && plan_lane_span_count(plan) == spans + 1);

    /* A motive node with the motive's span, Given. */
    assert(plan_node_kind(plan, node) == PLAN_NODE_MOTIVE && plan_node_parent(plan, node) == idea);
    assert(strcmp(plan_node_name(plan, node), "motive") == 0);
    assert(time_is(plan_node_span(plan, node).start, 0, 1) && time_is(plan_node_span(plan, node).length, 1, 1));
    assert(plan_node_state(plan, node) == PLAN_STATE_GIVEN);

    /* A Given melody lane span holding the notes. */
    LaneSpanId owned[2];
    assert(plan_node_lane_spans(plan, node, owned, 2) == 1 && owned[0] == span);
    assert(plan_lane_span_lane(plan, span) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, span) == 0);
    assert(plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN);
    const PlanLanePayload *payload = plan_lane_span_payload(plan, span);
    assert(payload->notes.note_count == 4);
    for (size_t i = 0; i < 4; i++) {
        assert(payload->notes.notes[i].midi == motive.notes[i].pitch);
        assert(rational_cmp(payload->notes.notes[i].offset, motive.notes[i].onset) == 0);
        assert(rational_cmp(payload->notes.notes[i].duration, motive.notes[i].duration) == 0);
    }

    /* Nothing changes it later. */
    assert(plan_node_set_state(plan, node, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_CONSTRAINED) == PLAN_ERR_GIVEN_LOCKED);
    PlanNote other = plan_note(60, whole(0, 1), whole(1, 4));
    PlanLanePayload rewrite;
    rewrite.notes.notes = &other;
    rewrite.notes.note_count = 1;
    assert(plan_lane_span_write(plan, span, &rewrite) == PLAN_ERR_GIVEN_LOCKED);
    assert(plan_lane_span_payload(plan, span)->notes.note_count == 4);

    /* The melody lane is taken there. */
    assert(motive_attach(plan, idea, whole(0, 1), &motive, NULL, NULL, NULL) == PLAN_ERR_LANE_OVERLAP);
    assert(plan_node_count(plan) == nodes + 1);

    /* It survives the plan's JSON, still Given, and writes the same text again. */
    PlanJsonError json_error;
    char *json = plan_to_json(plan, &json_error);
    assert(json);
    PiecePlan *copy = NULL;
    assert(plan_from_json(json, &copy, &json_error) == PLAN_OK);
    assert(plan_node_kind(copy, node) == PLAN_NODE_MOTIVE && plan_node_state(copy, node) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_state(copy, span) == PLAN_STATE_GIVEN && plan_lane_span_lane(copy, span) == PLAN_LANE_MELODY);
    const PlanLanePayload *read = plan_lane_span_payload(copy, span);
    assert(read->notes.note_count == 4);
    for (size_t i = 0; i < 4; i++) {
        assert(read->notes.notes[i].midi == motive.notes[i].pitch);
        assert(rational_cmp(read->notes.notes[i].offset, motive.notes[i].onset) == 0);
        assert(rational_cmp(read->notes.notes[i].duration, motive.notes[i].duration) == 0);
    }
    assert(plan_node_set_state(copy, node, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    char *again = plan_to_json(copy, &json_error);
    assert(again && strcmp(json, again) == 0);
    free(json);
    free(again);
    plan_destroy(copy);
    plan_destroy(plan);
}

/* ------------------------------------------------------- degrees, rhythm, contour */

typedef MotiveResult (*Reader)(const char *, const MotiveOptions *, Motive *, MotiveError *);

static Motive load_with(Reader reader, const char *text, const PlanMeter *meter, const AnalysisKey *key) {
    Motive motive;
    MotiveOptions options = options_with(meter, key);
    MotiveError error;
    MotiveResult result = reader(text, &options, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "'%s': %s at %s: %s\n", text, motive_result_name(result), error.location, error.message);
    }
    assert(result == MOTIVE_OK);
    return motive;
}

static void expect_read_error(Reader reader, const char *text, const AnalysisKey *key, MotiveResult code,
                              const char *location, const char *message) {
    Motive motive;
    MotiveOptions options = options_with(&FOUR_FOUR, key);
    MotiveError error;
    MotiveResult result = reader(text, &options, &motive, &error);
    if (result != code || strcmp(error.location, location) != 0 || strstr(error.message, message) == NULL) {
        fprintf(stderr, "'%s': got %s at '%s': %s\n", text, motive_result_name(result), error.location, error.message);
    }
    assert(result == code);
    assert(strcmp(error.location, location) == 0);
    assert(strstr(error.message, message) != NULL);
}

static void expect_text(char *text, const char *expected) {
    assert(text);
    if (strcmp(text, expected) != 0) {
        fprintf(stderr, "expected '%s', got '%s'\n", expected, text);
    }
    assert(strcmp(text, expected) == 0);
    free(text);
}

static void test_degrees_reading(void) {
    /* D minor: b3 is the third (F) lowered to E; the octave mark moves up 12. */
    Motive motive = load_with(motive_from_degrees_text, "1:q b3:e 5:e 1':h", &FOUR_FOUR, &D_MINOR);
    assert(motive.note_count == 4 && motive.form == MOTIVE_SKETCH_FORM_FULL);
    assert(note_is(&motive, 0, 62, 0, 1, 1, 4));
    assert(note_is(&motive, 1, 64, 1, 4, 1, 8));
    assert(note_is(&motive, 2, 69, 3, 8, 1, 8));
    assert(note_is(&motive, 3, 74, 1, 2, 1, 2));
    assert(motive.key_source == MOTIVE_SKETCH_KEY_GIVEN && motive.minor);

    /* Octave 0 puts the tonic in MIDI 60 to 71: G is 67. Marks are any number, up or down. */
    motive = load_with(motive_from_degrees_text, "1:q 1,:q 1':q 1'':q 1,,:q 1',:q", &FOUR_FOUR, &G_MAJOR);
    assert(motive.notes[0].pitch == 67 && motive.notes[1].pitch == 55 && motive.notes[2].pitch == 79);
    assert(motive.notes[3].pitch == 91 && motive.notes[4].pitch == 43 && motive.notes[5].pitch == 67);

    /* Degrees are counted in the natural minor scale; # and b alter them. */
    motive = load_with(motive_from_degrees_text, "3:q 6:q 7:q #4:q b7:q b2:q", &FOUR_FOUR, &D_MINOR);
    assert(motive.notes[0].pitch == 65 && motive.notes[1].pitch == 70 && motive.notes[2].pitch == 72);
    assert(motive.notes[3].pitch == 68 && motive.notes[4].pitch == 71 && motive.notes[5].pitch == 63);

    /* Rests, ties, dots and barlines are as in a note list. */
    motive = load_with(motive_from_degrees_text, "r:e 1:q.~ 1:s | 5:q", &FOUR_FOUR, &C_MAJOR);
    assert(motive.note_count == 2 && note_is(&motive, 0, 60, 1, 8, 7, 16) && note_is(&motive, 1, 67, 9, 16, 1, 4));

    /* With no key the pitches are C major's and the key is left as the pitches settle it. */
    motive = load_with(motive_from_degrees_text, "1:q 3:q 5:q", &FOUR_FOUR, NULL);
    assert(motive.notes[0].pitch == 60 && motive.notes[1].pitch == 64 && motive.notes[2].pitch == 67);
    assert(motive.key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED);

    /* The degrees format is read by motive_load, and auto-detection never picks it. */
    MotiveOptions options = options_with(&FOUR_FOUR, &C_MAJOR);
    MotiveError error;
    Motive loaded;
    assert(motive_load("1:q 5:q", 7, MOTIVE_FORMAT_DEGREES, &options, &loaded, &error) == MOTIVE_OK);
    assert(loaded.notes[1].pitch == 67);
    assert(motive_detect_format("1:q 5:q", 7) == MOTIVE_FORMAT_NOTES);
    MotiveFormat format;
    assert(motive_format_parse("degrees", &format) && format == MOTIVE_FORMAT_DEGREES);
    assert(motive_format_parse("rhythm", &format) && format == MOTIVE_FORMAT_RHYTHM);
    assert(motive_format_parse("contour", &format) && format == MOTIVE_FORMAT_CONTOUR);
    assert(strcmp(motive_format_name(MOTIVE_FORMAT_CONTOUR), "contour") == 0);
}

static void test_degrees_errors(void) {
    expect_read_error(motive_from_degrees_text, "1:q 8:e", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "not a scale degree");
    expect_read_error(motive_from_degrees_text, "1:q 0:e", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "not a scale degree");
    expect_read_error(motive_from_degrees_text, "1:q 3x:e", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "not an octave mark");
    expect_read_error(motive_from_degrees_text, "1:q 3", &C_MAJOR, MOTIVE_ERR_SYNTAX, "token 2 (column 5)",
                      "as in 5':q");
    expect_read_error(motive_from_degrees_text, "1:q 3:z", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "not a duration");
    expect_read_error(motive_from_degrees_text, "1:q 1:q~", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2", "no note after");
    expect_read_error(motive_from_degrees_text, "1:q 3:q~ 4:q", &C_MAJOR, MOTIVE_ERR_VALUE, "token 3 (column 10)",
                      "different pitch");
    expect_read_error(motive_from_degrees_text, "1:q r:q", &C_MAJOR, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "ends with a rest");
    expect_read_error(motive_from_degrees_text, "", &C_MAJOR, MOTIVE_ERR_EMPTY, "", "no notes");
    expect_read_error(motive_from_degrees_text, "1'''''''''''''':q", &C_MAJOR, MOTIVE_ERR_VALUE, "token 1 (column 1)",
                      "outside the MIDI range");
}

static void test_rhythm_and_contour_reading(void) {
    /* A rhythm sounds every note on degree 1: C4 with no key, the tonic with one. */
    Motive motive = load_with(motive_from_rhythm_text, "x:q x:e r:e x:h", &FOUR_FOUR, NULL);
    assert(motive.form == MOTIVE_SKETCH_FORM_RHYTHM && motive.note_count == 3);
    assert(note_is(&motive, 0, 60, 0, 1, 1, 4) && note_is(&motive, 1, 60, 1, 4, 1, 8) &&
           note_is(&motive, 2, 60, 1, 2, 1, 2));
    motive = load_with(motive_from_rhythm_text, "x:q~ x:q | x:h", &FOUR_FOUR, &D_MINOR);
    assert(motive.note_count == 2 && note_is(&motive, 0, 62, 0, 1, 1, 2) && note_is(&motive, 1, 62, 1, 2, 1, 2));

    /* A contour sounds rank r on degree r + 1 in quarters; equal ranks are the same height. */
    motive = load_with(motive_from_contour_text, "0 2 1 3", &FOUR_FOUR, &C_MAJOR);
    assert(motive.form == MOTIVE_SKETCH_FORM_CONTOUR && motive.note_count == 4);
    assert(note_is(&motive, 0, 60, 0, 1, 1, 4) && note_is(&motive, 1, 64, 1, 4, 1, 4) &&
           note_is(&motive, 2, 62, 1, 2, 1, 4) && note_is(&motive, 3, 65, 3, 4, 1, 4));
    motive = load_with(motive_from_contour_text, "1 1 0 7", &FOUR_FOUR, &D_MINOR);
    assert(motive.notes[0].pitch == 64 && motive.notes[1].pitch == 64 && motive.notes[2].pitch == 62);
    assert(motive.notes[3].pitch == 74);

    MotiveOptions options = options_with(&FOUR_FOUR, NULL);
    MotiveError error;
    Motive loaded;
    assert(motive_load("x:q x:q", 7, MOTIVE_FORMAT_RHYTHM, &options, &loaded, &error) == MOTIVE_OK);
    assert(loaded.form == MOTIVE_SKETCH_FORM_RHYTHM);
    assert(motive_load("0 1", 3, MOTIVE_FORMAT_CONTOUR, &options, &loaded, &error) == MOTIVE_OK);
    assert(loaded.form == MOTIVE_SKETCH_FORM_CONTOUR);

    expect_read_error(motive_from_rhythm_text, "x:q C4:e", NULL, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "not a note of a rhythm");
    expect_read_error(motive_from_rhythm_text, "x:q r:e", NULL, MOTIVE_ERR_VALUE, "token 2 (column 5)",
                      "ends with a rest");
    expect_read_error(motive_from_contour_text, "0 a", NULL, MOTIVE_ERR_SYNTAX, "token 2 (column 3)", "not a rank");
    expect_read_error(motive_from_contour_text, "0 -1", NULL, MOTIVE_ERR_SYNTAX, "token 2 (column 3)", "not a rank");
    expect_read_error(motive_from_contour_text, "0 q:e", NULL, MOTIVE_ERR_SYNTAX, "token 2 (column 3)", "not a rank");
    expect_read_error(motive_from_contour_text, "0 99", NULL, MOTIVE_ERR_VALUE, "token 2 (column 3)", "too high");
    expect_read_error(motive_from_contour_text, " | ", NULL, MOTIVE_ERR_EMPTY, "", "no notes");
}

static void test_writing_round_trips(void) {
    /* A note list with a rest, dots and a tie; 5/16 is a quarter and a sixteenth, written as a tie. */
    Motive motive = load_notes("r:e D4:q F4:e.~ F4:s F4:q.. A#3:e D5:h.. | C-1:q", &THREE_FOUR, &D_MINOR);
    char *text = motive_to_notes_text(&motive);
    assert(text);
    Motive again = load_with(motive_from_notes_text, text, &THREE_FOUR, &D_MINOR);
    assert(motive_equal(&motive, &again));
    free(text);
    motive = load_notes("D4:q~ D4:s F4:q", &FOUR_FOUR, NULL);
    assert(note_is(&motive, 0, 62, 0, 1, 5, 16));
    expect_text(motive_to_notes_text(&motive), "D4:q~ D4:s F4:q");

    /* Degrees, with octave marks and a raised degree. */
    motive = load_with(motive_from_degrees_text, "1:q #4:e 5,:e 1':h", &FOUR_FOUR, &D_MINOR);
    text = motive_to_degrees_text(&motive);
    assert(text);
    again = load_with(motive_from_degrees_text, text, &FOUR_FOUR, &D_MINOR);
    assert(motive_equal(&motive, &again));
    free(text);
    expect_text(motive_to_degrees_text(&motive), "1:q #4:e 5,:e 1':h");
    motive = load_notes("A4:q C5:q", &FOUR_FOUR, &G_MAJOR);
    expect_text(motive_to_degrees_text(&motive), "2:q 4:q");
    motive = load_notes("D4:q F4:q", &FOUR_FOUR, NULL);
    assert(motive.key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED);
    assert(motive_to_degrees_text(&motive) == NULL);

    /* Rhythm and contour. */
    motive = load_with(motive_from_rhythm_text, "x:q x:e r:e x:h", &FOUR_FOUR, NULL);
    expect_text(motive_to_rhythm_text(&motive), "x:q x:e r:e x:h");
    motive = load_notes("D4:q F4:e r:e A4:h", &FOUR_FOUR, NULL);
    expect_text(motive_to_rhythm_text(&motive), "x:q x:e r:e x:h");
    expect_text(motive_to_contour_text(&motive), "0 1 2");
    motive = load_with(motive_from_contour_text, "0 2 1 3", &FOUR_FOUR, &C_MAJOR);
    expect_text(motive_to_contour_text(&motive), "0 2 1 3");
    motive = load_with(motive_from_contour_text, "2 2 0 5 1", &FOUR_FOUR, NULL);
    expect_text(motive_to_contour_text(&motive), "2 2 0 3 1");
    assert(motive_to_notes_text(NULL) == NULL && motive_to_contour_text(NULL) == NULL);
}

/* ------------------------------------------------------------ pickup and form */

static void test_analysis_with_a_pickup(void) {
    /* A pickup of one beat in 3/4: the first note is the last beat of the bar before the first barline. */
    MotiveOptions options = options_with(&THREE_FOUR, &C_MAJOR);
    options.has_pickup = true;
    options.pickup.num = 1;
    options.pickup.den = 4;
    Motive motive;
    MotiveError error;
    assert(motive_from_notes_text("G4:q C5:h. D5:q", &options, &motive, &error) == MOTIVE_OK);
    assert(time_is(motive.pickup, 1, 4));
    MotiveAnalysis analysis;
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.rhythm[0].bar == -1 && time_is(analysis.rhythm[0].beat, 2, 1) && analysis.rhythm[0].on_beat);
    assert(analysis.rhythm[1].bar == 0 && time_is(analysis.rhythm[1].beat, 0, 1));
    assert(analysis.rhythm[2].bar == 1 && time_is(analysis.rhythm[2].beat, 0, 1));

    /* A half-beat pickup puts the first note on the off-beat before the barline. */
    options.pickup.num = 1;
    options.pickup.den = 8;
    assert(motive_from_notes_text("G4:e C5:h.", &options, &motive, &error) == MOTIVE_OK);
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.rhythm[0].bar == -1 && time_is(analysis.rhythm[0].beat, 5, 2) && !analysis.rhythm[0].on_beat);
    assert(analysis.rhythm[1].bar == 0 && time_is(analysis.rhythm[1].beat, 0, 1));

    /* No pickup: unchanged. */
    motive = load_notes("G4:q C5:h.", &THREE_FOUR, &C_MAJOR);
    assert(time_is(motive.pickup, 0, 1));
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.rhythm[0].bar == 0 && time_is(analysis.rhythm[0].beat, 0, 1));

    /* A pickup is shorter than a bar. */
    options.pickup.num = 3;
    options.pickup.den = 4;
    assert(motive_from_notes_text("G4:e C5:h.", &options, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "pickup") == 0);
}

static void test_pickup_and_form_in_json(void) {
    MotiveOptions options = options_with(&THREE_FOUR, NULL);
    options.has_pickup = true;
    options.pickup.num = 1;
    options.pickup.den = 4;
    Motive motive;
    MotiveError error;
    assert(motive_from_rhythm_text("x:q x:h.", &options, &motive, &error) == MOTIVE_OK);
    char *json = motive_to_json(&motive);
    assert(json && strstr(json, "\"form\": \"rhythm\"") && strstr(json, "\"pickup\""));
    Motive again;
    options = options_with(NULL, NULL);
    assert(motive_from_json(json, &options, &again, &error) == MOTIVE_OK);
    assert(again.form == MOTIVE_SKETCH_FORM_RHYTHM && time_is(again.pickup, 1, 4) && motive_equal(&motive, &again));
    free(json);

    /* A full motive without a pickup writes neither, and the two differ in motive_equal. */
    Motive plain = load_notes("D4:q F4:q", &FOUR_FOUR, NULL);
    json = motive_to_json(&plain);
    assert(json && !strstr(json, "\"form\"") && !strstr(json, "\"pickup\""));
    free(json);
    Motive shifted = plain;
    shifted.pickup = (Rational){1, 4};
    assert(!motive_equal(&plain, &shifted));
    shifted = plain;
    shifted.form = MOTIVE_SKETCH_FORM_CONTOUR;
    assert(!motive_equal(&plain, &shifted));

    const char *bad = "{\"meter\": {\"beats_per_bar\": 4, \"beat_unit\": 4}, \"form\": \"chord\", \"notes\": []}";
    assert(motive_from_json(bad, NULL, &again, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "form") == 0);
    MotiveSketchForm form;
    assert(motive_form_parse("contour", &form) && form == MOTIVE_SKETCH_FORM_CONTOUR && !motive_form_parse("chord", &form));
    assert(strcmp(motive_form_name(MOTIVE_SKETCH_FORM_RHYTHM), "rhythm") == 0);
}

static void test_analysis_by_form(void) {
    MotiveAnalysis analysis;
    /* A rhythm has a rhythm and nothing about pitch. */
    Motive motive = load_with(motive_from_rhythm_text, "x:q x:e x:e x:h", &FOUR_FOUR, &D_MINOR);
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.form == MOTIVE_SKETCH_FORM_RHYTHM && analysis.rhythm_known && !analysis.intervals_known &&
           !analysis.contour_known && !analysis.range_known && !analysis.degrees_known && !analysis.harmony_known);
    assert(analysis.rhythm[3].bar == 0 && time_is(analysis.rhythm[3].beat, 2, 1) && time_is(analysis.bars, 1, 1));

    /* A contour has a contour and no rhythm. */
    motive = load_with(motive_from_contour_text, "0 2 1 3", &FOUR_FOUR, &C_MAJOR);
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.form == MOTIVE_SKETCH_FORM_CONTOUR && analysis.contour_known && !analysis.rhythm_known &&
           !analysis.intervals_known && !analysis.range_known && !analysis.degrees_known && !analysis.harmony_known);
    assert(analysis.shape == MOTIVE_SHAPE_WAVE && analysis.directions[0] == CONTOUR_DIRECTION_UP &&
           analysis.directions[1] == CONTOUR_DIRECTION_DOWN);

    /* A full motive has all of it. */
    motive = load_notes("D4:q F4:q A4:q D5:q", &FOUR_FOUR, &D_MINOR);
    assert(motive_analyze(&motive, &analysis));
    assert(analysis.form == MOTIVE_SKETCH_FORM_FULL && analysis.rhythm_known && analysis.intervals_known &&
           analysis.contour_known && analysis.range_known && analysis.degrees_known && analysis.harmony_known);

    /* The JSON has null where a form has nothing; the text has no such section. */
    motive = load_with(motive_from_rhythm_text, "x:q x:e x:e x:h", &FOUR_FOUR, &D_MINOR);
    assert(motive_analyze(&motive, &analysis));
    char *json = motive_analysis_to_json(&motive, &analysis);
    assert(json && strstr(json, "\"intervals\": null") && strstr(json, "\"contour\": null") &&
           strstr(json, "\"range\": null") && strstr(json, "\"scale_degrees\": null"));
    assert(!strstr(json, "\"rhythm\": null"));
    free(json);
}

static void test_render_in_another_key(void) {
    /* D minor 1 b3 5 1' (D4 F4 A4 D5) in G minor: the same degrees, a fourth up. */
    Motive source = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    static const AnalysisKey G_MINOR = {NULL, PITCH_LETTER_G, 0, KEY_STRUCTURE_MODE_MINOR};
    Motive moved;
    assert(motive_in_key(&source, &G_MINOR, &moved));
    assert(moved.key_source == MOTIVE_SKETCH_KEY_GIVEN && moved.tonic_pc == 7 && moved.minor);
    assert(moved.note_count == 4 && moved.notes[0].pitch == 67 && moved.notes[1].pitch == 70 &&
           moved.notes[2].pitch == 74 && moved.notes[3].pitch == 79);
    assert(moved.length.num == source.length.num && moved.length.den == source.length.den);
    char *before = motive_to_degrees_text(&source);
    char *after = motive_to_degrees_text(&moved);
    assert(before && after && strcmp(before, after) == 0);
    free(before);
    free(after);

    /* The rhythm is the source's, and the pickup too. */
    source.pickup = (Rational){1, 4};
    assert(motive_in_key(&source, &G_MINOR, &moved));
    assert(time_is(moved.pickup, 1, 4) && time_is(moved.notes[1].onset, 1, 4) && time_is(moved.notes[3].duration, 1, 2));

    /* A raised degree stays raised; the mode may change. */
    source = load_notes("D4:q F#4:q A4:q", &FOUR_FOUR, &D_MINOR);
    assert(motive_in_key(&source, &C_MAJOR, &moved));
    assert(moved.notes[0].pitch == 60 && moved.notes[1].pitch == 65 && moved.notes[2].pitch == 67);

    /* The motive moves to its own key unchanged, and may be moved in place. */
    source = load_notes("D4:q F4:e A4:e D5:h", &FOUR_FOUR, &D_MINOR);
    assert(motive_in_key(&source, &D_MINOR, &moved) && motive_equal(&source, &moved));
    assert(motive_in_key(&source, &G_MINOR, &source) && source.notes[0].pitch == 67);

    /* No source key: refused, and out untouched. */
    Motive keyless = load_notes("D4:q F4:q", &FOUR_FOUR, NULL);
    moved = source;
    assert(!motive_in_key(&keyless, &G_MINOR, &moved) && motive_equal(&moved, &source));
    assert(!motive_in_key(NULL, &G_MINOR, &moved) && !motive_in_key(&source, NULL, &moved));
}

/* ------------------------------------------------- harmony, features and role */

static void test_harmony_text(void) {
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = 0;
    MotiveError error;
    assert(motive_harmony_parse("I V6@2 I@3", &FOUR_FOUR, chords, &count, &error) == MOTIVE_OK);
    assert(count == 3);
    assert(time_is(chords[0].onset, 0, 1) && chords[0].degree == 1 && chords[0].inversion == 0 && !chords[0].seventh);
    assert(time_is(chords[1].onset, 1, 2) && chords[1].degree == 5 && chords[1].inversion == 1);
    assert(time_is(chords[2].onset, 3, 4) && chords[2].degree == 1);

    /* Beats are of the beat unit: 6/8's beat is an eighth. */
    assert(motive_harmony_parse("I V7@3", &(PlanMeter){6, 8, 0}, chords, &count, &error) == MOTIVE_OK);
    assert(time_is(chords[1].onset, 3, 8) && chords[1].seventh);

    /* Alterations, every figure, fractions, and a first chord that starts late. */
    assert(motive_harmony_parse("bVII64@1/2 #IV65@3/2 bII42@2 VI43@5/2", &FOUR_FOUR, chords, &count, &error) == MOTIVE_OK);
    assert(count == 4);
    assert(chords[0].degree == 7 && chords[0].alteration == -1 && chords[0].inversion == 2 && !chords[0].seventh);
    assert(time_is(chords[0].onset, 1, 8));
    assert(chords[1].degree == 4 && chords[1].alteration == 1 && chords[1].inversion == 1 && chords[1].seventh);
    assert(chords[2].degree == 2 && chords[2].inversion == 3 && chords[2].seventh);
    assert(chords[3].degree == 6 && chords[3].inversion == 2 && chords[3].seventh);

    /* Nothing is no chords. */
    assert(motive_harmony_parse("  ", &FOUR_FOUR, chords, &count, &error) == MOTIVE_OK && count == 0);

    /* Written back, the text reads as the same chords. */
    Motive motive = load_notes("C4:q E4:q G4:q C5:q", &FOUR_FOUR, &C_MAJOR);
    const char *texts[] = {"I V6@2 I@3", "bVII64@1/2 #IV65@3/2 bII42@2 VI43@5/2", "V7@1", "I"};
    for (size_t t = 0; t < sizeof(texts) / sizeof(texts[0]); t++) {
        assert(motive_harmony_parse(texts[t], &FOUR_FOUR, motive.chords, &motive.chord_count, &error) == MOTIVE_OK);
        char *written = motive_harmony_to_text(&motive);
        assert(written && strcmp(written, texts[t]) == 0);
        free(written);
    }
    motive.chord_count = 0;
    char *none = motive_harmony_to_text(&motive);
    assert(none && none[0] == '\0');
    free(none);
}

static void test_harmony_errors(void) {
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = 0;
    MotiveError error;
    struct {
        const char *text;
        MotiveResult result;
        const char *location;
    } cases[] = {
        {"I V6", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},          /* later chords need an onset */
        {"I x@1", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},         /* not a numeral */
        {"I iv@1", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},        /* numerals are upper case */
        {"I V8@1", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},        /* not a figure */
        {"I V6@x", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},        /* not an onset */
        {"I V@-1", MOTIVE_ERR_SYNTAX, "token 2 (column 3)"},        /* a negative onset */
        {"I  V@2 I@2", MOTIVE_ERR_VALUE, "token 3 (column 8)"},     /* onsets must rise */
        {"V@2 I@1", MOTIVE_ERR_VALUE, "token 2 (column 5)"},
        {"I I@1 I@2 I@3 I@4 I@5 I@6 I@7 I@8", MOTIVE_ERR_VALUE, "token 9 (column 31)"}, /* nine chords */
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        MotiveResult result = motive_harmony_parse(cases[c].text, &FOUR_FOUR, chords, &count, &error);
        if (result != cases[c].result || strcmp(error.location, cases[c].location) != 0) {
            fprintf(stderr, "'%s': %s at '%s' (%s)\n", cases[c].text, motive_result_name(result), error.location,
                    error.message);
        }
        assert(result == cases[c].result);
        assert(strcmp(error.location, cases[c].location) == 0);
        assert(error.message[0] != '\0');
    }
    assert(motive_harmony_parse(NULL, &FOUR_FOUR, chords, &count, &error) == MOTIVE_ERR_NULL);
}

static void test_feature_and_role_text(void) {
    MotiveSketchFeature feature;
    MotiveError error;
    assert(motive_feature_parse("a:0-2", &feature, &error) == MOTIVE_OK);
    assert(strcmp(feature.label, "a") == 0 && feature.first == 0 && feature.last == 2);
    assert(motive_feature_parse("turn:3", &feature, &error) == MOTIVE_OK);
    assert(strcmp(feature.label, "turn") == 0 && feature.first == 3 && feature.last == 3);
    assert(motive_feature_parse("abcdefgh:1-5", &feature, &error) == MOTIVE_OK && strcmp(feature.label, "abcdefgh") == 0);

    const char *bad[] = {"a", ":1-2", "abcdefghi:1", "a:", "a:x", "a:1-", "a:1-2-3", "a b:1", "a:3-1", "a:-1"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(motive_feature_parse(bad[i], &feature, &error) != MOTIVE_OK);
        assert(strcmp(error.location, bad[i]) == 0 && error.message[0] != '\0');
    }

    bool role;
    assert(motive_role_parse("melodic", &role) && !role);
    assert(motive_role_parse("accompaniment", &role) && role);
    assert(!motive_role_parse("bass", &role) && !motive_role_parse("", &role) && !motive_role_parse(NULL, &role));
    assert(strcmp(motive_role_name(false), "melodic") == 0);
    assert(strcmp(motive_role_name(true), "accompaniment") == 0);
}

static const char *const EXTRAS_JSON =
    "{\"meter\": {\"beats_per_bar\": 4, \"beat_unit\": 4}, \"key\": {\"tonic_pitch_class\": 0, \"mode\": \"major\"},"
    " \"notes\": [{\"midi_pitch\": 60, \"offset\": {\"num\": 0, \"den\": 1}, \"duration\": {\"num\": 1, \"den\": 4}},"
    " {\"midi_pitch\": 64, \"offset\": {\"num\": 1, \"den\": 4}, \"duration\": {\"num\": 1, \"den\": 4}},"
    " {\"midi_pitch\": 67, \"offset\": {\"num\": 1, \"den\": 2}, \"duration\": {\"num\": 1, \"den\": 4}},"
    " {\"midi_pitch\": 72, \"offset\": {\"num\": 3, \"den\": 4}, \"duration\": {\"num\": 1, \"den\": 4}}],"
    " \"harmony\": \"I V6@2 I@3\","
    " \"features\": [{\"label\": \"a\", \"first\": 0, \"last\": 1}, {\"label\": \"b\", \"first\": 2, \"last\": 3}],"
    " \"role\": \"accompaniment\"}";

static void test_extras_in_json(void) {
    Motive motive;
    MotiveError error;
    assert(motive_from_json(EXTRAS_JSON, NULL, &motive, &error) == MOTIVE_OK);
    assert(motive.chord_count == 3 && motive.feature_count == 2 && motive.accompaniment);
    assert(motive.chords[1].degree == 5 && motive.chords[1].inversion == 1 && time_is(motive.chords[1].onset, 1, 2));
    assert(strcmp(motive.features[1].label, "b") == 0 && motive.features[1].first == 2 && motive.features[1].last == 3);

    /* The JSON written reads back as the same motive; a motive with none of it writes none of it. */
    char *written = motive_to_json(&motive);
    assert(written && strstr(written, "\"harmony\": \"I V6@2 I@3\"") && strstr(written, "\"role\": \"accompaniment\""));
    Motive again;
    assert(motive_from_json(written, NULL, &again, &error) == MOTIVE_OK);
    assert(motive_equal(&motive, &again));
    free(written);
    Motive plain = load_notes("C4:q E4:q G4:q C5:q", &FOUR_FOUR, &C_MAJOR);
    written = motive_to_json(&plain);
    assert(written && !strstr(written, "harmony") && !strstr(written, "features") && !strstr(written, "role"));
    free(written);

    /* The analysis document carries the harmony as text, only for a motive that has its own. */
    MotiveAnalysis analysis;
    assert(motive_analyze(&motive, &analysis));
    written = motive_analysis_to_json(&motive, &analysis);
    assert(written && strstr(written, "\"harmony_text\": \"I V6@2 I@3\""));
    free(written);
    assert(motive_analyze(&plain, &analysis));
    written = motive_analysis_to_json(&plain, &analysis);
    assert(written && !strstr(written, "harmony_text"));
    free(written);

    /* Options replace what the input says. */
    MotiveOptions options = options_with(NULL, NULL);
    options.has_role = true;
    options.accompaniment = false;
    options.has_features = true;
    options.feature_count = 1;
    strcpy(options.features[0].label, "x");
    options.features[0].last = 3;
    options.has_harmony = true;
    options.harmony_count = 1;
    options.harmony[0] = (MotiveSketchChord){{0, 1}, 4, 0, 0, false};
    assert(motive_from_json(EXTRAS_JSON, &options, &again, &error) == MOTIVE_OK);
    assert(!again.accompaniment && again.feature_count == 1 && again.chord_count == 1);
    assert(again.chords[0].degree == 4 && strcmp(again.features[0].label, "x") == 0);
    options.harmony_count = 0;
    assert(motive_from_json(EXTRAS_JSON, &options, &again, &error) == MOTIVE_OK && again.chord_count == 0);
}

static void test_extras_errors(void) {
    Motive motive;
    MotiveError error;
    const MotiveOptions base = options_with(&FOUR_FOUR, &C_MAJOR);
    MotiveOptions options = base;
    options.has_harmony = true;
    options.harmony_count = 2;
    options.harmony[0] = (MotiveSketchChord){{0, 1}, 1, 0, 0, false};
    options.harmony[1] = (MotiveSketchChord){{1, 1}, 5, 0, 0, false}; /* at the end of a one-bar motive */
    assert(motive_from_notes_text("C4:q E4:q G4:q C5:q", &options, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "harmony[1]") == 0);

    options = base;
    options.has_harmony = true;
    options.harmony_count = 1;
    options.harmony[0] = (MotiveSketchChord){{0, 1}, 8, 0, 0, false};
    assert(motive_from_notes_text("C4:q E4:q G4:q C5:q", &options, &motive, &error) == MOTIVE_ERR_VALUE);
    options.harmony[0] = (MotiveSketchChord){{0, 1}, 5, 0, 3, false}; /* a third inversion needs a seventh */
    assert(motive_from_notes_text("C4:q E4:q G4:q C5:q", &options, &motive, &error) == MOTIVE_ERR_VALUE);

    options = base;
    options.has_features = true;
    options.feature_count = 1;
    strcpy(options.features[0].label, "a");
    options.features[0].first = 2;
    options.features[0].last = 4; /* the motive has notes 0 to 3 */
    assert(motive_from_notes_text("C4:q E4:q G4:q C5:q", &options, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "features[0]") == 0);
    options.features[0].last = 3;
    options.feature_count = 2;
    strcpy(options.features[1].label, "a"); /* the same label twice */
    options.features[1].last = 1;
    assert(motive_from_notes_text("C4:q E4:q G4:q C5:q", &options, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "features[1]") == 0);

    /* In JSON the mistake is named with its place in the document. */
    const char *bad_harmony = "\"harmony\": \"I V6\"";
    char *text = malloc(strlen(EXTRAS_JSON) + 64);
    assert(text);
    const char *at = strstr(EXTRAS_JSON, "\"harmony\": \"I V6@2 I@3\"");
    assert(at);
    snprintf(text, strlen(EXTRAS_JSON) + 64, "%.*s%s%s", (int)(at - EXTRAS_JSON), EXTRAS_JSON, bad_harmony,
             at + strlen("\"harmony\": \"I V6@2 I@3\""));
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_ERR_SYNTAX);
    assert(strcmp(error.location, "harmony") == 0 && strstr(error.message, "token 2"));
    free(text);
    assert(motive_from_json("{\"role\": \"bass\", \"notes\": []}", NULL, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.location, "role") == 0);
}

static void test_equal_compares_extras(void) {
    Motive a;
    Motive b;
    MotiveError error;
    assert(motive_from_json(EXTRAS_JSON, NULL, &a, &error) == MOTIVE_OK);
    b = a;
    assert(motive_equal(&a, &b));
    b.chords[1].degree = 4;
    assert(!motive_equal(&a, &b));
    b = a;
    b.chords[2].onset = (Rational){7, 8};
    assert(!motive_equal(&a, &b));
    b = a;
    b.chord_count = 2;
    assert(!motive_equal(&a, &b));
    b = a;
    b.features[0].last = 2;
    assert(!motive_equal(&a, &b));
    b = a;
    strcpy(b.features[1].label, "c");
    assert(!motive_equal(&a, &b));
    b = a;
    b.feature_count = 0;
    assert(!motive_equal(&a, &b));
    b = a;
    b.accompaniment = false;
    assert(!motive_equal(&a, &b));
}

static Motive six_notes(void) {
    Motive motive = load_notes("C4:q D4:q E4:q F4:q G4:q A4:q", &(PlanMeter){6, 4, 0}, &C_MAJOR);
    MotiveError error;
    assert(motive_harmony_parse("I V@2 IV@4", &(PlanMeter){motive_meter(&motive).beats_per_bar, motive_meter(&motive).beat_unit, 0}, motive.chords, &motive.chord_count, &error) == MOTIVE_OK);
    const char *features[] = {"a:0-1", "b:2-3", "c:4-5"};
    for (size_t i = 0; i < 3; i++) {
        assert(motive_feature_parse(features[i], &motive.features[i], &error) == MOTIVE_OK);
    }
    motive.feature_count = 3;
    motive.accompaniment = true;
    return motive;
}

static bool feature_is(const Motive *motive, size_t index, const char *label, size_t first, size_t last) {
    return index < motive->feature_count && strcmp(motive->features[index].label, label) == 0 &&
           motive->features[index].first == first && motive->features[index].last == last;
}

static void test_features_follow_the_notes(void) {
    Motive source = six_notes();
    Motive result;
    MotiveRelationStep spec;

    /* Moving the pitches leaves every feature where it was, and the role stays. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_TRANSPOSE;
    spec.value[1] = 2;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.feature_count == 3 && feature_is(&result, 1, "b", 2, 3));
    assert(result.accompaniment && result.chord_count == 3);

    /* A retrograde turns them round. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_RETROGRADE;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.feature_count == 3);
    assert(feature_is(&result, 0, "a", 4, 5) && feature_is(&result, 1, "b", 2, 3) && feature_is(&result, 2, "c", 0, 1));
    assert(result.notes[0].pitch == 69);

    /* A fragment keeps the features it holds whole, renumbered from its first note. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_FRAGMENT;
    spec.value[0] = MOTIVE_FRAGMENT_TRAILING;
    spec.value[1] = 4;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.note_count == 4 && result.feature_count == 2);
    assert(feature_is(&result, 0, "b", 0, 1) && feature_is(&result, 1, "c", 2, 3));
    /* The chord sounding at its start (V, from beat 2) stands at 0; the chord at beat 4 moves up to beat 2. */
    assert(result.chord_count == 2 && result.chords[0].degree == 5 && time_is(result.chords[0].onset, 0, 1));
    assert(result.chords[1].degree == 4 && time_is(result.chords[1].onset, 1, 2));

    /* A fragment through the middle of a feature drops it. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_FRAGMENT;
    spec.value[0] = MOTIVE_FRAGMENT_LEADING;
    spec.value[1] = 3;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.feature_count == 1 && feature_is(&result, 0, "a", 0, 1));
    assert(result.chord_count == 2 && result.chords[1].degree == 5);

    /* Dropping a note takes its feature with it and renumbers the ones after. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_LIQUIDATE;
    spec.value[0] = 1;
    spec.value[2] = 2;
    spec.value[1] = true;
    assert(motive_develop(&source, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.note_count == 5 && result.feature_count == 2);

    /* Augmentation moves the chords with the notes. */
    memset(&spec, 0, sizeof(spec));
    spec.kind = MOTIVE_RELATION_AUGMENT;
    spec.value[0] = 2;
    spec.den[0] = 1;
    Motive shorter = load_notes("C4:e D4:e E4:e F4:e", &FOUR_FOUR, &C_MAJOR);
    MotiveError error;
    assert(motive_harmony_parse("I V@1", &(PlanMeter){motive_meter(&shorter).beats_per_bar, motive_meter(&shorter).beat_unit, 0}, shorter.chords, &shorter.chord_count, &error) == MOTIVE_OK);
    assert(motive_develop(&shorter, &spec, &result) == MOTIVE_DEV_OK);
    assert(result.chord_count == 2 && time_is(result.chords[1].onset, 1, 2));
}

int main(void) {
    test_same_motive_from_each_format();
    test_musicxml_keeps_spelling();
    test_options();
    test_analysis_ascending_arpeggio();
    test_analysis_rest_and_dotted();
    test_analysis_tie();
    test_analysis_without_a_key();
    test_key_is_inferred_only_when_the_pitches_settle();
    test_analysis_output();
    test_json_round_trip();
    test_note_list_errors();
    test_json_errors();
    test_file_errors();
    test_attach_as_given();
    test_degrees_reading();
    test_degrees_errors();
    test_rhythm_and_contour_reading();
    test_writing_round_trips();
    test_analysis_with_a_pickup();
    test_pickup_and_form_in_json();
    test_analysis_by_form();
    test_render_in_another_key();
    test_harmony_text();
    test_harmony_errors();
    test_feature_and_role_text();
    test_extras_in_json();
    test_extras_errors();
    test_equal_compares_extras();
    test_features_follow_the_notes();
    puts("test_motive: ok");
    return 0;
}
