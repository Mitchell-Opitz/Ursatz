#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K11 pattern: a rhythm cell (content.meter, rhythm, jobs) or an accompaniment (content.grid, steps, by_meter, compound, planing). */

static const char *const METERS[] = {"2/4", "3/4", "4/4", "6/8"};
static const unsigned BAR_UNITS[] = {48, 72, 96, 72}; /* a bar of each meter in 96ths of a whole note */
static const char *const JOBS[] = {"running", "long", "syncopated", "cadence"};
static const char *const GRIDS[] = {"beat", "eighth"};

/* The 96ths of a whole note a duration token (a letter, then up to two dots or a 3, then an optional tie) fills; 0 when it is none. */
static unsigned duration_units(const char *text) {
    static const char LETTERS[] = "whqest";
    static const unsigned UNITS[] = {96, 48, 24, 12, 6, 3};
    const char *letter = text[0] != '\0' ? strchr(LETTERS, text[0]) : NULL;
    if (letter == NULL) {
        return 0;
    }
    unsigned units = UNITS[letter - LETTERS];
    const char *rest = text + 1;
    unsigned result = units;
    if (rest[0] == '3') {
        result = units * 2 / 3;
        rest++;
    } else if (rest[0] == '.' && rest[1] == '.') {
        result = units * 7 / 4;
        rest += 2;
    } else if (rest[0] == '.') {
        result = units * 3 / 2;
        rest++;
    }
    if (rest[0] == '~') {
        rest++;
    }
    return rest[0] == '\0' ? result : 0;
}

/* Why `text` is not one bar of `meter` ("x:e x:q r:h"), or NULL. */
static const char *rhythm_problem(const char *text, size_t meter) {
    char copy[256];
    if (strlen(text) >= sizeof(copy)) {
        return "too long";
    }
    strcpy(copy, text);
    unsigned total = 0;
    size_t tokens = 0;
    for (char *token = strtok(copy, " "); token != NULL; token = strtok(NULL, " ")) {
        unsigned units = (token[0] == 'x' || token[0] == 'r') && token[1] == ':' ? duration_units(token + 2) : 0;
        if (units == 0) {
            return "not a note-list token: x or r, a colon, a duration letter with dots, 3 or ~";
        }
        total += units;
        tokens++;
    }
    if (tokens == 0) {
        return "empty";
    }
    return total == BAR_UNITS[meter] ? NULL : "the durations do not fill one bar of the meter";
}

static const char *read_steps(const cJSON *list, PatternStep *out, size_t *count) {
    int size = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : -1;
    if (size < 1 || size > PATTERN_STEP_MAX) {
        return "not 1 to 16 steps";
    }
    *count = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        PatternStep *step = &out[(*count)++];
        step->member = 0;
        if (cJSON_IsString(item) && strcmp(item->valuestring, "rest") == 0) {
            step->kind = PATTERN_STEP_REST;
        } else if (cJSON_IsString(item) && strcmp(item->valuestring, "chord") == 0) {
            step->kind = PATTERN_STEP_CHORD;
        } else if (cJSON_IsObject(item) && knowledge_member(item, "member") != NULL) {
            const cJSON *member = knowledge_member(item, "member");
            if (!knowledge_in_range(member, 0, 7)) {
                return "a member step is not an integer from 0 to 7";
            }
            step->kind = PATTERN_STEP_MEMBER;
            step->member = member->valueint;
        } else if (cJSON_IsObject(item) && knowledge_in_range(knowledge_member(item, "bass_octave"), 1, 1)) {
            step->kind = PATTERN_STEP_BASS_OCTAVE;
        } else {
            return "a step is not rest, chord, {\"member\": 0 to 7} or {\"bass_octave\": 1}";
        }
    }
    return NULL;
}

static KnowledgeStatus read_rhythm(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    PatternRhythm *rhythm = &slot->pattern.rhythm;
    const char *meter_text = knowledge_string_of(knowledge_member(content, "meter"));
    int meter = knowledge_word_index(meter_text, METERS, 4);
    if (meter < 0) {
        knowledge_fail(message, message_size, path, id, "content.meter", "not 2/4, 3/4, 4/4 or 6/8");
        return KNOWLEDGE_ERR_FIELD;
    }
    const char *text = knowledge_string_of(knowledge_member(content, "rhythm"));
    const char *problem = text == NULL ? "missing" : rhythm_problem(text, (size_t)meter);
    if (problem != NULL) {
        knowledge_fail(message, message_size, path, id, "content.rhythm", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *jobs = knowledge_member(content, "jobs");
    int count = cJSON_IsArray(jobs) ? cJSON_GetArraySize(jobs) : -1;
    if (count < 1 || count > 3) {
        knowledge_fail(message, message_size, path, id, "content.jobs", "not 1 to 3 jobs");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, jobs) {
        int job = knowledge_word_index(knowledge_string_of(item), JOBS, 4);
        if (job < 0 || (rhythm->jobs & (1u << job)) != 0) {
            knowledge_fail(message, message_size, path, id, "content.jobs", "a job is not running, long, syncopated or cadence, or is given twice");
            return KNOWLEDGE_ERR_FIELD;
        }
        rhythm->jobs |= 1u << job;
    }
    rhythm->meter = METERS[meter];
    rhythm->rhythm = knowledge_own(slot, text);
    return rhythm->rhythm == NULL ? KNOWLEDGE_ERR_ALLOC : KNOWLEDGE_OK;
}

static KnowledgeStatus read_accompaniment(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    PatternAccompaniment *accompaniment = &slot->pattern.accompaniment;
    int grid = knowledge_word_index(knowledge_string_of(knowledge_member(content, "grid")), GRIDS, 2);
    if (grid < 0) {
        knowledge_fail(message, message_size, path, id, "content.grid", "not beat or eighth");
        return KNOWLEDGE_ERR_FIELD;
    }
    accompaniment->beat_grid = grid == 0;
    const cJSON *compound = knowledge_member(content, "compound");
    if (compound != NULL && !cJSON_IsBool(compound)) {
        knowledge_fail(message, message_size, path, id, "content.compound", "not a boolean");
        return KNOWLEDGE_ERR_FIELD;
    }
    accompaniment->compound = compound == NULL || cJSON_IsTrue(compound);
    const cJSON *planing = knowledge_member(content, "planing");
    if (planing != NULL && !cJSON_IsBool(planing)) {
        knowledge_fail(message, message_size, path, id, "content.planing", "not a boolean");
        return KNOWLEDGE_ERR_FIELD;
    }
    accompaniment->planing = planing != NULL && cJSON_IsTrue(planing);
    const char *problem = read_steps(knowledge_member(content, "steps"), accompaniment->steps, &accompaniment->step_count);
    if (problem != NULL) {
        knowledge_fail(message, message_size, path, id, "content.steps", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *by_meter = knowledge_member(content, "by_meter");
    if (by_meter == NULL) {
        return KNOWLEDGE_OK;
    }
    if (!cJSON_IsObject(by_meter)) {
        knowledge_fail(message, message_size, path, id, "content.by_meter", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, by_meter) {
        if (knowledge_word_index(item->string, METERS, 4) < 0 || accompaniment->meter_count == PATTERN_METER_MAX) {
            knowledge_fail(message, message_size, path, id, "content.by_meter", "a key is not 2/4, 3/4, 4/4 or 6/8");
            return KNOWLEDGE_ERR_FIELD;
        }
        size_t at = accompaniment->meter_count++;
        strcpy(accompaniment->meters[at].meter, item->string);
        problem = read_steps(item, accompaniment->meters[at].steps, &accompaniment->meters[at].step_count);
        if (problem != NULL) {
            knowledge_fail(message, message_size, path, id, "content.by_meter", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_pattern_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                       size_t message_size) {
    const cJSON *content = knowledge_member(entry, "content");
    slot->pattern.id = slot->entry.id;
    slot->pattern.name = slot->entry.name;
    if (!cJSON_IsObject(content) || subtype == NULL || (strcmp(subtype, "rhythm") != 0 && strcmp(subtype, "accompaniment") != 0)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "not rhythm or accompaniment");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->pattern.is_rhythm = strcmp(subtype, "rhythm") == 0;
    return slot->pattern.is_rhythm ? read_rhythm(slot, content, path, message, message_size)
                                   : read_accompaniment(slot, content, path, message, message_size);
}

const PatternEntry *knowledge_snapshot_pattern(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_pattern ? &slot->pattern : NULL;
}

size_t knowledge_snapshot_pattern_count(const KnowledgeSnapshot *snapshot) {
    size_t count = 0;
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        count += snapshot->slots[i].has_pattern ? 1 : 0;
    }
    return count;
}

const PatternEntry *knowledge_snapshot_pattern_at(const KnowledgeSnapshot *snapshot, size_t index) {
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        if (snapshot->slots[i].has_pattern && index-- == 0) {
            return &snapshot->slots[i].pattern;
        }
    }
    return NULL;
}
