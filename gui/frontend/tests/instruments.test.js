/* Confirms the instrument registry (instruments.js): the fallback for
 * unmapped programs, a mapped program, a part override winning over the
 * program, and registering a second source. Plain Node script, no
 * dependencies. Run directly: `node frontend/tests/instruments.test.js`. */

const Instruments = require("../public/instruments.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

function source(id) {
  return { id, label: id, credit: "", load: () => Promise.resolve(), start: () => ({ stop() {} }) };
}

const registry = Instruments.createInstrumentRegistry({ fallback: "piano" });
registry.register(source("piano"));
check(registry.sources().length === 1, "a registered source is listed");
check(registry.sourceForProgram(0).id === "piano" && registry.sourceForProgram(73).id === "piano", "unmapped programs use the fallback");

registry.register(source("flute"));
check(registry.sources().map((s) => s.id).join() === "piano,flute", "a second source is listed after the first");
check(registry.sourceForProgram(73).id === "piano", "registering a source alone changes no program");
registry.mapProgram(73, "flute");
check(registry.sourceForProgram(73).id === "flute" && registry.sourceForProgram(0).id === "piano", "a mapped program uses its source");
check(registry.sourceForPart(2, 73).id === "flute", "a part with no override uses its program's source");

registry.setPartSource(2, "piano");
check(registry.sourceForPart(2, 73).id === "piano", "a part override wins over the program");
check(registry.sourceForPart(3, 73).id === "flute", "other parts are not affected by the override");
registry.setPartSource(2, null);
check(registry.sourceForPart(2, 73).id === "flute", "clearing an override goes back to the program");

let threw = false;
try {
  registry.mapProgram(5, "missing");
} catch (err) {
  threw = true;
}
check(threw, "mapping a program to an unknown source throws");
threw = false;
try {
  registry.register({ id: "broken" });
} catch (err) {
  threw = true;
}
check(threw, "a source without load and start is rejected");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
