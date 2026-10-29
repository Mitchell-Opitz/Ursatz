#include <assert.h>
#include <stdio.h>

#include "analysis_melody_nct.h"
#include "melody_test_util.h"

/*
 * standard_counterpoint_melody_nct_labels_with: NULL is the plain labels, an injected classifier replaces the attached results
 * of the notes that carry a chord mask (field for field), a rest or a note without a mask ends a run, a refusing classifier and the
 * notes outside a run keep what the line holds, and the line is not changed.
 */

typedef struct {
    int calls;
    size_t sizes[4];
    bool refuse;
} Spy;

/* Chord tones by the mask; the rest are labelled by the note's position: index 1 passing, index 2 an escape tone, else unclassified. */
static bool by_mask(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    Spy *spy = (Spy *)context;
    if (spy->calls < 4) {
        spy->sizes[spy->calls] = count;
    }
    spy->calls++;
    if (spy->refuse) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        tone_line_label_clear(&out[i]);
        out[i].chord_tone = (notes[i].chord_pitch_classes >> (((notes[i].midi % 12) + 12) % 12) & 1u) != 0;
        if (!out[i].chord_tone && i == 1) {
            out[i].classified = true;
            out[i].type = NON_CHORD_TONE_PASSING;
        } else if (!out[i].chord_tone && i == 2) {
            out[i].classified = true;
            out[i].type = NON_CHORD_TONE_ESCAPE_TONE;
        }
    }
    return true;
}

#define C_MAJOR_TRIAD ((1u << 0) | (1u << 4) | (1u << 7))

static void test_null_is_the_plain_labels(void) {
    const int pitches[] = {60, 62, 64};
    TestMelody m;
    test_melody_init(&m, pitches, 3);
    m.notes[1].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[1].nct_type = NON_CHORD_TONE_NEIGHBOR;
    test_melody_build(&m);
    StandardCounterpointNctLabels plain;
    StandardCounterpointNctLabels with_null;
    assert(standard_counterpoint_melody_nct_labels(&m.line, &plain) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(standard_counterpoint_melody_nct_labels_with(&m.line, NULL, NULL, &with_null) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(plain.count == with_null.count && plain.count == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(plain.items[i].claim.status == with_null.items[i].claim.status);
        assert(plain.items[i].claim.type == with_null.items[i].claim.type);
    }
    assert(with_null.items[1].claim.type == (int)NON_CHORD_TONE_NEIGHBOR);
    standard_counterpoint_nct_labels_destroy(&plain);
    standard_counterpoint_nct_labels_destroy(&with_null);
    test_melody_destroy(&m);
}

static void test_a_classifier_replaces_the_attached_results(void) {
    const int pitches[] = {60, 62, 65, 67}; /* C D F G over C major */
    TestMelody m;
    test_melody_init(&m, pitches, 4);
    for (size_t i = 0; i < 4; i++) {
        m.notes[i].chord_pitch_classes = C_MAJOR_TRIAD;
        m.notes[i].nct_ref = "seg";
    }
    /* what the line held: all labelled chord tones */
    for (size_t i = 0; i < 4; i++) {
        m.notes[i].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    }
    test_melody_build(&m);
    Spy spy = {0};
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels_with(&m.line, by_mask, &spy, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(spy.calls == 1 && spy.sizes[0] == 4);
    assert(labels.count == 4);
    assert(labels.items[0].claim.status == (int)STANDARD_COUNTERPOINT_NCT_CHORD_TONE && labels.items[0].claim.type == -1);
    assert(labels.items[1].claim.status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);
    assert(labels.items[1].claim.type == (int)NON_CHORD_TONE_PASSING);
    assert(labels.items[2].claim.status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);
    assert(labels.items[2].claim.type == (int)NON_CHORD_TONE_ESCAPE_TONE);
    assert(labels.items[3].claim.status == (int)STANDARD_COUNTERPOINT_NCT_CHORD_TONE);
    assert(labels.items[1].claim.has_ref); /* the reference stays */
    standard_counterpoint_nct_labels_destroy(&labels);
    /* the line itself is unchanged */
    assert(m.line.notes[1].nct == STANDARD_COUNTERPOINT_NCT_CHORD_TONE && m.line.notes[1].chord_pitch_classes == C_MAJOR_TRIAD);
    test_melody_destroy(&m);
}

static void test_a_rest_or_a_note_without_a_mask_ends_a_run(void) {
    const int pitches[] = {60, 62, 0, 64, 65, 67}; /* C D rest E F G */
    TestMelody m;
    test_melody_init(&m, pitches, 6);
    m.notes[0].chord_pitch_classes = C_MAJOR_TRIAD;
    m.notes[1].chord_pitch_classes = C_MAJOR_TRIAD;
    m.notes[3].chord_pitch_classes = C_MAJOR_TRIAD;
    m.notes[4].chord_pitch_classes = 0; /* no chord known */
    m.notes[5].chord_pitch_classes = C_MAJOR_TRIAD;
    m.notes[4].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[4].nct_type = NON_CHORD_TONE_SUSPENSION;
    test_melody_build(&m);
    Spy spy = {0};
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels_with(&m.line, by_mask, &spy, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(spy.calls == 3 && spy.sizes[0] == 2 && spy.sizes[1] == 1 && spy.sizes[2] == 1);
    assert(labels.count == 5); /* a rest has no label */
    assert(labels.items[1].claim.type == (int)NON_CHORD_TONE_PASSING); /* D, index 1 of its run */
    assert(labels.items[2].claim.status == (int)STANDARD_COUNTERPOINT_NCT_CHORD_TONE);       /* E alone, a chord tone */
    assert(labels.items[3].claim.status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);   /* F keeps what the line held */
    assert(labels.items[3].claim.type == (int)NON_CHORD_TONE_SUSPENSION);
    standard_counterpoint_nct_labels_destroy(&labels);
    test_melody_destroy(&m);
}

static void test_a_refusing_classifier_keeps_the_attached_results(void) {
    const int pitches[] = {60, 62, 64};
    TestMelody m;
    test_melody_init(&m, pitches, 3);
    for (size_t i = 0; i < 3; i++) {
        m.notes[i].chord_pitch_classes = C_MAJOR_TRIAD;
    }
    m.notes[1].nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
    m.notes[1].nct_type = NON_CHORD_TONE_NEIGHBOR;
    test_melody_build(&m);
    Spy spy = {0};
    spy.refuse = true;
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels_with(&m.line, by_mask, &spy, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(spy.calls == 1);
    assert(labels.items[1].claim.status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);
    assert(labels.items[1].claim.type == (int)NON_CHORD_TONE_NEIGHBOR);
    assert(labels.items[0].claim.status == (int)STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED);
    standard_counterpoint_nct_labels_destroy(&labels);
    test_melody_destroy(&m);
}

static void test_arguments(void) {
    StandardCounterpointNctLabels labels;
    assert(standard_counterpoint_melody_nct_labels_with(NULL, by_mask, NULL, &labels) ==
           STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT);
    StandardCounterpointMelodyLine empty;
    standard_counterpoint_melody_line_init(&empty);
    assert(standard_counterpoint_melody_nct_labels_with(&empty, by_mask, NULL, &labels) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(labels.count == 0 && labels.items == NULL);
    standard_counterpoint_melody_line_destroy(&empty);
}

int main(void) {
    test_null_is_the_plain_labels();
    test_a_classifier_replaces_the_attached_results();
    test_a_rest_or_a_note_without_a_mask_ends_a_run();
    test_a_refusing_classifier_keeps_the_attached_results();
    test_arguments();
    printf("melody nct injected: ok\n");
    return 0;
}
