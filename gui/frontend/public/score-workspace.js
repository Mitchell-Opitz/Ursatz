/* The score workspace (R-099): a toolbar, a Layers rail, the score on white
 * paper and an Inspector, mounted into any element. It knows the score only
 * through the URLs and data it is given, so the Composer can mount it too.
 *
 *   const ws = ScoreWorkspace.mount(rootEl, {
 *     title, subtitle,           toolbar text
 *     musicxmlUrl, metaUrl,      the score and its meter caption
 *     document, noteXmlIds,      the analysis, for the layers and the inspector
 *     playbackUrl,               the piece's playback data (timeline offset)
 *     actions: [{label, onClick}],  extra toolbar buttons
 *     onBack,                    the Library link
 *   });
 *   ws.on("rendered", (layout) => ...);
 *   ws.on("modechange", (mode) => ...);
 *   ws.on("layerchange", (enabledIds) => ...);
 *   ws.on("select", (selection) => ...);   {item, layer, others} or null
 *
 * Zoom, panel state and view mode are remembered (WorkspaceState). Needs
 * OSMD (window.opensheetmusicdisplay), ScoreNotes, ScoreLayers,
 * InspectorModel, ScoreGeometry, ScoreOverlay and WorkspaceState loaded
 * first. */
(function (root) {
  const RESIZE_DELAY_MS = 150;
  /* Most notehead halos drawn for one selection (a whole-piece claim can
   * name thousands of notes). */
  const MAX_HALOS = 2000;

  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) {
      node.className = className;
    }
    if (text !== undefined) {
      node.textContent = text;
    }
    return node;
  }

  function safeStorage() {
    try {
      return root.localStorage || null;
    } catch (err) {
      return null;
    }
  }

  function mount(rootEl, options) {
    const opts = options || {};
    const storage = safeStorage();
    const state = WorkspaceState.loadUiState(storage);
    /* What is open right now. A floating panel starts closed and is not
     * remembered; only docked panels write to the saved state. */
    const open = { layers: true, inspector: true };
    let panelsMode = WorkspaceState.panelMode(window.innerWidth);
    let focusRestore = null;
    let dialog = null;
    let renderDeferred = false;
    const listeners = new Map();
    /* The layers this piece offers and which are on (R-099): all off until
     * the reader turns some on. */
    const catalog = ScoreLayers.buildLayerCatalog(opts.document || {});
    const layerState = ScoreLayers.createLayerState(storage);
    layerState.setCatalog(catalog.map((layer) => layer.id));
    const layerById = new Map(catalog.map((layer) => [layer.id, layer]));
    const itemByKey = new Map();
    for (const layer of catalog) {
      for (const item of layer.items) {
        itemByKey.set(item.key, item);
      }
    }
    /* The one selected item: {item, layer, others}, where others are the
     * further items a clicked note belongs to. */
    let selection = null;
    /* A second highlight beside the selection: {tints, noteIds} or null (a
     * sub-span or sub-claim shown from the Inspector). */
    let secondary = null;
    const itemByClaim = new Map();
    for (const item of itemByKey.values()) {
      itemByClaim.set(item.claim, item);
    }
    let osmd = null;
    let overlay = null;
    /* OSMD time minus piece position; the timeline offset (see
     * ScoreGeometry.calibrateOffset). */
    let timelineOffset = 0;
    let calibrated = false;
    let playbackData = null;
    /* OSMD's own system spacing, read once before any change, and the
     * spacing last applied (OSMD units). */
    let spacingDefaults = null;
    let appliedSpacing = null;
    let xmlText = "";
    let layout = null;
    let destroyed = false;
    let layoutWarned = false;
    let lastWidth = 0;
    let resizeTimer = null;
    let savedScrollTop = 0;

    rootEl.innerHTML = "";
    const ws = el("div", "ws");
    ws.dataset.mode = state.mode;
    rootEl.appendChild(ws);

    /* Toolbar. */
    const toolbar = el("header", "ws-toolbar");
    const left = el("div", "ws-toolbar-left");
    const back = el("a", "ws-back");
    back.href = "#library";
    back.appendChild(el("span", "ws-back-arrow", "←"));
    back.appendChild(document.createTextNode(" Library"));
    back.addEventListener("click", (event) => {
      event.preventDefault();
      if (opts.onBack) {
        opts.onBack();
      }
    });
    const titleWrap = el("div", "ws-titles");
    const title = el("span", "ws-title", opts.title || "");
    title.title = opts.title || "";
    const meta = el("span", "ws-meta", opts.subtitle || "");
    titleWrap.append(title, meta);
    left.append(back, titleWrap);

    const transport = el("div", "ws-transport");

    const right = el("div", "ws-toolbar-right");
    const modeGroup = el("div", "ws-segmented");
    modeGroup.setAttribute("role", "group");
    modeGroup.setAttribute("aria-label", "View");
    const modeButtons = {};
    for (const [id, label] of [["score", "Score"], ["data", "Data"]]) {
      const button = el("button", "ws-seg-btn", label);
      button.type = "button";
      button.addEventListener("click", () => setMode(id));
      modeButtons[id] = button;
      modeGroup.appendChild(button);
    }
    right.appendChild(modeGroup);

    function iconButton(text, label, shortcut, onClick) {
      const button = el("button", "ws-icon-btn", text);
      button.type = "button";
      button.setAttribute("aria-label", label);
      button.title = `${label} (${shortcut})`;
      button.addEventListener("click", onClick);
      return button;
    }
    const zoomOutButton = iconButton("\u2212", "Zoom out", "-", () => setZoom(WorkspaceState.nextZoom(state.zoom, -1)));
    const zoomLabelButton = iconButton("", "Reset zoom to 100%", "0", () => setZoom(1));
    zoomLabelButton.classList.add("ws-zoom-label");
    const zoomInButton = iconButton("+", "Zoom in", "+", () => setZoom(WorkspaceState.nextZoom(state.zoom, 1)));
    const layersButton = iconButton("Layers", "Layers panel", "L", () => togglePanel("layers"));
    const inspectorButton = iconButton("Inspector", "Inspector panel", "I", () => togglePanel("inspector"));
    right.append(zoomOutButton, zoomLabelButton, zoomInButton, layersButton, inspectorButton);
    for (const action of opts.actions || []) {
      const button = el("button", "ws-btn", action.label);
      button.type = "button";
      button.addEventListener("click", (event) => action.onClick(event));
      right.appendChild(button);
    }
    toolbar.append(left, transport, right);

    const status = el("div", "ws-status");
    status.setAttribute("role", "status");
    status.hidden = true;

    /* Body: rail, score, inspector, and the Data pane in place of the score. */
    const body = el("div", "ws-body");
    const rail = el("aside", "ws-panel ws-rail");
    rail.setAttribute("aria-label", "Layers");
    const railBody = el("div", "ws-rail-body");
    rail.append(el("h2", "ws-panel-title", "Layers"), railBody);

    const main = el("main", "ws-main");
    const canvas = el("div", "ws-canvas");
    const caption = el("div", "ws-caption");
    caption.hidden = true;
    const paper = el("div", "ws-paper");
    const paperStatus = el("div", "ws-paper-status");
    const osmdHost = el("div", "ws-osmd");
    paper.append(paperStatus, osmdHost);
    overlay = ScoreOverlay.create(paper, osmdHost);
    canvas.append(caption, paper);
    main.appendChild(canvas);
    canvas.addEventListener("click", () => {
      closeFloatingPanels();
    });

    const dataPane = el("div", "ws-data");

    const inspector = el("aside", "ws-panel ws-inspector");
    inspector.setAttribute("aria-label", "Inspector");
    const inspectorBody = el("div", "ws-insp");
    inspector.append(el("h2", "ws-panel-title", "Inspector"), inspectorBody);

    body.append(rail, main, dataPane, inspector);
    ws.append(toolbar, status, body);

    function emit(name, payload) {
      for (const fn of listeners.get(name) || []) {
        fn(payload);
      }
    }

    function on(name, fn) {
      if (!listeners.has(name)) {
        listeners.set(name, new Set());
      }
      listeners.get(name).add(fn);
      return () => listeners.get(name).delete(fn);
    }

    const inspectorView = InspectorView.create(inspectorBody, {
      select: (item, options) => select(item, options),
      setSecondary: (next) => setSecondary(next),
      itemForClaim: (claim) => {
        const item = itemByClaim.get(claim);
        return item && layerState.isOn(item.layerId) ? item : null;
      },
      layerOf: (item) => layerById.get(item.layerId) || null,
      reveal: (position) => scrollToPosition(position, true),
    });
    on("select", (current) => inspectorView.show(current));

    /* The Layers rail: preset chips, then one switch per layer, grouped. A
     * layer with nothing in this piece is shown disabled. */
    const layerSwitches = new Map();

    function buildRail() {
      railBody.innerHTML = "";
      layerSwitches.clear();
      const presets = el("div", "ws-presets");
      presets.setAttribute("role", "group");
      presets.setAttribute("aria-label", "Layer presets");
      for (const [name, label] of [["none", "None"], ["harmony", "Harmony"], ["structure", "Structure"]]) {
        const chip = el("button", "ws-chip", label);
        chip.type = "button";
        chip.addEventListener("click", () => layerState.applyPreset(name));
        presets.appendChild(chip);
      }
      railBody.appendChild(presets);
      for (const group of ScoreLayers.LAYER_GROUPS) {
        const layers = catalog.filter((layer) => layer.group === group.id);
        if (layers.length === 0) {
          continue;
        }
        const section = el("section", "ws-layer-group");
        section.appendChild(el("h3", "ws-layer-group-title", group.label));
        for (const layer of layers) {
          const row = el("label", "ws-layer-row");
          row.title = layer.empty ? "None found in this piece" : layer.description;
          const input = el("input");
          input.type = "checkbox";
          input.setAttribute("role", "switch");
          input.disabled = layer.empty;
          input.addEventListener("change", () => layerState.setOn(layer.id, input.checked));
          const swatch = el("span", "ws-swatch");
          swatch.style.background = layer.color;
          row.append(input, swatch, el("span", "ws-layer-label", layer.label), el("span", "ws-layer-count", String(layer.count)));
          if (layer.empty) {
            row.classList.add("ws-layer-empty");
          }
          layerSwitches.set(layer.id, input);
          section.appendChild(row);
        }
        railBody.appendChild(section);
      }
      syncRail();
    }

    function syncRail() {
      for (const [id, input] of layerSwitches) {
        input.checked = layerState.isOn(id);
      }
    }

    /* xml:id -> the GraphicalNotes OSMD drew for it, for the current render
     * (a render makes new GraphicalNotes), with the barline fragments
     * "<id>_<letter>" and "<id>_<number>" Ursatz writes for a split note
     * listed under their base id. */
    let xmlEntries = null;
    let noteLookup = null;

    function getNoteLookup() {
      if (noteLookup) {
        return noteLookup;
      }
      if (!osmd || !xmlText) {
        return null;
      }
      if (!xmlEntries) {
        const parsed = new DOMParser().parseFromString(xmlText, "application/xml");
        xmlEntries = ScoreNotes.buildMusicXmlNoteKeys(parsed.documentElement);
      }
      const { index } = ScoreNotes.buildNoteIndex(xmlEntries, ScoreNotes.collectGraphicalNotes(osmd));
      const fragments = new Map();
      for (const [id, notes] of index) {
        const match = /^(.*)_([a-z]|[0-9]+)$/.exec(id);
        if (match) {
          if (!fragments.has(match[1])) {
            fragments.set(match[1], []);
          }
          fragments.get(match[1]).push(...notes);
        }
      }
      noteLookup = { index, fragments };
      return noteLookup;
    }

    function graphicalNotesFor(xmlId) {
      const lookup = getNoteLookup();
      if (!lookup) {
        return [];
      }
      return [...(lookup.index.get(xmlId) || []), ...(lookup.fragments.get(xmlId) || [])];
    }

    /* The notehead paths of one GraphicalNote: its own head within a chord. */
    function noteheadPaths(note) {
      const group = note.getSVGGElement && note.getSVGGElement();
      if (!group) {
        return [];
      }
      const heads = group.querySelectorAll(".vf-notehead");
      const head = heads[note.vfnoteIndex] || heads[0];
      return head ? Array.from(head.querySelectorAll("path")) : [];
    }

    /* Noteheads colored for the notes layers that are on; the original
     * colors are put back first, so a layer turned off disappears. */
    const coloredPaths = [];
    /* the colored notehead paths -> the note index they show */
    const pathNote = new Map();

    function restoreNoteColors() {
      pathNote.clear();
      for (const { path, fill, stroke } of coloredPaths) {
        for (const [name, value] of [["fill", fill], ["stroke", stroke]]) {
          if (value === null) {
            path.removeAttribute(name);
          } else {
            path.setAttribute(name, value);
          }
        }
      }
      coloredPaths.length = 0;
    }

    function applyNoteColors() {
      restoreNoteColors();
      if (!osmd) {
        return;
      }
      const enabled = new Set(layerState.enabled());
      const layers = catalog.filter((layer) => layer.kind === "notes" && enabled.has(layer.id) && !layer.empty);
      if (layers.length === 0) {
        return;
      }
      const colors = new Map(layers.map((layer) => [layer.id, layer.color]));
      for (const [noteIndex, layerId] of ScoreLayers.noteColorOwners(layers)) {
        const xmlId = (opts.noteXmlIds || [])[noteIndex];
        if (!xmlId) {
          continue;
        }
        for (const note of graphicalNotesFor(xmlId)) {
          for (const path of noteheadPaths(note)) {
            coloredPaths.push({ path, fill: path.getAttribute("fill"), stroke: path.getAttribute("stroke") });
            path.setAttribute("fill", colors.get(layerId));
            path.setAttribute("stroke", colors.get(layerId));
            pathNote.set(path, noteIndex);
          }
        }
      }
    }

    /* Each notehead's box in pixels relative to the OSMD SVG, for halos. */
    function noteRects(noteIds) {
      const svg = osmdHost.querySelector("svg");
      if (!svg) {
        return [];
      }
      const base = svg.getBoundingClientRect();
      const boxes = [];
      for (const index of noteIds) {
        const xmlId = (opts.noteXmlIds || [])[index];
        if (!xmlId) {
          continue;
        }
        for (const note of graphicalNotesFor(xmlId)) {
          const [path] = noteheadPaths(note);
          if (!path || boxes.length >= MAX_HALOS) {
            continue;
          }
          const rect = path.getBoundingClientRect();
          if (rect.width > 0 || rect.height > 0) {
            boxes.push({ x: rect.left - base.left, y: rect.top - base.top, w: rect.width, h: rect.height });
          }
        }
      }
      return boxes;
    }

    /* What the overlay shows for the selection. A ribbon layer's claim can
     * hold thousands of notes, so it gets a tint over its span; the others
     * get a halo around each of their noteheads. */
    function buildView() {
      const view = { selectedKey: null, tints: [], halos: [], secondaryTints: [], secondaryHalos: [] };
      if (selection) {
        const { item, layer } = selection;
        view.selectedKey = item.key;
        if (layer.kind === "ribbon") {
          view.tints.push({ start: item.start, end: item.end });
        } else {
          view.halos = noteRects(item.noteIds);
        }
      }
      if (secondary) {
        view.secondaryTints = secondary.tints;
        view.secondaryHalos = noteRects(secondary.noteIds);
      }
      return view;
    }

    function redrawOverlay() {
      applyNoteColors();
      if (overlay) {
        overlay.draw(layout, timelineOffset, catalog, layerState.enabled(), buildView());
      }
    }

    function reducedMotion() {
      return Boolean(window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);
    }

    /* Scrolls the score so the system holding the item's start sits at 25%
     * of the canvas height; with onlyIfHidden, only when that system is not
     * fully in view. scrollToPosition does the same for a piece position. */
    function scrollToItem(item, onlyIfHidden) {
      scrollToPosition(item.start, onlyIfHidden);
    }

    function scrollToPosition(position, onlyIfHidden) {
      if (!layout || layout.systems.length === 0) {
        return;
      }
      const system = layout.systems[ScoreGeometry.xAt(layout, position + timelineOffset).system];
      if (!system) {
        return;
      }
      const top = svgTopInCanvas() + system.top;
      const bottom = svgTopInCanvas() + system.bottom;
      if (onlyIfHidden && top >= canvas.scrollTop && bottom <= canvas.scrollTop + canvas.clientHeight) {
        return;
      }
      canvas.scrollTo({
        top: Math.max(0, top - canvas.clientHeight * 0.25),
        behavior: reducedMotion() ? "auto" : "smooth",
      });
    }

    /* Selects one item (a ribbon, mark, numeral or the item of a clicked
     * note) and shows it in the Inspector. */
    function select(item, options) {
      const layer = item && layerById.get(item.layerId);
      if (!layer) {
        return;
      }
      const given = options || {};
      selection = { item, layer, others: given.others || [] };
      secondary = null;
      redrawOverlay();
      if (state.mode === "score" && !open.inspector) {
        setPanel("inspector", true);
      }
      if (given.scroll) {
        scrollToItem(item, false);
      } else if (given.reveal) {
        scrollToItem(item, true);
      }
      emit("select", selection);
    }

    function setSecondary(next) {
      secondary = next;
      redrawOverlay();
    }

    function clearSelection() {
      if (!selection) {
        return false;
      }
      selection = null;
      secondary = null;
      redrawOverlay();
      emit("select", null);
      return true;
    }

    /* The previous or next item of the selected item's layer. */
    function selectNeighbor(direction) {
      if (!selection) {
        return false;
      }
      const next = InspectorModel.neighborItem(selection.layer.items, selection.item.key, direction);
      if (!next) {
        return false;
      }
      select(next, { reveal: true });
      return true;
    }

    /* The items of the layers that are on, in catalog order. */
    function enabledItems() {
      const on = new Set(layerState.enabled());
      return catalog.filter((layer) => on.has(layer.id) && !layer.empty).flatMap((layer) => layer.items);
    }

    /* A click on a ribbon, mark or numeral selects its item. */
    overlay.element.addEventListener("click", (event) => {
      const group = event.target.closest ? event.target.closest("g[data-key]") : null;
      const item = group ? itemByKey.get(group.getAttribute("data-key")) : null;
      if (item) {
        event.stopPropagation();
        select(item);
      }
    });

    /* A click on a notehead colored by a notes layer selects the shortest
     * item of the layers that are on that holds the note. */
    osmdHost.addEventListener("click", (event) => {
      const path = event.target.closest ? event.target.closest("path") : null;
      const noteIndex = path ? pathNote.get(path) : undefined;
      if (noteIndex === undefined) {
        return;
      }
      const hits = enabledItems().filter((item) => item.noteIds.includes(noteIndex));
      const picked = InspectorModel.pickItemForNote(hits);
      if (picked) {
        event.stopPropagation();
        select(picked, { others: hits.filter((item) => item !== picked) });
      }
    });

    /* Room between systems for the lanes of the layers that are on
     * (ScoreGeometry.systemSpacingUnits), never less than OSMD's own. */
    function wantedSpacing() {
      const { lanes } = ScoreOverlay.planLanes(catalog, layerState.enabled());
      const stack = ScoreGeometry.laneStack(lanes);
      return ScoreGeometry.systemSpacingUnits(stack, state.zoom, spacingDefaults);
    }

    function sameSpacing(a, b) {
      return (
        Math.abs(a.minimumDistanceBetweenSystems - b.minimumDistanceBetweenSystems) < 1e-6 &&
        Math.abs(a.pageTopMargin - b.pageTopMargin) < 1e-6
      );
    }

    function applySpacing() {
      const rules = osmd.EngravingRules;
      if (!spacingDefaults) {
        spacingDefaults = {
          minimumDistanceBetweenSystems: rules.MinimumDistanceBetweenSystems,
          pageTopMargin: rules.PageTopMargin,
        };
      }
      appliedSpacing = wantedSpacing();
      rules.MinimumDistanceBetweenSystems = appliedSpacing.minimumDistanceBetweenSystems;
      rules.PageTopMargin = appliedSpacing.pageTopMargin;
    }

    /* The timeline offset (R-101): OSMD time of a note minus its position
     * from the playback route, over the first 50 notes found in the score.
     * Needs the playback data and a render; done once per piece. */
    let mismatchWarned = false;

    function calibrate() {
      if (calibrated || !playbackData || !osmd) {
        return;
      }
      const pairs = [];
      const notes = playbackData.notes || [];
      for (let i = 0; i < notes.length && pairs.length < 50; i++) {
        const xmlId = (opts.noteXmlIds || [])[notes[i].i];
        const found = xmlId ? graphicalNotesFor(xmlId) : [];
        if (found.length > 0) {
          pairs.push({ ursatz: ScoreLayers.rat(notes[i].on), osmd: ScoreGeometry.osmdNoteTime(found[0]) });
        }
      }
      if (pairs.length === 0) {
        return;
      }
      const result = ScoreGeometry.calibrateOffset(pairs);
      timelineOffset = result.offset;
      calibrated = true;
      if (result.mismatches > 0 && !mismatchWarned) {
        mismatchWarned = true;
        console.warn(`Score workspace: ${result.mismatches} of ${pairs.length} notes do not fit the timeline offset.`);
      }
    }

    layerState.onChange((enabled) => {
      syncRail();
      /* a layer turned off takes its selection with it */
      if (selection && !enabled.includes(selection.item.layerId)) {
        selection = null;
        secondary = null;
        emit("select", null);
      }
      /* new lanes can need more room between systems: draw the score again */
      if (osmd && spacingDefaults && !sameSpacing(wantedSpacing(), appliedSpacing)) {
        render();
      } else {
        redrawOverlay();
      }
      emit("layerchange", enabled);
    });
    buildRail();

    function setPaperStatus(text, isError) {
      paperStatus.textContent = text;
      paperStatus.classList.toggle("error", Boolean(isError));
      paperStatus.hidden = text === "";
    }

    function setStatus(text, isError) {
      status.textContent = text || "";
      status.classList.toggle("error", Boolean(isError));
      status.hidden = !text;
    }

    function setMode(mode) {
      if (mode !== "score" && mode !== "data") {
        return;
      }
      if (mode === state.mode) {
        syncMode();
        return;
      }
      if (state.mode === "score") {
        savedScrollTop = canvas.scrollTop;
      }
      state.mode = mode;
      savePrefs();
      syncMode();
      if (mode === "score") {
        if (osmd && (renderDeferred || canvas.clientWidth !== lastWidth)) {
          render();
        }
        canvas.scrollTop = savedScrollTop;
      }
      emit("modechange", mode);
    }

    function syncMode() {
      ws.dataset.mode = state.mode;
      for (const [id, button] of Object.entries(modeButtons)) {
        button.setAttribute("aria-pressed", String(id === state.mode));
      }
      const inScore = state.mode === "score";
      layersButton.disabled = !inScore;
      inspectorButton.disabled = !inScore;
      syncZoom();
    }

    function savePrefs() {
      WorkspaceState.saveUiState(storage, {
        layersOpen: state.layersOpen,
        inspectorOpen: state.inspectorOpen,
        zoom: state.zoom,
        mode: state.mode,
      });
    }

    function syncZoom() {
      const inScore = state.mode === "score";
      const steps = WorkspaceState.ZOOM_STEPS;
      zoomLabelButton.textContent = WorkspaceState.zoomLabel(state.zoom);
      zoomLabelButton.disabled = !inScore;
      zoomOutButton.disabled = !inScore || state.zoom <= steps[0];
      zoomInButton.disabled = !inScore || state.zoom >= steps[steps.length - 1];
    }

    function setZoom(zoom) {
      if (zoom === state.zoom) {
        return;
      }
      state.zoom = zoom;
      savePrefs();
      syncZoom();
      if (osmd) {
        render();
      }
    }

    function isFloating(panel) {
      return panelsMode === "overlay" || (panelsMode === "inspector-overlay" && panel === "inspector");
    }

    function syncPanels() {
      ws.dataset.panels = panelsMode;
      ws.dataset.layers = open.layers ? "open" : "closed";
      ws.dataset.inspector = open.inspector ? "open" : "closed";
      layersButton.setAttribute("aria-pressed", String(open.layers));
      inspectorButton.setAttribute("aria-pressed", String(open.inspector));
    }

    function setPanel(panel, isOpen) {
      open[panel] = isOpen;
      if (!isFloating(panel)) {
        state[panel === "layers" ? "layersOpen" : "inspectorOpen"] = isOpen;
        savePrefs();
      }
      syncPanels();
    }

    function togglePanel(panel) {
      setPanel(panel, !open[panel]);
    }

    /* Focus mode: both panels closed, then back as they were. */
    function toggleFocus() {
      if (open.layers || open.inspector) {
        focusRestore = { layers: open.layers, inspector: open.inspector };
        setPanel("layers", false);
        setPanel("inspector", false);
        return;
      }
      const back = focusRestore || { layers: !isFloating("layers"), inspector: !isFloating("inspector") };
      focusRestore = null;
      setPanel("layers", back.layers);
      setPanel("inspector", back.inspector);
    }

    /* Closes the floating panels that are open; true when any was. */
    function closeFloatingPanels() {
      let closed = false;
      for (const panel of ["layers", "inspector"]) {
        if (isFloating(panel) && open[panel]) {
          setPanel(panel, false);
          closed = true;
        }
      }
      return closed;
    }

    /* A new window width can change which panels float: those start closed,
     * docked ones come back as saved. */
    function applyPanelMode(initial) {
      const next = WorkspaceState.panelMode(window.innerWidth);
      if (!initial && next === panelsMode) {
        return;
      }
      panelsMode = next;
      open.layers = isFloating("layers") ? false : state.layersOpen;
      open.inspector = isFloating("inspector") ? false : state.inspectorOpen;
      syncPanels();
    }

    function openShortcuts() {
      if (dialog) {
        return;
      }
      const backdrop = el("div", "ws-backdrop");
      const box = el("div", "ws-dialog");
      box.setAttribute("role", "dialog");
      box.setAttribute("aria-modal", "true");
      box.setAttribute("aria-labelledby", "ws-shortcuts-title");
      const heading = el("h2", "", "Keyboard shortcuts");
      heading.id = "ws-shortcuts-title";
      const list = el("dl", "ws-shortcut-list");
      for (const row of WorkspaceState.SHORTCUTS) {
        const term = el("dt");
        [row.key, ...(row.alt || [])].forEach((key, i) => {
          if (i > 0) {
            term.appendChild(document.createTextNode(" "));
          }
          term.appendChild(el("kbd", "ws-kbd", key === "Escape" ? "Esc" : key));
        });
        list.append(term, el("dd", "", row.label));
      }
      const close = el("button", "ws-btn", "Close");
      close.type = "button";
      close.addEventListener("click", closeShortcuts);
      box.append(heading, list, close);
      backdrop.appendChild(box);
      /* the Close button is the only control: Tab keeps focus on it */
      box.addEventListener("keydown", (event) => {
        if (event.key === "Tab") {
          event.preventDefault();
          close.focus();
        }
      });
      backdrop.addEventListener("click", (event) => {
        if (event.target === backdrop) {
          closeShortcuts();
        }
      });
      ws.appendChild(backdrop);
      dialog = { backdrop, previous: document.activeElement };
      close.focus();
    }

    function closeShortcuts() {
      if (!dialog) {
        return;
      }
      const { backdrop, previous } = dialog;
      dialog = null;
      backdrop.remove();
      if (previous && typeof previous.focus === "function" && document.contains(previous)) {
        previous.focus();
      }
    }

    function onKeyDown(event) {
      if (event.defaultPrevented) {
        return;
      }
      const row = WorkspaceState.shortcutFor(event);
      if (!row) {
        return;
      }
      const inScore = state.mode === "score";
      if (dialog && row.action !== "escape" && row.action !== "toggle-shortcuts") {
        return;
      }
      switch (row.action) {
        case "toggle-layers":
          if (!inScore) return;
          togglePanel("layers");
          break;
        case "toggle-inspector":
          if (!inScore) return;
          togglePanel("inspector");
          break;
        case "toggle-focus":
          if (!inScore) return;
          toggleFocus();
          break;
        case "zoom-in":
          if (!inScore) return;
          setZoom(WorkspaceState.nextZoom(state.zoom, 1));
          break;
        case "zoom-out":
          if (!inScore) return;
          setZoom(WorkspaceState.nextZoom(state.zoom, -1));
          break;
        case "zoom-reset":
          if (!inScore) return;
          setZoom(1);
          break;
        case "toggle-shortcuts":
          if (dialog) {
            closeShortcuts();
          } else {
            openShortcuts();
          }
          break;
        case "select-previous":
        case "select-next":
          if (!inScore || !selectNeighbor(row.action === "select-next" ? 1 : -1)) return;
          break;
        case "escape":
          if (dialog) {
            closeShortcuts();
          } else if (!closeFloatingPanels() && !(inScore && clearSelection())) {
            return;
          }
          break;
        default:
          return;
      }
      event.preventDefault();
    }

    /* The score's SVG top inside the scrolling canvas, in pixels. */
    function svgTopInCanvas() {
      const svg = osmdHost.querySelector("svg");
      if (!svg) {
        return 0;
      }
      return svg.getBoundingClientRect().top - canvas.getBoundingClientRect().top + canvas.scrollTop;
    }

    /* The measure at the top of the viewport, so a re-render can keep it
     * there. */
    function captureAnchor() {
      if (!layout || layout.systems.length === 0) {
        return null;
      }
      const y = canvas.scrollTop - svgTopInCanvas();
      const system = layout.systems.find((s) => s.bottom >= y) || layout.systems[layout.systems.length - 1];
      const first = system.measures[0];
      if (!first) {
        return null;
      }
      return { measure: first.number, delta: Math.max(0, y - system.top), unit: layout.unit };
    }

    function restoreAnchor(anchor) {
      if (!anchor || !layout) {
        return;
      }
      const system = layout.systems.find((s) => s.measures.some((m) => m.number === anchor.measure));
      if (!system) {
        return;
      }
      canvas.scrollTop = svgTopInCanvas() + system.top + anchor.delta * (layout.unit / anchor.unit);
    }

    function readLayout() {
      try {
        return ScoreGeometry.extractLayout(osmd);
      } catch (err) {
        if (!layoutWarned) {
          layoutWarned = true;
          console.warn(`Score workspace: could not read the score layout: ${err}`);
        }
        return null;
      }
    }

    /* Draws the score again at the current zoom and width, keeping the
     * measure at the top of the viewport where it was. */
    function render() {
      if (!osmd || destroyed) {
        return;
      }
      if (canvas.clientWidth === 0) {
        /* hidden (Data mode): draw when the score is shown again */
        renderDeferred = true;
        return;
      }
      renderDeferred = false;
      const anchor = captureAnchor();
      osmd.zoom = state.zoom;
      coloredPaths.length = 0;
      noteLookup = null;
      applySpacing();
      osmd.render();
      layout = readLayout();
      restoreAnchor(anchor);
      lastWidth = canvas.clientWidth;
      calibrate();
      redrawOverlay();
      emit("rendered", layout);
    }

    /* The playback data, once per piece: the timeline offset needs it now,
     * the transport later. */
    function loadPlayback() {
      if (!opts.playbackUrl) {
        return;
      }
      fetch(opts.playbackUrl)
        .then((res) => res.json())
        .then((data) => {
          if (destroyed || !data || !data.ok) {
            return;
          }
          playbackData = data;
          if (osmd && layout) {
            calibrate();
            redrawOverlay();
          }
        })
        .catch(() => {});
    }

    async function loadScore() {
      setPaperStatus("Rendering score...", false);
      loadPlayback();
      if (opts.metaUrl) {
        fetch(opts.metaUrl)
          .then((res) => res.json())
          .then((metaData) => {
            const text = ScoreNotes.meterCaption(metaData);
            if (text && !destroyed) {
              caption.textContent = text;
              caption.hidden = false;
            }
          })
          .catch(() => {});
      }
      try {
        const res = await fetch(opts.musicxmlUrl);
        const text = await res.text();
        if (destroyed) {
          return;
        }
        const problem = ScoreNotes.scoreResponseProblem(res.status, res.headers.get("Content-Type") || "", text);
        if (problem) {
          setPaperStatus(`Could not load score: ${problem}`, true);
          return;
        }
        osmd = new opensheetmusicdisplay.OpenSheetMusicDisplay(osmdHost, {
          autoResize: false,
          drawTitle: false,
          drawComposer: false,
          drawPartNames: true,
        });
        await osmd.load(text);
        xmlText = text;
        /* let "Rendering score..." paint before the blocking render */
        await new Promise((resolve) => requestAnimationFrame(() => setTimeout(resolve, 0)));
        if (destroyed) {
          return;
        }
        /* the status takes room above the score: clear it before the first
         * render so the overlay is placed against the final layout */
        setPaperStatus("", false);
        render();
      } catch (err) {
        if (!destroyed) {
          osmd = null;
          setPaperStatus(`Could not load score: ${err}`, true);
        }
      }
    }

    /* Re-render once the canvas width has settled. */
    const resizeObserver = new ResizeObserver(() => {
      const width = canvas.clientWidth;
      if (width === 0 || width === lastWidth || !osmd) {
        return;
      }
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(() => {
        if (canvas.clientWidth !== 0 && canvas.clientWidth !== lastWidth) {
          render();
        }
      }, RESIZE_DELAY_MS);
    });
    resizeObserver.observe(canvas);

    function destroy() {
      destroyed = true;
      document.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("resize", onWindowResize);
      closeShortcuts();
      clearTimeout(resizeTimer);
      resizeObserver.disconnect();
      listeners.clear();
      if (overlay) {
        overlay.destroy();
        overlay = null;
      }
      osmd = null;
      layout = null;
      rootEl.innerHTML = "";
    }

    function onWindowResize() {
      applyPanelMode(false);
    }
    document.addEventListener("keydown", onKeyDown);
    window.addEventListener("resize", onWindowResize);

    applyPanelMode(true);
    syncMode();
    loadScore();

    return {
      destroy,
      osmd: () => osmd,
      layout: () => layout,
      xmlText: () => xmlText,
      rerender: render,
      on,
      mode: () => state.mode,
      setMode,
      setStatus,
      dataEl: () => dataPane,
      layers: () => ({ catalog, state: layerState }),
      select,
      clearSelection,
      selected: () => selection,
      playback: () => playbackData,
      timelineOffset: () => timelineOffset,
    };
  }

  root.ScoreWorkspace = { mount };
})(typeof window !== "undefined" ? window : globalThis);
