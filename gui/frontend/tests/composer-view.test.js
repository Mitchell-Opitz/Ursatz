/* Confirms the Composer view's logic (composer.js, loaded here directly):
 * the plan tree built from a real plan.json, the request bodies built from
 * the forms and the plan actions against the backend's fields, the summary
 * rows, and the rule that enables the Harmonize toggle. Also checks that the
 * nav entry and the script are in place.
 *
 * Fixture: frontend/tests/fixtures/composer-plan.json is the plan.json the
 * Composer CLI wrote (`plan --form rounded-binary --key C --meter 4/4
 * --bars 16 --seed 3`, then `fill --seed 3`), untouched.
 *
 * Plain Node script, run directly: `node frontend/tests/composer-view.test.js`. */

const fs = require("fs");
const path = require("path");
const composer = require("../public/composer.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

function same(actual, expected, what) {
  check(JSON.stringify(actual) === JSON.stringify(expected), `${what} (got ${JSON.stringify(actual)})`);
}

const plan = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "composer-plan.json"), "utf8"));
const tree = composer.buildPlanTree(plan);

// Plan tree.
check(tree && tree.kind === "form" && tree.label === "rounded-binary", "the tree's root is the form");
same(tree.children.map((n) => n.letter), ["A", "B", "A'"], "the sections are A, B, A' (the material pool is left out)");
same(tree.children.map((n) => n.id), [2, 3, 4], "sections carry their composer ids");
same(tree.children.map((n) => n.bars), ["bars 1-8", "bars 9-12", "bars 13-16"], "sections show their bars");
check(tree.bars === "bars 1-16", "the form shows the bars of its sections only");
check(tree.children.every((n) => n.state === "realized"), "every section is Realized");
check(tree.state === "given", "the form root is Given");
const sectionA = tree.children[0];
same(sectionA.children.map((n) => [n.id, n.kind, n.label]), [[34, "phrase", "phrase"]], "section A has one phrase");
const presentation = sectionA.children[0].children[0];
same(
  presentation.children.map((n) => [n.id, n.label, n.bars, n.state]),
  [[36, "basic idea", "bars 1-2", "realized"], [37, "repetition", "bars 3-4", "realized"]],
  "the presentation's ideas have their bars and states",
);
const sectionB = tree.children[1];
const continuation = sectionB.children[0].children[1];
same(continuation.children.map((n) => n.bars), ["bar 11", "bar 11", "bar 12"], "a fragment inside a bar shows that bar");

// Grow request.
const grow = composer.buildGrowRequest({
  name: "Theme",
  motive: "D4:q F4:e A4:e D5:h",
  into: "sentence",
  key: "D minor",
  meter: "4/4",
  bars: "8",
  cadence: "",
  accompaniment: "alberti",
  check: true,
  seed: "3",
  tempo: "100",
});
same(
  grow.request,
  { name: "Theme", motive: "D4:q F4:e A4:e D5:h", into: "sentence", key: "D minor", meter: "4/4", bars: 8, accompaniment: "alberti", seed: 3, tempo: 100, check: true },
  "the theme form becomes the grow body",
);
same(
  composer.buildGrowRequest({ name: "T", motive: "C4:q", into: "hybrid", hybrid: "cbi+cadential", key: "C major", meter: "6/8", tempo: "250/3" }).request,
  { name: "T", motive: "C4:q", into: "hybrid", key: "C major", meter: "6/8", hybrid: "cbi+cadential", tempo: "250/3" },
  "a hybrid sends its beginning and ending, a fractional tempo stays text",
);
check(composer.buildGrowRequest({ name: "", motive: "C4:q", into: "period", key: "C", meter: "4/4" }).error, "a theme needs a name");
check(composer.buildGrowRequest({ name: "T", motive: "C4:q", into: "period", key: "C", meter: "4/4", bars: "9" }).error, "bars outside 4 to 8 are refused");
check(composer.buildGrowRequest({ name: "T", motive: "C4:q", into: "period", key: "C", meter: "4/4", tempo: "500" }).error, "a tempo above 400 is refused");
check(composer.buildGrowRequest({ name: "T", motive: "C4:q", into: "period", key: "C", meter: "4/4", seed: "-1" }).error, "a negative seed is refused");

// Plan request.
same(
  composer.buildPlanRequest({
    name: "Piece",
    form: "ternary",
    key: "C major",
    meter: "4/4",
    bars: "24",
    count: "",
    modulate: true,
    motive: "",
    given_notes: "C4:q E4:q G4:h",
    given_at: "B",
    cadence: "PAC",
    seed: "3",
  }).request,
  { name: "Piece", form: "ternary", key: "C major", meter: "4/4", bars: 24, modulate: true, given_melody: { notes: "C4:q E4:q G4:h", at: "B" }, seed: 3 },
  "the piece form becomes the plan body (no cadence field, given melody with its place)",
);
check(composer.buildPlanRequest({ name: "P", form: "binary", key: "C", meter: "4/4", given_notes: "C4:q" }).error, "a given melody needs its place");
check(composer.buildPlanRequest({ name: "P", form: "binary", key: "C", meter: "4/4", count: "9" }).error, "a count above 8 is refused");

// Plan action requests.
same(composer.buildLockRequest(2, { node: 3, lane: "" }), { version: 2, node: 3 }, "lock by node, all lanes");
same(composer.buildLockRequest(2, { node: 3, lane: "bass" }), { version: 2, node: 3, lane: "bass" }, "lock one lane");
same(composer.buildRegenerateRequest(1, { node: 4, lane: "" }, "").request, { version: 1, node: 4 }, "a blank seed regenerates with a new seed");
same(composer.buildRegenerateRequest(1, { node: 4, lane: "melody" }, "7").request, { version: 1, node: 4, lane: "melody", seed: 7 }, "regenerate with a seed and a lane");
check(composer.buildRegenerateRequest(1, { node: 4 }, "x").error, "a bad regenerate seed is refused");
same(
  composer.buildVariationsRequest(1, { node: 36, lane: "melody" }, "3", "", false).request,
  { version: 1, node: 36, lane: "melody", count: 3 },
  "variations of a node and lane",
);
same(
  composer.buildVariationsRequest(1, { node: 3, letter: "B", lane: "bass" }, "2", "5", true).request,
  { version: 1, section: "B", harmonize: true, count: 2, seed: 5 },
  "a harmonization names the section and no lane",
);
check(composer.buildVariationsRequest(1, { node: 3, letter: null }, "2", "", true).error, "harmonize needs a section letter");
check(composer.buildVariationsRequest(1, { node: 3 }, "0", "", false).error, "a variations count of 0 is refused");
check(composer.buildVariationsRequest(1, { node: 3 }, "9", "", false).error, "a variations count above 8 is refused");

// Harmonize rule.
check(tree.children.every((section) => !composer.canHarmonize(plan, section)), "no section of the all-Realized plan can be harmonized");
const given = (start, end, state) => ({
  span: { start: { num: start, den: 1 }, end: { num: end, den: 1 } },
  composer: { lane: "melody", state },
});
const mixed = { melody: [given(0, 8, "realized"), given(8, 12, "given"), given(12, 14, "given"), given(14, 16, "realized")] };
check(composer.canHarmonize(mixed, tree.children[1]), "a section whose melody is all Given can be harmonized");
check(!composer.canHarmonize(mixed, tree.children[0]), "a section with Realized melody cannot");
check(!composer.canHarmonize(mixed, tree.children[2]), "a section with Given and Realized melody cannot");
check(!composer.canHarmonize({ melody: [] }, tree.children[1]), "a section without melody spans cannot");
check(!composer.canHarmonize(mixed, { ...tree.children[1], letter: null }), "a section without a form letter cannot");

// Summary and errors.
same(
  composer.buildSummaryRows({ fill: { sections: [{ letter: "A", heard: "heard as sentence", cadences: ["perfect-authentic", "half"] }] } }),
  [{ section: "A", heard: "heard as sentence", cadences: "perfect-authentic, half" }],
  "a planned piece's summary has a row per section",
);
same(
  composer.buildSummaryRows({ into: "sentence", pairing: "presentation + continuation", cadence: "PAC", themes: [{ seed: 3 }] }),
  [{ section: "theme", heard: "sentence, presentation + continuation", cadences: "PAC" }],
  "a grown theme's summary has one row",
);
same(composer.buildSummaryRows(null), [], "no summary, no rows");
check(composer.composerErrorText(422, { error: "composer failed", stderr: "bad notes\n" }) === "composer failed\nbad notes", "an error shows the Composer's stderr");
check(composer.composerErrorText(500, null) === "request failed (500)", "an error without a body still says something");
check(composer.composerVersionLabel({ n: 2, action: "regenerate", seed: 9, created_at: "2026-10-17T01:02:03Z" }) === "v2 · regenerate · seed 9 · 2026-10-17 01:02:03", "a version label has its action, seed and time");

// Wiring.
const appSource = fs.readFileSync(path.join(__dirname, "..", "public", "app.js"), "utf8");
const indexSource = fs.readFileSync(path.join(__dirname, "..", "public", "index.html"), "utf8");
check(/id: "composer", label: "Composer"/.test(appSource), "the nav has a Composer entry");
check(!/Generator/.test(appSource), "the Generator placeholder is gone");
check(indexSource.indexOf("app.js") < indexSource.indexOf("composer.js"), "composer.js loads after app.js");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
