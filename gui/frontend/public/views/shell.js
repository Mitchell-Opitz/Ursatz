import ScoreNotes from "../score/score-notes.js";
import SubjectsPanel from "./subjects-panel.js";
import TagsPanel from "./tags-panel.js";
import Vocabulary from "../core/vocabulary.js";
import { events as Events } from "../core/events.js";
import { loadLibraryPieces, renderLibraryView } from "./library.js";
import { fetchExportText, leaveWorkspace, renderAnalysisTool, showAnalysisView } from "./analysis.js";
import { renderCorpusView } from "./collections.js";
import { buildStyleFromPanelRow } from "./styles.js";
import { mountProjectPicker, refreshMaterialsView, renderMaterialsDetail, renderMaterialsView } from "./materials-view.js";
import { openSubject, renderSubjectsView } from "./subjects-view.js";
import { reloadMotiveLinkRuns, renderMotiveLinksView } from "./motive-links-view.js";
import { setShell } from "./shell-context.js";
import { fetchJsonResult } from "../core/api.js";

// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "library", label: "Library", active: true, render: renderLibraryView },
  { id: "analysis", label: "Analysis", active: false, render: renderAnalysisTool },
  { id: "corpus", label: "Collections", active: false, render: renderCorpusView },
  { id: "motifs", label: "Motifs", active: false, render: () => renderMaterialsView("motif") },
  { id: "themes", label: "Themes", active: false, render: () => renderMaterialsView("theme") },
  { id: "pieces", label: "Pieces", active: false, render: () => renderMaterialsView("piece") },
  { id: "subjects", label: "Subjects", active: false, render: renderSubjectsView },
  { id: "motive-links", label: "Motive links", active: false, render: renderMotiveLinksView },
];

function renderToolList() {
  const list = document.getElementById("tool-list");
  list.innerHTML = "";
  for (const tool of TOOLS) {
    const li = document.createElement("li");
    li.textContent = tool.comingSoon ? `${tool.label} (soon)` : tool.label;
    if (tool.active) {
      li.classList.add("active");
    }
    if (tool.render) {
      li.classList.add("clickable");
      li.addEventListener("click", () => selectTool(tool.id));
    }
    list.appendChild(li);
  }
}

function setActiveTool(toolId) {
  for (const tool of TOOLS) {
    tool.active = tool.id === toolId;
  }
  renderToolList();
}

function selectTool(toolId) {
  leaveWorkspace();
  setActiveTool(toolId);
  if (toolId !== "library" && tagsPanel) {
    tagsPanel.setActive(null);
  }
  const selected = TOOLS.find((tool) => tool.id === toolId);
  if (selected && selected.render) {
    selected.render();
  }
}

function getContentEl() {
  return document.getElementById("content");
}

/* Fetches a MusicXML file and draws it with OSMD (vendored under
 * vendor/opensheetmusicdisplay/, loaded globally as
 * window.opensheetmusicdisplay -- see index.html) into containerEl; progress
 * and errors go to statusEl. Resolves to {osmd, xmlText}, or null when it
 * could not be drawn. zoom is optional (small previews). */
async function drawMusicXmlScore(url, containerEl, statusEl, zoom) {
  statusEl.textContent = "Loading score...";
  statusEl.classList.remove("error");
  containerEl.innerHTML = "";
  try {
    const res = await fetch(url);
    const xmlText = await res.text();
    const problem = ScoreNotes.scoreResponseProblem(res.status, res.headers.get("Content-Type") || "", xmlText);
    if (problem) {
      statusEl.textContent = `Could not load score: ${problem}`;
      statusEl.classList.add("error");
      return null;
    }
    const osmd = new opensheetmusicdisplay.OpenSheetMusicDisplay(containerEl);
    await osmd.load(xmlText);
    if (zoom) {
      osmd.zoom = zoom;
    }
    osmd.render();
    statusEl.textContent = "";
    return { osmd, xmlText };
  } catch (err) {
    statusEl.textContent = `Could not load score: ${err}`;
    statusEl.classList.add("error");
    return null;
  }
}

/* The sidebar Tags panel (tags-panel.js). */
let tagsPanel = null;

async function fetchTagsPanelGroups() {
  const result = await fetchJsonResult("/api/analyzer/groups");
  if (!(result.ok && result.data && result.data.ok)) {
    throw new Error("groups unavailable");
  }
  return result.data;
}

/* Selects the Library tool already filtered to a Tags panel group. */
function openLibraryWithFilter(filter) {
  leaveWorkspace();
  setActiveTool("library");
  renderLibraryView(filter);
}

function mountTagsPanel() {
  const el = document.getElementById("tags-panel");
  tagsPanel = TagsPanel.mount(el, {
    fetchGroups: fetchTagsPanelGroups,
    onSelect: openLibraryWithFilter,
    onBuildStyle: buildStyleFromPanelRow,
  });
  tagsPanel.refresh();
}

/* The sidebar Subjects panel; null until mounted. Call notifySubjectsChanged()
 * after any subject or link change so its counts follow. */
let subjectsPanel = null;

async function fetchSubjectGroups() {
  const result = await fetchJsonResult("/api/subjects/groups");
  if (!(result.ok && result.data && result.data.ok)) {
    throw new Error("subject groups unavailable");
  }
  return result.data.groups;
}

function notifySubjectsChanged() {
  if (subjectsPanel) {
    subjectsPanel.refresh();
  }
}

function mountSubjectsPanel() {
  subjectsPanel = SubjectsPanel.mount(document.getElementById("subjects-panel"), {
    fetchGroups: fetchSubjectGroups,
    onSelect: openSubject,
  });
  subjectsPanel.refresh();
}

/* The open view follows the workshop (R-648): an event that says what the view shows changed reloads it, a burst of them once; "reset" (this page's
 * queue overflowed) reloads whatever is open. The page's own saves reload their view too: the extra read is the price of one way to follow a change. */
function activeToolId() {
  const active = TOOLS.find((tool) => tool.active);
  return active ? active.id : "";
}

function startFollowingEvents() {
  if (typeof Events === "undefined" || !Events.available()) {
    return;
  }
  const pending = new Map();
  function later(key, fn) {
    clearTimeout(pending.get(key));
    pending.set(key, setTimeout(() => {
      pending.delete(key);
      fn();
    }, 300));
  }
  const reloadLibrary = () => later("library", () => activeToolId() === "library" && loadLibraryPieces());
  const reloadMaterials = () => later("materials", () => ["motifs", "themes", "pieces"].includes(activeToolId()) && refreshMaterialsView());
  const reloadCollections = () => later("collections", () => activeToolId() === "corpus" && renderCorpusView());
  const reloadSubjects = () =>
    later("subjects", () => {
      notifySubjectsChanged();
      if (activeToolId() === "subjects") {
        renderSubjectsView();
      }
    });
  const reloadLinks = () => later("links", () => activeToolId() === "motive-links" && reloadMotiveLinkRuns && reloadMotiveLinkRuns());
  Events.on("library-changed", reloadLibrary);
  Events.on("item-changed", reloadMaterials);
  Events.on("item-removed", reloadMaterials);
  Events.on("collection-changed", reloadCollections);
  Events.on("subject-changed", reloadSubjects);
  Events.on("links-changed", reloadLinks);
  Events.on("reset", () => {
    reloadLibrary();
    reloadMaterials();
    reloadCollections();
    reloadSubjects();
    reloadLinks();
  });
}

/* The Composer's words are loaded before any view mounts (the models read their lists from them). */
export async function boot() {
  try {
    await Vocabulary.load();
  } catch (err) {
    getContentEl().innerHTML = `<section class="panel"><h1>Composer unavailable</h1><p class="error">${err.message}</p></section>`;
    return;
  }
  setShell({
    contentEl: getContentEl,
    showAnalysis: showAnalysisView,
    openItem: renderMaterialsDetail,
    setActiveTool,
    activeToolId,
    leaveWorkspace,
    drawScore: drawMusicXmlScore,
    fetchExportText,
    notifySubjectsChanged,
    setTagsActive: (filter) => {
      if (tagsPanel) {
        tagsPanel.setActive(filter);
      }
    },
    refreshTags: () => {
      if (tagsPanel) {
        tagsPanel.refresh();
      }
    },
    selectTool,
    showCollections: renderCorpusView,
  });
  mountTagsPanel();
  mountSubjectsPanel();
  mountProjectPicker();
  selectTool("library");
  startFollowingEvents();
}

