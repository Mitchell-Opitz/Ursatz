# Playback views

Playback of pieces and motifs, with the motif score and playback service. [Docs index](../README.md).

## Playback

The score workspace plays the piece from the toolbar. Space plays or pauses,
Home stops and goes back to the start. The first Play creates the audio
context (browsers block audio before a click) and loads the piano; the button
reads "Loading piano..." until the samples are decoded.

- Transport: Play/Pause, Stop and a readout "m<measure> <m:ss> / <m:ss>". The
  times follow the current tempo.
- Tempo: a percentage with minus and plus buttons in 5% steps from 50% to
  150%; clicking the percentage resets it to 100%. It applies while playing
  and resets for each piece. The tempo itself comes from the playback route's
  `tempo` regions (a MIDI file's set-tempo events, a score's tempo and
  metronome marks). With `tempo_source` "default" the file gave none, the
  piece plays at 120 and the Sound menu says so.
- Volume: a slider, 0 to 100, default 80. Saved in `ursatz.workspace.v1`.
- Sound menu: one row per part with a select of the registered sounds, a
  Reverb slider (0 to 100, default 20, saved with the volume), and the credit
  lines of the sounds in use. Reverb is a generated convolution tail
  (`reverb.js`, 2.2 s), no impulse file is shipped; its dry and wet gains are
  an equal-power crossfade that keeps the dry signal audible, and both move
  with a short time constant so the slider makes no clicks.
- Cursor: a 2 px line over the playing system's staves with a soft teal band
  behind it, drawn in the score overlay. It glides through each measure
  (redrawn every animation frame while playing); with reduced motion it steps
  from note onset to note onset. It stays at the pause or stop position and is
  hidden only before the first play or click.
- Follow: while playing, when the cursor's system leaves the band between 10%
  and 75% of the score's height, the score scrolls so that system's top sits
  at 20% (smooth unless reduced motion). The wheel, touch, a drag on the
  scrollbar or a scroll key (arrows, Page Up and Down, End) turns Follow off
  and shows a "Follow" pill at the bottom right. The pill or `C` turns it on
  and scrolls to the cursor; each new Play turns it on again. The rule is the
  pure `WorkspaceState.followScrollTarget(viewport, systemRect)`.
- Clicking to seek: a click on bare paper (not on a ribbon, mark or colored
  note) finds the system under the click, converts the x position to a
  position in the piece (`tAt` minus the timeline offset) and snaps it to the
  nearest note onset in that system (`PlaybackEngine.nearestOnset`). Playback
  continues from there if it was playing; stopped, the next Play starts there.
  A click on a colored note seeks to that note's onset and selects its claim.
- Playing a claim: the Play button in the Inspector's actions row, `Enter`
  with a claim selected, or a double-click on a ribbon or mark plays the
  claim's span (a cadence plays from the dominant to the end of the arrival)
  and, at the end, the cursor returns to the claim's start. A span with no
  length plays a quarter note.
- Loop (pieces): the loop version of a piece in the Pieces view plays its
  intro once, then repeats the loop region from the result (`result.loop`)
  without a gap. The transport schedules each pass ahead of the clock
  (`createTransport` option `loop: {start, end}`, whole notes), so notes after
  the wrap are queued before it is reached; a note that sounds past the loop
  end is cut there. The readout and the cursor show the position inside the
  piece. The Loop button turns the repeat on or off while playing (on by
  default); a claim or range play ignores it.
- Play ending (pieces): the pass goes on to the end of the last body section,
  then the notes of the ending version (`ending.mid`) play from there to its
  end and playback stops. Pressed before the body's end of the current pass it
  switches there; pressed in the turnaround it waits for the next pass's body
  end (`transport.playEnding(endingTimeline, bodyEnd)`). Stopped, it starts
  from the beginning. While the ending plays the workspace shows the ending
  version's score (without layer highlights) and goes back to the loop score
  when playback stops.
- Output: every note is a voice into one bus (at most 64 voices, the oldest
  stops first), then the dry path and the reverb send, a master gain and a
  compressor. Velocity is fixed at 0.7 (the playback data has none).

Adding a sound is one `register` and one `mapProgram`, with no change to the
engine or the transport. A source is `{id, label, credit, load(ctx, {pitches}),
start(ctx, destination, {pitch, velocity, when, duration}) -> {stop(when)}}`:

```js
const registry = SoundSources.createDefaultRegistry(Instruments);
registry.register(mySource);
registry.mapProgram(73, "my-source"); // MIDI program 73 plays it
registry.setPartSource(2, "my-source"); // or override one part
```

The registry is `frontend/public/instruments.js` (global `Instruments`), the
built-in sounds are `sound-sources.js` (`gm-piano`, the default, and
`basic-synth`, the fallback that needs no files), the timing is
`playback-engine.js` (a timeline with tempo regions and a transport that
schedules notes 120 ms ahead of an injected clock) and the reverb helpers are
`reverb.js`. If the piano cannot load, playback uses the basic synth and the
status line says "Piano samples could not load; using basic synth".

The piano is the FluidR3 GM acoustic grand piano, vendored in
`frontend/public/vendor/soundfonts/fluidr3-gm/` with its source, checksum and
license in `vendor/soundfonts/README.md`. FluidR3_GM by Frank Wen, rendered by
Benjamin Gleitzman (midi-js-soundfonts), licensed Creative Commons
Attribution 3.0 (https://creativecommons.org/licenses/by/3.0/us/).

### Motif score and playback

A motif's stored `motif.degrees` is shown in the score workspace and sounded
in the motif's stored key (C major or C minor). Nothing is saved.

- `POST /api/materials/render` takes `{form, degrees, meter, key?, pickup?,
  render_key?, tempo?, role?, harmony?, features?}` (`degrees` 1 to 4000 characters; `tempo` a number or text like `195/2`;
  the form and the tempo range are the Composer's to refuse) and answers
  `{ok, musicxml, playback}`. The route calls `composer_api_motive_render` with
  `input.format` `degrees` (`rhythm` or `contour` for those forms), `input.form`, `meter`, `key`
  (when given), `pickup`, `harmony`, `features`, `role`, `render_key`, `tempo` and `musicxml: true`; the
  MusicXML is the response's `musicxml`, not a file.
  `render_key` defaults to `key`; without either the answer is 400 "give a
  key". A list the Composer refuses is 400 (malformed) or 422 (nothing satisfies it) with its message.
- `playback` is the Analyzer's playback of the render's MIDI file
  (`analyzer_playback_to_json`: `tempo_source`, `tempo`, `parts`, `end`,
  `notes`). Positions are whole notes from the pickup start, so the first note
  is at `[0, 1]`. The backend writes the MIDI bytes to
  `materials/.render-tmp/<random>.mid` (the Analyzer imports by path), reads
  it and deletes it; nothing is cached for this route.
- `GET /api/materials/items/<id>/versions/<n>/score?key=` answers MusicXML
  and `.../playback?key=` answers the playback JSON for that stored version;
  `key` defaults to the motif's own key, and a version without degrees is 404.
  Both come from one render kept under the version in
  `items/<id>/v<n>/render/`: `score.musicxml`, `motif.mid` and `render.json`
  `{api_version, composer_version, ursatz_commit, render_key, tempo}` (the three
  versions from `composer_api_versions`). A request with the same key, tempo
  and Composer build and Ursatz commit reads these files, an older cache missing once, so the Composer runs at most once
  per version and key.
- `GET /api/materials/items/<id>/versions/<n>/playback?file=theme|<name>` (`loop`, `ending`, `piece`: a version name of the piece's form)
  answers the playback of a theme or piece version's `theme.mid` or `<name>.mid`
  (400 for a `file` outside the version name pattern, 404 when the version has no
  such file).
- `GET /api/materials/items/<pce-id>/versions/<n>/variations/<k>/<file>/playback`
  answers the playback of a variation's MIDI file (`variation-<i>.mid`; 404 for
  any other file).
- What the served playback differs in from the browser's former reader:
  `tempo_source` is `file` or `default` (the browser said `given`); tempo
  values are rounded to thousandths; positions are reduced fractions; part
  names are empty for a MIDI file (the score workspace shows `Part <n>`).
- The motif detail mounts `ScoreWorkspace` with these URLs. The routes still
  accept `render_key` and `?key=` (pieces may use them), but the motif detail
  sends neither. `MaterialsModel.renderRequest(body)` builds the POST for an
  unsaved candidate.
