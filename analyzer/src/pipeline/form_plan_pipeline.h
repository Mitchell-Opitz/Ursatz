#ifndef URSATZ_ANALYZER_FORM_PLAN_PIPELINE_H
#define URSATZ_ANALYZER_FORM_PLAN_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>

#include "pipeline/form_pipeline.h"

/*
 * The form plan: sections and thematic returns over the phrases and themes of the form stage. Sections are the
 * kernel's (analysis/form_structure_detection.h) over one PhraseOccurrence per phrase with its cadence type;
 * returns are the kernel's thematic return detection (analysis/thematic_return_detection.h) over each theme's
 * soprano notes, run as Ursatz's tests/integration/framework_v1_thematic_returns.c runs it (R-053). The sections
 * are reported only: they do not feed back into the cadences or the key regions (R-054).
 *
 * The analyzer adds no music theory (R-018). What it adds is bookkeeping: the phrases the kernel leaves out of its
 * sections (those after the last closing cadence) form a last section so the sections tile the phrases; returns
 * between themes that share a phrase are dropped; and the section letters, which follow from the returns alone.
 *
 * Needs the report's phrases, themes, cadences and entries, as form_stage_collect leaves them. A report without
 * phrases is left without sections. Returns false only on allocation or library failure.
 */
bool form_plan_stage_collect(const FormStage *stage, AnalysisReport *report);

#endif
