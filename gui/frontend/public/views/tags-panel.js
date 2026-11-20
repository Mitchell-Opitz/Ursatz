
/* Sidebar Tags panel (issue 6): groups pieces by composer, source and tag
 * from the groups route and reports which group was picked. groupRows and
 * isActiveFilter are pure; mount renders into the page. */

import GroupedPanel from "./grouped-panel.js";

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

/* The row of one value: its button and, when onBuildStyle is given, the Build style button beside it. */
function buildRow(row, active, onSelect, onBuildStyle) {
  const li = GroupedPanel.buildPanelRow(row.label, row.count, () => onSelect(row.filter), active);
  if (onBuildStyle) {
    const build = document.createElement("button");
    build.type = "button";
    build.className = "tags-panel-build";
    build.textContent = "♫";
    build.title = "Build style";
    build.setAttribute("aria-label", `Build style from ${row.label}`);
    build.addEventListener("click", () => onBuildStyle(row, build));
    li.appendChild(build);
  }
  return li;
}

/* Renders the panel into el (a section holding the "Tags" label) and
 * returns {refresh(), setActive(filter)}. fetchGroups resolves to the
 * groups route's data and rejects (or resolves null) on failure. When
 * onBuildStyle is given, each row also has a Build style button. */
function mount(el, options) {
  const opts = options || {};
  const onSelect = opts.onSelect || function () {};
  const onBuildStyle = opts.onBuildStyle || null;
  const showAll = new Set();
  let active = null;
  const panel = GroupedPanel.mount(el, {
    openKey: OPEN_KEY,
    kinds: KINDS,
    unavailable: "Tags unavailable",
    fetchGroups: opts.fetchGroups,
    rows: (groups, kind) => groupRows(groups, kind, { showAll: showAll.has(kind) }),
    buildRow: (row) => buildRow(row, isActiveFilter(row.filter, active), onSelect, onBuildStyle),
    footer(groups, kind, render) {
      const total = valueCount(groups, kind);
      if (total <= DEFAULT_LIMIT || showAll.has(kind)) {
        return null;
      }
      const more = document.createElement("button");
      more.type = "button";
      more.className = "tags-panel-more";
      more.textContent = `Show all (${total})`;
      more.addEventListener("click", () => {
        showAll.add(kind);
        render();
      });
      return more;
    },
  });

  function setActive(filter) {
    active = filter || null;
    if (panel.loaded()) {
      panel.render();
    }
  }

  return { refresh: panel.refresh, setActive };
}

const api = { groupRows, valueCount, isActiveFilter, mount };
export default api;
