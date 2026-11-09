# standard-tonality numerals and keys

The Roman numeral, analysis adapter and key function rule groups of `frameworks/standard-tonality/`. [Docs index](../README.md)

Framework overview: [frameworks/standard-tonality/README.md](../../frameworks/standard-tonality/README.md).

- **Roman numerals** (`roman_numeral_rules.h`): the numeral is the scale
  degree of the root (I to VII); uppercase for a major third above the
  root (major, augmented), lowercase for a minor third (minor,
  diminished, half-diminished); "°" for diminished, "+" for augmented,
  "∅" for half-diminished, and a 7 for sevenths (V7, ii7, ii∅7, vii°7).
  Major-major and major-minor sevenths share a label, read as dominant on
  V and VII and as major elsewhere. Parse and format, and a `Chord`
  labeled with the numeral. common-practice-minimal's own triad labels
  are separate and unchanged.
- **Roman numeral grammar** (`roman_numeral_grammar.h`): one public
  grammar for every numeral form, read into `StandardTonalityNumeralText`
  and written back in one canonical spelling. Case-sensitive:
  `[alteration] numeral [mark] [figure] ["/" target]`.

  | Part | Accepted |
  |---|---|
  | alteration | `b` `#` (also the UTF-8 flat and sharp signs) |
  | numeral | `I`..`VII` (major third), `i`..`vii` (minor third) |
  | mark | `o` or `°` diminished (lowercase); `+` augmented (uppercase, triad); `ø`, `∅`, `/o` or `%` half-diminished (lowercase, seventh figure required); `M` or `maj` major seventh (uppercase, seventh figure required) |
  | figure | triad `6`, `64`; seventh `7`, `65`, `43`, `42`, `2`; `6/4`, `6/5`, `4/3`, `4/2` as the same |
  | target | `/` then a numeral with its own alteration, no mark or figure: `V7/V`, `viio7/ii`, `V/bVI`; lowercase is a minor target |

  An uppercase numeral with a seventh figure and no mark is a dominant
  seventh, a lowercase one a minor seventh, `o` with a seventh figure a
  fully diminished seventh. Rejected: mixed case, a mark that
  contradicts the case, `+` with a seventh figure, `ø` or `M` without
  one, trailing text, empty input. Canonical form is ASCII `o` and `+`,
  `ø`, `M`, figures `6 64 7 65 43 42`; parse then format is stable.
  `standard_tonality_numeral_root_semitone` resolves an applied numeral
  through its target (major for an uppercase target, harmonic minor for a
  lowercase one). `standard_tonality_roman_numeral_parse`/`_format` and
  `standard_tonality_format_roman_reading` run on it unchanged: the older
  entry points keep `°`, `∅` and slashed figures (`style` display) or the
  RomanText spelling (`viio6/5`, `ii/o7`). standard-harmony keeps its own
  `harmony_applied_label`; a test checks its labels parse here.
- **Roman numeral analysis** (`roman_numeral_analysis_rules.h`): key
  labels (capital note name for major, "mi" or a lowercase letter for
  minor), the root's degree in a major or minor key, and the numeral of
  any chord tones in a key. V and vii° (and III+) in minor imply the
  raised leading tone. Voicing, doubling, an omitted fifth, and
  inversion do not change the numeral. A chord with a root outside the
  key, a quality the key does not give that degree, or a quality outside
  the five sevenths gets no numeral. Embellishing tones and inversion
  figures are other chapters'.

- **Analysis adapter** (`analysis/`): Roman numeral Interpretations per
  chord segment in its local key, with applied chords, read from kernel
  Chord and Key data only. See `docs/modules/standard-tonality-analysis.md`.
  Inversion comes from the lowest sounding chord tone: a lower non-chord
  tone in the bass does not make the chord root position. When no chord
  tone sounds in the bass register (within an octave of the lowest
  pitch), the previous segment's bass holds while it is a member of the
  chord, else the chord reads as root position. A caller that passes no
  segment pitches keeps the Chord's own bass.

  **Applied chords** (`analysis/applied_chords.h`) come two ways.
  *Identified:* when chord identification already read the chord as
  applied (common-practice-minimal with `applied_chords` on, R-380), its
  root and quality are outside the key's diatonic vocabulary. The reading
  falls back to `standard_tonality_read_applied_chord`: the input's root
  and intervals against V, V7, vii° and vii°7, the tonicized root (the
  root a fifth below, or a semitone above) being a target of the key (major
  ii to vi, minor III to VII on the natural scale). It reads no weights
  and no neighbours, and gives the numeral, the target and the inversion
  from the bass. *Re-checked:* a diatonic reading (a ii that is a V/V with
  its F#) is relabelled from the weights and the resolution, as before.
  `StandardTonalityAppliedConfig.recheck_diatonic` (default true) turns
  that second pass off for a caller whose identification reads applied
  chords itself; identified applied chords are never re-checked.

  The link (`resolves_to`, `resolution_index`, `deceptive`) is set only
  when the applied chord resolves: the next segment with a different root
  has a numeral and the target's root, or deceptively the target's
  submediant. An applied chord that does not resolve keeps its applied
  label and has no link.

- **Key functions** (`tonal_key.h`): degree and alteration of a note in a
  key and mode (`standard_tonality_degree_of`), the note at a degree
  (`standard_tonality_spell_degree`), semitones above the tonic of a degree
  (`standard_tonality_degree_semitone`), the signature of a key as a count
  of sharps or flats or as a `KeySignature`, the key of a signature count,
  relative keys, and steps around the circle of fifths. Modes are major
  and the three minor forms (`STANDARD_TONALITY_MODE_*`; melodic is the
  ascending form; a bare "minor" is never implied). `standard_tonality_mode_of_minor_form`
  and `standard_tonality_mode_of_roman_key_mode` map the older enums.
  Spelling arithmetic comes from `src/pitch/pitch_arithmetic.h`.
  `standard_tonality_key_tonic_of_pitch_class` spells the tonic of a key from its
  pitch class with the fewer accidentals in its signature (sharps on a tie; a
  minor key by its relative major), and `standard_tonality_chromatic_degree_label`
  names a semitone distance above the tonic ("1", "b2", ... "7").

How accidentals are written and ordered in a key signature is notation
and lives in `frameworks/standard-notation/`. Built on `KeySignature`
(`src/pitch/`), `Scale`, `ScaleDegree`, and `Key` stay the entity types;
scale-degree vocabulary lives here, not in their opaque labels.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.
