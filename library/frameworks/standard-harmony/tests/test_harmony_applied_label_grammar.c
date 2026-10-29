#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_applied.h"
#include "roman_numeral_grammar.h"

/*
 * harmony_applied_label keeps its own formatter (standard-harmony does not depend on standard-tonality). This
 * test-only link checks that every label it writes is a numeral the standard-tonality grammar reads, and that the
 * grammar reads the same chord: the quality, the target degree, and the root.
 */

static const StandardTonalityNumeralQuality EXPECTED_QUALITY[HARMONY_APPLIED_QUALITY_COUNT] = {
    STANDARD_TONALITY_NUMERAL_MAJOR, STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH,
    STANDARD_TONALITY_NUMERAL_DIMINISHED, STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH,
    STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH};

int main(void) {
    size_t checked = 0;
    for (int mode = 0; mode < 2; mode++) {
        HarmonyMode harmony_mode = mode == 0 ? HARMONY_MODE_MAJOR : HARMONY_MODE_MINOR;
        StandardTonalityMode tonal_mode = mode == 0 ? STANDARD_TONALITY_MODE_MAJOR
                                                    : STANDARD_TONALITY_MODE_NATURAL_MINOR;
        for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
            for (int target = 1; target <= 7; target++) {
                HarmonyAppliedChord chord;
                chord.kind = (quality == HARMONY_APPLIED_MAJOR_TRIAD || quality == HARMONY_APPLIED_DOMINANT_SEVENTH)
                                 ? APPLIED_CHORD_KIND_SECONDARY_DOMINANT
                                 : APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE;
                chord.quality = (HarmonyAppliedQuality)quality;
                chord.target = target;
                char label[32];
                if (!harmony_applied_label(harmony_mode, &chord, label, sizeof(label))) {
                    continue;
                }
                StandardTonalityNumeralText text;
                if (!standard_tonality_numeral_parse(label, &text)) {
                    fprintf(stderr, "grammar rejects the applied label %s\n", label);
                    return 1;
                }
                assert(text.has_target && text.target_degree == target && text.alteration == 0 &&
                       text.target_alteration == 0 && text.inversion == 0);
                assert(text.quality == EXPECTED_QUALITY[quality]);

                int semitones = -1;
                int offset = chord.kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT ? 7 : 11;
                assert(standard_tonality_numeral_root_semitone(&text, tonal_mode, &semitones));
                assert(semitones == (harmony_applied_scale_semitone(harmony_mode, target) + offset) % 12);

                /* the canonical spelling of the same chord reads back to the same value */
                char canonical[32];
                StandardTonalityNumeralText again;
                assert(standard_tonality_numeral_format(&text, canonical, sizeof(canonical)));
                assert(standard_tonality_numeral_parse(canonical, &again) && memcmp(&again, &text, sizeof(text)) == 0);
                checked++;
            }
        }
    }
    assert(checked > 20);
    return 0;
}
