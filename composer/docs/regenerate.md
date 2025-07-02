# Locking and regenerating

`<ursatz_composer/regenerate.h>`. Milestone M2: a piece is planned and filled, then taken apart node by node. Material
the user likes is locked; the rest is written again with a seed, and only what depends on it with it.

## Locking

`plan_lock(plan, target)` makes a target Given. A node: it, every node under it and every lane span they own. A lane
span: that span alone. Given material is never changed by the Composer, and nothing leaves Given, so a lock is for good
in the plan. Unlocking is reading an earlier plan file.

Only what is written can be locked. A node or lane span still open (Constrained) is refused with `PLAN_ERR_BAD_PAYLOAD`
and nothing is changed: locking it would leave a hole nothing may fill.

## Removing a subtree

`plan_remove_subtree(plan, node)` removes the descendants of a node (not the node), the lane spans they own, and every
constraint anywhere in the plan that names one of them (a motive a span is made from, a section a section returns). It is
refused with `PLAN_ERR_GIVEN_LOCKED`, changing nothing, when any of that is Given.

Ids are never reused. A removed id stays unused in memory (`plan_has_node` and `plan_has_lane_span` are false for it, and
`plan_node_count` is still the last id given out). A plan file numbers what is left from 1 again when it is written, so
the ids of a file written after a section was built again are not the ids of the file before.

## Dependents

`plan_dependents(plan, target, &list, &count)` lists what is written from a target, without the Given ones, nodes before
lane spans, each by id:

| Target | Dependents |
| --- | --- |
| structure node | its whole subtree and the lane spans of the node and of the subtree |
| section | as a structure node, and every section whose RETURN names it, with its subtree and lane spans, and the sections that return those |
| harmony span | the bass, inner voice and melody spans over its time, and the next harmony span of the same section (the chord that joins it) |
| bass span | the inner voice and melody spans over its time |
| melody span | the inner voice spans with a VOICING or COUNTERPOINT constraint over its time, and the next melody span of the same section |
| material motive node | every melody span whose MOTIVE_REF names it |

"The next span" stops at the end of the section: each theme section is written from its own motive and key.

## Regenerating

`plan_regenerate(plan, target, config, outcome)` reopens the target and what depends on it, then runs `compose_run` with
the config's seed. `plan_regenerate_all` takes several targets (every harmony span of a section) and composes once;
`plan_regenerate_scope` lists what would be reopened without changing the plan.

| Target | What is reopened |
| --- | --- |
| section | Whole. Its phrases, ideas and lane spans are removed, the motives it added to the material go with them, and it is Constrained again. grow-theme builds it from the seed. A section that returns it is reopened the same way |
| the form | Every theme section that is not Given |
| phrase, subphrase, idea | Its nodes stay, since nothing builds a phrase alone. The harmony, bass, melody and texture spans of the node and what is under it are reopened, as for lane spans |
| lane span | It and its dependents, again for each one reopened, go back to Constrained with their constraints and a placeholder note or chord |

The chords and bass notes an operator wrote by splitting one open span are one open span again, with the constraints grow
gave it (the shape of the first, the cadence of the last), so the operators see what they saw the first time. An inner
voice span is reopened where it carries the accompaniment, voicing or counterpoint asked for; its other voices are
written with it.

Two rules keep a piece consistent. A target inside a section that returns another reopens that whole section, because a
return is made from its source. And whatever is reopened in a section reopens the sections that return it, unless they
are Given.

The same plan, target and seed give the same plan and the same records.

## Variations

`plan_variations` (`<ursatz_composer/variations.h>`) regenerates the same targets for several seeds and keeps the results
that sound different: for the seeds `config->seed`, `config->seed + 1`, ..., at most `4 * count` of them, the plan is
copied (`plan_to_json` then `plan_from_json`, so the input is never changed), `plan_regenerate_all` runs on the copy, and
the copy is kept when its signature differs from the plan's and from every copy kept before. A seed that gets stuck is
skipped. The signature is the FNV-1a 64 hash over the events of `plan_render`: for each its part, onset and duration as
reduced fractions, and MIDI pitch. `count` is 1 to 16. The plan must have the ids a plan file has (no removed nodes or
spans), since the targets are found again in the copy by id, and gives `REGENERATE_ERR_BAD_TARGET` otherwise. The errors
`plan_regenerate_all` gives before any seed (locked, bad target, not found) are returned as it gives them. The result is
`REGENERATE_OK` with `count` between 0 and the number asked for: what none kept means is for the caller (the commands
exit 2).

`plan_harmonizations` is the same for the harmony of one section over a melody that is given. Every melody lane span of
the section must be Given (and there must be one), else `REGENERATE_ERR_BAD_TARGET`; the targets are the section's
harmony spans that are not Given. A melody that is only partly Given is refused because regenerating the harmony then
writes the melody around the given part again, and the melody stage often cannot join it. Its signature is the hash of
the harmony lane alone (each span's place and numeral, in time order), so two harmonizations are different only when
their chords are. See [cli-variations.md](cli-variations.md) for the commands.

Variations are reseedings of `regenerate`: nothing yet ornaments a melody or reharmonizes with substitutions.

The sweep `test_variations sweep` (label `slow`) asks for 4 variations of section B of the 24-bar rounded binary filled
with seed 3, for each config seed 1 to 20. All 80 were kept; the test fails only below that baseline (R-075).

## Results

| Result | Meaning |
| --- | --- |
| `REGENERATE_OK` | The plan is complete again. |
| `REGENERATE_ERR_LOCKED` | The target is Given, or a section to reopen has Given material among its phrases and ideas. Nothing is changed. |
| `REGENERATE_ERR_BAD_TARGET` | A motive, the material and what is in it, a meter or key span that is not Given, or a harmony span the Composer did not open (no shape to build it from). Nothing is changed. |
| `REGENERATE_ERR_NOT_FOUND` | The target is not in the plan. |
| `REGENERATE_ERR_STUCK` | Given or written material around the target leaves it unfillable. `outcome->stuck_stage` names the stage. The plan is partly filled. |

## Measured

Section A of a 24-bar rounded binary has melody written after it (B), so its last note has to join B's first. Regenerating
it is stuck for some seeds, and the next seed completes it: in `test_regenerate returns`, 1 of the 10 runs needed a later
seed (seeds 1 to 5, A with A' unlocked and with A' locked). The commands try the next seeds themselves, up to 16.
Regenerating B (the other end of the piece, nothing written after it but A') completed on the first seed in all 5.

## Changes to the compose run

The melody retry of `compose_run` reopens only the melody spans that were open when its stage began, so a regenerated
plan keeps the melody written before it (a fresh plan is as before: all of them were open). A theme section that is grown
but has lane spans open again counts as open. The material gives a section the first gap of bars its motives need before
it uses the bars after the last motive, so a regenerated section takes the place its motives left.
