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

The entities these rules use live in `src/semantics/`: `HarmonicFunction`
(tonic, predominant, dominant), `CadenceType` (perfect authentic, imperfect
authentic, half, subverted), `SeventhResolution` (default, incomplete,
leading-tone drop), `PhraseModelLabel` (Tb, PD, D, Te), `SixFourType`, and `NonChordTone` (the embellishing tones), with
`BeatStrength` for the beat. They name things and carry no rule; which chords have
which function, and which progressions make which cadence, is this framework's.

## Scope

Every rule group has Rule/Constraint pairs referencing this Framework plus pure
check functions. A rule that states what the music requires is hard; "usually",
"almost always", and "not common" features are soft. `constraint_evaluate` is
never called here.

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
  `harmony_cadence_strength` orders them: half, imperfect authentic, perfect
  authentic. A V7 to I in root position with do in the soprano is still a PAC.
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
  Hyperbeats are not modeled, only the beat strength of each chord.
- **The analysis procedure.** Listening for phrase endings, labeling the cadence, and
  guessing the harmony from the bass line are the analyst's. The bass line hints (sol
  at the cadence, fa or re under a strong predominant) are the cadence and strong
  predominant rules'. Nothing here finds a phrase's chords or its cadence.
- **Other cadences.** Plagal and deceptive endings are not cadences in this
  chapter's vocabulary, so a progression like IV to I is reported as not
  cadential. A phrase that really ends on (IV)-I exists but is not modeled: plagal
  motion has no placement for a phrase ending, and is never a `CadenceType`. Deceptive motion is likewise never a `CadenceType`, and the phrygian half cadence is a half cadence the framework recognizes, not a type of its own. A phrygian half cadence approached from i, with the soprano me fa sol, needs the chords before and is not modeled. Chords before V other than as "x" are not modeled.
- **The strength of an IAC.** An imperfect authentic cadence can be made more or
  less strong by its melody, rhythm, and what follows; that is not modeled.
- **Minor-mode melodic alterations** beyond the leading tone in V.
