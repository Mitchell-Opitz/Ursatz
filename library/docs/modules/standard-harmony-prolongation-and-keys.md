# standard-harmony prolongation and keys

The rule groups of `frameworks/standard-harmony/` for tonic prolongation, plagal motion, phrase model labeling, tonicization, and modulation and closely related keys. [Docs index](../README.md)

Framework overview: [frameworks/standard-harmony/README.md](../../frameworks/standard-harmony/README.md).

- **Tonic prolongation** (`harmony_prolongation.h`, `harmony_prolongation_rules.h`,
  `harmony_prolongation_chain.h`), the way phrase beginnings extend the tonic's
  influence by alternating tonic with V6 or an inverted V7: three chords, I, then V6
  (the triad), V6/5, V4/3, or V4/2, then I or I6. The four progressions, by bass line:
  do ti do (I V6/5 I, or V6), do re mi (I V4/3 I6), do re do (I V4/3 I, less common),
  and do fa mi (I V4/2 I6); `harmony_prolongation_of` classifies three chords into one.
  The rules take the three chords in four voices as diatonic steps, with a beat strength
  for each. Hard: the progression is one of the four; the bass is its line (ti, re, or
  fa for the inversion); each chord has only its own notes (an inverted V7 has sol, ti,
  re, fa once each); ti goes up to do, fa down to mi, re a step to do or mi; the one
  exception is that in do re mi an upper voice on mi fa sol, in parallel tenths with the
  bass, takes fa up to sol; in minor, V has the leading tone; and no parallel fifths or
  octaves. Soft: avoid the V6 triad (prefer an inverted V7), avoid I V4/3 I (prefer
  I6), and the V on a weaker beat than both tonic chords. A chain is progressions that
  share their joining chord (the same chord, notes, and beat strength), which is always
  a root position tonic, in one mode; every link breaks no hard rule.
  The leading-tone chord (Prolongation at Phrase Beginnings using the Leading-Tone Chord)
  takes the place of the V with the same bass note: vii7 (root position, ti in the bass)
  for V6 or V6/5, vii6 (the triad, only in first inversion) or vii6/5 (re in the bass)
  for V4/3, and vii4/3 (fa in the bass) for V4/2, in the same four lines. In minor it is
  built on ti (`leading_tone`); in major its seventh is usually lowered, a fully
  diminished chord (`half_diminished` marks the rarer vii with a slashed circle, with la).
- **Plagal motion** (`harmony_plagal.h`, `harmony_plagal_rules.h`), (IV)-I as a
  prolongation of tonic, not a predominant: the IV is written in parentheses. It sits
  either after an authentic cadence or at a phrase beginning, and the caller gives which
  (`HarmonyPlagalPlacement`). IV is a root position triad; I is root position, or first
  inversion at a phrase beginning only. It is plagal motion, not a plagal cadence, since a
  cadence ends a phrase. The rules take the two chords in four voices as diatonic steps.
  Hard: the progression is (IV)-I in its placement; each chord has only its own notes, all
  three present, the bass on the root (the third in I6), and the bass doubled in root
  position; with both chords in root position, each upper voice moves by step or common tone;
  and no parallel fifths or octaves (in IV to I6, an upper voice that follows the bass down a
  step makes parallel octaves with it). Soft: do in the soprano after a cadence, and, at a
  phrase beginning, the less common choice of (IV) over an inverted V7.
  The notes are ti, re, fa, and le or la once each (the triad: ti, re, fa), and le or la
  falls a step to sol. Scale degrees do not tell le from la, so in the fully diminished
  chord re with the sixth degree above it is a diminished fifth, not a perfect one
  (resolving re and le to do and sol is no parallel fifths); in the half-diminished chord
  it is a perfect fifth, and parallel fifths are a hazard. Soft: prefer the fully
  diminished chord, and in the half-diminished chord keep re above la (impossible in
  vii6/5, where re is in the bass).
- **Phrase model labeling** (`harmony_phrase_labels.h`,
  `harmony_phrase_label_rules.h`), Performing Harmonic Analysis Using the Phrase
  Model: each phrase gets one set of the labels Tb, PD, D, Te
  (`PhraseModelLabel`), placed from the ending back. The cadence the listener heard
  (PAC, IAC, or HC) is given with the phrase. D goes on the cadential V or V7 (the last
  chord of a half cadence, the chord before the tonic in an authentic one); a cadential
  6/4 before it is part of the dominant unit and gets no label. Te goes on the final
  tonic of an authentic cadence (none for a half cadence). PD goes on the first strong
  predominant of the run of predominants just before the dominant, past a cadential
  6/4; a run of only weak predominants has none. Tb goes on the first chord when it is
  a tonic. A subverted cadence is not labeled. `harmony_phrase_model_label` returns
  false when the phrase does not end as its cadence says. Rules: hard, the phrase ends
  as its cadence says; soft, it starts on a tonic (a delayed or omitted tonic is rare,
  more often in Romantic music), and a strong predominant comes before the dominant.
- **Tonicization** (`harmony_applied.h`, `harmony_applied_rules.h`,
  `harmony_tonicization_frequency.h`, `harmony_tonicized_deceptive.h`), Tonicization: applied
  chords, chromatic chords that make a non-tonic chord sound like a temporary tonic
  (`AppliedChordKind`: secondary dominant or secondary leading-tone). An applied chord is a kind,
  a quality (major triad or dominant seventh; diminished triad, fully diminished seventh, or
  rarely half-diminished seventh), and the target chord, given as its scale degree: ii, iii, IV,
  V, vi in major and III, iv, V, VI, VII in minor (not the tonic, whose V is just V, and not the
  diminished chords, since there are no diminished keys). `harmony_applied_label` writes the
  label ("V7/V", "viio7/vi"); `harmony_applied_analyze` takes a chromatic chord's root (semitones
  above the tonic) and quality and returns the label, the target being the key in which the chord
  is V or vii; `harmony_applied_root_degree` and `harmony_applied_root_semitone` spell it; and
  `harmony_applied_from_diatonic_root` gives the applied V7 that is an altered diatonic chord (ii
  to II sharp, which is V7/V). The resolution rules take the applied chord, the chord after it,
  and four voices as diatonic steps with an alteration for each note (tonicization is chromatic).
  Hard: the chord after is the target (or, for V, the cadential 6/4); each voice is a note of the
  applied chord with its root, third, and seventh present; the secondary leading tone goes up a
  semitone (or down a semitone into the seventh of a following seventh chord); the secondary
  seventh goes down by step; and no parallels. Soft: not the rare half-diminished chord, and an
  altered note in the chord (applied chords nearly always have accidentals).
  `harmony_tonicization_frequency` ranks the target: V, IV, and ii (minor: V and iv) common, vi
  (minor: VI and III) frequent, iii and, in minor, VII uncommon. Tonicized deceptive motion is V,
  vii diminished 7 of vi, then vi (`harmony_tonicized_deceptive_is`).
- **Modulation and closely related keys** (`harmony_related_key.h`, `harmony_pivot.h`,
  `harmony_key_change.h`), Extended Tonicization and Modulation to Closely Related Keys. A key is
  given relative to the home key, as the scale degree of the diatonic chord whose root is its
  tonic and whether it is major or minor, so this module does not depend on spelled keys
  (`standard-tonality` has the circle of fifths and the relative keys by name). The closely
  related keys, within one accidental of the home key's signature, are the keys of its diatonic
  major and minor triads (the home key excluded): ii, iii, IV, V, vi in major; III, iv, v, VI, VII
  in minor (v is the natural-minor key). `harmony_key_relation_rank` calls V and vi in major and
  III and v in minor the common modulations. A pivot chord is a triad diatonic in both keys, with
  the same root and quality (natural scales; a seventh rides on the chord); `harmony_pivot_find`
  lists them as pairs of degrees and `harmony_pivot_label` writes "iv becomes ii".
  `harmony_pivot_rank` ranks a pivot: best a predominant in both keys, good a tonic that becomes a
  predominant, poor any pivot involving a dominant ("V becomes I"), fair the rest.
  `harmony_key_change_classify` takes the analyst's facts (chords in the new key, whether a
  cadence establishes it, whether the passage continues after it) and gives a `KeyChangeExtent`:
  two chords or fewer with no cadence is a tonicization, more with none an extended tonicization,
  a cadence a modulation (strong if it continues, weak if it goes right back home).
  `harmony_key_change_confirm` confirms a region's extent with cadences passed as arrival times
  (whole notes): a cadence arriving inside the region keeps a modulation (a short region with a
  cadence becomes a weak modulation); with none, the region is a tonicization however long it is.
  With no cadences given (NULL) the length rule stands. The Framework v1 chain passes the cadences
  it found, and the change out of an excursion takes the excursion's extent.
  `harmony_extended_tonicization_label` writes the Roman numeral for the bracket under an
  extended tonicization ("ii" for F minor in E-flat major). `harmony_modulation_kind` is pivot
  when a pivot is given and direct when not.
