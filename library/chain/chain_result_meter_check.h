#ifndef URSATZ_CHAIN_CHAIN_RESULT_METER_CHECK_H
#define URSATZ_CHAIN_CHAIN_RESULT_METER_CHECK_H

#include <stdbool.h>
#include <stddef.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>

/*
 * The meter checked against the harmonic rhythm and the cadences (R-110): the chain's collector for the library's meter check
 * (analysis/meter/meter_analysis_check.h owns the candidates, the scoring and STANDARD_METER_CHECK_MIN_MARGIN). The evidence is the
 * harmony changes (report entries as harmonic_rhythm_change_flags reads them, the first segment included), the cadence arrivals and
 * every note onset; the effective reading is the analysis's own meter map. The check reports the best other reading and never
 * applies one.
 */

/* Fills *out from the chord segments and the cadences (indices into entries). present is false when there are no
   segments. Returns false only on allocation failure. */
bool meter_check_run(const ImportedScore *notes, const ChainEntry *entries, size_t entry_count,
                     const AnalysisCadence *cadences, size_t cadence_count, AnalysisMeterCheck *out);

/* Fills report->meter_check from report->entries and report->cadences; run after the harmonic rhythm stage. */
bool meter_check_stage_collect(const ImportedScore *notes, ChainResult *report);

#endif
