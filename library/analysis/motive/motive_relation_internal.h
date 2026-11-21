#ifndef URSATZ_ANALYSIS_MOTIVE_MOTIVE_RELATION_INTERNAL_H
#define URSATZ_ANALYSIS_MOTIVE_MOTIVE_RELATION_INTERNAL_H

/* What the files of the relation catalog share (motive_relation.c holds the bodies): private to analysis/motive, not staged. */

#include "analysis/motive/motive_relation.h"


bool motive_relation_pitched(const MotiveSketch *sketch);
Rational motive_relation_note_end(const MotiveSketchNote *note);
void motive_relation_settle(MotiveSketch *sketch);
bool motive_relation_midi(long pitch);
bool motive_relation_nice_time(const MotiveSketch *source, Rational duration);
MotiveRelationResult motive_relation_scale_time(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out, bool augment);
bool motive_relation_insert_notes(const MotiveSketch *in, MotiveSketch *out, size_t at, const MotiveSketchNote *added, size_t count);
MotiveRelationResult motive_relation_prepare_harmony(MotiveSketch *work);
void motive_relation_adapt_melody(const MotiveSketch *before, MotiveSketch *sketch);
MotiveRelationResult motive_relation_apply_exact(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_transpose(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_invert(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_retrograde(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_interval_change(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_fragment(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_liquidate(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_augment(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_diminish(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_note_repeat(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_rhythm_repeat(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_rhythm_shift(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_add_upbeat(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);
MotiveRelationResult motive_relation_apply_change_meter(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);

#endif
