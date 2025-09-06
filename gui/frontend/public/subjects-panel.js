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

  const api = { KINDS, kindLabel, groupRows, mount };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.SubjectsPanel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
