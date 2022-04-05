#include <assert.h>
#include <stdio.h>

#include "ground_piece.h"
#include "ground_test_util.h"

static void test_valid(void) {
    GroundFixture f;
    ground_fixture(&f);
    assert(ground_piece_valid(&f.piece));
    assert(ground_piece_slot_count(&f.piece) == 96);
    assert(ground_piece_iteration_count(&f.piece) == 4);
    assert(!ground_piece_valid(NULL));
    assert(ground_piece_slot_count(NULL) == 0);
    assert(ground_piece_iteration_count(NULL) == 0);

    /* A length that is not a whole number of iterations is still a piece. */
    f.piece.slots_per_iteration = 25;
    assert(ground_piece_valid(&f.piece));
    assert(ground_piece_iteration_count(&f.piece) == 3);
}

static void test_no_statements(void) {
    GroundFixture f;
    ground_fixture(&f);
    f.piece.statements = NULL;
    f.piece.statement_count = 0;
    assert(ground_piece_valid(&f.piece));
}

static void test_invalid_fields(void) {
    GroundFixture f;
    ground_fixture(&f);

    GroundBassPiece one_voice = f.piece;
    one_voice.voice_count = 1;
    assert(!ground_piece_valid(&one_voice));

    GroundBassPiece too_many = f.piece;
    too_many.voice_count = GROUND_MAX_VOICES + 1;
    assert(!ground_piece_valid(&too_many));

    GroundBassPiece bad_bass = f.piece;
    bad_bass.bass_voice = 3;
    assert(!ground_piece_valid(&bad_bass));

    GroundBassPiece no_iteration = f.piece;
    no_iteration.slots_per_iteration = 0;
    assert(!ground_piece_valid(&no_iteration));

    GroundBassPiece no_bar = f.piece;
    no_bar.slots_per_bar = 0;
    assert(!ground_piece_valid(&no_bar));

    GroundBassPiece no_beats = f.piece;
    no_beats.beats_per_bar = 0;
    assert(!ground_piece_valid(&no_beats));

    GroundBassPiece bad_type = f.piece;
    bad_type.type = OSTINATO_TYPE_COUNT;
    assert(!ground_piece_valid(&bad_type));

    GroundBassPiece no_voices = f.piece;
    no_voices.voices = NULL;
    assert(!ground_piece_valid(&no_voices));

    StandardCounterpointSlotLine uneven[3] = {f.voices[0], f.voices[1], f.voices[2]};
    uneven[1].count = FIXTURE_SLOTS - 1;
    GroundBassPiece uneven_piece = f.piece;
    uneven_piece.voices = uneven;
    assert(!ground_piece_valid(&uneven_piece));

    /* A tie that does not continue the same pitch. */
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    StandardCounterpointSlot wrong[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, c4}, {STANDARD_COUNTERPOINT_SLOT_TIE, d4}};
    StandardCounterpointSlotLine wrong_voices[] = {{wrong, 2}, {wrong, 2}};
    GroundBassPiece wrong_piece = {wrong_voices, 2, 1, 2, 2, 3, OSTINATO_TYPE_GROUND_BASS, NULL, 0, NULL};
    assert(!ground_piece_valid(&wrong_piece));

    /* An attack with no pitch. */
    StandardCounterpointSlot nothing[] = {{STANDARD_COUNTERPOINT_SLOT_ATTACK, NULL}, {STANDARD_COUNTERPOINT_SLOT_REST, NULL}};
    StandardCounterpointSlotLine nothing_voices[] = {{nothing, 2}, {nothing, 2}};
    GroundBassPiece nothing_piece = {nothing_voices, 2, 1, 2, 2, 3, OSTINATO_TYPE_GROUND_BASS, NULL, 0, NULL};
    assert(!ground_piece_valid(&nothing_piece));
}

static void test_invalid_statements(void) {
    GroundFixture f;
    ground_fixture(&f);

    GroundBassPiece missing = f.piece;
    missing.statements = NULL;
    assert(!ground_piece_valid(&missing));

    GroundBassPiece too_many = f.piece;
    too_many.statement_count = GROUND_MAX_STATEMENTS + 1;
    assert(!ground_piece_valid(&too_many));

    GroundStatement saved = f.statements[1];

    f.statements[1].start = 7;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    f.statements[1].note_count = 0;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    f.statements[1].note_count = 99;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    f.statements[1].voice = 3;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    f.statements[1].role = IMITATIVE_ROLE_COUNT;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    f.statements[1].start = 90;
    assert(!ground_piece_valid(&f.piece));
    f.statements[1] = saved;

    /* Statements must be listed in the order they begin. */
    GroundStatement swapped[2] = {f.statements[2], f.statements[0]};
    GroundBassPiece swapped_piece = f.piece;
    swapped_piece.statements = swapped;
    swapped_piece.statement_count = 2;
    assert(!ground_piece_valid(&swapped_piece));

    assert(ground_piece_valid(&f.piece));
}

static void test_limits(void) {
    GroundFixture f;
    ground_fixture(&f);
    assert(ground_default_limits().max_altered_notes == 1);
    assert(ground_piece_limits(&f.piece).max_altered_notes == 1);
    assert(ground_piece_limits(NULL).max_altered_notes == 1);
    GroundLimits strict = {0};
    f.piece.limits = &strict;
    assert(ground_piece_limits(&f.piece).max_altered_notes == 0);
}

static void test_pitch_and_attacks(void) {
    GroundFixture f;
    ground_fixture(&f);
    const PitchSpelling *g2 = P(G, 0, 2);
    assert(pitch_spelling_equals(ground_pitch_at(&f.piece, 2, 0), g2));
    assert(pitch_spelling_equals(ground_pitch_at(&f.piece, 2, 5), g2));
    assert(!pitch_spelling_equals(ground_pitch_at(&f.piece, 2, 6), g2));
    assert(ground_pitch_at(&f.piece, 2, 96) == NULL);
    assert(ground_pitch_at(&f.piece, 3, 0) == NULL);
    assert(ground_attacks_at(&f.piece, 2, 0));
    assert(!ground_attacks_at(&f.piece, 2, 1));
    assert(ground_attacks_at(&f.piece, 2, 6));
    assert(!ground_attacks_at(&f.piece, 3, 0));
    assert(!ground_attacks_at(&f.piece, 2, 96));

    /* A rest has no pitch. */
    f.slots[0][0].kind = STANDARD_COUNTERPOINT_SLOT_REST;
    f.slots[0][0].pitch = NULL;
    f.slots[0][1].kind = STANDARD_COUNTERPOINT_SLOT_REST;
    f.slots[0][1].pitch = NULL;
    assert(ground_pitch_at(&f.piece, 0, 0) == NULL);
}

static void test_notes(void) {
    GroundFixture f;
    ground_fixture(&f);
    GroundNote notes[GROUND_MAX_NOTES];
    size_t count = 0;

    assert(ground_voice_notes(&f.piece, 2, notes, GROUND_MAX_NOTES, &count));
    assert(count == 16);
    assert(notes[0].start == 0 && notes[0].length == 6);
    assert(notes[15].start == 90 && notes[15].length == 6);
    assert(!ground_voice_notes(&f.piece, 2, notes, 15, &count));
    assert(!ground_voice_notes(&f.piece, 3, notes, GROUND_MAX_NOTES, &count));
    assert(!ground_voice_notes(&f.piece, 2, NULL, GROUND_MAX_NOTES, &count));
    assert(!ground_voice_notes(&f.piece, 2, notes, GROUND_MAX_NOTES, NULL));

    GroundNote comes[8];
    assert(ground_statement_notes(&f.piece, 1, comes, 8, &count));
    assert(count == 3);
    assert(comes[0].start == 6 && comes[0].length == 2);
    assert(comes[2].start == 10);
    assert(!ground_statement_notes(&f.piece, 1, comes, 2, &count));
    assert(!ground_statement_notes(&f.piece, 8, comes, 8, &count));
}

int main(void) {
    test_valid();
    test_no_statements();
    test_invalid_fields();
    test_invalid_statements();
    test_limits();
    test_pitch_and_attacks();
    test_notes();
    printf("test_ground_piece: ok\n");
    return 0;
}
