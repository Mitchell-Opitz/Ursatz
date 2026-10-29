#include <assert.h>
#include <stdlib.h>

#include "analysis/outer_voice.h"
#include "identity/entity_id.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * outer_voice_attack_soprano: the highest note sounding at each onset, kept when attacked there. Times are in
 * sixteenths of a whole note; notes are listed sorted by start, then pitch, then id.
 */

static TimeSpan *span_of(long start, long end) {
    MusicalTime *s = NULL;
    MusicalTime *e = NULL;
    assert(musical_time_create(start, 16, &s) == MUSICAL_TIME_OK);
    assert(musical_time_create(end, 16, &e) == MUSICAL_TIME_OK);
    TimeSpan *span = NULL;
    assert(time_span_create(s, e, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(s);
    musical_time_destroy(e);
    return span;
}

typedef struct {
    const char *id;
    long start;
    long end;
    int midi;
} Spec;

typedef struct {
    MeterMap *map;
    TimeSpan **spans;
    AnalysisNote *notes;
    AnalysisNoteSet set;
} Fixture;

static Fixture fixture_make(const Spec *specs, size_t count) {
    Fixture f;
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    assert(meter_map_create_defaulted(id, &f.map) == METER_MAP_OK);
    f.spans = calloc(count > 0 ? count : 1, sizeof(TimeSpan *));
    f.notes = calloc(count > 0 ? count : 1, sizeof(AnalysisNote));
    for (size_t i = 0; i < count; i++) {
        f.spans[i] = span_of(specs[i].start, specs[i].end);
        AnalysisNote note = {specs[i].id, NULL, f.spans[i], specs[i].midi, NULL, NULL, ANALYSIS_INDEX_NONE,
                             ANALYSIS_INDEX_NONE, 1.0};
        f.notes[i] = note;
    }
    f.set.notes = f.notes;
    f.set.count = count;
    f.set.map = f.map;
    return f;
}

static void fixture_destroy(Fixture *f, size_t count) {
    for (size_t i = 0; i < count; i++) {
        time_span_destroy(f->spans[i]);
    }
    free(f->spans);
    free(f->notes);
    meter_map_destroy(f->map);
}

static void test_chords(void) {
    /* Two chords: C E G, then D F A. The top note of each, attacked there. */
    const Spec specs[] = {{"a", 0, 4, 60}, {"b", 0, 4, 64}, {"c", 0, 4, 67},
                          {"d", 4, 8, 62}, {"e", 4, 8, 65}, {"f", 4, 8, 69}};
    Fixture f = fixture_make(specs, 6);
    size_t *line = NULL;
    size_t count = 99;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 2 && line[0] == 2 && line[1] == 5);
    free(line);
    fixture_destroy(&f, 6);
}

static void test_held_under_attack(void) {
    /* A5 is held over a new C4 attack and a later E4: only the A5 attack is the soprano. */
    const Spec specs[] = {{"a", 0, 8, 81}, {"b", 2, 4, 60}, {"c", 4, 6, 64}};
    Fixture f = fixture_make(specs, 3);
    size_t *line = NULL;
    size_t count = 0;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 1 && line[0] == 0);
    free(line);
    fixture_destroy(&f, 3);
}

static void test_ends_before_onset(void) {
    /* A high note that ended no longer hides a lower attack. */
    const Spec specs[] = {{"a", 0, 2, 81}, {"b", 2, 4, 60}};
    Fixture f = fixture_make(specs, 2);
    size_t *line = NULL;
    size_t count = 0;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 2 && line[0] == 0 && line[1] == 1);
    free(line);
    fixture_destroy(&f, 2);
}

static void test_equal_pitches(void) {
    /* The same pitch twice at one onset: the first listed. Re-struck while held: the later attack. */
    const Spec same_onset[] = {{"a", 0, 4, 60}, {"b", 0, 4, 60}};
    Fixture f = fixture_make(same_onset, 2);
    size_t *line = NULL;
    size_t count = 0;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 1 && line[0] == 0);
    free(line);
    fixture_destroy(&f, 2);

    const Spec restruck[] = {{"a", 0, 8, 60}, {"b", 4, 8, 60}};
    f = fixture_make(restruck, 2);
    line = NULL;
    count = 0;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 2 && line[0] == 0 && line[1] == 1);
    free(line);
    fixture_destroy(&f, 2);
}

static void test_unknown_pitches(void) {
    /* An unknown pitch is skipped: it neither is the soprano nor hides a known note. */
    const Spec mixed[] = {{"a", 0, 4, ANALYSIS_PITCH_UNKNOWN}, {"b", 0, 4, 60}, {"c", 4, 8, ANALYSIS_PITCH_UNKNOWN}};
    Fixture f = fixture_make(mixed, 3);
    size_t *line = NULL;
    size_t count = 0;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(count == 1 && line[0] == 1);
    free(line);
    fixture_destroy(&f, 3);

    const Spec only_unknown[] = {{"a", 0, 4, ANALYSIS_PITCH_UNKNOWN}};
    f = fixture_make(only_unknown, 1);
    line = (size_t *)&f;
    count = 7;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(line == NULL && count == 0);
    fixture_destroy(&f, 1);
}

static void test_empty_and_errors(void) {
    Fixture f = fixture_make(NULL, 0);
    size_t *line = (size_t *)&f;
    size_t count = 7;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_OK);
    assert(line == NULL && count == 0);
    fixture_destroy(&f, 0);

    const Spec unsorted[] = {{"b", 4, 8, 62}, {"a", 0, 4, 60}};
    f = fixture_make(unsorted, 2);
    line = NULL;
    count = 7;
    assert(outer_voice_attack_soprano(&f.set, &line, &count) == OUTER_VOICE_ERR_NULL_NOTE);
    assert(line == NULL && count == 7);
    assert(outer_voice_attack_soprano(NULL, &line, &count) == OUTER_VOICE_ERR_NULL_ARGUMENT);
    assert(outer_voice_attack_soprano(&f.set, NULL, &count) == OUTER_VOICE_ERR_NULL_ARGUMENT);
    assert(outer_voice_attack_soprano(&f.set, &line, NULL) == OUTER_VOICE_ERR_NULL_ARGUMENT);
    fixture_destroy(&f, 2);
}

int main(void) {
    test_chords();
    test_held_under_attack();
    test_ends_before_onset();
    test_equal_pitches();
    test_unknown_pitches();
    test_empty_and_errors();
    return 0;
}
