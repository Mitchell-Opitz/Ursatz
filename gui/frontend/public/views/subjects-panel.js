/* Sidebar Subjects panel (R-117): subjects grouped by kind with their link
 * counts, collapsible like the Tags panel. groupRows and kindLabel are pure;
 * mount renders into the page. */

import Req from "../core/request.js";
import { commandRequest } from "../core/commands.js";
import GroupedPanel from "./grouped-panel.js";

const OPEN_KEY = "ursatz.subjectsPanel.open";
const KINDS = [
  { kind: "character", title: "Characters" },
  { kind: "place", title: "Places" },
  { kind: "idea", title: "Ideas" },
  { kind: "note", title: "Notes" },
];

function kindLabel(kind) {
  const found = KINDS.find((entry) => entry.kind === kind);
  return found ? found.title : String(kind || "");
}

/* Rows {id, name, count} of one kind from the groups route's data (an
 * array of {kind, subjects}); count is the number of links. */
function groupRows(groups, kind) {
  const group = (Array.isArray(groups) ? groups : []).find((entry) => entry.kind === kind);
  const subjects = group && Array.isArray(group.subjects) ? group.subjects : [];
  return subjects.map((subject) => ({
    id: subject.id,
    name: subject.name,
    count: Number(subject.link_count) || 0,
  }));
}

/* Renders the panel into el (a section holding the "Subjects" label) and
 * returns {refresh()}. fetchGroups resolves to the groups array and
 * rejects (or resolves null) on failure; onSelect(subjectId) opens one. */
function mount(el, options) {
  const opts = options || {};
  const onSelect = opts.onSelect || function () {};
  const panel = GroupedPanel.mount(el, {
    openKey: OPEN_KEY,
    kinds: KINDS,
    unavailable: "Subjects unavailable",
    fetchGroups: opts.fetchGroups,
    rows: groupRows,
    buildRow: (row) => GroupedPanel.buildPanelRow(row.name, row.count, () => onSelect(row.id), false),
  });
  return { refresh: panel.refresh };
}

const SUBJECTS_URL = "/api/subjects";

function buildSubjectListRequest() {
  return Req.jsonRequest("GET", SUBJECTS_URL);
}

function buildSubjectGetRequest(id) {
  return Req.jsonRequest("GET", `${SUBJECTS_URL}/${encodeURIComponent(id)}`);
}

/* fields: name, kind, note; the note is left out when empty. */
function buildCreateSubjectRequest(fields) {
  const f = fields || {};
  const body = { name: String(f.name || "").trim(), kind: f.kind };
  if (f.note && String(f.note).trim() !== "") {
    body.note = String(f.note).trim();
  }
  return commandRequest("subject.create", { params: body });
}

/* Only the fields that are given go in; an empty note is sent so it can be cleared. */
function buildUpdateSubjectRequest(id, fields) {
  const f = fields || {};
  const body = {};
  if (typeof f.name === "string" && f.name.trim() !== "") {
    body.name = f.name.trim();
  }
  if (typeof f.kind === "string" && f.kind !== "") {
    body.kind = f.kind;
  }
  if (typeof f.note === "string") {
    body.note = f.note.trim();
  }
  return commandRequest("subject.update", { target: id, params: body });
}

function buildDeleteSubjectRequest(id) {
  return commandRequest("subject.delete", { target: id });
}

/* group is "items", "pieces" or "related"; add true links, false removes. */
function buildLinkRequest(subjectId, group, otherId, add) {
  const names = {
    items: { add: "subject.link-item", remove: "subject.unlink-item", param: "item_id" },
    pieces: { add: "subject.link-piece", remove: "subject.unlink-piece", param: "piece_id" },
    related: { add: "subject.relate", remove: "subject.unrelate", param: "other_id" },
  };
  const kind = names[group];
  return commandRequest(add ? kind.add : kind.remove, { target: subjectId, params: { [kind.param]: otherId } });
}

function kindOrder(kind) {
  const index = KINDS.findIndex((entry) => entry.kind === kind);
  return index < 0 ? KINDS.length : index;
}

/* Rows for the Subjects list: by kind, then name. */
function subjectRows(subjects) {
  return (Array.isArray(subjects) ? subjects : [])
    .map((subject) => ({
      id: subject.id,
      name: subject.name,
      kind: subject.kind,
      kindLabel: kindLabel(subject.kind),
      items: Number(subject.item_count) || 0,
      pieces: Number(subject.piece_count) || 0,
      related: Number(subject.related_count) || 0,
    }))
    .sort((a, b) => kindOrder(a.kind) - kindOrder(b.kind) || a.name.localeCompare(b.name));
}

/* Options {value, label} for a picker over the subjects not in excludeIds. */
function pickerOptions(subjects, excludeIds) {
  const skip = new Set(excludeIds || []);
  return subjectRows(subjects)
    .filter((row) => !skip.has(row.id))
    .map((row) => ({ value: row.id, label: `${row.name} (${row.kindLabel})` }));
}

/* Related subjects of one subject as {id, label}; an id that is no longer in the list is skipped. */
function relatedRows(ids, subjects) {
  const byId = new Map((Array.isArray(subjects) ? subjects : []).map((subject) => [subject.id, subject]));
  return (ids || [])
    .filter((id) => byId.has(id))
    .map((id) => ({ id, label: `${byId.get(id).name} (${kindLabel(byId.get(id).kind)})` }));
}

/* Linked items as {id, name, type, missing}; items is the material item list. */
function itemLinkRows(ids, items) {
  const byId = new Map((Array.isArray(items) ? items : []).map((item) => [item.id, item]));
  return (ids || []).map((id) => {
    const item = byId.get(id);
    return item
      ? { id, name: item.name, type: item.type, missing: false }
      : { id, name: id, type: "", missing: true };
  });
}

/* Linked Library pieces as {id, name}; pieces is the Library list. */
function pieceLinkRows(ids, pieces) {
  const byId = new Map((Array.isArray(pieces) ? pieces : []).map((piece) => [piece.id, piece]));
  return (ids || []).map((id) => {
    const piece = byId.get(id);
    return { id, name: piece ? piece.filename || id : id };
  });
}

/* Chips {id, label, title} for the subjects linked to an item or piece
 * (the {id, name, kind} rows the item and piece answers carry). */
function chipRows(subjects) {
  return (Array.isArray(subjects) ? subjects : []).map((subject) => ({
    id: subject.id,
    label: subject.name,
    title: kindLabel(subject.kind),
  }));
}

/* A subject list with subject added (by id, no duplicates) or removed. Used
 * for the pending list of an item that is not saved yet. */
function withSubject(list, subject) {
  const current = Array.isArray(list) ? list : [];
  return current.some((entry) => entry.id === subject.id)
    ? current.slice()
    : [...current, { id: subject.id, name: subject.name, kind: subject.kind }];
}

function withoutSubject(list, subjectId) {
  return (Array.isArray(list) ? list : []).filter((entry) => entry.id !== subjectId);
}

const api = {
  KINDS,
  kindLabel,
  groupRows,
  mount,
  buildSubjectListRequest,
  buildSubjectGetRequest,
  buildCreateSubjectRequest,
  buildUpdateSubjectRequest,
  buildDeleteSubjectRequest,
  buildLinkRequest,
  subjectRows,
  pickerOptions,
  relatedRows,
  itemLinkRows,
  pieceLinkRows,
  chipRows,
  withSubject,
  withoutSubject,
  errorText: Req.errorText,
};
export default api;
