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
check(V.treeRequest("v1").method === "GET" && V.treeRequest("v1").url === "/api/materials/items/v1/variant-tree", "tree url");

const cv = V.candidateView(candidate);
check(cv.recognizability === "62% recognizable" && cv.kind.text === "Motive-form" && cv.chips.length === 2, "candidate view");
check(V.candidateDegrees(candidate, "full") === "1:q 3:q" && V.candidateDegrees(candidate, "rhythm") === "x:q x:q" && V.candidateDegrees(candidate, "contour") === "0 1", "degrees by form");
const body = V.candidateBody({ motif: { form: "rhythm", degrees: "x:h", key: "C major" } }, candidate);
check(body.motif.degrees === "x:q x:q" && body.motif.key === "C major", "a candidate body keeps the source and swaps the degrees");

const rich = { ...candidate, functions: ["beginning", "bogus", "cadential"], remoteness: 0.3333, harmony: "I V@2", motive: { motive: { features: [{ label: "a", first: 0, last: 1 }] }, analysis: candidate.motive.analysis } };
const rv = V.candidateView(rich);
check(same(rv.functions.map((chip) => chip.text), ["beginning", "cadential"]) && rv.functions[0].state === "function", "function chips name the functions that exist");
check(rv.remoteness === "33% remote from the basic motive" && V.candidateView(candidate).remoteness === "", "remoteness reads as a percentage of the basic motive");
check(rv.harmony === "Harmony: I V@2" && V.candidateView(candidate).harmony === "", "the harmony line shows only with chords");
const richBody = V.candidateBody({ motif: { form: "full", degrees: "1:h", harmony: "I", features: [{ label: "z", first: 0, last: 5 }] } }, rich);
check(richBody.motif.harmony === "I V@2" && same(richBody.motif.features, [{ label: "a", first: 0, last: 1 }]), "a candidate body takes the candidate's harmony and features");
check(V.candidateBody({ motif: { harmony: "I" } }, candidate).motif.harmony === null, "a candidate without chords renders without the source's");
const richTree = { id: "m1", name: "Opening", current_version: 1, derived: null, children: [{ id: "m2", name: "Opening transpose", current_version: 1, derived: { kind: "motive-form", flag: "unflagged", kept: [], changed: [], operations: [], functions: ["contrast"], remoteness: 0.5, harmony: "I V@2" }, children: [] }] };
const richRows = V.treeRows(richTree, false).rows;
check(richRows[1].functions[0].text === "contrast" && richRows[1].remoteness === "50% remote from the basic motive" && richRows[1].harmony === "Harmony: I V@2", "saved forms show functions, remoteness and harmony");
check(richRows[0].functions.length === 0 && richRows[0].remoteness === "", "the root has none");
const filtered = JSON.parse(V.generateRequest("m1", { functions: ["cadential", "bogus", "beginning"] }).request.body);
check(same(filtered.functions, ["beginning", "cadential"]) && JSON.parse(V.generateRequest("m1", {}).request.body).functions === undefined, "the function filter is sent in order and only when set");
check(same(V.featureOptions([{ label: "a", first: 0, last: 2 }, { label: "turn", first: 3, last: 3 }]), [{ value: "", text: "drawn" }, { value: "0", text: "a (notes 1-3)" }, { value: "1", text: "turn (notes 4-4)" }]) && V.featureOptions(null).length === 1, "feature parameters offer the motif's labels");
check(V.stepFromValues({ name: "repeat-feature", params: [{ name: "feature", kind: "int", min: 0, max: 7 }] }, { feature: "1" }).step.params.feature === 1, "a chosen feature becomes its index");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
