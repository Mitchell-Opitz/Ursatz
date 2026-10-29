#include <assert.h>
#include <stdio.h>

#include "semantics/analysis_note.h"
#include "semantics/tone_line.h"

static bool refuse_all(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    (void)notes;
    (void)count;
    (void)out;
    (*(int *)context)++;
    return false;
}

static void test_clear_is_unclassified(void) {
    ToneLineLabel label = {true, true, NON_CHORD_TONE_PEDAL, 7, true};
    tone_line_label_clear(&label);
    assert(!label.chord_tone && !label.classified && !label.unresolved);
    assert(label.type == NON_CHORD_TONE_PASSING && label.resolves_to == -1);
    tone_line_label_clear(NULL);
}

static void test_a_note_holds_an_unknown_pitch_and_a_mask(void) {
    ToneLineNote note = {ANALYSIS_PITCH_UNKNOWN, rational_ratio(1, 4), rational_ratio(1, 8), 0x091u};
    assert(note.midi == -1 && note.chord_pitch_classes == 0x091u);
    assert(note.onset.num == 1 && note.onset.den == 4 && note.duration.den == 8);
}

static void test_a_classifier_is_called_through_the_type(void) {
    int calls = 0;
    NonChordToneClassifier classifier = refuse_all;
    ToneLineNote note = {60, rational_ratio(0, 1), rational_ratio(1, 4), 0x091u};
    ToneLineLabel label;
    assert(!classifier(&note, 1, &label, &calls));
    assert(calls == 1);
}

int main(void) {
    test_clear_is_unclassified();
    test_a_note_holds_an_unknown_pitch_and_a_mask();
    test_a_classifier_is_called_through_the_type();
    printf("tone_line: ok\n");
    return 0;
}
