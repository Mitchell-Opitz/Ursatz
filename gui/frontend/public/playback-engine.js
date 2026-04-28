/* The pure timing engine for playback: a timeline built from the playback
 * route's JSON (notes by onset, tempo regions with cumulative seconds), exact
 * conversions between whole-note positions and seconds, and a transport that
 * schedules notes a little ahead of a clock. The clock and the timer are
 * injected: the browser passes the AudioContext time and setInterval, tests
 * pass a fake clock.
 *
 * Positions and lengths are whole notes as numbers. The seconds of a span of
 * whole notes at q quarters per minute are span * 4 * 60 / q.
 *
 * Pure, no DOM or audio access, so Node tests load this file directly. */
(function (root) {
  const WHOLE_NOTE_SECONDS_AT_QPM = 240; /* 4 quarters * 60 seconds */
  const DEFAULT_QPM = 120;
  const TICK_MS = 25;
  const LOOKAHEAD_SECONDS = 0.12;
  const START_LEAD_SECONDS = 0.05;
  const MIN_RATE = 0.5;
  const MAX_RATE = 1.5;
  const EPSILON = 1e-9;

  function ratNumber(value) {
    if (Array.isArray(value)) {
      return value[1] === 0 ? 0 : value[0] / value[1];
    }
    return Number(value) || 0;
  }

  /* Notes sorted by onset (then by index) with numeric on and len, tempo
   * regions {at, qpm, seconds} with cumulative seconds at each region start
   * (the first region always starts at 0), and the end in whole notes. */
  function buildTimeline(playback) {
    const source = playback || {};
    const notes = (source.notes || []).map((note) => ({
      i: note.i,
      on: ratNumber(note.on),
      len: ratNumber(note.len),
      pitch: note.pitch,
      part: note.part,
    }));
    notes.sort((a, b) => a.on - b.on || a.i - b.i);

    const tempo = (source.tempo || []).map((region) => ({ at: ratNumber(region.at), qpm: region.qpm }));
    tempo.sort((a, b) => a.at - b.at);
    if (tempo.length === 0) {
      tempo.push({ at: 0, qpm: DEFAULT_QPM });
    } else if (tempo[0].at > EPSILON) {
      tempo.unshift({ at: 0, qpm: tempo[0].qpm });
    } else {
      tempo[0].at = 0;
    }
    const regions = [];
    let seconds = 0;
    for (let i = 0; i < tempo.length; i++) {
      if (i > 0) {
        seconds += ((tempo[i].at - tempo[i - 1].at) * WHOLE_NOTE_SECONDS_AT_QPM) / tempo[i - 1].qpm;
      }
      regions.push({ at: tempo[i].at, qpm: tempo[i].qpm, seconds });
    }

    let end = ratNumber(source.end);
    for (const note of notes) {
      end = Math.max(end, note.on + note.len);
    }
    return { notes, tempo: regions, end, tempoSource: source.tempo_source || "default" };
  }

  function secondsAt(timeline, w) {
    const position = Math.max(0, w);
    const regions = timeline.tempo;
    let r = regions[0];
    for (let i = regions.length - 1; i >= 0; i--) {
      if (regions[i].at <= position + EPSILON) {
        r = regions[i];
        break;
      }
    }
    return r.seconds + ((position - r.at) * WHOLE_NOTE_SECONDS_AT_QPM) / r.qpm;
  }

  function positionAt(timeline, s) {
    const seconds = Math.max(0, s);
    const regions = timeline.tempo;
    let r = regions[0];
    for (let i = regions.length - 1; i >= 0; i--) {
      if (regions[i].seconds <= seconds + EPSILON) {
        r = regions[i];
        break;
      }
    }
    return r.at + ((seconds - r.seconds) * r.qpm) / WHOLE_NOTE_SECONDS_AT_QPM;
  }

  /* Seconds as m:ss, rounded down. */
  function formatClock(seconds) {
    const whole = Math.max(0, Math.floor(seconds + EPSILON));
    return `${Math.floor(whole / 60)}:${String(whole % 60).padStart(2, "0")}`;
  }

  function firstIndexAtOrAfter(notes, w) {
    let lo = 0;
    let hi = notes.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (notes[mid].on < w - EPSILON) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    return lo;
  }

  /* The note onset nearest to w among the onsets in [from, to), or null when
   * none falls there. A tie goes to the earlier onset. Click-to-seek snaps a
   * click to the music with it. */
  function nearestOnset(timeline, w, from, to) {
    const notes = timeline.notes;
    const lo = firstIndexAtOrAfter(notes, from);
    const hi = firstIndexAtOrAfter(notes, to);
    if (lo >= hi) {
      return null;
    }
    const after = Math.min(hi, Math.max(lo, firstIndexAtOrAfter(notes, w)));
    let best = null;
    for (const index of [after - 1, after]) {
      if (index < lo || index >= hi) {
        continue;
      }
      if (best === null || Math.abs(notes[index].on - w) < Math.abs(best - w) - EPSILON) {
        best = notes[index].on;
      }
    }
    return best;
  }

  /* The latest note onset at or before w, or w itself when no note has
   * started yet. The cursor steps between these for readers who prefer
   * reduced motion. */
  function onsetAtOrBefore(timeline, w) {
    const index = firstIndexAtOrAfter(timeline.notes, w + 2 * EPSILON) - 1;
    return index >= 0 ? timeline.notes[index].on : w;
  }

  function createTransport(options) {
    const clock = options.clock;
    const setTimer = options.setTimer;
    const clearTimer = options.clearTimer;
    const onNote = options.onNote || (() => {});
    const onState = options.onState || (() => {});
    const onEnd = options.onEnd || (() => {});
    const onEnding = options.onEnding || (() => {});
    const mainTimeline = options.timeline;
    let timeline = mainTimeline; /* the ending's timeline once the switch is made */
    let ending = null; /* {timeline, bodyEnd, at, next} while a switch is pending */
    let scheduledTo = 0; /* piece seconds the scheduler has looked ahead to */

    let state = "stopped";
    let rate = 1;
    let timer = null;
    let heldPosition = 0; /* the position while stopped or paused */
    let playFrom = 0; /* where stop() goes back to */
    let playTo = null; /* where playing stops, null for the end of the piece */
    let anchorClock = 0; /* clock time at the anchor */
    let anchorSeconds = 0; /* piece seconds (rate 1) at the anchor */
    let next = 0; /* index of the next note not yet scheduled */
    let loop = validLoop(options.loop); /* {start, end} in whole notes, or null */
    let loopActive = false; /* the loop wraps the current play */
    let shift = 0; /* seconds added to the notes of the pass being scheduled */

    function setState(value) {
      if (state !== value) {
        state = value;
        onState(value);
      }
    }

    function endPosition() {
      return playTo === null ? timeline.end : Math.min(playTo, timeline.end);
    }

    function currentSeconds() {
      return anchorSeconds + Math.max(0, clock() - anchorClock) * rate;
    }

    function loopSeconds() {
      return secondsAt(timeline, loop.end) - secondsAt(timeline, loop.start);
    }

    /* Piece seconds with the loop's wraps taken out. */
    function wrapSeconds(s) {
      if (!loopActive) {
        return s;
      }
      const endSeconds = secondsAt(timeline, loop.end);
      if (s < endSeconds - EPSILON) {
        return s;
      }
      const startSeconds = secondsAt(timeline, loop.start);
      return startSeconds + ((s - startSeconds) % (endSeconds - startSeconds));
    }

    function position() {
      if (state === "playing" && ending !== null && currentSeconds() >= ending.at - EPSILON) {
        const seconds = secondsAt(ending.timeline, ending.bodyEnd) + currentSeconds() - ending.at;
        return positionAt(ending.timeline, seconds);
      }
      return state === "playing" ? positionAt(timeline, wrapSeconds(currentSeconds())) : heldPosition;
    }

    function validLoop(value) {
      return value && value.end > value.start + EPSILON ? { start: value.start, end: value.end } : null;
    }

    function anchorAt(w) {
      anchorClock = clock() + START_LEAD_SECONDS;
      anchorSeconds = secondsAt(timeline, w);
      shift = 0;
      loopActive = loop !== null && playTo === null && w < loop.end - EPSILON;
      next = firstIndexAtOrAfter(timeline.notes, w);
      scheduledTo = anchorSeconds;
      if (ending !== null) {
        armEnding();
      }
    }

    /* Finds the moment the pending ending takes over: the next time the
     * body's end comes up that the scheduler has not already passed. Drops
     * the ending when no pass is left (no loop and the body's end behind). */
    function armEnding() {
      const bodyAt = secondsAt(timeline, ending.bodyEnd);
      let at = bodyAt;
      if (loopActive && ending.bodyEnd >= loop.start - EPSILON) {
        const passes = Math.max(0, Math.ceil((scheduledTo - bodyAt) / loopSeconds() - EPSILON));
        at = bodyAt + passes * loopSeconds();
      }
      if (at < scheduledTo - EPSILON) {
        ending = null;
        return false;
      }
      ending.at = at;
      ending.next = firstIndexAtOrAfter(ending.timeline.notes, ending.bodyEnd);
      return true;
    }

    /* The ending's notes start on the clock where the body's end is. */
    function endingNoteSeconds(note) {
      return ending.at + secondsAt(ending.timeline, note.on) - secondsAt(ending.timeline, ending.bodyEnd);
    }

    function swapToEnding() {
      const taken = ending;
      anchorClock += (taken.at - anchorSeconds) / rate;
      anchorSeconds = secondsAt(taken.timeline, taken.bodyEnd);
      timeline = taken.timeline;
      next = taken.next;
      shift = 0;
      loopActive = false;
      ending = null;
      onEnding();
    }

    function resetEnding() {
      timeline = mainTimeline;
      ending = null;
    }

    function startTimer() {
      if (timer === null) {
        timer = setTimer(tick, TICK_MS);
      }
    }

    function stopTimer() {
      if (timer !== null) {
        clearTimer(timer);
        timer = null;
      }
    }

    function tick() {
      if (state !== "playing") {
        return;
      }
      if (ending !== null && currentSeconds() >= ending.at - EPSILON) {
        swapToEnding();
      }
      const horizon = clock() + LOOKAHEAD_SECONDS;
      scheduledTo = anchorSeconds + (horizon - anchorClock) * rate;
      const stopAt = endPosition();
      const limit = ending !== null ? ending.at : Infinity;
      for (;;) {
        if (loopActive && (next >= timeline.notes.length || timeline.notes[next].on >= loop.end - EPSILON)) {
          const first = firstIndexAtOrAfter(timeline.notes, loop.start);
          if (first >= timeline.notes.length || timeline.notes[first].on >= loop.end - EPSILON) {
            break;
          }
          shift += loopSeconds();
          next = first;
          continue;
        }
        if (next >= timeline.notes.length) {
          break;
        }
        const note = timeline.notes[next];
        if (!loopActive && note.on >= stopAt - EPSILON) {
          break;
        }
        const noteStart = secondsAt(timeline, note.on);
        if (noteStart + shift >= limit - EPSILON) {
          break;
        }
        const when = anchorClock + (noteStart + shift - anchorSeconds) / rate;
        if (when > horizon) {
          break;
        }
        const noteEnd = loopActive ? Math.min(note.on + note.len, loop.end) : note.on + note.len;
        const duration = (secondsAt(timeline, noteEnd) - noteStart) / rate;
        next++;
        onNote({ i: note.i, pitch: note.pitch, part: note.part, when, duration, ending: timeline !== mainTimeline });
      }
      while (ending !== null && ending.next < ending.timeline.notes.length) {
        const note = ending.timeline.notes[ending.next];
        const when = anchorClock + (endingNoteSeconds(note) - anchorSeconds) / rate;
        if (when > horizon) {
          break;
        }
        const duration = (secondsAt(ending.timeline, note.on + note.len) - secondsAt(ending.timeline, note.on)) / rate;
        ending.next++;
        onNote({ i: note.i, pitch: note.pitch, part: note.part, when, duration, ending: true });
      }
      if (ending === null && !loopActive && currentSeconds() >= secondsAt(timeline, stopAt) - EPSILON) {
        stopTimer();
        resetEnding();
        heldPosition = playFrom;
        setState("stopped");
        onEnd();
      }
    }

    /* Plays from `from` (default: the current position) to `to` (default: the
     * end of the piece; with no arguments after a pause the earlier `to`
     * stays). */
    function play(from, to) {
      const explicit = from !== undefined && from !== null;
      if (state === "playing" && !explicit) {
        return;
      }
      const resuming = state === "paused" && !explicit;
      const start = explicit ? Math.max(0, from) : position();
      if (explicit || state === "stopped") {
        playTo = to === undefined ? null : to;
      }
      if (!resuming) {
        playFrom = start;
      }
      heldPosition = start;
      anchorAt(start);
      setState("playing");
      startTimer();
      tick();
    }

    function pause() {
      if (state !== "playing") {
        return;
      }
      heldPosition = Math.min(position(), endPosition());
      stopTimer();
      setState("paused");
    }

    function stop() {
      stopTimer();
      resetEnding();
      heldPosition = playFrom;
      setState("stopped");
    }

    function seek(w) {
      const target = Math.max(0, w);
      if (state === "playing") {
        anchorAt(target);
        heldPosition = target;
        tick();
      } else {
        heldPosition = target;
        if (state === "stopped") {
          playFrom = target;
        }
      }
    }

    /* Tempo multiplier from the current position on. */
    function setRate(value) {
      const clamped = Math.min(MAX_RATE, Math.max(MIN_RATE, value));
      if (state === "playing") {
        anchorSeconds = currentSeconds();
        anchorClock = clock();
      }
      rate = clamped;
    }

    /* Turns the loop on (a {start, end} in whole notes) or off (null); a
     * playing transport carries on from where it is. */
    function setLoop(value) {
      const here = position();
      const lookedTo = positionAt(timeline, wrapSeconds(scheduledTo));
      loop = validLoop(value);
      if (state === "playing") {
        anchorAt(here);
        /* notes already scheduled for the lookahead window are not played again */
        next = firstIndexAtOrAfter(timeline.notes, Math.max(here, lookedTo + 2 * EPSILON));
        heldPosition = here;
        tick();
      }
    }

    /* Makes the current play finish with `endingTimeline`: the pass goes on to
     * `bodyEnd` (a position in whole notes, the same in both timelines), then
     * the ending's notes from there play to its end and playback stops. Before
     * `bodyEnd` of this pass the switch waits for it; past it, the next pass's.
     * Returns false when nothing is playing, a range is playing, the ending has
     * begun, or no pass is left to switch in. */
    function playEnding(endingTimeline, bodyEnd) {
      if ((state !== "playing" && state !== "paused") || playTo !== null || timeline !== mainTimeline) {
        return false;
      }
      ending = { timeline: endingTimeline, bodyEnd, at: 0, next: 0 };
      if (state === "playing") {
        armEnding();
        tick();
      }
      return ending !== null;
    }

    return {
      play,
      pause,
      stop,
      seek,
      setRate,
      setLoop,
      playEnding,
      loop: () => loop,
      endingPending: () => ending !== null,
      endingPlaying: () => timeline !== mainTimeline,
      position,
      state: () => state,
      rate: () => rate,
    };
  }

  const api = {
    buildTimeline,
    secondsAt,
    positionAt,
    formatClock,
    nearestOnset,
    onsetAtOrBefore,
    createTransport,
    TICK_MS,
    LOOKAHEAD_SECONDS,
    MIN_RATE,
    MAX_RATE,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.PlaybackEngine = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
