# Modes in the Composer

The Composer writes major and natural minor keys, and since P69 the five church modes (lydian, mixolydian, dorian, phrygian, locrian) as the key of a phrase (R-725). This page is the modal harmony: what a modal key is, how its chords are stacked, numbered and judged, and what a modal key does not write. Requests name a mode as the `material`; settings for modal keys come with the branches that add them. Back to the [Composer docs](README.md).

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

## Requests

A request names a mode as its `material` (`"dorian"`, `"lydian"`, `"mixolydian"`, `"phrygian"`, `"locrian"`) in the theme, piece, plan, grow, counterpoint and random motive requests, and `listen --material` takes it. A random motive in a mode has the default tonic of the major degree its steps rotate from: D for dorian, E phrygian, F lydian, G mixolydian, B locrian (A for the natural minor, C for the major). Keys are the same only when their materials are: a D natural minor motive is not a D dorian one. The old `mode` member stays major or minor.

The seventh is raised to a leading tone in the natural minor only (`composer_material_raises_seventh`): the modes keep their own, whatever their third. A word the Composer does not write (the harmonic and melodic minor, the synthetic scales) is refused with "the Composer writes major, minor and modal keys; '<word>' is not one of them". A section of a piece may be in the home's mode, or change between major and minor; a region in another mode is refused with "section 'b' is in C locrian, the piece in C major: a section changes mode only between major and minor".

A locrian key's tonic is voiced in the accompaniment as its root, third and the root an octave up: its diminished fifth is not sounded.

## The color note

Each church mode has characteristic steps (`characteristic_degrees` of its material entry): lydian the #4, mixolydian the b7, dorian the natural 6, phrygian the b2, locrian the b2 and b5 (R-732). Two settings ask for them where they are heard, and do nothing in a major or minor key:

- `motif.color_note` (0 to 8, default 4): a random motif in a modal key is kept only when one of its notes on a beat is a characteristic step: a strong beat (the first of a bar, or the third of four) for the first half of the tries, any beat for the rest, up to `color_note` tries of a setup; later tries keep any motif. 0 asks for none.
- `melody.color_note` (0 to 20, default 2): a melody note the operator places on a strong beat or held for a cell, that is neither the first nor the last of its idea, takes the characteristic step among its chord tones nearest the ideal pitch when a draw of `color_note + 1` falls below `color_note`. 0 never takes it and draws nothing; the notes of a developed motif are not placed by this operator, so it moves only a few notes of a theme.

A motif's drawn chords in a modal key are the tonic and the mode's primary chords, which hold its characteristic step (Persichetti ch. 2): the drawn V is the cadence kind's approach chord and the drawn IV the primary chord that is neither the tonic nor an approach chord (`composer_modal_plan_degree`). `I-V` and `I-IV` then read on C as dorian IV and ii, mixolydian VII and v, lydian II and vii, phrygian II and vii, locrian V and II. A plan given with `motif.plan` keeps its degrees. Without this, lydian's IV and phrygian's v (chords the modes avoid) held no characteristic step.

Measured at P69 B5 over twenty sentences and twenty periods of each mode from seed 500 (`composed`, then the characteristic step on a strong beat): dorian 14 (12 of 14) and 16 (13 of 16), mixolydian 13 (12 of 13) and 15 (14 of 15), lydian 17 (16 of 17) and 17 (16 of 17), phrygian 16 (16 of 16) and 15 (13 of 15), locrian 19 (19 of 19) and 17 (17 of 17); no chord outside the mode's collection, at most 0.015 of the melody notes outside it, and every chord and every cadence holds the step. With both settings at 0 a sentence in mixolydian puts it on a strong beat in 8 of 15 themes instead of 14 of 15.

## Modal mixture

A chord taken from another mode on the key's tonic is a borrowed chord (R-729, OMT V ch. 1): the triad on a degree of that mode, written with the numeral of its root and quality against the key's own steps (`iv` and `bVI` in C major, from the natural minor; `IV` in C minor, from dorian). `HarmonyChord.borrowed_from` is the material, the plan's harmony payload keeps it as `borrowed_from` (a material id, only when set; format 6) and the theme response's chord rows show it. Every consumer reads the chord's tones from `composer_chord_tones`, which gives the borrowed triad. The recognizer and the judge read the key's own chord on that degree (mixture is judged by function, as the predominant that stands in its place).

A numeral the key's own chords do not read (an alteration, or another quality than the key's triad there) names a borrowed chord when a mode holds it: the nearest in brightness, darker before brighter at a tie (`composer_chords_borrow_source`). A seventh, an applied chord, an augmented triad and the tonic are refused; a key read without the run's chords borrows nothing. The theme request's `chords` read an altered numeral as a major triad (the chord text has no case: `"I bVI bVII I"`), the piece request's `harmony` progression any case (`"i IV i"` in C minor); both need the snapshot, which every request has.

`harmony.mixture` (R-729, off by default) lets the phrase model borrow:

- `share` (a ratio `n/d`, 0/1 to 1/1, default 0/1): the chance each predominant chord of the phrase model's run is borrowed. 0 draws nothing, so no output moves.
- `from` (1 to 6 relation words, default `parallel`, `one step darker`, `one step brighter`): a borrowed chord is the same degree's triad of the first of these modes whose triad differs from the key's, is major, minor or diminished and is not avoided there; a later predominant of the run borrows from the mode the run already uses when it can. The tonic and the cadence's approach chords are never borrowed.

The settings reach only the chords the phrase model deals: a theme whose motives have chords of their own keeps them, so `share` moves a theme with motives given no harmony and the harmony around given chords. In a modal key the same applies, with the mode's own predominant chords.

## Mode shifts in pieces

A region of a piece is in any material the Composer writes, on one tonic or another (R-611, R-730): the Persona 5 move, A in C locrian and B in C natural minor, `two steps brighter` of A. `region_material_written` is `composer_material_supported`; the harmonic minor and the synthetic scales are refused with "the Composer writes major, minor and modal keys". A theme given to a section is read in that section's key, so its material must be the section's: `return_key_shift` and the theme fill compare the materials exactly, and a request that gives a C natural minor theme to a C locrian region is refused at the section's `fill` with "the theme is in C natural minor, the section in C locrian". A section that returns another is in the key of the section it returns.

Each section row of a piece response has `brightness` (`{"numerator", "denominator"}`, null for a section without notes): the measure of R-730 over the section's melody, bass and inner voices, against the tonic and material of the key lane at its start. A diatonic mode whose notes all sound measures its rank, locrian 1 to lydian 7. For the R-611 piece (seed 1, themes drawn from a motive with no harmony) A measures 1 and B 4: B is more than 3/2 brighter than A, which `test_ternary_modes` asserts. The `brightness` dimension is not a contrast aspect; a brightness target is a region relation.

## What a modal key does not write

- Applied chords, dominant options, predominant forms and the prolongation figures with V6, V65, V43 or viio6: the embellishment operator does not apply, and a prolongation draws the figures on the tonic, IV and the root-position dominant (the approach chord of the mode's cadence).
- Galant schemas. Scale sequences are written, with the tonic of a locrian key allowed in root position.
- Pivot chords: no key change is written in a modal key. Sections in another mode wait for mode shifts between sections.
- A deceptive or plagal cadence (see Cadences).
- In a locrian cadence the approach chord is written in first inversion (V6 then io), as its root lies a tritone from the tonic's; the bass between a V and an io leaps that tritone where it must, which the other modes never do.
