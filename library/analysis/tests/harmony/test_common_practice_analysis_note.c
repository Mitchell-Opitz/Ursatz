#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "analysis/harmony/key_regions.h"
#include "theory/tonality/spelled_key.h"
#include "analysis/notes/analysis_note.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"
#include "analysis/harmony/weighted_identification.h"
#include "tests/key_index_test_util.h"
#include "test_knowledge.h"

static TimeSpan make_span(long start, long end, unsigned long den) {
    MusicalTime from;
    MusicalTime to;
    TimeSpan span;
    assert(musical_time_make(start, den, &from) == MUSICAL_TIME_OK && musical_time_make(end, den, &to) == MUSICAL_TIME_OK);
    assert(time_span_make(from, to, &span) == TIME_SPAN_OK);
    return span;
}

static void exact(Rational value, long long num, long long den) {
    assert(rational_equals(value, rational_ratio(num, den)));
}

static void test_member(void) {
    TimeSpan segment = make_span(1, 3, 4); /* 1/4 .. 3/4 */
    CommonPracticeMember member;

    /* Attacked inside the segment and ending inside it: the whole note counts. */
    TimeSpan inside_span = make_span(1, 2, 4);
    AnalysisNote inside = {"inside", NULL, inside_span, 60, NULL, "voice-a", 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&inside, segment, BEAT_STRENGTH_MEDIUM, &member));
    assert(strcmp(member.note_id, "inside") == 0);
    assert(member.pitch == 60);
    assert(!member.held);
    exact(member.duration, 1, 4);
    exact(member.onset, 1, 4);
    assert(member.strength == BEAT_STRENGTH_MEDIUM);
    assert(strcmp(member.voice_id, "voice-a") == 0);

    /* Sounding before the segment and ending inside it: held, with the partial overlap. */
    TimeSpan held_span = make_span(0, 2, 4);
    AnalysisNote held = {"held", NULL, held_span, 64, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&held, segment, BEAT_STRENGTH_STRONG, &member));
    assert(member.held);
    exact(member.duration, 1, 4);
    exact(member.onset, 0, 1);
    assert(member.voice_id == NULL);

    /* Attacked inside and running past the end: the overlap is cut at the segment's end. */
    TimeSpan long_span = make_span(2, 6, 4);
    AnalysisNote running = {"running", NULL, long_span, 67, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&running, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!member.held);
    exact(member.duration, 1, 4);
    exact(member.onset, 1, 2);

    /* Covering the whole segment, from before to after: held, duration the segment's length. */
    TimeSpan cover_span = make_span(0, 4, 4);
    AnalysisNote cover = {"cover", NULL, cover_span, 55, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&cover, segment, BEAT_STRENGTH_WEAK, &member));
    assert(member.held);
    exact(member.duration, 1, 2);

    /* A third that does not divide a quarter is converted as a double. */
    TimeSpan third_segment = make_span(0, 1, 3);
    TimeSpan third_span = make_span(0, 1, 6);
    AnalysisNote third = {"third", NULL, third_span, 62, NULL, NULL, 0, 0, 1.0};
    assert(common_practice_member_from_analysis_note(&third, third_segment, BEAT_STRENGTH_STRONG, &member));
    exact(member.duration, 1, 6);

    /* Rejected: no overlap, unknown pitch, invalid note, NULL arguments; *out is left alone. */
    member.pitch = 99;
    TimeSpan after_span = make_span(3, 4, 4);
    AnalysisNote after = {"after", NULL, after_span, 60, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&after, segment, BEAT_STRENGTH_WEAK, &member));
    AnalysisNote unknown = {"unknown", NULL, inside_span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&unknown, segment, BEAT_STRENGTH_WEAK, &member));
    AnalysisNote invalid = {NULL, NULL, inside_span, 60, NULL, NULL, 0, 0, 1.0};
    assert(!common_practice_member_from_analysis_note(&invalid, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!common_practice_member_from_analysis_note(NULL, segment, BEAT_STRENGTH_WEAK, &member));
    assert(!common_practice_member_from_analysis_note(&inside, segment, BEAT_STRENGTH_WEAK, NULL));
    assert(member.pitch == 99);

}

static void test_signature_event(void) {
    MusicalTime time;
    assert(musical_time_make(2, 1, &time) == MUSICAL_TIME_OK);
    CommonPracticeKeySignatureEvent out = {.sharps_flats = 99};

    for (int mode = COMMON_PRACTICE_KEY_MAJOR; mode <= COMMON_PRACTICE_KEY_MINOR; mode++) {
        for (int fifths = -7; fifths <= 7; fifths++) {
            SpelledKeyEvent event;
            event.time = time;
            assert(spelled_key_from_fifths(fifths, common_practice_material(test_materials(), (CommonPracticeKeyMode)mode), &event.key));
            assert(common_practice_key_signature_event_from_analysis(&event, &out));
            assert(musical_time_same(out.position, time));
            assert(out.sharps_flats == fifths);
        }
    }

    out.sharps_flats = 99;
    SpelledKeyEvent too_sharp = {time, {NULL, PITCH_LETTER_G, 1, test_materials()->major}}; /* G sharp major: 8 sharps */
    assert(!common_practice_key_signature_event_from_analysis(&too_sharp, &out));
    assert(!common_practice_key_signature_event_from_analysis(NULL, &out));
    assert(!common_practice_key_signature_event_from_analysis(&too_sharp, NULL));
    assert(out.sharps_flats == 99);
}

int main(void) {
    test_member();
    test_signature_event();
    printf("test_common_practice_analysis_note: ok\n");
    return 0;
}
