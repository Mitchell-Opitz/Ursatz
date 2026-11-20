/* The playback transport of the score workspace: the Play, Stop, Loop, Play ending, tempo, volume and Sound controls, the audio output and the sound
 * sources, the playback cursor and its follow, seeking from the score, the position readout and the ending swap. createTransport(host) builds it
 * when the workspace mounts; the host gives what the transport reads of the workspace:
 *   opts, compact, state, layoutParts, playGroup, canvas, paper, main, osmdHost, transportEl (the toolbar's transport box), iconButton, inspectorView,
 *   announce, savePrefs, setStatus, svgTopInCanvas, reducedMotion(), and the live values selection(), overlay(), osmd(), layout(), timelineOffset(),
 *   playbackData(), isDestroyed(), plus scoreVersionLoaded(text) when the score on show changed to another version's text.
 * It returns what the workspace calls. */
import PlaybackEngine from "./playback-engine.js";
import Reverb from "./reverb.js";
import ScoreGeometry from "./score-geometry.js";
import Instruments from "./instruments.js";
import SoundSources from "./sound-sources.js";
import WorkspaceState from "../models/workspace-state.js";
import { el } from "../core/dom.js";

export function createTransport(host) {
  const { opts, compact, state, layoutParts, playGroup, canvas, paper, main, osmdHost, transportEl, iconButton, inspectorView, announce, savePrefs, setStatus, svgTopInCanvas, reducedMotion } = host;
  /* The playback cursor shows from the first play (or seek) on. */
  let cursorShown = false;
  let cursorFrame = null;


  const MAX_VOICES = 64;
  const NOTE_VELOCITY = 0.7;
  const FALLBACK_SOURCE_ID = "basic-synth";
  const registry = SoundSources.createDefaultRegistry(Instruments);
  registry.mapProgram(0, "gm-piano");
  let timeline = null;
  let endingTimeline = null;
  let loopOn = Boolean(opts.loop) && !compact;
  /* the score on show is the ending version's while it plays */
  let endingShown = false;
  let loopXmlText = "";
  let endingXmlText = null;
  /* note index -> onset in whole notes, for seeking from a colored note */
  let onsetByNote = new Map();
  let player = null;
  let audio = null; /* {ctx, bus, dry, master} once the first Play click made it */
  let soundsReady = false;
  let preparing = false;
  const failedSources = new Set();
  const voices = [];
  const partInfo = new Map();
  const pitchesByPart = new Map();

  const playButton = el("button", "ws-btn ws-play", "Play");
  playButton.type = "button";
  playButton.disabled = true;
  playButton.title = "Play or pause (Space)";
  playButton.addEventListener("click", togglePlay);
  const stopButton = iconButton("Stop", "Stop and go back to the start", "Home", rewind);
  stopButton.disabled = true;
  const loopButton = el("button", "ws-btn ws-loop", "Loop");
  loopButton.type = "button";
  loopButton.title = "Repeat the loop without a gap";
  loopButton.setAttribute("aria-pressed", String(loopOn));
  loopButton.addEventListener("click", toggleLoop);
  const endingButton = el("button", "ws-btn ws-ending", "Play ending");
  endingButton.type = "button";
  endingButton.title = "Finish at the end of the loop body with the ending";
  endingButton.disabled = true;
  endingButton.addEventListener("click", playEnding);
  const readout = el("span", "ws-readout", "m1 0:00 / 0:00");
  readout.setAttribute("aria-label", "Position");
  const tempoDown = iconButton("−", "Slower", "5% steps", () => nudgeTempo(-5));
  const tempoLabel = iconButton("100%", "Reset tempo to 100%", "click", () => setTempoPercent(100));
  tempoLabel.classList.add("ws-tempo-label");
  const tempoUp = iconButton("+", "Faster", "5% steps", () => nudgeTempo(5));
  const tempoGroup = el("div", "ws-tempo");
  tempoGroup.setAttribute("role", "group");
  tempoGroup.setAttribute("aria-label", "Tempo");
  tempoGroup.append(tempoDown, tempoLabel, tempoUp);
  const volume = el("input", "ws-volume");
  volume.type = "range";
  volume.min = "0";
  volume.max = "100";
  volume.step = "1";
  volume.value = String(state.volume);
  volume.setAttribute("aria-label", "Volume");
  volume.title = "Volume";
  volume.addEventListener("input", () => {
    state.volume = Number(volume.value);
    applyVolume();
    savePrefs();
  });
  const soundButton = el("button", "ws-btn ws-sound-btn", "Sound");
  soundButton.type = "button";
  soundButton.setAttribute("aria-haspopup", "true");
  soundButton.setAttribute("aria-expanded", "false");
  soundButton.disabled = true;
  const soundMenu = el("div", "ws-sound");
  soundMenu.hidden = true;
  soundButton.addEventListener("click", () => {
    if (soundMenu.hidden) {
      buildSoundMenu();
      soundMenu.hidden = false;
      soundButton.setAttribute("aria-expanded", "true");
    } else {
      closeSoundMenu();
    }
  });
  const transportParts = {
    play: [playButton],
    stop: [stopButton],
    loop: [loopButton],
    ending: [endingButton],
    readout: [readout],
    tempo: [tempoGroup],
    volume: [volume],
    sound: [soundButton, soundMenu],
  };
  for (const name of layoutParts.transport) {
    transportEl.append(...transportParts[name]);
  }

  function closeSoundMenu() {
    if (soundMenu.hidden) {
      return false;
    }
    soundMenu.hidden = true;
    soundButton.setAttribute("aria-expanded", "false");
    return true;
  }

  function ensureAudio() {
    if (audio) {
      return audio;
    }
    const Context = globalThis.AudioContext || globalThis.webkitAudioContext;
    const ctx = new Context();
    const bus = ctx.createGain();
    const dry = ctx.createGain();
    const master = ctx.createGain();
    const compressor = ctx.createDynamicsCompressor();
    const convolver = ctx.createConvolver();
    const wet = ctx.createGain();
    const impulse = Reverb.buildImpulse(ctx.sampleRate, 2.2, 3, Math.random);
    const impulseBuffer = ctx.createBuffer(2, impulse[0].length, ctx.sampleRate);
    impulseBuffer.copyToChannel(impulse[0], 0);
    impulseBuffer.copyToChannel(impulse[1], 1);
    convolver.buffer = impulseBuffer;
    bus.connect(dry);
    bus.connect(convolver);
    convolver.connect(wet);
    dry.connect(master);
    wet.connect(master);
    master.connect(compressor);
    compressor.connect(ctx.destination);
    audio = { ctx, bus, dry, wet, master };
    const gains = Reverb.reverbGains(state.reverb);
    dry.gain.value = gains.dry;
    wet.gain.value = gains.wet;
    applyVolume();
    return audio;
  }

  /* Both gains move with a short time constant, so the slider makes no
   * clicks while playing. */
  function applyReverb() {
    if (audio) {
      const gains = Reverb.reverbGains(state.reverb);
      audio.dry.gain.setTargetAtTime(gains.dry, audio.ctx.currentTime, 0.03);
      audio.wet.gain.setTargetAtTime(gains.wet, audio.ctx.currentTime, 0.03);
    }
  }

  function applyVolume() {
    if (audio) {
      audio.master.gain.setTargetAtTime(state.volume / 100, audio.ctx.currentTime, 0.03);
    }
  }

  /* The source a part plays: its override or program, and the basic synth
   * when that source could not load. */
  function sourceForPartIndex(partIndex) {
    const info = partInfo.get(partIndex);
    const source = registry.sourceForPart(partIndex, info ? info.program : 0);
    if (failedSources.has(source.id)) {
      return registry.sources().find((candidate) => candidate.id === FALLBACK_SOURCE_ID) || source;
    }
    return source;
  }

  /* Loads every source the parts use, with the pitches each will play. A
   * source that fails to load is replaced by the basic synth. */
  function prepareSounds() {
    const wanted = new Map();
    for (const partIndex of partInfo.keys()) {
      const source = sourceForPartIndex(partIndex);
      if (!wanted.has(source.id)) {
        wanted.set(source.id, { source, pitches: new Set() });
      }
      for (const pitch of pitchesByPart.get(partIndex) || []) {
        wanted.get(source.id).pitches.add(pitch);
      }
    }
    return Promise.all(
      Array.from(wanted.values()).map(({ source, pitches }) =>
        source.load(audio.ctx, { pitches: Array.from(pitches) }).catch(() => {
          failedSources.add(source.id);
          setStatus("Piano samples could not load; using basic synth", false);
        }),
      ),
    );
  }

  function playNote(note) {
    if (!audio) {
      return;
    }
    const source = sourceForPartIndex(note.part);
    const voice = source.start(audio.ctx, audio.bus, {
      pitch: note.pitch,
      velocity: NOTE_VELOCITY,
      when: note.when,
      duration: note.duration,
    });
    const now = audio.ctx.currentTime;
    while (voices.length > 0 && voices[0].end < now) {
      voices.shift();
    }
    voices.push({ voice, end: note.when + note.duration + 0.5 });
    while (voices.length > MAX_VOICES) {
      voices.shift().voice.stop(now);
    }
  }

  function silenceVoices() {
    if (!audio) {
      return;
    }
    const now = audio.ctx.currentTime;
    for (const entry of voices.splice(0)) {
      entry.voice.stop(now);
    }
  }

  function onPlaybackLoaded() {
    timeline = PlaybackEngine.buildTimeline(host.playbackData());
    onsetByNote = new Map(timeline.notes.map((note) => [note.i, note.on]));
    partInfo.clear();
    pitchesByPart.clear();
    for (const part of host.playbackData().parts || []) {
      partInfo.set(part.index, part);
    }
    for (const note of host.playbackData().notes || []) {
      if (!pitchesByPart.has(note.part)) {
        pitchesByPart.set(note.part, new Set());
      }
      pitchesByPart.get(note.part).add(note.pitch);
      if (!partInfo.has(note.part)) {
        partInfo.set(note.part, { index: note.part, name: "", program: 0 });
      }
    }
    endingTimeline =
      opts.ending && opts.ending.playback ? PlaybackEngine.buildTimeline(opts.ending.playback) : null;
    player = PlaybackEngine.createTransport({
      timeline,
      loop: loopOn ? opts.loop : null,
      onEnding: () => {
        showScoreVersion(true);
        syncTransport();
      },
      clock: () => (audio ? audio.ctx.currentTime : 0),
      setTimer: (fn, ms) => setInterval(fn, ms),
      clearTimer: (handle) => clearInterval(handle),
      onNote: playNote,
      onState: () => syncTransport(),
      onEnd: () => announce("Playback finished"),
    });
    const playable = timeline.notes.length > 0;
    soundButton.disabled = !playable;
    syncTransport();
    /* a claim shown before the data arrived gets its Play button */
    inspectorView.show(host.selection());
    if (opts.autoplay && playable) {
      startPlayback();
    }
  }

  function syncTransport() {
    const current = player ? player.state() : "stopped";
    playButton.disabled = !player || timeline.notes.length === 0 || preparing;
    playButton.textContent = preparing ? "Loading piano..." : current === "playing" ? "Pause" : "Play";
    playButton.setAttribute("aria-pressed", String(current === "playing"));
    stopButton.disabled = !player;
    loopButton.disabled = !player;
    loopButton.setAttribute("aria-pressed", String(loopOn));
    const endingBusy = Boolean(player) && (player.endingPending() || player.endingPlaying());
    endingButton.disabled = !player || !endingTimeline || preparing || endingBusy;
    endingButton.textContent = endingBusy ? "Ending next" : "Play ending";
    if (current === "stopped" && endingShown) {
      showScoreVersion(false);
    }
    const percent = Math.round((player ? player.rate() : 1) * 100);
    tempoLabel.textContent = `${percent}%`;
    tempoDown.disabled = !player || percent <= PlaybackEngine.MIN_RATE * 100;
    tempoUp.disabled = !player || percent >= PlaybackEngine.MAX_RATE * 100;
    tempoLabel.disabled = !player;
    updateReadout();
    if (current === "playing") {
      cursorShown = true;
      if (lastTransportState !== "playing") {
        setFollow(true);
      }
      startCursorLoop();
    }
    lastTransportState = current;
    syncFollowPill();
    updateCursor();
  }

  /* Where the cursor stands, in piece positions. Reduced motion steps it
   * from onset to onset while playing. */
  function cursorPosition() {
    const position = player.position();
    if (player.state() === "playing" && reducedMotion()) {
      return PlaybackEngine.onsetAtOrBefore(timeline, position);
    }
    return position;
  }

  /* Puts the cursor where the music is. It stays at the pause or stop
   * position and is hidden only before the first play. */
  function updateCursor() {
    if (!host.overlay() || !player || !host.layout()) {
      return null;
    }
    if (!cursorShown) {
      host.overlay().hideCursor();
      return null;
    }
    return host.overlay().setCursor(host.layout(), cursorPosition() + host.timelineOffset());
  }

  /* Follow: while playing, the score scrolls to keep the cursor's system
   * in view. Reading ahead by hand (wheel, touch, scrollbar, scroll keys)
   * turns it off and shows a "Follow" pill; the pill or C turns it on. */
  let following = true;
  let followedSystem = null;
  let lastTransportState = "stopped";
  const followPill = el("button", "ws-follow-pill", "Follow");
  followPill.type = "button";
  followPill.title = "Follow the cursor (C)";
  followPill.hidden = true;
  main.appendChild(followPill);

  function syncFollowPill() {
    followPill.hidden = following || !player || player.state() !== "playing";
  }

  function setFollow(value) {
    following = value;
    followedSystem = null;
    syncFollowPill();
  }

  function systemRectInCanvas(systemIndex) {
    const system = host.layout() && host.layout().systems.find((s) => s.index === systemIndex);
    if (!system) {
      return null;
    }
    const base = svgTopInCanvas();
    return { top: base + system.top, bottom: base + system.bottom };
  }

  function followScroll(systemIndex, force) {
    const rect = systemRectInCanvas(systemIndex);
    if (!rect) {
      return;
    }
    const target = WorkspaceState.followScrollTarget(
      { scrollTop: canvas.scrollTop, height: canvas.clientHeight, contentHeight: canvas.scrollHeight },
      rect,
      force,
    );
    if (target !== null) {
      canvas.scrollTo({ top: target, behavior: reducedMotion() ? "auto" : "smooth" });
    }
  }

  /* The Follow pill and C: follow again and scroll to the cursor. */
  function followNow() {
    if (!player) {
      return;
    }
    setFollow(true);
    const spot = updateCursor();
    if (spot) {
      followScroll(spot.system, true);
    }
  }

  function manualScroll() {
    if (following && player && player.state() === "playing") {
      setFollow(false);
    }
  }
  followPill.addEventListener("click", followNow);
  canvas.addEventListener("wheel", manualScroll, { passive: true });
  canvas.addEventListener("touchmove", manualScroll, { passive: true });
  /* a press on the scrollbar lands on the canvas itself, beyond its content */
  canvas.addEventListener("pointerdown", (event) => {
    if (event.target === canvas && event.clientX >= canvas.getBoundingClientRect().left + canvas.clientWidth) {
      manualScroll();
    }
  });

  /* Moves the play position (piece position); playing carries on from
   * there. */
  function seekTo(w) {
    if (!player) {
      return;
    }
    if (player.state() === "playing") {
      silenceVoices();
    }
    player.seek(w);
    cursorShown = true;
    followedSystem = null;
    syncTransport();
  }

  /* A click on bare paper: the system under it, the position under the
   * click, snapped to the nearest note onset of that system. Clicks on
   * ribbons, marks and colored notes never get here (they select). */
  function seekToPoint(x, y) {
    let system = null;
    let nearest = Infinity;
    for (const candidate of host.layout().systems) {
      const gap = y < candidate.top ? candidate.top - y : y > candidate.bottom ? y - candidate.bottom : 0;
      if (candidate.measures.length > 0 && gap < nearest) {
        nearest = gap;
        system = candidate;
      }
    }
    if (!system) {
      return;
    }
    const from = system.measures[0].start - host.timelineOffset();
    const to = system.measures[system.measures.length - 1].end - host.timelineOffset();
    const raw = ScoreGeometry.tAt(host.layout(), system.index, x) - host.timelineOffset();
    const snapped = PlaybackEngine.nearestOnset(timeline, raw, from, to);
    seekTo(snapped !== null ? snapped : Math.min(to, Math.max(from, raw)));
  }

  paper.addEventListener("click", (event) => {
    const svg = osmdHost.querySelector("svg");
    if (!player || !host.layout() || !svg || state.mode !== "score" || timeline.notes.length === 0) {
      return;
    }
    const rect = svg.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    if (x >= 0 && y >= 0 && x <= rect.width && y <= rect.height) {
      seekToPoint(x, y);
    }
  });

  /* One animation frame at a time while playing, so the cursor glides. */
  function cursorTick() {
    cursorFrame = null;
    if (host.isDestroyed()) {
      return;
    }
    const spot = updateCursor();
    if (spot && following && player.state() === "playing" && spot.system !== followedSystem) {
      followedSystem = spot.system;
      followScroll(spot.system, false);
    }
    if (player && player.state() === "playing") {
      cursorFrame = requestAnimationFrame(cursorTick);
    }
  }

  function startCursorLoop() {
    if (cursorFrame === null) {
      cursorFrame = requestAnimationFrame(cursorTick);
    }
  }

  /* The number of the measure holding OSMD time t (the last one past the
   * end, the first before it). */
  function measureNumberAt(t) {
    const measures = host.layout() ? host.layout().systems.flatMap((system) => system.measures) : [];
    if (measures.length === 0) {
      return 1;
    }
    const found = measures.find((measure) => t >= measure.start && t < measure.end);
    if (found) {
      return found.number;
    }
    return t < measures[0].start ? measures[0].number : measures[measures.length - 1].number;
  }

  function updateReadout() {
    if (!player) {
      return;
    }
    const position = player.position();
    const rate = player.rate();
    const here = PlaybackEngine.secondsAt(timeline, position) / rate;
    const total = PlaybackEngine.secondsAt(timeline, timeline.end) / rate;
    const text = `m${measureNumberAt(position + host.timelineOffset())} ${PlaybackEngine.formatClock(here)} / ${PlaybackEngine.formatClock(total)}`;
    if (readout.textContent !== text) {
      readout.textContent = text;
    }
  }
  const readoutTimer = setInterval(updateReadout, 100);

  /* What the page's play group calls when another workspace starts. */
  const groupEntry = {
    pause() {
      if (player && player.state() === "playing") {
        player.pause();
        silenceVoices();
      }
    },
  };
  playGroup.add(groupEntry);

  function togglePlay() {
    if (!player || preparing) {
      return;
    }
    if (player.state() === "playing") {
      closeSoundMenu();
      player.pause();
      silenceVoices();
      return;
    }
    startPlayback();
  }

  /* Starts the sound (the audio context and the samples on the first
   * time) and plays from `from` to `to`, both piece positions; with none
   * it plays or resumes from where the transport stands. */
  async function startPlayback(from, to) {
    if (!player || preparing) {
      return;
    }
    closeSoundMenu();
    ensureAudio();
    if (audio.ctx.state === "suspended") {
      await audio.ctx.resume().catch(() => {});
    }
    if (!soundsReady) {
      preparing = true;
      syncTransport();
      await prepareSounds();
      soundsReady = true;
      preparing = false;
      if (host.isDestroyed()) {
        return;
      }
    }
    silenceVoices();
    playGroup.claim(groupEntry);
    player.play(from, to);
  }

  /* Plays a claim's span and returns the cursor to its start at the end.
   * A span with no length plays a quarter note. */
  function playItem(item) {
    if (!item || !player) {
      return;
    }
    const end = item.end > item.start ? item.end : item.start + 0.25;
    startPlayback(item.start, end);
  }

  function toggleLoop() {
    if (!player) {
      return;
    }
    loopOn = !loopOn;
    player.setLoop(loopOn ? opts.loop : null);
    syncTransport();
  }

  /* Plays on (from the start when stopped) and makes the end of the loop
   * body the last pass: the ending takes over there and playback stops. */
  async function playEnding() {
    if (!player || !endingTimeline || preparing) {
      return;
    }
    prefetchEndingScore();
    if (player.state() !== "playing") {
      await startPlayback(player.state() === "stopped" ? 0 : undefined);
    }
    if (player.state() === "playing") {
      player.playEnding(endingTimeline, opts.ending.bodyEnd);
    }
    syncTransport();
  }

  function prefetchEndingScore() {
    if (endingXmlText !== null || !opts.ending || !opts.ending.musicxmlUrl) {
      return;
    }
    endingXmlText = "";
    fetch(opts.ending.musicxmlUrl)
      .then((res) => (res.ok ? res.text() : ""))
      .then((text) => {
        endingXmlText = text;
      })
      .catch(() => {
        endingXmlText = null;
      });
  }

  /* Swaps the score on show between the loop version and the ending
   * version. The highlights belong to the loop version, so they are off
   * while the ending shows. */
  async function showScoreVersion(useEnding) {
    const text = useEnding ? endingXmlText : loopXmlText;
    if (!host.osmd() || host.isDestroyed() || endingShown === useEnding || !text) {
      return;
    }
    endingShown = useEnding;
    try {
      await host.osmd().load(text);
    } catch (err) {
      endingShown = !useEnding;
      return;
    }
    host.scoreVersionLoaded(text);
  }

  function rewind() {
    if (!player) {
      return;
    }
    player.stop();
    silenceVoices();
    player.seek(0);
    syncTransport();
  }

  function setTempoPercent(percent) {
    if (!player) {
      return;
    }
    player.setRate(percent / 100);
    syncTransport();
  }

  function nudgeTempo(delta) {
    if (player) {
      setTempoPercent(Math.round(player.rate() * 100) + delta);
    }
  }

  /* The Sound menu: a source per part and the credits of the sources in
   * use. */
  function buildSoundMenu() {
    soundMenu.innerHTML = "";
    const sources = registry.sources();
    for (const [partIndex, info] of partInfo) {
      const row = el("label", "ws-sound-row");
      row.appendChild(el("span", "ws-sound-part", info.name || `Part ${partIndex + 1}`));
      const select = el("select", "ws-sound-select");
      for (const source of sources) {
        const option = el("option", "", source.label);
        option.value = source.id;
        select.appendChild(option);
      }
      select.value = registry.sourceForPart(partIndex, info.program).id;
      select.addEventListener("change", () => {
        registry.setPartSource(partIndex, select.value);
        failedSources.delete(select.value);
        if (audio) {
          soundsReady = false;
          preparing = true;
          syncTransport();
          prepareSounds().then(() => {
            soundsReady = true;
            preparing = false;
            syncTransport();
          });
        }
        buildSoundMenu();
      });
      row.appendChild(select);
      soundMenu.appendChild(row);
    }
    const reverbRow = el("label", "ws-sound-row");
    const reverbText = el("span", "ws-sound-part", `Reverb ${state.reverb}`);
    const reverbSlider = el("input", "ws-reverb");
    reverbSlider.type = "range";
    reverbSlider.min = "0";
    reverbSlider.max = "100";
    reverbSlider.step = "1";
    reverbSlider.value = String(state.reverb);
    reverbSlider.setAttribute("aria-label", "Reverb");
    reverbSlider.addEventListener("input", () => {
      state.reverb = Number(reverbSlider.value);
      reverbText.textContent = `Reverb ${state.reverb}`;
      applyReverb();
      savePrefs();
    });
    reverbRow.append(reverbText, reverbSlider);
    soundMenu.appendChild(reverbRow);
    const credits = new Set();
    for (const partIndex of partInfo.keys()) {
      const source = registry.sourceForPart(partIndex, (partInfo.get(partIndex) || {}).program || 0);
      if (source.credit) {
        credits.add(source.credit);
      }
    }
    for (const credit of credits) {
      soundMenu.appendChild(el("p", "ws-sound-credit", credit));
    }
    if (timeline && timeline.tempoSource === "default") {
      soundMenu.appendChild(el("p", "ws-sound-hint", "No tempo in the file; playing at 120"));
    }
  }

  function destroy() {
    playGroup.remove(groupEntry);
    clearInterval(readoutTimer);
    if (cursorFrame !== null) {
      cancelAnimationFrame(cursorFrame);
      cursorFrame = null;
    }
    if (player) {
      player.stop();
    }
    silenceVoices();
    if (audio) {
      audio.ctx.close().catch(() => {});
      audio = null;
    }
  }

  return {
    closeSoundMenu,
    canPlay: () => Boolean(player) && timeline.notes.length > 0,
    hasPlayer: () => Boolean(player),
    playItem,
    endingShown: () => endingShown,
    updateCursor,
    onsetOf: (noteIndex) => onsetByNote.get(noteIndex),
    seekTo,
    manualScroll,
    togglePlay,
    rewind,
    followNow,
    resetFollow: () => {
      followedSystem = null;
    },
    onPlaybackLoaded,
    setLoopXml: (xml) => {
      loopXmlText = xml;
      prefetchEndingScore();
    },
    destroy,
  };
}
