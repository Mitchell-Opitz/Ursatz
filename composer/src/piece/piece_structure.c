#include <ursatz_composer/piece_structure.h>

#include <string.h>

#include "grow/grow_internal.h"

static const char *const PART_NAMES[PIECE_PART_COUNT] = {"intro", "body", "turnaround", "ending"};
static const char *const FILL_NAMES[] = {"empty", "motif", "theme", "progression"};

/* The bars of the ending section piece_structure_default_ending adds. */
#define DEFAULT_ENDING_BARS 2

const char *piece_part_name(PiecePart part) {
    return part >= 0 && part < PIECE_PART_COUNT ? PART_NAMES[part] : "unknown";
}

bool piece_part_from_name(const char *name, PiecePart *out) {
    for (int part = 0; name && part < PIECE_PART_COUNT; part++) {
        if (strcmp(name, PART_NAMES[part]) == 0) {
            if (out) {
                *out = (PiecePart)part;
            }
            return true;
        }
    }
    return false;
}

const char *piece_fill_name(PieceFillKind fill) {
    return fill >= PIECE_FILL_EMPTY && fill <= PIECE_FILL_PROGRESSION ? FILL_NAMES[fill] : "unknown";
}

bool piece_fill_from_name(const char *name, PieceFillKind *out) {
    for (int fill = PIECE_FILL_EMPTY; name && fill <= PIECE_FILL_PROGRESSION; fill++) {
        if (strcmp(name, FILL_NAMES[fill]) == 0) {
            if (out) {
                *out = (PieceFillKind)fill;
            }
            return true;
        }
    }
    return false;
}

/* Whether the pickup is a multiple of 1/32 of a whole note, not negative and shorter than a bar. */
static bool pickup_valid(PlanTime pickup, PlanTime bar) {
    if (pickup.den == 0 || pickup.num < 0 || plan_time_compare(pickup, bar) >= 0) {
        return false;
    }
    return (pickup.num * (long)PIECE_PICKUP_GRID) % (long)pickup.den == 0;
}

PieceResult piece_structure_check(const PieceStructure *structure) {
    if (!structure || (structure->section_count > 0 && !structure->sections)) {
        return PIECE_ERR_NULL;
    }
    size_t cells;
    if (!grow_cells_per_bar(&structure->meter, &cells) || structure->key.tonic_pitch_class < 0 ||
        structure->key.tonic_pitch_class > 11 ||
        (structure->key.mode != PLAN_MODE_MAJOR && structure->key.mode != PLAN_MODE_MINOR) ||
        structure->meter.beats_per_bar <= 0 || structure->meter.beat_unit <= 0 ||
        structure->section_count > PIECE_STRUCTURE_MAX_SECTIONS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanTime bar = grow_bar_length(&structure->meter);
    size_t beats = 0;
    bool body = false;
    int last_part = PIECE_PART_INTRO;
    for (size_t i = 0; i < structure->section_count; i++) {
        const PieceSection *section = &structure->sections[i];
        if (section->id[0] == '\0' || strnlen(section->id, PIECE_SECTION_ID_MAX) == PIECE_SECTION_ID_MAX ||
            section->part < PIECE_PART_INTRO || section->part > PIECE_PART_ENDING || (int)section->part < last_part ||
            section->beats < 1 || !pickup_valid(section->pickup, bar) ||
            section->fill < PIECE_FILL_EMPTY || section->fill > PIECE_FILL_PROGRESSION) {
            return PIECE_ERR_BAD_REQUEST;
        }
        for (size_t other = 0; other < i; other++) {
            if (strcmp(structure->sections[other].id, section->id) == 0) {
                return PIECE_ERR_BAD_REQUEST;
            }
        }
        last_part = (int)section->part;
        body = body || section->part == PIECE_PART_BODY;
        beats += section->beats;
    }
    size_t per_bar = (size_t)structure->meter.beats_per_bar;
    if (!body || (beats + per_bar - 1) / per_bar > PIECE_STRUCTURE_MAX_BARS) {
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

void piece_structure_default_cadences(PieceStructure *structure) {
    if (!structure || !structure->sections) {
        return;
    }
    size_t count = structure->section_count;
    /* The last section of each part that leads on: the intro into the body, the loop back to its start. */
    size_t last_intro = count;
    size_t last_loop = count;
    size_t last_ending = count;
    for (size_t i = 0; i < count; i++) {
        PiecePart part = structure->sections[i].part;
        if (part == PIECE_PART_INTRO) {
            last_intro = i;
        } else if (part == PIECE_PART_BODY || part == PIECE_PART_TURNAROUND) {
            last_loop = i;
        } else {
            last_ending = i;
        }
    }
    for (size_t i = 0; i < count; i++) {
        PieceSection *section = &structure->sections[i];
        if (section->has_cadence) {
            continue;
        }
        section->has_cadence = true;
        if (i == last_ending) {
            section->cadence = PLAN_CADENCE_PAC;
        } else if (i == last_loop || i == last_intro) {
            section->cadence = PLAN_CADENCE_HC;
        } else {
            section->cadence = PLAN_CADENCE_IAC;
        }
    }
}

bool piece_structure_default_ending(PieceStructure *structure, size_t capacity) {
    if (!structure || !structure->sections) {
        return false;
    }
    for (size_t i = 0; i < structure->section_count; i++) {
        if (structure->sections[i].part == PIECE_PART_ENDING) {
            return true;
        }
    }
    if (structure->section_count >= capacity || structure->meter.beats_per_bar <= 0) {
        return false;
    }
    PieceSection *section = &structure->sections[structure->section_count++];
    memset(section, 0, sizeof(*section));
    strcpy(section->id, "end");
    strcpy(section->letter, "E");
    section->part = PIECE_PART_ENDING;
    section->beats = DEFAULT_ENDING_BARS * (size_t)structure->meter.beats_per_bar;
    section->pickup = (PlanTime){0, 1};
    section->has_cadence = true;
    section->cadence = PLAN_CADENCE_PAC;
    section->fill = PIECE_FILL_EMPTY;
    return true;
}
