#include <assert.h>
#include <stdio.h>

#include "fugue_exposition.h"
#include "fugue_test_util.h"

/* Two voices of sixteen slots: a subject in the upper voice, and in the lower
   voice an answer of three notes after a half rest. */
#define TWO_VOICES(name)                                                                                    \
    const PitchSpelling *name##_c4 = P(C, 0, 4);                                                           \
    const PitchSpelling *name##_d4 = P(D, 0, 4);                                                           \
    const PitchSpelling *name##_e4 = P(E, 0, 4);                                                           \
    const PitchSpelling *name##_g3 = P(G, 0, 3);                                                           \
    const PitchSpelling *name##_a3 = P(A, 0, 3);                                                           \
    const PitchSpelling *name##_b3 = P(B, 0, 3);                                                           \
    StandardCounterpointSlot name##_upper[] = {HALF(name##_c4), HALF(name##_d4), HALF(name##_e4),         \
                                               HALF(name##_c4)};                                          \
    StandardCounterpointSlot name##_lower[] = {REST4(), HALF(name##_g3), HALF(name##_a3), HALF(name##_b3)}; \
    StandardCounterpointSlotLine name##_voices[] = {LINE_OF(name##_upper), LINE_OF(name##_lower)};          \
    FugueStatement name##_statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 4, 0}, {1, 4, FUGUE_ROLE_ANSWER, 3, 0}}; \
    EXPOSITION_OF(name, name##_voices, name##_statements)

static void test_valid(void) {
    TWO_VOICES(fugue);
    assert(fugue_exposition_valid(&fugue));
    assert(fugue_exposition_slot_count(&fugue) == 16);
    assert(!fugue_exposition_valid(NULL));
    assert(fugue_exposition_slot_count(NULL) == 0);
}

static void test_invalid_voices(void) {
    TWO_VOICES(fugue);

    FugueExposition one_voice = fugue;
    one_voice.voice_count = 1;
    assert(!fugue_exposition_valid(&one_voice));

    FugueExposition too_many = fugue;
    too_many.voice_count = FUGUE_MAX_VOICES + 1;
    assert(!fugue_exposition_valid(&too_many));

    StandardCounterpointSlotLine uneven[] = {fugue_voices[0], {fugue_lower, 15}};
    FugueExposition uneven_fugue = fugue;
    uneven_fugue.voices = uneven;
    assert(!fugue_exposition_valid(&uneven_fugue));

    FugueExposition no_voices = fugue;
    no_voices.voices = NULL;
    assert(!fugue_exposition_valid(&no_voices));

    /* A tie that does not continue the same pitch. */
    StandardCounterpointSlot wrong_tie[] = {ATK(fugue_c4), TIE(fugue_d4), REST(), REST()};
    StandardCounterpointSlotLine wrong_tie_voices[] = {LINE_OF(wrong_tie), LINE_OF(wrong_tie)};
    FugueStatement one[] = {{0, 0, FUGUE_ROLE_SUBJECT, 1, 0}};
    EXPOSITION_OF(wrong, wrong_tie_voices, one);
    assert(!fugue_exposition_valid(&wrong));

    /* An attack with no pitch. */
    StandardCounterpointSlot no_pitch[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}, REST(), REST(), REST()};
    StandardCounterpointSlotLine no_pitch_voices[] = {LINE_OF(no_pitch), LINE_OF(no_pitch)};
    EXPOSITION_OF(missing, no_pitch_voices, one);
    assert(!fugue_exposition_valid(&missing));
}

static void test_invalid_statements(void) {
    TWO_VOICES(fugue);

    FugueExposition none = fugue;
    none.statement_count = 0;
    assert(!fugue_exposition_valid(&none));

    FugueExposition too_many = fugue;
    too_many.statement_count = FUGUE_MAX_STATEMENTS + 1;
    assert(!fugue_exposition_valid(&too_many));

    FugueExposition no_bar = fugue;
    no_bar.slots_per_bar = 0;
    assert(!fugue_exposition_valid(&no_bar));

    fugue_statements[0].start = 1;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[0].start = 0;

    fugue_statements[0].note_count = 5;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[0].note_count = 0;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[0].note_count = 4;

    fugue_statements[1].voice = 2;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[1].voice = 1;

    fugue_statements[1].role = FUGUE_ROLE_COUNT;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[1].role = FUGUE_ROLE_ANSWER;

    /* The answer has only three notes. */
    fugue_statements[1].note_count = 4;
    assert(!fugue_exposition_valid(&fugue));
    fugue_statements[1].note_count = 3;

    /* Statements must be listed in the order they begin. */
    FugueStatement swapped[] = {fugue_statements[1], fugue_statements[0]};
    FugueExposition swapped_fugue = fugue;
    swapped_fugue.statements = swapped;
    assert(!fugue_exposition_valid(&swapped_fugue));

    assert(fugue_exposition_valid(&fugue));
}

static void test_pitch_and_attacks(void) {
    TWO_VOICES(fugue);
    assert(fugue_pitch_at(&fugue, 0, 0) == fugue_c4);
    assert(fugue_pitch_at(&fugue, 0, 3) == fugue_c4);
    assert(fugue_pitch_at(&fugue, 0, 4) == fugue_d4);
    assert(fugue_pitch_at(&fugue, 1, 0) == NULL);
    assert(fugue_pitch_at(&fugue, 1, 4) == fugue_g3);
    assert(fugue_pitch_at(&fugue, 1, 16) == NULL);
    assert(fugue_pitch_at(&fugue, 2, 0) == NULL);
    assert(fugue_attacks_at(&fugue, 0, 0));
    assert(!fugue_attacks_at(&fugue, 0, 1));
    assert(fugue_attacks_at(&fugue, 1, 4));
    assert(!fugue_attacks_at(&fugue, 1, 0));
    assert(!fugue_attacks_at(&fugue, 1, 16));
    assert(!fugue_attacks_at(&fugue, 2, 0));
}

static void test_notes(void) {
    TWO_VOICES(fugue);
    FugueNote notes[8];
    size_t count = 0;

    assert(fugue_voice_notes(&fugue, 0, notes, 8, &count));
    assert(count == 4);
    assert(notes[0].start == 0 && notes[0].length == 4 && notes[0].pitch == fugue_c4);
    assert(notes[3].start == 12 && notes[3].length == 4);
    assert(!fugue_voice_notes(&fugue, 0, notes, 3, &count));
    assert(!fugue_voice_notes(&fugue, 2, notes, 8, &count));
    assert(!fugue_voice_notes(&fugue, 0, NULL, 8, &count));
    assert(!fugue_voice_notes(&fugue, 0, notes, 8, NULL));

    assert(fugue_statement_notes(&fugue, 1, notes, 8, &count));
    assert(count == 3);
    assert(notes[0].start == 4 && notes[0].pitch == fugue_g3);
    assert(notes[2].start == 12 && notes[2].pitch == fugue_b3);
    assert(!fugue_statement_notes(&fugue, 1, notes, 2, &count));
    assert(!fugue_statement_notes(&fugue, 2, notes, 8, &count));
}

static void test_limits(void) {
    TWO_VOICES(fugue);
    assert(fugue_default_limits().max_altered_intervals == 2);
    assert(fugue_exposition_limits(&fugue).max_altered_intervals == 2);
    assert(fugue_exposition_limits(NULL).max_altered_intervals == 2);
    FugueLimits strict = {0};
    fugue.limits = &strict;
    assert(fugue_exposition_limits(&fugue).max_altered_intervals == 0);
}

static void test_voice_part(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot slots[] = {HALF(c4)};
    StandardCounterpointSlotLine voices[] = {LINE_OF(slots), LINE_OF(slots), LINE_OF(slots), LINE_OF(slots)};
    FugueStatement statements[] = {{0, 0, FUGUE_ROLE_SUBJECT, 1, 0}};
    EXPOSITION_OF(four, voices, statements);
    VoicePart part;
    assert(fugue_voice_part(&four, 0, &part) && part == VOICE_PART_SOPRANO);
    assert(fugue_voice_part(&four, 1, &part) && part == VOICE_PART_ALTO);
    assert(fugue_voice_part(&four, 2, &part) && part == VOICE_PART_TENOR);
    assert(fugue_voice_part(&four, 3, &part) && part == VOICE_PART_BASS);
    part = VOICE_PART_ALTO;
    assert(!fugue_voice_part(&four, 4, &part));
    assert(!fugue_voice_part(&four, 0, NULL));
    assert(!fugue_voice_part(NULL, 0, &part));
    assert(part == VOICE_PART_ALTO);

    TWO_VOICES(two);
    assert(!fugue_voice_part(&two, 0, &part));
}

int main(void) {
    test_valid();
    test_invalid_voices();
    test_invalid_statements();
    test_pitch_and_attacks();
    test_notes();
    test_limits();
    test_voice_part();
    printf("test_fugue_exposition: ok\n");
    return 0;
}
