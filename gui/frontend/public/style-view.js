/* Pure helpers for the style detail (issue 7): shape the statistics of
 * GET /api/styles/<id> into rows with a share bar and a fraction text. No
 * DOM. statistics is {dimension: {category: {labels: [{label, count,
 * total}], more}}}. */
(function (root) {
  function titleOf(key) {
    const text = String(key || "").replace(/[_-]+/g, " ").trim();
    return text ? text[0].toUpperCase() + text.slice(1) : "";
  }

  function compareText(a, b) {
    return a < b ? -1 : a > b ? 1 : 0;
  }

  function shapeRow(item) {
    const count = Number(item.count) || 0;
    const total = Number(item.total) || 0;
    const fraction = total > 0 ? Math.min(1, count / total) : 0;
    return {
      label: String(item.label),
      count,
      total,
      fraction,
      percent: Math.round(fraction * 100),
      text: `${count}/${total}`,
    };
  }

  /* [{dimension, title, categories: [{category, title, rows, more,
   * moreText}]}]: dimensions and categories in the order given, rows by count
   * descending then label. Categories with nothing to show are left out. */
  function styleSummaryRows(statistics) {
    const result = [];
    for (const dimension of Object.keys(statistics || {})) {
      const categories = [];
      for (const category of Object.keys(statistics[dimension] || {})) {
        const entry = statistics[dimension][category] || {};
        const rows = (Array.isArray(entry.labels) ? entry.labels : [])
          .map(shapeRow)
          .sort((a, b) => b.count - a.count || compareText(a.label.toLowerCase(), b.label.toLowerCase()));
        const more = Number(entry.more) > 0 ? Number(entry.more) : 0;
        if (rows.length === 0 && more === 0) {
          continue;
        }
        categories.push({
          category,
          title: titleOf(category),
          rows,
          more,
          moreText: more > 0 ? `and ${more} more` : "",
        });
      }
      if (categories.length > 0) {
        result.push({ dimension, title: titleOf(dimension), categories });
      }
    }
    return result;
  }

  const api = { styleSummaryRows };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.StyleView = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
