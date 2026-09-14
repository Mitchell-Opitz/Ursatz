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

  const FUNCTIONS = ["beginning", "continuation", "contrast", "cadential"];

  /* One chip per function the form suits (R-143). */
  function functionChips(functions) {
    return (Array.isArray(functions) ? functions : [])
      .filter((name) => FUNCTIONS.includes(name))
      .map((name) => ({ name, state: "function", text: name }));
  }

  const SUITS_FILTERS = [{ value: "", text: "Any" }, ...FUNCTIONS.map((name) => ({ value: name, text: titleOf(name) }))];
  const SUITS_EXPLANATION =
    "Where in a phrase a form fits, worked out from its harmony, rhythm and length (Schoenberg). Theme slots use it to pick forms.";

  /* Whether a form with these functions passes the Suits filter ("" is Any). */
  function suitsMatch(functions, filter) {
    return !filter || (Array.isArray(functions) && functions.includes(filter));
  }

  /* "N% remote from the basic motive" for a remoteness from 0 to 1; empty when it is unknown. */
  function remotenessText(remoteness) {
    return typeof remoteness === "number" && remoteness >= 0 && remoteness <= 1
      ? `${Math.round(remoteness * 100)}% remote from the basic motive`
      : "";
  }

  /* The harmony line of a form with chords of its own; empty without. */
  function harmonyLine(harmony) {
    return typeof harmony === "string" && harmony !== "" ? `Harmony: ${harmony}` : "";
  }

  /* The derivation line of a stored variant: the steps joined by "then". */
  function derivationText(derived) {
    const steps = derived && Array.isArray(derived.operations) ? derived.operations : [];
    return steps.map((step) => step.op).join(" then ");
  }

  /* The saved tree as flat rows, depth first, in the order of the children.
   * A rejected variant is left out with everything under it unless
   * showRejected; hidden counts what was left out. The root is never hidden.
   * A promoted motif (standalone) and what is under it are not rows: promoted lists each one reached without
   * crossing another, as {id, name, derivation} (R-188). A Suits filter (a function name) leaves out the variants
   * that do not suit it; what is under them stays. */
  function treeRows(tree, showRejected, suits) {
    const rows = [];
    const promoted = [];
    let hidden = 0;
    function countAll(node) {
      return 1 + (node.children || []).reduce((sum, child) => sum + countAll(child), 0);
    }
    function walk(node, depth) {
      const derived = node.derived || null;
      const flag = flagOf(derived);
      if (depth > 0 && node.standalone === true) {
        promoted.push({ id: node.id, name: String(node.name || ""), derivation: derivationText(derived) });
        return;
      }
      if (depth > 0 && flag === "rejected" && !showRejected) {
        hidden += countAll(node);
        return;
      }
      if (depth > 0 && !suitsMatch(derived && derived.functions, suits)) {
        for (const child of node.children || []) {
          walk(child, depth + 1);
        }
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
        functions: derived ? functionChips(derived.functions) : [],
        remoteness: derived ? remotenessText(derived.remoteness) : "",
        harmony: derived ? harmonyLine(derived.harmony) : "",
        seed: derived && Number.isInteger(derived.seed) ? derived.seed : null,
      });
      for (const child of node.children || []) {
        walk(child, depth + 1);
      }
    }
    if (tree) {
      walk(tree, 0);
    }
    return { rows, hidden, promoted };
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

  /* The choices of a feature parameter: drawn, then one per labelled feature of the motif (the value is the 0-based
   * index the chain file takes, the text the label and its 1-based notes). */
  function featureOptions(features) {
    const options = [{ value: "", text: "drawn" }];
    (Array.isArray(features) ? features : []).forEach((feature, index) => {
      options.push({ value: String(index), text: `${feature.label} (notes ${Number(feature.first) + 1}-${Number(feature.last) + 1})` });
    });
    return options;
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

  const DISTANCES = [
    { value: "", text: "Any" },
    { value: "close", text: "Close" },
    { value: "medium", text: "Medium" },
    { value: "far", text: "Far" },
  ];

  /* The status after Generate: the seed, or how many came out when the distance left fewer than asked. */
  function generatedNote(variants, count, seed, distance) {
    const found = Array.isArray(variants) ? variants.length : 0;
    if (found === 0) {
      return "No variants came out";
    }
    const asked = Number.isInteger(count) ? count : DEFAULT_COUNT;
    return distance && found < asked ? `${found} of ${asked} found at this distance.` : `Seed ${seed}`;
  }

  /* The variants POST. options: {count, seed, steps, functions, distance} (texts or numbers; functions are the names
   * to keep; distance is close, medium or far, and takes no steps). Returns {request} or {error}. */
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
    const distance = DISTANCES.some((d) => d.value !== "" && d.value === o.distance) ? o.distance : null;
    if (chain && distance) {
      return { error: "Distance takes random operations" };
    }
    if (chain) {
      body.chain = chain;
    }
    if (distance) {
      body.distance = distance;
    }
    const functions = FUNCTIONS.filter((name) => Array.isArray(o.functions) && o.functions.includes(name));
    if (functions.length > 0) {
      body.functions = functions;
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

  /* The promote request: the variant becomes a motif of its own, named and in a project. */
  function promoteRequest(id, name, projectId) {
    return jsonRequest("POST", itemUrl(id, "promote"), { name: String(name || "").trim(), project_id: projectId });
  }

  /* The promoted list line: "<name> (opens it)" with the derivation under it. */
  function promotedText(entry) {
    return `${entry.name} (opens it)`;
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

  /* The recognize request: how the target motif (an {item, version} reference) derives from the source. */
  function compareRequest(source, target) {
    return jsonRequest("POST", "/api/materials/recognize", {
      source: { item: source.item, version: source.version },
      target: { item: target.item, version: target.version },
    });
  }

  /* The link-form request: the target item becomes a form of the source, from the recognition the compare gave. */
  function linkFormRequest(targetId, source, recognition) {
    return jsonRequest("POST", itemUrl(targetId, "link-form"), {
      source: { item: source.item, version: source.version },
      recognition,
    });
  }

  /* What the compare panel shows for a recognize answer: the chain, the derivation, the kept and changed chips and the
   * distance as "N% related". A search that found no chain is "Not related within 4 steps" and cannot be linked. */
  function compareView(recognition) {
    const r = recognition || {};
    if (r.found !== true) {
      return { found: false, text: "Not related within 4 steps", chain: "", derivation: "", chips: [], related: "", canLink: false };
    }
    const distance = typeof r.distance === "number" && r.distance >= 0 && r.distance <= 1 ? r.distance : null;
    const steps = r.chain && Array.isArray(r.chain.steps) ? r.chain.steps : [];
    return {
      found: true,
      text: "",
      chain: steps.map((step) => step.op).join(" then "),
      derivation: String(r.derivation || ""),
      chips: chips(r.kept, r.changed),
      related: distance === null ? "" : `${Math.round((1 - distance) * 100)}% related`,
      canLink: true,
    };
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
      functions: functionChips(c.functions),
      remoteness: remotenessText(c.remoteness),
      harmony: harmonyLine(c.harmony),
      recognizability: recognizability === null ? "" : `${Math.round(recognizability * 100)}% recognizable`,
    };
  }

  /* The degree, rhythm or contour list of a candidate for its form. */
  function candidateDegrees(candidate, form) {
    const analysis = candidate && candidate.motive && candidate.motive.analysis ? candidate.motive.analysis : {};
    const key = form === "rhythm" ? "rhythm_text" : form === "contour" ? "contour_text" : "degrees_text";
    return typeof analysis[key] === "string" ? analysis[key] : "";
  }

  /* Why a motif cannot be varied: empty when it has degrees, else the message
   * with the reason stored with its version. */
  function noDegreesMessage(motif) {
    if (motif && typeof motif.degrees === "string" && motif.degrees !== "") {
      return "";
    }
    const stored = motif && typeof motif.analysis_error === "string" ? motif.analysis_error : "";
    return `No analysis yet: ${stored || "the motif was not analyzed"}. Re-analyze on the motif first.`;
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

  /* Most render requests the candidate cards have in flight at once. */
  const RENDER_LIMIT = 4;

  /* Runs async tasks, at most `limit` at a time, in the order they were added. add(task) answers a promise of the
   * task's result; a task that throws answers its error and frees its slot. active() and waiting() count the running
   * and the queued tasks. */
  function createQueue(limit) {
    const waiting = [];
    let active = 0;
    function next() {
      while (active < limit && waiting.length > 0) {
        const job = waiting.shift();
        active++;
        Promise.resolve()
          .then(job.task)
          .then(job.resolve, job.reject)
          .finally(() => {
            active--;
            next();
          });
      }
    }
    return {
      add(task) {
        return new Promise((resolve, reject) => {
          waiting.push({ task, resolve, reject });
          next();
        });
      },
      active: () => active,
      waiting: () => waiting.length,
    };
  }

  const api = {
    RENDER_LIMIT,
    createQueue,
    FLAGS,
    KINDS,
    KIND_LABELS,
    FLAG_LABELS,
    FUNCTIONS,
    DEFAULT_COUNT,
    DISTANCES,
    SUITS_FILTERS,
    SUITS_EXPLANATION,
    suitsMatch,
    generatedNote,
    MAX_COUNT,
    treeRows,
    operationGroups,
    featureOptions,
    stepFromValues,
    chainFromSteps,
    generateRequest,
    fixedChain,
    candidateRerollRequest,
    candidateName,
    saveRequest,
    flagRequest,
    promoteRequest,
    promotedText,
    rerollRequest,
    treeRequest,
    operationsRequest,
    compareRequest,
    linkFormRequest,
    compareView,
    candidateView,
    candidateDegrees,
    noDegreesMessage,
    candidateFeatures,
    candidateBody,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.VariantTree = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
