#include "reading/reading.h"

#include <stdlib.h>
#include <string.h>

#include "model/core/content_hash.h"

/* A growing array of one kind of claim. */
typedef struct {
    void *items;
    size_t count;
    size_t capacity;
} ClaimList;

struct Reading {
    ReadingSource source;
    ReadingTarget target;
    ClaimList regions;
    ClaimList chords;
    ClaimList cadences;
    ClaimList groups;
    ClaimList motive_occurrences;
    ClaimList meters;
    ClaimList lines;
    ClaimList voice_faults;
    /* The ids of every claim, in a hash set, so that a repeat is found at once. */
    char (*ids)[REALIZATION_ID_MAX];
    size_t id_count;
    size_t id_capacity;
    size_t *slots; /* an index into ids plus one, 0 for an empty slot; id_capacity entries (a power of two) */
    const AnalysisChain *detail;
};

static bool key_valid(const char *text, bool allow_empty) {
    size_t length = text ? strlen(text) : 0;
    if (length == 0) {
        return allow_empty && text != NULL;
    }
    if (length != 16) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

RealizationResult reading_create(const ReadingSource *source, const ReadingTarget *target, Reading **out) {
    if (!source || !target || !out) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(target->work) || !realization_id_valid(target->movement)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if ((int)source->kind < READING_SOURCE_ANALYZER || source->kind > READING_SOURCE_PERSON || strnlen(source->name, sizeof(source->name)) >= sizeof(source->name) ||
        !key_valid(source->producer_key, true) || !key_valid(source->knowledge_hash, true) || !key_valid(target->digest, true)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    Reading *reading = calloc(1, sizeof(Reading));
    if (!reading) {
        return REALIZATION_ERR_ALLOC;
    }
    reading->source = *source;
    reading->target = *target;
    *out = reading;
    return REALIZATION_OK;
}

static void list_free(ClaimList *list, size_t size) {
    for (size_t i = 0; i < list->count; i++) {
        ReadingClaim *claim = (ReadingClaim *)((char *)list->items + i * size);
        free(claim->evidence.ids);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

void reading_destroy(Reading *reading) {
    if (!reading) {
        return;
    }
    list_free(&reading->regions, sizeof(ReadingRegion));
    list_free(&reading->chords, sizeof(ReadingChord));
    list_free(&reading->cadences, sizeof(ReadingCadence));
    list_free(&reading->groups, sizeof(ReadingGroup));
    list_free(&reading->motive_occurrences, sizeof(ReadingMotiveOccurrence));
    list_free(&reading->meters, sizeof(ReadingMeter));
    list_free(&reading->lines, sizeof(ReadingLine));
    list_free(&reading->voice_faults, sizeof(ReadingVoiceFault));
    free(reading->ids);
    free(reading->slots);
    free(reading);
}

static size_t id_hash(const char *id) {
    return (size_t)content_hash_fnv1a64(CONTENT_HASH_FNV1A64_OFFSET, id, strlen(id));
}

/* Grows the set when it is half full. */
static bool ids_grow(Reading *reading) {
    if (reading->id_count * 2 < reading->id_capacity) {
        return true;
    }
    size_t capacity = reading->id_capacity ? reading->id_capacity * 2 : 64;
    char(*ids)[REALIZATION_ID_MAX] = realloc(reading->ids, capacity * REALIZATION_ID_MAX);
    size_t *slots = calloc(capacity, sizeof(size_t));
    if (!ids || !slots) {
        free(slots);
        if (ids) {
            reading->ids = ids; /* realloc succeeded: keep the larger block, the old capacity is still valid */
        }
        return false;
    }
    reading->ids = ids;
    reading->id_capacity = capacity;
    free(reading->slots);
    reading->slots = slots;
    for (size_t i = 0; i < reading->id_count; i++) {
        size_t at = id_hash(reading->ids[i]) & (capacity - 1);
        while (reading->slots[at]) {
            at = (at + 1) & (capacity - 1);
        }
        reading->slots[at] = i + 1;
    }
    return true;
}

static bool ids_has(const Reading *reading, const char *id) {
    if (reading->id_capacity == 0) {
        return false;
    }
    for (size_t at = id_hash(id) & (reading->id_capacity - 1); reading->slots[at] != 0; at = (at + 1) & (reading->id_capacity - 1)) {
        if (strcmp(reading->ids[reading->slots[at] - 1], id) == 0) {
            return true;
        }
    }
    return false;
}

/* The checks every claim passes before it is kept. */
static RealizationResult check_claim(const Reading *reading, const ReadingClaim *claim) {
    if (!realization_id_valid(claim->id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    for (size_t i = 0; i < claim->evidence.count; i++) {
        if (!claim->evidence.ids || !realization_id_valid(claim->evidence.ids[i])) {
            return REALIZATION_ERR_BAD_ID;
        }
    }
    if (ids_has(reading, claim->id)) {
        return REALIZATION_ERR_DUPLICATE_ID;
    }
    if (claim->start.den <= 0 || claim->end.den <= 0 || rational_cmp(claim->start, claim->end) > 0) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (claim->confidence.present) {
        Rational zero = {0, 1};
        Rational one = {1, 1};
        if (claim->confidence.value.den <= 0 || rational_cmp(claim->confidence.value, zero) < 0 || rational_cmp(claim->confidence.value, one) > 0) {
            return REALIZATION_ERR_BAD_VALUE;
        }
    }
    return REALIZATION_OK;
}

/* Copies the claim into the list, with its own copy of the evidence, and records its id. */
static RealizationResult add_claim(Reading *reading, ClaimList *list, size_t size, const void *claim) {
    const ReadingClaim *head = claim;
    RealizationResult checked = check_claim(reading, head);
    if (checked != REALIZATION_OK) {
        return checked;
    }
    if (!ids_grow(reading)) {
        return REALIZATION_ERR_ALLOC;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        void *items = realloc(list->items, capacity * size);
        if (!items) {
            return REALIZATION_ERR_ALLOC;
        }
        list->items = items;
        list->capacity = capacity;
    }
    char(*evidence)[REALIZATION_ID_MAX] = NULL;
    if (head->evidence.count > 0) {
        evidence = malloc(head->evidence.count * REALIZATION_ID_MAX);
        if (!evidence) {
            return REALIZATION_ERR_ALLOC;
        }
        memcpy(evidence, head->evidence.ids, head->evidence.count * REALIZATION_ID_MAX);
    }
    ReadingClaim *stored = (ReadingClaim *)((char *)list->items + list->count * size);
    memcpy(stored, claim, size);
    stored->evidence.ids = evidence;
    list->count++;
    memcpy(reading->ids[reading->id_count], head->id, REALIZATION_ID_MAX);
    reading->id_count++;
    size_t at = id_hash(head->id) & (reading->id_capacity - 1);
    while (reading->slots[at]) {
        at = (at + 1) & (reading->id_capacity - 1);
    }
    reading->slots[at] = reading->id_count;
    return REALIZATION_OK;
}

RealizationResult reading_add_region(Reading *reading, const ReadingRegion *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    if (claim->center < 0 || claim->center > 11 || !claim->material || (int)claim->strength < REGION_STRENGTH_ESTABLISHED || claim->strength > REGION_STRENGTH_ABSENT) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    return add_claim(reading, &reading->regions, sizeof(ReadingRegion), claim);
}

RealizationResult reading_add_chord(Reading *reading, const ReadingChord *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    if (claim->has_reading && (claim->reading.root_pitch_class < 0 || claim->reading.root_pitch_class > 11 || claim->reading.bass_pitch_class < -1 ||
                               claim->reading.bass_pitch_class > 11 || (int)claim->reading.role < CHORD_ROLE_FUNCTIONAL || claim->reading.role > CHORD_ROLE_EMBELLISHING)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    return add_claim(reading, &reading->chords, sizeof(ReadingChord), claim);
}

RealizationResult reading_add_cadence(Reading *reading, const ReadingCadence *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    if (!cadence_type_valid(claim->type) || claim->alternative_count > 4 || claim->arrival.den <= 0) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    for (size_t i = 0; i < claim->alternative_count; i++) {
        if (!cadence_type_valid(claim->alternatives[i])) {
            return REALIZATION_ERR_BAD_VALUE;
        }
    }
    return add_claim(reading, &reading->cadences, sizeof(ReadingCadence), claim);
}

RealizationResult reading_add_group(Reading *reading, const ReadingGroup *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    if ((int)claim->kind < READING_GROUP_SECTION || claim->kind > READING_GROUP_IDEA || claim->part_cadence_count > 2 ||
        (claim->parent != READING_NO_PARENT && claim->parent >= reading->groups.count)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    for (size_t i = 0; i < claim->part_cadence_count; i++) {
        if (!cadence_type_valid(claim->part_cadences[i])) {
            return REALIZATION_ERR_BAD_VALUE;
        }
    }
    return add_claim(reading, &reading->groups, sizeof(ReadingGroup), claim);
}

RealizationResult reading_add_motive_occurrence(Reading *reading, const ReadingMotiveOccurrence *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    return add_claim(reading, &reading->motive_occurrences, sizeof(ReadingMotiveOccurrence), claim);
}

RealizationResult reading_add_meter(Reading *reading, const ReadingMeter *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    return add_claim(reading, &reading->meters, sizeof(ReadingMeter), claim);
}

RealizationResult reading_add_line(Reading *reading, const ReadingLine *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    if (!line_role_name(claim->role)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    return add_claim(reading, &reading->lines, sizeof(ReadingLine), claim);
}

RealizationResult reading_add_voice_fault(Reading *reading, const ReadingVoiceFault *claim) {
    if (!reading || !claim) {
        return REALIZATION_ERR_NULL;
    }
    return add_claim(reading, &reading->voice_faults, sizeof(ReadingVoiceFault), claim);
}

RealizationResult reading_set_target_digest(Reading *reading, const char *digest) {
    if (!reading || !digest) {
        return REALIZATION_ERR_NULL;
    }
    if (!key_valid(digest, false)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    memcpy(reading->target.digest, digest, 17);
    return REALIZATION_OK;
}

const ReadingSource *reading_source(const Reading *reading) {
    return reading ? &reading->source : NULL;
}

const ReadingTarget *reading_target(const Reading *reading) {
    return reading ? &reading->target : NULL;
}

#define ACCESSORS(kind, type, list)                                                    \
    size_t reading_##kind##_count(const Reading *reading) {                            \
        return reading ? reading->list.count : 0;                                      \
    }                                                                                  \
    const type *reading_##kind##_at(const Reading *reading, size_t index) {            \
        return reading && index < reading->list.count ? &((const type *)reading->list.items)[index] : NULL; \
    }

ACCESSORS(region, ReadingRegion, regions)
ACCESSORS(chord, ReadingChord, chords)
ACCESSORS(cadence, ReadingCadence, cadences)
ACCESSORS(group, ReadingGroup, groups)
ACCESSORS(motive_occurrence, ReadingMotiveOccurrence, motive_occurrences)
ACCESSORS(meter, ReadingMeter, meters)
ACCESSORS(line, ReadingLine, lines)
ACCESSORS(voice_fault, ReadingVoiceFault, voice_faults)

RealizationResult reading_set_detail(Reading *reading, const AnalysisChain *chain) {
    if (!reading) {
        return REALIZATION_ERR_NULL;
    }
    reading->detail = chain;
    return REALIZATION_OK;
}

const AnalysisChain *reading_detail(const Reading *reading) {
    return reading ? reading->detail : NULL;
}
