#include "score_roundtrip_util.h"

#include "io/midi_importer.h"
#include "../io/midi_import_split.h"

/*
 * musicxml_export_write_score writes chords and overlapping notes instead of refusing them: a chord as notes with
 * <chord/>, any other overlap as an extra <voice> in the same part and staff. Each case is re-imported and compared
 * with its source on the notes alone (onset, duration, sounding pitch); voices are not compared. URSATZ_CORPUS_DIR
 * and URSATZ_MUSICXML_DIR are passed in by CMake.
 */

typedef struct {
    long pos_n;
    unsigned long pos_d;
    long dur_n;
    unsigned long dur_d;
    int midi;
} Item;

static int midi_of(const NoteEvent *event) {
    static const int SEMITONES[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(event);
    return 12 * (pitch_spelling_octave(spelling) + 1) + SEMITONES[pitch_spelling_letter(spelling)] +
           pitch_spelling_accidental(spelling);
}

static int compare_items(const void *left, const void *right) {
    const Item *a = (const Item *)left;
    const Item *b = (const Item *)right;
    long long position_a = (long long)a->pos_n * (long long)b->pos_d;
    long long position_b = (long long)b->pos_n * (long long)a->pos_d;
    if (position_a != position_b) {
        return position_a < position_b ? -1 : 1;
    }
    long long duration_a = (long long)a->dur_n * (long long)b->dur_d;
    long long duration_b = (long long)b->dur_n * (long long)a->dur_d;
    if (duration_a != duration_b) {
        return duration_a < duration_b ? -1 : 1;
    }
    return a->midi < b->midi ? -1 : (a->midi > b->midi ? 1 : 0);
}

static Item *items_of(NoteEvent *const *events, size_t count) {
    Item *items = (Item *)calloc(count > 0 ? count : 1, sizeof(Item));
    assert(items != NULL);
    for (size_t i = 0; i < count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        items[i].pos_n = musical_time_numerator(position);
        items[i].pos_d = musical_time_denominator(position);
        items[i].dur_n = duration_numerator(duration);
        items[i].dur_d = duration_denominator(duration);
        items[i].midi = midi_of(events[i]);
    }
    qsort(items, count, sizeof(Item), compare_items);
    return items;
}

static size_t count_of(const char *xml, const char *needle) {
    size_t found = 0;
    for (const char *at = strstr(xml, needle); at != NULL; at = strstr(at + strlen(needle), needle)) {
        found++;
    }
    return found;
}

/* Exports score, re-imports the document, and checks the same notes came back. The document is returned. */
static char *export_and_compare(const MusicXmlExportScore *score) {
    char *xml = NULL;
    MusicXmlExportResult exported = musicxml_export_write_score(score, &xml);
    if (exported != MUSICXML_EXPORT_OK) {
        printf("score export failed with status %d\n", (int)exported);
    }
    assert(exported == MUSICXML_EXPORT_OK && xml != NULL);

    MusicXmlImport second;
    MusicXmlImportStatus status = musicxml_import_buffer(xml, strlen(xml), &second);
    if (status != MUSICXML_IMPORT_OK) {
        printf("re-import failed: %s\n", second.error_message);
    }
    assert(status == MUSICXML_IMPORT_OK);
    assert(second.count == score->count);
    Item *before = items_of(score->events, score->count);
    Item *after = items_of(second.events, second.count);
    for (size_t i = 0; i < score->count; i++) {
        assert(compare_items(&before[i], &after[i]) == 0);
    }
    free(before);
    free(after);
    musicxml_import_destroy(&second);
    return xml;
}

static MusicXmlExportScore score_of(const TestBag *bag) {
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = bag->events;
    score.count = bag->count;
    score.part_index = bag->part;
    score.staff = bag->staff;
    return score;
}

static void test_chord_in_one_voice(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "c", 0, 1, 1, 4, PITCH_LETTER_C, "v");
    bag_add_note(&bag, "e", 0, 1, 1, 4, PITCH_LETTER_E, "v");
    bag_add_note(&bag, "g", 0, 1, 1, 4, PITCH_LETTER_G, "v");
    MusicXmlExportScore score = score_of(&bag);
    char *xml = export_and_compare(&score);
    assert(count_of(xml, "<chord/>") == 2);
    assert(count_of(xml, "<voice>1</voice>") == 3 + 2); /* the three notes, then a quarter rest and a half rest */
    assert(!TU_HAS(xml, "<voice>2</voice>"));
    free(xml);
    bag_free(&bag);
}

static void test_held_note_under_two_notes(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "held", 0, 1, 1, 2, PITCH_LETTER_C, "v");
    bag_add_note(&bag, "d", 0, 1, 1, 4, PITCH_LETTER_D, "v");
    bag_add_note(&bag, "e", 1, 4, 1, 4, PITCH_LETTER_E, "v");
    MusicXmlExportScore score = score_of(&bag);
    char *xml = export_and_compare(&score);
    assert(TU_HAS(xml, "<voice>2</voice>"));
    assert(!TU_HAS(xml, "<voice>3</voice>"));
    assert(!TU_HAS(xml, "<chord/>"));
    assert(TU_HAS(xml, "<backup>"));
    free(xml);
    bag_free(&bag);
}

/* The case the export used to refuse: a note that starts while an earlier note of its voice still sounds. */
static void test_partial_overlap(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add_note(&bag, "a", 0, 1, 1, 2, PITCH_LETTER_C, "v");
    bag_add_note(&bag, "b", 1, 4, 1, 2, PITCH_LETTER_D, "v");
    MusicXmlExportScore score = score_of(&bag);
    char *xml = export_and_compare(&score);
    assert(TU_HAS(xml, "<voice>2</voice>"));
    free(xml);
    bag_free(&bag);
}

/* Extra voices are numbered after the voices the part already has, and stay on their staff. */
static void test_lane_voice_numbers(void) {
    TestBag bag;
    memset(&bag, 0, sizeof(bag));
    bag_add(&bag, tu_note("a", 0, 1, 1, 2, PITCH_LETTER_C, 0, 4, "v"), 0, 1);
    bag_add(&bag, tu_note("b", 0, 1, 1, 4, PITCH_LETTER_D, 0, 4, "v"), 0, 1);
    bag_add(&bag, tu_note("c", 0, 1, 1, 2, PITCH_LETTER_E, 0, 3, "w"), 0, 2);
    MusicXmlExportScore score = score_of(&bag);
    MusicXmlExportPart parts[1] = {{"P1", "One", 2}};
    score.parts = parts;
    score.part_count = 1;
    char *xml = export_and_compare(&score);
    assert(TU_HAS(xml, "<voice>1</voice>") && TU_HAS(xml, "<voice>2</voice>") && TU_HAS(xml, "<voice>3</voice>"));
    assert(!TU_HAS(xml, "<voice>4</voice>"));
    free(xml);
    bag_free(&bag);
}

static unsigned char *read_file(const char *directory, const char *name, size_t *size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length);
    assert(data != NULL && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return data;
}

/* The MIDI file with no staff or part arrays at all: every overlap of its implicit voices becomes a lane. */
static void test_midi_without_staves(const char *name) {
    size_t size = 0;
    unsigned char *data = read_file(URSATZ_CORPUS_DIR, name, &size);
    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    assert(test_midi_import_parts(data, size, &events, &pitches, &count, &meter, NULL) == MIDI_IMPORT_OK);
    assert(count > 0);
    score.events = events;
    score.pitch_values = pitches;
    score.count = count;
    score.meter_map = meter.meter_map;
    char *xml = export_and_compare(&score);
    printf("%s: %zu notes, %zu <chord/>, exported and re-imported with the same notes\n", name, count,
           count_of(xml, "<chord/>"));
    free(xml);
    midi_meter_import_destroy(&meter);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    free(data);
}

static void test_chords_backup_fixture(void) {
    size_t size = 0;
    unsigned char *data = read_file(URSATZ_MUSICXML_DIR, "chords_backup.musicxml", &size);
    MusicXmlImport first;
    assert(musicxml_import_buffer((const char *)data, size, &first) == MUSICXML_IMPORT_OK);
    MusicXmlExportScore score;
    RoundTripHolder holder;
    rt_fill_score(&first, &score, &holder);
    char *xml = export_and_compare(&score);
    assert(TU_HAS(xml, "<chord/>"));
    free(xml);
    free(holder.parts);
    free(holder.keys);
    musicxml_import_destroy(&first);
    free(data);
}

int main(void) {
    test_chord_in_one_voice();
    test_held_note_under_two_notes();
    test_partial_overlap();
    test_lane_voice_numbers();
    test_midi_without_staves("Clementi 36.mid");
    test_midi_without_staves("Field 1.mid");
    test_chords_backup_fixture();
    printf("test_musicxml_score_overlaps: all tests passed\n");
    return 0;
}
