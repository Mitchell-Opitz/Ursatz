# Form performance

How a form's piece is led, repeated and performed: a section that leads into the next with a connective (R-486, P48 B2),
written-out repeats (R-485, P48 B3) and the parts of a compound form (R-485, P48 B4). Back to the
[docs index](README.md).

## Composed once, performed in order

A form is composed once. What is played can be longer: a form with `repeats` plays each repeated group twice in place
(A A B A1 B A1), and a compound form with `parts` (B4) plays its parts in order. The composed plan, with its compose outcome,
gives the hearing: the Analyzer's check, the cadences, the score, the explanation. The performed plan is what is rendered:
the plan JSON, the MIDI file and the MusicXML. A form with neither member composes and answers exactly as before.

`piece_perform` (`piece_perform.h`) builds the performed plan from the composed plan of each part and its size:

- a form node named the first part's `plan_name` over the whole performance, with the meter Given over it (the parts'
  meter, which must be one: `PIECE_ERR_BAD_REQUEST` otherwise);
- one section node for each performed section, at its performed start, named as the form names it (`A`, then `A (repeat)`
  its second time), holding the source's letter, cadences and key, and the key lane over the source section cut to it and
  moved;
- the section's lanes Given from the source (`plan_give_section`): harmony, bass, melody and inner voices, with its phrase
  and idea nodes. Every node is Realized and the form node Given. The material, the motive nodes and the seam are not
  performed.

A performance has at most `PIECE_PERFORMED_MAX` (32) sections. `piece_perform_compound` is `piece_perform` for a compound form: it makes a part for each of the form's parts and names the form node for the compound form (`plan_name`). `PiecePerformPart` is a part's form, size, composed plan, whether its repeats
are written out, and its name (the first part is not prefixed; a later part's sections are `Trio A`).

## Repeats

`repeats` in the form document ([forms.md](forms.md)) lists the groups. `repeat-form.json` (A, B and A' of 4 bars with
`[["A"], ["B", "A'"]]`) performs as A, A (repeat), B, A', B (repeat), A' (repeat), at bars 0, 4, 8, 12, 16 and 20 of a
24-bar plan. Each section's melody, harmony, bass and inner voice spans are the source's moved, and repeats do not change
composing: the composed plan is the plan of the same form without `repeats`. Repeat signs in the MusicXML are Later; the
repeats are written out.

## Parts

A compound form (`parts` in the form document, [forms.md](forms.md)) is made of parts, each a piece of a built-in form with
sizes. A part is composed on its own, as the piece of that form at the size of the compound form's bars, and the parts are
performed in order. The trio of a minuet is a part in its own key with its own motive. A form with parts has no sections of
its own: its sizes have `bars` and no `sections`.

`piece_compose_parts` (`piece_perform.h`) composes the parts into a `PieceParts` (a plan, a compose outcome, a home key and
the built-in form of each part; an `again` part has none of its own: it is its source's):

- **Keys.** They are drawn first, from `fill_rng_seed(request->seed)`, one `piece_region_key` draw for each part whose key is a
  region, in part order; a part with the home key is in the request's key. So a part's key does not depend on how another
  part composed. `piece_region_key` takes the region's degrees for the home key's mode, draws one, and answers its key:
  degree 1 is the home key's parallel key (C major: C minor, A minor: A major), any other degree is the closely related key
  of standard-harmony on it (6 of C major: A minor, 3 of A minor: C major). `piece_plan_keys` draws a form's keys with the
  same function and the same draws, so no existing plan moves; it still refuses a region with degree 1 for a section's keys.
- **Seeds.** The part at index `i` is composed from the request's seed plus `PIECE_PART_SEED_STRIDE` (40) times `i`, so no
  part repeats another's draws. A part composed with a motive of its own (`motive` `"new"`) gets no motive from the request
  and the compose run compares with none: the motive is invented.
- **Failing.** A part that fails fails the whole call with its result; `PIECE_ERR_BAD_SIZE` is a size the compound form
  does not have. The piece entry tries the next seed (`PIECE_SEED_TRIES`), so a retry composes the whole compound piece again.
- **Again.** An `again` part performs the composed plan of its source, with its own `repeats` (none by default: the da
  capo is played without repeats). Its sections are named `<part name> <section name>` (`Minuet da capo A`), the notes are
  the source's moved, and its key is the source's.

The sections of the performance are named as the form names them for the first part, and `<part name> <section name>` for a
later part (`Second A`, `First again B`). Each section's lanes are the source's moved, and the key lane has a span over each
performed section, in its part's key.

`parts-form.json` (First and Second, binary pieces of 16 bars, Second in the region `other` with a motive of its own, then
First again) composes in C major 4/4: the keys are C major, A minor and C major, and the performance is 48 bars of A, B,
Second A, Second B, First again A and First again B, 10 of 10 seeds. The piece entry answers with the performed plan as for
repeats; the version's own plan and compose outcome are empty (`composed` is true), its rows are one for each performed
section, each from its part's composed plan and outcome (a part's `key_heard` is heard over that part's plan), and the
explanation is the first part's. Regenerate and variations of a compound form are Later, as for repeats.

## What the piece version answers

For a performed form the piece entry answers with the performed plan: the `plans` member, the plan JSON, the MIDI file and
the MusicXML. The hearing members still come from the composed plan and its outcome. A row of the version's sections is
one for each performed section: its `start` and `length` from the performed plan, every other member from the source
section in the composed plan, so a repeat's row is its source's at its own start (`repeat_of` and `part` come in B6).

## Regenerate and variations are Later

A performed form is composed whole. The piece entry with `from` (regenerate) and the piece variations entry answer
`COMPOSER_ERR_REQUEST` at `/request/form`: `regenerate and variations of the <label> form are Later: it is composed whole`.

## Leading into a section

A section of a sized form may lead into the section after it. The member is `leads_in` in the form document
([forms.md](forms.md)); it is refused on the last section of a size, on a section that `returns` another, on a section
another section returns, and on a section whose next section also leads in.

`form_build_plan` gives such a section the `hold` constraint, as the loop's end has it ([loop-seam.md](loop-seam.md)). The
stages from structure to texture leave it alone, so the sections after it are written first (a returning section is
written by the returns stage). The seam stage then fills it: structure, motives, harmony, bass, melody and texture run for
the held section against the sections after it, whose Given chords and first Given note the operators already read after a
target. The stage works the same with no seam section in the plan (nothing is copied; `seam_planned` stays false); a plan
with no held section and no seam is untouched. The held section keeps its `hold` constraint. Its records and its
explanation stay under the stage name `seam`.

The upbeat chord of a section that leads in is the one its last bar ends on. In the minuet's B that is the dominant, the
half cadence B already ends in, followed by the tonic of A return.

## The connective

`connective` (`"sometimes"` or `"always"`, only with `leads_in`) asks for a short stepwise link in the last bar of the held
section. The plan carries it as a `connective` constraint with `written` (false until written):

- `always` gives the constraint to the section on every seed;
- `sometimes` gives it on some seeds: one draw for each such section, in section order, from the request's seed plus 7, so
  the keys' stream and every existing plan are untouched. The same seed always gives the same plan.

`piece_connective_write` writes it in the seam stage after the melody and before the texture. No random number is used.

1. The melody note that starts on the downbeat of the section's last bar is kept and shortened to one beat (a dotted
   quarter in 6/8). The rest of the bar takes eighth notes: 4 in 3/4, 2 in 2/4, 6 in 4/4, 3 in 6/8.
2. The line is a run of scale steps in the scale of the key the section ends in (minor: harmonic minor, the raised
   seventh). It ends one scale step from the first melody note of the next section, on the side the downbeat note is on:
   from above when the downbeat note is higher, else from below.
3. It is written only when the move from the downbeat note to the first note of the line is at most 7 semitones and not a
   tritone, the step into the next section's first note is 1 or 2 semitones, and every note of the line is within the
   range of the section's other melody notes plus or minus 2 semitones (`standard-counterpoint` melodic rules, as
   `seam_hear.c` reads them). Then the notes after the downbeat in that bar are replaced and `written` is set. Otherwise
   nothing changes and `written` stays false.

The line is the scale, so in minor it may use the augmented second between the sixth and the raised seventh degree.

## Measured

`test_form_spec leads` composes `tests/fixtures/forms/lead-form.json` (A, B held with an `always` connective over a
dominant pedal, A return) in 3/4, seeds 1 to 10:

| Key | Composed | Connective written |
| --- | --- | --- |
| C major | 10 of 10 | 7 of 10 |
| A minor | 10 of 10 | 6 of 10 |

In every composed run B ends on V or V7 and A return opens on the tonic. Where the connective was written, B's downbeat note
is the one the same seed gives without the connective, every line note is in the scale, and the last is 1 or 2 semitones
from A return's first note. A `sometimes` copy of the fixture asks for the connective on 4 of the 10 seeds in C major.
