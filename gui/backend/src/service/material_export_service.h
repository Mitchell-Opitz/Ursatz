#ifndef URSATZ_GUI_MATERIAL_EXPORT_SERVICE_H
#define URSATZ_GUI_MATERIAL_EXPORT_SERVICE_H

#include <stddef.h>

/*
 * The text export of store material (P31 B5): a motif with its variants, a composed or uncomposed theme with its slot
 * motifs, a piece with its loop and ending. The service gathers the MusicXML files and the store and Composer values
 * and hands them to the Analyzer's analyzer_text_export_material, which writes every line; it formats no music. Every
 * fact value is a store or Composer string copied as it is (lists joined with ", ").
 *
 *   motif: section "motif" (the Composer's render of the motif's request with musicxml, its segments from `features`,
 *          the facts role, form, meter, key, pickup, notes, base harmony and tones), then one section for each variant
 *          "variant <n>: <operations>" (the variant rendered the same way, facts kind, kept, changed, functions, flag).
 *   theme: section "theme" from the version's theme.musicxml (none when it was not composed); material facts from the
 *          form, the chords ("bar <n>") and the slots; one section "slot <slot>: <motif name> v<n>" for each filled slot.
 *   piece: sections "loop" and "ending" from the version's loop.musicxml and ending.musicxml (none when not composed);
 *          material facts from the structure's key, mode and meter and one fact for each section.
 *
 * The Composer's tone labels are a list of objects, so `tones` is "<note> <type>" for each note joined with ", " (the
 * role when the label has no type), and `base harmony` is the base_harmony text of the stored analysis.
 */

typedef enum {
  URSATZ_GUI_MATERIAL_EXPORT_OK = 0,
  URSATZ_GUI_MATERIAL_EXPORT_NOT_FOUND,
  URSATZ_GUI_MATERIAL_EXPORT_REFUSED,
  URSATZ_GUI_MATERIAL_EXPORT_FAILED,
  URSATZ_GUI_MATERIAL_EXPORT_ALLOC
} UrsatzGuiMaterialExportResult;

#define URSATZ_GUI_MATERIAL_EXPORT_MESSAGE_SIZE 512

/*
 * Exports version `version` of item `item_id` (0 is the current one) as text. On OK, *text is newly allocated and
 * caller-owned (free() it). NOT_FOUND: no such item (any id the store does not hold) or version. REFUSED: the
 * Composer refused to render a motif; message holds its reason. FAILED: the Analyzer or the store failed; message is
 * "ANALYZER_ERR_ARGUMENT", "ANALYZER_ERR_LIBRARY" or a short reason. ALLOC: out of memory. message is empty for NOT_FOUND
 * and OK.
 */
UrsatzGuiMaterialExportResult ursatz_gui_material_export(const char *item_id, int version, char **text,
                                                          char message[URSATZ_GUI_MATERIAL_EXPORT_MESSAGE_SIZE]);

#endif /* URSATZ_GUI_MATERIAL_EXPORT_SERVICE_H */
