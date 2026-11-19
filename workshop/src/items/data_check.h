#ifndef WORKSHOP_DATA_CHECK_H
#define WORKSHOP_DATA_CHECK_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Startup data check (P55 B2): the GUI before the overhaul migrated its stores once on start (the corpus.db import,
 * the tone and segment re-analysis of motifs, the clearing of old themes) and let a motif stand on any tonic. Those
 * paths are deleted, so a data folder they never ran on must be refused instead of read wrongly.
 *
 * data_dir is the folder that holds collections/, materials/ and data/ (the backend's working directory). With a
 * `.pre-overhaul-checked` file in it the check passes at once. Otherwise it collects these problems:
 *   - data/corpus.db exists and collections/.imported-from-corpus-db does not;
 *   - materials/items/ holds an item and materials/ lacks one of tones-migrated, segments-migrated, themes-cleared;
 *   - a motif whose current version has a key other than C major or C minor (an empty key does not count).
 * With problems nothing is written, false is returned and message holds the refusal (what is missing, and to run the
 * GUI built from the git tag pre-overhaul once on this folder). Without problems the marker is written and true is
 * returned with an empty message. A marker that cannot be written is a refusal naming it.
 */
bool workshop_data_check(const char *data_dir, char *message, size_t size);

#endif
