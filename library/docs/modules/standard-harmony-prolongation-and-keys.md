# standard-harmony prolongation and keys

The rule groups of `frameworks/standard-harmony/` for tonic prolongation, plagal motion, phrase model labeling, tonicization, and modulation and closely related keys. [Docs index](../README.md)

Framework overview: [frameworks/standard-harmony/README.md](../../frameworks/standard-harmony/README.md).

- **Tonic prolongation** (`harmony_prolongation.h`), the way phrase beginnings extend the tonic's
  influence by alternating tonic with V6 or an inverted V7: three chords, I, then V6
  (the triad), V6/5, V4/3, or V4/2, then I or I6. The four progressions, by bass line:
  do ti do (I V6/5 I, or V6), do re mi (I V4/3 I6), do re do (I V4/3 I, less common),
  and do fa mi (I V4/2 I6); `harmony_prolongation_of` classifies three chords into one.
  The leading-tone chord (Prolongation at Phrase Beginnings using the Leading-Tone Chord)
  takes the place of the V with the same bass note: vii7 (root position, ti in the bass)
  for V6 or V6/5, vii6 (the triad, only in first inversion) or vii6/5 (re in the bass)
  for V4/3, and vii4/3 (fa in the bass) for V4/2, in the same four lines. In minor it is
  built on ti (`leading_tone`); in major its seventh is usually lowered, a fully
  diminished chord (`half_diminished` marks the rarer vii with a slashed circle, with la).
- **Plagal motion** (`harmony_plagal.h`), (IV)-I as a
  prolongation of tonic, not a predominant: the IV is written in parentheses. It sits
  either after an authentic cadence or at a phrase beginning, and the caller gives which
  (`HarmonyPlagalPlacement`). IV is a root position triad; I is root position, or first
  inversion at a phrase beginning only. It is plagal motion, not a plagal cadence, since a
  cadence ends a phrase. `harmony_plagal_progression_is` says whether two chords are (IV)-I
  in a placement.
- **Phrase model labeling** (`harmony_phrase_labels.h`), Performing Harmonic Analysis Using the Phrase
  Model: each phrase gets one set of the labels Tb, PD, D, Te
  (`PhraseModelLabel`), placed from the ending back. The cadence the listener heard
  (PAC, IAC, or HC) is given with the phrase. D goes on the cadential V or V7 (the last
  chord of a half cadence, the chord before the tonic in an authentic one); a cadential
  6/4 before it is part of the dominant unit and gets no label. Te goes on the final
  tonic of an authentic cadence (none for a half cadence). PD goes on the first strong
  predominant of the run of predominants just before the dominant, past a cadential
  6/4; a run of only weak predominants has none. Tb goes on the first chord when it is
  a tonic. A subverted cadence is not labeled. `harmony_phrase_model_label` returns
  false when the phrase does not end as its cadence says.
- **Tonicization** (`harmony_applied.h`), Tonicization: applied
  chords, chromatic chords that make a non-tonic chord sound like a temporary tonic
  (`AppliedChordKind`: secondary dominant or secondary leading-tone). An applied chord is a kind,
  a quality (major triad or dominant seventh; diminished triad, fully diminished seventh, or
  rarely half-diminished seventh), and the target chord, given as its scale degree: ii, iii, IV,
  V, vi in major and III, iv, V, VI, VII in minor (not the tonic, whose V is just V, and not the
  diminished chords, since there are no diminished keys). `harmony_applied_label` writes the
  label ("V7/V", "viio7/vi"), and `harmony_applied_root_degree` and
  `harmony_applied_root_semitone` spell its root.
- **Modulation and closely related keys** (`harmony_related_key.h`, `harmony_pivot.h`,
  `harmony_key_change.h`), Extended Tonicization and Modulation to Closely Related Keys. A key is
  given relative to the home key, as the scale degree of the diatonic chord whose root is its
  tonic and whether it is major or minor, so this module does not depend on spelled keys
  (`standard-tonality` has the circle of fifths and the relative keys by name). The closely
  related keys, within one accidental of the home key's signature, are the keys of its diatonic
  major and minor triads (the home key excluded): ii, iii, IV, V, vi in major; III, iv, v, VI, VII
  in minor (v is the natural-minor key). A pivot chord is a triad diatonic in both keys, with
  the same root and quality (natural scales; a seventh rides on the chord); `harmony_pivot_find`
  lists them as pairs of degrees.
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
