/* Covers the sound sources (public/score/sound-sources.js) with a fake MIDI soundfont global and a recording audio context: the key and the
 * sample plan of a pitch, the registry with the piano as the fallback, the piano loading only the keys a piece uses, a piano note starting a
 * sample at the right rate, a missing sample skipping the note, the basic synth starting two oscillators and a stop silencing a voice.
 * Plain Node script, run through ctest. */
import Instruments from "../public/score/instruments.js";
import SoundSources from "../public/score/sound-sources.js";
import { check, near, same, finish } from "./support/check.js";

/* An audio context that records what the sources ask of it. */
function recorder() {
  const log = [];
  const param = (name) => ({
    set value(v) {
      log.push([name, "value", v]);
    },
    setValueAtTime: (v, t) => log.push([name, "set", v, t]),
    linearRampToValueAtTime: (v, t) => log.push([name, "ramp", v, t]),
    cancelScheduledValues: (t) => log.push([name, "cancel", t]),
    setTargetAtTime: (v, t) => log.push([name, "target", v, t]),
  });
  const node = (kind) => {
    const n = { kind, gain: param(`${kind}.gain`), frequency: param(`${kind}.freq`), playbackRate: param(`${kind}.rate`), connect: (to) => log.push([kind, "connect", to.kind || "destination"]), disconnect() {}, start: (t) => log.push([kind, "start", t]), stop: (t) => log.push([kind, "stop", t]) };
    return n;
  };
  return {
    log,
    currentTime: 1,
    createGain: () => node("gain"),
    createOscillator: () => node("osc"),
    createBufferSource: () => node("sample"),
    decodeAudioData: async () => ({ duration: 2 }),
  };
}

check(SoundSources.keyName(60) === "C4" && SoundSources.keyName(61) === "Db4" && SoundSources.keyName(21) === "A0", "a pitch has the soundfont's key name, with flats");
check(same(SoundSources.pitchPlan(60), { key: "C4", rate: 1 }) && SoundSources.pitchPlan(110).key === "C8" && near(SoundSources.pitchPlan(110).rate, Math.pow(2, 2 / 12)), "a pitch above the samples uses the top one, shifted");
check(SoundSources.pitchPlan(10).key === "A0" && SoundSources.pitchPlan(10).rate < 1, "a pitch below uses the lowest one, shifted down");

const registry = SoundSources.createDefaultRegistry(Instruments);
check(registry.sources().map((s) => s.id).join() === "gm-piano,basic-synth", "the registry offers the piano and the basic synth");
check(registry.sourceForProgram(40).id === "gm-piano", "a program with no source plays the piano");
registry.setPartSource(0, "basic-synth");
check(registry.sourceForPart(0, 0).id === "basic-synth" && registry.sourceForPart(1, 0).id === "gm-piano", "a part can be given the basic synth");

/* The piano: the soundfont is a global that holds a data url per key; only the keys of the pitches given are fetched and decoded. */
const fetched = [];
globalThis.MIDI = { Soundfont: { acoustic_grand_piano: { C4: "data:c4", D4: "data:d4", E4: "data:e4" } } };
globalThis.fetch = async (url) => {
  fetched.push(url);
  return { arrayBuffer: async () => new ArrayBuffer(4) };
};
const ctx = recorder();
await SoundSources.gmPiano.load(ctx, { pitches: [60, 62, 60] });
check(same(fetched.sort(), ["data:c4", "data:d4"]), "the piano fetches each key the piece uses once");
const voice = SoundSources.gmPiano.start(ctx, { kind: "bus" }, { pitch: 60, velocity: 0.7, when: 2, duration: 1 });
check(ctx.log.some((row) => row[0] === "sample" && row[1] === "start" && row[2] === 2), "a loaded key plays its sample at the time asked");
check(ctx.log.some((row) => row[0] === "sample.rate" && row[2] === 1), "at the sample's own rate");
ctx.log.length = 0;
voice.stop(3);
check(ctx.log.some((row) => row[0] === "gain.gain" && row[1] === "target" && row[2] === 0), "stopping a voice fades its gain to nothing");
ctx.log.length = 0;
SoundSources.gmPiano.start(ctx, { kind: "bus" }, { pitch: 80, velocity: 0.7, when: 2, duration: 1 });
check(!ctx.log.some((row) => row[1] === "start"), "a key that is not decoded yet is skipped (it is decoded for next time)");
await new Promise((resolve) => setTimeout(resolve, 5));
check(fetched.includes("data:c4") && globalThis.MIDI.Soundfont.acoustic_grand_piano.Ab5 === undefined, "a pitch with no sample in the font stays silent without a failure");

/* The basic synth needs no files. */
await SoundSources.basicSynth.load(ctx);
const synthCtx = recorder();
const synth = SoundSources.basicSynth.start(synthCtx, { kind: "bus" }, { pitch: 69, velocity: 0.5, when: 0.5, duration: 0.2 });
const starts = synthCtx.log.filter((row) => row[0] === "osc" && row[1] === "start");
check(starts.length === 2 && starts.every((row) => row[2] === 1), "the synth starts two oscillators, at the current time when the note is already late");
check(synthCtx.log.some((row) => row[0] === "osc.freq" && row[1] === "value" && near(row[2], 440)), "the pitch 69 is 440 Hz");
synthCtx.log.length = 0;
synth.stop(2);
check(synthCtx.log.filter((row) => row[0] === "osc" && row[1] === "stop").length === 2, "stopping the synth stops both oscillators");

finish();
