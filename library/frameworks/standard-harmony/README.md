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

`ModulationKind` (modulation_kind.h, how a new key is introduced) is single-framework, so it lives in this module, not in `src/semantics/`.

`SixFourType` (six_four_type.h, cadential, passing, neighbor, arpeggiating) is single-framework, so it lives in this module, not in `src/semantics/`.

`PhraseModelLabel` (phrase_model_label.h, Tb, PD, D, Te) is single-framework, so it lives in this module, not in `src/semantics/`.

`SeventhResolution` (seventh_resolution.h, default, incomplete, leading-tone drop) is single-framework, so it lives in this module, not in `src/semantics/`.

`HarmonicFunction` (harmonic_function.h, tonic, predominant, dominant) is single-framework, so it lives in this module, not in `src/semantics/`.

The **analysis adapter** (`analysis/`) turns chord segments into this framework's inputs and returns function,
phrase model, six-four, prolongation and cadence Interpretations (PAC and IAC told apart by the soprano). See `docs/modules/standard-harmony-analysis.md`.

Cadence arrivals below `min_arrival_strength` (MEDIUM) are found when `allow_weak_arrival` is on (default true): a
root-position tonic directly after a root-position V that starts on a stronger beat than the tonic (at equal strength,
on the beat against a tonic off it, the weak beat and the weak half beat after it), and the tonic is long
(`arrival_min_beats`), followed by a soprano rest, or ends the piece. The claim records the arrival's beat strength
(`arrival_beat_strength`). Half cadences arriving on a weak beat are not found.

## Scope

Every rule group has Rule/Constraint pairs referencing this Framework plus pure
check functions. A rule that states what the music requires is hard; "usually",
"almost always", and "not common" features are soft. Each rule table registers an
evaluation callback on its Constraints, so `constraint_evaluate` runs the same check
(`harmony_rule_table_evaluate`) on a candidate `Value` tagged `standard-harmony/<table prefix>`
holding the rule's subject struct. Each soft rule also has a `Preference` (`harmony_*_preference`)
whose scoring callback makes `preference_rank` score the same candidate 1/1 when the rule holds
and 0/1 when it does not.

- **Harmonic function** (`harmony_functions.h`): the function of a chord by the
  scale degree of its root, whatever its inversion. Tonic: I (minor: i).
  Predominant, strong: IV and ii (minor: iv and ii diminished); weak: iii and vi
  (minor: VII, III, and VI). Dominant: V and vii diminished. In minor, degree 7 is
  VII (a weak predominant) unless the chord is marked as the one on the raised,
  leading-tone degree (vii, a dominant). `harmony_predominant_strength` says strong
  or weak.
- **Phrase model rules** (`harmony_phrase_rules.h`), on a run of chords in a mode.
  Hard: a tonic comes before a dominant (the least a phrase needs); when IV and ii
  (or ii6) both come before the dominant, in one run of predominants, ii comes after
  IV, never before (ii's root is a fifth from V's, which makes it the stronger
  predominant). Soft: no right to left motion (a predominant never follows a dominant, a tonic never follows a
  predominant); a weak predominant is followed by another predominant.
- **Cadence rules** (`harmony_cadence_rules.h`), on the last two chords and the
  soprano degree over the last. Hard: the progression is V to I or x to V.
  `harmony_cadence_classify` gives the type: V to I is a PAC when both chords are
  in root position and do is in the soprano, otherwise an IAC; x to V is a HC; a
  cadential progression the caller says is subverted is subverted.
  `harmony_cadence_strength` is the one strength order for every `CadenceType`: half,
  deceptive, plagal, authentic (unspecified), imperfect authentic, perfect authentic
  (a subverted cadence ranks 0, an unknown type -1). Kernel analysis receives it from
  its caller as a ranking function, and `harmony_cadence_compare` compares two types. A V7 to I in root position with do in the soprano is still a PAC.
  Soft: a half cadence does not end on V7 (the "19th-century HC", uncommon; it is
  still a half cadence).
- **V7 resolution rules** (`harmony_seventh_rules.h`), V7 to I in four voices, as
  scale degrees for each voice from the soprano to the bass, in root position (the
  chapter Strengthening Endings with V7). Hard: the bass is sol to do; V7 is
  complete (sol, ti, re, fa) or incomplete (the fifth omitted, the root doubled);
  fa goes to mi, re to do or mi, ti to do, or (alto or tenor of a complete V7 only)
  ti drops to sol; the tonic chord has only do, mi, and sol, with do and mi; in minor
  the V7 has the leading tone. Soft: avoid re to mi (two roots, two thirds).
  `harmony_seventh_classify` gives the `SeventhResolution`: default (three roots, one
  third, no fifth), incomplete V7 (a complete tonic chord; only the root can be
  doubled, since doubling ti or fa makes parallel octaves), or leading-tone drop (a
  complete tonic chord).
- **Strong predominants** (`harmony_strong_predominants.h`): the chords that
  harmonize fa in the bass before V, so a phrase ends fa to sol (half cadence) or fa
  to sol to do (authentic cadence): IV, ii6 (the more common), and root position ii
  (a major key only, much less common). Only the triads in those inversions count.
- **Strong predominant part-writing rules** (`harmony_predominant_rules.h`), the
  predominant, V, and (for an authentic cadence) I in four voices, as diatonic steps
  for each voice from the soprano to the bass (`harmony_steps.h`; the octave is kept
  so the direction of motion is known). Hard: IV, ii6, or ii; the bass is fa (re for
  ii) to sol to do; each chord has only the notes of its triad, with its root and
  third; ti goes up to do and re down to do over I; in minor, V has the leading
  tone; and no parallel fifths or octaves from the predominant to V or from V to I
  (a fifth between ti and fa is diminished, so it does not count). Soft: not all
  voices move the same way into V; the soprano reaches V from above or by common
  tone; IV doubles its bass; over ii the soprano is re or fa, not la; root position
  ii is for a major key. A half cadence leaves off I (`has_tonic` false).
- **Embellishing tones** (`harmony_embellishing.h`, `harmony_embellishing_rules.h`),
  notes that embellish a chord tone, as a gesture of two or three notes (diatonic
  steps, whether each is a chord tone, and its beat strength). The kind comes from
  how the middle note is approached and left (a step is one diatonic step, a leap
  more): passing tone (step, step, same direction), neighbor tone (step, step,
  opposite), appoggiatura (leap, step, opposite), escape tone (step, leap,
  opposite), suspension (static, step down), retardation (static, step up), pedal
  (static, static), and anticipation (a two-note gesture: the same pitch twice).
  Hard: the gesture is one of those eight; a suspension or retardation is on a
  stronger beat than the notes around it. Soft: the first and last notes are chord
  tones; the embellishing note is not; an appoggiatura is on a stronger beat and an
  escape tone on a weaker one; an anticipation is on a weaker beat than the note
  after it; an appoggiatura or escape tone is left downward.
- **Cadential 6/4 rules** (`harmony_cadential_rules.h`), the cadential 6/4, an
  embellishment of V from two embellishing tones, a sixth and a fourth above the bass
  sol, and its resolution to V or V7 (Strengthening Endings with Cadential 6/4). The
  chord is marked `cadential` on `HarmonyChord` (the tonic triad in second
  inversion): it has dominant function, not tonic, so it counts as a dominant in the
  phrase model. The rules take the chords as diatonic steps in four voices. Hard: the
  bass is sol and stays; the chord is sol, sol, mi, do (the bass doubled); the voice
  on mi falls a step to re and the voice on do falls a step to ti (6-5, 4-3), in the
  same voice; the voice doubling the bass stays on sol (V) or falls a step to fa
  (V7, 8-7); in minor, V has the leading tone; and no parallel fifths or octaves
  into the 6/4 or out of it. Soft: no voice but the bass leaps into the 6/4; no voice
  leaps to the fourth above the bass, the dissonance; the chord that approaches it is
  IV or ii6. The approach chord is optional.
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
- **6/4 chords** (`harmony_six_four.h`, `harmony_passing_rules.h`,
  `harmony_neighbor_rules.h`, `harmony_arpeggiating_rules.h`), 6/4 Chords as Forms of
  Prolongation: the three kinds of 6/4 chord besides the cadential one, `SixFourType`
  (cadential, passing, neighbor, arpeggiating), which the label depends on, not the Roman
  numeral alone. The subject is three chords (before, the 6/4, after), their notes as
  diatonic steps in four voices, and a beat strength for each; the 6/4 is a second
  inversion triad whose root is a fourth above the bass. `harmony_six_four_classify` says
  which kind it is: a chord marked cadential; a static bass with the same root position
  chord on both sides is a neighbor; a bass in three steps one way with the same function on
  both sides is passing; a bass leap to the fifth of the same chord with the upper voices
  sustaining it is arpeggiating. Passing, which prolongs tonic or predominant: hard, the bass
  steps one way, both sides have the same function, and the 6/4 is the bass doubled with the
  fourth and sixth above it; soft, tonic or predominant, the soprano moves by step, and the
  inner voices stay or step. Neighbor (pedal), over do do do or sol sol sol: hard, the bass is
  static, the same root position chord is on both sides, the bass is that chord's root, the 6/4
  notes, and two upper voices go up a step and back while the third stays; soft, I or V.
  Arpeggiating: hard, the same chord throughout, the bass leaps to its fifth, and the upper
  voices sustain; it is not labeled in analysis. The cadential 6/4 gets a soft rule that it is
  on a stronger beat than the V(7) it resolves to.
- **Cadence hearing rules** (`harmony_hearing_rules.h`), whether a potential
  cadence point is a true cadence, from what the caller says is heard. Hard: the
  harmonies are V to I or x to V; a sense of beginning follows; what follows does
  not repeat material from the middle of the phrase. Soft: the phrase is more than
  two measures long unless the tempo is quite slow.
- **Cadence writing rules** (`harmony_writing_rules.h`), the bass and soprano of a
  cadence in I and V only, as degrees. PAC: bass 5 to 1, soprano 2 to 1 or 7 to 1.
  IAC: bass 5 to 1, soprano 2 to 3 or 5 to 5. HC: bass 1 to 5, soprano 1 to 7 or 3
  to 2 (5 to 5 is possible but not common, a soft rule). In minor the V chord needs
  the leading tone. A subverted cadence is not written.
- **La in the bass** (`harmony_deceptive.h`, `harmony_deceptive_rules.h`,
  `harmony_iv6_prolongation.h`, `harmony_iv6_prolongation_rules.h`,
  `harmony_plagal_six_rules.h`, `harmony_connection_rules.h`, `harmony_phrygian.h`,
  `harmony_phrygian_rules.h`), La in the Bass at Beginnings, Middles, and Endings. Each takes
  its chords in four voices as diatonic steps. Deceptive motion (V or V7 to vi, or less
  commonly IV6; a motion, never a cadence): hard, the chords, their notes, ti up to do (and fa
  down to mi in V7), every other upper voice down against the bass in V(7) to vi, and no
  parallels; soft, vi over IV6. Tonic prolongation with IV6 as a predominant (I IV6 V I, or I
  IV6 V6 I, bass do la sol|ti do): hard, the chords, the bass line, their notes, no parallels,
  no doubled leading tone; soft, the soprano do do re do, V6 over V, and a major key. Plagal
  (IV6) prolongation (I (IV6) I6, bass do la mi): hard, the bass leaps down each time, the
  notes, the other voices by step or common tone, no parallels; soft, the soprano mi fa sol.
  La connecting tonic to a strong predominant in a phrase middle (I, vi or IV6, IV or ii6,
  bass do la fa): all hard, with the bass leaping down and the upper voices by step or common
  tone. The phrygian half cadence (iv6 to V, minor only, a half cadence and not a
  `CadenceType`): hard, minor, the notes, do doubled in the iv6, no augmented second from le up
  to ti, no parallels. For the phrase model labels, a vi that connects the areas or stands in
  for a strong predominant is a weak predominant, so only a strong one gets PD.
- **The mediant** (`harmony_mediant.h`, `harmony_mediant_rules.h`), The Mediant Harmonizing
  Mi in the Bass: iii (III in minor), a weak predominant that usually travels through a strong
  predominant on its way to V, used after I in root position to harmonize a descending ti.
  Three chords, I (root position), iii (root position, or less often first inversion), then
  a strong predominant (IV, ii6, ii) or V directly: `harmony_mediant_of` says which of the two
  kinds. Hard: the chords; each chord's own bass (do mi, then fa or re, or sol to V); each
  chord's notes with root and third present; an upper voice on ti over iii that is do over the
  I and goes down to la over the predominant (or stays on ti into V); and no parallels. Soft:
  iii in root position, iii through a predominant rather than directly to V, and, in minor,
  no iii directly to V. iii is not a substitute for I6, which the prolongation rules
  harmonize I V4/3 I6, so no chord before iii but I is accepted.
- **Predominant seventh chords** (`harmony_predominant_seventh.h`,
  `harmony_predominant_seventh_rules.h`, `harmony_ii7_v7_rules.h`), Predominant Seventh
  Chords: ii7 and its inversions (ii6/5, ii4/3, ii4/2), IV7, vi7, and iii7, which have
  predominant function. `harmony_predominant_seventh_of` gives the kind, and
  `harmony_predominant_seventh_rank` how common it is (a value of the kind, not a rule): ii6/5
  most, ii7 somewhat, ii4/3 less, ii4/2 least; IV7 and vi7 less common, iii7 least. The
  treatment rules take three chords (before, the seventh chord, after) in four voices as
  diatonic steps. Hard: the middle chord is a predominant seventh; its notes (root, third, and
  seventh present, the bass on the root, third, fifth, or seventh of its inversion); the chordal
  seventh is approached by step or common tone and resolves down by step; and no parallels.
  Soft: ii7 is preceded by I6 rather than I, IV7 and vi7 are in root position, and vi7
  connects the tonic area to a strong predominant (it is not reached from V7 or followed by
  V). ii7 to V7, both in root position, is its own subject: hard, the notes, one of the two
  incomplete (ii7 with re doubled and no la, or V7 with sol doubled and no re), and no
  parallels.
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
- **Finding modulations.** Finding cadences, noticing repeated accidentals, backing up a chord
  to find the pivot, hearing x/ii, y/ii, z/ii over and over, and deciding where a passage sits on
  the spectrum are the analyst's; the facts are given. Writing advice for a modulating phrase (do
  not cadence too quickly after the pivot, use deceptive motion to avoid a cadence in the new key
  and then retry) is not a rule, since it gives no number. Modulation to keys beyond the closely
  related ones, and pivots through the harmonic minor's V or vii diminished, are not modeled.
  Predominant sevenths in minor have altered notes (ii7 is half diminished), which are not
  modeled. The treatment rules cover each predominant seventh alone; the progressions ii6/5
  for ii6, I ii4/3 I6, and ii4/2 in place of a strong predominant are not separate subjects.
  Hyperbeats are not modeled, only the beat strength of each chord.
- **The analysis procedure.** Listening for phrase endings, labeling the cadence, and
  guessing the harmony from the bass line are the analyst's. The bass line hints (sol
  at the cadence, fa or re under a strong predominant) are the cadence and strong
  predominant rules'. Nothing here finds a phrase's chords or its cadence.
- **Other cadences.** Plagal and deceptive endings are not cadences in this
  chapter's vocabulary, so a progression like IV to I is reported as not
  cadential. A phrase that really ends on (IV)-I exists but is not modeled: plagal
  motion has no placement for a phrase ending, and is never a `CadenceType`. V going to iii is not stylistically normative and is not a recognized progression. Deceptive motion is likewise never a `CadenceType`, and the phrygian half cadence is a half cadence the framework recognizes, not a type of its own. A phrygian half cadence approached from i, with the soprano me fa sol, needs the chords before and is not modeled. Chords before V other than as "x" are not modeled.
- **The strength of an IAC.** An imperfect authentic cadence can be made more or
  less strong by its melody, rhythm, and what follows; that is not modeled.
- **Minor-mode melodic alterations** beyond the leading tone in V.

## Voice leading analysis

`analysis/analysis_resolution.h` checks seventh and leading-tone resolution per voice from a voice-pitch grid and a chord run list, using `harmony_resolution_rules.h`. See `docs/modules/voice-leading-analysis.md`.
