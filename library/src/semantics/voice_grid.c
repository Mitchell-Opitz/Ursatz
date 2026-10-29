#include "semantics/voice_grid.h"

const AnalysisVoiceCell *analysis_voice_grid_cell(const AnalysisVoiceGrid *grid, size_t segment, size_t voice) {
    if (grid == NULL || grid->cells == NULL || segment >= grid->segment_count || voice >= grid->voice_count) {
        return NULL;
    }
    return &grid->cells[segment * grid->voice_count + voice];
}
