/* Covers the Composer vocabulary client (public/vocabulary.js): loading from the route, reading the lists and limits
 * from the captured fixture (fixtures/vocabulary.json is the route's answer), and the debounced validator with a fake
 * fetch and fake timers. Also checks the model tables that map the Composer's request paths to form fields.
 * Plain Node script: `node frontend/tests/vocabulary.test.js`. */
const V = require("../public/vocabulary.js");
const fixture = require("./fixtures/vocabulary.json");

let failures = 0;
function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

async function main() {
  // before it is loaded
  let threw = false;
  try {
    V.list("meters");
  } catch (err) {
    threw = true;
  }
  check(!V.loaded() && threw, "reading the lists before the vocabulary is loaded is an error");

  // load
  const calls = [];
  const loaded = await V.load(async (url) => {
    calls.push(url);
    return { status: 200, json: async () => (url === "/api/composer/forms" ? { forms: [] } : fixture) };
  });
  check(loaded === fixture && V.loaded() && same(calls, ["/api/composer/vocabulary", "/api/composer/forms"]), "load fetches the routes once and keeps the answers");
  const forms = require("./fixtures/forms.json");
  V.set(fixture, forms);
  check(V.forms().length === forms.forms.length && V.form("game-loop").label === "Game loop" && V.form("rounded-binary").sections === "sizes" && V.form("nothing") === null,
        "the forms are the Composer's, each by name");
  check(same(V.forms().map((form) => form.name), ["binary", "rounded-binary", "ternary", "variations", "loop", "layered", "game-loop"]), "the forms come in the Composer's order");
  await V.load(async (url) => ({ status: 200, json: async () => (url === "/api/composer/forms" ? { forms: forms.forms.slice(0, 2) } : fixture) }));
  check(V.forms().length === 2, "load keeps the forms the route gave");
  let formsFailed = false;
  try {
    await V.load(async (url) => ({ status: url === "/api/composer/forms" ? 500 : 200, json: async () => fixture }));
  } catch (err) {
    formsFailed = /forms are unavailable/.test(err.message);
  }
  check(formsFailed && V.forms().length === 2, "unavailable forms are an error and keep what was loaded");
  let failed = false;
  try {
    await V.load(async () => ({ status: 500, json: async () => ({}) }));
  } catch (err) {
    failed = /unavailable/.test(err.message);
  }
  check(failed && V.loaded(), "an unavailable vocabulary is an error and keeps what was loaded");

  // lists and limits
  check(same(V.values("meters"), ["2/4", "3/4", "4/4", "6/8"]), "the meters are the Composer's four");
  check(V.labelOf("piece_modes", "style") === "From the brief" && V.labelOf("meters", "9/8") === "9/8", "labels come from the list, else the value");
  check(V.entry("meters", "6/8").beats === 6 && V.entry("meters", "6/8").second_chord_beat === 3 && V.entry("meters", "2/4").second_chord_beat === undefined,
        "a meter says its beats and where a second chord starts");
  check(same(V.values("keys").slice(0, 3), ["C major", "Db major", "D major"]) && V.entry("pitch_class_names", 3).minor === "D#" && V.entry("pitch_class_names", 3).major === "Eb",
        "keys are spelled the Composer's way");
  check(V.limit("tempo_min") === 20 && V.limit("tempo_max") === 400 && V.limit("nothing", 7) === 7, "limits are numbers, with a fallback");
  check(V.seedMax("theme") === 4294967295 && V.seedMax("variants") === 4294967231, "the seed limits are given for each request");
  check(same(V.list("nothing"), []), "an unknown list is empty");

  // the models read from it
  const T = require("../public/themes-model.js");
  const P = require("../public/pieces-model.js");
  const M = require("../public/materials-model.js");
  check(T.fieldForPath("/meter") === "meter" && T.fieldForPath("/first_cadence") === "firstCadence" && T.fieldForPath("/cadence") === "cadence" &&
          T.fieldForPath("/harmony") === "chords" && T.fieldForPath("/motives/0/degrees") === "slots" && T.fieldForPath("/nothing") === null,
        "theme request paths name the form fields");
  const themeRequest = T.validateRequest(T.emptyState("t"));
  check(themeRequest.method === "POST" && themeRequest.url === "/api/materials/items/validate" && JSON.parse(themeRequest.body).type === "theme" &&
          JSON.parse(themeRequest.body).body.theme.form.meter === "4/4",
        "the theme form asks the validate route about its body");
  const pieceRequest = P.validateRequest(P.emptyState("p"));
  check(JSON.parse(pieceRequest.body).type === "piece" && Array.isArray(JSON.parse(pieceRequest.body).body.piece.structure.sections),
        "the piece form asks the validate route about its body");
  check(M.fieldForPath("/input/text") === "notes" && M.fieldForPath("/harmony") === "harmony" && M.fieldForPath("/features/1") === "features" &&
          M.fieldForPath("/meter") === "meter" && M.fieldForPath("/tones/2") === "notes",
        "motif request paths name the editor fields");
  check(M.TONE_ROLE_OPTIONS[0].value === "" && M.TONE_ROLE_OPTIONS[0].label === "" && M.TONE_ROLE_OPTIONS.length === 1 + fixture.tone_labels.length,
        "the tone choices are the Composer's with an empty one for no override");
  check(M.toneRoleLabel("chord") === "CT" && M.toneRoleLabel("changing-tone") === "CT2" && M.toneRoleLabel("") === null, "tone abbreviations are the Composer's");
  const motif = M.validateRequest({ role: "melodic", form: "full", notes: "C4:q D4:q", meter: "4/4", mode: "minor", pickup: "", harmony: "", features: [], toneOverrides: {} }, null);
  check(JSON.parse(motif.body).type === "motif" && JSON.parse(motif.body).body.motif.key === "C minor", "the motif editor asks the validate route about its body");
  // a GUI spelling of a key is kept as stored in the body
  check(T.formFromBody({ uses: [], theme: { form: { into: "period", meter: "6/8", bars: 4, cadence: "HC" }, slots: [] } }, "x").meter === "6/8" &&
          P.stateFromBody({ uses: [], piece: { structure: { key: "C# major", mode: "major", meter: "4/4", sections: [] } } }, "x").key === "C# major",
        "an item saved with a GUI key spelling opens as stored");

  // the validator
  const timers = [];
  const timer = {
    setTimer: (fn, ms) => {
      timers.push({ fn, ms });
      return timers.length - 1;
    },
    clearTimer: (id) => {
      timers[id] = null;
    },
  };
  const shown = [];
  let reply = [200, { ok: true, complete: true, request: {} }];
  const sent = [];
  const validator = V.createValidator({
    ...timer,
    build: () => ({ method: "POST", url: "/api/materials/items/validate", headers: {}, body: "{}" }),
    field: (path) => (path === "/meter" ? "meter" : null),
    show: (result) => shown.push(result),
    fetch: async (url, init) => {
      sent.push({ url, method: init.method });
      const [status, body] = reply;
      return { status, json: async () => body };
    },
  });
  validator.check();
  validator.check();
  validator.check();
  check(timers.filter(Boolean).length === 1 && timers.filter(Boolean)[0].ms === 400, "several checks wait 400 ms and make one call");
  timers.filter(Boolean)[0].fn();
  await new Promise((resolve) => setImmediate(resolve));
  check(sent.length === 1 && shown.length === 1 && shown[0].ok && shown[0].complete, "a complete body shows complete");
  reply = [200, { ok: true, complete: false, message: "the theme needs a basic slot" }];
  await validator.now();
  check(shown[1].ok && !shown[1].complete && shown[1].message === "the theme needs a basic slot", "an incomplete body shows no refusal");
  reply = [400, { ok: false, error: "meter '5/4' is not supported", path: "/meter" }];
  await validator.now();
  check(!shown[2].ok && shown[2].complete && shown[2].message === "meter '5/4' is not supported" && shown[2].field === "meter" && shown[2].path === "/meter",
        "a refusal shows the message as given and the field its path names");
  reply = [500, { ok: false, error: "out of memory" }];
  await validator.now();
  check(shown[3].ok && !shown[3].complete, "a failure of the backend is no refusal of the form");

  // an answer a later check has passed is dropped
  let release;
  const slow = V.createValidator({
    ...timer,
    build: () => ({ method: "POST", url: "/x", headers: {}, body: "{}" }),
    show: (result) => shown.push(result),
    fetch: () => new Promise((resolve) => (release = resolve)),
  });
  const before = shown.length;
  const first = slow.now();
  slow.cancel();
  release({ status: 200, json: async () => ({ ok: true, complete: true }) });
  await first;
  check(shown.length === before, "a cancelled check shows nothing");

  const empty = V.createValidator({ ...timer, build: () => null, show: (result) => shown.push(result), fetch: async () => { throw new Error("no call"); } });
  await empty.now();
  check(!shown[shown.length - 1].complete && shown[shown.length - 1].ok, "nothing to check shows nothing");

  if (failures > 0) {
    console.error(`${failures} check(s) failed`);
    process.exit(1);
  }
}

main();
