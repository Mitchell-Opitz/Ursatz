#include "export/score_quantize.h"

#include <stdlib.h>
#include <string.h>

ScoreQuantizeOptions score_quantize_options_default(void) {
    ScoreQuantizeOptions options;
    options.grid_denominator = 32;
    options.allow_triplets = true;
    options.gap_absorb_numerator = 1;
    options.gap_absorb_denominator = 4;
    return options;
}

void score_quantized_destroy(ScoreQuantized *quantized) {
    if (quantized == NULL) {
        return;
    }
    free(quantized->start_num);
    free(quantized->start_den);
    free(quantized->end_num);
    free(quantized->end_den);
    memset(quantized, 0, sizeof(*quantized));
}
