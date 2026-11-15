// The Export control: fetch the Analyzer's text export of what a view shows, then Copy it or Download it as .txt.
// The text is written by the Analyzer (docs/text-export.md); this file builds no line of it. The pure helpers
// (exportFilename, rangeQuery) are loaded by frontend/tests/export-control.test.js without a DOM.

/* "Sonata No. 1.mid" -> "sonata-no-1-mid.txt"; an empty name gives "export.txt". */
function exportFilename(name) {
  const slug = String(name === null || name === undefined ? "" : name)
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
  return slug === "" ? "export.txt" : `${slug}.txt`;
}

function isMeasureNumber(text) {
  return /^\d+$/.test(text);
}

/* The query string for a measure range: "" when both ends are blank, "?from=a&to=b", or one end only. null when an
   end is not a whole number or from is greater than to. */
function rangeQuery(from, to) {
  const fromText = String(from === null || from === undefined ? "" : from).trim();
  const toText = String(to === null || to === undefined ? "" : to).trim();
  if ((fromText !== "" && !isMeasureNumber(fromText)) || (toText !== "" && !isMeasureNumber(toText))) {
    return null;
  }
  if (fromText !== "" && toText !== "" && Number(fromText) > Number(toText)) {
    return null;
  }
  const parts = [];
  if (fromText !== "") {
    parts.push(`from=${Number(fromText)}`);
  }
  if (toText !== "") {
    parts.push(`to=${Number(toText)}`);
  }
  return parts.length === 0 ? "" : `?${parts.join("&")}`;
}

/* Copies text to the clipboard: navigator.clipboard when it exists, else a selected hidden textarea and
   document.execCommand("copy"). Resolves true on success. */
async function copyText(text) {
  if (typeof navigator !== "undefined" && navigator.clipboard && navigator.clipboard.writeText) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch (error) {
      // Fall through to the textarea route.
    }
  }
  const area = document.createElement("textarea");
  area.value = text;
  area.setAttribute("readonly", "");
  area.style.position = "fixed";
  area.style.opacity = "0";
  document.body.appendChild(area);
  area.select();
  let copied = false;
  try {
    copied = document.execCommand("copy");
  } catch (error) {
    copied = false;
  }
  area.remove();
  return copied;
}

function downloadText(text, filename) {
  const url = URL.createObjectURL(new Blob([text], { type: "text/plain;charset=utf-8" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 0);
}

/* options: fetchText(query) -> Promise<string> (rejects with an Error whose message is shown), filename (a string or
   a function returning one; passed through exportFilename), withRange (adds From and To measure inputs). Returns the
   control's element. */
function create(options) {
  const root = document.createElement("div");
  root.className = "export-control";

  let from = null;
  let to = null;
  if (options.withRange) {
    from = document.createElement("input");
    to = document.createElement("input");
    for (const [input, label] of [
      [from, "From measure"],
      [to, "To measure"],
    ]) {
      input.type = "text";
      input.inputMode = "numeric";
      input.size = 4;
      input.placeholder = label.split(" ")[0];
      input.setAttribute("aria-label", label);
    }
  }

  const exportButton = document.createElement("button");
  exportButton.type = "button";
  exportButton.textContent = "Export";
  const copyButton = document.createElement("button");
  copyButton.type = "button";
  copyButton.textContent = "Copy";
  copyButton.hidden = true;
  const downloadButton = document.createElement("button");
  downloadButton.type = "button";
  downloadButton.textContent = "Download .txt";
  downloadButton.hidden = true;
  const message = document.createElement("span");
  message.className = "export-message";
  message.setAttribute("role", "status");

  if (from) {
    root.append(from, to);
  }
  root.append(exportButton, copyButton, downloadButton, message);

  let text = "";
  let copiedTimer = null;

  function showError(error) {
    text = "";
    copyButton.hidden = true;
    downloadButton.hidden = true;
    message.textContent = error;
  }

  // A changed range makes the fetched text stale: back to Export.
  function reset() {
    text = "";
    copyButton.hidden = true;
    downloadButton.hidden = true;
    message.textContent = "";
  }
  if (from) {
    from.addEventListener("input", reset);
    to.addEventListener("input", reset);
  }

  exportButton.addEventListener("click", async () => {
    const query = from ? rangeQuery(from.value, to.value) : "";
    if (query === null) {
      showError("From and To must be measure numbers, From not after To.");
      return;
    }
    exportButton.disabled = true;
    message.textContent = "Exporting...";
    try {
      text = await options.fetchText(query);
      copyButton.hidden = false;
      downloadButton.hidden = false;
      message.textContent = "";
    } catch (error) {
      showError(error && error.message ? error.message : "Export failed.");
    } finally {
      exportButton.disabled = false;
    }
  });

  copyButton.addEventListener("click", async () => {
    const copied = await copyText(text);
    if (!copied) {
      message.textContent = "Copy failed. Use Download .txt.";
      return;
    }
    message.textContent = "";
    copyButton.textContent = "Copied";
    clearTimeout(copiedTimer);
    copiedTimer = setTimeout(() => {
      copyButton.textContent = "Copy";
    }, 2000);
  });

  downloadButton.addEventListener("click", () => {
    const name = typeof options.filename === "function" ? options.filename() : options.filename;
    downloadText(text, exportFilename(name));
  });

  return root;
}

const ExportControl = { exportFilename, rangeQuery, create };

if (typeof module !== "undefined" && module.exports) {
  module.exports = ExportControl;
}
