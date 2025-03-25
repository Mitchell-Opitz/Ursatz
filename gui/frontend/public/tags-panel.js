/* Sidebar Tags panel (issue 6): groups pieces by composer, source and tag
 * from the groups route and reports which group was picked. groupRows and
 * isActiveFilter are pure; mount renders into the page. */
(function (root) {
  const DEFAULT_LIMIT = 12;
  const OPEN_KEY = "ursatz.tagsPanel.open";
  const KINDS = [
    { kind: "composer", title: "Composer" },
    { kind: "source", title: "Source" },
    { kind: "tag", title: "Tags" },
  ];

  function groupList(groups, kind) {
    const g = groups || {};
    const list = kind === "composer" ? g.composers : kind === "source" ? g.sources : g.tags;
    return Array.isArray(list) ? list : [];
  }

  function valueFilter(kind, value) {
    return kind === "composer" ? { composer: value } : kind === "source" ? { source: value } : { tag: value };
  }

  /* The number of distinct values in a group, before the limit. */
  function valueCount(groups, kind) {
    return groupList(groups, kind).length;
  }

  /* Rows {label, count, filter} for one group: the values by count (the top
   * `limit`, 12 by default, unless showAll), then "Unassigned" (composer,
   * source) or "Untagged" (tags) when its count is above 0. */
  function groupRows(groups, kind, options) {
    const opts = options || {};
    const limit = opts.limit > 0 ? opts.limit : DEFAULT_LIMIT;
    const values = groupList(groups, kind)
      .map((item, index) => ({ item, index }))
      .sort((a, b) => b.item.count - a.item.count || a.index - b.index)
      .map((entry) => entry.item);
    const shown = opts.showAll ? values : values.slice(0, limit);
    const rows = shown.map((item) => ({
      label: item.value,
      count: item.count,
      filter: valueFilter(kind, item.value),
    }));
    const g = groups || {};
    if (kind === "tag") {
      if (g.untagged > 0) {
        rows.push({ label: "Untagged", count: g.untagged, filter: { untagged: true } });
      }
    } else {
      const unassigned = g.unassigned ? g.unassigned[kind] : 0;
      if (unassigned > 0) {
        rows.push({ label: "Unassigned", count: unassigned, filter: { unassigned: [kind] } });
      }
    }
    return rows;
  }

  function lower(text) {
    return String(text || "").toLowerCase();
  }

  /* True when a row's filter is exactly the Library's current filter on
   * composer, source, tag, unassigned and untagged (values compare
   * case-insensitively). Search and sort do not count. */
  function isActiveFilter(rowFilter, current) {
    if (!rowFilter || !current) {
      return false;
    }
    const sorted = (list) => [...(list || [])].sort().join(",");
    return (
      lower(rowFilter.composer) === lower(current.composer) &&
      lower(rowFilter.source) === lower(current.source) &&
      lower(rowFilter.tag) === lower(current.tag) &&
      sorted(rowFilter.unassigned) === sorted(current.unassigned) &&
      Boolean(rowFilter.untagged) === Boolean(current.untagged)
    );
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

  function buildRow(row, active, onSelect) {
    const li = document.createElement("li");
    const button = document.createElement("button");
    button.type = "button";
    button.className = "tags-panel-row";
    if (active) {
      button.setAttribute("aria-current", "true");
    }
    const label = document.createElement("span");
    label.className = "tags-panel-label";
    label.textContent = row.label;
    const count = document.createElement("span");
    count.className = "tags-panel-count";
    count.textContent = String(row.count);
    button.append(label, count);
    button.addEventListener("click", () => onSelect(row.filter));
    li.appendChild(button);
    return li;
  }

  /* Renders the panel into el (a section holding the "Tags" label) and
   * returns {refresh(), setActive(filter)}. fetchGroups resolves to the
   * groups route's data and rejects (or resolves null) on failure. */
  function mount(el, options) {
    const opts = options || {};
    const onSelect = opts.onSelect || function () {};
    const open = readOpenState();
    const showAll = new Set();
    let body = el.querySelector(".tags-panel-body");
    if (!body) {
      body = document.createElement("div");
      body.className = "tags-panel-body";
      el.appendChild(body);
    }
    let groups = null;
    let active = null;

    function render() {
      body.innerHTML = "";
      if (!groups) {
        const note = document.createElement("p");
        note.className = "tags-panel-note";
        note.textContent = "Tags unavailable";
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

        const rows = groupRows(groups, entry.kind, { showAll: showAll.has(entry.kind) });
        if (rows.length === 0) {
          const none = document.createElement("p");
          none.className = "tags-panel-note";
          none.textContent = "None yet";
          details.appendChild(none);
        } else {
          const list = document.createElement("ul");
          list.className = "tags-panel-list";
          for (const row of rows) {
            list.appendChild(buildRow(row, isActiveFilter(row.filter, active), onSelect));
          }
          details.appendChild(list);
        }

        const total = valueCount(groups, entry.kind);
        if (total > DEFAULT_LIMIT && !showAll.has(entry.kind)) {
          const more = document.createElement("button");
          more.type = "button";
          more.className = "tags-panel-more";
          more.textContent = `Show all (${total})`;
          more.addEventListener("click", () => {
            showAll.add(entry.kind);
            render();
          });
          details.appendChild(more);
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

    function setActive(filter) {
      active = filter || null;
      if (groups) {
        render();
      }
    }

    return { refresh, setActive };
  }

  const api = { groupRows, valueCount, isActiveFilter, mount };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.TagsPanel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
