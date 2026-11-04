/* Covers the pure variant tree helpers (public/variant-tree.js): tree rows
 * with depth, flags and the rejected filter, the operation groups, chain
 * steps from the picker, generate and reroll requests, candidate naming and
 * candidate bodies. Plain Node script: `node frontend/tests/variant-tree.test.js`. */
const V = require("../public/variant-tree.js");

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

const node = (id, flag, children, extra) => ({
  id,
  name: `Motif ${id}`,
  current_version: 2,
  derived: id === "root" ? null : { kind: "motive-form", flag, kept: ["rhythm"], changed: ["intervals"], operations: [{ op: "invert" }, { op: "augment" }], seed: 7 },
  children: children || [],
  ...(extra || {}),
});
const tree = node("root", null, [
  node("a", "approved", [node("a1", "unflagged")]),
  node("b", "rejected", [node("b1", "approved")]),
]);

let view = V.treeRows(tree, false);
check(same(view.rows.map((r) => r.id), ["root", "a", "a1"]), "rejected variants and their subtree are hidden");
check(view.hidden === 2, "hidden counts the rejected variant and what is under it");
check(same(view.rows.map((r) => r.depth), [0, 1, 2]), "rows carry their depth");
view = V.treeRows(tree, true);
check(same(view.rows.map((r) => r.id), ["root", "a", "a1", "b", "b1"]) && view.hidden === 0, "show rejected lists every node depth first");
check(view.rows[0].isRoot && view.rows[0].kind === null && view.rows[0].chips.length === 0, "the root has no kind or chips");
check(view.rows[1].flag.text === "Approved" && view.rows[1].kind.text === "Motive-form", "flag and kind badges");
check(view.rows[1].derivation === "invert then augment", "the derivation joins the steps");
check(same(view.rows[1].chips.map((c) => c.text), ["kept rhythm", "changed intervals"]), "kept and changed chips");
check(V.treeRows(null, true).rows.length === 0, "no tree gives no rows");

/* Promoted motifs (R-188): a standalone node and what is under it are not rows; each one reached is listed. */
const promotedTree = node("root", null, [
  node("a", "approved", [node("a1", "unflagged", [], { standalone: true }), node("a2", "unflagged")]),
  node("p", "unflagged", [node("p1", "approved")], { standalone: true }),
  node("r", "rejected", [], { standalone: true }),
]);
const split = V.treeRows(promotedTree, false);
check(same(split.rows.map((r) => r.id), ["root", "a", "a2"]), "a promoted node and what is under it are not saved rows");
check(same(split.promoted.map((e) => e.id), ["a1", "p", "r"]), "each promoted node is listed once, depth first, rejected or not");
check(split.promoted[1].name === "Motif p" && split.promoted[1].derivation === "invert then augment", "a promoted entry has its name and derivation");
check(split.hidden === 0, "promoted nodes are not counted as hidden");
check(V.promotedText(split.promoted[0]) === "Motif a1 (opens it)", "a promoted entry reads <name> (opens it)");
check(V.treeRows(tree, true).promoted.length === 0 && V.treeRows(null, true).promoted.length === 0, "no standalone node gives an empty Promoted list");
const promoteRequest = V.promoteRequest("v1", "  Own  ", "prj-1");
check(promoteRequest.method === "POST" && promoteRequest.url === "/api/materials/items/v1/promote" &&
  same(JSON.parse(promoteRequest.body), { name: "Own", project_id: "prj-1" }), "the promote request names the motif and its project");

const operations = [
  { name: "augment", label: "Augment", category: "rhythm", citation: "ch. III", params: [{ name: "ratio", kind: "ratio", min: 0, max: 0 }], forms: ["full"] },
  { name: "transpose", label: "Transpose", category: "melody adaptation", citation: "ch. III", params: [{ name: "steps", kind: "int", min: -7, max: 7 }], forms: [] },
  { name: "exact", label: "Exact", category: "exact", citation: "ch. III, Ex. 14", params: [], forms: [] },
  { name: "invert", label: "Invert", category: "intervals", citation: "ch. III", params: [{ name: "axis", kind: "pitch", min: 0, max: 127 }, { name: "tonal", kind: "bool", min: 0, max: 1 }], forms: [] },
];
const groups = V.operationGroups(operations);
check(same(groups.map((g) => g.category), ["exact", "rhythm", "intervals", "melody adaptation"]), "groups follow Schoenberg's order");
check(groups[3].title === "Melody adaptation" && groups[0].operations[0].citation === "ch. III, Ex. 14", "titles and citations");

let made = V.stepFromValues(operations[0], { ratio: "2:1" });
check(same(made, { step: { op: "augment", params: { ratio: "2:1" } } }), "a ratio step");
made = V.stepFromValues(operations[3], { axis: "", tonal: true });
check(same(made, { step: { op: "invert", params: { tonal: true } } }), "an empty text leaves the parameter open");
check(same(V.stepFromValues(operations[2], {}), { step: { op: "exact" } }), "a step without parameters");
check(V.stepFromValues(operations[1], { steps: "9" }).error !== undefined, "a whole number outside its range is refused");
check(V.stepFromValues(operations[1], { steps: "x" }).error !== undefined, "text for a whole number is refused");
check(V.stepFromValues(operations[0], { ratio: "2/1" }).error !== undefined, "a bad ratio is refused");
check(V.chainFromSteps([]) === null, "no steps gives no chain");

let request = V.generateRequest("m1", { count: "4", seed: "12", steps: [{ op: "invert" }] });
check(request.request.method === "POST" && request.request.url === "/api/materials/items/m1/variants", "generate posts to the variants route");
check(same(JSON.parse(request.request.body), { count: 4, seed: 12, chain: { steps: [{ op: "invert" }] } }), "generate body");
check(same(JSON.parse(V.generateRequest("m1", {}).request.body), {}), "blank controls send nothing, so the route draws random chains");
check(V.generateRequest("m1", { count: "17" }).error !== undefined, "count above 16 is refused");
check(V.generateRequest("m1", { count: "0" }).error !== undefined, "count 0 is refused");
check(V.generateRequest("m1", { seed: "-1" }).error !== undefined, "a negative seed is refused");

const candidate = {
  seed: 40,
  chain: { steps: [{ op: "transpose", params: { steps: 2 }, open: ["steps"] }, { op: "augment", params: { ratio: "2:1" }, open: [] }] },
  kind: "motive-form",
  kept: ["rhythm"],
  changed: ["intervals"],
  derivation: "Transposed then augmented. Kept rhythm; changed intervals.",
  recognizability: 0.6234,
  motive: { analysis: { degrees_text: "1:q 3:q", rhythm_text: "x:q x:q", contour_text: "0 1" } },
};
check(same(V.fixedChain(candidate), { steps: [{ op: "transpose" }, { op: "augment", params: { ratio: "2:1" } }] }), "a reroll chain keeps the fixed parameters and opens the drawn ones");
const reroll = V.candidateRerollRequest("m1", candidate);
check(same(JSON.parse(reroll.body), { chain: V.fixedChain(candidate), count: 1, seed: 41 }), "a candidate reroll asks for one variant with the next seed");
check(V.candidateName("Opening", candidate) === "Opening transpose + augment", "candidate naming");
check(V.candidateName("Opening", { chain: { steps: [] } }) === "Opening", "no operations keeps the name");

check(same(JSON.parse(V.saveRequest("m1", candidate, "approved", "  My name ").body), { variant: candidate, flag: "approved", name: "My name" }), "save body");
check(JSON.parse(V.saveRequest("m1", candidate, "bogus").body).flag === "unflagged", "an unknown flag saves as unflagged");
check(same(JSON.parse(V.flagRequest("v1", "rejected", "variant").body), { flag: "rejected", kind: "variant" }), "flag body with a kind");
check(V.rerollRequest("v1").url === "/api/materials/items/v1/reroll", "reroll url");
check(V.rerollRequest("v1").job === true && V.candidateRerollRequest("m1", candidate).job === true, "a reroll is a job");
check(V.generateRequest("m1", {}).request.job === true && V.eachOpRequest("m1", "7").request.job === true, "variants and each-op are jobs");
check(V.treeRequest("v1").method === "GET" && V.treeRequest("v1").url === "/api/materials/items/v1/variant-tree", "tree url");

const cv = V.candidateView(candidate);
check(cv.recognizability === "62% recognizable" && cv.kind.text === "Motive-form" && cv.chips.length === 2, "candidate view");
check(V.candidateDegrees(candidate, "full") === "1:q 3:q" && V.candidateDegrees(candidate, "rhythm") === "x:q x:q" && V.candidateDegrees(candidate, "contour") === "0 1", "degrees by form");
const body = V.candidateBody({ motif: { form: "rhythm", degrees: "x:h", key: "C major" } }, candidate);
check(body.motif.degrees === "x:q x:q" && body.motif.key === "C major", "a candidate body keeps the source and swaps the degrees");

const rich = { ...candidate, functions: ["beginning", "bogus", "cadential"], remoteness: 0.3333, harmony: "I V@2", motive: { motive: { features: [{ label: "a", first: 0, last: 1 }] }, analysis: candidate.motive.analysis } };
const rv = V.candidateView(rich);
check(rv.suits === "Suits: beginning, cadential", "the suits line names the functions that exist");
check(rv.remoteness === "33% remote from the basic motive" && V.candidateView(candidate).remoteness === "", "remoteness reads as a percentage of the basic motive");
check(rv.harmony === "Harmony: I V@2" && V.candidateView(candidate).harmony === "", "the harmony line shows only with chords");
const richBody = V.candidateBody({ motif: { form: "full", degrees: "1:h", harmony: "I", features: [{ label: "z", first: 0, last: 5 }] } }, rich);
check(richBody.motif.harmony === "I V@2" && same(richBody.motif.features, [{ label: "a", first: 0, last: 1 }]), "a candidate body takes the candidate's harmony and features");
check(V.candidateBody({ motif: { harmony: "I" } }, candidate).motif.harmony === null, "a candidate without chords renders without the source's");
const richTree = { id: "m1", name: "Opening", current_version: 1, derived: null, children: [{ id: "m2", name: "Opening transpose", current_version: 1, derived: { kind: "motive-form", flag: "unflagged", kept: [], changed: [], operations: [], functions: ["contrast"], remoteness: 0.5, harmony: "I V@2" }, children: [] }] };
const richRows = V.treeRows(richTree, false).rows;
check(richRows[1].suits === "Suits: contrast" && richRows[1].remoteness === "50% remote from the basic motive" && richRows[1].harmony === "Harmony: I V@2", "saved forms show functions, remoteness and harmony");
check(richRows[0].suits === "" && richRows[0].remoteness === "", "the root has none");
/* R-189: distance and the Suits filter. */
check(JSON.parse(V.generateRequest("m1", { distance: "far" }).request.body).distance === "far" &&
  JSON.parse(V.generateRequest("m1", { distance: "" }).request.body).distance === undefined &&
  JSON.parse(V.generateRequest("m1", { distance: "bogus" }).request.body).distance === undefined, "distance is sent only when close, medium or far");
check(V.generateRequest("m1", { distance: "close", steps: [{ op: "invert" }] }).error === "Distance takes random operations", "a distance with chosen steps is refused");
check(V.generatedNote([1, 2], 6, 9, "close") === "2 of 6 found at this distance." && V.generatedNote([1, 2], 6, 9, "") === "Seed 9" &&
  V.generatedNote([1, 2, 3, 4, 5, 6], 6, 9, "far") === "Seed 9" && V.generatedNote([], 6, 9, "far") === "No variants came out", "the status says how many were found at the distance");
check(V.suitsLine(["beginning", "bogus", "cadential"]) === "Suits: beginning, cadential" && V.suitsLine([]) === "Suits: none" && V.suitsLine(null) === "Suits: none", "the suits line lists the functions or none");
check(same(V.SUITS_FILTERS.map((f) => f.text), ["Any", "Beginning", "Continuation", "Contrast", "Cadential"]), "the Suits filter offers Any and the four functions");
check(V.suitsMatch(["contrast"], "") && V.suitsMatch(["contrast", "cadential"], "cadential") && !V.suitsMatch(["contrast"], "cadential") && !V.suitsMatch(undefined, "cadential"), "a form passes the filter when it suits the function");
const suitsTree = { id: "m1", name: "Opening", current_version: 1, derived: null, children: [
  { id: "s1", name: "One", current_version: 1, derived: { kind: "variant", flag: "unflagged", functions: ["beginning"] }, children: [
    { id: "s2", name: "Two", current_version: 1, derived: { kind: "variant", flag: "unflagged", functions: ["cadential"] }, children: [] } ] },
  { id: "s3", name: "Three", current_version: 1, derived: { kind: "variant", flag: "unflagged", functions: [] }, children: [] } ] };
check(same(V.treeRows(suitsTree, false, "cadential").rows.map((r) => r.id), ["m1", "s2"]), "the Suits filter keeps the root and the saved variants that suit it");
check(same(V.treeRows(suitsTree, false, "").rows.map((r) => r.id), ["m1", "s1", "s2", "s3"]) && V.treeRows(suitsTree, false).rows.length === 4, "Any keeps every saved variant");
check(V.treeRows(suitsTree, false).rows[3].suits === "Suits: none" && V.treeRows(suitsTree, false).rows[1].suits === "Suits: beginning", "a saved row has its suits line");
/* R-190: Try every operation. */
const mkResult = (op, notes, seed) => ({ seed, chain: { steps: [{ op }] }, derivation: op, motive: { analysis: { notes_text: notes } } });
const eachOps = [
  { name: "exact", label: "Exact", category: "exact", citation: "ch. II", params: [], forms: ["full"] },
  { name: "augment", label: "Augment", category: "rhythm", citation: "ch. III", params: [], forms: ["full"] },
  { name: "invert", label: "Invert", category: "intervals", citation: "ch. IV", params: [], forms: ["full"] },
  { name: "transpose", label: "Transpose", category: "intervals", citation: "ch. IV", params: [], forms: ["full"] },
];
const eachGroups = V.eachOpGroups([mkResult("exact", "C4:q", 1), mkResult("augment", "C4:h", 2), mkResult("transpose", "D4:q", 3), mkResult("invert", "A3:q", 4), mkResult("mystery", "C4:e", 5)], eachOps);
check(same(eachGroups.map((g) => g.category), ["exact", "rhythm", "intervals", ""]), "results group by category in Schoenberg's order, an unknown operation last");
check(same(eachGroups[2].entries.map((e) => e.op), ["transpose", "invert"]) && eachGroups[2].entries[0].label === "Transpose" && eachGroups[2].entries[0].citation === "ch. IV",
  "inside a category the results keep the order they came in, with label and citation");
check(eachGroups[3].title === "Other" && eachGroups[3].entries[0].label === "mystery", "an operation not in the list gets an Other group");
check(V.eachOpGroups([], eachOps).length === 0 && V.eachOpGroups(null, eachOps).length === 0, "no results give no groups");
check(V.resultNotes(mkResult("exact", "C4:q", 1)) === "C4:q" && V.resultNotes({}) === "", "the result's notes come from its analysis");
const mark = V.makeMark("wrong", "  too low  ", mkResult("invert", "A3:q", 4), "2026-10-23");
check(same(mark, { verdict: "wrong", comment: "too low", seed: 4, notes_text: "A3:q", date: "2026-10-23" }), "a mark keeps the verdict, comment, seed, notes and date");
check(V.makeMark("bogus", "", mkResult("x", "", 1), "2026-10-23").verdict === "works" && V.makeMark("works", "x".repeat(600), mkResult("x", "", 1), "d").comment.length === 500, "a mark's verdict defaults to works and its comment is cut to 500");
check(V.markView(null, mkResult("invert", "A3:q", 4)) === null && V.markView({ verdict: "fine" }, mkResult("invert", "A3:q", 4)) === null, "no mark, no mark view");
const sameView = V.markView(mark, mkResult("invert", "A3:q", 4));
check(sameView.text === "Wrong" && sameView.comment === "too low" && sameView.date === "2026-10-23" && sameView.changed === false, "a mark shows its verdict, comment and date");
check(V.markView(mark, mkResult("invert", "B3:q", 4)).changed === true, "a mark is changed since when the result's notes differ");
const eachRequest = V.eachOpRequest("m1", "7");
check(eachRequest.request.method === "POST" && eachRequest.request.url === "/api/materials/items/m1/each-op" && same(JSON.parse(eachRequest.request.body), { seed: 7 }) &&
  same(JSON.parse(V.eachOpRequest("m1", "").request.body), {}) && V.eachOpRequest("m1", "x").error !== undefined, "the each-op request takes an optional seed");
check(V.opChecksRequest("m1").method === "GET" && V.opChecksPutRequest("m1", { invert: mark }).method === "PUT" &&
  same(JSON.parse(V.opChecksPutRequest("m1", { invert: mark }).body), { checks: { invert: mark } }), "the op-checks requests GET and PUT the marks");
const findingsMarks = {
  augment: { verdict: "works", comment: "", seed: 2, notes_text: "C4:h", date: "2026-10-23" },
  invert: { verdict: "wrong", comment: "too low", seed: 4, notes_text: "A3:q", date: "2026-10-23" },
  exact: { verdict: "works", comment: "same notes", seed: 1, notes_text: "C4:q", date: "2026-10-23" },
  transpose: { verdict: "wrong", comment: "", seed: 3, notes_text: "D4:q", date: "2026-10-23" },
};
const findings = V.findingsText("Opening", { notes: "C4:q E4:q", key: "C minor", meter: "4/4" }, findingsMarks, ["exact", "augment", "invert", "transpose"]);
check(findings.split("\n").slice(0, 4).join("|") === "Opening|Notes: C4:q E4:q|Mode: minor|Meter: 4/4", "the findings start with the motif's name, notes, mode and meter");
check(same(findings.split("\n").slice(5), ["invert: wrong - too low (result A3:q)", "transpose: wrong (result D4:q)", "exact: works - same notes (result C4:q)", "augment: works (result C4:h)"]),
  "one line per marked operation, wrong first, each in registry order");
check(V.findingsText("Empty", { notes: "", key: "C major", meter: "3/4" }, {}).split("\n").length === 5, "no marks give only the header");
const filtered = JSON.parse(V.generateRequest("m1", { functions: ["cadential", "bogus", "beginning"] }).request.body);
check(same(filtered.functions, ["beginning", "cadential"]) && JSON.parse(V.generateRequest("m1", {}).request.body).functions === undefined, "the function filter is sent in order and only when set");
check(same(V.featureOptions([{ label: "a", first: 0, last: 2 }, { label: "turn", first: 3, last: 3 }]), [{ value: "", text: "drawn" }, { value: "0", text: "a (notes 1-3)" }, { value: "1", text: "turn (notes 4-4)" }]) && V.featureOptions(null).length === 1, "feature parameters offer the motif's labels");

/* a variant's own pickup and meter */
const pickupSource = { motif: { form: "full", pickup: "0", meter: "4/4", key: "C major" } };
const pickupCandidate = (pickup, meter) => ({ motive: { motive: { meter }, analysis: { notes_text: "C4:q E4:q", pickup } }, harmony: null });
const shifted = pickupCandidate("1", { beats_per_bar: 4, beat_unit: 4 });
check(V.candidateBody(pickupSource, shifted).motif.pickup === "1", "a rhythm-shift or add-upbeat candidate keeps its pickup");
check(V.candidateBody(pickupSource, pickupCandidate("0", { beats_per_bar: 3, beat_unit: 4 })).motif.meter === "3/4", "a change-meter candidate keeps its meter");
check(V.candidateBody(pickupSource, { motive: {} }).motif.pickup === "0" && V.candidateBody(pickupSource, { motive: {} }).motif.meter === "4/4", "a candidate without them keeps the source's");
check(V.resultNotes(shifted) === "C4:q E4:q pickup 1" && V.resultNotes(pickupCandidate("0", null)) === "C4:q E4:q", "the result text names a pickup that is not zero");
const pickupMark = V.makeMark("works", "", shifted, "2026-10-28");
check(pickupMark.notes_text === "C4:q E4:q pickup 1" && V.markView(pickupMark, shifted).changed === false && V.markView(pickupMark, pickupCandidate("0", null)).changed === true, "a mark records the pickup and changed since compares it");
check(V.findingsText("Opening", { notes: "C4:q", key: "C major", meter: "4/4" }, { "rhythm-shift": pickupMark }, ["rhythm-shift"]).includes("(result C4:q E4:q pickup 1)"), "Copy findings shows the pickup");
check(V.stepFromValues({ name: "repeat-feature", params: [{ name: "feature", kind: "int", min: 0, max: 7 }] }, { feature: "1" }).step.params.feature === 1, "a chosen feature becomes its index");

const compareReq = V.compareRequest({ item: "m1", version: 2 }, { item: "m2", version: 3 });
check(compareReq.method === "POST" && compareReq.url === "/api/materials/recognize" && same(JSON.parse(compareReq.body), { source: { item: "m1", version: 2 }, target: { item: "m2", version: 3 } }), "the compare request names both motifs and their versions");
const recognition = { found: true, chain: { steps: [{ op: "invert", params: {} }, { op: "augment", params: { ratio: "2:1" } }] }, distance: 0.25, kept: ["rhythm"], changed: ["intervals"], derivation: "Inverted, then augmented." };
const linkReq = V.linkFormRequest("m2", { item: "m1", version: 2 }, recognition);
check(linkReq.method === "POST" && linkReq.url === "/api/materials/items/m2/link-form" && same(JSON.parse(linkReq.body), { source: { item: "m1", version: 2 }, recognition }), "the link request goes to the target with the source and the recognition");
const related = V.compareView(recognition);
check(related.found && related.canLink && related.chain === "invert then augment" && related.related === "75% related" && related.derivation === "Inverted, then augmented.", "a found chain shows the chain, the derivation and N% related");
check(same(related.chips.map((c) => c.text), ["kept rhythm", "changed intervals"]), "the compare result shows kept and changed chips");
const unrelated = V.compareView({ found: false, chain: { steps: [] }, distance: 1, kept: [], changed: [] });
check(!unrelated.found && !unrelated.canLink && unrelated.text === "Not related within 4 steps" && unrelated.related === "" && unrelated.chips.length === 0, "no chain reads Not related within 4 steps and cannot be linked");
check(!V.compareView(null).found && V.compareView({ found: true, chain: { steps: [] }, distance: 0, kept: [], changed: [] }).related === "100% related", "a missing answer is not related and an exact one is 100% related");
const linkedRows = V.treeRows({ id: "m1", name: "A", current_version: 1, derived: null, children: [{ id: "m2", name: "B", current_version: 2, derived: { kind: "motive-form", flag: "unflagged", kept: ["rhythm"], changed: ["intervals"], operations: [{ op: "invert" }], seed: null, functions: [], remoteness: 0.2, harmony: null }, children: [] }] }, false).rows;
check(linkedRows.length === 2 && linkedRows[1].seed === null && linkedRows[1].derivation === "invert" && linkedRows[1].remoteness === "20% remote from the basic motive", "a linked form is a tree row with no seed");

check(V.noDegreesMessage({ degrees: "1:q 5:e" }) === "", "a motif with degrees has no blocking message");
check(V.noDegreesMessage({ degrees: null, analysis_error: "composer not found" }) === "No analysis yet: composer not found. Re-analyze on the motif first.", "the message carries the stored reason");
check(V.noDegreesMessage({ degrees: null }).startsWith("No analysis yet: ") && V.noDegreesMessage(null).endsWith("Re-analyze on the motif first."), "a motif with no stored reason still says to re-analyze");


/* The render queue: no more than the limit run at once, all of them finish, in order. */
const queue = V.createQueue(V.RENDER_LIMIT);
let running = 0;
let peak = 0;
const order = [];
const queued = Array.from({ length: 10 }, (_, n) =>
  queue.add(
    () =>
      new Promise((resolve) => {
        running++;
        peak = Math.max(peak, running);
        order.push(n);
        setTimeout(() => {
          running--;
          resolve(n);
        }, 5);
      }),
  ),
);
check(V.RENDER_LIMIT === 4 && queue.active() === 4 && queue.waiting() === 6, "ten tasks: four run, six wait");

/* A minimal DOM, enough for VariantTreeView to build and click. */
class FakeEl {
  constructor(tag) {
    this.tag = tag;
    this.children = [];
    this.listeners = {};
    this.attributes = {};
    this.style = {};
    this.className = "";
    this.textContent = "";
    const owner = this;
    this.classList = {
      toggle(name, on) {
        const names = new Set(owner.className.split(" ").filter(Boolean));
        if (on) {
          names.add(name);
        } else {
          names.delete(name);
        }
        owner.className = [...names].join(" ");
      },
      add(name) {
        this.toggle(name, true);
      },
      remove(name) {
        this.toggle(name, false);
      },
    };
  }
  set innerHTML(value) {
    this.children = [];
  }
  append(...nodes) {
    this.children.push(...nodes);
  }
  appendChild(node) {
    this.children.push(node);
    return node;
  }
  setAttribute(name, value) {
    this.attributes[name] = value;
  }
  addEventListener(name, fn) {
    (this.listeners[name] = this.listeners[name] || []).push(fn);
  }
  click() {
    for (const fn of this.listeners.click || []) {
      fn({});
    }
  }
  all() {
    return [this, ...this.children.flatMap((child) => (child instanceof FakeEl ? child.all() : []))];
  }
}
const named = (root, tag, text) => root.all().filter((node) => node.tag === tag && node.textContent === text);

const fs = require("fs");
const vm = require("vm");
const mounts = [];
const destroyed = [];
const sandbox = {
  document: { createElement: (tag) => new FakeEl(tag) },
  window: { localStorage: { getItem: () => null, setItem: () => {} } },
  VariantTree: V,
  MaterialsModel: require("../public/materials-model.js"),
  ScoreWorkspace: {
    mount(host, options) {
      mounts.push({ host, options });
      return { destroy: () => destroyed.push(options.title), play: () => {} };
    },
  },
  URL: { createObjectURL: () => "blob:x", revokeObjectURL: () => {} },
  Blob: class {},
  JSON,
  Promise,
  Number,
  Math,
};
vm.createContext(sandbox);
const View = vm.runInContext(fs.readFileSync(`${__dirname}/../public/variant-tree-view.js`, "utf8") + "\n;VariantTreeView", sandbox);

const savedTree = node("root", null, [node("a", "approved", []), node("b", "unflagged", [])]);
let renderRunning = 0;
let renderPeak = 0;
let renderCount = 0;
const answers = [];
const ctx = {
  id: "root",
  name: "Motif",
  body: { motif: { degrees: "1:q 5:e", form: "full" } },
  projectId: "p1",
  projects: [{ id: "p1", name: "Project" }],
  open: () => {},
  send(request) {
    if (request.url.endsWith("variant-operations")) {
      return Promise.resolve({ ok: true, data: { operations: [] } });
    }
    if (request.url.endsWith("op-checks")) {
      return Promise.resolve({ ok: true, data: { checks: {} } });
    }
    if (request.url.endsWith("variant-tree")) {
      return Promise.resolve({ ok: true, data: { tree: savedTree } });
    }
    if (request.url.endsWith("/variants")) {
      const variants = Array.from({ length: 6 }, (_, n) => ({ seed: n, derivation: `invert ${n}`, motive: { motive: {}, analysis: {} } }));
      return Promise.resolve({ ok: true, data: { variants, seed: 1 } });
    }
    renderRunning++;
    renderCount++;
    renderPeak = Math.max(renderPeak, renderRunning);
    return new Promise((resolve) => {
      answers.push(() => {
        renderRunning--;
        resolve({ ok: true, data: { musicxml: "<x/>", playback: {} } });
      });
    });
  },
};
const settle = () => new Promise((resolve) => setTimeout(resolve, 20));
const parent = new FakeEl("div");


const withHarmony = (text, tones, warnings) => ({
  ...candidate,
  derivation: "transpose",
  motive: { motive: {}, analysis: { notes_text: "C5:q", base_harmony: { text, given: false }, tones: tones || [], warnings: warnings || [] } },
});
const hv = V.candidateView(withHarmony("I V@2", [{ note: 1, role: "chord" }], ["Does not outline its harmony"]), "I");
check(hv.harmony === "Harmony: I V@2" && same(hv.warnings, ["Does not outline its harmony"]), "a candidate shows its base harmony and warnings");
check(hv.derivation === "transpose, harmony I to I V@2", "the derivation names the harmony change");
check(V.candidateView(withHarmony("I"), "I").derivation === "transpose" && V.candidateView(withHarmony("I"), "").derivation === "transpose", "no harmony change is named when it is the same or the source is unknown");
check(V.sourceHarmonyText({ motif: { analysis: { analysis: { base_harmony: { text: "I IV@3" } } } } }) === "I IV@3" && V.sourceHarmonyText({}) === "", "the source harmony comes from the stored analysis");
const harmonyTree = { id: "m1", name: "A", current_version: 1, derived: null, children: [
  { id: "m2", name: "B", current_version: 1, derived: { kind: "variant", flag: "unflagged", kept: [], changed: [], operations: [{ op: "harmony-append" }], base_harmony: "I V@2", tones: [{ note: 1, role: "chord" }], warnings: ["w"] }, children: [
    { id: "m3", name: "C", current_version: 1, derived: { kind: "variant", flag: "unflagged", kept: [], changed: [], operations: [{ op: "transpose" }], base_harmony: "I V@2" }, children: [] } ] } ] };
const harmonyRows = V.treeRows(harmonyTree, false, "", "I").rows;
check(harmonyRows[1].harmony === "Harmony: I V@2" && harmonyRows[1].derivation === "harmony-append, harmony I to I V@2" && same(harmonyRows[1].warnings, ["w"]), "a saved row shows harmony, the change from the source and warnings");
check(harmonyRows[1].toneBody.motif.analysis.analysis.tones.length === 1 && harmonyRows[2].derivation === "transpose", "a saved row carries its tones and a child with the same harmony names no change");
const marks = { "harmony-append": { verdict: "works", comment: "ok", notes_text: "C5:q" }, transpose: { verdict: "wrong", comment: "", notes_text: "D5:q" } };
const harmonies = V.resultHarmonies([{ ...withHarmony("I V@2"), chain: { steps: [{ op: "harmony-append" }] } }]);
check(same(harmonies, { "harmony-append": "I V@2" }), "result harmonies are by operation");
const text = V.findingsText("Opening", { key: "C major", notes: "C5:q", meter: "4/4" }, marks, ["harmony-append", "transpose"], harmonies);
check(text.includes("harmony-append: works - ok (result C5:q harmony I V@2)") && text.includes("transpose: wrong (result D5:q)"), "findings add the harmony only when the variant has one");

(async () => {
  const order2 = await Promise.all(queued);
  check(peak === 4 && order2.length === 10 && same(order, order2), "the queue never runs more than four and finishes every task in order");

  const view = View.mount(parent, ctx);
  await settle();
  const rows = () => parent.all().filter((n) => n.className.split(" ")[0] === "variant-node");
  check(rows().length === 3 && named(parent, "button", "Show notation").length === 2, "the root has no toggle; each saved variant has Show notation");

  named(parent, "button", "Generate")[0].click();
  await settle();
  check(renderCount === 4 && renderPeak === 4, "six candidates ask the Composer for four renders at first");
  answers.shift()();
  await settle();
  check(renderCount === 5 && renderPeak === 4, "a finished render lets the next candidate start");
  while (answers.length > 0) {
    answers.shift()();
    await settle();
  }
  const candidateMounts = mounts.filter((m) => m.options.subtitle.startsWith("candidate"));
  check(candidateMounts.length === 6 && candidateMounts.every((m) => m.options.compact === true), "every candidate mounts a compact score");

  view.leave();
  check(destroyed.length === 6, "leaving the tab destroys the candidate scores");

  named(parent, "button", "Show notation")[0].click();
  const rowMounts = () => mounts.filter((m) => m.options.subtitle.startsWith("variant"));
  check(rowMounts().length === 1 && named(parent, "button", "Hide notation").length === 1, "Show notation mounts that row's compact score");
  await view.refresh();
  check(rowMounts().length === 2 && named(parent, "button", "Hide notation").length === 1, "the open row stays open when the tree is refreshed");
  named(parent, "button", "Hide notation")[0].click();
  check(rowMounts().length === 2 && named(parent, "button", "Show notation").length === 2, "Hide notation closes it again");

  if (failures > 0) {
    console.error(`${failures} check(s) failed`);
    process.exit(1);
  }
})();
