#include <assert.h>
#include <string.h>

#include <ursatz_composer/harmony_phrase.h>

static const KeyStructureMode MODES[2] = {KEY_STRUCTURE_MODE_MAJOR, KEY_STRUCTURE_MODE_MINOR};

static HarmonyMode harmony_mode_of(KeyStructureMode mode) {
    return mode == KEY_STRUCTURE_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

/* Every chord that can be formatted reads back as the same chord, and no two chords share a numeral. */
static void test_diatonic_round_trip(void) {
    for (int m = 0; m < 2; m++) {
        char seen[64][PLAN_ROMAN_NUMERAL_MAX];
        int seen_count = 0;
        for (int degree = 1; degree <= 7; degree++) {
            for (int leading = 0; leading < 2; leading++) {
                for (int seventh = 0; seventh < 2; seventh++) {
                    for (int inversion = 0; inversion < 4; inversion++) {
                        HarmonyChord chord;
                        memset(&chord, 0, sizeof(chord));
                        chord.degree = degree;
                        chord.leading_tone = leading;
                        chord.seventh = seventh;
                        chord.inversion = inversion;
                        char roman[PLAN_ROMAN_NUMERAL_MAX];
                        if (!harmony_chord_valid(&chord)) {
                            assert(!harmony_roman_format(MODES[m], &chord, roman));
                            continue;
                        }
                        /* The leading-tone chord of a major key is the chord on 7, with no separate form. */
                        if (leading && MODES[m] == KEY_STRUCTURE_MODE_MAJOR) {
                            continue;
                        }
                        assert(harmony_roman_format(MODES[m], &chord, roman));
                        for (int i = 0; i < seen_count; i++) {
                            assert(strcmp(seen[i], roman) != 0);
                        }
                        strcpy(seen[seen_count++], roman);

                        HarmonyChord parsed;
                        assert(harmony_roman_parse(MODES[m], roman, &parsed));
                        assert(parsed.degree == chord.degree && parsed.inversion == chord.inversion &&
                               parsed.seventh == chord.seventh && parsed.leading_tone == chord.leading_tone);
                    }
                }
            }
        }
    }
}

static void test_known_numerals(void) {
    HarmonyChord chord;
    assert(harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "I6/4", &chord));
    assert(chord.degree == 1 && chord.inversion == 2 && !chord.seventh && !chord.cadential);
    assert(harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "V4/3", &chord));
    assert(chord.degree == 5 && chord.inversion == 2 && chord.seventh);
    assert(harmony_roman_parse(KEY_STRUCTURE_MODE_MINOR, "iv6", &chord));
    assert(chord.degree == 4 && chord.inversion == 1);
    assert(harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "ii6/5", &chord));
    assert(chord.degree == 2 && chord.inversion == 1 && chord.seventh);
    assert(!harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "V9", &chord));
    assert(!harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "v", &chord));
    assert(!harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "V/V", &chord));
    assert(!harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, "", &chord));
    assert(!harmony_roman_parse(KEY_STRUCTURE_MODE_MAJOR, NULL, &chord));
}

static void test_applied_round_trip(void) {
    for (int m = 0; m < 2; m++) {
        int formatted = 0;
        for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
            for (int target = 1; target <= 7; target++) {
                HarmonyAppliedChord applied;
                memset(&applied, 0, sizeof(applied));
                applied.quality = (HarmonyAppliedQuality)quality;
                applied.target = target;
                assert(harmony_applied_quality_kind(applied.quality, &applied.kind));
                char roman[PLAN_ROMAN_NUMERAL_MAX];
                if (!harmony_applied_valid(harmony_mode_of(MODES[m]), &applied)) {
                    assert(!harmony_roman_format_applied(MODES[m], &applied, roman));
                    continue;
                }
                assert(harmony_roman_format_applied(MODES[m], &applied, roman));
                HarmonyAppliedChord parsed;
                assert(harmony_roman_parse_applied(MODES[m], roman, &parsed));
                assert(parsed.quality == applied.quality && parsed.target == applied.target);
                /* An applied numeral is never a diatonic one. */
                HarmonyChord chord;
                assert(!harmony_roman_parse(MODES[m], roman, &chord));
                formatted++;
            }
        }
        assert(formatted > 0);
    }
    HarmonyAppliedChord parsed;
    assert(harmony_roman_parse_applied(KEY_STRUCTURE_MODE_MAJOR, "V7/V", &parsed));
    assert(parsed.quality == HARMONY_APPLIED_DOMINANT_SEVENTH && parsed.target == 5);
    assert(!harmony_roman_parse_applied(KEY_STRUCTURE_MODE_MAJOR, "V", &parsed));
    assert(!harmony_roman_parse_applied(KEY_STRUCTURE_MODE_MAJOR, "V/I", &parsed));
}

static void test_inverted_applied(void) {
    for (int m = 0; m < 2; m++) {
        int formatted = 0;
        for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
            for (int target = 1; target <= 7; target++) {
                for (int inversion = 0; inversion <= 3; inversion++) {
                    HarmonyAppliedChord applied;
                    memset(&applied, 0, sizeof(applied));
                    applied.quality = (HarmonyAppliedQuality)quality;
                    applied.target = target;
                    assert(harmony_applied_quality_kind(applied.quality, &applied.kind));
                    char roman[PLAN_ROMAN_NUMERAL_MAX];
                    if (!harmony_applied_valid(harmony_mode_of(MODES[m]), &applied) ||
                        !harmony_roman_format_applied_inverted(MODES[m], &applied, inversion, roman)) {
                        continue;
                    }
                    HarmonyAppliedChord parsed;
                    int parsed_inversion = -1;
                    assert(harmony_roman_parse_applied_inverted(MODES[m], roman, &parsed, &parsed_inversion));
                    assert(parsed.quality == applied.quality && parsed.target == applied.target);
                    assert(parsed_inversion == inversion);
                    /* The plain parser reads root position only. */
                    assert(harmony_roman_parse_applied(MODES[m], roman, &parsed) == (inversion == 0));
                    formatted++;
                }
            }
        }
        assert(formatted > 0);
    }
    HarmonyAppliedChord parsed;
    int inversion;
    assert(harmony_roman_parse_applied_inverted(KEY_STRUCTURE_MODE_MAJOR, "V6/5/ii", &parsed, &inversion));
    assert(parsed.quality == HARMONY_APPLIED_DOMINANT_SEVENTH && parsed.target == 2 && inversion == 1);
    assert(harmony_roman_parse_applied_inverted(KEY_STRUCTURE_MODE_MAJOR, "V4/2/IV", &parsed, &inversion));
    assert(parsed.target == 4 && inversion == 3);
    assert(harmony_roman_parse_applied_inverted(KEY_STRUCTURE_MODE_MAJOR, "V6/vi", &parsed, &inversion));
    assert(parsed.quality == HARMONY_APPLIED_MAJOR_TRIAD && inversion == 1);
    assert(!harmony_roman_parse_applied_inverted(KEY_STRUCTURE_MODE_MAJOR, "V6/4/V", &parsed, &inversion));
}

int main(void) {
    test_inverted_applied();
    test_diatonic_round_trip();
    test_known_numerals();
    test_applied_round_trip();
    return 0;
}
