/* Confirms the playback engine (playback-engine.js): seconds and positions
 * across two tempo regions, a transport on a fake clock that schedules each
 * note once and in order, pause and resume, seek while playing, a rate change
 * mid-piece, range end and onEnd, a start mid-piece, a loop region that
 * repeats at exact positions, and the switch to an ending. Plain Node script,
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
/* Nearest onset in a range (the onsets are 0, 0.5, 1 and 1.5). */
const { nearestOnset, onsetAtOrBefore } = PlaybackEngine;
check(nearestOnset(timeline, 0.6, 0, 2) === 0.5 && nearestOnset(timeline, 0.8, 0, 2) === 1, "nearestOnset snaps to the closest onset");
check(nearestOnset(timeline, 0.25, 0, 2) === 0, "a tie goes to the earlier onset");
check(nearestOnset(timeline, 0.1, 1, 2) === 1, "before the first onset in the range gives that onset");
check(nearestOnset(timeline, 5, 0, 2) === 1.5, "after the last onset gives the last onset");
check(nearestOnset(timeline, 1.2, 0, 1) === 0.5, "the end of the range is exclusive");
check(nearestOnset(timeline, 0.4, 0.5, 2) === 0.5, "the start of the range is inclusive");
check(nearestOnset(timeline, 0.2, 0.1, 0.4) === null, "a range with no onset gives null");
check(nearestOnset(timeline, 0.2, 1, 1) === null, "an empty range gives null");

/* The onset a cursor steps to. */
check(onsetAtOrBefore(timeline, 0.7) === 0.5 && onsetAtOrBefore(timeline, 0.5) === 0.5, "onsetAtOrBefore gives the latest onset up to the position");
check(onsetAtOrBefore(timeline, 2) === 1.5, "onsetAtOrBefore past the end gives the last onset");
check(
  onsetAtOrBefore(PlaybackEngine.buildTimeline({ notes: [{ i: 0, on: [1, 4], len: [1, 4], pitch: 60, part: 0 }] }), 0.1) === 0.1,
  "onsetAtOrBefore before any onset gives the position itself",
);
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

/* Loop playback: a 1-bar intro, then a 2-bar loop (120 qpm: a whole note is 2 s). */
const note = (i, on, len) => ({ i, on: [on * 4, 4], len: [len * 4, 4], pitch: 60 + i, part: 0 });
const loopTimeline = PlaybackEngine.buildTimeline({
  tempo: [{ at: [0, 1], qpm: 120 }],
  notes: [note(0, 0, 1), note(1, 1, 0.5), note(2, 1.5, 0.5), note(3, 2, 2)],
  end: [4, 1],
});

function makeLoop(extra) {
  const made = makeTransport({ timeline: loopTimeline, loop: { start: 1, end: 3 }, ...extra });
  made.at = (note) => note.when - (10 + 0.05); /* seconds after the first note's start */
  return made;
}

{
  const { transport, log, advance, at } = makeLoop();
  transport.play(0);
  advance(15.2);
  const seen = log.notes.map((n) => `${n.i}@${at(n).toFixed(2)}`);
  check(
    seen.join() === "0@0.00,1@2.00,2@3.00,3@4.00,1@6.00,2@7.00,3@8.00,1@10.00,2@11.00,3@12.00,1@14.00,2@15.00",
    "the intro plays once and the loop notes repeat at exact positions across three wraps",
  );
  check(near(log.notes[3].duration, 2) && near(log.notes[6].duration, 2), "a held note is cut at the loop end");
  check(near(log.notes[0].duration, 2), "an intro note is not cut");
  const starts = new Set(log.notes.map((n) => at(n).toFixed(6)));
  check(starts.size === log.notes.length, "no note starts twice");
  check(transport.state() === "playing", "a loop keeps playing past the end of the piece");
  const here = transport.position();
  check(here >= 1 && here < 3, "the position stays inside the loop after a wrap");
  transport.stop();
}

{
  const { transport, log, advance } = makeLoop();
  transport.play(0);
  advance(3);
  transport.setLoop(null);
  advance(14);
  check(log.notes.map((n) => n.i).join() === "0,1,2,3", "turning the loop off plays on to the end of the piece, each note once");
  check(transport.state() === "stopped" && log.ends === 1, "without the loop the piece ends");
}

{
  const { transport, log, advance } = makeLoop();
  transport.play(1.5);
  advance(5);
  check(log.notes.map((n) => n.i).slice(0, 4).join() === "2,3,1,2", "a start inside the loop plays to the loop end, then wraps");
  transport.stop();
}

{
  const { transport, log, advance } = makeLoop();
  transport.play(0.5, 2);
  advance(5);
  check(log.notes.map((n) => n.i).join() === "1,2" && log.ends === 1, "a ranged play ignores the loop");
}

{
  const { transport, advance } = makeLoop();
  transport.play(0);
  advance(7);
  transport.pause();
  const held = transport.position();
  advance(2);
  check(near(transport.position(), held) && held >= 1 && held < 3, "pause inside a later pass holds a position inside the loop");
  transport.stop();
}

/* Play ending: loop [1, 4) with the body ending at 3 (bar 4 is the turnaround). The ending timeline repeats the
 * intro and the body, then adds two ending bars. */
const bodyTimeline = PlaybackEngine.buildTimeline({
  tempo: [{ at: [0, 1], qpm: 120 }],
  notes: [note(0, 0, 1), note(1, 1, 1), note(2, 2, 1), note(3, 3, 1)],
  end: [4, 1],
});
const endingTimeline = PlaybackEngine.buildTimeline({
  tempo: [{ at: [0, 1], qpm: 120 }],
  notes: [note(0, 0, 1), note(1, 1, 1), note(2, 2, 1), note(10, 3, 1), note(11, 4, 1)],
  end: [5, 1],
});

function playEndingAfter(seconds) {
  let switched = 0;
  const wrapped = makeTransport({
    timeline: bodyTimeline,
    loop: { start: 1, end: 4 },
    onEnding: () => {
      switched++;
    },
  });
  wrapped.transport.play(0);
  wrapped.advance(seconds);
  const accepted = wrapped.transport.playEnding(endingTimeline, 3);
  wrapped.advance(30);
  const names = wrapped.log.notes.map((n) => `${n.ending ? "E" : ""}${n.i}@${(n.when - 10.05).toFixed(2)}`);
  return { accepted, names, switched, wrapped };
}

{
  /* before the body end of the first pass (bar 2 is playing) */
  const r = playEndingAfter(3);
  check(r.accepted, "Play ending before the body end is accepted");
  check(r.names.join() === "0@0.00,1@2.00,2@4.00,E10@6.00,E11@8.00", "before the body end the ending follows this pass's body");
  check(r.switched === 1 && r.wrapped.transport.state() === "stopped" && r.wrapped.log.ends === 1, "the ending ends playback once");
}

{
  /* in the turnaround of the first pass (bar 4 plays at 6 to 8 s) */
  const r = playEndingAfter(6.5);
  check(r.names.join() === "0@0.00,1@2.00,2@4.00,3@6.00,1@8.00,2@10.00,E10@12.00,E11@14.00", "in the turnaround the ending waits for the next pass's body end");
  check(r.wrapped.transport.state() === "stopped" && r.switched === 1, "the switch happens once and playback stops");
}

{
  /* just after the wrap (second pass has begun at 8 s) */
  const r = playEndingAfter(8.5);
  check(r.names.join() === "0@0.00,1@2.00,2@4.00,3@6.00,1@8.00,2@10.00,E10@12.00,E11@14.00", "just after a wrap the ending takes this pass's body end");
}

{
  const r = playEndingAfter(3);
  const lastEnding = r.wrapped.log.notes.filter((n) => n.ending);
  check(lastEnding.length === 2 && lastEnding[1].i === 11 && near(lastEnding[1].duration, 2), "the ending plays to its own end");
}

{
  const { transport } = makeTransport({ timeline: bodyTimeline, loop: { start: 1, end: 4 } });
  check(transport.playEnding(endingTimeline, 3) === false, "Play ending while stopped is refused");
}

{
  const made = makeTransport({ timeline: bodyTimeline });
  made.transport.play(0);
  made.advance(7);
  check(made.transport.playEnding(endingTimeline, 3) === false, "without a loop, an ending whose body end has passed is refused");
  made.transport.stop();
}

{
  const made = makeTransport({ timeline: bodyTimeline, loop: { start: 1, end: 4 } });
  made.transport.play(0);
  made.advance(1);
  made.transport.playEnding(endingTimeline, 3);
  made.transport.stop();
  made.transport.play(0);
  made.advance(6);
  check(!made.log.notes.some((n) => n.ending) && made.transport.endingPending() === false, "stop drops a pending ending");
  made.transport.stop();
}

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
