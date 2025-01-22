/* The pure model behind the Inspector (R-099): which item a click on a note
 * means, the neighbors of an item, list filtering, value formatting and the
 * evidence of one claim as plain rows. No DOM, so Node tests load this file
 * directly.
 *
 * Items are the layer items of score-layers.js ({key, layerId, text, label,
 * start, end, claim, ...}). Needs formatLocation (analysis-tabs.js) and
 * ScoreLayers (score-layers.js) loaded first in the browser. */
(function (root) {
  const isNode = typeof module !== "undefined" && module.exports;
  const tabs = isNode ? require("./analysis-tabs.js") : root;
  const layers = isNode ? require("./score-layers.js") : root.ScoreLayers;

  /* Fields shown elsewhere in the Inspector, not as evidence rows. */
  const SKIPPED_FIELDS = [
    "framework", "span", "location", "note_ids", "label", "share", "alternative",
    "uncertainty", "score", "confidence",
  ];

  function isPlainObject(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
  }

  function isClaim(value) {
    return isPlainObject(value) && ("span" in value || "location" in value);
  }

  function spanOf(item) {
    return item.end - item.start;
  }

  /* Of the enabled layers' items that contain a clicked note, the one with
   * the shortest span; the earlier in the list (catalog order) wins a tie.
   * null for no items. */
  function pickItemForNote(items) {
    let best = null;
    for (const item of items || []) {
      if (best === null || spanOf(item) < spanOf(best)) {
        best = item;
      }
    }
    return best;
  }

  /* The previous (direction < 0) or next item of the same layer by start,
   * or null at either end or for an unknown key. */
  function neighborItem(items, key, direction) {
    const current = (items || []).find((item) => item.key === key);
    if (!current) {
      return null;
    }
    const same = items
      .filter((item) => item.layerId === current.layerId)
      .sort((a, b) => a.start - b.start || a.end - b.end);
    const at = same.indexOf(current);
    const next = same[at + (direction < 0 ? -1 : 1)];
    return next || null;
  }

  /* Items whose text or label contains the query, ignoring case. A blank
   * query keeps everything. */
  function filterItems(items, query) {
    const needle = String(query || "").trim().toLowerCase();
    if (needle === "") {
      return [...(items || [])];
    }
    return (items || []).filter(
      (item) =>
        String(item.text || "").toLowerCase().includes(needle) ||
        String(item.label || "").toLowerCase().includes(needle),
    );
  }

  /* "arrival_beat_strength" to "Arrival beat strength". */
  function humanizeKey(key) {
    const words = String(key).replace(/_/g, " ");
    return words.charAt(0).toUpperCase() + words.slice(1);
  }

  /* A value as text: strings as they are, numbers to at most 3 decimals,
   * true "yes", false "no", null "none"; a list joins its values. */
  function formatValue(value) {
    if (value === null || value === undefined) {
      return "none";
    }
    if (typeof value === "string") {
      return value;
    }
    if (typeof value === "number") {
      return String(Number(value.toFixed(3)));
    }
    if (typeof value === "boolean") {
      return value ? "yes" : "no";
    }
    if (Array.isArray(value)) {
      return value.length === 0 ? "none" : value.map(formatValue).join(", ");
    }
    return JSON.stringify(value);
  }

  function percent(share) {
    return typeof share === "number" ? Math.round(share) / 10 : null;
  }

  /* A cadence's reading and, when it has one, the competing reading. */
  function buildReadings(claim) {
    const primary = {
      abbr: layers.abbreviateCadence(claim.type),
      type: claim.type ?? null,
      share: percent(claim.share),
    };
    const alt = isPlainObject(claim.alternative) ? claim.alternative : null;
    const alternative = alt
      ? {
          abbr: layers.abbreviateCadence(alt.type),
          reading: alt.reading ?? null,
          share: percent(alt.share),
          noteIds: Array.isArray(alt.note_ids) ? alt.note_ids : [],
          measure: alt.measure ?? null,
          beat: alt.beat ?? null,
        }
      : null;
    return { primary, alternative };
  }

  function claimText(claim, index) {
    return typeof claim.label === "string" && claim.label !== "" ? claim.label : `#${index + 1}`;
  }

  function evidenceRow(key, value) {
    const label = humanizeKey(key);
    if (Array.isArray(value) && value.length > 0 && value.every(isClaim)) {
      return {
        key,
        label,
        kind: "claims",
        count: value.length,
        items: value.map((claim, i) => ({
          text: claimText(claim, i),
          location: tabs.formatLocation(claim.location),
          start: claim.span && claim.span.start ? layers.rat(claim.span.start) : null,
          end: claim.span && claim.span.end ? layers.rat(claim.span.end) : null,
          noteIds: Array.isArray(claim.note_ids) ? claim.note_ids : [],
          claim,
        })),
      };
    }
    if (isPlainObject(value) && isPlainObject(value.span) && value.span.start && value.span.end) {
      return {
        key,
        label,
        kind: "span",
        start: layers.rat(value.span.start),
        end: layers.rat(value.span.end),
        location: tabs.formatLocation(value.location),
      };
    }
    if (isPlainObject(value)) {
      return { key, label, kind: "group", rows: Object.keys(value).map((k) => evidenceRow(k, value[k])) };
    }
    return { key, label, kind: "value", text: formatValue(value), value };
  }

  function buildEvidence(claim) {
    return Object.keys(claim)
      .filter((key) => !SKIPPED_FIELDS.includes(key))
      .map((key) => evidenceRow(key, claim[key]));
  }

  /* The numbers a claim carries about itself, each under its own name. */
  function buildScores(claim) {
    const scores = [];
    for (const name of ["uncertainty", "score"]) {
      if (typeof claim[name] === "number") {
        scores.push({ name, value: claim[name], text: formatValue(claim[name]) });
      }
    }
    if (isPlainObject(claim.confidence)) {
      for (const key of Object.keys(claim.confidence)) {
        const value = claim.confidence[key];
        if (typeof value === "number") {
          scores.push({ name: `confidence.${key}`, value, text: formatValue(value) });
        }
      }
    }
    return scores;
  }

  function measureCount(claim) {
    const m = claim.measures;
    if (isPlainObject(m) && typeof m.first === "number" && typeof m.last === "number") {
      return m.last - m.first + 1;
    }
    return null;
  }

  /* Everything the Claim tab shows for one item of `layer`:
   * {header, readings, evidence, scores, raw}. */
  function buildInspectorModel(item, layer) {
    const claim = item.claim || {};
    const label = typeof claim.label === "string" && claim.label !== item.text ? claim.label : null;
    return {
      header: {
        layerId: layer.id,
        layerLabel: layer.label,
        color: layer.color,
        text: item.text,
        label,
        location: tabs.formatLocation(claim.location),
        measures: measureCount(claim),
      },
      readings: layer.id === "cadences" ? buildReadings(claim) : null,
      evidence: buildEvidence(claim),
      scores: buildScores(claim),
      raw: JSON.stringify(claim, null, 2),
    };
  }

  const api = {
    pickItemForNote,
    neighborItem,
    filterItems,
    humanizeKey,
    formatValue,
    buildInspectorModel,
  };
  if (isNode) {
    module.exports = api;
  } else {
    root.InspectorModel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
