// Frontend logic only — no styling decisions here, no markup beyond what
// index.html already defines. Theming lives entirely in style.css.

const TOOLS = [
  { id: "analysis", label: "Analysis", active: true, render: renderAnalysisView },
  { id: "corpus", label: "Corpus", active: false },
  { id: "generator", label: "Generator", active: false, comingSoon: true },
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

function selectTool(toolId) {
  for (const tool of TOOLS) {
    tool.active = tool.id === toolId;
  }
  renderToolList();
  const selected = TOOLS.find((tool) => tool.id === toolId);
  if (selected && selected.render) {
    selected.render();
  }
}

function getContentEl() {
  return document.getElementById("content");
}

async function checkHealth() {
  const el = document.getElementById("health-status");
  try {
    const res = await fetch("/api/health/status");
    const data = await res.json();
    el.textContent = data.message;
    el.classList.add(data.ok ? "ok" : "error");
  } catch (err) {
    el.textContent = `Could not reach backend: ${err}`;
    el.classList.add("error");
  }
}

async function renderAnalysisView() {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Analysis</h1>
      <div id="piece-list-status" class="piece-list-status">Loading pieces...</div>
      <ul id="piece-list" class="piece-list"></ul>
    </section>
  `;

  const statusEl = document.getElementById("piece-list-status");
  const listEl = document.getElementById("piece-list");

  try {
    const res = await fetch("/api/analyzer/pieces");
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = `Failed to load pieces: ${data.message || "unknown error"}`;
      statusEl.classList.add("error");
      return;
    }

    if (data.pieces.length === 0) {
      statusEl.textContent = "No pieces yet.";
      return;
    }

    statusEl.textContent = "";
    for (const piece of data.pieces) {
      const li = document.createElement("li");
      li.textContent = piece.id;
      li.classList.add("clickable");
      li.addEventListener("click", () => showPieceDetail(piece.id));
      listEl.appendChild(li);
    }
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
  }
}

// Placeholder for this branch — replaced with real piece detail
// rendering in feat/analyzer-detail-view.
function showPieceDetail(id) {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Piece Detail</h1>
      <div>Loading piece ${id}...</div>
    </section>
  `;
}

renderToolList();
checkHealth();
