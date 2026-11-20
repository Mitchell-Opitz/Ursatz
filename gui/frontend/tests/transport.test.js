/* The playback transport of the score workspace (score/transport.js) with a fake clock, fake timers and a fake audio context: Play makes the audio
 * and plays the notes (the piano samples cannot load here, so the basic synth plays), Pause stops the sound, seeking moves the cursor, Loop is
 * a toggle, Play ending is offered with an ending and the tempo steps by 5%. Plain Node script, run through ctest. */
import path from "path";
import { fileURLToPath } from "url";
import PlaybackEngine from "../public/score/playback-engine.js";
import ScoreWorkspace from "../public/score/score-workspace.js";
import { createTransport } from "../public/score/transport.js";
import { check, finish } from "./support/check.js";
import { installDom } from "./support/dom.js";
import { readJson } from "./support/fixtures.js";

const here = path.dirname(fileURLToPath(import.meta.url));
const { document } = installDom();

/* The clock: audio time, the intervals the transport starts and the animation frames of the cursor, all driven by advance(seconds). */
let now = 0;
const intervals = new Map();
let nextHandle = 1;
const frames = [];
globalThis.setInterval = (fn, ms) => {
  intervals.set(nextHandle, { fn, ms, last: now });
  return nextHandle++;
};
globalThis.clearInterval = (handle) => intervals.delete(handle);
globalThis.requestAnimationFrame = (fn) => frames.push(fn);
globalThis.cancelAnimationFrame = () => {};
function advance(seconds) {
  const end = now + seconds;
  while (now < end) {
    now = Math.min(end, now + 0.01);
    for (const timer of [...intervals.values()]) {
      if (now - timer.last >= timer.ms / 1000 - 1e-9) {
        timer.last = now;
        timer.fn();
      }
    }
  }
  for (const frame of frames.splice(0)) {
    frame();
  }
}

/* The audio context: nodes that accept every call a source makes, and the count of oscillators started. */
let oscillators = 0;
const stops = [];
function node() {
  const param = { value: 0, setTargetAtTime() {}, setValueAtTime() {}, linearRampToValueAtTime() {}, exponentialRampToValueAtTime() {}, cancelScheduledValues() {} };
  return new Proxy({ gain: param, frequency: param, Q: param, detune: param, connect() {}, disconnect() {}, start() { oscillators++; }, stop: (when) => stops.push(when), copyToChannel() {} }, {
    get: (target, key) => (key in target ? target[key] : param),
    set: (target, key, value) => {
      target[key] = value;
      return true;
    },
  });
}
class FakeAudio {
  constructor() {
    this.sampleRate = 8000;
    this.destination = node();
    this.state = "running";
  }
  get currentTime() {
    return now;
  }
  createGain() {
    return node();
  }
  createDynamicsCompressor() {
    return node();
  }
  createConvolver() {
    return node();
  }
  createOscillator() {
    return node();
  }
  createBiquadFilter() {
    return node();
  }
  createBuffer(channels, length) {
    return { length, copyToChannel() {} };
  }
  createBufferSource() {
    return node();
  }
  async resume() {}
  async close() {}
}
globalThis.AudioContext = FakeAudio;

/* The workspace the transport lives in: a fake toolbar box, canvas, paper, score host and cursor overlay. */
const el = (tag) => document.createElement(tag);
const transportEl = el("div");
const main = el("div");
const canvas = Object.assign(el("div"), { scrollTop: 0, clientHeight: 400, scrollHeight: 800, scrollTo() {} });
const paper = el("div");
const osmdHost = el("div");
const cursors = [];
const overlay = { hideCursor: () => cursors.push(null), setCursor: (layout, position) => (cursors.push(position), { system: 0 }) };
const layout = { systems: [{ index: 0, top: 0, bottom: 100, measures: [{ number: 1, start: 0, end: 4 }, { number: 2, start: 4, end: 8 }] }] };
const playbackData = readJson(path.join(here, "fixtures", "theme.playback.json"));
const announced = [];
const statuses = [];
const state = { volume: 80, reverb: 30, mode: "score" };
const iconButton = (text, label, shortcut, onClick) => {
  const button = el("button");
  button.textContent = text;
  button.setAttribute("aria-label", label);
  button.addEventListener("click", onClick);
  return button;
};
const versions = [];
const host = {
  opts: { loop: { start: 0, end: 4 }, ending: { playback: playbackData, bodyEnd: 4, musicxmlUrl: "ending.musicxml" } },
  compact: false,
  state,
  layoutParts: { transport: ["play", "stop", "loop", "ending", "readout", "tempo", "volume", "sound"] },
  playGroup: ScoreWorkspace.createPlayGroup(),
  canvas,
  paper,
  main,
  osmdHost,
  transportEl,
  iconButton,
  inspectorView: { show() {} },
  announce: (text) => announced.push(text),
  savePrefs() {},
  setStatus: (text) => statuses.push(text),
  svgTopInCanvas: () => 0,
  reducedMotion: () => false,
  selection: () => null,
  overlay: () => overlay,
  osmd: () => ({ load: async () => {} }),
  layout: () => layout,
  timelineOffset: () => 0,
  playbackData: () => playbackData,
  isDestroyed: () => false,
  scoreVersionLoaded: (text) => versions.push(text),
};
globalThis.fetch = async () => ({ ok: true, text: async () => "<ending/>" });
const transport = createTransport(host);
transport.setLoopXml("<loop/>");
const button = (cls) => transportEl.querySelector(`.${cls}`);

/* Before the data arrives nothing plays. */
check(transportEl.querySelectorAll("button").length >= 6 && button("ws-play").disabled === true, "the controls are built and Play waits for the playback data");
check(!transport.hasPlayer() && !transport.canPlay(), "there is no player before the data");

transport.onPlaybackLoaded();
check(transport.hasPlayer() && transport.canPlay() && button("ws-play").disabled === false && button("ws-play").textContent === "Play", "the data makes Play available");
check(button("ws-ending").disabled === false && button("ws-ending").textContent === "Play ending", "an ending makes Play ending available");

/* Play. */
button("ws-play").click();
await new Promise((resolve) => setTimeout(resolve, 20));
check(button("ws-play").textContent === "Pause", "Play starts the sound and the button says Pause");
check(statuses.includes("Piano samples could not load; using basic synth"), "samples that cannot load fall back to the basic synth");
advance(1.5);
check(oscillators > 0, "the notes of the first second are started on the audio context");
check(cursors.some((position) => position !== null && position > 0), "the cursor moves with the music");
check(/^m\d+ 0:0\d \/ 0:\d\d$/.test(button("ws-readout").textContent), "the readout shows the measure and the clock");

/* Pause. */
const stopped = stops.length;
button("ws-play").click();
check(button("ws-play").textContent === "Play" && stops.length > stopped, "Pause stops the sound and the button says Play");

/* Seek. */
const before = oscillators;
transport.seekTo(2);
check(cursors[cursors.length - 1] === 2, "seeking puts the cursor at the position");
check(oscillators === before, "and plays nothing while paused");

/* Loop. */
check(button("ws-loop").getAttribute("aria-pressed") === "true", "the loop starts on when the workspace has one");
button("ws-loop").click();
check(button("ws-loop").getAttribute("aria-pressed") === "false", "Loop toggles it off");

/* Tempo. */
button("ws-tempo-label").click();
check(button("ws-tempo-label").textContent === "100%", "the tempo label starts at 100%");
const buttons = transportEl.querySelector(".ws-tempo").querySelectorAll("button");
buttons[2].click();
check(button("ws-tempo-label").textContent === "105%", "Faster steps the tempo by 5%");
buttons[0].click();
buttons[0].click();
check(button("ws-tempo-label").textContent === "95%", "Slower steps it down");
button("ws-tempo-label").click();
check(button("ws-tempo-label").textContent === "100%", "a click on the label resets it");

/* Ending. */
button("ws-play").click();
await new Promise((resolve) => setTimeout(resolve, 20));
button("ws-ending").click();
await new Promise((resolve) => setTimeout(resolve, 20));
check(button("ws-ending").textContent === "Ending next" && button("ws-ending").disabled === true, "Play ending marks the ending as next");
advance(40);
check(button("ws-play").textContent === "Play" && button("ws-ending").textContent === "Play ending", "the ending takes over, the playback ends and the controls are back");
check(announced.includes("Playback finished"), "the end of the playback is announced");
await new Promise((resolve) => setTimeout(resolve, 10));
check(JSON.stringify(versions) === JSON.stringify(["<ending/>", "<loop/>"]), "the workspace is told when the score on show changes to the ending and back to the loop");

/* Rewind and destroy. */
transport.rewind();
check(cursors[cursors.length - 1] === 0, "Stop goes back to the start");
const open = intervals.size;
transport.destroy();
check(intervals.size < open, "destroying the transport stops its timers");

finish();
