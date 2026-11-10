# standard-harmony

Theory Framework instance `standard-harmony-v1`: the basics of harmony in
common-practice Western music (Open Music Theory, Introduction to Harmony,
Cadences, and Phrase Endings). Path `frameworks/standard-harmony/`, tests in
`frameworks/standard-harmony/tests/`. A Framework extension module physically
separate from `src/theory/`, the same shape as `frameworks/standard-tonality/`.

It depends on no other framework. Chords are plain data (`HarmonyChord`,
`harmony_chord.h`: the scale degree of the root, 1 to 7, the inversion, and
whether it is a seventh chord, and whether it is a cadential 6/4), not
Roman numerals (`standard-tonality`) and not pitches. Voicing the inner voices is
`standard-voicing`'s. File names carry a `harmony_` prefix so they never collide
with the other frameworks' headers on the include path.

The entities these rules use live in `src/semantics/`: `CadenceType` (perfect authentic, imperfect
authentic, half, subverted), `NonChordTone` (the embellishing tones), with
`BeatStrength` for the beat. They name things and carry no rule; which chords have
which function, and which progressions make which cadence, is this framework's.

The cadence vocabulary (`harmony_cadence_kinds.h`, R-415) gives each cadence kind a name, a label, an abbreviation, a core `CadenceType`, a strength and whether it is conclusive. It holds the seven core types and three kinds the core has no value for: evaded (a subverted cadence), abandoned (no cadence, so core unknown and never ranked) and medial caesura (a half cadence that ends a transition). `harmony_cadence_strength` reads its strength from the table, which is the one owner of cadence strength. New kinds are entries here, never new `CadenceType` values.

`ModulationKind` (modulation_kind.h, how a new key is introduced) is single-framework, so it lives in this module, not in `src/semantics/`.

`SixFourType` (six_four_type.h, cadential, passing, neighbor, arpeggiating) is single-framework, so it lives in this module, not in `src/semantics/`.

`PhraseModelLabel` (phrase_model_label.h, Tb, PD, D, Te) is single-framework, so it lives in this module, not in `src/semantics/`.

`SeventhResolution` (seventh_resolution.h, default, incomplete, leading-tone drop) is single-framework, so it lives in this module, not in `src/semantics/`.

`HarmonicFunction` (harmonic_function.h, tonic, predominant, dominant) is single-framework, so it lives in this module, not in `src/semantics/`.

The **analysis adapter** (`analysis/`) turns chord segments into this framework's inputs and returns function,
phrase model, six-four, prolongation and cadence Interpretations (PAC and IAC told apart by the soprano). See `docs/modules/standard-harmony-analysis.md`.

## Scope

Every rule group has Rule/Constraint pairs referencing this Framework plus pure check functions, and a `Preference` for each soft rule. The groups are documented in two files:

- [Rules](../../docs/modules/standard-harmony-rules.md): harmonic function, phrase model rules, cadence rules, V7 resolution, strong predominants, embellishing tones, cadential 6/4, 6/4 chords, cadence hearing and writing rules, La in the bass, the mediant, predominant sevenths.
- [Prolongation and keys](../../docs/modules/standard-harmony-prolongation-and-keys.md): tonic prolongation, plagal motion, phrase model labeling, tonicization, modulation and closely related keys.

## Not covered

- **Inner voices and doubling.** Filling in the inner voices (the notes still
  needed to complete each chord, doubling the bass in a root position chord) is
  `standard-voicing`'s.
- **Hearing.** The caller says whether a beginning follows and whether material
  repeats; this module does not detect either.
- **V7 inversions.** Only root position V7 to root position I is modeled; V6/5,
  V4/3, and V4/2 are not.
- **V7 after a predominant.** The strong predominant rules end on a V triad; V7 is
  `harmony_seventh_rules.h`'s, and the two are not combined.
- **Parallels shared across frameworks.** `standard-counterpoint`,
  `standard-voicing`, `scale-harmonization`, and this module each carry their own
  parallel fifths and octaves check; frameworks do not depend on each other.
- **Voicing the V7.** `standard-voicing`'s `SEVENTH_CHORD` doubling rule says a
  seventh chord has four notes with none doubled, which the incomplete V7 (the fifth
  omitted, the root doubled) breaks. The two frameworks are independent, so this
  one allows the incomplete V7; relaxing that voicing rule is a separate change.
- **Other embellishing tones.** The double neighbor and the changing tone are
  `NonChordTone` values but not in this chapter, so a gesture of them is not
  classified (`standard-counterpoint` has them). A pedal tone is classified as a
  three-note static gesture, not as a long note under changing chords. Chromatic
  steps are not modeled, and neither is the interval with the bass: whether a note
  is consonant with the bass is a chord-tone flag the caller sets.
- **Labeling a 6/4 in an analysis.** The classification says which kind a given three-chord
  gesture is, but `HarmonyChord` itself only marks the cadential one. How a passing or
  neighbor 6/4 counts in the phrase model (a V6/4 passing between tonics belongs to the tonic
  area, unlike the cadential 6/4, which is dominant) is not modeled: the phrase rules use each
  chord's own function.
- **vii4/2.** The leading-tone seventh chord in third inversion, with le in the bass, is
  very rare and is not an inversion of V7: le falls to sol in the bass, to a chord with sol
  in the bass. It is not modeled. Other uses of the leading-tone chord in place of V or V7
  (cadences, the strong predominant ending) are not modeled either.
- **Other prolongations.** Prolonging tonic by holding or repeating a chord has no
  rule beyond the chord repeating. Progressions that begin on I6, and a phrase
  that begins by prolonging tonic as a rule of the phrase model, are not modeled.
  Plagal motion is not a link in a prolongation chain: the chain needs V6 or an inverted
  V7 in the middle.
  In minor, raising le to la to avoid the augmented second in the IV6 prolongation is not
  modeled (accidentals are not), so the progression is only a soft "avoid" there.
  The same holds for the mediant: III has te, not ti, in minor, and te descending to le is not
  distinguished from the major mode's ti to la.
- **Tonicization and keys.** Tonicization is not a modulation: nothing tracks a temporary key, and
  the applied chord's label is the whole result. The phrase model (`harmony_phrase_labels.h`)
  does not label applied chords, which are not `HarmonyChord`s. Whether a chromatic note belongs to
  the chord or is an embellishing tone is the analyst's. The applied chord's inversion and the
  doubling in its voices are not modeled beyond the notes of the chord, and tonicizing a chord
  with an applied half-diminished seventh is only a soft rule.
- **Finding modulations.** Noticing repeated accidentals, backing up a chord
  to find the pivot, hearing x/ii, y/ii, z/ii over and over, and deciding where a passage sits on
  the spectrum are the analyst's; the facts are given. Writing advice for a modulating phrase (do
  not cadence too quickly after the pivot, use deceptive motion to avoid a cadence in the new key
  and then retry) is not a rule, since it gives no number. Modulation to keys beyond the closely
  related ones, and pivots through the harmonic minor's V or vii diminished, are not modeled.
  Predominant sevenths in minor have altered notes (ii7 is half diminished), which are not
  modeled. The treatment rules cover each predominant seventh alone; the progressions ii6/5
  for ii6, I ii4/3 I6, and ii4/2 in place of a strong predominant are not separate subjects.
  Hyperbeats are not modeled, only the beat strength of each chord.
- **The analysis procedure.** Guessing the harmony from the bass line is the analyst's. `frameworks/standard-harmony/analysis/` finds cadences, phrases, functions, six-fours and prolongations from the chord segments it is given (`docs/modules/standard-harmony-analysis.md`). The bass line hints (sol
  at the cadence, fa or re under a strong predominant) are the cadence and strong
  predominant rules'.
- **Other cadences.** Plagal and deceptive endings are not cadences in this
  chapter's vocabulary, so a progression like IV to I is reported as not
  cadential. A phrase that really ends on (IV)-I exists but is not modeled: plagal
  motion has no placement for a phrase ending, and is never a `CadenceType`. V going to iii is not stylistically normative and is not a recognized progression. Deceptive motion is likewise never a `CadenceType`, and the phrygian half cadence is a half cadence the framework recognizes, not a type of its own. A phrygian half cadence approached from i, with the soprano me fa sol, needs the chords before and is not modeled. Chords before V other than as "x" are not modeled.
- **The strength of an IAC.** An imperfect authentic cadence can be made more or
  less strong by its melody, rhythm, and what follows; that is not modeled.
- **Minor-mode melodic alterations** beyond the leading tone in V.

## Voice leading analysis

`analysis/analysis_resolution.h` checks seventh and leading-tone resolution per voice from a voice-pitch grid and a chord run list, using `harmony_resolution_rules.h`. See `docs/modules/voice-leading-analysis.md`.

## Neutral analysis input

`harmony_mode_of_analysis_key` and `harmony_segment_input_set_segment` (`analysis/analysis_input.h`) read an `AnalysisKey` and an `AnalysisSegment` (`semantics/analysis_key.h`, `analysis_segment.h`). `HarmonyVoiceCell` and `HarmonyVoiceGrid` are the shared `AnalysisVoiceCell` and `AnalysisVoiceGrid` (`semantics/voice_grid.h`); `segment_ids` is not read here.
