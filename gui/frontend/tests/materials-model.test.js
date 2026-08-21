/* Covers the pure materials helpers (public/materials-model.js): request
 * builders, new item bodies, the project filter, item and version rows.
 * Plain Node script: `node frontend/tests/materials-model.test.js`. */
const M = require("../public/materials-model.js");

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

const projects = [
  { id: "prj-a", name: "Sonata" },
  { id: "prj-b", name: "Etudes" },
];

check(same(M.TYPES.map((t) => t.label), ["Motifs", "Themes", "Pieces"]), "three types in sidebar order");
check(M.typeInfo("theme").singular === "theme" && M.typeInfo("x") === null, "typeInfo finds a type or null");

const create = M.buildCreateProjectRequest("  Sonata ", "first");
check(create.method === "POST" && create.url === "/api/materials/projects", "create project posts to the projects route");
check(same(JSON.parse(create.body), { name: "Sonata", description: "first" }), "create project trims the name");
check(!("description" in JSON.parse(M.buildCreateProjectRequest("a").body)), "create project leaves out an empty description");
const rename = M.buildRenameProjectRequest("prj-a", " Op. 1 ");
check(rename.method === "PATCH" && rename.url === "/api/materials/projects/prj-a", "rename project patches the project");
check(same(JSON.parse(rename.body), { name: "Op. 1" }), "rename project sends the trimmed name");
check(M.buildDeleteProjectRequest("prj-a").method === "DELETE" && !M.buildDeleteProjectRequest("prj-a").body, "delete project has no body");

check(M.buildItemListRequest("motif", "all").url === "/api/materials/items?type=motif", "item list for all projects has no project");
check(
  M.buildItemListRequest("piece", "prj-a").url === "/api/materials/items?type=piece&project=prj-a",
  "item list for a project adds it",
);
check(M.buildVersionGetRequest("mot-1", 2).url === "/api/materials/items/mot-1/versions/2", "version route");

const motif = M.newItemBody("motif").motif;
check(motif.notes !== "" && motif.pickup === "0" && same(motif.features, []) && motif.derived === null, "new motif body is valid");
check(same(M.newItemBody("theme"), { uses: [], theme: { form: null, slots: [] } }), "new theme body");
check(same(M.newItemBody("piece"), { uses: [], piece: { structure: null } }), "new piece body");
const item = JSON.parse(M.buildCreateItemRequest("theme", " Main ", "prj-a").body);
check(item.type === "theme" && item.name === "Main" && item.project_id === "prj-a" && item.body.theme, "create item body");
check(same(JSON.parse(M.buildUpdateItemRequest("x", { name: " B ", project_id: "" }).body), { name: "B" }), "update item skips empty fields");

check(M.errorText({ error: "project is not empty" }) === "project is not empty", "error text from the answer");
check(M.errorText(null, "fallback") === "fallback", "error text fallback");

check(same(M.projectOptions(projects).map((o) => o.label), ["All projects", "Sonata", "Etudes"]), "project options start with All projects");
check(M.resolveFilter("prj-b", projects) === "prj-b", "a known filter is kept");
check(M.resolveFilter("prj-gone", projects) === "all", "a deleted project falls back to all");
check(M.defaultProject("prj-b", projects) === "prj-b" && M.defaultProject("all", projects) === "prj-a", "new item project");
check(M.defaultProject("all", []) === "", "no projects, no default");
check(M.readFilter() === "all", "filter reads as all without storage");
M.writeFilter("prj-a");

const rows = M.itemRows(
  [
    { id: "mot-1", name: "Old", project_id: "prj-a", current_version: 1, updated_at: "2026-10-01T10:00:00Z" },
    { id: "mot-2", name: "New", project_id: "prj-b", current_version: 3, updated_at: "2026-10-20T09:00:00Z" },
  ],
  projects,
);
check(rows[0].id === "mot-2" && rows[0].project === "Etudes" && rows[0].version === "v3", "item rows newest first with project and version");
check(rows[0].updated === "2026-10-20", "item row date");
check(M.itemRows(null, projects).length === 0, "no items, no rows");

const vrows = M.versionRows({
  current_version: 2,
  versions: [
    { number: 1, created_at: "2026-10-01T10:00:00Z", note: "first" },
    { number: 2, created_at: "2026-10-02T10:00:00Z" },
  ],
});
check(vrows[0].number === 2 && vrows[0].current && vrows[0].note === "", "version rows newest first, current marked");
check(vrows[1].note === "first" && !vrows[1].current, "older version row");

const uses = M.useRows([{ item_id: "mot-1", name: "Hook", version: 2, missing: false }, { item_id: "mot-9", version: 1, missing: true }]);
check(uses[0].label === "Hook v2" && uses[1].missing && uses[1].label === "mot-9 v1", "use rows");

// Motif editor helpers.
const body = {
  uses: [{ item_id: "mot-9", version: 1, copy: {} }],
  motif: {
    role: "accompaniment",
    form: "full",
    notes: "D4:q F4:e",
    meter: "3/4",
    key: "D minor",
    pickup: "1/2",
    features: [{ label: "a", first: 0, last: 1 }],
    analysis: { analysis: {} },
    derived: { source_item: "mot-1" },
  },
};
const values = M.motifFormValues(body);
check(values.role === "accompaniment" && values.mode === "minor" && values.pickup === "1/2", "form values read the motif and the mode of its key");
check(values.legacyKey === "D minor", "a key on another tonic is the legacy key");
check(M.motifFormValues({ motif: { key: "C minor" } }).legacyKey === null && M.motifFormValues({ motif: { key: "C major" } }).mode === "major", "a key on C is not legacy");
check(M.motifFormValues({ motif: { key: "Eb major" } }).legacyKey === "Eb major" && M.motifFormValues({ motif: { key: null } }).legacyKey === null && M.motifFormValues(null).mode === "major", "mode defaults to major and no key is not legacy");
check(same(M.MOTIF_MODES, ["major", "minor"]) && M.TONIC_HINT.startsWith("Write the notes with C as the tonic"), "the modes and the tonic hint");
check(same(values.features, [{ label: "a", first: 1, last: 2 }]), "features show note numbers from 1");
check(values.harmony === "" && M.motifFormValues({ motif: { harmony: "I V6@2" } }).harmony === "I V6@2", "form values read the harmony");
check(M.motifFormProblems({ ...values, legacyKey: null, harmony: "I V6@2 I@3" }).length === 0, "a harmony is fine");
check(M.motifFormProblems({ ...values, legacyKey: null, harmony: "I ".repeat(101) })[0].field === "harmony", "a harmony over 200 characters is a problem");
check(M.buildMotifBody({ ...values, harmony: " I V6@2 " }, null).motif.harmony === "I V6@2", "the body holds the trimmed harmony");
check(M.buildMotifBody({ ...values, harmony: "  " }, null).motif.harmony === null, "an empty harmony is stored as null");
check(M.newItemBody("motif").motif.harmony === null, "a new motif has no harmony");
check(M.errorField("harmony: token 2 (column 4) is not a chord") === "harmony" && M.errorField("features[0] is outside the notes") === "features" && M.errorField("give a key") === "notes", "a Composer refusal goes to its field");
const withHarmony = M.renderRequest({ motif: { form: "full", degrees: "1:q", meter: "4/4", harmony: "I V@2", features: [{ label: "a", first: 0, last: 1 }] } });
check(JSON.parse(withHarmony.body).harmony === "I V@2" && JSON.parse(withHarmony.body).features.length === 1, "a render passes the harmony and features");
check(JSON.parse(M.renderRequest({ motif: { form: "full", degrees: "1:q", meter: "4/4" } }).body).harmony === undefined, "a render without harmony leaves it out");
check(JSON.parse(M.randomRequest({ mode: "major", meter: "4/4", role: "accompaniment" }).body).role === "accompaniment" && JSON.parse(M.randomRequest({ mode: "major", meter: "4/4" }).body).role === undefined, "random motifs pass an accompaniment role");
const blank = M.motifFormValues(null);
check(blank.notes === "" && blank.meter === "4/4" && blank.pickup === "0" && blank.form === "full", "a new motif form has defaults");
check(M.motifFormProblems(blank).length === 1 && M.motifFormProblems(blank)[0].field === "notes", "empty notes are a problem");
check(M.motifFormProblems({ ...values, legacyKey: null }).length === 0, "a filled form has no problems");
check(M.motifFormProblems(values)[0].field === "notes" && /Rewrite the motif in C/.test(M.motifFormProblems(values)[0].message), "an older motif must be rewritten in C before saving");
check(M.motifFormProblems({ ...values, legacyKey: null, meter: "four" })[0].field === "meter", "a bad meter is a problem");
check(M.motifFormProblems({ ...values, legacyKey: null, key: "Dm" }).length === 0, "the form has no key to check");
check(M.motifFormProblems({ ...values, legacyKey: null, pickup: "x" })[0].field === "pickup", "a bad pickup is a problem");

const rebuilt = M.buildMotifBody({ ...values, notes: " C4:q ", pickup: "" }, body);
check(rebuilt.motif.notes === "C4:q" && rebuilt.motif.key === "C minor" && rebuilt.motif.pickup === "0", "body trims notes, writes the key in C and an empty pickup is 0");
check(M.buildMotifBody({ ...values, mode: "major" }, null).motif.key === "C major" && M.buildMotifBody({ ...values, mode: "dorian" }, null).motif.key === "C major", "the body key is C in the chosen mode, major for anything else");
check(M.buildMotifBody({ ...values, form: "rhythm", mode: "minor" }, null).motif.key === "C minor", "every form is written in C");
check(same(rebuilt.motif.features, [{ label: "a", first: 0, last: 1 }]), "body stores features from 0");
check(rebuilt.uses.length === 1 && rebuilt.motif.derived.source_item === "mot-1", "body keeps uses and derived");
check(rebuilt.motif.analysis === null, "body leaves the analysis to the backend");
check(M.buildMotifBody(blank, null).uses.length === 0 && M.buildMotifBody(blank, null).motif.derived === null, "a new body has no uses or derived");
check(M.MOTIF_FORMS.filter((f) => f.enabled).map((f) => f.value).join() === "full,rhythm,contour", "all three forms are enabled");

const saveNew = M.buildSaveMotifRequest({ name: " Hook ", project_id: "prj-a", body: rebuilt });
check(saveNew.url === "/api/materials/items" && JSON.parse(saveNew.body).type === "motif", "a new motif posts an item");
check(JSON.parse(saveNew.body).name === "Hook", "new motif name is trimmed");
const saveVersion = M.buildSaveMotifRequest({ id: "mot-1", note: " tweak ", body: rebuilt });
check(saveVersion.url === "/api/materials/items/mot-1/versions" && JSON.parse(saveVersion.body).note === "tweak", "an existing motif posts a version with its note");
check(!("note" in JSON.parse(M.buildSaveMotifRequest({ id: "mot-1", note: " ", body: rebuilt }).body)), "an empty note is left out");

// Features.
check(M.featureProblems([{ label: "a", first: 1, last: 2 }]).length === 0, "a good feature row passes");
check(M.featureProblems([{ label: "", first: 1, last: 1 }]).length === 1, "an empty label fails");
check(M.featureProblems([{ label: "abcdefghi", first: 1, last: 1 }]).length === 1, "a label over 8 characters fails");
check(M.featureProblems([{ label: "a", first: 1, last: 1 }, { label: "a", first: 2, last: 2 }]).length === 1, "a repeated label fails");
check(M.featureProblems([{ label: "a", first: 3, last: 2 }]).length === 1, "first after last fails");
check(M.featureProblems([{ label: "a", first: 0, last: 1 }]).length === 1, "note numbers start at 1");
check(M.featureProblems([{ label: "a", first: NaN, last: 1 }]).length === 1, "a blank number fails");
check(M.motifFormProblems({ ...values, legacyKey: null, features: [{ label: "", first: 1, last: 1 }] })[0].field === "features", "feature problems show on the form");
check(M.newFeatureRow([]).label === "a" && M.newFeatureRow([{ label: "a" }]).label === "b", "a new feature row takes the next free letter");

// Analysis rows.
const heard = {
  analysis: {
    motive: {},
    analysis: {
      scale_degrees: { all: [{ degree: 1, alteration: 0 }, { degree: 3, alteration: 0 }, { degree: 4, alteration: 1 }] },
      intervals: [{ semitones: 3, steps: 2 }, { semitones: -1, steps: -1 }],
      contour: { shape: "arch", directions: ["up", "down"] },
      rhythm: [
        { bar: 0, beat: { num: 0, den: 1 }, duration: { num: 1, den: 4 }, on_beat: true },
        { bar: 1, beat: { num: 3, den: 2 }, duration: { num: 1, den: 8 }, on_beat: false },
      ],
      implied_harmony: { key_known: true, candidates: [{ roman_numeral: "i", function: "tonic", fit: 0.8333 }] },
    },
  },
};
const info = M.analysisRows(heard);
check(info.analyzed && info.rows.map((r) => r.label).join() === "Scale degrees,Intervals,Contour,Rhythm,Implied harmony", "analysis rows in order");
const text = (label) => info.rows.find((r) => r.label === label).text;
check(text("Scale degrees") === "1 3 #4", "scale degrees show raised degrees");
check(text("Intervals") === "+3 (+2 steps), -1 (-1 steps)", "intervals show semitones and steps");
check(text("Contour") === "arch (up, down)", "contour shows shape and directions");
check(text("Rhythm") === "bar 1 beat 1, length 1/4; bar 2 beat 2+1/2, length 1/8 (off the beat)", "rhythm shows bar, beat and length");
check(text("Implied harmony") === "i (tonic) 83%", "harmony shows numeral, function and fit");
const unkeyed = M.analysisRows({
  analysis: { analysis: { scale_degrees: null, intervals: [{ semitones: 2, steps: null }], contour: { shape: "single note", directions: [] }, rhythm: [], implied_harmony: { key_known: false, candidates: [] } } },
});
check(unkeyed.rows.every((r) => r.label !== "Scale degrees"), "no scale degrees without a key");
check(unkeyed.rows.find((r) => r.label === "Intervals").text === "+2", "intervals without a key are semitones");
check(unkeyed.rows.find((r) => r.label === "Implied harmony").text === "needs a key", "harmony needs a key");
const none = M.analysisRows({ analysis: null }, "composer not found");
check(!none.analyzed && none.message === "Not analyzed: composer not found", "a null analysis names the reason");
check(M.analysisRows(null).message === "Not analyzed", "no motif is not analyzed");

// Motif forms: hints, MIDI import, degrees.
check(M.formHint("full") === M.NOTE_HINT, "the full form shows the note list hint");
check(M.formHint("rhythm").includes("x:q x:e x:e x:h"), "the rhythm form shows the rhythm syntax");
check(M.formHint("contour").includes("0 2 1 3"), "the contour form shows the contour syntax");
check(M.formHint("rhythm") !== M.formHint("contour"), "each form has its own hint");
const imported = M.importToFormValues({ notes_text: "C4:q Eb4:q", meter: "3/4", key: "C minor", moved_from: "D minor", key_found: true, pickup: "1" });
check(imported.form === "full" && imported.notes === "C4:q Eb4:q" && imported.meter === "3/4", "an import fills a full motif");
check(imported.mode === "minor" && imported.pickup === "1", "an import fills the mode and pickup");
const unkeyedImport = M.importToFormValues({ notes_text: "D4:q", meter: "4/4", key: "C major", moved_from: null, key_found: false, pickup: "0" });
check(unkeyedImport.mode === "major" && unkeyedImport.pickup === "0", "an import without a key reads as C major");
check(M.importNotice({ key: "C minor", moved_from: "D minor", key_found: true }) === "Moved from D minor to C minor.", "the notice names the key the notes moved from");
check(M.importNotice({ key: "C major", moved_from: null, key_found: false }) === "No key found: read as C major.", "the notice says when no key was found");
check(M.importNotice({ key: "C minor", moved_from: null, key_found: true }) === "Filled from the MIDI file.", "a MIDI file already in C is just filled");
check(M.legacyNotice("D minor") === "Written in D minor before motifs moved to C.", "the older motif notice names its key");
const toC = M.toCRequest({ motif: { degrees: "1:q 3:e", meter: "3/4", pickup: "" } }, "minor");
check(toC.method === "POST" && toC.url === "/api/materials/motifs/to-c" && same(JSON.parse(toC.body), { degrees: "1:q 3:e", mode: "minor", meter: "3/4", pickup: "0" }), "to-c posts the stored degrees, mode, meter and pickup");
check(M.toCRequest({ motif: { degrees: null } }, "minor") === null && M.toCRequest(null, "minor") === null, "a motif without degrees has nothing to rewrite");
check(JSON.parse(M.toCRequest({ motif: { degrees: "1:q" } }, "dorian").body).mode === "major", "to-c defaults the mode to major");
check(M.importToFormValues(null).meter === "4/4" && M.importToFormValues(null).notes === "", "an empty import answer gives defaults");
const importRequest = M.buildImportMidiRequest("file");
check(importRequest.method === "POST" && importRequest.url === "/api/materials/motifs/import" && importRequest.body === "file", "the import posts the file");
check(M.buildMotifBody(values, { ...body, motif: { ...body.motif, degrees: "1:q 3:e" } }).motif.degrees === "1:q 3:e", "body keeps the degrees");
check(M.buildMotifBody(blank, null).motif.degrees === null, "a new body has no degrees");
check(M.motifFormProblems({ ...values, legacyKey: null, pickup: "123" })[0].field === "pickup", "a pickup of three digits is a problem");
const withDegrees = M.analysisRows({ form: "full", degrees: "1:q 3:e", analysis: heard.analysis });
check(withDegrees.rows[0].label === "Degree list" && withDegrees.rows[0].text === "1:q 3:e", "the degree list is the first analysis row");
const rhythmRows = M.analysisRows({
  form: "rhythm",
  degrees: "x:q x:e",
  analysis: { analysis: { intervals: null, contour: null, rhythm: [{ bar: 0, beat: { num: 0, den: 1 }, duration: { num: 1, den: 4 }, on_beat: true }], implied_harmony: { key_known: false, candidates: [] } } },
});
check(rhythmRows.rows.map((r) => r.label).join() === "Rhythm list,Rhythm", "a rhythm motif shows its list and rhythm only");
const contourRows = M.analysisRows({
  form: "contour",
  degrees: "0 2 1",
  analysis: { analysis: { intervals: null, contour: { shape: "arch", directions: ["up", "down"] }, rhythm: null, implied_harmony: { key_known: false, candidates: [] } } },
});
check(contourRows.rows.map((r) => r.label).join() === "Contour list,Contour", "a contour motif shows its list and contour only");

// Item references.
const statuses = [
  { item_id: "mot-1", version: 1, current_version: 3, name: "Hook", type: "motif", project_id: "prj-a", missing: false },
  { item_id: "mot-2", version: 2, current_version: 2, name: "Tail", type: "motif", project_id: "prj-b", missing: false },
  { item_id: "mot-9", version: 1, name: "Gone", missing: true },
];
const useInfo = M.useRows(statuses, projects);
check(useInfo[0].stale && useInfo[0].note === "source updated to v3" && useInfo[0].current === 3, "a newer source version flags the use");
check(!useInfo[1].stale && useInfo[1].note === "", "an unchanged source is not flagged");
check(useInfo[2].missing && !useInfo[2].stale && useInfo[2].note === "source deleted", "a missing source says deleted, not updated");
check(useInfo[0].project === "Sonata" && useInfo[0].type === "motif", "use rows carry project and type");
check(M.hasUpdatedUse(statuses) && !M.hasUpdatedUse([statuses[1], statuses[2]]) && !M.hasUpdatedUse(null), "badge only for a newer source");
const upd = M.buildUpdateUseRequest("thm-1", "mot-1");
check(upd.method === "POST" && upd.url === "/api/materials/items/thm-1/uses/mot-1/update", "update use route");

const all = [
  { id: "mot-1", type: "motif", name: "Hook", project_id: "prj-a", current_version: 3 },
  { id: "mot-2", type: "motif", name: "Tail", project_id: "prj-b", current_version: 1 },
  { id: "thm-1", type: "theme", name: "Main hook", project_id: "prj-b", current_version: 2 },
];
check(same(M.pickerItems(all, projects, {}).map((i) => i.id), ["mot-1", "thm-1", "mot-2"]), "picker lists items of every project by name");
check(same(M.pickerItems(all, projects, { query: "HOOK" }).map((i) => i.id), ["mot-1", "thm-1"]), "picker searches by name, any case");
check(same(M.pickerItems(all, projects, { type: "theme" }).map((i) => i.id), ["thm-1"]), "picker filters by type");
check(same(M.pickerItems(all, projects, { project: "prj-b" }).map((i) => i.id), ["thm-1", "mot-2"]), "picker filters by project");
check(same(M.pickerItems(all, projects, { type: "all", project: "all" }).map((i) => i.id), ["mot-1", "thm-1", "mot-2"]), "all means no filter");
check(same(M.pickerItems(all, projects, { exclude: ["mot-1", "mot-2"] }).map((i) => i.id), ["thm-1"]), "picker leaves out excluded items");
check(M.pickerItems(all, projects, {})[0].label === "Hook (motif, Sonata) v3", "picker label");
check(M.pickerItems(null, projects, {}).length === 0, "no items, no choices");

const base = { uses: [{ item_id: "mot-1", version: 1, copy: {} }], motif: { notes: "x" } };
const added = M.bodyWithUse(base, "mot-2", 4, true);
check(added.uses.length === 2 && same(added.uses[1], { item_id: "mot-2", version: 4 }), "adding a use appends it at that version");
check(base.uses.length === 1, "the original body is not changed");
check(M.bodyWithUse(base, "mot-1", 3, true).uses.length === 1 && M.bodyWithUse(base, "mot-1", 3, true).uses[0].version === 3, "adding an existing use replaces its version");
check(M.bodyWithUse(base, "mot-1", 1, false).uses.length === 0, "removing a use drops it");
check(M.bodyWithUse(base, "mot-1", 1, false).motif.notes === "x", "the rest of the body is kept");
check(M.bodyWithUse(null, "mot-1", 1, true).uses.length === 1, "a missing body gets a uses array");
const vreq = M.buildAddVersionRequest("thm-1", added, " Added use ");
check(vreq.url === "/api/materials/items/thm-1/versions" && JSON.parse(vreq.body).note === "Added use", "version request with note");
check(!("note" in JSON.parse(M.buildAddVersionRequest("thm-1", added).body)), "version request without note");

const users = M.usedByRows([{ id: "thm-1", name: "Main", type: "theme", project_id: "prj-b", version: 2 }], projects);
check(users[0].label === "Main (theme) uses v2" && users[0].project === "Etudes", "used by rows");
check(M.usedByRows(null, projects).length === 0, "no users, no rows");

check(M.buildDeleteItemRequest("mot-1", false).url === "/api/materials/items/mot-1", "delete without force");
check(M.buildDeleteItemRequest("mot-1", true).url === "/api/materials/items/mot-1?force=1", "delete with force");
check(M.buildDeleteItemRequest("mot-1", true).method === "DELETE", "delete uses DELETE");
const warn = M.deleteWarning("Hook", 409, { ok: false, error: "item is used", used_by: [{ id: "a", name: "Main" }, { id: "b" }] });
check(warn && warn.users.join() === "Main,b" && warn.message.includes("used by 2 items: Main, b"), "warning names the users");
check(M.deleteWarning("Hook", 409, { used_by: [{ id: "a", name: "Main" }] }).message.includes("1 item:"), "warning uses the singular");
check(M.deleteWarning("Hook", 404, { error: "not found" }) === null, "another failure is not the warning");
check(M.deleteWarning("Hook", 409, null) === null, "a 409 without users is not the warning");

const renderBody = {
  motif: { role: "accompaniment", form: "full", meter: "3/4", key: "D minor", pickup: "1", degrees: "1:q 3:e 5:e" },
};
const rendered = M.renderRequest(renderBody);
const renderedBody = JSON.parse(rendered.body);
check(rendered.method === "POST" && rendered.url === "/api/materials/render", "render request is a POST to /render");
check(renderedBody.degrees === "1:q 3:e 5:e" && renderedBody.key === "D minor" && renderedBody.pickup === "1", "render request carries the stored motif");
check(!("render_key" in renderedBody) && renderedBody.role === "accompaniment", "render request carries the role and no render key");
const plain = JSON.parse(M.renderRequest({ motif: { form: "rhythm", degrees: "x:q x:q", meter: "4/4", key: null } }).body);
check(!("key" in plain) && !("render_key" in plain) && plain.pickup === "0" && plain.role === "melodic", "render request leaves out a missing key and defaults the pickup and role");
check(M.RENDER_KEYS === undefined, "the render keys are gone");
const urls = M.versionScoreUrls("mot-1", 2, "D minor");
check(urls.musicxmlUrl === "/api/materials/items/mot-1/versions/2/score", "score url has no key query");
check(urls.playbackUrl === "/api/materials/items/mot-1/versions/2/playback", "playback url has no key query");

const random = M.randomRequest({ mode: "minor", meter: " 3/4 ", key: "", form: "contour", style_id: "", count: "", seed: "" });
const randomBody = JSON.parse(random.body);
check(random.method === "POST" && random.url === "/api/materials/motifs/random", "random request is a POST to /motifs/random");
check(randomBody.mode === "minor" && randomBody.meter === "3/4" && randomBody.form === "contour" && randomBody.count === 6, "random request defaults the count and trims the meter");
check(randomBody.key === "C minor" && !("style_id" in randomBody) && !("seed" in randomBody), "random request draws in C minor and leaves out an empty style and seed");
const randomFull = JSON.parse(M.randomRequest({ mode: "major", meter: "4/4", key: "D major", form: "full", style_id: "sty-1", count: 20, seed: "7" }).body);
check(randomFull.key === "C major" && randomFull.style_id === "sty-1" && randomFull.seed === 7 && randomFull.count === 16, "random request always draws in C, carries the style and seed and caps the count");
check(M.randomProblems({ meter: "4/4", seed: "", count: "6" }).length === 0, "good random values have no problems");
check(M.randomProblems({ meter: "four", seed: "x", count: "99" }).map((p) => p.field).join() === "meter,seed,count", "each bad random value is reported");

const candidate = {
  seed: 3,
  motive: {
    motive: { key: { tonic_pitch_class: 3, mode: "minor" } },
    analysis: { notes_text: "c4:q eb4:e g4:e", degrees_text: "1:q b3:e 5:e", rhythm_text: "x:q x:e x:e", pickup: "1" },
  },
};
check(M.candidateKeyText(candidate) === "Eb minor", "the candidate key is written as the Composer writes keys");
const saved = M.randomCandidateToBody(candidate, { form: "full", meter: "3/4", role: "accompaniment" });
check(saved.motif.notes === "c4:q eb4:e g4:e" && saved.motif.key === "Eb minor" && saved.motif.meter === "3/4", "a saved random motif keeps the notes, key and meter");
check(saved.motif.pickup === "1" && saved.motif.role === "accompaniment" && saved.motif.form === "full" && saved.motif.degrees === null, "a saved random motif keeps the pickup and role and leaves the degrees to the backend");
const savedRhythm = M.randomCandidateToBody(candidate, { form: "rhythm", meter: "4/4" });
check(savedRhythm.motif.notes === "x:q x:e x:e" && savedRhythm.motif.role === "melodic", "a rhythm candidate is saved from its rhythm text");
check(M.randomCandidateToBody({ motive: { analysis: {} } }, {}).motif.key === null, "a candidate without a key saves a null key");
const drawnMotive = { motive: { key: { tonic_pitch_class: 0, mode: "major" }, notes: [] }, analysis: { notes_text: "C4:q E4:e G4:e", degrees_text: "1:q 3:e 5:e", pickup: "0" } };
const wrapped = M.randomCandidate(drawnMotive, 5);
check(wrapped.seed === 5 && wrapped.motive.analysis.notes_text === "C4:q E4:e G4:e" && M.candidateKeyText(wrapped) === "C major", "a motive as the Composer draws it is wrapped for the page");
check(M.randomCandidateToBody(wrapped, { form: "full", meter: "4/4" }).motif.notes === "C4:q E4:e G4:e", "a drawn candidate gives its notes to the editor");
check(M.randomCandidate(candidate.motive ? candidate : null, 7).motive.analysis.notes_text === "c4:q eb4:e g4:e", "an already wrapped candidate is kept");
check(M.rerollSeed(3) === 4 && M.rerollSeed(undefined) === 0, "reroll uses the next seed");

check(M.analysisRows({ analysis: null, analysis_error: "composer not found" }).message === "Not analyzed: composer not found", "the stored reason is read when no argument is given");
check(M.analysisRows({ analysis: null, analysis_error: "stored" }, "given").message === "Not analyzed: given", "an argument wins over the stored reason");
check(M.analysisRows({ analysis: null, analysis_error: null }).message === "Not analyzed", "no stored reason gives a bare status");
check(M.analysisRows(heard).message === "Analyzed", "an analyzed motif reads Analyzed");
check(M.buildReanalyzeRequest("mot-1").url === "/api/materials/items/mot-1/reanalyze" && M.buildReanalyzeRequest("mot-1").method === "POST", "re-analyze posts to the item's reanalyze route");
check(M.composerAvailable({ ok: true, data: { available: true } }) && !M.composerAvailable({ ok: true, data: { available: false } }) && !M.composerAvailable({ ok: false }), "the Composer counts as available only when the status says so");

/* The motif tabs, the single Save and the Used by table (P18 B3). */
check(same(M.MOTIF_TABS.map((t) => t.label), ["Motif", "Variants", "Used by", "Other"]), "four motif tabs in order");
check(M.readMotifTab() === "motif", "the motif tab falls back to Motif without storage");
global.localStorage = { getItem: () => "other", setItem: () => {} };
check(M.readMotifTab() === "other", "a remembered tab is read");
global.localStorage = { getItem: () => "nope", setItem: () => {} };
check(M.readMotifTab() === "motif", "an unknown remembered tab falls back to Motif");
global.localStorage = {
  getItem: () => {
    throw new Error("blocked");
  },
  setItem: () => {
    throw new Error("blocked");
  },
};
check(M.readMotifTab() === "motif", "a throwing storage falls back to Motif");
let wrote = true;
try {
  M.writeMotifTab("variants");
} catch (err) {
  wrote = false;
}
check(wrote, "a throwing storage does not break writing the tab");
delete global.localStorage;

const stored = {
  uses: [],
  motif: {
    role: "melodic",
    form: "full",
    notes: "C4:q E4:e G4:e",
    meter: "4/4",
    key: "C major",
    pickup: "0",
    harmony: null,
    features: [{ label: "head", first: 0, last: 1 }],
    analysis: { analysis: {} },
    degrees: "1:q 3:e 5:e",
    derived: null,
  },
};
const editorValues = () => M.motifFormValues(stored);
check(!M.motifBodyChanged(editorValues(), stored), "an untouched editor is not a change");
check(M.motifBodyChanged({ ...editorValues(), notes: "C4:q E4:e A4:e" }, stored), "other notes are a change");
check(M.motifBodyChanged({ ...editorValues(), mode: "minor" }, stored), "another mode is a change");
check(M.motifBodyChanged({ ...editorValues(), features: [{ label: "tail", first: 1, last: 2 }] }, stored), "another segment is a change");
check(!M.motifBodyChanged({ ...editorValues(), notes: "  C4:q E4:e G4:e  " }, stored), "spaces around the notes are not a change");
check(!M.motifBodyChanged({ ...editorValues(), pickup: "" }, { motif: { ...stored.motif, pickup: "0" } }), "an empty pickup reads as 0");
const legacy = { uses: [], motif: { ...stored.motif, key: "D minor" } };
check(M.motifBodyChanged({ ...M.motifFormValues(legacy), legacyKey: null }, legacy), "rewriting an older motif in C is a change");
check(!M.motifBodyChanged(M.motifFormValues(legacy), legacy), "an older motif left alone is not a change");

check(M.motifSaveDecision({}, "x").kind === "none" && M.motifSaveDecision({}, "x").message === "Nothing to save.", "no change says Nothing to save.");
check(
  M.motifSaveDecision({ body: true }, "").kind === "blocked" && M.motifSaveDecision({ body: true }, "  ").message === "Give a version note.",
  "a body change without a note is blocked",
);
check(M.motifSaveDecision({ body: true }, "Shorter").kind === "save", "a body change with a note saves");
check(M.motifSaveDecision({ name: true }, "").kind === "save" && M.motifSaveDecision({ project: true }, "").kind === "save", "a rename or move needs no note");
check(M.motifSaveDecision({ nothing: true }, "note").kind === "none", "a note alone is not a change");

const createdRequest = M.buildSaveMotifRequest({ name: " New ", project_id: "prj-a", note: "Created", body: stored });
check(createdRequest.url === "/api/materials/items" && JSON.parse(createdRequest.body).note === "Created", "a new motif sends the Created note");
check(!("note" in JSON.parse(M.buildSaveMotifRequest({ name: "n", project_id: "prj-a", body: stored }).body)), "a new motif without a note sends none");

const usedByUsers = [
  { id: "t1", name: "Theme", type: "theme", project_id: "prj-a", version: 1 },
  { id: "t2", name: "Piece", type: "piece", project_id: "prj-b", version: 3 },
];
const table = M.usedByTable(usedByUsers, projects, 3);
check(table.length === 2 && table[0].updateAvailable && !table[1].updateAvailable, "a use behind the current version has an update available");
check(table[0].project === "Sonata" && table[0].version === 1 && table[0].type === "theme", "the Used by row carries project, version and type");
check(M.usedByTable(null, projects, 1).length === 0, "no usedByUsers, no rows");

check(M.errorField("Segment 1: the label is 1 to 8 characters.") === "features", "a segment problem belongs to the segments field");
check(M.motifFormProblems({ ...editorValues(), features: [{ label: "", first: 1, last: 1 }] }).some((p) => /^Segment 1:/.test(p.message)), "segment problems are named Segment");
check(/head and tail/.test(M.SEGMENTS_HINT) && /Liquidation and motive-forms/.test(M.SEGMENTS_HINT), "the segments hint names the use");

if (failures > 0) {
  process.exit(1);
}
