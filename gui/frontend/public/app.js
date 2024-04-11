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
      <div class="upload-controls">
        <input type="file" id="upload-file" accept=".mid,.midi" />
        <button id="upload-button" type="button">Upload</button>
      </div>
      <div id="upload-status" class="upload-status"></div>
    </section>
  `;

  document.getElementById("upload-button").addEventListener("click", handleUpload);

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

async function handleUpload() {
  const fileInput = document.getElementById("upload-file");
  const uploadButton = document.getElementById("upload-button");
  const statusEl = document.getElementById("upload-status");

  const file = fileInput.files[0];
  if (!file) {
    statusEl.textContent = "Select a MIDI file first.";
    statusEl.classList.add("error");
    return;
  }

  statusEl.textContent = "";
  statusEl.classList.remove("error");
  uploadButton.disabled = true;
  statusEl.textContent = "Uploading and analyzing...";

  try {
    const bytes = await file.arrayBuffer();
    const res = await fetch("/api/analyzer/pieces", { method: "POST", body: bytes });
    const data = await res.json();

    if (!data.ok) {
      statusEl.textContent = data.message || "Upload failed.";
      statusEl.classList.add("error");
      uploadButton.disabled = false;
      return;
    }

    showPieceDetail(data.id);
  } catch (err) {
    statusEl.textContent = `Could not reach backend: ${err}`;
    statusEl.classList.add("error");
    uploadButton.disabled = false;
  }
}

const PROVENANCE_LABELS = {
  present: "Analyzed",
  absent: "Hand-entered",
};

function renderBackLink() {
  const back = document.createElement("a");
  back.textContent = "Back to list";
  back.classList.add("back-link", "clickable");
  back.addEventListener("click", () => renderAnalysisView());
  return back;
}

function showPieceDetail(id) {
  const content = getContentEl();
  content.innerHTML = `
    <section class="panel">
      <h1>Piece Detail</h1>
      <div id="piece-detail-status" class="piece-detail-status">Loading piece ${id}...</div>
      <div id="piece-detail-body"></div>
    </section>
  `;

  const statusEl = document.getElementById("piece-detail-status");
  const bodyEl = document.getElementById("piece-detail-body");

  fetch(`/api/analyzer/pieces/${id}`)
    .then((res) => res.json())
    .then((data) => {
      if (!data.ok) {
        statusEl.textContent = data.message === "piece not found" ? "Piece not found" : data.message || "Piece not found";
        statusEl.classList.add("error");
        bodyEl.appendChild(renderBackLink());
        return;
      }

      statusEl.textContent = "";

      const noteCount = document.createElement("div");
      noteCount.textContent = `Note count: ${data.note_event_count}`;
      bodyEl.appendChild(noteCount);

      const list = document.createElement("ul");
      list.classList.add("interpretation-list");
      if (data.interpretations.length === 0) {
        const li = document.createElement("li");
        li.textContent = "No interpretations yet.";
        list.appendChild(li);
      } else {
        for (const interpretation of data.interpretations) {
          const li = document.createElement("li");
          const provenanceLabel = PROVENANCE_LABELS[interpretation.provenance_state] || interpretation.provenance_state;
          li.textContent = `${interpretation.type}: ${interpretation.label} — `;
          const flag = document.createElement("span");
          flag.classList.add("provenance-flag");
          flag.textContent = provenanceLabel;
          li.appendChild(flag);
          list.appendChild(li);
        }
      }
      bodyEl.appendChild(list);

      bodyEl.appendChild(renderBackLink());
    })
    .catch((err) => {
      statusEl.textContent = `Could not reach backend: ${err}`;
      statusEl.classList.add("error");
      bodyEl.appendChild(renderBackLink());
    });
}

renderToolList();
checkHealth();
