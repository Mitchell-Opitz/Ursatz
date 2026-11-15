/* The built-in sound sources for the instrument registry (instruments.js):
 * "gm-piano" plays samples from the vendored FluidR3 GM soundfont and
 * "basic-synth" is an oscillator voice that needs no files. Both follow the
 * source contract {id, label, credit, load(ctx, {pitches}), start(ctx,
 * destination, {pitch, velocity, when, duration}) -> {stop(when)}}.
 *
 * Needs Web Audio and a document; keyName and pitchPlan are pure. */
(function (root) {
  const SOUNDFONT_URL = "vendor/soundfonts/fluidr3-gm/acoustic_grand_piano-mp3.js";
  const LOWEST_PITCH = 21; /* A0 */
  const HIGHEST_PITCH = 108; /* C8 */
  const NOTE_NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];

  /* MIDI pitch to the soundfont's key name, with flats: 60 is C4, 61 is Db4. */
  function keyName(pitch) {
    return NOTE_NAMES[((pitch % 12) + 12) % 12] + (Math.floor(pitch / 12) - 1);
  }

  /* The sample to play for a pitch and how much to shift it: pitches outside
   * A0 to C8 use the nearest sample with playbackRate shifted. */
  function pitchPlan(pitch) {
    const samplePitch = Math.min(HIGHEST_PITCH, Math.max(LOWEST_PITCH, Math.round(pitch)));
    return { key: keyName(samplePitch), rate: Math.pow(2, (pitch - samplePitch) / 12) };
  }

  /* ---- gm-piano ---- */

  const PIANO_ATTACK = 0.003;
  const PIANO_RELEASE = 0.25;
  const buffers = new Map(); /* key name -> decoded AudioBuffer */
  const decoding = new Map(); /* key name -> pending promise */
  let soundfontPromise = null;

  function loadSoundfont() {
    if (soundfontPromise) {
      return soundfontPromise;
    }
    soundfontPromise = new Promise((resolve, reject) => {
      const present = () => root.MIDI && root.MIDI.Soundfont && root.MIDI.Soundfont.acoustic_grand_piano;
      if (present()) {
        resolve(present());
        return;
      }
      const tag = document.createElement("script");
      tag.src = SOUNDFONT_URL;
      tag.async = true;
      tag.onload = () => (present() ? resolve(present()) : reject(new Error("The soundfont file is empty")));
      tag.onerror = () => reject(new Error("The soundfont file could not be loaded"));
      document.head.appendChild(tag);
    });
    soundfontPromise.catch(() => {
      soundfontPromise = null; /* allow a retry on the next Play */
    });
    return soundfontPromise;
  }

  function decodeKey(ctx, key) {
    if (buffers.has(key)) {
      return Promise.resolve(buffers.get(key));
    }
    if (decoding.has(key)) {
      return decoding.get(key);
    }
    const pending = loadSoundfont()
      .then((font) => {
        if (!font[key]) {
          throw new Error(`No sample for ${key}`);
        }
        return fetch(font[key]);
      })
      .then((response) => response.arrayBuffer())
      .then((bytes) => ctx.decodeAudioData(bytes))
      .then((buffer) => {
        buffers.set(key, buffer);
        decoding.delete(key);
        return buffer;
      });
    pending.catch(() => decoding.delete(key));
    decoding.set(key, pending);
    return pending;
  }

  const gmPiano = {
    id: "gm-piano",
    label: "Acoustic grand piano",
    credit: "FluidR3_GM by Frank Wen, rendered by Benjamin Gleitzman (midi-js-soundfonts), CC BY 3.0",
    /* Loads the soundfont script, then decodes only the pitches the piece
     * uses; any other pitch is decoded on demand. */
    load(ctx, hints) {
      const pitches = (hints && hints.pitches) || [];
      const keys = Array.from(new Set(pitches.map((pitch) => pitchPlan(pitch).key)));
      return loadSoundfont().then(() => Promise.all(keys.map((key) => decodeKey(ctx, key)))).then(() => undefined);
    },
    start(ctx, destination, note) {
      const plan = pitchPlan(note.pitch);
      const buffer = buffers.get(plan.key);
      if (!buffer) {
        /* Not decoded yet: decode for next time and skip this note. */
        decodeKey(ctx, plan.key).catch(() => {});
        return { stop() {} };
      }
      const when = Math.max(note.when, ctx.currentTime);
      const velocity = note.velocity;
      const release = when + Math.max(note.duration, PIANO_ATTACK);
      const source = ctx.createBufferSource();
      source.buffer = buffer;
      source.playbackRate.value = plan.rate;
      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0, when);
      gain.gain.linearRampToValueAtTime(velocity, when + PIANO_ATTACK);
      gain.gain.setValueAtTime(velocity, release);
      gain.gain.linearRampToValueAtTime(0, release + PIANO_RELEASE);
      source.connect(gain);
      gain.connect(destination);
      source.start(when);
      source.stop(release + PIANO_RELEASE + 0.05);
      source.onended = () => gain.disconnect();
      return {
        stop(at) {
          const t = Math.max(at, ctx.currentTime);
          gain.gain.cancelScheduledValues(t);
          gain.gain.setTargetAtTime(0, t, 0.05);
          try {
            source.stop(t + 0.4);
          } catch (err) {
            /* already stopped */
          }
        },
      };
    },
  };

  /* ---- basic-synth ---- */

  const SYNTH_ATTACK = 0.005;
  const SYNTH_DECAY = 0.3;
  const SYNTH_SUSTAIN = 0.3;
  const SYNTH_RELEASE = 0.2;
  const SYNTH_OCTAVE_GAIN = 0.2;

  const basicSynth = {
    id: "basic-synth",
    label: "Basic synth",
    credit: "",
    load() {
      return Promise.resolve();
    },
    start(ctx, destination, note) {
      const when = Math.max(note.when, ctx.currentTime);
      const velocity = note.velocity;
      const release = when + Math.max(note.duration, SYNTH_ATTACK);
      const frequency = 440 * Math.pow(2, (note.pitch - 69) / 12);
      const envelope = ctx.createGain();
      const body = ctx.createOscillator();
      body.type = "triangle";
      body.frequency.value = frequency;
      const octave = ctx.createOscillator();
      octave.type = "sine";
      octave.frequency.value = frequency * 2;
      const octaveGain = ctx.createGain();
      octaveGain.gain.value = SYNTH_OCTAVE_GAIN;
      body.connect(envelope);
      octave.connect(octaveGain);
      octaveGain.connect(envelope);
      envelope.connect(destination);

      const sustain = velocity * SYNTH_SUSTAIN;
      const decayEnd = when + SYNTH_ATTACK + SYNTH_DECAY;
      envelope.gain.setValueAtTime(0, when);
      envelope.gain.linearRampToValueAtTime(velocity, when + SYNTH_ATTACK);
      let level = sustain;
      if (release >= decayEnd) {
        envelope.gain.linearRampToValueAtTime(sustain, decayEnd);
      } else {
        const progress = Math.max(0, (release - when - SYNTH_ATTACK) / SYNTH_DECAY);
        level = velocity - (velocity - sustain) * progress;
        envelope.gain.linearRampToValueAtTime(level, release);
      }
      envelope.gain.setValueAtTime(level, release);
      envelope.gain.linearRampToValueAtTime(0, release + SYNTH_RELEASE);
      body.start(when);
      octave.start(when);
      body.stop(release + SYNTH_RELEASE + 0.05);
      octave.stop(release + SYNTH_RELEASE + 0.05);
      body.onended = () => envelope.disconnect();
      return {
        stop(at) {
          const t = Math.max(at, ctx.currentTime);
          envelope.gain.cancelScheduledValues(t);
          envelope.gain.setTargetAtTime(0, t, 0.05);
          try {
            body.stop(t + 0.4);
            octave.stop(t + 0.4);
          } catch (err) {
            /* already stopped */
          }
        },
      };
    },
  };

  const api = {
    keyName,
    pitchPlan,
    gmPiano,
    basicSynth,
    sources: () => [gmPiano, basicSynth],
    /* Registers both sources and falls back to the piano. */
    createDefaultRegistry(Instruments) {
      const registry = Instruments.createInstrumentRegistry({ fallback: gmPiano.id });
      registry.register(gmPiano);
      registry.register(basicSynth);
      return registry;
    },
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.SoundSources = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
