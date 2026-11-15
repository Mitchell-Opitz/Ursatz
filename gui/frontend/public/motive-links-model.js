/* Pure helpers for the Motive links view (R-154, R-156): the scope body of a
 * run, the progress line, and rows for links, groups, earlier runs and a
 * piece's motive families. No DOM. A run is the answer of GET
 * /api/links/runs/<id>; its result is the Analyzer's motive-links output. */
(function (root) {
  const SCOPE_KINDS = [
    { kind: "all", title: "All Library pieces" },
    { kind: "collection", title: "Collection" },
    { kind: "filter", title: "Composer / Source / Tag" },
    { kind: "subject", title: "Subject" },
    { kind: "project", title: "Project" },
  ];
  const DIMENSIONS = ["rhythm", "intervals", "contour", "harmony"];

  function scopeTitle(kind) {
    const found = SCOPE_KINDS.find((entry) => entry.kind === kind);
    return found ? found.title : String(kind || "");
  }

  function text(value) {
    return typeof value === "string" ? value.trim() : "";
  }

  /* The body of POST /api/links/runs for a picker selection {kind,
   * collectionId, composer, source, tag, subjectId, projectId}, or null when
   * the selection does not name a scope yet. */
  function scopeBody(selection) {
    const sel = selection || {};
    switch (sel.kind) {
      case "all":
        return { scope: { kind: "all" } };
      case "collection":
        return text(sel.collectionId) ? { scope: { kind: "collection", collection_id: sel.collectionId } } : null;
      case "filter": {
        const filter = {};
        for (const key of ["composer", "source", "tag"]) {
          if (text(sel[key])) {
            filter[key] = sel[key];
          }
        }
        return Object.keys(filter).length > 0 ? { scope: { kind: "filter", filter } } : null;
      }
      case "subject":
        return text(sel.subjectId) ? { scope: { kind: "subject", subject_id: sel.subjectId } } : null;
      case "project":
        return text(sel.projectId) ? { scope: { kind: "project", project_id: sel.projectId } } : null;
      default:
        return null;
    }
  }

  function plural(count, one, many) {
    return count === 1 ? one : many;
  }

  /* The line under the Run button: the live count while a run is going, then
   * what it compared and took from the cache. */
  function progressText(run) {
    if (!run) {
      return "";
    }
    const done = Number(run.done) || 0;
    const total = Number(run.total) || 0;
    if (run.status === "running") {
      return `Comparing ${done} of ${total} pairs`;
    }
    if (run.status === "failed") {
      return run.message ? `Run failed: ${run.message}` : "Run failed";
    }
    const result = run.result || {};
    const compared = Number(result.compared) || 0;
    const cached = Number(result.cached) || 0;
    const pairs = Number(result.pairs) || 0;
    return `compared ${compared}, cached ${cached} of ${pairs} pairs`;
  }

  /* The pieces a run left out, as text lines "<id>: <reason>". */
  function skippedLines(run) {
    const skipped = run && Array.isArray(run.skipped) ? run.skipped : [];
    return skipped.map((entry) => `${entry.id}: ${entry.reason}`);
  }

  function measuresText(measures) {
    if (!measures || typeof measures.first !== "number") {
      return "-";
    }
    return measures.last === measures.first || typeof measures.last !== "number"
      ? `m${measures.first}`
      : `m${measures.first}-${measures.last}`;
  }

  function endRow(end) {
    return {
      piece: end.piece,
      family: end.family,
      title: end.title || end.piece,
      measures: measuresText(end.measures),
      degrees: end.degrees || "",
    };
  }

  function percentText(distance) {
    const value = distance && typeof distance.value === "number" ? distance.value : 0;
    return `${Math.round(value * 100)}%`;
  }

  /* One row per link of a result: both ends, the steps joined by "then", the
   * distance as a percent, and the dimensions kept (similarity 1) and changed. */
  function linkRows(result) {
    const links = result && Array.isArray(result.links) ? result.links : [];
    return links.map((link) => {
      const similarity = link.similarity || {};
      return {
        a: endRow(link.a),
        b: endRow(link.b),
        reversed: Boolean(link.reversed),
        steps: (link.steps || []).join(" then "),
        distance: percentText(link.distance),
        kept: DIMENSIONS.filter((name) => similarity[name] === 1),
        changed: DIMENSIONS.filter((name) => similarity[name] !== 1),
      };
    });
  }

  /* The groups of a result, each with its members resolved against the link
   * ends (a group member only names a piece and a family). */
  function groupRows(result) {
    const groups = result && Array.isArray(result.groups) ? result.groups : [];
    const known = new Map();
    for (const link of result && Array.isArray(result.links) ? result.links : []) {
      for (const end of [link.a, link.b]) {
        known.set(`${end.piece}#${end.family}`, end);
      }
    }
    return groups.map((group) => ({
      members: (group.members || []).map((member) => {
        const end = known.get(`${member.piece}#${member.family}`);
        const row = end ? endRow(end) : endRow(member);
        if (member.title) {
          row.title = member.title;
        }
        row.family = member.family;
        return row;
      }),
    }));
  }

  function dateText(createdAt) {
    const match = /^(\d{4}-\d{2}-\d{2})[T ](\d{2}:\d{2})/.exec(String(createdAt || ""));
    return match ? `${match[1]} ${match[2]}` : String(createdAt || "");
  }

  /* A scope as one phrase; names maps {collections, subjects, projects} ids to
   * names for the kinds that point at one. */
  function scopeText(scope, names) {
    const lookup = names || {};
    const nameOf = (table, id) => (table && table[id]) || id || "";
    const s = scope || {};
    switch (s.kind) {
      case "collection":
        return `Collection ${nameOf(lookup.collections, s.collection_id)}`;
      case "subject":
        return `Subject ${nameOf(lookup.subjects, s.subject_id)}`;
      case "project":
        return `Project ${nameOf(lookup.projects, s.project_id)}`;
      case "filter": {
        const filter = s.filter || {};
        const parts = ["composer", "source", "tag"].filter((key) => filter[key]).map((key) => `${key} ${filter[key]}`);
        return parts.length > 0 ? parts.join(", ") : scopeTitle("filter");
      }
      default:
        return scopeTitle("all");
    }
  }

  /* Rows for the earlier runs of GET /api/links/runs (newest first as sent). */
  function runRows(runs, names) {
    return (Array.isArray(runs) ? runs : []).map((run) => ({
      id: run.id,
      status: run.status,
      label: `${scopeText(run.scope, names)} · ${dateText(run.created_at)}`,
      progress: run.status === "running" ? progressText(run) : run.status === "failed" ? "failed" : "",
    }));
  }

  /* The links of GET /api/links/pieces/<id> that touch one motive family of
   * the piece: the other piece, its measures and the steps. */
  function pieceLinkRows(entries, pieceId, family) {
    const rows = [];
    for (const entry of Array.isArray(entries) ? entries : []) {
      const link = entry && entry.link;
      if (!link || !link.a || !link.b) {
        continue;
      }
      const mine = link.a.piece === pieceId && link.a.family === family ? link.a : link.b.piece === pieceId && link.b.family === family ? link.b : null;
      if (!mine) {
        continue;
      }
      const other = mine === link.a ? link.b : link.a;
      rows.push({
        run: entry.run,
        piece: other.piece,
        family: other.family,
        title: other.title || other.piece,
        measures: measuresText(other.measures),
        degrees: other.degrees || "",
        steps: (link.steps || []).join(" then "),
        distance: percentText(link.distance),
      });
    }
    return rows;
  }

  /* [label, value] rows for a motive family's features: degrees, intervals,
   * rhythm and the non-chord tones by note (1-based within the motive). */
  function motiveFeatureRows(motive) {
    const features = motive && motive.features;
    if (!features) {
      return [];
    }
    const rows = [];
    if (Array.isArray(features.degrees)) {
      rows.push(["Degrees", features.degrees.join(" ")]);
    }
    if (Array.isArray(features.intervals)) {
      rows.push(["Intervals", features.intervals.join(" ")]);
    }
    if (Array.isArray(features.rhythm)) {
      rows.push(["Rhythm", features.rhythm.join(" ")]);
    }
    if (Array.isArray(features.ncts)) {
      const tones = features.ncts.map((type, i) => (type ? `note ${i + 1}: ${type}` : null)).filter(Boolean);
      rows.push(["Non-chord tones", tones.length > 0 ? tones.join(", ") : "none"]);
    }
    return rows;
  }

  const api = {
    SCOPE_KINDS,
    scopeTitle,
    scopeBody,
    progressText,
    skippedLines,
    measuresText,
    linkRows,
    groupRows,
    scopeText,
    runRows,
    pieceLinkRows,
    motiveFeatureRows,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.MotiveLinksModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
