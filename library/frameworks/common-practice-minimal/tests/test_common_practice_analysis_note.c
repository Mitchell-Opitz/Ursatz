#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "key_regions.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_note.h"
#include "time/musical_time.h"
#include "time/time_span.h"
#include "weighted_identification.h"

static TimeSpan *make_span(long start, long end, unsigned long den) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(start, den, &from) == MUSICAL_TIME_OK && musical_time_create(end, den, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(from);
    musical_time_destroy(to);
    return span;
}

static void near(double a, double b) {
    assert(fabs(a - b) < 1e-12);
}

static void test_member(void) {
    TimeSpan *segment = make_span(1, 3, 4); /* 1/4 .. 3/4 */
    CommonPracticeMember member;

    /* Attacked inside the segment and ending inside it: the whole note counts. */
    TimeSpan *inside_span = make_span(1, 2, 4);
    AnalysisNote inside = {"inside", NULL, inside_span, 60, NULL, "voice-a", 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&inside, segment, BEAT_STRENGTH_MEDIUM, &member));
    assert(strcmp(member.note_id, "inside") == 0);
    assert(member.pitch == 60);
    assert(!member.held);
    near(member.duration, 0.25);
    near(member.onset, 0.25);
    assert(member.strength == BEAT_STRENGTH_MEDIUM);
    assert(strcmp(member.voice_id, "voice-a") == 0);

    /* Sounding before the segment and ending inside it: held, with the partial overlap. */
    TimeSpan *held_span = make_span(0, 2, 4);
    AnalysisNote held = {"held", NULL, held_span, 64, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&held, segment, BEAT_STRENGTH_STRONG, &member));
    assert(member.held);
    near(member.duration, 0.25);
    near(member.onset, 0.0);
    assert(member.voice_id == NULL);

    /* Attacked inside and running past the end: the overlap is cut at the segment's end. */
    TimeSpan *long_span = make_span(2, 6, 4);
    AnalysisNote running = {"running", NULL, long_span, 67, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&running, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!member.held);
    near(member.duration, 0.25);
    near(member.onset, 0.5);

    /* Covering the whole segment, from before to after: held, duration the segment's length. */
    TimeSpan *cover_span = make_span(0, 4, 4);
    AnalysisNote cover = {"cover", NULL, cover_span, 55, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&cover, segment, BEAT_STRENGTH_WEAK, &member));
    assert(member.held);
    near(member.duration, 0.5);

    /* A third that does not divide a quarter is converted as a double. */
    TimeSpan *third_segment = make_span(0, 1, 3);
    TimeSpan *third_span = make_span(0, 1, 6);
    AnalysisNote third = {"third", NULL, third_span, 62, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&third, third_segment, BEAT_STRENGTH_STRONG, &member));
    near(member.duration, 1.0 / 6.0);

    /* Rejected: no overlap, unknown pitch, invalid note, NULL arguments; *out is left alone. */
    member.pitch = 99;
    TimeSpan *after_span = make_span(3, 4, 4);
    AnalysisNote after = {"after", NULL, after_span, 60, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&after, segment, BEAT_STRENGTH_WEAK, &member));
    AnalysisNote unknown = {"unknown", NULL, inside_span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&unknown, segment, BEAT_STRENGTH_WEAK, &member));
    AnalysisNote invalid = {NULL, NULL, inside_span, 60, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&invalid, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!common_practice_member_from_analysis_note(NULL, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!common_practice_member_from_analysis_note(&inside, NULL, BEAT_STRENGTH_WEAK, &member));
    assert(!common_practice_member_from_analysis_note(&inside, segment, BEAT_STRENGTH_WEAK, NULL));
    assert(member.pitch == 99);

    time_span_destroy(segment);
    time_span_destroy(inside_span);
    time_span_destroy(held_span);
    time_span_destroy(long_span);
    time_span_destroy(cover_span);
    time_span_destroy(third_segment);
    time_span_destroy(third_span);
    time_span_destroy(after_span);
}

static void test_region_key(void) {
    /* Every key index maps to the tonic and mode the vocabulary names. */
    for (int index = 0; index < COMMON_PRACTICE_KEY_COUNT; index++) {
        CommonPracticeKeyRegion region;
        memset(&region, 0, sizeof(region));
        region.key_index = index;
        AnalysisKey key;
        assert(common_practice_key_region_analysis_key(&region, &key));
        assert(analysis_key_tonic_pitch_class(&key) == common_practice_key_index_tonic(index));
        assert((key.mode == KEY_STRUCTURE_MODE_MINOR) == (common_practice_key_index_mode(index) == COMMON_PRACTICE_KEY_MINOR));
        assert(key.id == NULL);
    }
    CommonPracticeKeyRegion region;
    memset(&region, 0, sizeof(region));
    AnalysisKey key = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
    region.key_index = common_practice_key_index_of(3, COMMON_PRACTICE_KEY_MINOR);
    assert(common_practice_key_region_analysis_key(&region, &key));
    assert(key.tonic_letter == PITCH_LETTER_E && key.tonic_accidental == -1 && key.mode == KEY_STRUCTURE_MODE_MINOR);
    region.key_index = common_practice_key_index_of(6, COMMON_PRACTICE_KEY_MAJOR);
    assert(common_practice_key_region_analysis_key(&region, &key));
    assert(key.tonic_letter == PITCH_LETTER_F && key.tonic_accidental == 1 && key.mode == KEY_STRUCTURE_MODE_MAJOR);

    region.key_index = 24;
    assert(!common_practice_key_region_analysis_key(&region, &key));
    assert(!common_practice_key_region_analysis_key(NULL, &key));
    assert(!common_practice_key_region_analysis_key(&region, NULL));
}

static void test_signature_event(void) {
    MusicalTime *time = NULL;
    assert(musical_time_create(2, 1, &time) == MUSICAL_TIME_OK);
    CommonPracticeKeySignatureEvent out = {NULL, 99};

    for (int mode = KEY_STRUCTURE_MODE_MAJOR; mode <= KEY_STRUCTURE_MODE_MINOR; mode++) {
        for (int fifths = -7; fifths <= 7; fifths++) {
            AnalysisKeyEvent event;
            event.time = time;
            assert(analysis_key_from_fifths(fifths, (KeyStructureMode)mode, &event.key));
            assert(common_practice_key_signature_event_from_analysis(&event, &out));
            assert(out.position == time);
            assert(out.sharps_flats == fifths);
        }
    }

    out.sharps_flats = 99;
    AnalysisKeyEvent too_sharp = {time, {NULL, PITCH_LETTER_G, 1, KEY_STRUCTURE_MODE_MAJOR}}; /* G sharp major: 8 sharps */
    assert(!common_practice_key_signature_event_from_analysis(&too_sharp, &out));
    AnalysisKeyEvent no_time = {NULL, {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR}};
    assert(!common_practice_key_signature_event_from_analysis(&no_time, &out));
    assert(!common_practice_key_signature_event_from_analysis(NULL, &out));
    assert(!common_practice_key_signature_event_from_analysis(&too_sharp, NULL));
    assert(out.sharps_flats == 99);
    musical_time_destroy(time);
}

int main(void) {
    test_member();
    test_region_key();
    test_signature_event();
    printf("test_common_practice_analysis_note: ok\n");
    return 0;
}
