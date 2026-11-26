# Reference themes

Nine hand-written 8-bar piano themes and their blueprints (P69b, R-740). The themes were written outside the Composer from the taste profile and rated by Michelle; the blueprints write each one as a theme request (`docs/blueprints.md`). The tests `test_reference_blueprints` and `test_reference_skeletons` read them.

Where they came from: the `.mid` and `.musicxml` files are copies of Ursatz-Music-Storage `references/` (that repository is never built, R-406); the 13 blueprints were written for P69b in Claude Master (`phases/P69b-measure/blueprints/`). The ratings are Claude Master's `references.md` (1 to 5, the taste-profile scale).

| Theme | Style, key, meter | Rating | Blueprints |
|---|---|---|---|
| `hand-A` | none, C minor, 4/4 | great (no number) | full, skeleton |
| `hand-B` | none, C major, 4/4 | great (no number) | full, skeleton |
| `ref-1-megaman` | Mega Man, E minor, 4/4 | 4.5 | full, skeleton |
| `ref-2-ff7` | Final Fantasy VII, D minor, 4/4 | 5 | full, skeleton |
| `ref-3-banjo` | Banjo-Kazooie, F major, 6/8 | 4 | full, skeleton |
| `ref-4-hollowknight` | Hollow Knight, E minor, 3/4 | 5 | full, skeleton |
| `ref-5-shostakovich` | Shostakovich, C minor, 4/4 | 4.5 | full |
| `ref-6-holst` | Holst, G pedal, 5/4 | 4 | none (planed chords, P70) |
| `ref-7-debussy` | Debussy, D-flat major, 4/4 | 4.5 | none (outside gate (a), R-742) |

A `.full.json` gives every bar's chords, melody, bass and inner voices: it renders the reference note for note. A `.skeleton.json` gives the chords, some rhythms, jobs, anchors, events, the climax and the accompaniment, and leaves the rest to the Composer.

## Changes from the copies

The blueprints differ from the Claude Master copies only here; the themes are unchanged.

- `ref-2-ff7` and `ref-4-hollowknight`, full and skeleton: `Iadd9` is `iadd9` and `IVadd9` is `ivadd9` (and `IVadd6` is `ivadd6` in `ref-4`). A color goes on the key's own triad, and in a minor key the tonic and the subdominant are minor (the references sound D minor and G minor, E minor and A minor there).
- `ref-3-banjo` skeleton, bar 7: the rhythm `x:q r:e x:e r:q.` filled 7 eighths of a 6/8 bar; it is `x:q r:e x:e r:q`, the reference's last bar.
- `ref-3-banjo` skeleton, bar 0: the neighbor at eighth 3 is anchored at the reference's G#5 and the note before it at A5. Unanchored, the Composer places a neighbor a step from the note before it but does not return to that note, so it was no neighbor.
- `ref-2-ff7` skeleton: bar 3's C#5 at beat 3/2 is an `anticipation` (it sounds over the Vsus4 half a beat before the V it belongs to; as a plain anchor the checker refused it as a non-chord tone of no allowed figure); bars 4 and 5 give the reference's rhythms and bar 5 its A5 on beat 2 (restating bars 0 and 1, bar 5 ended low and the climax Bb5 in bar 6 was a leap no rule allows; the reference climbs F5 G5 A5 to it); `preferences.melody.waive` holds `forbidden-outline` (the reference's own line from G5 in bar 2 down to C#5 in bar 3 outlines a diminished fifth). Before these, no seed composed.
- `ref-1-megaman` skeleton: the accompaniment is `arpeggiated`, not `michelle.pattern.octave-pulse`, which grows on none of seeds 1 to 16 in E minor (the full blueprint writes the octave bass itself).
