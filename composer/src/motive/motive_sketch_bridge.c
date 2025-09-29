#include <ursatz_composer/motive_sketch_bridge.h>

#include <limits.h>
#include <string.h>

#include "motive/motive_internal.h"

/* Times in the sketch are unsigned and exact; the Composer's are signed longs over unsigned denominators. */
#define TIME_LIMIT 1000000000L

static ExactRational sketch_time(PlanTime time) {
    if (time.den == 0 || time.num < 0) {
        return msk_bad();
    }
    return msk_make((unsigned long long)time.num, time.den);
}

static bool plan_time(ExactRational time, PlanTime *out) {
    if (!msk_ok(time) || time.n > (unsigned long long)TIME_LIMIT || time.d > (unsigned long long)TIME_LIMIT) {
        return false;
    }
    return plan_time_make((long)time.n, (unsigned long)time.d, out) == PLAN_OK;
}

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static bool has_pitch_class(const int *pcs, size_t count, int pc) {
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

/* The chord whose triad holds the most notes, then the most time, the first of I V IV vi ii iii vii on a tie. */
static bool implied_chord(const MotiveSketch *sketch, MotiveSketchChord *out) {
    static const int ORDER[7] = {1, 5, 4, 6, 2, 3, 7};
    size_t best_hits = 0;
    ExactRational best_time = msk_int(0);
    bool found = false;
    for (int k = 0; k < 7; k++) {
        MotiveSketchChord chord = {msk_int(0), ORDER[k], 0, 0, false};
        int pcs[4];
        size_t count = motive_sketch_chord_tones(sketch, &chord, pcs);
        size_t hits = 0;
        ExactRational time = msk_int(0);
        for (size_t i = 0; i < sketch->note_count; i++) {
            if (has_pitch_class(pcs, count, pitch_class(sketch->notes[i].pitch))) {
                hits++;
                time = msk_add(time, sketch->notes[i].duration);
            }
        }
        if (hits > best_hits || (hits == best_hits && hits > 0 && msk_cmp(time, best_time) > 0)) {
            best_hits = hits;
            best_time = time;
            *out = chord;
            found = true;
        }
    }
    return found;
}

bool motive_to_sketch(const Motive *motive, MotiveSketch *out) {
    if (!motive || !out || motive->note_count == 0 || motive->note_count > MOTIVE_SKETCH_MAX_NOTES ||
        motive->harmony_count > MOTIVE_SKETCH_MAX_CHORDS || motive->feature_count > MOTIVE_SKETCH_MAX_FEATURES) {
        return false;
    }
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    bool pitched = motive->form != MOTIVE_FORM_RHYTHM;
    sketch.note_count = motive->note_count;
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        sketch.notes[i].pitch = pitched ? note->midi_pitch : MOTIVE_SKETCH_PITCH_UNKNOWN;
        sketch.notes[i].onset = sketch_time(note->offset);
        sketch.notes[i].duration = sketch_time(note->duration);
    }
    if (motive->key_source != MOTIVE_KEY_UNSPECIFIED && pitched) {
        sketch.has_scale = true;
        sketch.tonic_pc = motive->key.tonic_pitch_class;
        sketch.minor = motive->key.mode == PLAN_MODE_MINOR;
    }
    sketch.beat = msk_make(1, (unsigned long long)motive->meter.beat_unit);
    sketch.bar = msk_make((unsigned long long)motive->meter.beats_per_bar, (unsigned long long)motive->meter.beat_unit);
    sketch.pickup = sketch_time(motive_pickup(motive));
    sketch.length = sketch_time(motive->length);
    for (size_t i = 0; i < motive->harmony_count; i++) {
        const MotiveChord *chord = &motive->harmony[i];
        sketch.chords[i].onset = sketch_time(chord->onset);
        sketch.chords[i].degree = chord->degree;
        sketch.chords[i].alteration = chord->alteration;
        sketch.chords[i].inversion = chord->inversion;
        sketch.chords[i].seventh = chord->seventh;
    }
    sketch.chord_count = motive->harmony_count;
    for (size_t i = 0; i < motive->feature_count; i++) {
        memcpy(sketch.features[i].label, motive->features[i].label, sizeof(sketch.features[i].label));
        sketch.features[i].first = motive->features[i].first;
        sketch.features[i].last = motive->features[i].last;
    }
    sketch.feature_count = motive->feature_count;
    sketch.accompaniment = motive->role == MOTIVE_ROLE_ACCOMPANIMENT;

    if (sketch.chord_count == 0 && sketch.has_scale && motive->form == MOTIVE_FORM_FULL &&
        implied_chord(&sketch, &sketch.chords[0])) {
        sketch.chord_count = 1;
    }
    if (!motive_sketch_validate(&sketch)) {
        return false;
    }
    *out = sketch;
    return true;
}

/* The meter of a bar and a beat: beats_per_bar over a beat unit of 1, 2, 4, 8 or 16. */
static bool meter_of(ExactRational bar, ExactRational beat, PlanMeter *meter) {
    if (!msk_ok(bar) || !msk_ok(beat) || beat.n != 1 || (beat.d != 1 && beat.d != 2 && beat.d != 4 && beat.d != 8 && beat.d != 16)) {
        return false;
    }
    ExactRational beats = msk_div(bar, beat);
    if (!msk_ok(beats) || beats.d != 1 || beats.n < 1 || beats.n > 64) {
        return false;
    }
    meter->beats_per_bar = (int)beats.n;
    meter->beat_unit = (int)beat.d;
    return true;
}

bool motive_from_sketch(const MotiveSketch *sketch, const Motive *template_motive, Motive *out) {
    if (!sketch || !template_motive || !out || !motive_sketch_validate(sketch)) {
        return false;
    }
    Motive motive = *template_motive;
    PlanMeter meter = template_motive->meter;
    if (!meter_of(sketch->bar, sketch->beat, &meter)) {
        return false;
    }
    if (meter.beats_per_bar != motive.meter.beats_per_bar || meter.beat_unit != motive.meter.beat_unit) {
        motive.meter.beats_per_bar = meter.beats_per_bar;
        motive.meter.beat_unit = meter.beat_unit;
    }
    int unknown_pitch = template_motive->note_count > 0 ? template_motive->notes[0].midi_pitch : 60;
    motive.note_count = sketch->note_count;
    for (size_t i = 0; i < sketch->note_count; i++) {
        PlanNote *note = &motive.notes[i];
        memset(note, 0, sizeof(*note));
        note->midi_pitch = sketch->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN ? unknown_pitch : sketch->notes[i].pitch;
        if (!plan_time(sketch->notes[i].onset, &note->offset) || !plan_time(sketch->notes[i].duration, &note->duration)) {
            return false;
        }
    }
    if (!plan_time(sketch->pickup, &motive.pickup)) {
        return false;
    }
    if (sketch->has_scale &&
        (motive.key_source == MOTIVE_KEY_UNSPECIFIED || sketch->tonic_pc != motive.key.tonic_pitch_class ||
         sketch->minor != (motive.key.mode == PLAN_MODE_MINOR))) {
        motive.key.tonic_pitch_class = sketch->tonic_pc;
        motive.key.mode = sketch->minor ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR;
        motive.key_source = MOTIVE_KEY_GIVEN;
    }

    /* The chord motive_to_sketch added for a motive without harmony is not the motive's own. */
    bool implied_only = false;
    if (template_motive->harmony_count == 0 && sketch->chord_count == 1 && sketch->has_scale &&
        msk_is_zero(sketch->chords[0].onset)) {
        MotiveSketchChord implied;
        implied_only = implied_chord(sketch, &implied) && implied.degree == sketch->chords[0].degree &&
                       implied.alteration == sketch->chords[0].alteration &&
                       implied.inversion == sketch->chords[0].inversion && implied.seventh == sketch->chords[0].seventh;
    }
    motive.harmony_count = 0;
    for (size_t i = 0; !implied_only && i < sketch->chord_count; i++) {
        MotiveChord *chord = &motive.harmony[motive.harmony_count++];
        if (!plan_time(sketch->chords[i].onset, &chord->onset)) {
            return false;
        }
        chord->degree = sketch->chords[i].degree;
        chord->alteration = sketch->chords[i].alteration;
        chord->inversion = sketch->chords[i].inversion;
        chord->seventh = sketch->chords[i].seventh;
    }
    motive.feature_count = sketch->feature_count;
    for (size_t i = 0; i < sketch->feature_count; i++) {
        memset(&motive.features[i], 0, sizeof(motive.features[i]));
        memcpy(motive.features[i].label, sketch->features[i].label, sizeof(sketch->features[i].label));
        motive.features[i].first = sketch->features[i].first;
        motive.features[i].last = sketch->features[i].last;
    }
    motive.role = sketch->accompaniment ? MOTIVE_ROLE_ACCOMPANIMENT : MOTIVE_ROLE_MELODIC;

    /* A motive ends where its last note ends, whatever length the sketch gives. */
    if (motive_check_notes(&motive, &motive.length, NULL) != MOTIVE_OK || motive_check_extras(&motive, NULL) != MOTIVE_OK) {
        return false;
    }
    *out = motive;
    return true;
}

MotiveDevResult motive_sketch_result(MotiveRelationResult result) {
    switch (result) {
    case MOTIVE_RELATION_OK:
        return MOTIVE_DEV_OK;
    case MOTIVE_RELATION_UNCHANGED:
        return MOTIVE_DEV_ERR_UNCHANGED;
    case MOTIVE_RELATION_RANGE:
        return MOTIVE_DEV_ERR_RANGE;
    case MOTIVE_RELATION_NOT_EXPRESSIBLE:
        return MOTIVE_DEV_ERR_NOT_EXPRESSIBLE;
    case MOTIVE_RELATION_NO_SCALE:
        return MOTIVE_DEV_ERR_NO_KEY;
    case MOTIVE_RELATION_INVALID:
        return MOTIVE_DEV_ERR_INVALID;
    default:
        return MOTIVE_DEV_ERR_NOT_APPLICABLE;
    }
}
