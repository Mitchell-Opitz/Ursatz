/* The grouped panel of the sidebar (Tags and Subjects): one collapsible group per kind, the open groups remembered in the page's storage, a note
 * when the groups cannot be read, and a refresh that reads them again. The panels give what differs: the rows of a group, the row's own button and an
 * optional footer (the Tags panel's Show all). */

function readOpenState(key, kinds) {
  try {
    const saved = JSON.parse(localStorage.getItem(key));
    if (Array.isArray(saved)) {
      return new Set(saved);
    }
  } catch (err) {
    // No storage or bad JSON: fall back to all groups open.
  }
  return new Set(kinds.map((entry) => entry.kind));
}

function writeOpenState(key, open) {
  try {
    localStorage.setItem(key, JSON.stringify([...open]));
  } catch (err) {
    // Storage unavailable: the state just is not remembered.
  }
}

/* The row of a group: a list item with the label and count button; li.appendChild may add more to it. */
function buildPanelRow(label, count, onClick, active) {
  const li = document.createElement("li");
  li.className = "tags-panel-item";
  const button = document.createElement("button");
  button.type = "button";
  button.className = "tags-panel-row";
  if (active) {
    button.setAttribute("aria-current", "true");
  }
  const labelEl = document.createElement("span");
  labelEl.className = "tags-panel-label";
  labelEl.textContent = label;
  const countEl = document.createElement("span");
  countEl.className = "tags-panel-count";
  countEl.textContent = String(count);
  button.append(labelEl, countEl);
  button.addEventListener("click", onClick);
  li.appendChild(button);
  return li;
}

/* Renders into host (a section holding the panel's label) and returns {refresh(), render(), loaded()}. config:
 *   openKey, kinds [{kind, title}], unavailable (the note text), fetchGroups() (resolves to the groups, rejects or resolves null on failure),
 *   rows(groups, kind) -> rows, buildRow(row) -> li, footer(groups, kind, render) -> element or null. */
function mount(host, config) {
  const open = readOpenState(config.openKey, config.kinds);
  let body = host.querySelector(".tags-panel-body");
  if (!body) {
    body = document.createElement("div");
    body.className = "tags-panel-body";
    host.appendChild(body);
  }
  let groups = null;

  function render() {
    body.innerHTML = "";
    if (!groups) {
      const note = document.createElement("p");
      note.className = "tags-panel-note";
      note.textContent = config.unavailable;
      body.appendChild(note);
      return;
    }
    for (const entry of config.kinds) {
      const details = document.createElement("details");
      details.className = "tags-panel-group";
      details.open = open.has(entry.kind);
      details.addEventListener("toggle", () => {
        if (details.open) {
          open.add(entry.kind);
        } else {
          open.delete(entry.kind);
        }
        writeOpenState(config.openKey, open);
      });
      const summary = document.createElement("summary");
      summary.textContent = entry.title;
      details.appendChild(summary);

      const rows = config.rows(groups, entry.kind);
      if (rows.length === 0) {
        const none = document.createElement("p");
        none.className = "tags-panel-note";
        none.textContent = "None yet";
        details.appendChild(none);
      } else {
        const list = document.createElement("ul");
        list.className = "tags-panel-list";
        for (const row of rows) {
          list.appendChild(config.buildRow(row));
        }
        details.appendChild(list);
      }

      const footer = config.footer ? config.footer(groups, entry.kind, render) : null;
      if (footer) {
        details.appendChild(footer);
      }
      body.appendChild(details);
    }
  }

  async function refresh() {
    try {
      groups = (await config.fetchGroups()) || null;
    } catch (err) {
      groups = null;
    }
    render();
  }

  return { refresh, render, loaded: () => groups !== null };
}

export default { mount, buildPanelRow };
