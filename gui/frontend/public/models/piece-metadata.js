/* Pure helpers for piece metadata in the Library (issue 6): tag parsing,
 * filter queries, sorting, searching and bulk-edit bodies. No DOM; the
 * backend stays the authority on what a valid value is. */

function parseTagInput(text) {
  const seen = new Set();
  const tags = [];
  for (const part of String(text || "").split(",")) {
    const tag = part.trim().toLowerCase();
    if (tag && !seen.has(tag)) {
      seen.add(tag);
      tags.push(tag);
    }
  }
  return tags;
}

/* The query of the Library list and groups routes for a filter; collection (an id) limits them to that collection's members. */
function filterQuery(filter, collection) {
  const f = filter || {};
  const params = [];
  if (collection) {
    params.push(`collection=${encodeURIComponent(collection)}`);
  }
  if (f.composer) {
    params.push(`composer=${encodeURIComponent(f.composer)}`);
  }
  if (f.source) {
    params.push(`source=${encodeURIComponent(f.source)}`);
  }
  if (f.tag) {
    params.push(`tag=${encodeURIComponent(f.tag)}`);
  }
  for (const which of f.unassigned || []) {
    params.push(`unassigned=${encodeURIComponent(which)}`);
  }
  if (f.untagged) {
    params.push("untagged=1");
  }
  return params.join("&");
}

function titleOf(piece) {
  return String(piece.filename || piece.id || "").toLowerCase();
}

function compareText(a, b) {
  return a < b ? -1 : a > b ? 1 : 0;
}

function sortPieces(pieces, key) {
  const list = pieces.slice();
  if (key === "title") {
    return list.sort((a, b) => compareText(titleOf(a), titleOf(b)));
  }
  if (key === "composer") {
    return list.sort((a, b) => {
      const ca = String(a.composer || "").toLowerCase();
      const cb = String(b.composer || "").toLowerCase();
      if (!ca !== !cb) {
        return ca ? -1 : 1;
      }
      return compareText(ca, cb) || compareText(titleOf(a), titleOf(b));
    });
  }
  return list.sort((a, b) => compareText(String(b.uploaded_at || ""), String(a.uploaded_at || "")));
}

function searchPieces(pieces, text) {
  const needle = String(text || "").trim().toLowerCase();
  if (!needle) {
    return pieces.slice();
  }
  return pieces.filter((piece) => {
    const haystack = [titleOf(piece), piece.composer, piece.source, ...(piece.tags || [])]
      .map((part) => String(part || "").toLowerCase());
    return haystack.some((part) => part.includes(needle));
  });
}

function bulkPatch(action, value) {
  switch (action) {
    case "set-composer":
      return { composer: value };
    case "set-source":
      return { source: value };
    case "add-tag":
      return { add_tags: Array.isArray(value) ? value : [value] };
    case "remove-tag":
      return { remove_tags: Array.isArray(value) ? value : [value] };
    default:
      return null;
  }
}

/* The values of a group by count, then name (case-insensitive), at most limit of them. */
function topGroupValues(values, limit) {
  return (Array.isArray(values) ? values : [])
    .map((entry) => ({ value: entry.value, count: entry.count }))
    .sort((a, b) => b.count - a.count || compareText(a.value.toLowerCase(), b.value.toLowerCase()))
    .slice(0, limit);
}

/* The top 3 composers and top 3 tags of a collection from its groups (the groups route's {composers, tags}), by count then name
 * (case-insensitive). Unassigned composers are not counted. */
function collectionSummary(groups) {
  const g = groups || {};
  return { composers: topGroupValues(g.composers, 3), tags: topGroupValues(g.tags, 3) };
}

/* Pieces not yet in the collection, for the add-pieces picker. */
function pickerCandidates(pieces, memberIds) {
  const members = new Set(memberIds);
  return pieces.filter((piece) => !members.has(piece.id));
}

function addPiecesBody(ids) {
  return { piece_ids: ids.slice() };
}

function addResultText(added, already) {
  if (!already) {
    return `Added ${added}`;
  }
  return `Added ${added}, ${already} ${already === 1 ? "was" : "were"} already in it`;
}

/* A collection name for a Library filter: "John Field", "Tag: whimsical",
 * parts combined with " · ". Search text is named only when nothing else is
 * filtered. At most 120 characters. */
function collectionNameFromFilter(filter) {
  const f = filter || {};
  const parts = [];
  if (f.composer) {
    parts.push(f.composer);
  } else if ((f.unassigned || []).includes("composer")) {
    parts.push("No composer");
  }
  if (f.source) {
    parts.push(`Source: ${f.source}`);
  } else if ((f.unassigned || []).includes("source")) {
    parts.push("No source");
  }
  if (f.tag) {
    parts.push(`Tag: ${f.tag}`);
  } else if (f.untagged) {
    parts.push("Untagged");
  }
  if (parts.length === 0 && String(f.search || "").trim()) {
    parts.push(`Search: ${String(f.search).trim()}`);
  }
  return (parts.join(" · ") || "Selected pieces").slice(0, 120);
}

function summaryLine(label, entries) {
  return entries.length ? `${label}: ${entries.map((entry) => entry.value).join(", ")}` : "";
}

function pieceCountText(count) {
  return `${count} ${count === 1 ? "piece" : "pieces"}`;
}

/* The source a style is built from, for a Library filter: the backend's
 * filter (composer, source, tag) when the filter is only those values, else
 * the ids of the pieces shown (unassigned, untagged and search have no
 * backend form). */
function styleSourceForFilter(filter, pieceIds) {
  const f = filter || {};
  const plain = (f.composer || f.source || f.tag) && !(f.unassigned || []).length && !f.untagged && !String(f.search || "").trim();
  if (!plain) {
    return { kind: "pieces", piece_ids: (pieceIds || []).slice() };
  }
  const backendFilter = {};
  for (const key of ["composer", "source", "tag"]) {
    if (f[key]) {
      backendFilter[key] = f[key];
    }
  }
  return { kind: "filter", filter: backendFilter };
}

/* The default name the backend would give a style (Composer: X, Source: X,
 * Tag: x joined with " · "; Collection: <name>; Selected pieces), at most
 * 120 characters. */
function defaultStyleName(source) {
  const s = source || {};
  if (s.kind === "filter") {
    const f = s.filter || {};
    const parts = [];
    if (f.composer) {
      parts.push(`Composer: ${f.composer}`);
    }
    if (f.source) {
      parts.push(`Source: ${f.source}`);
    }
    if (f.tag) {
      parts.push(`Tag: ${String(f.tag).toLowerCase()}`);
    }
    return (parts.join(" · ") || "Selected pieces").slice(0, 120);
  }
  if (s.kind === "collection") {
    return `Collection: ${s.name || ""}`.slice(0, 120);
  }
  return "Selected pieces";
}

/* The POST /api/styles body: the name only when not blank, plus exactly one
 * source member. */
function styleRequestBody(source, name) {
  const body = {};
  if (String(name || "").trim()) {
    body.name = String(name).trim();
  }
  if (source.kind === "filter") {
    body.filter = source.filter;
  } else if (source.kind === "collection") {
    body.collection_id = source.collection_id;
  } else {
    body.piece_ids = source.piece_ids;
  }
  return body;
}

/* "Built from 12 pieces" plus ", 2 skipped" when pieces were skipped. The
 * verb is "Built" or, after a rebuild, "Rebuilt". */
function styleBuildText(pieceCount, skippedCount, verb) {
  const text = `${verb || "Built"} from ${pieceCountText(pieceCount)}`;
  return skippedCount > 0 ? `${text}, ${skippedCount} skipped` : text;
}

/* A style's source as the list shows it: "Tag: whimsical", "Collection:
 * <name>" (collectionNames maps id to name; "a collection" when unknown),
 * "Selected pieces (n)". */
function styleSourceText(source, collectionNames) {
  const s = source || {};
  if (s.kind === "filter") {
    return defaultStyleName(s);
  }
  if (s.kind === "collection") {
    const name = collectionNames && collectionNames[s.collection_id];
    return name ? `Collection: ${name}` : "Collection (deleted)";
  }
  if (s.kind === "brief") {
    return "Brief";
  }
  const count = Array.isArray(s.piece_ids) ? s.piece_ids.length : 0;
  return count ? `Selected pieces (${count})` : "Selected pieces";
}

const api = {
  styleSourceText,
  styleSourceForFilter,
  defaultStyleName,
  styleRequestBody,
  styleBuildText,
  summaryLine,
  pieceCountText,
  collectionNameFromFilter,
  pickerCandidates,
  addPiecesBody,
  addResultText,
  parseTagInput,
  filterQuery,
  sortPieces,
  searchPieces,
  bulkPatch,
  collectionSummary,
};
export default api;
