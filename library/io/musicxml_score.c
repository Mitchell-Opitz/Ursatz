#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/events/note_event.h"
#include "io/musicxml_export.h"
#include "io/score_layout.h"
#include "io/score_clefs.h"
#include "io/score_quantize.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_spelling.h"
#include "model/events/dynamics.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/metronome_marking.h"
#include "model/time/tempo_map.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"
#include "io/musicxml_score_internal.h"

/*
 * musicxml_export_write_score: lays a score out on the measures of a MeterMap and writes it as MusicXML 3.1.
 *
 * All time is whole notes, exactly. <divisions> D (per quarter note) is the smallest whole number for which every
 * position and duration, times 4 * D, is an integer; from then on the code works in integer ticks (1/(4 * D) of a
 * whole note), so nothing is rounded and no fraction is compared. The stages:
 *   1. validate the input and gather every note;
 *   2. choose D for the notes (and the markings), then fetch the measure spans from the MeterMap (which may need a
 *      finer D, to which the ticks are rescaled);
 *   3. cut each part at every barline (and every marking position), split what crosses a cut with ties, fill gaps
 *      with rests;
 *   4. write each measure.
 */

#define MAX_DIVISIONS ((long long)INT32_MAX)

bool musicxml_score_xmlbuf_init(XmlBuf *buf) {
    buf->len = 0;
    buf->cap = 4096;
    buf->failed = false;
    buf->data = (char *)malloc(buf->cap);
    if (buf->data == NULL) {
        buf->failed = true;
        return false;
    }
    buf->data[0] = '\0';
    return true;
}

void musicxml_score_xmlbuf_free(XmlBuf *buf) {
    free(buf->data);
    buf->data = NULL;
}

void musicxml_score_xmlbuf_append(XmlBuf *buf, const char *fmt, ...) {
    if (buf->failed) {
        return;
    }
    for (;;) {
        va_list args;
        va_start(args, fmt);
        int needed = vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
        va_end(args);
        if (needed < 0) {
            buf->failed = true;
            return;
        }
        if ((size_t)needed < buf->cap - buf->len) {
            buf->len += (size_t)needed;
            return;
        }
        size_t new_cap = buf->cap * 2;
        while (new_cap < buf->len + (size_t)needed + 1) {
            new_cap *= 2;
        }
        char *grown = (char *)realloc(buf->data, new_cap);
        if (grown == NULL) {
            buf->failed = true;
            return;
        }
        buf->data = grown;
        buf->cap = new_cap;
    }
}

/* Text or attribute content with the five XML specials escaped. */
void musicxml_score_xmlbuf_append_escaped(XmlBuf *buf, const char *text) {
    for (const char *c = text; *c != '\0'; c++) {
        switch (*c) {
        case '&':
            musicxml_score_xmlbuf_append(buf, "&amp;");
            break;
        case '<':
            musicxml_score_xmlbuf_append(buf, "&lt;");
            break;
        case '>':
            musicxml_score_xmlbuf_append(buf, "&gt;");
            break;
        case '"':
            musicxml_score_xmlbuf_append(buf, "&quot;");
            break;
        case '\'':
            musicxml_score_xmlbuf_append(buf, "&apos;");
            break;
        default:
            musicxml_score_xmlbuf_append(buf, "%c", *c);
        }
    }
}

/* --- exact integer helpers --- */

/* *divisions = lcm(*divisions, den), false when that is above MAX_DIVISIONS. */
static bool lcm_into(long long *divisions, long long den) {
    long long g = rational_gcd(*divisions, den);
    long long product = 0;
    if (g == 0 || __builtin_mul_overflow(*divisions / g, den, &product) || product > MAX_DIVISIONS) {
        return false;
    }
    *divisions = product;
    return true;
}

/* The denominator of 4 * (n / d) in lowest terms: the <divisions> that makes this one time an integer. */
static TimeStatus time_denominator(long n, unsigned long d, long long *out) {
    long long four_n = 0;
    if (d == 0 || d > (unsigned long)LLONG_MAX || __builtin_mul_overflow((long long)n, 4LL, &four_n)) {
        return TIME_NOT_REPRESENTABLE;
    }
    *out = (long long)d / rational_gcd(four_n, (long long)d);
    return TIME_OK;
}

/* 4 * D * (n / d), which the caller has made an integer by choosing D. */
TimeStatus musicxml_score_time_ticks(long n, unsigned long d, long long divisions, long long *out) {
    long long four_n = 0;
    if (d == 0 || d > (unsigned long)LLONG_MAX || __builtin_mul_overflow((long long)n, 4LL, &four_n)) {
        return TIME_NOT_REPRESENTABLE;
    }
    long long g = rational_gcd(four_n, (long long)d);
    long long den = (long long)d / g;
    if (divisions % den != 0 || __builtin_mul_overflow(four_n / g, divisions / den, out)) {
        return TIME_NOT_REPRESENTABLE;
    }
    return TIME_OK;
}

static MusicXmlExportResult from_time_status(TimeStatus status) {
    return status == TIME_DIVISIONS_OVERFLOW ? MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW
                                             : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
}

#define MAX_TUPLET_ACTUAL 15 /* the most notes a written tuplet holds: a hint above it is ignored */

void musicxml_score_prep_free(Prep *prep) {
    free(prep->parts);
    free(prep->notes);
    free(prep->groups);
    free(prep->dirs);
    free(prep->dir_first);
    free(prep->measure_start);
    free(prep->measure_end);
    free(prep->measure_beat);
    free(prep->measure_four_four);
    if (prep->cuts != NULL) {
        for (size_t p = 0; p < prep->part_count; p++) {
            free(prep->cuts[p]);
        }
    }
    free(prep->cuts);
    free(prep->cut_count);
    score_quantized_destroy(&prep->quantized);
    score_clef_changes_destroy(&prep->clefs);
    if (prep->own_meter != NULL) {
        meter_map_destroy(prep->own_meter);
    }
    memset(prep, 0, sizeof(*prep));
}

static MusicXmlExportResult validate_arrays(const MusicXmlExportScore *score) {
    if (score->count > 0 && score->events == NULL) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    for (size_t i = 0; i < score->count; i++) {
        if (score->events[i] == NULL || (score->pitch_values != NULL && score->pitch_values[i] == NULL)) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
    }
    if ((score->part_count > 0) != (score->parts != NULL)) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    for (size_t i = 0; i < score->part_count; i++) {
        if (score->parts[i].id == NULL || score->parts[i].staff_count < 1) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
    }
    if ((score->tempo_marking_count > 0 && score->tempo_markings == NULL) ||
        (score->dynamics_count > 0 && score->dynamics == NULL) ||
        (score->pedal_count > 0 && score->pedals == NULL) ||
        (score->key_signature_count > 0 && score->key_signatures == NULL)) {
        return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
    }
    return MUSICXML_EXPORT_OK;
}

/* The effective parts: the supplied ones, or one default part wide enough for every staff used. */
static MusicXmlExportResult build_parts(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    prep->part_count = score->part_count > 0 ? score->part_count : 1;
    prep->parts = (PartInfo *)calloc(prep->part_count, sizeof(PartInfo));
    if (prep->parts == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    if (score->part_count > 0) {
        for (size_t i = 0; i < score->part_count; i++) {
            prep->parts[i].id = score->parts[i].id;
            prep->parts[i].name = score->parts[i].name != NULL ? score->parts[i].name : "";
            prep->parts[i].staff_count = score->parts[i].staff_count;
            prep->parts[i].midi_program =
                score->parts[i].midi_program >= 0 && score->parts[i].midi_program <= 127 ? score->parts[i].midi_program : -1;
        }
        return MUSICXML_EXPORT_OK;
    }
    int staves = 1;
    for (size_t i = 0; i < score->count && score->staff != NULL; i++) {
        if (score->staff[i] > staves) {
            staves = score->staff[i];
        }
    }
    prep->parts[0].id = "P1";
    prep->parts[0].name = "";
    prep->parts[0].staff_count = staves;
    prep->parts[0].midi_program = -1;
    return MUSICXML_EXPORT_OK;
}

static int compare_notes(const void *left, const void *right) {
    const NoteRec *a = (const NoteRec *)left;
    const NoteRec *b = (const NoteRec *)right;
    if (a->group != b->group) {
        return a->group < b->group ? -1 : 1;
    }
    if (a->start != b->start) {
        return a->start < b->start ? -1 : 1;
    }
    return a->index < b->index ? -1 : (a->index > b->index ? 1 : 0);
}

/* Replaces the played start and duration of every note with the notated ones from score_quantize, which stay in
   prep->quantized for the clefs. */
static MusicXmlExportResult apply_quantize(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    ScoreQuantized *quantized = &prep->quantized;
    ScoreQuantizeResult status = score_quantize(score->events, score->count, score->part_index, score->staff,
                                                score->meter_map, score->quantize, quantized);
    if (status != SCORE_QUANTIZE_OK) {
        return status == SCORE_QUANTIZE_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    for (size_t i = 0; i < prep->note_count && result == MUSICXML_EXPORT_OK; i++) {
        Rational start;
        Rational end;
        Rational duration;
        if (quantized->start_num[i] > (unsigned long long)LLONG_MAX || quantized->start_den[i] > (unsigned long long)LLONG_MAX ||
            quantized->end_num[i] > (unsigned long long)LLONG_MAX || quantized->end_den[i] > (unsigned long long)LLONG_MAX ||
            rational_make((long long)quantized->start_num[i], (long long)quantized->start_den[i], &start) != RATIONAL_OK ||
            rational_make((long long)quantized->end_num[i], (long long)quantized->end_den[i], &end) != RATIONAL_OK ||
            rational_cmp(end, start) <= 0 || rational_sub(end, start, &duration) != RATIONAL_OK ||
            start.num > (long long)LONG_MAX || duration.num > (long long)LONG_MAX ||
            (unsigned long long)start.den > ULONG_MAX || (unsigned long long)duration.den > ULONG_MAX) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
            break;
        }
        prep->notes[i].start_n = (long)start.num;
        prep->notes[i].start_d = (unsigned long)start.den;
        prep->notes[i].dur_n = (long)duration.num;
        prep->notes[i].dur_d = (unsigned long)duration.den;
    }
    return result;
}

/* The notes: part, staff, spelling present, exact start and duration, and their voice groups. */
static MusicXmlExportResult gather_notes(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    prep->note_count = score->count;
    prep->notes = (NoteRec *)calloc(score->count > 0 ? score->count : 1, sizeof(NoteRec));
    prep->groups = (Group *)calloc(score->count > 0 ? score->count : 1, sizeof(Group));
    if (prep->notes == NULL || prep->groups == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t i = 0; i < score->count; i++) {
        NoteEvent *event = score->events[i];
        NoteRec *note = &prep->notes[i];
        note->index = i;
        note->part = score->part_index != NULL ? score->part_index[i] : 0;
        note->staff = score->staff != NULL ? score->staff[i] : 1;
        if (note->part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (note->staff < 1 || note->staff > prep->parts[note->part].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        if (note_event_optional_pitch_spelling(event) == NULL) {
            return MUSICXML_EXPORT_ERR_MISSING_SPELLING;
        }
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
        const Duration *duration = note_event_duration(event);
        note->start_n = (long)position->value.num;
        note->start_d = (unsigned long)position->value.den;
        note->dur_n = (long)duration->value.num;
        note->dur_d = (unsigned long)duration->value.den;
        if (note->start_n < 0 || note->dur_n <= 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        unsigned actual = 0;
        unsigned normal = 0;
        if (note_event_tuplet(event, &actual, &normal) && actual <= MAX_TUPLET_ACTUAL) {
            note->actual = actual;
            note->normal = normal;
        }

        const VoiceReference *voice = note_event_optional_voice(event);
        const char *key = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
        size_t found = prep->group_count;
        int in_part = 0;
        for (size_t g = 0; g < prep->group_count; g++) {
            if (prep->groups[g].part != note->part) {
                continue;
            }
            in_part++;
            const Group *candidate = &prep->groups[g];
            bool same_voice = (key == NULL && candidate->voice == NULL) ||
                              (key != NULL && candidate->voice != NULL && strcmp(key, candidate->voice) == 0);
            if (candidate->staff == note->staff && same_voice) {
                found = g;
                break;
            }
        }
        if (found == prep->group_count) {
            prep->groups[found].part = note->part;
            prep->groups[found].staff = note->staff;
            prep->groups[found].voice = key;
            prep->groups[found].voice_number = in_part + 1;
            prep->group_count++;
        }
        note->group = found;
    }
    return prep->score->quantize != NULL ? apply_quantize(prep) : MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_keys(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->key_signature_count; i++) {
        const MusicXmlExportKeySignature *key = &score->key_signatures[i];
        if (key->part_index >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (key->sharps_flats < -7 || key->sharps_flats > 7 || key->position.value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_KEY;
        dir->part = key->part_index;
        dir->staff = 1;
        dir->raw_n = (long)key->position.value.num;
        dir->raw_d = (unsigned long)key->position.value.den;
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

/* The beat units MusicXML names, as whole-note fractions. */
static const struct {
    const char *name;
    unsigned long long numerator;
    unsigned long long denominator;
} BEAT_UNITS[] = {{"long", 4, 1},   {"breve", 2, 1},  {"whole", 1, 1},   {"half", 1, 2},    {"quarter", 1, 4},
                  {"eighth", 1, 8}, {"16th", 1, 16},  {"32nd", 1, 32},   {"64th", 1, 64},  {"128th", 1, 128}};

#define MAX_BEAT_UNIT_DOTS 4

/* The name and dot count of a beat unit n/d, false when MusicXML has no such note. */
bool musicxml_score_beat_unit_name(unsigned long n, unsigned long d, const char **name, int *dots) {
    for (size_t u = 0; u < sizeof(BEAT_UNITS) / sizeof(BEAT_UNITS[0]); u++) {
        for (int k = 0; k <= MAX_BEAT_UNIT_DOTS; k++) {
            /* base * (2^(k+1) - 1) / 2^k */
            unsigned long long want_n = BEAT_UNITS[u].numerator * ((1ull << (k + 1)) - 1);
            unsigned long long want_d = BEAT_UNITS[u].denominator * (1ull << k);
            if ((unsigned long long)n * want_d == want_n * (unsigned long long)d) {
                *name = BEAT_UNITS[u].name;
                *dots = k;
                return true;
            }
        }
    }
    return false;
}

/* num/den as a decimal with at most 12 places, trailing zeros dropped. *exact is false when it had to be rounded. */
bool musicxml_score_ratio_to_decimal(unsigned long long num, unsigned long long den, char *out, size_t size, bool *exact) {
    if (den == 0 || den > 100000000000000000ull) {
        return false;
    }
    unsigned long long whole = num / den;
    unsigned long long rem = num % den;
    char digits[13];
    int used = 0;
    while (used < 12 && rem != 0) {
        rem *= 10;
        digits[used++] = (char)('0' + rem / den);
        rem %= den;
    }
    *exact = rem == 0;
    if (rem != 0 && rem * 2 >= den) {
        int at = used - 1;
        while (at >= 0 && digits[at] == '9') {
            digits[at--] = '0';
        }
        if (at >= 0) {
            digits[at]++;
        } else {
            whole++;
        }
    }
    while (used > 0 && digits[used - 1] == '0') {
        used--;
    }
    digits[used] = '\0';
    int written = snprintf(out, size, used > 0 ? "%llu.%s" : "%llu", whole, digits);
    return written > 0 && (size_t)written < size;
}

static MusicXmlExportResult gather_tempo(Prep *prep) {
    const TempoMap *map = prep->score->tempo_map;
    if (map == NULL || tempo_map_is_defaulted(map)) {
        return MUSICXML_EXPORT_OK;
    }
    for (size_t i = 0; i < tempo_map_region_count(map); i++) {
        const TempoMapRegion *region = tempo_map_region(map, i);
        if (tempo_map_region_is_defaulted(region)) {
            continue;
        }
        if (tempo_map_region_microseconds_per_quarter(region) == 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_TEMPO;
        dir->part = 0;
        dir->staff = 1;
        MusicalTime region_start = tempo_map_region_start(region);
        dir->raw_n = (long)region_start.value.num;
        dir->raw_d = (unsigned long)region_start.value.den;
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_markings(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->tempo_marking_count; i++) {
        const TempoMarking *marking = score->tempo_markings[i];
        if (marking == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
        size_t part = score->tempo_marking_part != NULL ? score->tempo_marking_part[i] : 0;
        if (part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (tempo_marking_has_metronome_marking(marking)) {
            const MetronomeMarking *metronome = tempo_marking_metronome_marking(marking);
            const char *name = NULL;
            int dots = 0;
            char text[64];
            bool exact = false;
            if (!musicxml_score_beat_unit_name(metronome_marking_beat_unit_numerator(metronome),
                                metronome_marking_beat_unit_denominator(metronome), &name, &dots) ||
                !musicxml_score_ratio_to_decimal(metronome_marking_bpm_numerator(metronome),
                                  metronome_marking_bpm_denominator(metronome), text, sizeof(text), &exact) ||
                !exact) {
                return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
            }
        }
        const MusicalTime *position = temporal_anchor_position(tempo_marking_temporal_anchor(marking));
        if (position->value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        Dir *dir = &prep->dirs[prep->dir_count++];
        dir->kind = DIR_MARKING;
        dir->part = part;
        dir->staff = 1;
        dir->raw_n = (long)position->value.num;
        dir->raw_d = (unsigned long)position->value.den;
        dir->source = i;
    }
    return MUSICXML_EXPORT_OK;
}

/* Where a Dynamics sounds: the start of its span, or the start of the event it targets. */
static MusicXmlExportResult dynamics_position(const Prep *prep, const Dynamics *dynamics, long *n, unsigned long *d) {
    const MusicalTime *position = NULL;
    if (dynamics_target_kind(dynamics) == DYNAMICS_TARGET_TIME_SPAN) {
        position = &dynamics_target_time_span(dynamics)->start;
    } else {
        const char *id = entity_id_value(dynamics_target_event_id(dynamics));
        for (size_t i = 0; i < prep->score->count && position == NULL; i++) {
            if (strcmp(entity_id_value(note_event_id(prep->score->events[i])), id) == 0) {
                position = temporal_anchor_position(note_event_temporal_anchor(prep->score->events[i]));
            }
        }
        if (position == NULL) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_EVENT;
        }
    }
    if (position->value.num < 0) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    *n = (long)position->value.num;
    *d = (unsigned long)position->value.den;
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_dynamics(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->dynamics_count; i++) {
        const Dynamics *dynamics = score->dynamics[i];
        if (dynamics == NULL) {
            return MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT;
        }
        size_t part = score->dynamics_part != NULL ? score->dynamics_part[i] : 0;
        int staff = score->dynamics_staff != NULL ? score->dynamics_staff[i] : 1;
        if (part >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (staff < 1 || staff > prep->parts[part].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        Dir *dir = &prep->dirs[prep->dir_count];
        MusicXmlExportResult result = dynamics_position(prep, dynamics, &dir->raw_n, &dir->raw_d);
        if (result != MUSICXML_EXPORT_OK) {
            return result;
        }
        dir->kind = DIR_DYNAMIC;
        dir->part = part;
        dir->staff = staff;
        dir->source = i;
        prep->dir_count++;
    }
    return MUSICXML_EXPORT_OK;
}

/* The sustain pedals: a start at the span's start and a stop at its end, in the staff of the part they name. */
static MusicXmlExportResult gather_pedals(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    for (size_t i = 0; i < score->pedal_count; i++) {
        const MusicXmlExportPedal *pedal = &score->pedals[i];
        if (pedal->part_index >= prep->part_count) {
            return MUSICXML_EXPORT_ERR_UNKNOWN_PART;
        }
        if (pedal->staff < 1 || pedal->staff > prep->parts[pedal->part_index].staff_count) {
            return MUSICXML_EXPORT_ERR_BAD_STAFF;
        }
        if (!time_span_holds(pedal->span) || pedal->span.start.value.num < 0) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        for (int end = 0; end < 2; end++) {
            const MusicalTime *at = end == 0 ? &pedal->span.start : &pedal->span.end;
            Dir *dir = &prep->dirs[prep->dir_count++];
            dir->kind = end == 0 ? DIR_PEDAL_START : DIR_PEDAL_STOP;
            dir->part = pedal->part_index;
            dir->staff = pedal->staff;
            dir->raw_n = (long)at->value.num;
            dir->raw_d = (unsigned long)at->value.den;
            dir->source = i;
        }
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult gather_dirs(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    size_t capacity = score->key_signature_count + score->tempo_marking_count + score->dynamics_count + 2 * score->pedal_count;
    if (score->tempo_map != NULL) {
        capacity += tempo_map_region_count(score->tempo_map);
    }
    prep->dirs = (Dir *)calloc(capacity > 0 ? capacity : 1, sizeof(Dir));
    if (prep->dirs == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    MusicXmlExportResult result = gather_keys(prep);
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_tempo(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_markings(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_dynamics(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_pedals(prep);
    }
    return result;
}

/* The smallest divisions for the notes and the dirs; false (with the status) when one does not fit. */
static MusicXmlExportResult choose_divisions(Prep *prep) {
    long long divisions = 1;
    for (size_t i = 0; i < prep->note_count; i++) {
        const NoteRec *note = &prep->notes[i];
        long long den = 0;
        TimeStatus status = time_denominator(note->start_n, note->start_d, &den);
        if (status == TIME_OK && !lcm_into(&divisions, den)) {
            status = TIME_DIVISIONS_OVERFLOW;
        }
        if (status == TIME_OK) {
            status = time_denominator(note->dur_n, note->dur_d, &den);
        }
        if (status == TIME_OK && !lcm_into(&divisions, den)) {
            status = TIME_DIVISIONS_OVERFLOW;
        }
        if (status != TIME_OK) {
            return from_time_status(status);
        }
    }
    for (size_t i = 0; i < prep->dir_count; i++) {
        long long den = 0;
        TimeStatus status = time_denominator(prep->dirs[i].raw_n, prep->dirs[i].raw_d, &den);
        if (status == TIME_OK && !lcm_into(&divisions, den)) {
            status = TIME_DIVISIONS_OVERFLOW;
        }
        if (status != TIME_OK) {
            return from_time_status(status);
        }
    }
    prep->divisions = divisions;
    return MUSICXML_EXPORT_OK;
}

/* Every note's start and end in ticks at the current divisions. */
static MusicXmlExportResult notes_to_ticks(Prep *prep) {
    prep->extent = 0;
    for (size_t i = 0; i < prep->note_count; i++) {
        NoteRec *note = &prep->notes[i];
        long long length = 0;
        if (musicxml_score_time_ticks(note->start_n, note->start_d, prep->divisions, &note->start) != TIME_OK ||
            musicxml_score_time_ticks(note->dur_n, note->dur_d, prep->divisions, &length) != TIME_OK ||
            __builtin_add_overflow(note->start, length, &note->end)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (note->end > prep->extent) {
            prep->extent = note->end;
        }
    }
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult dirs_to_ticks(Prep *prep) {
    prep->dir_max_tick = -1;
    prep->dir_stop_tick = -1;
    for (size_t i = 0; i < prep->dir_count; i++) {
        Dir *dir = &prep->dirs[i];
        if (musicxml_score_time_ticks(dir->raw_n, dir->raw_d, prep->divisions, &dir->tick) != TIME_OK) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (dir->kind == DIR_PEDAL_STOP) {
            if (dir->tick > prep->dir_stop_tick) {
                prep->dir_stop_tick = dir->tick;
            }
        } else if (dir->tick > prep->dir_max_tick) {
            prep->dir_max_tick = dir->tick;
        }
    }
    return MUSICXML_EXPORT_OK;
}

/* Multiplies every tick by factor (the divisions grew by that much). */
static MusicXmlExportResult rescale_ticks(Prep *prep, long long factor) {
    for (size_t i = 0; i < prep->note_count; i++) {
        if (__builtin_mul_overflow(prep->notes[i].start, factor, &prep->notes[i].start) ||
            __builtin_mul_overflow(prep->notes[i].end, factor, &prep->notes[i].end)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
    }
    if (__builtin_mul_overflow(prep->extent, factor, &prep->extent)) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    for (size_t i = 0; i < prep->dir_count; i++) {
        if (__builtin_mul_overflow(prep->dirs[i].tick, factor, &prep->dirs[i].tick)) {
            return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
    }
    if (prep->dir_max_tick > 0 && __builtin_mul_overflow(prep->dir_max_tick, factor, &prep->dir_max_tick)) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    if (prep->dir_stop_tick > 0 && __builtin_mul_overflow(prep->dir_stop_tick, factor, &prep->dir_stop_tick)) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    return MUSICXML_EXPORT_OK;
}

MusicXmlExportResult musicxml_score_from_meter_status(MeterMapResult status) {
    return status == METER_MAP_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
}

/*
 * The measures: how many cover the content, and where each starts and ends, from the MeterMap alone. The spans'
 * denominators may need a finer divisions; if so every tick is rescaled.
 */
static MusicXmlExportResult build_measures(Prep *prep) {
    /* enough measures for the last note and for the measure that holds the last dir */
    long long covered = prep->extent;
    if (prep->dir_max_tick >= 0 && prep->dir_max_tick + 1 > covered) {
        covered = prep->dir_max_tick + 1;
    }
    if (prep->dir_stop_tick > covered) {
        covered = prep->dir_stop_tick; /* a pedal stop on a barline ends the measure before it */
    }
    MusicalTime extent_time;
    if (musical_time_make((long)covered, (long long)(4 * prep->divisions), &extent_time) != MUSICAL_TIME_OK) {
        return MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
    }
    size_t count = 0;
    MeterMapResult status = meter_map_measure_count(prep->meter, &extent_time, &count);
    if (status != METER_MAP_OK) {
        return musicxml_score_from_meter_status(status);
    }
    if (count == 0) {
        count = 1;
    }
    prep->measure_count = count;
    prep->first_measure_number = meter_map_first_measure_number(prep->meter);

    long *start_n = (long *)calloc(count, sizeof(long));
    unsigned long *start_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    long *end_n = (long *)calloc(count, sizeof(long));
    unsigned long *end_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    prep->measure_start = (long long *)calloc(count, sizeof(long long));
    prep->measure_end = (long long *)calloc(count, sizeof(long long));
    prep->measure_beat = (long long *)calloc(count, sizeof(long long));
    prep->measure_four_four = (bool *)calloc(count, sizeof(bool));
    unsigned long *beat_d = (unsigned long *)calloc(count, sizeof(unsigned long));
    unsigned long *beat_n = (unsigned long *)calloc(count, sizeof(unsigned long));
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    if (start_n == NULL || start_d == NULL || end_n == NULL || end_d == NULL || prep->measure_start == NULL ||
        prep->measure_end == NULL || prep->measure_beat == NULL || prep->measure_four_four == NULL || beat_n == NULL ||
        beat_d == NULL) {
        result = MUSICXML_EXPORT_ERR_ALLOC;
    }

    long long divisions = prep->divisions;
    for (size_t m = 0; m < count && result == MUSICXML_EXPORT_OK; m++) {
        TimeSpan span;
        status = meter_map_measure_span(prep->meter, prep->first_measure_number + (int)m, &span);
        if (status != METER_MAP_OK) {
            result = musicxml_score_from_meter_status(status);
            break;
        }
        start_n[m] = (long)span.start.value.num;
        start_d[m] = (unsigned long)span.start.value.den;
        end_n[m] = (long)span.end.value.num;
        end_d[m] = (unsigned long)span.end.value.den;
        long long den = 0;
        TimeStatus time_status = time_denominator(start_n[m], start_d[m], &den);
        if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
            time_status = TIME_DIVISIONS_OVERFLOW;
        }
        if (time_status == TIME_OK) {
            time_status = time_denominator(end_n[m], end_d[m], &den);
        }
        if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
            time_status = TIME_DIVISIONS_OVERFLOW;
        }
        if (time_status == TIME_OK) {
            /* the beat, so that a split at a beat falls on a whole tick (the quantizer's beat: see score_quantize.c) */
            const MeterMapRegion *region = NULL;
            status = meter_map_region_of_measure(prep->meter, prep->first_measure_number + (int)m, &region);
            if (status != METER_MAP_OK) {
                result = musicxml_score_from_meter_status(status);
                break;
            }
            const TimeSignature *signature = meter_map_region_time_signature(region);
            unsigned top = time_signature_top_number(signature);
            unsigned bottom = time_signature_bottom_number(signature);
            Duration beat_length;
            if (time_signature_beat_length(signature, &beat_length) != DURATION_OK ||
                beat_length.value.num > LONG_MAX || beat_length.value.den > LONG_MAX) {
                result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
                break;
            }
            beat_n[m] = (unsigned long)beat_length.value.num;
            beat_d[m] = (unsigned long)beat_length.value.den;
            prep->measure_four_four[m] = top == 4 && bottom == 4;
            time_status = time_denominator((long)beat_n[m], beat_d[m], &den);
            if (time_status == TIME_OK && !lcm_into(&divisions, den)) {
                time_status = TIME_DIVISIONS_OVERFLOW;
            }
        }
        if (time_status != TIME_OK) {
            result = from_time_status(time_status);
        }
    }

    if (result == MUSICXML_EXPORT_OK && divisions != prep->divisions) {
        result = rescale_ticks(prep, divisions / prep->divisions);
        prep->divisions = divisions;
    }
    for (size_t m = 0; m < count && result == MUSICXML_EXPORT_OK; m++) {
        if (musicxml_score_time_ticks(start_n[m], start_d[m], prep->divisions, &prep->measure_start[m]) != TIME_OK ||
            musicxml_score_time_ticks(end_n[m], end_d[m], prep->divisions, &prep->measure_end[m]) != TIME_OK) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (result == MUSICXML_EXPORT_OK &&
            (musicxml_score_time_ticks((long)beat_n[m], beat_d[m], prep->divisions, &prep->measure_beat[m]) != TIME_OK ||
             prep->measure_beat[m] <= 0)) {
            result = MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
        }
        if (result == MUSICXML_EXPORT_OK) {
            prep->measure_four_four[m] = prep->measure_four_four[m] &&
                                         prep->measure_end[m] - prep->measure_start[m] == 4 * prep->measure_beat[m];
        }
    }
    free(start_n);
    free(start_d);
    free(end_n);
    free(end_d);
    free(beat_n);
    free(beat_d);
    return result;
}

static int compare_ticks(const void *left, const void *right) {
    long long a = *(const long long *)left;
    long long b = *(const long long *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Per part, the ticks at which every note and rest is cut: each barline (and the end of the last measure). */
static int compare_dirs(const void *left, const void *right) {
    const Dir *a = (const Dir *)left;
    const Dir *b = (const Dir *)right;
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->tick != b->tick) {
        return a->tick < b->tick ? -1 : 1;
    }
    if (a->kind != b->kind) {
        return a->kind < b->kind ? -1 : 1;
    }
    return a->source < b->source ? -1 : (a->source > b->source ? 1 : 0);
}

/* Orders the dirs and finds where each part's start. */
static MusicXmlExportResult order_dirs(Prep *prep) {
    qsort(prep->dirs, prep->dir_count, sizeof(Dir), compare_dirs);
    prep->dir_first = (size_t *)calloc(prep->part_count + 1, sizeof(size_t));
    if (prep->dir_first == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    size_t at = 0;
    for (size_t p = 0; p < prep->part_count; p++) {
        prep->dir_first[p] = at;
        while (at < prep->dir_count && prep->dirs[at].part == p) {
            at++;
        }
    }
    prep->dir_first[prep->part_count] = at;
    return MUSICXML_EXPORT_OK;
}

static MusicXmlExportResult build_cuts(Prep *prep) {
    prep->cuts = (long long **)calloc(prep->part_count, sizeof(long long *));
    prep->cut_count = (size_t *)calloc(prep->part_count, sizeof(size_t));
    if (prep->cuts == NULL || prep->cut_count == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count; p++) {
        size_t dirs_of_part = prep->dir_first[p + 1] - prep->dir_first[p];
        long long *cuts = (long long *)calloc(prep->measure_count + 1 + dirs_of_part, sizeof(long long));
        if (cuts == NULL) {
            return MUSICXML_EXPORT_ERR_ALLOC;
        }
        size_t n = 0;
        for (size_t m = 0; m < prep->measure_count; m++) {
            cuts[n++] = prep->measure_start[m];
        }
        cuts[n++] = prep->measure_end[prep->measure_count - 1];
        for (size_t d = prep->dir_first[p]; d < prep->dir_first[p + 1]; d++) {
            cuts[n++] = prep->dirs[d].tick;
        }
        qsort(cuts, n, sizeof(long long), compare_ticks);
        size_t unique = 0;
        for (size_t i = 0; i < n; i++) {
            if (unique == 0 || cuts[i] != cuts[unique - 1]) {
                cuts[unique++] = cuts[i];
            }
        }
        prep->cuts[p] = cuts;
        prep->cut_count[p] = unique;
    }
    return MUSICXML_EXPORT_OK;
}

/* One strand of non-overlapping notes of a voice group; its last chord is the notes sharing chord_start/chord_end. */
typedef struct {
    size_t group;
    long long cursor; /* the end of the last note */
    long long chord_start;
    long long chord_end;
} Lane;

/*
 * Splits every voice group into lanes so no group has overlapping notes. The notes must be sorted (group, start,
 * index). A note that starts and ends with a lane's last chord joins that chord; any other goes to the lowest lane
 * that is free at its start, and a new lane is made when none is. Lane 0 stays the group itself; each further lane
 * becomes a group of its own on the same part and staff, with the next voice number unused in the part.
 */
static MusicXmlExportResult split_lanes(Prep *prep) {
    int *next_voice = (int *)calloc(prep->part_count, sizeof(int));
    if (next_voice == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t g = 0; g < prep->group_count; g++) {
        if (prep->groups[g].voice_number >= next_voice[prep->groups[g].part]) {
            next_voice[prep->groups[g].part] = prep->groups[g].voice_number + 1;
        }
    }
    Lane *lanes = NULL;
    size_t lane_count = 0;
    size_t lane_cap = 0;
    size_t home = SIZE_MAX;
    MusicXmlExportResult result = MUSICXML_EXPORT_OK;
    for (size_t i = 0; i < prep->note_count && result == MUSICXML_EXPORT_OK; i++) {
        NoteRec *note = &prep->notes[i];
        if (note->group != home) {
            home = note->group;
            lane_count = 0;
        }
        size_t chosen = lane_count;
        for (size_t l = 0; l < lane_count && chosen == lane_count; l++) {
            if (lanes[l].chord_start == note->start && lanes[l].chord_end == note->end) {
                chosen = l;
            }
        }
        for (size_t l = 0; l < lane_count && chosen == lane_count; l++) {
            if (lanes[l].cursor <= note->start) {
                chosen = l;
            }
        }
        if (chosen == lane_count) {
            if (lane_count == lane_cap) {
                size_t cap = lane_cap == 0 ? 4 : lane_cap * 2;
                Lane *grown = (Lane *)realloc(lanes, cap * sizeof(Lane));
                if (grown == NULL) {
                    result = MUSICXML_EXPORT_ERR_ALLOC;
                    break;
                }
                lanes = grown;
                lane_cap = cap;
            }
            lanes[lane_count].group = home;
            if (lane_count > 0) {
                /* at most one lane per note, so the groups array (one slot per note) has room */
                lanes[lane_count].group = prep->group_count++;
                prep->groups[lanes[lane_count].group] = prep->groups[home];
                prep->groups[lanes[lane_count].group].voice_number = next_voice[prep->groups[home].part]++;
            }
            lane_count++;
        }
        lanes[chosen].cursor = note->end;
        lanes[chosen].chord_start = note->start;
        lanes[chosen].chord_end = note->end;
        note->group = lanes[chosen].group;
    }
    free(lanes);
    free(next_voice);
    return result;
}

/* The clef of every staff at measure 1 and each change, from the notes as they are written (the notated times when the
   score is quantized). */
static MusicXmlExportResult infer_clefs(Prep *prep) {
    const MusicXmlExportScore *score = prep->score;
    int *staff_counts = (int *)calloc(prep->part_count, sizeof(int));
    if (staff_counts == NULL) {
        return MUSICXML_EXPORT_ERR_ALLOC;
    }
    for (size_t p = 0; p < prep->part_count; p++) {
        staff_counts[p] = prep->parts[p].staff_count;
    }
    ScoreClefResult status = score_clefs_infer(score->events, score->pitch_values, score->count, score->part_index,
                                               score->staff, staff_counts, prep->part_count, prep->meter,
                                               score->quantize != NULL ? &prep->quantized : NULL, &prep->clefs);
    free(staff_counts);
    if (status == SCORE_CLEFS_OK) {
        return MUSICXML_EXPORT_OK;
    }
    return status == SCORE_CLEFS_ERR_ALLOC ? MUSICXML_EXPORT_ERR_ALLOC : MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE;
}

MusicXmlExportResult musicxml_score_prepare(Prep *prep) {
    MusicXmlExportResult result = validate_arrays(prep->score);
    if (result == MUSICXML_EXPORT_OK) {
        result = build_parts(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_notes(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = gather_dirs(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = choose_divisions(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = notes_to_ticks(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = dirs_to_ticks(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        if (prep->score->meter_map != NULL) {
            prep->meter = prep->score->meter_map;
        } else {
            EntityID *id = NULL;
            if (entity_id_create("musicxml-export-meter-map", &id) != ENTITY_ID_OK) {
                return MUSICXML_EXPORT_ERR_ALLOC;
            }
            if (meter_map_create_defaulted(id, &prep->own_meter) != METER_MAP_OK) {
                entity_id_destroy(id);
                return MUSICXML_EXPORT_ERR_ALLOC;
            }
            prep->meter = prep->own_meter;
        }
        prep->meter_defaulted = meter_map_is_defaulted(prep->meter);
        result = build_measures(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = infer_clefs(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = order_dirs(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        result = build_cuts(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        qsort(prep->notes, prep->note_count, sizeof(NoteRec), compare_notes);
        result = split_lanes(prep);
    }
    if (result == MUSICXML_EXPORT_OK) {
        qsort(prep->notes, prep->note_count, sizeof(NoteRec), compare_notes);
    }
    return result;
}
