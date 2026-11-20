/* Covers the pure Subjects helpers (public/subjects-panel.js): panel rows by
 * kind, list rows, pickers, chips, link rows and request builders.
 * Plain Node script: `node frontend/tests/subjects-panel.test.js`. */
import S from "../public/views/subjects-panel.js";
import { check, same, finish } from "./support/check.js";
import { installDom } from "./support/dom.js";
import { params, isCommand } from "./support/command.js";

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
check(isCommand(create, "subject.create"), "create is subject.create");
check(same(params(create), { name: "Hero", kind: "character" }), "create trims and leaves out an empty note");
check(params(S.buildCreateSubjectRequest({ name: "a", kind: "idea", note: " x " })).note === "x", "create keeps a note");
const update = S.buildUpdateSubjectRequest("sub-1", { name: " B ", kind: "place", note: "" });
check(isCommand(update, "subject.update", "sub-1"), "update is subject.update of the subject");
check(same(params(update), { name: "B", kind: "place", note: "" }), "update sends an empty note to clear it");
check(!("name" in params(S.buildUpdateSubjectRequest("sub-1", { name: " " }))), "update skips an empty name");
check(isCommand(S.buildDeleteSubjectRequest("sub-1"), "subject.delete", "sub-1"), "delete");
check(S.buildSubjectGetRequest("sub-1").url === "/api/subjects/sub-1" && S.buildSubjectListRequest().url === "/api/subjects", "get and list");
const add = S.buildLinkRequest("sub-1", "items", "mot-1", true);
check(isCommand(add, "subject.link-item", "sub-1") && same(params(add), { item_id: "mot-1" }), "link is subject.link-item");
const drop = S.buildLinkRequest("sub-1", "related", "sub-2", false);
check(isCommand(drop, "subject.unrelate", "sub-1") && same(params(drop), { other_id: "sub-2" }), "unlink is subject.unrelate");
check(S.errorText({ error: "not linked" }) === "not linked" && S.errorText(null, "x") === "x", "error text");

const pending = S.withSubject([], { id: "sub-1", name: "Hero", kind: "character", item_count: 4 });
check(same(pending, [{ id: "sub-1", name: "Hero", kind: "character" }]), "a pending subject keeps its id, name and kind");
check(S.withSubject(pending, { id: "sub-1", name: "Hero", kind: "character" }).length === 1, "a subject is not added twice");
check(S.withSubject(pending, { id: "sub-2", name: "Longing", kind: "idea" }).length === 2, "another subject is added");
check(pending.length === 1, "adding a subject leaves the list alone");
check(S.withoutSubject(pending, "sub-1").length === 0 && S.withoutSubject(pending, "sub-9").length === 1, "a pending subject is removed by id");
check(S.withSubject(undefined, { id: "sub-1", name: "Hero", kind: "character" }).length === 1 && S.withoutSubject(null, "sub-1").length === 0, "no list is an empty list");

/* The mounted panel (the grouped panel it is built from): a group per kind with its subjects, a row opens its subject. */
const { document } = installDom();
const section = document.createElement("section");
document.body.appendChild(section);
const chosen = [];
const panel = S.mount(section, { fetchGroups: async () => groups, onSelect: (id) => chosen.push(id) });
await panel.refresh();
check(same(section.querySelectorAll("details summary").map((el) => el.textContent), ["Characters", "Places", "Ideas", "Notes"]), "the panel has a group for each kind");
check(section.querySelectorAll("details")[0].querySelectorAll("li").length === 1 && section.querySelectorAll("details")[1].textContent.includes("None yet"), "a kind with subjects lists them and one without says None yet");
section.querySelector("li button.tags-panel-row").click();
check(same(chosen, ["sub-1"]), "a row opens its subject");
const failing = document.createElement("section");
await S.mount(failing, { fetchGroups: async () => { throw new Error("down"); } }).refresh();
check(failing.textContent === "Subjects unavailable", "a panel whose groups cannot be read says so");

finish();
