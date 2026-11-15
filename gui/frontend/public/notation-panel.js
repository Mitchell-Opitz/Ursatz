/* The notation panel's model (R-110): what the meter check reports, how a
 * reading is worded, and the PATCH body for a notation override. Pure
 * functions, no DOM access.
 *
 * Readings are written the way the backend stores them: meter "3/4", value
 * scale "2" or "1/2", pickup "1/4" (whole notes), "" = not set. The meter
 * check's claim (rhythm section, label "meter-fit") carries the same values
 * as {numerator, denominator} and {num, den} objects. */
(function (root) {
  const PICKUP_NAMES = {
    "1/1": "a whole note",
    "1/2": "a half note",
    "3/4": "a dotted half",
    "1/4": "a quarter",
    "3/8": "a dotted quarter",
    "1/8": "an eighth",
    "3/16": "a dotted eighth",
    "1/16": "a sixteenth",
  };

  function rationalText(value) {
    if (typeof value === "string") {
      return value.trim();
    }
    if (value && typeof value.num === "number" && typeof value.den === "number") {
      return value.den === 1 ? String(value.num) : `${value.num}/${value.den}`;
    }
    return "";
  }

  function meterText(value) {
    if (typeof value === "string") {
      return value.trim();
    }
    if (value && typeof value.numerator === "number" && typeof value.denominator === "number") {
      return `${value.numerator}/${value.denominator}`;
    }
    return "";
  }

  /* A pickup of zero is no pickup. */
  function pickupText(value) {
    const text = rationalText(value);
    return text === "0" || text.startsWith("0/") ? "" : text;
  }

  function scaleWords(scale) {
    if (scale === "" || scale === "1") {
      return "";
    }
    if (scale === "2") {
      return "values doubled";
    }
    if (scale === "1/2") {
      return "values halved";
    }
    return `values x${scale}`;
  }

  /* The pieces of a reading, in order: meter, value scale, pickup. */
  function readingParts(meter, scale, pickup) {
    const pickupValue = pickupText(pickup);
    return {
      meter: meterText(meter),
      rest: [
        scaleWords(rationalText(scale)),
        pickupValue ? `pickup of ${PICKUP_NAMES[pickupValue] || pickupValue}` : "",
      ].filter((part) => part !== ""),
    };
  }

  /* "3/4, values doubled", "values halved", "pickup of a quarter". */
  function readingText(meter, scale, pickup) {
    const parts = readingParts(meter, scale, pickup);
    return [parts.meter, ...parts.rest].filter((part) => part !== "").join(", ");
  }

  /* "3/4 with values doubled, pickup of a quarter", for a sentence. */
  function readingPhrase(meter, scale, pickup) {
    const parts = readingParts(meter, scale, pickup);
    return parts.meter && parts.rest.length > 0 ? `${parts.meter} with ${parts.rest.join(", ")}` : readingText(meter, scale, pickup);
  }

  function percent(score) {
    return `${Math.round((typeof score === "number" ? score : 0) * 100)}%`;
  }

  function findMeterFit(document) {
    const rhythm = document && Array.isArray(document.rhythm) ? document.rhythm : [];
    return rhythm.find((claim) => claim && claim.label === "meter-fit") || null;
  }

  /* What the meter check says, or null when the document has no check.
   * `declared` is the meter the analysis used (the file's, or the override),
   * `alternative` the better reading or null. */
  function meterFitSummary(document) {
    const claim = findMeterFit(document);
    if (!claim || !claim.declared) {
      return null;
    }
    const overridden = Boolean(document.meter) && document.meter.source === "override";
    const declared = claim.declared;
    let alternative = null;
    if (claim.alternative) {
      const alt = claim.alternative;
      alternative = {
        text: readingText(alt.meter, alt.scale, alt.pickup),
        meter: meterText(alt.meter),
        value_scale: rationalText(alt.scale),
        pickup: pickupText(alt.pickup),
        score: alt.score,
      };
    }
    let line = `${overridden ? "Applied" : "Declared"} ${readingPhrase(declared.meter, declared.scale, declared.pickup)} fits ${percent(declared.score)}.`;
    if (alternative) {
      line += ` ${readingPhrase(alternative.meter, alternative.value_scale, alternative.pickup)} fits ${percent(alternative.score)}.`;
    }
    return {
      declared: meterText(declared.meter),
      declaredScore: declared.score,
      reading: readingText(declared.meter, declared.scale, declared.pickup),
      alternative,
      overridden,
      line,
    };
  }

  /* The PATCH body for fields {meter, value_scale, pickup}; blank fields are
   * left out, and nothing set (or clear) removes the override. */
  function notationBody(fields) {
    const input = fields || {};
    if (input.clear) {
      return { notation: null };
    }
    const notation = {};
    for (const key of ["meter", "value_scale", "pickup"]) {
      const value = typeof input[key] === "string" ? input[key].trim() : "";
      if (value !== "") {
        notation[key] = value;
      }
    }
    return { notation: Object.keys(notation).length > 0 ? notation : null };
  }

  const api = { meterFitSummary, notationBody, readingText };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.NotationPanel = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
