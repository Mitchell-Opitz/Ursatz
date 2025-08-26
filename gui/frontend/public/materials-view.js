/* Materials views (P12): the Motifs, Themes and Pieces tools. Renders from
 * MaterialsModel; the page globals (getContentEl, fetchJsonResult) come from
 * app.js. */
function renderMaterialsView(type) {
  const info = MaterialsModel.typeInfo(type);
  const content = getContentEl();
  content.innerHTML = `<h1 class="view-title">${info.label}</h1>`;
}
