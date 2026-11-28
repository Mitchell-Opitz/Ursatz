#include <stdio.h>
#include <string.h>

#include <model/pitch/pitch_arithmetic.h>
#include <theory/tonality/modal_palette.h>
#include <ursatz_composer/harmony_chords.h>

#include "api_internal.h"
#include "piece/request_chords.h"
#include "structure/request_parse.h"
#include "util/util.h"

/*
 * `palette` as a composer API entry (R-765, composer/docs/harmony-lab.md): the chords of a key's material and of a second material on the same
 * tonic, which ones both hold and which carry a tone the other lacks, each with the chord text the Composer reads.
 */

static const char *const QUALITY_NAMES[] = {"major", "minor", "diminished", "augmented"};
static const char *const QUALITY_SUFFIX[] = {"", "m", "dim", "aug"};

/* The chord's name spelled from the key's tonic letter: "Cm", "Eb", "Adim". */
static void chord_name(const SpelledKey *key, const PaletteChord *chord, char *out, size_t size) {
    int letter = ((int)key->tonic_letter + chord->degree - 1) % 7;
    int natural = pitch_class_of_letter((PitchLetter)letter, 0);
    int tonic = spelled_key_tonic_pitch_class(key);
    int accidental = ((tonic + chord->root_semitones - natural + 18) % 12) - 6;
    char mark[3] = "";
    if (accidental < 0) {
        snprintf(mark, sizeof mark, "%s", accidental == -1 ? "b" : "bb");
    } else if (accidental > 0) {
        snprintf(mark, sizeof mark, "%s", accidental == 1 ? "#" : "##");
    }
    snprintf(out, size, "%c%s%s", "ABCDEFG"[letter], mark, QUALITY_SUFFIX[chord->quality]);
}

/* The degree of the material whose step is the chord's root. */
static int material_degree(const PitchMaterial *material, int root) {
    for (int degree = 1; degree <= 7; degree++) {
        if (pitch_material_step(material, degree) == root) {
            return degree;
        }
    }
    return 0;
}

static cJSON *material_json(const ComposerChords *chords, const SpelledKey *key, const PitchMaterial *material, const PitchMaterial *mode,
                            const PaletteChord *list, size_t count) {
    cJSON *entry = cJSON_CreateObject();
    cJSON *array = entry != NULL ? cJSON_AddArrayToObject(entry, "chords") : NULL;
    if (array == NULL || cJSON_AddStringToObject(entry, "material", material->id) == NULL ||
        cJSON_AddStringToObject(entry, "word", material->key_word) == NULL) {
        cJSON_Delete(entry);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        const PaletteChord *chord = &list[i];
        char name[16];
        char text[PLAN_ROMAN_NUMERAL_MAX] = "";
        chord_name(key, chord, name, sizeof name);
        bool written = composer_chord_text_of(chords, key, chord->degree, chord->alteration, chord->quality, mode, text);
        cJSON *item = cJSON_CreateObject();
        if (item == NULL || cJSON_AddNumberToObject(item, "degree", material_degree(material, chord->root_semitones)) == NULL ||
            cJSON_AddStringToObject(item, "numeral", chord->numeral) == NULL || cJSON_AddStringToObject(item, "name", name) == NULL ||
            cJSON_AddStringToObject(item, "quality", QUALITY_NAMES[chord->quality]) == NULL ||
            (written && cJSON_AddStringToObject(item, "text", text) == NULL) || cJSON_AddBoolToObject(item, "shared", chord->shared) == NULL ||
            cJSON_AddBoolToObject(item, "color", chord->color) == NULL || !cJSON_AddItemToArray(array, item)) {
            cJSON_Delete(item);
            cJSON_Delete(entry);
            return NULL;
        }
    }
    return entry;
}

UrsatzStatus composer_api_palette(const ComposerContext *context, const char *request_json, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    UrsatzStatus status = api_require_knowledge(context, &materials, &out->error);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *request = NULL;
    status = api_parse_request(request_json, &request, &out->error);
    if (status != URSATZ_OK) {
        return status;
    }
    const char *key_text = NULL;
    const char *with_text = NULL;
    bool found;
    SpelledKey key;
    const PitchMaterial *with = NULL;
    for (const cJSON *member = request->child; member != NULL; member = member->next) {
        if (strcmp(member->string, "key") != 0 && strcmp(member->string, "with") != 0 && strcmp(member->string, "api_version") != 0) {
            status = api_fail(&out->error, URSATZ_ERR_REQUEST, "", "palette takes key and with; '%s' is not one of them", member->string);
        }
    }
    if (status == URSATZ_OK && (!api_member_string(request, "key", &key_text, &found, &out->error) || !found)) {
        status = out->error.message[0] != '\0' ? URSATZ_ERR_REQUEST : api_fail(&out->error, URSATZ_ERR_REQUEST, "/key", "palette needs a key");
    }
    if (status == URSATZ_OK && !api_member_string(request, "with", &with_text, &found, &out->error)) {
        status = URSATZ_ERR_REQUEST;
    }
    if (status == URSATZ_OK && !request_parse_key(key_text, context->knowledge, &key)) {
        status = api_fail_unsupported_key(&out->error, "/key", context->knowledge, key_text)
                     ? URSATZ_ERR_REQUEST
                     : api_fail(&out->error, URSATZ_ERR_REQUEST, "/key", "unknown key '%s'; write a tonic letter A-G with an optional # or b, then a mode, for example \"C minor\"", key_text);
    }
    if (status == URSATZ_OK && with_text != NULL) {
        with = knowledge_snapshot_material_named(context->knowledge, with_text);
        if (with == NULL || !composer_material_supported(&materials, with)) {
            status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/with", "'%s' is not a mode the Composer writes; write major, minor, dorian, phrygian, lydian, mixolydian or locrian",
                              with_text);
        } else if (with == key.material) {
            status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/with", "%s is the key's own mode; leave with out or name another", with->key_word);
        }
    }
    cJSON *document = NULL;
    if (status == URSATZ_OK) {
        PaletteChord palette[2][PALETTE_DEGREES];
        size_t counts[2];
        ComposerChords chords;
        bool have_chords = composer_chords_init(&chords, context->knowledge, NULL, 0);
        document = cJSON_CreateObject();
        cJSON *list = document != NULL && cJSON_AddStringToObject(document, "key", key_text) != NULL ? cJSON_AddArrayToObject(document, "materials") : NULL;
        cJSON *shared = list != NULL ? cJSON_AddArrayToObject(document, "shared") : NULL;
        cJSON *color = shared != NULL ? cJSON_AddArrayToObject(document, "color") : NULL;
        if (color == NULL || !have_chords || !modal_palette(key.material, with, palette, counts)) {
            status = api_fail_alloc(&out->error);
        }
        for (int m = 0; status == URSATZ_OK && m < (with != NULL ? 2 : 1); m++) {
            const PitchMaterial *material = m == 0 ? key.material : with;
            cJSON *entry = material_json(&chords, &key, material, with, palette[m], counts[m]);
            cJSON *carriers = m == 1 || with != NULL ? cJSON_CreateObject() : NULL;
            cJSON *numerals = carriers != NULL ? cJSON_AddArrayToObject(carriers, "numerals") : NULL;
            if (entry == NULL || (with != NULL && (numerals == NULL || cJSON_AddStringToObject(carriers, "material", material->id) == NULL))) {
                cJSON_Delete(entry);
                cJSON_Delete(carriers);
                status = api_fail_alloc(&out->error);
                break;
            }
            for (size_t i = 0; i < counts[m]; i++) {
                if (m == 0 && palette[m][i].shared) {
                    cJSON_AddItemToArray(shared, cJSON_CreateString(palette[m][i].numeral));
                }
                if (palette[m][i].color && numerals != NULL) {
                    cJSON_AddItemToArray(numerals, cJSON_CreateString(palette[m][i].numeral));
                }
            }
            cJSON_AddItemToArray(list, entry);
            if (carriers != NULL) {
                cJSON_AddItemToArray(color, carriers);
            }
        }
    }
    cJSON_Delete(request);
    if (status != URSATZ_OK) {
        cJSON_Delete(document);
        return status;
    }
    return api_respond_json(out, document);
}
