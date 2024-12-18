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

  const api = {
    LAYER_GROUPS,
    LAYERS,
    abbreviateCadence,
    rat,
    ratCompare,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreLayers = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
