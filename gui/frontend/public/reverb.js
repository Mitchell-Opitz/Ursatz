/* The reverb for playback: a generated stereo impulse response for a
 * ConvolverNode (no impulse file is shipped) and the dry and wet gains for a
 * Reverb amount.
 *
 * Pure, no audio access, so Node tests load this file directly. */
(function (root) {
  const FADE_IN_SECONDS = 0.01;
  const MAX_WET_SHARE = 0.6;

  /* [left, right] Float32Arrays of round(sampleRate * seconds) samples:
   * independent noise in -1 to 1 from random(), scaled by
   * (1 - n / length) ** decay, with the first 10 ms faded in linearly so the
   * tail starts without a click. */
  function buildImpulse(sampleRate, seconds, decay, random) {
    const length = Math.round(sampleRate * seconds);
    const fade = Math.max(1, Math.round(sampleRate * FADE_IN_SECONDS));
    const channels = [new Float32Array(length), new Float32Array(length)];
    for (const channel of channels) {
      for (let n = 0; n < length; n++) {
        const noise = random() * 2 - 1;
        const envelope = Math.pow(1 - n / length, decay);
        const ramp = n < fade ? n / fade : 1;
        channel[n] = noise * envelope * ramp;
      }
    }
    return channels;
  }

  /* An equal-power crossfade for an amount of 0 to 100: even 100 keeps the
   * dry signal audible. */
  function reverbGains(amount) {
    const clamped = Math.min(100, Math.max(0, Number(amount) || 0));
    const a = (clamped / 100) * MAX_WET_SHARE;
    return { dry: Math.cos((a * Math.PI) / 2), wet: Math.sin((a * Math.PI) / 2) };
  }

  const api = { buildImpulse, reverbGains };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.Reverb = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
