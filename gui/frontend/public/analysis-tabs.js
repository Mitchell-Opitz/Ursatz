// Pure data logic of the Analysis view: which sections of the analysis
// document each tab shows, and how a section turns into rows. No DOM here,
// so frontend/tests/analysis-view.test.js loads this file directly.

/* One tab per group of document sections, in display order. */
const ANALYSIS_TABS = [
  { id: "keys", label: "Keys", sections: ["keys", "tonicizations", "modulations", "pivot_chords"] },
  { id: "chords", label: "Chords", sections: ["chords", "chord_vocabulary", "non_chord_tones", "prolongations"] },
  { id: "cadences", label: "Cadences", sections: ["cadences"] },
  { id: "progressions", label: "Progressions", sections: ["progressions", "harmonic_rhythm"] },
  { id: "phrases", label: "Phrases", sections: ["phrases"] },
  { id: "themes-form", label: "Themes and form", sections: ["themes", "form"] },
  { id: "motives", label: "Motives", sections: ["motives"] },
  { id: "melody", label: "Melody", sections: ["melody", "outer_voices"] },
  { id: "rhythm-meter", label: "Rhythm and meter", sections: ["meter", "rhythm"] },
  { id: "texture", label: "Texture", sections: ["texture", "voice_leading"] },
  { id: "schemas", label: "Schemas", sections: ["schemas"] },
  { id: "markings", label: "Markings", sections: ["markings"] },
  { id: "counterpoint", label: "Counterpoint", sections: ["counterpoint_check"] },
  { id: "notes", label: "Notes", sections: ["notes"] },
];

function isPlainObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

/* An array item that sits on the measure grid. Arrays inside an object
 * section only become rows when their items are claims. */
function isClaim(item) {
  return isPlainObject(item) && ("span" in item || "location" in item);
}

function isClaimArray(value) {
  return Array.isArray(value) && value.length > 0 && value.every(isClaim);
}

function sectionIsEmpty(value) {
  return value === null || value === undefined || (Array.isArray(value) && value.length === 0);
}

/* "m<measure> b<beat>", or "-" when the position is null. A subdivision
 * past the beat is added to it as a decimal, like the report ("b2.5"). */
function formatPosition(position) {
  if (!position || typeof position.measure !== "number") {
    return "-";
  }
  let beat = position.beat;
  const sub = position.subdivision;
  if (sub && sub.num > 0 && sub.den > 0) {
    beat = Number((position.beat + sub.num / sub.den).toFixed(2));
  }
  return `m${position.measure} b${beat}`;
}

function formatLocation(location) {
  if (!location || !location.start) {
    return "-";
  }
  const start = formatPosition(location.start);
  if (!location.end) {
    return start;
  }
  return `${start} to ${formatPosition(location.end)}`;
}

/* The row's name: its label, else the section's natural name field. */
function claimLabel(item, index) {
  if (typeof item.label === "string" && item.label !== "") {
    return item.label;
  }
  for (const field of ["numeral", "type", "key"]) {
    if (typeof item[field] === "string" && item[field] !== "") {
      return item[field];
    }
  }
  if (isPlainObject(item.roman_numeral) && typeof item.roman_numeral.label === "string") {
    return item.roman_numeral.label;
  }
  return `#${index + 1}`;
}

/* The item's remaining fields as formatted JSON (label and location are
 * already shown in the row). */
function claimDetailJson(item) {
  const rest = {};
  for (const key of Object.keys(item)) {
    if (key !== "label" && key !== "location") {
      rest[key] = item[key];
    }
  }
  return JSON.stringify(rest, null, 2);
}

function buildRow(sectionName, item, index) {
  const claim = isPlainObject(item) ? item : { value: item };
  return {
    section: sectionName,
    index,
    label: claimLabel(claim, index),
    where: formatLocation(claim.location),
    noteIds: Array.isArray(claim.note_ids) ? claim.note_ids : [],
    framework: typeof claim.framework === "string" ? claim.framework : "",
    detail: claimDetailJson(claim),
  };
}

/* An object section as one JSON block, without the claim arrays that are
 * shown as rows. */
function objectBlockText(value) {
  const rest = {};
  for (const key of Object.keys(value)) {
    if (!isClaimArray(value[key])) {
      rest[key] = value[key];
    }
  }
  return JSON.stringify(rest, null, 2);
}

/* The blocks of one section: { name, kind: "rows" | "object" | "empty", rows, text }.
 * An array section gives one row per item; an object section gives its JSON
 * block plus one row per claim in its claim arrays. */
function buildSectionBlock(name, value) {
  if (sectionIsEmpty(value)) {
    return { name, kind: "empty", rows: [], text: "" };
  }
  if (Array.isArray(value)) {
    return { name, kind: "rows", rows: value.map((item, i) => buildRow(name, item, i)), text: "" };
  }
  if (isPlainObject(value)) {
    const rows = [];
    for (const key of Object.keys(value)) {
      if (isClaimArray(value[key])) {
        value[key].forEach((item, i) => rows.push(buildRow(`${name}.${key}`, item, i)));
      }
    }
    return { name, kind: "object", rows, text: objectBlockText(value) };
  }
  return { name, kind: "object", rows: [], text: JSON.stringify(value, null, 2) };
}

/* The state of one tab for a document: its blocks, its row count, and
 * whether every section is empty (the tab is then disabled). */
function buildTabState(tab, document) {
  const blocks = tab.sections.map((name) => buildSectionBlock(name, document ? document[name] : null));
  return {
    tab,
    blocks,
    rowCount: blocks.reduce((total, block) => total + block.rows.length, 0),
    empty: blocks.every((block) => block.kind === "empty"),
  };
}

function buildAllTabStates(document) {
  return ANALYSIS_TABS.map((tab) => buildTabState(tab, document));
}

/* The header above the tabs. */
function buildAnalysisHeader(data) {
  const document = data.document || {};
  const metadata = document.metadata || {};
  const meter = document.meter || {};
  const commit = typeof metadata.ursatz_commit === "string" ? metadata.ursatz_commit.slice(0, 7) : "";
  const noteCount = typeof metadata.note_count === "number" ? metadata.note_count : (data.note_xml_ids || []).length;
  return {
    filename: data.filename || data.id,
    format: data.format || "",
    analyzerVersion: metadata.analyzer_version || "",
    ursatzCommit: commit,
    noteCount,
    measureCount: typeof meter.measure_count === "number" ? meter.measure_count : null,
  };
}

/* The metadata fields shown in the Provenance panel, as [label, value]. */
function buildProvenanceFields(document) {
  const metadata = (document && document.metadata) || {};
  const input = metadata.input || {};
  const fields = [
    ["Source file", metadata.source_file],
    ["Analyzer version", metadata.analyzer_version],
    ["Ursatz commit", metadata.ursatz_commit],
    ["Document schema", document && document.schema_version],
    ["Input format", input.format],
    ["Source type", input.source_type],
    ["Meter", input.meter],
    ["Key signature", input.key],
    ["Spelling", input.spelling],
    ["Importer warnings", input.warning_count],
  ];
  return fields.filter(([, value]) => value !== undefined && value !== null && value !== "").map(([label, value]) => [label, String(value)]);
}

/* A claim's note_ids are indices into the imported notes; the score knows
 * notes by xml:id, which the piece detail lists in the same order. */
function mapNoteIdsToXmlIds(noteIds, noteXmlIds) {
  const ids = [];
  for (const noteId of noteIds || []) {
    const xmlId = (noteXmlIds || [])[noteId];
    if (xmlId) {
      ids.push(xmlId);
    }
  }
  return ids;
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    ANALYSIS_TABS,
    buildAllTabStates,
    buildTabState,
    buildSectionBlock,
    buildAnalysisHeader,
    buildProvenanceFields,
    mapNoteIdsToXmlIds,
    formatLocation,
    formatPosition,
    claimLabel,
    sectionIsEmpty,
  };
}
