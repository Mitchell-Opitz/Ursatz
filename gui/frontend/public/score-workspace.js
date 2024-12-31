/* The score workspace (R-099): a toolbar, a Layers rail, the score on white
 * paper and an Inspector, mounted into any element. It knows the score only
 * through the URLs and data it is given, so the Composer can mount it too.
 *
 *   const ws = ScoreWorkspace.mount(rootEl, {
 *     title, subtitle,           toolbar text
 *     musicxmlUrl, metaUrl,      the score and its meter caption
 *     document, noteXmlIds,      the analysis, for the layers and the inspector
 *     actions: [{label, onClick}],  extra toolbar buttons
 *     onBack,                    the Library link
 *   });
 *   ws.on("rendered", (layout) => ...);
 *   ws.on("modechange", (mode) => ...);
 *
 * Needs OSMD (window.opensheetmusicdisplay), ScoreNotes, ScoreGeometry and
 * WorkspaceState loaded first. */
(function (root) {
  const RESIZE_DELAY_MS = 150;

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

  function mount(rootEl, options) {
    const opts = options || {};
    const state = WorkspaceState.defaultUiState();
    const listeners = new Map();
    let osmd = null;
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
    rail.append(el("h2", "ws-panel-title", "Layers"), el("p", "ws-empty", "Layers appear here."));

    const main = el("main", "ws-main");
    const canvas = el("div", "ws-canvas");
    const caption = el("div", "ws-caption");
    caption.hidden = true;
    const paper = el("div", "ws-paper");
    const paperStatus = el("div", "ws-paper-status");
    const osmdHost = el("div", "ws-osmd");
    paper.append(paperStatus, osmdHost);
    canvas.append(caption, paper);
    main.appendChild(canvas);

    const dataPane = el("div", "ws-data");

    const inspector = el("aside", "ws-panel ws-inspector");
    inspector.setAttribute("aria-label", "Inspector");
    inspector.append(
      el("h2", "ws-panel-title", "Inspector"),
      el("p", "ws-empty", "Click a highlight on the score to see its evidence."),
    );

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
      syncMode();
      if (mode === "score") {
        canvas.scrollTop = savedScrollTop;
      }
      emit("modechange", mode);
    }

    function syncMode() {
      ws.dataset.mode = state.mode;
      for (const [id, button] of Object.entries(modeButtons)) {
        button.setAttribute("aria-pressed", String(id === state.mode));
      }
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
      const anchor = captureAnchor();
      osmd.zoom = state.zoom;
      osmd.render();
      layout = readLayout();
      restoreAnchor(anchor);
      lastWidth = canvas.clientWidth;
      emit("rendered", layout);
    }

    async function loadScore() {
      setPaperStatus("Rendering score...", false);
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
        render();
        setPaperStatus("", false);
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
      clearTimeout(resizeTimer);
      resizeObserver.disconnect();
      listeners.clear();
      osmd = null;
      layout = null;
      rootEl.innerHTML = "";
    }

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
    };
  }

  root.ScoreWorkspace = { mount };
})(typeof window !== "undefined" ? window : globalThis);
