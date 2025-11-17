#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../corpus_stats/stats_test_util.h"
#include "persistence/persistence_context_query.h"

/*
 * Listing by context returns what was stored for that context, in the order
 * it was stored, and nothing for another or an unknown context.
 */

int main(void) {
    const char *path = "test_persistence_context_query.sqlite";
    remove(path);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);

    stats_store_chord(persistence, "chord-1", "I", "piece-a");
    stats_store_chord(persistence, "chord-2", "IV", "piece-b");
    stats_store_chord(persistence, "chord-3", "V7", "piece-a");
    stats_store_chord(persistence, "chord-4", "I", "piece-a");
    stats_store_chord(persistence, "chord-5", "vi", NULL); /* no context at all */

    char **ids = NULL;
    size_t count = 0;
    assert(persistence_list_chord_ids_for_context(persistence, "piece-a", &ids, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 3);
    assert(strcmp(ids[0], "chord-1") == 0 && strcmp(ids[1], "chord-3") == 0 && strcmp(ids[2], "chord-4") == 0);
    persistence_free_id_list(ids, count);

    assert(persistence_list_chord_ids_for_context(persistence, "piece-b", &ids, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 1 && strcmp(ids[0], "chord-2") == 0);
    persistence_free_id_list(ids, count);

    /* An unknown context is not an error: zero ids. */
    ids = (char **)0x1;
    assert(persistence_list_chord_ids_for_context(persistence, "no-such-piece", &ids, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 0 && ids == NULL);

    /* A stored chord's id loads it back, whatever its context. */
    assert(persistence_list_chord_ids_for_context(persistence, "piece-a", &ids, &count) == PERSISTENCE_QUERY_OK);
    Chord *loaded = NULL;
    assert(persistence_load_chord(persistence, ids[1], &loaded) == PERSISTENCE_CHORD_OK);
    assert(strcmp(chord_label(loaded), "V7") == 0);
    chord_destroy(loaded);
    persistence_free_id_list(ids, count);

    /* Generic interpretations, by claim value type, with their raw claim content. */
    const char cadence_a[] = "PAC";
    const char cadence_b[] = "HC";
    const long interval = -3;
    stats_store_claim(persistence, "claim-1", "cadence-detection-top-candidate", cadence_a, sizeof(cadence_a), "piece-a");
    stats_store_claim(persistence, "claim-2", "interval-analysis-directed-semitones", &interval, sizeof(interval),
                      "piece-a");
    stats_store_claim(persistence, "claim-3", "cadence-detection-top-candidate", cadence_b, sizeof(cadence_b), "piece-a");
    stats_store_claim(persistence, "claim-4", "cadence-detection-top-candidate", cadence_b, sizeof(cadence_b), "piece-b");

    assert(persistence_list_interpretation_ids_for_context(persistence, "piece-a", "cadence-detection-top-candidate",
                                                           &ids, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 2 && strcmp(ids[0], "claim-1") == 0 && strcmp(ids[1], "claim-3") == 0);
    persistence_free_id_list(ids, count);

    PersistenceClaimContent *contents = NULL;
    assert(persistence_list_claim_contents_for_context(persistence, "piece-a", "cadence-detection-top-candidate",
                                                       &contents, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 2);
    assert(contents[0].size == sizeof(cadence_a) && strcmp((const char *)contents[0].bytes, "PAC") == 0);
    assert(contents[1].size == sizeof(cadence_b) && strcmp((const char *)contents[1].bytes, "HC") == 0);
    persistence_free_claim_contents(contents, count);

    assert(persistence_list_claim_contents_for_context(persistence, "piece-a", "interval-analysis-directed-semitones",
                                                       &contents, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 1 && contents[0].size == sizeof(long));
    long read_back;
    memcpy(&read_back, contents[0].bytes, sizeof(read_back));
    assert(read_back == -3);
    persistence_free_claim_contents(contents, count);

    contents = (PersistenceClaimContent *)0x1;
    assert(persistence_list_claim_contents_for_context(persistence, "piece-b", "interval-analysis-directed-semitones",
                                                       &contents, &count) == PERSISTENCE_QUERY_OK);
    assert(count == 0 && contents == NULL);

    /* Missing arguments. */
    assert(persistence_list_chord_ids_for_context(NULL, "piece-a", &ids, &count) == PERSISTENCE_QUERY_ERR_NULL_ARG);
    assert(persistence_list_chord_ids_for_context(persistence, NULL, &ids, &count) == PERSISTENCE_QUERY_ERR_NULL_ARG);
    assert(persistence_list_chord_ids_for_context(persistence, "piece-a", NULL, &count) ==
           PERSISTENCE_QUERY_ERR_NULL_ARG);
    assert(persistence_list_interpretation_ids_for_context(persistence, "piece-a", NULL, &ids, &count) ==
           PERSISTENCE_QUERY_ERR_NULL_ARG);
    assert(persistence_list_claim_contents_for_context(persistence, "piece-a", "x", NULL, &count) ==
           PERSISTENCE_QUERY_ERR_NULL_ARG);
    persistence_free_id_list(NULL, 0);
    persistence_free_claim_contents(NULL, 0);

    persistence_close(persistence);
    remove(path);
    printf("test_persistence_context_query: ok\n");
    return 0;
}
