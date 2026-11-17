#ifndef URSATZ_ANALYSIS_HARMONY_KEY_REGIONS_INTERNAL_H
#define URSATZ_ANALYSIS_HARMONY_KEY_REGIONS_INTERNAL_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "analysis/harmony/key_regions.h"
#include "model/pitch/midi_pitch.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"

/*
 * Internal to the key regions (key_regions*.c): what the stage files share. key_regions_windows.c gathers the measures and scores the
 * windows, key_regions_path.c chooses the path and builds the regions, key_regions_refine.c holds the home key and snaps the edges,
 * key_regions_phrases.c places the keys on the phrases. Not staged.
 */

/* The per-measure pitch-class weights of the segments, from the first measure that holds a segment start to
   the last. */
typedef struct {
    int *segment_measure;  /* owned; the measure each segment starts in */
    double (*weights)[12]; /* owned; measure_count rows */
    size_t measure_count;
    int first_measure;
    int last_measure;
    double (*evidence)[COMMON_PRACTICE_KEY_COUNT]; /* owned; per measure, per key: the unweighted evidence */
    double *chords;                                 /* owned; per measure: the chords starting in it */
} MeasureWeights;

#define PITCH_CLASS_BIT(pc) (1u << pitch_class_of_midi(pc))

/* --- the region path --- */

#define TIE 1e-12

/* The least-cost path when the FIRST region (the windows before the first change of key) must be one of the
   `allowed` keys (a bit mask over the 24): a two-phase search, phase 0 = still in that first region. */
#define BIG 1e300

/* The spans of the piece the events cut it into: span i starts at measure slot start[i] and its first region
   is restricted to the keys in allowed[i] (0 = unrestricted). */
typedef struct {
    size_t *start;
    unsigned *allowed;
    size_t count;
} SpanPlan;

/* key_regions_windows.c */
double key_regions_correlate(const double x[12], const double y[12]);
void key_regions_rotated_profile(const CommonPracticeKeyProfiles *profiles, int key_index, double out[12]);
bool key_regions_time_ok(const MusicalTime *time);
bool key_regions_before(const MusicalTime *a, const MusicalTime *b);
CommonPracticeKeyRegionsStatus key_regions_gather_measures(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, MeasureWeights *out);
void key_regions_measures_release(MeasureWeights *measures);
CommonPracticeKeyRegionsStatus key_regions_validate_inputs(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                       const MeterMap *map, const CommonPracticeKeyRegionsConfig *config);
CommonPracticeKeyRegionsStatus key_regions_score_windows(const MeasureWeights *measures,
                                                     const CommonPracticeKeyRegionsConfig *config,
                                                     CommonPracticeKeyWindows *out);

/* key_regions_path.c */
CommonPracticeKeyRegionsStatus key_regions_choose_path(const CommonPracticeKeyWindows *windows, double penalty,
                                                   int *path);
CommonPracticeKeyRegionsStatus key_regions_build_region(const CommonPracticeKeyRegionsConfig *config, size_t number,
                                                    int key_index, const MeterMap *map, int first_measure,
                                                    int last_measure, double uncertainty,
                                                    CommonPracticeRegionSource source, CommonPracticeKeyRegion *out);
double key_regions_region_uncertainty(const MeasureWeights *measures, size_t from, size_t to, int key_index,
                                 const CommonPracticeKeyProfiles *profiles);
CommonPracticeKeyRegionsStatus key_regions_choose_path_restricted(const CommonPracticeKeyWindows *windows, double penalty,
                                                              unsigned allowed, int *path);
CommonPracticeKeyRegionsStatus key_regions_nearest_barline(const MeterMap *map, const MusicalTime *position,
                                                       int *out_measure);
CommonPracticeKeyRegionsStatus key_regions_plan_spans(const CommonPracticeKeySignatureEvent *events, size_t event_count,
                                                  const MeterMap *map, const MeasureWeights *measures,
                                                  SpanPlan *plan);

/* key_regions_refine.c */
void key_regions_hold_home_keys(const CommonPracticeKeySegment *segments, size_t segment_count,
                           const MeasureWeights *measures, const bool *fixed, double tolerance, int *measure_key);
CommonPracticeKeyRegionsStatus key_regions_snap_edges(int *measure_key, const bool *fixed, size_t measure_count,
                                                  const size_t *boundaries, size_t boundary_count, size_t reach);
CommonPracticeKeyRegionsStatus key_regions_find_pivot(const CommonPracticeKeySegment *segments, size_t segment_count,
                                                  const MeasureWeights *measures, int boundary_measure,
                                                  const MusicalTime *boundary_start, int old_key, int new_key,
                                                  size_t search_measures, bool strictly_before, bool *found,
                                                  size_t *pivot);

/* key_regions_phrases.c */
CommonPracticeKeyRegionsStatus key_regions_place_keys_on_phrases(const CommonPracticeKeyRegionsConfig *config,
                                                            const MeasureWeights *measures, const SpanPlan *plan,
                                                            int *measure_key, bool *forced, bool *arrival_slot);

#endif
