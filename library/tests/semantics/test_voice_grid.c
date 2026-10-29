#include "semantics/voice_grid.h"

#include <assert.h>
#include <stdio.h>

static void test_cell_lookup(void) {
    AnalysisVoiceCell cells[6] = {{true, NULL, NULL, "a0", true, 1.0},  {true, NULL, NULL, "a1", true, 0.5},
                                  {false, NULL, NULL, NULL, false, 0.0}, {true, NULL, NULL, "b0", false, 0.9},
                                  {true, NULL, NULL, "b1", true, 0.8},  {true, NULL, NULL, "b2", true, 0.7}};
    const char *ids[2] = {"seg-0", "seg-1"};
    AnalysisVoiceGrid grid = {2, 3, ids, cells};

    assert(analysis_voice_grid_cell(&grid, 0, 0) == &cells[0]);
    assert(analysis_voice_grid_cell(&grid, 0, 2) == &cells[2]);
    assert(analysis_voice_grid_cell(&grid, 1, 0) == &cells[3]);
    assert(analysis_voice_grid_cell(&grid, 1, 2) == &cells[5]);
    assert(!analysis_voice_grid_cell(&grid, 0, 2)->present);
    assert(analysis_voice_grid_cell(&grid, 1, 1)->note_id[0] == 'b');
    assert(!analysis_voice_grid_cell(&grid, 1, 0)->attacked);
}

static void test_out_of_range(void) {
    AnalysisVoiceCell cells[2] = {{true, NULL, NULL, "a", true, 1.0}, {true, NULL, NULL, "b", true, 1.0}};
    AnalysisVoiceGrid grid = {1, 2, NULL, cells};
    assert(analysis_voice_grid_cell(&grid, 1, 0) == NULL);
    assert(analysis_voice_grid_cell(&grid, 0, 2) == NULL);
    assert(analysis_voice_grid_cell(&grid, (size_t)-1, 0) == NULL);
    assert(analysis_voice_grid_cell(NULL, 0, 0) == NULL);
    AnalysisVoiceGrid empty = {1, 2, NULL, NULL};
    assert(analysis_voice_grid_cell(&empty, 0, 0) == NULL);
}

int main(void) {
    test_cell_lookup();
    test_out_of_range();
    printf("test_voice_grid: ok\n");
    return 0;
}
