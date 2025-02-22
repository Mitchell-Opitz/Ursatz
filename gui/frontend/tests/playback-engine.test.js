/* Confirms the playback engine (playback-engine.js): seconds and positions
 * across two tempo regions, a transport on a fake clock that schedules each
 * note once and in order, pause and resume, seek while playing, a rate change
 * mid-piece, range end and onEnd, and a start mid-piece. Plain Node script,
 * no dependencies. Run directly: `node frontend/tests/playback-engine.test.js`. */

const PlaybackEngine = require("../public/playback-engine.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

const near = (a, b) => Math.abs(a - b) < 1e-9;

/* Two tempo regions: 120 qpm for the first whole note, 60 qpm after. */
const playback = {
  tempo_source: "file",
  tempo: [
    { at: [0, 1], qpm: 120 },
    { at: [1, 1], qpm: 60 },
  ],
  parts: [{ index: 0, name: "", program: 0 }],
  end: [2, 1],
  notes: [
    { i: 1, on: [1, 2], len: [1, 2], pitch: 62, part: 0 },
    { i: 0, on: [0, 1], len: [1, 2], pitch: 60, part: 0 },
    { i: 2, on: [1, 1], len: [1, 2], pitch: 64, part: 0 },
    { i: 3, on: [3, 2], len: [1, 2], pitch: 65, part: 0 },
  ],
};
const timeline = PlaybackEngine.buildTimeline(playback);
const { secondsAt, positionAt } = PlaybackEngine;

check(timeline.notes.map((note) => note.i).join() === "0,1,2,3", "notes are sorted by onset");
check(typeof timeline.notes[1].on === "number" && timeline.notes[1].on === 0.5, "onsets are numbers in whole notes");
check(near(secondsAt(timeline, 1), 2) && near(secondsAt(timeline, 2), 6), "seconds accumulate across tempo regions");
check(near(secondsAt(timeline, 0.5), 1) && near(secondsAt(timeline, 1.5), 4), "seconds inside each region");
let roundTrip = true;
for (const w of [0, 0.25, 0.999, 1, 1.3, 1.75, 2]) {
  if (!near(positionAt(timeline, secondsAt(timeline, w)), w)) roundTrip = false;
}
check(roundTrip, "positionAt inverts secondsAt");
check(PlaybackEngine.formatClock(75.9) === "1:15" && PlaybackEngine.formatClock(0) === "0:00", "clock labels are m:ss");
check(buildDefault(), "a piece with no tempo plays at 120");
function buildDefault() {
  const tl = PlaybackEngine.buildTimeline({ notes: [], end: [1, 1] });
  return near(secondsAt(tl, 1), 2);
}

/* A transport on a fake clock. */
function makeTransport(extra) {
  const log = { notes: [], states: [], ends: 0 };
  const clockState = { now: 10 };
  let tick = null;
  const transport = PlaybackEngine.createTransport({
    timeline,
    clock: () => clockState.now,
    setTimer: (fn) => {
      tick = fn;
      return 1;
    },
    clearTimer: () => {
      tick = null;
    },
    onNote: (note) => log.notes.push(note),
    onState: (state) => log.states.push(state),
    onEnd: () => {
      log.ends++;
    },
    ...extra,
  });
  const advance = (seconds) => {
    const steps = Math.round(seconds / 0.025);
    for (let n = 0; n < steps; n++) {
      clockState.now += 0.025;
      if (tick) tick();
    }
  };
  return { transport, log, clockState, advance };
}

{
  const { transport, log, advance } = makeTransport();
  transport.play(0);
  advance(8);
  check(log.notes.map((note) => note.i).join() === "0,1,2,3", "each note is scheduled once and in order");
  check(near(log.notes[1].when - log.notes[0].when, 1) && near(log.notes[2].when - log.notes[1].when, 1), "note times follow the tempo regions");
  check(near(log.notes[0].duration, 1) && near(log.notes[2].duration, 2), "durations follow the tempo regions");
  check(log.ends === 1 && transport.state() === "stopped", "onEnd fires once at the timeline end and the transport stops");
  check(transport.position() === 0, "after the end the position is back at the start");
  check(log.states.join() === "playing,stopped", "states are reported on change");
}

{
  const { transport, log, advance } = makeTransport();
  transport.play(0);
  advance(1.2);
  transport.pause();
  const heldAt = transport.position();
  check(transport.state() === "paused" && heldAt > 0.5 && heldAt < 0.7, "pause holds the position");
  const scheduled = log.notes.length;
  advance(3);
  check(log.notes.length === scheduled && near(transport.position(), heldAt), "nothing plays and the position stays while paused");
  transport.play();
  advance(4);
  check(log.notes.map((note) => note.i).join() === "0,1,2,3", "resuming plays the rest, each note once");
  transport.stop();
}

{
  const { transport, log, advance } = makeTransport();
  transport.play(0);
  advance(0.5);
  transport.seek(1.5);
  check(transport.state() === "playing" && near(transport.position(), 1.5), "seek keeps playing and moves the position");
  advance(2);
  check(log.notes.some((note) => note.i === 3), "notes after the seek point play");
  check(log.notes.filter((note) => note.i === 2).length === 0, "notes skipped by the seek do not play");
}

{
  const { transport, advance } = makeTransport();
  transport.play(0);
  advance(0.5);
  transport.setRate(2);
  check(transport.rate() === 1.5, "the rate is clamped to 1.5");
  transport.setRate(0.1);
  check(transport.rate() === 0.5, "the rate is clamped to 0.5");
  transport.setRate(1.5);
  const before = transport.position();
  advance(1);
  check(near(secondsAt(timeline, transport.position()) - secondsAt(timeline, before), 1.5), "a faster rate moves the position faster from the change on");
  transport.stop();
}

{
  const { transport, log, advance } = makeTransport();
  transport.play(0.5, 1.5);
  advance(5);
  check(log.notes.map((note) => note.i).join() === "1,2", "a ranged play schedules only the notes inside the range");
  check(log.ends === 1 && transport.state() === "stopped", "a ranged play ends at its end");
  check(near(transport.position(), 0.5), "stop after a ranged play returns to its start");
}

{
  const { transport, log, advance } = makeTransport();
  transport.play(1);
  advance(6);
  check(log.notes.map((note) => note.i).join() === "2,3", "starting mid-piece skips earlier onsets");
}

{
  const { transport, advance } = makeTransport();
  transport.play(0.75, 2);
  advance(0.2);
  transport.stop();
  check(near(transport.position(), 0.75), "stop goes back to where the last play began");
  const fresh = makeTransport().transport;
  check(fresh.position() === 0, "a new transport sits at 0");
}

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
