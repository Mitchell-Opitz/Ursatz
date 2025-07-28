#include "alignment.h"

#include "events/note_event.h"
#include "pitch/pitch_spelling.h"
#include "time/exact_rational.h"
#include "time/temporal_anchor.h"

static int pitch_class(const PitchSpelling *spelling) {
    static const int LETTER_SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};
    int pc = (LETTER_SEMITONE[pitch_spelling_letter(spelling)] + pitch_spelling_accidental(spelling)) % 12;
    return pc < 0 ? pc + 12 : pc;
}

double alignment_root_share(const RomanTextAnalysis *analysis, const MusicXmlImport *score) {
    size_t checked = 0;
    size_t sounding = 0;
    for (size_t c = 0; c < analysis->chord_count; c++) {
        const RomanTextChord *chord = &analysis->chords[c];
        if (!chord->supported) {
            continue;
        }
        int root = 0;
        int intervals[4];
        romantext_expected_chord(chord, &root, intervals);
        ExactRational at = er_of_time(chord->time);
        bool found = false;
        for (size_t i = 0; i < score->count && !found; i++) {
            ExactRational onset = er_of_time(temporal_anchor_position(note_event_temporal_anchor(score->events[i])));
            ExactRational release = {0, 1};
            if (er_cmp(onset, at) > 0 || er_add(onset, er_of_duration(note_event_duration(score->events[i])), &release) != 0 ||
                er_cmp(release, at) <= 0) {
                continue;
            }
            found = pitch_class(note_event_optional_pitch_spelling(score->events[i])) == root;
        }
        checked++;
        sounding += found ? 1 : 0;
    }
    return checked == 0 ? 0.0 : (double)sounding / (double)checked;
}
