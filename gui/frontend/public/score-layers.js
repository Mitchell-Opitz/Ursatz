/* The claim layers of the score workspace (R-099, R-101): which layers
 * exist, their colors and groups. A layer is drawn as one of three kinds:
 *
 *   ribbon  a bar over the claim's span in a lane above or below the staff
 *   mark    a short label at one point in a lane
 *   notes   the claim's noteheads colored, no lane
 *
 * Positions are whole notes from the piece start, as in a claim's span.
 *
 * Pure functions, no DOM access, so Node tests load this file directly. */
(function (root) {
  const LAYER_GROUPS = [
    { id: "harmony", label: "Harmony" },
    { id: "structure", label: "Structure" },
    { id: "texture", label: "Texture and melody" },
    { id: "other", label: "Other" },
  ];

  const LAYERS = [
    { id: "keys", label: "Keys", group: "harmony", kind: "ribbon", placement: "above", color: "#1f6fd1", description: "Key regions" },
    { id: "key-changes", label: "Key changes", group: "harmony", kind: "ribbon", placement: "above", color: "#7b4fd8", description: "Tonicizations and modulations" },
    { id: "chords", label: "Chords", group: "harmony", kind: "mark", placement: "below", color: "#0b7a70", description: "Roman numerals" },
    { id: "cadences", label: "Cadences", group: "harmony", kind: "mark", placement: "above", color: "#d0342c", description: "Cadence arrivals" },
    { id: "non-chord-tones", label: "Non-chord tones", group: "harmony", kind: "notes", placement: null, color: "#8a6a00", description: "Notes outside the chord" },
    { id: "prolongations", label: "Prolongations", group: "harmony", kind: "ribbon", placement: "above", color: "#3b8a2f", description: "Prolonged harmonies" },
    { id: "progressions", label: "Progressions", group: "harmony", kind: "ribbon", placement: "above", color: "#b5530a", description: "Recurring chord patterns" },
    { id: "form", label: "Form sections", group: "structure", kind: "ribbon", placement: "above", color: "#3f4b5e", description: "Sections of the form" },
    { id: "themes", label: "Themes", group: "structure", kind: "ribbon", placement: "above", color: "#c2185b", description: "Theme types" },
    { id: "phrases", label: "Phrases", group: "structure", kind: "ribbon", placement: "above", color: "#006f99", description: "Phrases and subphrases" },
    { id: "motives", label: "Motives", group: "structure", kind: "notes", placement: null, color: "#a2369a", description: "Motive occurrences" },
    { id: "schemas", label: "Schemas", group: "structure", kind: "ribbon", placement: "above", color: "#7a5a3a", description: "Galant schemas" },
    { id: "texture", label: "Texture", group: "texture", kind: "ribbon", placement: "above", color: "#5f6b00", description: "Texture by passage" },
    { id: "melody", label: "Melody", group: "texture", kind: "notes", placement: null, color: "#2c3fa8", description: "The melody line" },
  ];

  const CADENCE_ABBREVIATIONS = {
    "perfect-authentic": "PAC",
    "imperfect-authentic": "IAC",
    half: "HC",
    subverted: "EV",
    plagal: "PC",
    deceptive: "DC",
    "authentic-unspecified": "AC",
    unknown: "?",
  };

  /* A null type is the prolongation reading of a competing cadence
   * (R-090); an unlisted type gives its hyphen-separated initials. */
  function abbreviateCadence(type) {
    if (type === null || type === undefined) {
      return "PROL";
    }
    if (Object.prototype.hasOwnProperty.call(CADENCE_ABBREVIATIONS, type)) {
      return CADENCE_ABBREVIATIONS[type];
    }
    return String(type)
      .split("-")
      .filter((part) => part.length > 0)
      .map((part) => part[0].toUpperCase())
      .join("");
  }

  /* {num, den} or [num, den] to a number; anything else is NaN. */
  function rat(value) {
    if (Array.isArray(value) && value.length === 2) {
      return value[0] / value[1];
    }
    if (value && typeof value === "object" && "num" in value && "den" in value) {
      return value.num / value.den;
    }
    return NaN;
  }

  function ratParts(value) {
    return Array.isArray(value) ? value : [value.num, value.den];
  }

  /* Exact comparison by cross multiplication: negative, zero or positive. */
  function ratCompare(a, b) {
    let [an, ad] = ratParts(a);
    let [bn, bd] = ratParts(b);
    if (ad < 0) {
      [an, ad] = [-an, -ad];
    }
    if (bd < 0) {
      [bn, bd] = [-bn, -bd];
    }
    return Math.sign(an * bd - bn * ad);
  }

  function isPlainObject(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
  }

  /* The isClaim rule of analysis-tabs.js: an object on the measure grid. */
  function isClaim(item) {
    return isPlainObject(item) && ("span" in item || "location" in item);
  }

  function isClaimArray(value) {
    return Array.isArray(value) && value.length > 0 && value.every(isClaim);
  }

  function arrayOf(value) {
    return Array.isArray(value) ? value : [];
  }

  function hasSpan(claim) {
    return isPlainObject(claim) && isPlainObject(claim.span) && claim.span.start && claim.span.end;
  }

  /* The claim's own index field when it has one, else its array position. */
  function ordinal(claim, position) {
    return Number.isInteger(claim.index) ? claim.index + 1 : position + 1;
  }

  /* Raw sources of each catalog layer: [{claim, parent, text, style, at}].
   * text and at left undefined fall back to the claim's label and none. */
  const SOURCES = {
    keys: (doc) => arrayOf(doc.keys && doc.keys.regions).map((claim) => ({ claim })),
    "key-changes": (doc) => [
      ...arrayOf(doc.tonicizations).map((claim) => ({ claim, style: "dashed" })),
      ...arrayOf(doc.modulations).map((claim) => ({ claim, style: "solid" })),
    ],
    chords: (doc) =>
      arrayOf(doc.chords).map((claim) => ({
        claim,
        text: (claim.roman_numeral && claim.roman_numeral.label) || claim.label,
        at: hasSpan(claim) ? rat(claim.span.start) : undefined,
      })),
    cadences: (doc) =>
      arrayOf(doc.cadences).map((claim) => {
        const arrival = claim.arrival && hasSpan(claim.arrival) ? claim.arrival : claim;
        return {
          claim,
          text: abbreviateCadence(claim.type),
          at: hasSpan(arrival) ? rat(arrival.span.start) : undefined,
        };
      }),
    "non-chord-tones": (doc) => arrayOf(doc.non_chord_tones).map((claim) => ({ claim })),
    prolongations: (doc) => arrayOf(doc.prolongations).map((claim) => ({ claim })),
    progressions: (doc) =>
      arrayOf(doc.progressions).flatMap((parent) =>
        arrayOf(parent.occurrences).map((claim) => ({ claim, parent, text: parent.label })),
      ),
    form: (doc) => arrayOf(doc.form && doc.form.sections).map((claim) => ({ claim })),
    themes: (doc) => arrayOf(doc.themes).map((claim) => ({ claim })),
    phrases: (doc) =>
      arrayOf(doc.phrases).flatMap((claim, i) => [
        { claim, text: `Phrase ${ordinal(claim, i)}` },
        ...arrayOf(claim.subphrases).map((sub) => ({ claim: sub, parent: claim, style: "thin", text: sub.label })),
      ]),
    motives: (doc) =>
      arrayOf(doc.motives).flatMap((parent, i) =>
        arrayOf(parent.occurrences).map((claim) => ({ claim, parent, text: `Motive ${ordinal(parent, i)}` })),
      ),
    schemas: (doc) => arrayOf(doc.schemas).map((claim) => ({ claim, text: claim.name || claim.label })),
    texture: (doc) => arrayOf(doc.texture).map((claim) => ({ claim, text: claim.texture || claim.label })),
    melody: (doc) => arrayOf(doc.melody).map((claim) => ({ claim })),
  };

  /* Sections the catalog draws itself, and sections that are never a layer. */
  const CATALOG_SECTIONS = [
    "keys", "tonicizations", "modulations", "chords", "cadences", "non_chord_tones", "prolongations",
    "progressions", "form", "themes", "phrases", "motives", "schemas", "texture", "melody",
  ];
  const IGNORED_SECTIONS = [
    "schema_version", "metadata", "meter", "notes", "chord_vocabulary", "harmonic_rhythm",
    "outer_voices", "markings", "voice_leading", "rhythm",
  ];
  const OTHER_COLOR = "#6b7280";

  /* Every claim of an Other section: the section itself when it is a claim
   * array, else the claim arrays it holds, in key order. */
  function otherSectionClaims(value) {
    if (isClaimArray(value)) {
      return value;
    }
    if (isPlainObject(value)) {
      return Object.keys(value).flatMap((key) => (isClaimArray(value[key]) ? value[key] : []));
    }
    return [];
  }

  function sectionLabel(section) {
    const words = section.replace(/_/g, " ");
    return words.charAt(0).toUpperCase() + words.slice(1);
  }

  /* One layer per top-level section the catalog does not know, so a new
   * Analyzer section shows up without a GUI change. */
  function discoverOtherLayers(document) {
    if (!isPlainObject(document)) {
      return [];
    }
    return Object.keys(document)
      .filter((section) => !CATALOG_SECTIONS.includes(section) && !IGNORED_SECTIONS.includes(section))
      .filter((section) => otherSectionClaims(document[section]).length > 0)
      .map((section) => ({
        id: `other:${section}`,
        label: sectionLabel(section),
        group: "other",
        kind: "ribbon",
        placement: "above",
        color: OTHER_COLOR,
        description: `The ${section} section`,
        section,
      }));
  }

  function layerSources(document, layer) {
    if (!isPlainObject(document)) {
      return [];
    }
    if (SOURCES[layer.id]) {
      return SOURCES[layer.id](document);
    }
    if (layer.section) {
      return otherSectionClaims(document[layer.section]).map((claim) => ({ claim }));
    }
    return [];
  }

  /* The drawable items of one layer, sorted by start, then end. */
  function collectLayerItems(document, layer) {
    const items = layerSources(document, layer)
      .filter((source) => hasSpan(source.claim))
      .map((source) => {
        const claim = source.claim;
        const item = {
          key: null,
          layerId: layer.id,
          text: source.text !== undefined && source.text !== null ? String(source.text) : claim.label ?? "",
          label: claim.label ?? null,
          start: rat(claim.span.start),
          end: rat(claim.span.end),
          noteIds: Array.isArray(claim.note_ids) ? claim.note_ids : [],
          style: source.style || "solid",
          claim,
          parent: source.parent || null,
        };
        if (layer.kind === "mark") {
          item.at = source.at !== undefined ? source.at : item.start;
        }
        return item;
      });
    items.sort((a, b) => a.start - b.start || a.end - b.end);
    items.forEach((item, n) => {
      item.key = `${layer.id}/${n}`;
    });
    return items;
  }

  /* Every layer with its items: catalog layers first, then Other layers in
   * document key order. */
  function buildLayerCatalog(document) {
    return [...LAYERS, ...discoverOtherLayers(document)].map((layer) => {
      const items = collectLayerItems(document, layer);
      return { ...layer, items, count: items.length, empty: items.length === 0 };
    });
  }

  const api = {
    LAYER_GROUPS,
    LAYERS,
    abbreviateCadence,
    rat,
    ratCompare,
    collectLayerItems,
    discoverOtherLayers,
    buildLayerCatalog,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreLayers = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
