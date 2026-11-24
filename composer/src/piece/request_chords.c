#include "piece/request_chords.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <theory/harmony/harmony_chord.h>
#include <theory/tonality/roman_numeral_grammar.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>

#include "structure/plan_util.h"

static UrsatzStatus fail(UrsatzError *error, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s", path ? path : "");
    }
    return URSATZ_ERR_REQUEST;
}

/* The chords a request gives, and what a list of them is read as: see request_chords.h. */
UrsatzStatus request_read_chords(const ComposerChords *tones, const cJSON *list, const char *base, const char *what, long first_bar, const PlanMeter *meter,
                                   const SpelledKey *key, long bars, GivenChord *chords, char numerals[][PLAN_ROMAN_NUMERAL_MAX], size_t capacity,
                                   size_t *chord_count, UrsatzError *error) {
    if (!list || cJSON_IsNull(list)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsArray(list)) {
        return fail(error, base,
                        "\"chords\" must be a list of {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
    }
    Rational bar_length;
    if (plan_meter_bar_length(meter, &bar_length) != PLAN_OK) {
        return fail(error, "/meter", "the meter has no bar length");
    }
    Rational beat;
    Rational division;
    if (!plan_meter_grid(*meter, &beat, &division)) {
        return fail(error, "/meter", "the meter has no rhythmic grid");
    }
    bool seen[REQUEST_CHORDS_BARS_MAX] = {false};
    const cJSON *entry = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(entry, list) {
        char path[160];
        snprintf(path, sizeof path, "%.140s/%zu", base, index++);
        const cJSON *bar_item = cJSON_IsObject(entry) ? cJSON_GetObjectItemCaseSensitive(entry, "bar") : NULL;
        const cJSON *text_item = cJSON_IsObject(entry) ? cJSON_GetObjectItemCaseSensitive(entry, "text") : NULL;
        const char *text = cJSON_IsString(text_item) ? text_item->valuestring : NULL;
        if (!cJSON_IsObject(entry) || !cJSON_IsNumber(bar_item) || !text) {
            return fail(error, path,
                            "each of \"chords\" must be {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
        }
        long bar = (long)bar_item->valuedouble;
        if ((double)bar != bar_item->valuedouble || bar < first_bar || bar >= first_bar + bars || bar - first_bar >= REQUEST_CHORDS_BARS_MAX) {
            return fail(error, path, "chords bar %g: %s has bars %ld to %ld",
                        bar_item->valuedouble, what, first_bar, first_bar + bars - 1);
        }
        if (seen[bar - first_bar]) {
            return fail(error, path, "chords bar %ld: given twice", bar);
        }
        seen[bar - first_bar] = true;
        MotiveSketchChord parsed[MOTIVE_MAX_CHORDS];
        size_t count = 0;
        MotiveError motive_error;
        if (motive_harmony_parse_in(tones != NULL ? tones->knowledge : NULL, text, meter, parsed, &count, &motive_error) != MOTIVE_OK) {
            return fail(error, path, "chords bar %ld: '%s' at %s: %s", bar, text,
                            motive_error.location, motive_error.message);
        }
        if (count == 0 || count > 2) {
            return fail(error, path, "chords bar %ld: write 1 or 2 chords, not %zu", bar, count);
        }
        if (*chord_count + count > capacity) {
            return fail(error, base, "chords: too many chords");
        }
        Rational bar_start;
        if (rational_mul_int(bar_length, (long long)(bar - first_bar), &bar_start) != RATIONAL_OK) {
            return fail(error, path, "chords bar %ld: the bar is out of range", bar);
        }
        for (size_t c = 0; c < count; c++) {
            const MotiveSketchChord *chord = &parsed[c];
            Rational divisions;
            Rational end = bar_length;
            /* An onset is a whole number of the meter's division (half a beat, a third in a compound meter). */
            if (rational_div(chord->onset, division, &divisions) != RATIONAL_OK || divisions.den != 1) {
                return fail(error, path, "chords bar %ld: a chord starts on a beat or a half beat",
                                bar);
            }
            Rational cell;
            Rational in_cells;
            if (!given_cell_length(meter, &cell) ||
                rational_make(chord->onset.num * (long)cell.den, chord->onset.den * (long long)cell.num, &in_cells) !=
                    RATIONAL_OK ||
                in_cells.den != 1) {
                /* The harmony and bass operators write chords on the cells of the meter only (see docs/theme-request.md). */
                long beats = cell.num * (long)meter->beat_unit / (long)cell.den;
                return rational_cmp(cell, bar_length) == 0
                           ? fail(error, path, "chords bar %ld: a bar of %d/%d takes one chord", bar,
                                      meter->beats_per_bar, meter->beat_unit)
                           : fail(error, path,
                                      "chords bar %ld: in %d/%d a chord starts only every %ld beats (@%ld)", bar,
                                      meter->beats_per_bar, meter->beat_unit, beats, beats);
            }
            if (rational_cmp(chord->onset, bar_length) >= 0) {
                return fail(error, path, "chords bar %ld: a chord starts inside the bar", bar);
            }
            size_t at = *chord_count;
            if (chord->alteration != 0) {
                /* A borrowed chord (R-729): the altered numeral is a major triad ("bVI", "bII": the chord text has no case), read in the
                   key by the nearest mode that holds it. */
                HarmonyKind kind = harmony_kind(tones, key);
                HarmonyChord borrowed;
                char altered[PLAN_ROMAN_NUMERAL_MAX];
                StandardTonalityNumeralText numeral;
                memset(&numeral, 0, sizeof(numeral));
                numeral.alteration = chord->alteration;
                numeral.degree = chord->degree;
                numeral.inversion = chord->inversion;
                numeral.quality = STANDARD_TONALITY_NUMERAL_MAJOR;
                if (tones == NULL || chord->seventh || !standard_tonality_numeral_format(&numeral, altered, sizeof(altered)) ||
                    !harmony_roman_parse_in(&kind, altered, &borrowed) || !harmony_roman_format_in(&kind, &borrowed, numerals[at])) {
                    return fail(error, path, tones == NULL ? "chords bar %ld: altered chords are not given; write a diatonic numeral"
                                                           : "chords bar %ld: that altered chord is not in a mode on the key's tonic",
                                bar);
                }
            } else {
                HarmonyChord harmony;
                memset(&harmony, 0, sizeof(harmony));
                harmony.degree = chord->degree;
                harmony.inversion = chord->inversion;
                harmony.seventh = chord->seventh;
                HarmonyKind plain = harmony_kind(tones, key);
                if (chord->color != NULL && (tones == NULL || chord->inversion != 0 || chord->seventh ||
                                             !composer_color_fits(tones, harmony_key_mode(key), plain.modal, &harmony, chord->color))) {
                    return fail(error, path, "chords bar %ld: that color chord does not fit the key (a color type goes on a triad of the key whose third and fifth are its own)", bar);
                }
                harmony.color = chord->color;
                if (!(chord->color != NULL ? harmony_roman_format_in(&plain, &harmony, numerals[at])
                                           : harmony_roman_format(harmony_key_mode(key), &harmony, numerals[at]))) {
                    return fail(error, path, "chords bar %ld: that chord does not exist in the key", bar);
                }
            }
            if (c + 1 < count) {
                end = parsed[c + 1].onset;
            }
            GivenChord *given = &chords[at];
            given->numeral = numerals[at];
            if (rational_add(bar_start, chord->onset, &given->onset) != RATIONAL_OK ||
                rational_sub(end, chord->onset, &given->length) != RATIONAL_OK) {
                return fail(error, path, "chords bar %ld: the chord has no length", bar);
            }
            (*chord_count)++;
        }
    }
    return URSATZ_OK;
}
