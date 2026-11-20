#ifndef URSATZ_TEXT_TEXT_LAYERS_H
#define URSATZ_TEXT_TEXT_LAYERS_H

#include "plan/plan_json.h"
#include "text/text.h"
#include "theory/knowledge/knowledge_snapshot.h"

/* cJSON.h declares it; a header of this group needs only the name. */
typedef struct cJSON cJSON;

/*
 * The four layer kinds as Ursatz Text (R-662): a plan, a reading, a realization (with its scoring assignments, which the realization
 * codec writes inside a movement), a knowledge file and a knowledge entry, through the tree form of text_tree.h. The codecs are the
 * layers' own: the JSON text_to_json gives for a plan, a reading or a realization is what the codec writes for the object the text
 * reads back to, so print then parse is lossless in that sense.
 */

/*
 * The JSON text as a document of `kind`. It is parsed and checked to be that kind: "format" "ursatz-plan" or "ursatz-composer-plan"
 * for a plan, "ursatz-reading", "ursatz-realization"; "schema" "ursatz-knowledge" for a knowledge file; an object with a string "id"
 * and "kind" for a knowledge entry; any object for json. Otherwise "this is not a <kind>" (line 0). NULL with *error on failure.
 * Caller frees.
 */
char *text_from_json(const char *json, TextKind kind, TextError *error);

/*
 * A document as JSON. A plan, a reading or a realization goes through its codec (plan_from_json and plan_to_json, ...), so the answer
 * is the codec's text and a refusal is the codec's: "<path>: <message>" at line 0. A knowledge file or entry and json answer
 * cJSON_Print of the tree (the snapshot does not check them; tools/ci/check-knowledge.py does). *kind is the document's kind.
 * NULL with *error on failure. Caller frees.
 */
char *text_to_json(const KnowledgeSnapshot *knowledge, const char *text, TextKind *kind, TextError *error);

/* The producer a plan document names in its "metadata" (producer, producer_version, producer_key, composition_key, knowledge_hash); an
   absent member is empty. false when `document` is not an object. */
bool text_plan_producer(const cJSON *document, PlanProducer *out);

#endif
