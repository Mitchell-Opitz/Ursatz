#include "analysis/piece_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/analysis.h"
#include "items/layers.h"
#include "items/layout.h"
#include "items/library_pieces.h"
#include "items/views.h"

UrsatzStatus workshop_view_piece(const char *id, const char *view, const TextViewLimits *limits, const char *title, char **text, UrsatzError *error) {
    *text = NULL;
    char dir[320];
    if (!workshop_library_piece_dir(WORKSHOP_DIR_PIECES, id, dir, sizeof dir)) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/id", "piece not found");
    }
    char address[160];
    snprintf(address, sizeof address, "piece:%s", id);
    if (strcmp(view, "reading") == 0) {
        char *export_text = NULL;
        UrsatzStatus status = workshop_analysis_get_piece_export(WORKSHOP_DIR_PIECES, id, limits->first_measure, limits->last_measure, title, &export_text);
        if (status != URSATZ_OK) {
            switch (status) {
                case URSATZ_ERR_NOT_FOUND:
                    return ursatz_error_set(error, status, "/params/id", "piece not found");
                case URSATZ_ERR_FORMAT:
                    return ursatz_error_set(error, status, "", "could not import the stored source");
                case URSATZ_ERR_REQUEST:
                    return ursatz_error_set(error, status, "/params/measures", "no measure in that range");
                default:
                    return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "failed to export the analysis");
            }
        }
        char expand[256];
        if (limits->first_measure > 0 || limits->last_measure > 0) {
            snprintf(expand, sizeof expand, "ursatz show %s --view reading --measures %d-%d", address, limits->first_measure > 0 ? limits->first_measure : 1,
                     limits->last_measure);
        } else {
            snprintf(expand, sizeof expand, "ursatz show %s --view reading --measures a-b", address);
        }
        *text = text_view_frame("reading", address, export_text, "nothing the analysis document holds; the notes are the import's", expand);
        free(export_text);
        return *text ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    bool none = false;
    UrsatzStatus status = workshop_view_folder(dir, false, WORKSHOP_DIR_READINGS "/analyzer.json", address, view, limits, text, &none, error);
    if (status == URSATZ_OK && none) {
        status = ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/view", "re-analyze the piece to store its layers");
    }
    return status;
}
