#include <ursatz_composer/corpus_seed.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <model/core/rational.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/tonal_key.h>
#include "theory/style/style_labels.h"

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/harmony_phrase.h>
#include "structure/request_parse.h"
#include "util/util.h"
#include "motive/motive_internal.h"
#include <plan/plan_common.h>
#include <model/pitch/pitch_arithmetic.h>
#include <common/json.h>

/* The beats of a passage's places are compared as doubles; two are the same place within this. */
#define BEAT_EPSILON 1e-6

const char *corpus_seed_result_name(CorpusSeedResult result) {
    switch (result) {
    case CORPUS_SEED_OK:
        return "ok";
    case CORPUS_SEED_ERR_ARGUMENT:
        return "invalid argument";
    case CORPUS_SEED_ERR_NO_MATCH:
        return "no passage can seed this";
    case CORPUS_SEED_ERR_ALLOC:
        return "out of memory";
    }
    return "unknown";
}

/* --- reading the passages ---------------------------------------------------------- */

void corpus_passages_destroy(CorpusPassages *passages) {
    if (!passages) {
        return;
    }
    for (size_t i = 0; i < passages->count; i++) {
        free(passages->items[i].notes);
        free(passages->items[i].chords);
    }
    free(passages->items);
    memset(passages, 0, sizeof(*passages));
}

static void fail_at(char *path, size_t path_size, char *message, size_t message_size, const char *where, const char *text) {
    if (path) {
        snprintf(path, path_size, "%s", where);
    }
    if (message) {
        snprintf(message, message_size, "%s", text);
    }
}

static const cJSON *member(const cJSON *object, const char *name) {
    return object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
}

static bool whole_number(const cJSON *item, long long minimum, long long *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long long)item->valuedouble ||
        item->valuedouble < (double)minimum || item->valuedouble > 9e15) {
        return false;
    }
    *out = (long long)item->valuedouble;
    return true;
}

static bool fraction_of(const cJSON *item, Rational *out) {
    long long n, d;
    return common_json_rational_read(item, COMMON_RATIONAL_OBJECT, &n, &d) && n >= 0 && n <= 9000000000000000LL &&
           d <= 9000000000000000LL && rational_make(n, d, out) == RATIONAL_OK;
}

static bool place_of(const cJSON *item, int *measure, double *beat) {
    long long m;
    const cJSON *b = member(item, "beat");
    if (!whole_number(member(item, "measure"), 0, &m) || m > 1000000 || !cJSON_IsNumber(b) || b->valuedouble < 0) {
        return false;
    }
    *measure = (int)m;
    *beat = b->valuedouble;
    return true;
}

static bool read_notes(const cJSON *array, CorpusPassage *passage, const char *base, char *path, size_t path_size, char *message,
                       size_t message_size) {
    int count = cJSON_GetArraySize(array);
    if (count == 0) {
        return true;
    }
    passage->notes = calloc((size_t)count, sizeof(*passage->notes));
    if (!passage->notes) {
        fail_at(path, path_size, message, message_size, base, "out of memory");
        return false;
    }
    for (int i = 0; i < count; i++) {
        const cJSON *note = cJSON_GetArrayItem(array, i);
        long long pitch;
        char where[160];
        snprintf(where, sizeof where, "%s/%d", base, i);
        if (!cJSON_IsObject(note) || !whole_number(member(note, "midi"), 0, &pitch) || pitch > 127 ||
            !fraction_of(member(note, "onset"), &passage->notes[i].onset) ||
            !fraction_of(member(note, "duration"), &passage->notes[i].duration) || passage->notes[i].duration.num == 0) {
            fail_at(path, path_size, message, message_size, where,
                    "a note needs midi (0 to 127), onset and duration (fractions {\"num\", \"den\"} of whole notes)");
            return false;
        }
        passage->notes[i].pitch = (int)pitch;
        passage->note_count++;
    }
    return true;
}

static bool read_chords(const cJSON *array, CorpusPassage *passage, const char *base, char *path, size_t path_size, char *message,
                        size_t message_size) {
    int count = cJSON_GetArraySize(array);
    if (count == 0) {
        return true;
    }
    passage->chords = calloc((size_t)count, sizeof(*passage->chords));
    if (!passage->chords) {
        fail_at(path, path_size, message, message_size, base, "out of memory");
        return false;
    }
    for (int i = 0; i < count; i++) {
        const cJSON *chord = cJSON_GetArrayItem(array, i);
        const cJSON *numeral = member(chord, "numeral");
        const cJSON *key = member(chord, "key");
        char where[160];
        snprintf(where, sizeof where, "%s/%d", base, i);
        CorpusPassageChord *made = &passage->chords[i];
        if (!cJSON_IsObject(chord) || !cJSON_IsString(numeral) || strlen(numeral->valuestring) >= CORPUS_SEED_NUMERAL_MAX ||
            (key && (!cJSON_IsString(key) || strlen(key->valuestring) >= CORPUS_PASSAGE_KEY_MAX)) ||
            !place_of(chord, &made->measure, &made->beat)) {
            fail_at(path, path_size, message, message_size, where,
                    "a chord needs numeral (text), measure, beat and optionally key (a key label)");
            return false;
        }
        snprintf(made->numeral, sizeof made->numeral, "%s", numeral->valuestring);
        snprintf(made->key_label, sizeof made->key_label, "%s", key ? key->valuestring : "");
        passage->chord_count++;
    }
    return true;
}

bool corpus_passages_from_json(const cJSON *array, CorpusPassages *out, char *path, size_t path_size, char *message,
                               size_t message_size) {
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsArray(array)) {
        fail_at(path, path_size, message, message_size, "/corpus_passages", "corpus_passages must be an array of passages");
        return false;
    }
    int count = cJSON_GetArraySize(array);
    if (count > 0) {
        out->items = calloc((size_t)count, sizeof(*out->items));
        if (!out->items) {
            fail_at(path, path_size, message, message_size, "/corpus_passages", "out of memory");
            return false;
        }
    }
    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        CorpusPassage *passage = &out->items[i];
        char where[96];
        snprintf(where, sizeof where, "/corpus_passages/%d", i);
        const cJSON *piece = member(item, "piece");
        const cJSON *stale = member(item, "stale");
        out->count = (size_t)i + 1;
        if (!cJSON_IsObject(item) || !cJSON_IsString(piece) || strlen(piece->valuestring) >= CORPUS_SEED_PIECE_MAX ||
            !place_of(member(item, "start"), &passage->start_measure, &passage->start_beat) ||
            !place_of(member(item, "end"), &passage->end_measure, &passage->end_beat) ||
            !fraction_of(member(item, "similarity"), &passage->similarity) || rational_cmp(passage->similarity, rational_ratio(1, 1)) > 0 ||
            (stale && !cJSON_IsBool(stale)) || !cJSON_IsArray(member(item, "notes")) || !cJSON_IsArray(member(item, "chords"))) {
            fail_at(path, path_size, message, message_size, where,
                    "a passage needs piece (text), start and end ({\"measure\", \"beat\"}), similarity (a fraction from 0 to 1), "
                    "notes and chords (arrays), and optionally stale (true or false)");
            corpus_passages_destroy(out);
            return false;
        }
        snprintf(passage->piece, sizeof passage->piece, "%s", piece->valuestring);
        passage->stale = cJSON_IsTrue(stale);
        char base[128];
        snprintf(base, sizeof base, "%s/notes", where);
        bool ok = read_notes(member(item, "notes"), passage, base, path, path_size, message, message_size);
        snprintf(base, sizeof base, "%s/chords", where);
        ok = ok && read_chords(member(item, "chords"), passage, base, path, path_size, message, message_size);
        if (!ok) {
            corpus_passages_destroy(out);
            return false;
        }
    }
    return true;
}

static bool same_place(int measure_a, double beat_a, int measure_b, double beat_b) {
    return measure_a == measure_b && fabs(beat_a - beat_b) < BEAT_EPSILON;
}

/* --- the key the source was heard in ------------------------------------------------ */

/* The key of the last chord that starts at or before the passage, else of the first chord; false when no chord names one. */
static bool source_key(const KnowledgeSnapshot *knowledge, const CorpusPassageChord *chords, size_t count, int measure,
                       double beat, SpelledKey *out) {
    const CorpusPassageChord *found = NULL;
    for (size_t i = 0; i < count; i++) {
        bool before = chords[i].measure < measure || same_place(chords[i].measure, chords[i].beat, measure, beat) ||
                      (chords[i].measure == measure && chords[i].beat < beat);
        if (before || !found) {
            found = &chords[i];
        }
        if (!before) {
            break;
        }
    }
    return found && request_parse_key(found->key_label, knowledge, out);
}

/* --- the motive ------------------------------------------------------------------- */

/*
 * How far to move a melody that starts on pitch so the note keeps its scale degree: the degree of the note in the
 * source key (a note off the scale keeps its distance above the degree below it), placed in the target key, with the
 * move from the pitch within a tritone either way so the melody stays where it was heard.
 */
static int transposition(int pitch, const SpelledKey *source, const SpelledKey *target) {
    int above = util_pitch_class(pitch - spelled_key_tonic_pitch_class(source));
    int degree = 1;
    int beyond = 0;
    standard_tonality_material_degree_at_or_below_semitone(above, source->material, &degree, &beyond);
    int landed = (spelled_key_tonic_pitch_class(target) + util_scale_semitone(target->material, degree) + beyond) % 12;
    return pitch_class_step(pitch, landed, true);
}

/* A plan time fits when its parts do: a numerator to 0x7fffffff and a denominator to 0xffffffff. */
static bool plan_time_fits(Rational time) {
    return time.num >= 0 && time.num <= 0x7fffffffLL && time.den <= 0xffffffffLL;
}

/*
 * One bar of a passage's melody as a motive: the notes that start in the first bar, the last shortened to the barline,
 * moved by shift semitones. false when fewer than two notes are left or one does not fit a plan time.
 */
static bool motive_from_passage(const CorpusPassage *passage, const PlanMeter *meter, const SpelledKey *key, int shift,
                                Motive *out) {
    Rational bar;
    if (plan_meter_bar_length(meter, &bar) != PLAN_OK) {
        return false;
    }
    motive_init(out, *meter);
    motive_set_key(out, key, MOTIVE_SKETCH_KEY_GIVEN);
    for (size_t i = 0; i < passage->note_count && out->note_count < MOTIVE_MAX_NOTES; i++) {
        const CorpusPassageNote *note = &passage->notes[i];
        /* The note starts in the bar when onset < bar. */
        if (rational_cmp(note->onset, bar) >= 0) {
            break;
        }
        Rational end, length = note->duration;
        if (rational_add(note->onset, note->duration, &end) != RATIONAL_OK) {
            return false;
        }
        if (rational_cmp(end, bar) > 0) {
            /* Across the barline: the note lasts until it, bar - onset. */
            if (rational_sub(bar, note->onset, &length) != RATIONAL_OK) {
                return false;
            }
        }
        MotiveSketchNote *placed = &out->notes[out->note_count];
        int pitch = note->pitch + shift;
        if (pitch < 0 || pitch > 127 || !plan_time_fits(note->onset) || !plan_time_fits(length)) {
            return false;
        }
        placed->onset = note->onset;
        placed->duration = length;
        placed->pitch = pitch;
        out->note_count++;
    }
    if (out->note_count < 2) {
        return false;
    }
    const MotiveSketchNote *last = &out->notes[out->note_count - 1];
    return rational_add(last->onset, last->duration, &out->length) == RATIONAL_OK;
}

/* Whether a score is within 1/10 of the best: 10 * score >= 10 * best - 1, in fractions. */
static bool near_best(const Rational *score, const Rational *best) {
    long double s = (long double)score->num / (long double)score->den;
    long double b = (long double)best->num / (long double)best->den;
    return s >= b - 0.1L - 1e-12L;
}

static void fill_match(const CorpusPassage *passage, CorpusSeed *out) {
    snprintf(out->piece, sizeof(out->piece), "%s", passage->piece);
    out->start_measure = passage->start_measure;
    out->start_beat = passage->start_beat;
    out->end_measure = passage->end_measure;
    out->end_beat = passage->end_beat;
    out->score_numerator = (unsigned long)passage->similarity.num;
    out->score_denominator = (unsigned long)passage->similarity.den;
}

CorpusSeedResult corpus_seed_motive(const KnowledgeSnapshot *knowledge, const CorpusPassage *passages, size_t count,
                                    const PlanMeter *meter, const SpelledKey *key, unsigned long seed, CorpusSeed *out) {
    if (!knowledge || (count > 0 && !passages) || !meter || !key || !out || meter->beats_per_bar < 1 || meter->beat_unit < 1) {
        return CORPUS_SEED_ERR_ARGUMENT;
    }
    CommonPracticeMaterials materials;
    if (!common_practice_materials_resolve(knowledge, &materials, NULL, 0)) {
        return CORPUS_SEED_ERR_ARGUMENT;
    }
    if (count == 0) {
        return CORPUS_SEED_ERR_NO_MATCH;
    }

    /* The passages are ranked best first; the ones within 1/10 of the best are the choice. */
    size_t eligible = 0;
    while (eligible < count && near_best(&passages[eligible].similarity, &passages[0].similarity)) {
        eligible++;
    }
    FillRng rng;
    fill_rng_seed(&rng, seed);
    size_t pick = (size_t)fill_rng_below(&rng, eligible);

    CorpusSeedResult status = CORPUS_SEED_ERR_NO_MATCH;
    size_t stale = 0;
    for (size_t step = 0; step < eligible && status != CORPUS_SEED_OK; step++) {
        const CorpusPassage *passage = &passages[(pick + step) % eligible];
        if (passage->note_count == 0) {
            continue;
        }
        int shift = 0;
        SpelledKey from;
        stale += passage->stale ? 1 : 0;
        if (source_key(knowledge, passage->chords, passage->chord_count, passage->start_measure, passage->start_beat, &from)) {
            shift = transposition(passage->notes[0].pitch, &from, key);
        }
        CorpusSeed seeded;
        memset(&seeded, 0, sizeof(seeded));
        if (motive_from_passage(passage, meter, key, shift, &seeded.motive)) {
            fill_match(passage, &seeded);
            seeded.stale_documents = stale;
            *out = seeded;
            status = CORPUS_SEED_OK;
        }
    }
    return status;
}

/* Whether the plan can place the numeral under a key of the mode (given.h reads them with these two). */
static bool numeral_placeable(HarmonyMode mode, const char *numeral) {
    HarmonyChord chord;
    HarmonyAppliedChord applied;
    int inversion;
    return strlen(numeral) < PLAN_ROMAN_NUMERAL_MAX &&
           (harmony_roman_parse(mode, numeral, &chord) || harmony_roman_parse_applied_inverted(mode, numeral, &applied, &inversion));
}

/*
 * The numerals of the passage, read from its chords: the chords from the one that starts where the passage does. false when the
 * passage has no chord there, or a numeral cannot be placed under the mode.
 */
static bool passage_numerals(const CorpusPassage *passage, size_t wanted, HarmonyMode mode, CorpusSeed *seeded) {
    bool ok = false;
    for (size_t first = 0; first < passage->chord_count && !ok; first++) {
        if (!same_place(passage->chords[first].measure, passage->chords[first].beat, passage->start_measure, passage->start_beat)) {
            continue;
        }
        ok = first + wanted <= passage->chord_count;
        for (size_t i = 0; ok && i < wanted; i++) {
            const char *numeral = passage->chords[first + i].numeral;
            ok = strlen(numeral) < CORPUS_SEED_NUMERAL_MAX && numeral_placeable(mode, numeral);
            if (ok) {
                snprintf(seeded->numerals[i], CORPUS_SEED_NUMERAL_MAX, "%s", numeral);
            }
        }
        break;
    }
    return ok;
}

CorpusSeedResult corpus_seed_progression(const KnowledgeSnapshot *knowledge, const CorpusPassage *passages, size_t count,
                                         size_t wanted, HarmonyMode mode, CorpusSeed *out) {
    if (!knowledge || (count > 0 && !passages) || !out || wanted < 1 || wanted > CORPUS_SEED_MAX_NUMERALS) {
        return CORPUS_SEED_ERR_ARGUMENT;
    }
    /* The first passage, best first, whose chords the plan can place in the key's mode. */
    CorpusSeedResult status = CORPUS_SEED_ERR_NO_MATCH;
    size_t stale = 0;
    for (size_t i = 0; i < count && status == CORPUS_SEED_ERR_NO_MATCH; i++) {
        CorpusSeed seeded;
        memset(&seeded, 0, sizeof(seeded));
        stale += passages[i].stale ? 1 : 0;
        if (passage_numerals(&passages[i], wanted, mode, &seeded)) {
            fill_match(&passages[i], &seeded);
            seeded.stale_documents = stale;
            seeded.numeral_count = wanted;
            *out = seeded;
            status = CORPUS_SEED_OK;
        }
    }
    return status;
}
