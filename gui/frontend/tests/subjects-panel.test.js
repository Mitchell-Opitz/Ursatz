/* Covers the pure Subjects helpers (public/subjects-panel.js): panel rows by
 * kind, list rows, pickers, chips, link rows and request builders.
 * Plain Node script: `node frontend/tests/subjects-panel.test.js`. */
const S = require("../public/subjects-panel.js");

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

const groups = [
  { kind: "character", subjects: [{ id: "sub-1", name: "Hero", item_count: 2, piece_count: 1, link_count: 3 }] },
  { kind: "place", subjects: [] },
  { kind: "idea", subjects: [{ id: "sub-2", name: "Longing", link_count: 0 }] },
  { kind: "note", subjects: [] },
];

check(same(S.KINDS.map((k) => k.title), ["Characters", "Places", "Ideas", "Notes"]), "four kinds in order");
check(S.kindLabel("place") === "Places" && S.kindLabel("zzz") === "zzz", "kind labels");
check(same(S.groupRows(groups, "character"), [{ id: "sub-1", name: "Hero", count: 3 }]), "panel row carries the link count");
check(S.groupRows(groups, "place").length === 0, "an empty kind has no rows");
check(S.groupRows(groups, "idea")[0].count === 0, "a subject without links counts 0");
check(S.groupRows(null, "idea").length === 0 && S.groupRows([], "idea").length === 0, "no groups, no rows");

const list = [
  { id: "sub-3", name: "Zed", kind: "idea", item_count: 0, piece_count: 0, related_count: 1 },
  { id: "sub-1", name: "Hero", kind: "character", item_count: 2, piece_count: 1, related_count: 0 },
  { id: "sub-4", name: "Anna", kind: "character", item_count: 0, piece_count: 0, related_count: 0 },
];
const rows = S.subjectRows(list);
check(same(rows.map((r) => r.id), ["sub-4", "sub-1", "sub-3"]), "list rows by kind then name");
check(rows[1].items === 2 && rows[1].pieces === 1 && rows[1].kindLabel === "Characters", "list row counts and kind");
check(S.subjectRows(null).length === 0, "no list, no rows");

const options = S.pickerOptions(list, ["sub-1"]);
check(same(options.map((o) => o.value), ["sub-4", "sub-3"]), "picker leaves out linked subjects");
check(options[0].label === "Anna (Characters)", "picker label has the kind");
check(S.pickerOptions(list, ["sub-1", "sub-3", "sub-4"]).length === 0, "nothing left to pick");

check(
  same(S.relatedRows(["sub-1", "sub-gone"], list), [{ id: "sub-1", label: "Hero (Characters)" }]),
  "related rows skip a subject that is gone",
);
check(same(S.chipRows([{ id: "sub-1", name: "Hero", kind: "character" }]), [{ id: "sub-1", label: "Hero", title: "Characters" }]), "chips");
check(S.chipRows(undefined).length === 0, "no subjects, no chips");

const itemRows = S.itemLinkRows(["mot-1", "mot-9"], [{ id: "mot-1", name: "Hook", type: "motif" }]);
check(itemRows[0].name === "Hook" && itemRows[0].type === "motif" && !itemRows[0].missing, "linked item row");
check(itemRows[1].missing && itemRows[1].name === "mot-9", "a missing item is marked");
const pieceRows = S.pieceLinkRows(["p1", "p2"], [{ id: "p1", filename: "Nocturne" }]);
check(pieceRows[0].name === "Nocturne" && pieceRows[1].name === "p2", "linked piece rows fall back to the id");

const create = S.buildCreateSubjectRequest({ name: " Hero ", kind: "character", note: " " });
check(create.method === "POST" && create.url === "/api/subjects", "create posts to subjects");
check(same(JSON.parse(create.body), { name: "Hero", kind: "character" }), "create trims and leaves out an empty note");
check(JSON.parse(S.buildCreateSubjectRequest({ name: "a", kind: "idea", note: " x " }).body).note === "x", "create keeps a note");
const update = S.buildUpdateSubjectRequest("sub-1", { name: " B ", kind: "place", note: "" });
check(update.method === "PATCH" && update.url === "/api/subjects/sub-1", "update patches the subject");
check(same(JSON.parse(update.body), { name: "B", kind: "place", note: "" }), "update sends an empty note to clear it");
check(!("name" in JSON.parse(S.buildUpdateSubjectRequest("sub-1", { name: " " }).body)), "update skips an empty name");
check(S.buildDeleteSubjectRequest("sub-1").method === "DELETE", "delete");
check(S.buildSubjectGetRequest("sub-1").url === "/api/subjects/sub-1" && S.buildSubjectListRequest().url === "/api/subjects", "get and list");
const add = S.buildLinkRequest("sub-1", "items", "mot-1", true);
check(add.method === "POST" && add.url === "/api/subjects/sub-1/items/mot-1", "link posts");
const drop = S.buildLinkRequest("sub-1", "related", "sub-2", false);
check(drop.method === "DELETE" && drop.url === "/api/subjects/sub-1/related/sub-2", "unlink deletes");
check(S.errorText({ error: "not linked" }) === "not linked" && S.errorText(null, "x") === "x", "error text");

const pending = S.withSubject([], { id: "sub-1", name: "Hero", kind: "character", item_count: 4 });
check(same(pending, [{ id: "sub-1", name: "Hero", kind: "character" }]), "a pending subject keeps its id, name and kind");
check(S.withSubject(pending, { id: "sub-1", name: "Hero", kind: "character" }).length === 1, "a subject is not added twice");
check(S.withSubject(pending, { id: "sub-2", name: "Longing", kind: "idea" }).length === 2, "another subject is added");
check(pending.length === 1, "adding a subject leaves the list alone");
check(S.withoutSubject(pending, "sub-1").length === 0 && S.withoutSubject(pending, "sub-9").length === 1, "a pending subject is removed by id");
check(S.withSubject(undefined, { id: "sub-1", name: "Hero", kind: "character" }).length === 1 && S.withoutSubject(null, "sub-1").length === 0, "no list is an empty list");

if (failures > 0) {
  process.exit(1);
}
