#include "theory/tonality/region_relation.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/brightness.h"

#define SEMITONES_PER_OCTAVE 12

static const char *const CENTER_NAMES[SEMITONES_PER_OCTAVE] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static int reduced(int semitones) {
    int value = semitones % SEMITONES_PER_OCTAVE;
    return value < 0 ? value + SEMITONES_PER_OCTAVE : value;
}

/* A refusal: "relation '<name>' from <anchor>: <reason>". */
static RegionRelationStatus refuse(char *message, size_t size, const Relation *relation, const Region *anchor, const char *format,
                                   ...) {
    if (message != NULL && size > 0) {
        char reason[192];
        va_list args;
        va_start(args, format);
        vsnprintf(reason, sizeof(reason), format, args);
        va_end(args);
        const PitchMaterial *material = anchor->material;
        const char *word = material != NULL && material->key_word != NULL ? material->key_word : "?";
        snprintf(message, size, "relation '%s' from %s %s: %s", relation->name != NULL ? relation->name : relation->id,
                 anchor->center >= 0 && anchor->center < SEMITONES_PER_OCTAVE ? CENTER_NAMES[anchor->center] : "-", word, reason);
    }
    return REGION_RELATION_REFUSED;
}

static bool same_material(const PitchMaterial *a, const PitchMaterial *b) {
    return a == b || (a != NULL && b != NULL && a->id != NULL && b->id != NULL && strcmp(a->id, b->id) == 0);
}

static void give(Region *out, int center, const PitchMaterial *material) {
    *out = region_tonal(center, material);
}

/* The material for the key words "major" and "minor" (natural minor), NULL when a word names none or several. */
static const PitchMaterial *major_of(const KnowledgeSnapshot *snapshot) {
    return knowledge_snapshot_material_named(snapshot, "major");
}

static const PitchMaterial *minor_of(const KnowledgeSnapshot *snapshot) {
    return knowledge_snapshot_material_named(snapshot, "minor");
}

static bool is_minor_form(const PitchMaterial *material, const PitchMaterial *minor) {
    return same_material(material, minor) || (material->family != NULL && strcmp(material->family, "minor") == 0);
}

static RegionRelationStatus resolve_tonal(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor,
                                          Region *out, char *message, size_t size) {
    const PitchMaterial *anchor_material = anchor->material;
    int center = anchor->center;
    int degree_index = 0;
    if (relation->tonal.by_degree) {
        int step = pitch_material_step(anchor_material, relation->tonal.degree);
        if (step < 0) {
            return refuse(message, size, relation, anchor, "the material has no degree %d", relation->tonal.degree);
        }
        degree_index = relation->tonal.degree - 1;
        center += step;
    } else {
        center += relation->tonal.root_semitones;
    }
    center = reduced(center);

    const PitchMaterial *major = NULL;
    const PitchMaterial *minor = NULL;
    switch (relation->tonal.material) {
    case RELATION_MATERIAL_SAME:
        give(out, center, anchor_material);
        return REGION_RELATION_OK;
    case RELATION_MATERIAL_ENTRY:
        if (relation->tonal.material_entry == NULL) {
            return refuse(message, size, relation, anchor, "the relation names no material");
        }
        give(out, center, relation->tonal.material_entry);
        return REGION_RELATION_OK;
    case RELATION_MATERIAL_PARALLEL:
        major = major_of(snapshot);
        minor = minor_of(snapshot);
        if (major != NULL && minor != NULL && same_material(anchor_material, major)) {
            give(out, center, minor);
            return REGION_RELATION_OK;
        }
        if (major != NULL && minor != NULL && is_minor_form(anchor_material, minor)) {
            give(out, center, major);
            return REGION_RELATION_OK;
        }
        return refuse(message, size, relation, anchor, "only major and the minor forms have a parallel key");
    case RELATION_MATERIAL_DIATONIC: {
        major = major_of(snapshot);
        minor = minor_of(snapshot);
        size_t count = anchor_material->step_count;
        if (!relation->tonal.by_degree || major == NULL || minor == NULL || count == 0) {
            return refuse(message, size, relation, anchor, "a diatonic key needs a degree, and one major and one minor material");
        }
        int root = anchor_material->steps[degree_index];
        int third = reduced(anchor_material->steps[(degree_index + 2) % count] - root);
        int fifth = reduced(anchor_material->steps[(degree_index + 4) % count] - root);
        if (third == 4 && fifth == 7) {
            give(out, center, major);
            return REGION_RELATION_OK;
        }
        if (third == 3 && fifth == 7) {
            give(out, center, minor);
            return REGION_RELATION_OK;
        }
        return refuse(message, size, relation, anchor, "the triad on degree %d is not major or minor, so it gives no key",
                      relation->tonal.degree);
    }
    }
    return refuse(message, size, relation, anchor, "unknown material rule");
}

static RegionRelationStatus resolve_modal(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor,
                                          Region *out, char *message, size_t size) {
    char why[160] = "";
    const PitchMaterial *shifted = brightness_shift(snapshot, anchor->material, relation->modal.brightness_steps, why, sizeof(why));
    if (shifted == NULL) {
        return refuse(message, size, relation, anchor, "%s", why);
    }
    int center = relation->modal.same_center ? anchor->center : anchor->center + relation->modal.center;
    give(out, center, shifted);
    return REGION_RELATION_OK;
}

static RegionRelationStatus resolve_neo_riemannian(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor,
                                                   Region *out, char *message, size_t size) {
    const PitchMaterial *major = major_of(snapshot);
    const PitchMaterial *minor = minor_of(snapshot);
    if (major == NULL || minor == NULL) {
        return refuse(message, size, relation, anchor, "the snapshot has no single major and minor material");
    }
    int center = anchor->center;
    const PitchMaterial *kind = anchor->material;
    for (size_t i = 0; i < relation->neo_riemannian.move_count; i++) {
        char move = relation->neo_riemannian.moves[i];
        if (move != 'P' && move != 'L' && move != 'R') {
            return refuse(message, size, relation, anchor, "'%c' is not a neo-Riemannian move", move);
        }
        if (same_material(kind, major)) {
            center += move == 'L' ? 4 : move == 'R' ? -3 : 0;
            kind = minor;
        } else if (same_material(kind, minor)) {
            center += move == 'L' ? -4 : move == 'R' ? 3 : 0;
            kind = major;
        } else {
            return refuse(message, size, relation, anchor, "neo-Riemannian moves apply to major and natural minor triads only");
        }
        center = reduced(center);
    }
    give(out, center, kind);
    return REGION_RELATION_OK;
}

static RegionRelationStatus resolve_set(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor, Region *out,
                                        char *message, size_t size) {
    if (!relation->set.inversion) {
        give(out, anchor->center + relation->set.n, anchor->material);
        return REGION_RELATION_OK;
    }
    unsigned mask = pitch_material_mask(anchor->material, anchor->center);
    unsigned inverted = 0;
    for (int pitch = 0; pitch < SEMITONES_PER_OCTAVE; pitch++) {
        if ((mask >> pitch) & 1u) {
            inverted |= 1u << reduced(relation->set.n - pitch);
        }
    }
    int center = reduced(relation->set.n - anchor->center);
    for (size_t i = 0; i < knowledge_snapshot_material_count(snapshot); i++) {
        const PitchMaterial *candidate = knowledge_snapshot_material_at(snapshot, i);
        if (candidate != NULL && pitch_material_mask(candidate, center) == inverted) {
            give(out, center, candidate);
            return REGION_RELATION_OK;
        }
    }
    return refuse(message, size, relation, anchor, "the inverted collection is no material of the snapshot");
}

RegionRelationStatus region_resolve(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor, Region *out,
                                    char *message, size_t size) {
    if (snapshot == NULL || relation == NULL || anchor == NULL || out == NULL) {
        if (message != NULL && size > 0) {
            snprintf(message, size, "region relation: a NULL snapshot, relation, anchor or result");
        }
        return REGION_RELATION_ERR_ARGUMENT;
    }
    if (anchor->kind != REGION_TONAL_MODAL || anchor->material == NULL || anchor->center < 0 ||
        anchor->center >= SEMITONES_PER_OCTAVE) {
        if (message != NULL && size > 0) {
            snprintf(message, size, "relation '%s': the anchor is not a tonal or modal region with a center and a material",
                     relation->name != NULL ? relation->name : relation->id);
        }
        return REGION_RELATION_ERR_ARGUMENT;
    }
    Region resolved;
    RegionRelationStatus status;
    switch (relation->family) {
    case RELATION_FAMILY_TONAL:
        status = resolve_tonal(snapshot, relation, anchor, &resolved, message, size);
        break;
    case RELATION_FAMILY_MODAL:
        status = resolve_modal(snapshot, relation, anchor, &resolved, message, size);
        break;
    case RELATION_FAMILY_NEO_RIEMANNIAN:
        status = resolve_neo_riemannian(snapshot, relation, anchor, &resolved, message, size);
        break;
    case RELATION_FAMILY_SET:
        status = resolve_set(snapshot, relation, anchor, &resolved, message, size);
        break;
    default:
        return refuse(message, size, relation, anchor, "a serial relation needs serial material, which does not exist yet");
    }
    if (status == REGION_RELATION_OK) {
        *out = resolved;
    }
    return status;
}

RegionRelationStatus region_resolve_named(const KnowledgeSnapshot *snapshot, const char *word, const Region *anchor, Region *out,
                                          char *message, size_t size) {
    if (snapshot == NULL || word == NULL) {
        if (message != NULL && size > 0) {
            snprintf(message, size, "region relation: a NULL snapshot or word");
        }
        return REGION_RELATION_ERR_ARGUMENT;
    }
    const Relation *relation = knowledge_snapshot_relation_named(snapshot, word);
    if (relation == NULL) {
        if (message != NULL && size > 0) {
            snprintf(message, size, "region relation: no single relation is named '%s'", word);
        }
        return REGION_RELATION_ERR_UNKNOWN_RELATION;
    }
    return region_resolve(snapshot, relation, anchor, out, message, size);
}
