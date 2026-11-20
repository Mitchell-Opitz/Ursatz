/* What the shell gives the views (R-652): the content area, the page's JSON request and the moves between views. The shell sets it once at boot
 * (setShell), so a view never imports the shell or a sibling view that imports it back; a test sets a fake. */
let current = null;

export function setShell(context) {
  current = context;
}

/* {contentEl(), send(url, options), showAnalysis(id, options), openItem(type, id, analysisError), setActiveTool(id), activeToolId(), leaveWorkspace(),
 *  drawScore(url, containerEl, statusEl, zoom), fetchExportText(url), notifySubjectsChanged()} */
export function shell() {
  if (current === null) {
    throw new Error("the shell has not been set");
  }
  return current;
}
