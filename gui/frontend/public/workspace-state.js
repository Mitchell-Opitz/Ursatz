/* The pure state of the score workspace: zoom steps, which panel layout fits
 * a window width, the keyboard shortcut table and the saved interface state.
 * Later branches append shortcut rows and helpers here.
 *
 * Pure functions, no DOM access, so Node tests load this file directly. */
(function (root) {
  const ZOOM_STEPS = [0.5, 0.625, 0.75, 0.875, 1, 1.25, 1.5, 1.75, 2];
  const ZOOM_EPSILON = 1e-9;

  /* The next zoom step above (direction > 0) or below (direction < 0) the
   * current zoom, which need not be a step itself. Clamped to the range. */
  function nextZoom(current, direction) {
    const first = ZOOM_STEPS[0];
    const last = ZOOM_STEPS[ZOOM_STEPS.length - 1];
    if (!(direction > 0) && !(direction < 0)) {
      return Math.min(last, Math.max(first, current));
    }
    if (direction > 0) {
      const above = ZOOM_STEPS.find((step) => step > current + ZOOM_EPSILON);
      return above === undefined ? last : above;
    }
    for (let i = ZOOM_STEPS.length - 1; i >= 0; i--) {
      if (ZOOM_STEPS[i] < current - ZOOM_EPSILON) {
        return ZOOM_STEPS[i];
      }
    }
    return first;
  }

  function zoomLabel(zoom) {
    return `${Math.round(zoom * 100)}%`;
  }

  /* docked: rail and inspector sit beside the score. inspector-overlay: the
   * rail is docked and the inspector floats over the score's right side.
   * overlay: both float. */
  function panelMode(viewportWidth) {
    if (viewportWidth >= 1280) {
      return "docked";
    }
    if (viewportWidth >= 1024) {
      return "inspector-overlay";
    }
    return "overlay";
  }

  /* One row per key. alt lists further keys that do the same. */
  const SHORTCUTS = [
    { key: "L", label: "Layers panel", action: "toggle-layers" },
    { key: "I", label: "Inspector panel", action: "toggle-inspector" },
    { key: "F", label: "Focus mode", action: "toggle-focus" },
    { key: "+", alt: ["="], label: "Zoom in", action: "zoom-in" },
    { key: "-", label: "Zoom out", action: "zoom-out" },
    { key: "0", label: "Zoom to 100%", action: "zoom-reset" },
    { key: "[", label: "Previous item in the selected layer", action: "select-previous" },
    { key: "]", label: "Next item in the selected layer", action: "select-next" },
    { key: "?", label: "Shortcut list", action: "toggle-shortcuts" },
    { key: "Escape", label: "Close a panel, the shortcut list or clear the selection", action: "escape" },
  ];

  function isTypingTarget(target) {
    if (!target) {
      return false;
    }
    const tag = String(target.tagName || "").toUpperCase();
    return tag === "INPUT" || tag === "SELECT" || tag === "TEXTAREA" || target.isContentEditable === true;
  }

  /* The shortcut row an event triggers, or null. eventLike has key, target,
   * ctrlKey, metaKey and altKey. Typing in a field and Ctrl, Meta or Alt
   * combinations never trigger one. */
  function shortcutFor(eventLike) {
    if (!eventLike || typeof eventLike.key !== "string") {
      return null;
    }
    if (isTypingTarget(eventLike.target) || eventLike.ctrlKey || eventLike.metaKey || eventLike.altKey) {
      return null;
    }
    const pressed = eventLike.key.length === 1 ? eventLike.key.toLowerCase() : eventLike.key;
    for (const row of SHORTCUTS) {
      for (const key of [row.key, ...(row.alt || [])]) {
        const wanted = key.length === 1 ? key.toLowerCase() : key;
        if (wanted === pressed) {
          return row;
        }
      }
    }
    return null;
  }

  const UI_STATE_KEY = "ursatz.workspace.v1";

  function defaultUiState() {
    return { layersOpen: true, inspectorOpen: true, zoom: 1, mode: "score" };
  }

  /* The saved state with defaults for anything missing or of the wrong type.
   * A missing or throwing storage gives the defaults. */
  function loadUiState(storage) {
    const state = defaultUiState();
    let saved = null;
    try {
      const text = storage ? storage.getItem(UI_STATE_KEY) : null;
      saved = text ? JSON.parse(text) : null;
    } catch (err) {
      saved = null;
    }
    if (!saved || typeof saved !== "object" || Array.isArray(saved)) {
      return state;
    }
    if (typeof saved.layersOpen === "boolean") {
      state.layersOpen = saved.layersOpen;
    }
    if (typeof saved.inspectorOpen === "boolean") {
      state.inspectorOpen = saved.inspectorOpen;
    }
    if (ZOOM_STEPS.includes(saved.zoom)) {
      state.zoom = saved.zoom;
    }
    if (saved.mode === "score" || saved.mode === "data") {
      state.mode = saved.mode;
    }
    return state;
  }

  function saveUiState(storage, state) {
    try {
      if (storage) {
        storage.setItem(UI_STATE_KEY, JSON.stringify(state));
      }
    } catch (err) {
      /* storage full or blocked: the state is simply not remembered */
    }
  }

  const api = {
    ZOOM_STEPS,
    nextZoom,
    zoomLabel,
    panelMode,
    SHORTCUTS,
    shortcutFor,
    UI_STATE_KEY,
    defaultUiState,
    loadUiState,
    saveUiState,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.WorkspaceState = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
