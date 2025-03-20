/* Pure helpers for piece metadata in the Library (issue 6): tag parsing,
 * filter queries, sorting, searching and bulk-edit bodies. No DOM; the
 * backend stays the authority on what a valid value is. */
(function (root) {
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

  function filterQuery(filter) {
    const f = filter || {};
    const params = [];
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

  const api = { parseTagInput, filterQuery, sortPieces, searchPieces, bulkPatch };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.PieceMetadata = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
