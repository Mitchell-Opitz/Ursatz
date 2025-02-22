/* Confirms the reverb helpers (reverb.js): the impulse has two channels of
 * the right length, fades in from 0 and decays, and the gains are an
 * equal-power crossfade that keeps the dry signal audible. Plain Node script,
 * no dependencies. Run directly: `node frontend/tests/reverb.test.js`. */

const Reverb = require("../public/reverb.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* A seeded random so the envelope check is repeatable. */
function seeded(seed) {
  let state = seed;
  return () => {
    state = (state * 1664525 + 1013904223) % 4294967296;
    return state / 4294967296;
  };
}

const sampleRate = 8000;
const impulse = Reverb.buildImpulse(sampleRate, 2.2, 3, seeded(7));
check(impulse.length === 2, "the impulse has two channels");
check(impulse[0].length === Math.round(sampleRate * 2.2) && impulse[1].length === impulse[0].length, "each channel is sampleRate * seconds long");
check(impulse[0] instanceof Float32Array, "channels are Float32Arrays");
check(impulse[0].some((value, n) => value !== impulse[1][n]), "the two channels are independent noise");
check(impulse[0][0] === 0 && impulse[1][0] === 0, "the fade-in starts at 0");
check(Math.abs(impulse[0][Math.round(sampleRate * 0.005)]) <= 0.5, "the first 10 ms stay below full level");
check(impulse[0].every((value) => value >= -1 && value <= 1), "samples stay within -1 to 1");

function meanAbs(channel, from, to) {
  let sum = 0;
  for (let n = from; n < to; n++) sum += Math.abs(channel[n]);
  return sum / (to - from);
}
const tenth = Math.floor(impulse[0].length / 10);
check(
  meanAbs(impulse[0], impulse[0].length - tenth, impulse[0].length) < meanAbs(impulse[0], 0, tenth),
  "the envelope decays: the last tenth is quieter than the first",
);

const off = Reverb.reverbGains(0);
check(off.dry === 1 && off.wet === 0, "amount 0 is fully dry");
for (const amount of [0, 20, 100]) {
  const { dry, wet } = Reverb.reverbGains(amount);
  check(Math.abs(dry * dry + wet * wet - 1) < 1e-9, `amount ${amount}: dry squared plus wet squared equals 1`);
  if (amount <= 20) {
    check(wet <= dry, `amount ${amount}: wet is not above dry`);
  }
}
check(Reverb.reverbGains(100).dry > 0.5 && Reverb.reverbGains(100).dry < 1, "amount 100 keeps the dry signal audible");
check(Reverb.reverbGains(-5).wet === 0 && Reverb.reverbGains(500).wet === Reverb.reverbGains(100).wet, "amounts outside 0 to 100 are clamped");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
