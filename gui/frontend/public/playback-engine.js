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
    const timeline = options.timeline;
    const clock = options.clock;
    const setTimer = options.setTimer;
    const clearTimer = options.clearTimer;
    const onNote = options.onNote || (() => {});
    const onState = options.onState || (() => {});
    const onEnd = options.onEnd || (() => {});

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
      const horizon = clock() + LOOKAHEAD_SECONDS;
      const stopAt = endPosition();
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
        const when = anchorClock + (noteStart + shift - anchorSeconds) / rate;
        if (when > horizon) {
          break;
        }
        const noteEnd = loopActive ? Math.min(note.on + note.len, loop.end) : note.on + note.len;
        const duration = (secondsAt(timeline, noteEnd) - noteStart) / rate;
        next++;
        onNote({ i: note.i, pitch: note.pitch, part: note.part, when, duration });
      }
      if (!loopActive && currentSeconds() >= secondsAt(timeline, stopAt) - EPSILON) {
        stopTimer();
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
      loop = validLoop(value);
      if (state === "playing") {
        const here = position();
        anchorAt(here);
        heldPosition = here;
        tick();
      }
    }

    return {
      play,
      pause,
      stop,
      seek,
      setRate,
      setLoop,
      loop: () => loop,
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
