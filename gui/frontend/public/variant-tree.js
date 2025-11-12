/* Pure helpers for the motif variant tree (P13 B9): rows of the saved tree
 * with depth, flag and kind badges, kept and changed chips, the operation
 * list grouped by category, chain requests built from the operation picker
 * and naming of candidates. No DOM. A candidate is one entry of the
 * `variants` route's answer: {seed, chain: {steps: [{op, params, open}]},
 * kind, kept, changed, derivation, recognizability, motive}. */
(function (root) {
  const CATEGORY_ORDER = ["exact", "rhythm", "intervals", "harmony", "melody adaptation"];
  const FLAGS = ["unflagged", "approved", "rejected"];
  const KINDS = ["exact", "motive-form", "variant"];
  const KIND_LABELS = { exact: "Exact", "motive-form": "Motive-form", variant: "Variant" };
  const FLAG_LABELS = { unflagged: "Unflagged", approved: "Approved", rejected: "Rejected" };
  const DEFAULT_COUNT = 6;
  const MAX_COUNT = 16;

  function jsonRequest(method, url, body) {
    const request = { method, url };
    if (body !== undefined) {
      request.headers = { "Content-Type": "application/json" };
      request.body = JSON.stringify(body);
    }
    return request;
  }

  function itemUrl(id, tail) {
    return `/api/materials/items/${encodeURIComponent(id)}/${tail}`;
  }

  function titleOf(text) {
    const value = String(text || "");
    return value ? value[0].toUpperCase() + value.slice(1) : "";
  }

  function flagOf(derived) {
    return derived && FLAGS.includes(derived.flag) ? derived.flag : "unflagged";
  }

  function badge(value, labels) {
    return { value, text: labels[value] || titleOf(value) };
  }

  function chips(kept, changed) {
    const list = [];
    for (const name of Array.isArray(kept) ? kept : []) {
      list.push({ name: String(name), state: "kept", text: `kept ${name}` });
    }
    for (const name of Array.isArray(changed) ? changed : []) {
      list.push({ name: String(name), state: "changed", text: `changed ${name}` });
    }
    return list;
  }

  /* The derivation line of a stored variant: the steps joined by "then". */
  function derivationText(derived) {
    const steps = derived && Array.isArray(derived.operations) ? derived.operations : [];
    return steps.map((step) => step.op).join(" then ");
  }

  /* The saved tree as flat rows, depth first, in the order of the children.
   * A rejected variant is left out with everything under it unless
   * showRejected; hidden counts what was left out. The root is never hidden. */
  function treeRows(tree, showRejected) {
    const rows = [];
    let hidden = 0;
    function countAll(node) {
      return 1 + (node.children || []).reduce((sum, child) => sum + countAll(child), 0);
    }
    function walk(node, depth) {
      const derived = node.derived || null;
      const flag = flagOf(derived);
      if (depth > 0 && flag === "rejected" && !showRejected) {
        hidden += countAll(node);
        return;
      }
      rows.push({
        id: node.id,
        name: String(node.name || ""),
        depth,
        isRoot: depth === 0,
        version: node.current_version,
        derivation: derivationText(derived),
        flag: badge(flag, FLAG_LABELS),
        kind: derived ? badge(derived.kind, KIND_LABELS) : null,
        kindValue: derived && KINDS.includes(derived.kind) ? derived.kind : "motive-form",
        chips: derived ? chips(derived.kept, derived.changed) : [],
        seed: derived && Number.isInteger(derived.seed) ? derived.seed : null,
      });
      for (const child of node.children || []) {
        walk(child, depth + 1);
      }
    }
    if (tree) {
      walk(tree, 0);
    }
    return { rows, hidden };
  }

  /* The operations of GET /api/materials/variant-operations by category, in
   * Schoenberg's order, then any other category alphabetically. */
  function operationGroups(operations) {
    const groups = new Map();
    for (const op of Array.isArray(operations) ? operations : []) {
      const category = String(op.category || "");
      if (!groups.has(category)) {
        groups.set(category, []);
      }
      groups.get(category).push({
        name: op.name,
        label: op.label || op.name,
        citation: op.citation || "",
        params: Array.isArray(op.params) ? op.params : [],
        forms: Array.isArray(op.forms) ? op.forms : [],
      });
    }
    const known = CATEGORY_ORDER.filter((category) => groups.has(category));
    const other = [...groups.keys()].filter((category) => !CATEGORY_ORDER.includes(category)).sort();
    return [...known, ...other].map((category) => ({
      category,
      title: titleOf(category),
      operations: groups.get(category),
    }));
  }

  /* One chain step from the picker. values maps a parameter name to the text
   * or boolean the control holds; an empty text leaves the parameter open
   * (the seed draws it). Returns {step} or {error}. */
  function stepFromValues(operation, values) {
    const step = { op: operation.name };
    const params = {};
    const given = values || {};
    for (const param of operation.params || []) {
      const raw = given[param.name];
      if (param.kind === "bool") {
        if (raw === true || raw === false) {
          params[param.name] = raw;
        }
        continue;
      }
      const text = raw === undefined || raw === null ? "" : String(raw).trim();
      if (text === "") {
        continue;
      }
      if (param.kind === "int" || param.kind === "pitch") {
        const number = Number(text);
        if (!/^-?\d+$/.test(text) || !Number.isSafeInteger(number)) {
          return { error: `${param.name} of ${operation.name} must be a whole number` };
        }
        if (number < param.min || number > param.max) {
          return { error: `${param.name} of ${operation.name} is ${param.min} to ${param.max}` };
        }
        params[param.name] = number;
      } else if (param.kind === "ratio") {
        if (!/^[1-9]\d*:[1-9]\d*$/.test(text)) {
          return { error: `${param.name} of ${operation.name} must be a ratio like 2:1` };
        }
        params[param.name] = text;
      } else {
        params[param.name] = text;
      }
    }
    if (Object.keys(params).length > 0) {
      step.params = params;
    }
    return { step };
  }

  /* The chain of the Generate control from its steps; null when empty, so
   * the route draws random chains. */
  function chainFromSteps(steps) {
    return Array.isArray(steps) && steps.length > 0 ? { steps: steps.map((step) => ({ ...step })) } : null;
  }

  /* A whole number from the seed or count text, or null when blank. NaN for
   * text that is not a whole number from min. */
  function wholeNumber(text, min) {
    const value = String(text === undefined || text === null ? "" : text).trim();
    if (value === "") {
      return null;
    }
    const number = Number(value);
    return /^\d+$/.test(value) && Number.isSafeInteger(number) && number >= min ? number : NaN;
  }

  /* The variants POST. options: {count, seed, steps} (texts or numbers).
   * Returns {request} or {error}. */
  function generateRequest(id, options) {
    const o = options || {};
    const body = {};
    const count = wholeNumber(o.count, 1);
    if (Number.isNaN(count) || (count !== null && count > MAX_COUNT)) {
      return { error: `Count is 1 to ${MAX_COUNT}` };
    }
    if (count !== null) {
      body.count = count;
    }
    const seed = wholeNumber(o.seed, 0);
    if (Number.isNaN(seed)) {
      return { error: "Seed must be a whole number from 0" };
    }
    if (seed !== null) {
      body.seed = seed;
    }
    const chain = chainFromSteps(o.steps);
    if (chain) {
      body.chain = chain;
    }
    return { request: jsonRequest("POST", itemUrl(id, "variants"), body) };
  }

  /* The chain of a candidate with the drawn parameters left open, so the
   * next seed draws them again. */
  function fixedChain(candidate) {
    const steps = candidate && candidate.chain && Array.isArray(candidate.chain.steps) ? candidate.chain.steps : [];
    return {
      steps: steps.map((step) => {
        const drawn = new Set(Array.isArray(step.open) ? step.open : []);
        const params = {};
        for (const name of Object.keys(step.params || {})) {
          if (!drawn.has(name)) {
            params[name] = step.params[name];
          }
        }
        const fixed = { op: step.op };
        if (Object.keys(params).length > 0) {
          fixed.params = params;
        }
        return fixed;
      }),
    };
  }

  /* Reroll of one candidate: its chain again, one candidate, the next seed. */
  function candidateRerollRequest(id, candidate) {
    const seed = candidate && Number.isInteger(candidate.seed) ? candidate.seed + 1 : 0;
    return jsonRequest("POST", itemUrl(id, "variants"), { chain: fixedChain(candidate), count: 1, seed });
  }

  function operationNames(candidate) {
    const steps = candidate && candidate.chain && Array.isArray(candidate.chain.steps) ? candidate.chain.steps : [];
    return steps.map((step) => step.op);
  }

  /* The name a saved candidate gets: "<source> <op>[ + <op>]". */
  function candidateName(sourceName, candidate) {
    const ops = operationNames(candidate);
    const base = String(sourceName || "").trim();
    return ops.length > 0 ? `${base} ${ops.join(" + ")}` : base;
  }

  function saveRequest(id, candidate, flag, name) {
    const body = { variant: candidate, flag: FLAGS.includes(flag) ? flag : "unflagged" };
    if (name && String(name).trim() !== "") {
      body.name = String(name).trim();
    }
    return jsonRequest("POST", itemUrl(id, "variants/save"), body);
  }

  function flagRequest(id, flag, kind) {
    const body = { flag };
    if (kind) {
      body.kind = kind;
    }
    return jsonRequest("POST", itemUrl(id, "flag"), body);
  }

  function rerollRequest(id) {
    return jsonRequest("POST", itemUrl(id, "reroll"));
  }

  function treeRequest(id) {
    return jsonRequest("GET", itemUrl(id, "variant-tree"));
  }

  function operationsRequest() {
    return jsonRequest("GET", "/api/materials/variant-operations");
  }

  /* What a candidate card shows. */
  function candidateView(candidate) {
    const c = candidate || {};
    const recognizability = typeof c.recognizability === "number" ? c.recognizability : null;
    return {
      seed: Number.isInteger(c.seed) ? c.seed : null,
      derivation: String(c.derivation || ""),
      kind: badge(c.kind, KIND_LABELS),
      chips: chips(c.kept, c.changed),
      recognizability: recognizability === null ? "" : `${Math.round(recognizability * 100)}% recognizable`,
    };
  }

  /* The degree, rhythm or contour list of a candidate for its form. */
  function candidateDegrees(candidate, form) {
    const analysis = candidate && candidate.motive && candidate.motive.analysis ? candidate.motive.analysis : {};
    const key = form === "rhythm" ? "rhythm_text" : form === "contour" ? "contour_text" : "degrees_text";
    return typeof analysis[key] === "string" ? analysis[key] : "";
  }

  /* The labelled features a candidate kept, as the editor stores them. */
  function candidateFeatures(candidate) {
    const list = candidate && candidate.motive && candidate.motive.motive ? candidate.motive.motive.features : null;
    return Array.isArray(list)
      ? list.map((feature) => ({ label: String(feature.label), first: Number(feature.first), last: Number(feature.last) }))
      : [];
  }

  /* A motif body for rendering a candidate: the source body with the
   * candidate's degrees, harmony and features (the source's would not fit a
   * fragment or a retrograde). Feeds MaterialsModel.renderRequest. */
  function candidateBody(sourceBody, candidate) {
    const motif = sourceBody && sourceBody.motif ? sourceBody.motif : {};
    return {
      motif: {
        ...motif,
        degrees: candidateDegrees(candidate, motif.form || "full"),
        harmony: candidate && typeof candidate.harmony === "string" ? candidate.harmony : null,
        features: candidateFeatures(candidate),
      },
    };
  }

  const api = {
    FLAGS,
    KINDS,
    KIND_LABELS,
    FLAG_LABELS,
    DEFAULT_COUNT,
    MAX_COUNT,
    treeRows,
    operationGroups,
    stepFromValues,
    chainFromSteps,
    generateRequest,
    fixedChain,
    candidateRerollRequest,
    candidateName,
    saveRequest,
    flagRequest,
    rerollRequest,
    treeRequest,
    operationsRequest,
    candidateView,
    candidateDegrees,
    candidateFeatures,
    candidateBody,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.VariantTree = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
