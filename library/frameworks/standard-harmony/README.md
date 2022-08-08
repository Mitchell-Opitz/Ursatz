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
leading-tone drop), and `NonChordTone` (the embellishing tones), with
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
- **Labeling a 6/4.** The caller says which second inversion tonic chords are
  cadential; this module does not decide it. A second inversion chord has other
  uses (the chapter says there are four categories), which later chapters introduce.
- **Other cadences.** Plagal and deceptive endings are not cadences in this
  chapter's vocabulary, so a progression like IV to I is reported as not
  cadential. Chords before V other than as "x" are not modeled.
- **The strength of an IAC.** An imperfect authentic cadence can be made more or
  less strong by its melody, rhythm, and what follows; that is not modeled.
- **Minor-mode melodic alterations** beyond the leading tone in V.
