# Placement rule

Where an analysis algorithm lives: kernel, framework, chain or app. [Docs index](README.md)

## Placement rule

Every analysis algorithm is library code (R-217). The kernel owns what needs no theory; a framework owns what one theory defines; the chain owns what joins them. The Analyzer orchestrates, combines, caches and exports: it calls owners, places their results in its report, stores them and renders documents. It holds no copy of an algorithm a library owns. What stays in the Analyzer is the names of its own style statistics (`style_label_interval`, `_contour`, `_duration`, `_tempo`, `_density`, `_join`, `_nct_figure`, `_progression` and the style dimension and category tables), the window search and scoring of similar material over stored documents, motive link caching and scope reading, report placement, and document rendering. The Composer and the GUI follow the same rule: a theory rule either app needs is added here first and the app calls it.
