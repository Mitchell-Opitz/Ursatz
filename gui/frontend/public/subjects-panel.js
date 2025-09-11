/* Sidebar Subjects panel (R-117): subjects grouped by kind with their link
 * counts, collapsible like the Tags panel. groupRows and kindLabel are pure;
 * mount renders into the page. */
(function (root) {
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

  function readOpenState() {
    try {
      const saved = JSON.parse(localStorage.getItem(OPEN_KEY));
      if (Array.isArray(saved)) {
        return new Set(saved);
      }
    } catch (err) {
      // No storage or bad JSON: fall back to all groups open.
    }
    return new Set(KINDS.map((entry) => entry.kind));
  }

  function writeOpenState(open) {
    try {
      localStorage.setItem(OPEN_KEY, JSON.stringify([...open]));
    } catch (err) {
      // Storage unavailable: the state just is not remembered.
    }
  }

  function buildRow(row, onSelect) {
    const li = document.createElement("li");
    li.className = "tags-panel-item";
    const button = document.createElement("button");
    button.type = "button";
    button.className = "tags-panel-row";
    const label = document.createElement("span");
    label.className = "tags-panel-label";
    label.textContent = row.name;
    const count = document.createElement("span");
    count.className = "tags-panel-count";
    count.textContent = String(row.count);
    button.append(label, count);
    button.addEventListener("click", () => onSelect(row.id));
    li.appendChild(button);
    return li;
  }

  /* Renders the panel into el (a section holding the "Subjects" label) and
   * returns {refresh()}. fetchGroups resolves to the groups array and
   * rejects (or resolves null) on failure; onSelect(subjectId) opens one. */
  function mount(el, options) {
    const opts = options || {};
    const onSelect = opts.onSelect || function () {};
    const open = readOpenState();
    let body = el.querySelector(".tags-panel-body");
    if (!body) {
      body = document.createElement("div");
      body.className = "tags-panel-body";
      el.appendChild(body);
    }
    let groups = null;

    function render() {
      body.innerHTML = "";
      if (!groups) {
        const note = document.createElement("p");
        note.className = "tags-panel-note";
        note.textContent = "Subjects unavailable";
        body.appendChild(note);
        return;
      }
      for (const entry of KINDS) {
        const details = document.createElement("details");
        details.className = "tags-panel-group";
        details.open = open.has(entry.kind);
        details.addEventListener("toggle", () => {
          if (details.open) {
            open.add(entry.kind);
          } else {
            open.delete(entry.kind);
          }
          writeOpenState(open);
        });
        const summary = document.createElement("summary");
        summary.textContent = entry.title;
        details.appendChild(summary);
        const rows = groupRows(groups, entry.kind);
        if (rows.length === 0) {
          const none = document.createElement("p");
          none.className = "tags-panel-note";
          none.textContent = "None yet";
          details.appendChild(none);
        } else {
          const list = document.createElement("ul");
          list.className = "tags-panel-list";
          for (const row of rows) {
            list.appendChild(buildRow(row, onSelect));
          }
          details.appendChild(list);
        }
        body.appendChild(details);
      }
    }

    async function refresh() {
      try {
        groups = (await opts.fetchGroups()) || null;
      } catch (err) {
        groups = null;
      }
      render();
    }

    return { refresh };
  }

  function jsonRequest(method, url, body) {
    const request = { method, url };
    if (body !== undefined) {
      request.headers = { "Content-Type": "application/json" };
      request.body = JSON.stringify(body);
    }
    return request;
  }

  const SUBJECTS_URL = "/api/subjects";

  function buildSubjectListRequest() {
    return jsonRequest("GET", SUBJECTS_URL);
  }

  function buildSubjectGetRequest(id) {
    return jsonRequest("GET", `${SUBJECTS_URL}/${encodeURIComponent(id)}`);
  }

  /* fields: name, kind, note; the note is left out when empty. */
  function buildCreateSubjectRequest(fields) {
    const f = fields || {};
    const body = { name: String(f.name || "").trim(), kind: f.kind };
    if (f.note && String(f.note).trim() !== "") {
      body.note = String(f.note).trim();
    }
    return jsonRequest("POST", SUBJECTS_URL, body);
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
    return jsonRequest("PATCH", `${SUBJECTS_URL}/${encodeURIComponent(id)}`, body);
  }

  function buildDeleteSubjectRequest(id) {
    return jsonRequest("DELETE", `${SUBJECTS_URL}/${encodeURIComponent(id)}`);
  }

  /* group is "items", "pieces" or "related"; add true links, false removes. */
  function buildLinkRequest(subjectId, group, otherId, add) {
    return jsonRequest(
      add ? "POST" : "DELETE",
      `${SUBJECTS_URL}/${encodeURIComponent(subjectId)}/${group}/${encodeURIComponent(otherId)}`,
    );
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

  /* The error text of a failed answer, or the fallback. */
  function errorText(data, fallback) {
    if (data && typeof data.error === "string" && data.error !== "") {
      return data.error;
    }
    return fallback || "Request failed";
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
    errorText,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.SubjectsPanel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
