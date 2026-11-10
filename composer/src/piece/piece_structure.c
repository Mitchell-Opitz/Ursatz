#include <ursatz_composer/piece_structure.h>

#include <string.h>

#include <ursatz_composer/form_spec.h>

/*
 * The structure of a game loop as a piece request of the form "game-loop" (piece.h): the sections are shared, not copied.
 * Until P44 B5 retires PieceStructure these call the one builder.
 */
static PieceRequest request_of(const PieceStructure *structure) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = "game-loop";
    request.key = structure->key;
    request.meter = structure->meter;
    request.motive = structure->motive;
    request.has_accompaniment = structure->has_accompaniment;
    request.accompaniment = structure->accompaniment;
    request.has_style = structure->has_style;
    request.style = structure->style;
    request.sections = structure->sections;
    request.section_count = structure->section_count;
    request.has_voicing = structure->has_voicing;
    request.voicing = structure->voicing;
    return request;
}

PieceResult piece_structure_check(const PieceStructure *structure) {
    if (!structure) {
        return PIECE_ERR_NULL;
    }
    PieceRequest request = request_of(structure);
    return piece_request_check(&request);
}

void piece_structure_default_cadences(PieceStructure *structure) {
    FormSpec *form = NULL;
    if (!structure || !structure->sections || form_spec_builtin("game-loop", &form) != FORM_SPEC_OK) {
        return;
    }
    PieceRequest request = request_of(structure);
    piece_default_cadences(form, &request);
    form_spec_destroy(form);
}

bool piece_structure_default_ending(PieceStructure *structure, size_t capacity) {
    FormSpec *form = NULL;
    if (!structure || !structure->sections || form_spec_builtin("game-loop", &form) != FORM_SPEC_OK) {
        return false;
    }
    PieceRequest request = request_of(structure);
    bool added = piece_default_ending(form, &request, capacity);
    structure->section_count = request.section_count;
    form_spec_destroy(form);
    return added;
}

PieceResult piece_structure_build_plan(const PieceStructure *structure, PiecePlan **out) {
    if (!structure || !out) {
        return PIECE_ERR_NULL;
    }
    PieceRequest request = request_of(structure);
    return piece_build_plan(&request, out);
}

void piece_structure_basic_idea(const PieceStructure *structure, size_t *theme_bars, size_t *idea_bars) {
    PieceRequest request = request_of(structure);
    if (!piece_basic_idea(&request, theme_bars, idea_bars)) {
        *theme_bars = 8;
        *idea_bars = 2;
    }
}

PieceResult piece_ending_plan(const PiecePlan *loop_plan, const PieceStructure *structure, PiecePlan **out) {
    if (!loop_plan || !structure) {
        if (out) {
            *out = NULL;
        }
        return PIECE_ERR_NULL;
    }
    PieceRequest request = request_of(structure);
    return piece_build_ending(loop_plan, &request, out);
}
