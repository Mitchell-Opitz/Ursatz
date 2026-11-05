# standard-tonality analysis adapter (L3)

[Docs index](../README.md): the Ursatz docs by topic.

`frameworks/standard-tonality/analysis/` turns L2 facts into standard-tonality's inputs and returns Interpretations. It is the Roman numeral adapter: one Interpretation per chord segment, in the segment's local key, with applied chords. It does not assign harmonic function or cadences (L3.2, L3.3).

## Input: kernel data only

standard-tonality includes no other framework, so the adapter reads only kernel-level data (`chord_input.h`):

- the segment's kernel `Chord` and its `ChordStructure` (`interpretation/chord.h`): root pitch class, quality as a root-relative interval set, bass pitch class;
- the kernel `Key` of each key region and its `KeyStructure` (`interpretation/key.h`): tonic pitch class and mode;
- the segment's 12 pitch-class weights, as a plain `double[12]` (index 0 = C).

`ChordStructure` and `KeyStructure` are additive: a Chord or Key built with `chord_create` / `key_create` reports none. common-practice-minimal fills them when it builds a chord (`weighted_chord_identification`) or a key (`common_practice_build_key`). The architecture test scans `analysis/` as well as the module's top directory.

`standard_tonality_build_chord_inputs` returns one record per segment, in order. A segment with no Chord, no structure, no key region, or a Key with no structure passes through as no-numeral (`has_numeral` false).

## Local key

A segment is read in the key region that contains its start time. Regions are given in time order; each runs to the next region's start and the last runs on. A segment before the first region has no key.

## Roman numerals

`standard_tonality_build_roman_numerals` (`roman_numeral_adapter.h`). The root's distance above the tonic gives the degree: the major scale, or the natural minor scale with the raised leading tone as degree 7. The interval set gives the quality (five seventh qualities, four triad qualities). The degree and quality must be ones the key gives (`standard_tonality_roman_numeral_fits_key`, the same test as `standard_tonality_identify_roman_numeral`): in minor, V and vii-diminished from the raised seventh, VII from the subtonic, a minor v from the natural scale. A chord the key does not give has no numeral.

Inversion comes from the bass: the interval from the root to the bass picks the chord member in the bass. Triad: none, `6`, `6/4`. Seventh chord: `7`, `6/5`, `4/3`, `4/2`. The bass is the lowest sounding chord tone (within an octave of the lowest pitch; lower non-chord tones are ignored); with no chord tone there, the previous segment's bass holds while it is a member of the chord, else the chord reads as root position. A caller that passes no segment pitches gets the Chord's own bass. The applied-chord reading keeps scoring against the Chord's own bass (`sounding_bass_pitch_class`). An inverted seventh chord's `7` is replaced by its figure (V6/5).

Each matched segment gets one `Interpretation`: target = the segment's Sonority id, FrameworkReference standard-tonality, claim = a `StandardTonalityRomanClaim` (label, degree, quality, inversion, applied, applied kind, applied target degree), and a scalar confidence.

Uncertainty = 1 − (1 − chord uncertainty)(1 − key region uncertainty). The chord's is 1 minus its Interpretation's scalar confidence; the region's is passed with the region (a Key's own Interpretation does not carry it).

## Applied chords

Chord identification is diatonic only, so a V/V (D F# A in C) arrives as a ii with F# as a leftover tone. `applied_chords.h` re-checks each segment that has a diatonic reading and weights.

Candidates for each target x (major: ii, iii, IV, V, vi; minor: III, iv, V, VI, VII): V/x, V7/x, vii-diminished/x, vii-diminished-7/x. Scoring is the L2.5 formula over the passed weights, with the L2.5 defaults:

    score = weight of the chord's pitch classes / total weight
            - 0.15 if the chord's third has no weight
            + 0.05 if the bass pitch class is a chord tone

(The constants are a copy: a framework does not include another. A candidate whose root does not sound is not eligible.) The segment is relabelled as applied only when all hold:

1. the chromatic share (weight outside the key; in minor the natural scale plus the raised leading tone) is at least `chromatic_min` (default 0.15);
2. the best applied candidate scores strictly above the diatonic chord;
3. the next segment whose chord is not a repeat of this one (same root) has the target's root, or the target's submediant root (nine semitones above the target's root: a deceptive-style resolution, recorded as `deceptive`).

Otherwise the diatonic label stands. The kind is the existing `AppliedChordKind` (`src/semantics/`, shared with standard-harmony, so it stays there): `SECONDARY_DOMINANT` for V and V7, `SECONDARY_LEADING_TONE` for the diminished chords. The label is the numeral as it reads in the tonicized key, a slash, and the target numeral: V7/V, V6/5/V, vii°6/V. The label is written by `standard_tonality_numeral_format_styled` (`roman_numeral_grammar.h`): the same grammar `standard_tonality_numeral_parse` reads, in the display style (`°`, `∅`, figures `6/5`) or, for a minor key, the RomanText style (`viio6/5`, `ii/o7`).

An applied chord is linked to the segment it resolves to by a `Relationship`, predicate `resolves-to`, source = the applied segment's Sonority id, target = the resolution segment's. The link is its own object (the durable claim pattern), owned by the result.

## Enum relocation

No enum in `src/semantics/` is used only by standard-tonality. `AppliedChordKind` is shared with standard-harmony. Nothing was relocated.

## Limits

- No modal mixture, Neapolitan or augmented sixth chords, and no chromatic chord other than the applied dominants and leading-tone chords listed.
- Applied labels need a resolution: a chromatic chord that is not followed by its target (or the target's submediant) keeps its diatonic label, and so does the last segment.
- Only a segment with a diatonic reading is re-checked. A chord outside the vocabulary of chord identification, or read in a key that does not give it, is not recovered.
- Chord identification is major-key and diatonic to one tonic (`common_practice_rank_candidates`), so chords foreign to that key never reach the adapter as such.
- A tonicization that key estimation reads as a region (D major in G) reads its chords in that key and not as applied chords.
- Half-diminished applied leading-tone sevenths are not candidates.
