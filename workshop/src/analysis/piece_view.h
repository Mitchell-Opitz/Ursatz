#ifndef URSATZ_WORKSHOP_ANALYSIS_PIECE_VIEW_H
#define URSATZ_WORKSHOP_ANALYSIS_PIECE_VIEW_H

#include <common/status.h>
#include <text/text_view.h>

/*
 * The views of a library piece (R-663): summary, structure, harmony and part:<name> read the piece's readings/analyzer.json and realization.json
 * (R-644), full gives their texts joined by a blank line, and reading is the Analyzer's text export framed as a view (`limits` first_measure and
 * last_measure its measure range, 0 and 0 for the whole piece; `title` its title, NULL for the file name). A piece analyzed before the layers
 * were stored refuses the others with URSATZ_ERR_NOT_FOUND at "/params/view": "re-analyze the piece to store its layers". A piece that is not
 * there is URSATZ_ERR_NOT_FOUND at "/params/id". *text is caller-owned.
 */
UrsatzStatus workshop_view_piece(const char *id, const char *view, const TextViewLimits *limits, const char *title, char **text, UrsatzError *error);

#endif
