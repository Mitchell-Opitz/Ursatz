#ifndef URSATZ_COMPOSER_STYLE_INTERNAL_H
#define URSATZ_COMPOSER_STYLE_INTERNAL_H

#include <stdbool.h>

#include <theory/style/style_reference.h>

#include <ursatz_composer/fill_loop.h>
#include <plan/plan.h>
#include <ursatz_composer/style.h>

/* Adds categories to a style's without list (copied, repeats skipped), for a blend that keeps both lists. */
ComposerStyleResult composer_style_add_without(ComposerStyle *style, const char *const *categories, size_t count);

/* The model with the without list applied, prepared once for the distances (style_reference_create); NULL when
   the library refused the list. */
const StyleReference *composer_style_reference(const ComposerStyle *style);

/* True when category is in the style's without list. */
bool composer_style_without_has(const ComposerStyle *style, const char *category);

#endif
