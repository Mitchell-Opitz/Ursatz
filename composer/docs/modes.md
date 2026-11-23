# Modes in the Composer

The Composer writes major and natural minor keys, and since P69 the five church modes (lydian, mixolydian, dorian, phrygian, locrian) as the key of a phrase (R-725). This page is the modal harmony: what a modal key is, how its chords are stacked, numbered and judged, and what a modal key does not write. Requests and settings for modal keys are added by the branches that open them. Back to the [Composer docs](README.md).

## What a modal key is

A modal key is a seven-step material that is neither the major nor the natural minor and names a cadence kind (`pitch_material_is_modal_key`, `library/theory/tonality/modal_chords.h`). The five church modes of `knowledge/material/omt.json` are the only ones. A key holds the material (`SpelledKey.material`); `composer_chords_modal` reads it, and `harmony_kind` gives the harmony layer the key as a `HarmonyKind`: the mode of its third (major or minor, as before), the modal material, and the cadence kind that material names. A major or minor key has no modal material and reads every table as before.

## Stacked chords

The chord on a degree is the material's third and fifth above it (`pitch_material_triad_on`), so the chords are the mode's own and nothing is raised: no leading tone, no dominant seventh. On C:

| mode | chords |
|---|---|
| lydian | C D e f#o G a b |
| mixolydian | C d eo F g a Bb |
| dorian | c d Eb F g ao Bb |
| phrygian | c Db Eb f go Ab bb |
| locrian | co Db eb f Gb Ab bb |

Upper case is major, lower case minor, `o` diminished. `composer_chord_tones` is the one owner of a chord's pitch classes: a major or minor key reads the mode tables, a modal key the stacked triad. The melody mask, the bass, the accompaniment and the recognizer's intervals all read it.

## Numerals

A numeral is relative to the mode (`harmony_roman_format_in`, `harmony_roman_parse_in`): its quality is the stacked triad's, so the chord on the fifth degree is `v` in C dorian and `V` in C major. A numeral of another quality, an alteration, a seventh or an applied chord is refused (borrowed chords come with modal mixture). The chord the mode avoids is a readable numeral (`vio` in dorian) that the Composer never writes.

## Functions and the phrase model

The function of a chord in a modal key (`modal_function_of`, `harmony_function_in`) is tonic for the first degree, dominant for an approach degree of the mode's cadence kind, and predominant for any other major or minor triad the mode does not avoid. The phrase model keeps its order (opening tonic, predominant, dominant, last chord) with these chords: the predominant is one chord drawn uniformly from the mode's primary chords that are neither the tonic nor an approach chord (none when none remains), and the dominant is the approach chord.

## Cadences

The cadence of a mode is its kind of `knowledge/vocabulary/persichetti.json`: the approach, then the arrival (OMT part VII chapter 11 and Persichetti chapter 2). A PAC or IAC ends on the approach and the arrival, a half cadence on the last approach chord.

| mode | approach | arrival | PAC | half |
|---|---|---|---|---|
| lydian | II | I | II I | ends on II |
| mixolydian | VII | I | VII I | ends on VII |
| dorian | IV | i | IV i | ends on IV |
| phrygian | II | i | II i | ends on II |
| locrian | V | io | V io | ends on V |

A deceptive or plagal cadence needs a dominant and is refused in a modal key with "the dorian cadence is the only one written in C dorian; DC and PC need a dominant" (`harmony_cadence_allowed`).

## The judge

The recognizer hears major and minor, so a modal phrase is not asked of it. The judge (`harmony_judge`) holds a modal phrase to the modal function table (a tonic, then a dominant), the cadence kind's ending, no avoided chord and root-position chords only, and scores the backward motion and the root progression as soft rules.

## What a modal key does not write

- Applied chords, dominant options, predominant forms and the prolongation figures with V, V6, V65, V43 or viio6: the embellishment operator does not apply, and a prolongation draws only the figures on the tonic and IV.
- Galant schemas. Scale sequences are written, with the tonic of a locrian key allowed in root position.
- Chords developed from a motive's tones: a motive writes its harmony as the degrees of a major or minor key, so the motive operator does not apply and the phrase model writes the chords.
- Pivot chords: no key change is written in a modal key.
