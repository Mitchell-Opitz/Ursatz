#include "composition/variants.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/http_status.h"
#include "base/last_error.h"
#include "composition/compose_jobs.h"
#include "composition/composer_client.h"
#include "composition/motifs.h"
#include "items/store.h"
#include "base/params.h"
#include <common/json.h>

/* The variant engine of the motif area (A20261109-76): variants and each-op jobs, reroll, save, flag, promote, recognize, link form and
   the variant tree. The routes parse a request and call one function here. */

/* A store failure as an error: the message the store left, or the fallback. Returns status. */
static UrsatzStatus store_failure(UrsatzStatus status, UrsatzError *error) {
  const UrsatzError *last = workshop_last_error();
  ursatz_error_set(error, status, NULL, "%s", last->message[0] != '\0' ? last->message : "material store failure");
  return status;
}

/* The names --function takes, the vocabulary's functions; the stored derived.functions hold the same. */
static bool function_name_ok(const char *name) {
  return workshop_composer_vocabulary_has("functions", name);
}

/* The version body of the basic motive of motif's lineage: the end of its derived.source_item links, at most 32 hops.
   Caller-owned; NULL when the motif is not derived or the lineage cannot be read. */
cJSON *workshop_variants_lineage_root(const cJSON *motif) {
  cJSON *root = NULL;
  const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
  for (int hops = 0; hops < 32 && cJSON_IsObject(derived); hops++) {
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
    cJSON *body = NULL;
    if (!cJSON_IsString(source) || workshop_material_item_get(source->valuestring, NULL, &body) != URSATZ_OK) {
      break;
    }
    cJSON_Delete(root);
    root = body;
    derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "motif"), "derived");
  }
  return cJSON_IsObject(derived) ? (cJSON_Delete(root), NULL) : root;
}

/* The largest seed a variants request takes: limits.seed_max.variants of the vocabulary, -1 when it is not given (no
   seed is then valid). */
static double max_variant_seed(void) {
  return workshop_composer_seed_max("variants");
}

/* A variants run on the job queue: the variants of the item's motif, the one variant per operation (each-op), or a reroll
   (a new version of a derived motif). The queue owns it. */
struct WorkshopVariantsJob {
  WorkshopVariantsOp op;
  char *id;
  cJSON *body; /* the request body of the variants route */
  int count;
  double seed; /* negative: from the clock */
};

void workshop_variants_job_free(void *input) {
  WorkshopVariantsJob *job = input;
  if (job) {
    cJSON_Delete(job->body);
    free(job->id);
    free(job);
  }
}

const char *workshop_variants_job_kind(const WorkshopVariantsJob *job) {
  return job->op == WORKSHOP_VARIANTS_VARIANTS ? "variants" : job->op == WORKSHOP_VARIANTS_EACH_OP ? "each-op" : "reroll";
}

/* A job input; takes body. NULL on allocation failure (body is freed too). */
static WorkshopVariantsJob *variants_job_new(WorkshopVariantsOp op, const char *id, cJSON *body, int count, double seed) {
  WorkshopVariantsJob *job = calloc(1, sizeof(*job));
  if (job) {
    job->id = strdup(id);
  }
  if (!job || !job->id) {
    free(job);
    cJSON_Delete(body);
    return NULL;
  }
  job->op = op;
  job->body = body;
  job->count = count;
  job->seed = seed;
  return job;
}

static void job_failure(WorkshopJobResult *out, int status, const char *message) {
  workshop_job_result_failure(out, status, message);
}

static void job_ok_with(WorkshopJobResult *out, const char *key, cJSON *value) {
  cJSON *object = cJSON_CreateObject();
  if (!object || !value || !cJSON_AddBoolToObject(object, "ok", 1) || !cJSON_AddItemToObject(object, key, value)) {
    cJSON_Delete(value);
    cJSON_Delete(object);
    job_failure(out, 500, "out of memory");
    return;
  }
  workshop_job_result_json(out, 200, object);
}

static void job_material_failure(WorkshopJobResult *out, UrsatzStatus result) {
  const UrsatzError *last = workshop_last_error();
  job_failure(out, workshop_http_status(result, last), last->message[0] != '\0' ? last->message : "material store failure");
}

/* The jobs of the variants and each-op routes: checks the request body and makes the job input (taking body, also on a
   failure). The names (which functions, which distance, how many) are the Composer's to refuse, with the path at fault. */
UrsatzStatus workshop_variants_job_new(WorkshopVariantsOp op, const char *id, cJSON *body, WorkshopVariantsJob **out,
                                       UrsatzError *error) {
  *out = NULL;
  if (op == WORKSHOP_VARIANTS_REROLL) {
    *out = variants_job_new(op, id, NULL, 0, -1);
    cJSON_Delete(body);
    return *out ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(body, "seed");
  if (cJSON_IsNull(seed_item)) {
    seed_item = NULL;
  }
  int count = 0;
  double seed = -1;
  if (op == WORKSHOP_VARIANTS_EACH_OP) {
    if (seed_item && (!cJSON_IsNumber(seed_item) || seed_item->valuedouble < 0 ||
                      seed_item->valuedouble > max_variant_seed() ||
                      seed_item->valuedouble != (double)(long long)seed_item->valuedouble)) {
      cJSON_Delete(body);
      return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "seed is a whole number from 0");
    }
    seed = seed_item ? seed_item->valuedouble : -1;
  } else {
    const cJSON *chain = cJSON_GetObjectItemCaseSensitive(body, "chain");
    const cJSON *count_item = cJSON_GetObjectItemCaseSensitive(body, "count");
    const cJSON *functions = cJSON_GetObjectItemCaseSensitive(body, "functions");
    const cJSON *distance_item = cJSON_GetObjectItemCaseSensitive(body, "distance");
    distance_item = cJSON_IsNull(distance_item) ? NULL : distance_item;
    functions = cJSON_IsNull(functions) ? NULL : functions;
    chain = cJSON_IsNull(chain) ? NULL : chain;
    count_item = cJSON_IsNull(count_item) ? NULL : count_item;
    count = 6;
    bool functions_ok = !functions || cJSON_IsArray(functions);
    const cJSON *function = NULL;
    cJSON_ArrayForEach(function, functions) {
      functions_ok = functions_ok && cJSON_IsString(function);
    }
    const char *distance = cJSON_IsString(distance_item) ? distance_item->valuestring : NULL;
    bool distance_ok = !distance_item || distance;
    if (distance_ok && distance && chain) {
      cJSON_Delete(body);
      return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "distance takes random operations");
    }
    bool valid = functions_ok && distance_ok &&
                 (!chain || (cJSON_IsObject(chain) && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(chain, "steps")))) &&
                 (!count_item || (cJSON_IsNumber(count_item) && count_item->valuedouble >= 1 &&
                                  count_item->valuedouble <= 16 && count_item->valuedouble == (int)count_item->valuedouble)) &&
                 (!seed_item || (cJSON_IsNumber(seed_item) && seed_item->valuedouble >= 0 &&
                                 seed_item->valuedouble <= max_variant_seed() &&
                                 seed_item->valuedouble == (double)(long long)seed_item->valuedouble));
    if (!valid) {
      cJSON_Delete(body);
      return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL,
                              "chain is {steps: [...]}, count is 1 to 16, seed a whole number from 0, distance a name and functions a list of names");
    }
    if (count_item) {
      count = (int)count_item->valuedouble;
    }
    if (seed_item) {
      seed = seed_item->valuedouble;
    }
  }
  *out = variants_job_new(op, id, body, count, seed);
  return *out ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
}

/* The motif member of the version body a {item, version} reference names, with the body kept in *body. NULL with
   *error set when the reference is malformed, gone, not a motif or has no degrees (not analyzed). */
static const cJSON *motif_by_reference(const cJSON *reference, const char *which, cJSON **body, UrsatzStatus *status,
                                       UrsatzError *error) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(reference, "item");
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
  if (!cJSON_IsString(item) || !cJSON_IsNumber(version) || version->valuedouble < 1 ||
      version->valuedouble != (int)version->valuedouble) {
    *status = ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "%s is {item, version}", which);
    return NULL;
  }
  UrsatzStatus result = workshop_material_item_get_version(item->valuestring, version->valueint, body);
  if (result != URSATZ_OK) {
    *status = ursatz_error_set(error, result == URSATZ_ERR_NOT_FOUND ? URSATZ_ERR_NOT_FOUND : result, NULL,
                               "the %s motif or its version was not found", which);
    return NULL;
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(*body, "motif");
  const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
  if (!cJSON_IsObject(motif) || !cJSON_IsString(degrees) || degrees->valuestring[0] == '\0') {
    *status = ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "the %s is not an analyzed motif", which);
    return NULL;
  }
  return motif;
}

/* How the target of request {source: {item, version}, target: {item, version}} derives from the source, from the Composer's
   recognize command: the Composer's document plus "ok". Nothing is saved. *http_status is the status of a failure (the
   Composer's refusal keeps its path in error). */
UrsatzStatus workshop_variants_recognize(const cJSON *request, cJSON **document_out, int *http_status, UrsatzError *error) {
  *document_out = NULL;
  UrsatzStatus status = URSATZ_OK;
  cJSON *source_body = NULL;
  cJSON *target_body = NULL;
  const cJSON *source = motif_by_reference(cJSON_GetObjectItemCaseSensitive(request, "source"), "source", &source_body,
                                           &status, error);
  const cJSON *target = source ? motif_by_reference(cJSON_GetObjectItemCaseSensitive(request, "target"), "target",
                                                    &target_body, &status, error)
                               : NULL;
  if (!target) {
    cJSON_Delete(source_body);
    *http_status = workshop_http_status(status, NULL);
    return status;
  }
  bool bad = false;
  const char *form = workshop_param_string(source, "form", &bad);
  const char *meter = workshop_param_string(source, "meter", &bad);
  char source_key[64];
  char target_key_text[64];
  const char *key = workshop_motif_key_text(source, source_key, sizeof(source_key));
  const char *pickup = workshop_param_string(source, "pickup", &bad);
  const char *target_pickup = workshop_param_string(target, "pickup", &bad);
  const char *source_degrees = workshop_param_string(source, "degrees", &bad);
  const char *target_degrees = workshop_param_string(target, "degrees", &bad);
  const char *target_form = workshop_param_string(target, "form", &bad);
  const char *target_meter = workshop_param_string(target, "meter", &bad);
  const char *target_key = workshop_motif_key_text(target, target_key_text, sizeof(target_key_text));
  if (bad || !form || !meter || !source_degrees || !target_degrees ||
      (target_form && strcmp(form, target_form) != 0) || (target_meter && strcmp(meter, target_meter) != 0) ||
      (key ? !target_key || strcmp(key, target_key) != 0 : target_key != NULL)) {
    cJSON_Delete(source_body);
    cJSON_Delete(target_body);
    *http_status = 400;
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "the two motifs need the same form, meter and key to be compared");
  }
  bool full = strcmp(form, "full") == 0;
  cJSON *source_request = workshop_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, source_degrees, NULL, NULL, NULL);
  cJSON *target_request = workshop_motif_motive_request(full ? "degrees" : NULL, full ? NULL : form, target_degrees, NULL, NULL, NULL);
  const char *role = workshop_param_string(source, "role", &(bool){false});
  cJSON *recognize = cJSON_CreateObject();
  bool built = source_request && target_request && recognize &&
               workshop_motif_add_extras(source_request, workshop_motif_harmony(source), cJSON_GetObjectItemCaseSensitive(source, "features"),
                                 NULL) &&
               workshop_motif_add_extras(target_request, workshop_motif_harmony(target), cJSON_GetObjectItemCaseSensitive(target, "features"),
                                 NULL) &&
               cJSON_AddStringToObject(recognize, "meter", meter) &&
               (!key || key[0] == '\0' || cJSON_AddStringToObject(recognize, "key", key)) &&
               common_json_add_string_if(recognize, "role", role) &&
               common_json_add_string_if(recognize, "pickup", pickup) &&
               common_json_add_string_if(recognize, "target_pickup", target_pickup);
  if (built) {
    built = cJSON_AddItemToObject(recognize, "source", source_request);
    source_request = built ? NULL : source_request;
  }
  if (built) {
    built = cJSON_AddItemToObject(recognize, "target", target_request);
    target_request = built ? NULL : target_request;
  }
  cJSON_Delete(source_request);
  cJSON_Delete(target_request);
  cJSON_Delete(source_body);
  cJSON_Delete(target_body);
  WorkshopComposerReply reply;
  bool called = built && workshop_motif_recognize(recognize, &reply);
  cJSON_Delete(recognize);
  if (!called) {
    *http_status = 500;
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  cJSON *document = reply.status == URSATZ_OK && reply.json ? cJSON_Parse(reply.json) : NULL;
  if (!document) {
    *http_status = workshop_composer_reply_failure_status(&reply);
    status = reply.status != URSATZ_OK ? reply.status : URSATZ_ERR_INTERNAL;
    ursatz_error_set(error, status, reply.path, "%s", reply.message[0] ? reply.message : "composer could not recognize the motifs");
    workshop_composer_reply_destroy(&reply);
    return status;
  }
  workshop_composer_reply_destroy(&reply);
  if (!cJSON_AddBoolToObject(document, "ok", 1)) {
    cJSON_Delete(document);
    *http_status = 500;
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  *document_out = document;
  return URSATZ_OK;
}

/* The analysis text of a variant's motive document for the form: notes_text (degrees_text when degrees is set),
   rhythm_text or contour_text. */
static const char *variant_text(const cJSON *variant, const char *form, bool degrees) {
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis");
  const char *name = strcmp(form, "rhythm") == 0 ? "rhythm_text" : strcmp(form, "contour") == 0 ? "contour_text" : degrees ? "degrees_text" : "notes_text";
  const cJSON *text = cJSON_GetObjectItemCaseSensitive(analysis, name);
  return cJSON_IsString(text) && text->valuestring[0] != '\0' ? text->valuestring : NULL;
}

/* Adds a duplicate of the member of from named name to object (an empty array when from has none of that type). */
static bool copy_member(cJSON *object, const cJSON *from, const char *name, bool as_array) {
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(from, name);
  cJSON *copy = (as_array ? cJSON_IsArray(value) : value != NULL) ? cJSON_Duplicate(value, 1) : cJSON_CreateArray();
  return copy && cJSON_AddItemToObject(object, name, copy);
}

static bool apply_variant_extras(cJSON *derived, const cJSON *variant);
static bool set_variant_motif_members(cJSON *motif, const cJSON *variant);

/* The derivation record of a stored variant: the chain steps, the seed, the kind and the dimensions kept and changed. */
static cJSON *derived_record(const char *source_id, int source_version, const cJSON *variant, const char *flag) {
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  const cJSON *seed = cJSON_GetObjectItemCaseSensitive(variant, "seed");
  const cJSON *kind = cJSON_GetObjectItemCaseSensitive(variant, "kind");
  cJSON *derived = cJSON_CreateObject();
  cJSON *steps = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(chain, "steps"), 1);
  bool ok = derived && steps && cJSON_AddStringToObject(derived, "source_item", source_id) &&
            cJSON_AddNumberToObject(derived, "source_version", source_version) &&
            cJSON_AddItemToObject(derived, "operations", steps) &&
            cJSON_AddNumberToObject(derived, "seed", cJSON_IsNumber(seed) ? seed->valuedouble : 0) &&
            cJSON_AddStringToObject(derived, "kind", cJSON_IsString(kind) ? kind->valuestring : "motive-form") &&
            copy_member(derived, variant, "kept", true) && copy_member(derived, variant, "changed", true) &&
            apply_variant_extras(derived, variant) && cJSON_AddStringToObject(derived, "flag", flag);
  if (!ok) {
    if (derived && !cJSON_GetObjectItemCaseSensitive(derived, "operations")) {
      cJSON_Delete(steps);
    }
    cJSON_Delete(derived);
    return NULL;
  }
  return derived;
}
/* Writes prefix and text into out (501 bytes), cut with "..." on a character boundary when over 500 bytes. */
static void bounded_note(char *out, size_t size, const char *prefix, const char *text) {
  snprintf(out, size, "%s%s", prefix, text);
  if (strlen(prefix) + strlen(text) > size - 1) {
    size_t cut = size - 4;
    while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80) {
      cut--;
    }
    snprintf(out + cut, size - cut, "...");
  }
}

/* True when the variant document carries what a derived motif is made from. */
static bool variant_ok(const cJSON *variant) {
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  return cJSON_IsObject(variant) && cJSON_IsObject(chain) &&
         cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(chain, "steps")) &&
         cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(variant, "seed")) &&
         cJSON_IsString(cJSON_GetObjectItemCaseSensitive(variant, "kind")) &&
         cJSON_IsString(cJSON_GetObjectItemCaseSensitive(variant, "derivation")) &&
         cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis"));
}

static bool flag_ok(const char *flag) {
  return flag && (strcmp(flag, "approved") == 0 || strcmp(flag, "rejected") == 0 || strcmp(flag, "unflagged") == 0);
}

/* "<source name> <op>[ + <op>]", the source name cut so the whole stays within 200 bytes. */
static void variant_default_name(const char *source_name, const cJSON *steps, char *out, size_t size) {
  char ops[160] = "";
  size_t used = 0;
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, steps) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    if (cJSON_IsString(op)) {
      used += (size_t)snprintf(ops + used, sizeof(ops) - used, "%s%s", used ? " + " : "", op->valuestring);
      if (used >= sizeof(ops)) {
        used = sizeof(ops) - 1;
        break;
      }
    }
  }
  size_t limit = size - 1 > 200 ? 200 : size - 1;
  size_t keep = strlen(source_name);
  size_t room = limit > used + 1 ? limit - used - 1 : 0;
  if (keep > room) {
    keep = room;
    while (keep > 0 && ((unsigned char)source_name[keep] & 0xC0) == 0x80) {
      keep--;
    }
  }
  snprintf(out, size, "%.*s %s", (int)keep, source_name, ops);
}
/* A derived motif made from one candidate of the variants run: body {variant, name?, flag}. The new item in *item. */
UrsatzStatus workshop_variants_save(const char *id, const cJSON *body, cJSON **item_out, UrsatzError *error) {
  *item_out = NULL;
  bool bad = false;
  const cJSON *variant = cJSON_GetObjectItemCaseSensitive(body, "variant");
  const char *name = workshop_param_string(body, "name", &bad);
  const char *flag = workshop_param_string(body, "flag", &bad);
  if (!flag) {
    flag = "unflagged";
  }
  if (bad || !variant_ok(variant) || !flag_ok(flag)) {
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "variant must be a candidate from the variants route; flag is approved, rejected or unflagged");
  }
  cJSON *meta = NULL;
  cJSON *source = NULL;
  UrsatzStatus result = workshop_material_item_get(id, &meta, &source);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(source, "motif");
  const char *form = workshop_param_string(motif, "form", &(bool){false});
  const char *text = form ? variant_text(variant, form, false) : NULL;
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
  if (!cJSON_IsObject(motif) || !form || !text || !variant_text(variant, form, true) || !cJSON_IsNumber(version)) {
    cJSON_Delete(meta);
    cJSON_Delete(source);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "the variant does not match this motif");
  }
  const char *source_name = workshop_param_string(meta, "name", &(bool){false});
  const char *project_id = workshop_param_string(meta, "project_id", &(bool){false});
  char default_name[256];
  variant_default_name(source_name ? source_name : "", cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "chain"), "steps"),
                       default_name, sizeof(default_name));
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(variant, "derivation");
  char note[501];
  bounded_note(note, sizeof(note), "", derivation->valuestring);

  cJSON *new_body = cJSON_CreateObject();
  cJSON *new_motif = cJSON_CreateObject();
  cJSON *derived = derived_record(id, version->valueint, variant, flag);
  const cJSON *role = cJSON_GetObjectItemCaseSensitive(motif, "role");
  const cJSON *material = cJSON_GetObjectItemCaseSensitive(motif, "material");
  const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
  const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
  cJSON *analysis = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "motive"), 1);
  bool ok = new_body && new_motif && derived && analysis && cJSON_IsString(role) && cJSON_IsString(meter) &&
            cJSON_IsString(pickup) && cJSON_AddArrayToObject(new_body, "uses") &&
            cJSON_AddStringToObject(new_motif, "role", role->valuestring) &&
            cJSON_AddStringToObject(new_motif, "form", form) && cJSON_AddStringToObject(new_motif, "notes", text) &&
            cJSON_AddStringToObject(new_motif, "meter", meter->valuestring) &&
            (cJSON_IsString(material) ? cJSON_AddStringToObject(new_motif, "material", material->valuestring) != NULL
                                      : cJSON_AddNullToObject(new_motif, "material") != NULL) &&
            cJSON_AddStringToObject(new_motif, "pickup", pickup->valuestring) &&
            set_variant_motif_members(new_motif, variant) && cJSON_AddItemToObject(new_motif, "analysis", analysis) &&
            cJSON_AddStringToObject(new_motif, "degrees", variant_text(variant, form, true)) &&
            cJSON_AddItemToObject(new_motif, "derived", derived) && cJSON_AddItemToObject(new_body, "motif", new_motif);
  if (!ok) {
    /* Whatever was not attached to a parent is still ours. */
    if (analysis && !cJSON_GetObjectItemCaseSensitive(new_motif, "analysis")) {
      cJSON_Delete(analysis);
    }
    if (derived && new_motif && !cJSON_GetObjectItemCaseSensitive(new_motif, "derived")) {
      cJSON_Delete(derived);
    }
    if (new_motif && new_body && !cJSON_GetObjectItemCaseSensitive(new_body, "motif")) {
      cJSON_Delete(new_motif);
    }
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(source);
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  cJSON *item = NULL;
  result = workshop_material_item_create_noted("motif", name && name[0] != '\0' ? name : default_name,
                                                 project_id ? project_id : "", new_body, note, &item);
  cJSON_Delete(new_body);
  cJSON_Delete(meta);
  cJSON_Delete(source);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  *item_out = item;
  return URSATZ_OK;
}

static bool kind_ok(const char *kind) {
  return kind && (strcmp(kind, "exact") == 0 || strcmp(kind, "motive-form") == 0 || strcmp(kind, "variant") == 0);
}

/* The derived object of a motif version body, NULL when it has none. */
static cJSON *derived_of(cJSON *version_body) {
  cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(version_body, "motif"), "derived");
  return cJSON_IsObject(derived) ? derived : NULL;
}

/* A new version that changes only derived.flag (and kind): body {flag, kind?}. */
UrsatzStatus workshop_variants_flag(const char *id, const cJSON *body, cJSON **item_out, UrsatzError *error) {
  *item_out = NULL;
  bool bad = false;
  const char *flag = workshop_param_string(body, "flag", &bad);
  const char *kind = workshop_param_string(body, "kind", &bad);
  if (bad || !flag_ok(flag) || (kind && !kind_ok(kind))) {
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "flag is approved, rejected or unflagged; kind is exact, motive-form or variant");
  }
  cJSON *current = NULL;
  UrsatzStatus result = workshop_material_item_get(id, NULL, &current);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  cJSON *derived = derived_of(current);
  if (!derived) {
    cJSON_Delete(current);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "only a derived motif has a flag");
  }
  if (!common_json_set_string(derived, "flag", flag) || (kind && !common_json_set_string(derived, "kind", kind))) {
    cJSON_Delete(current);
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  cJSON *item = NULL;
  result = workshop_material_item_add_version(id, current, flag, &item);
  cJSON_Delete(current);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  *item_out = item;
  return URSATZ_OK;
}

/* A variant becomes a motif of its own: body {name, project_id, note?}. A new version with derived.standalone true (R-192), the
   item renamed and moved. The id, versions and derived record stay. */
UrsatzStatus workshop_variants_promote(const char *id, const cJSON *body, cJSON **item_out, UrsatzError *error) {
  *item_out = NULL;
  bool bad = false;
  const char *name = workshop_param_string(body, "name", &bad);
  const char *project_id = workshop_param_string(body, "project_id", &bad);
  const char *note = workshop_param_string(body, "note", &bad);
  if (bad || !name || !project_id) {
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "promote needs a name and a project_id; note is a string");
  }
  cJSON *current = NULL;
  UrsatzStatus result = workshop_material_item_get(id, NULL, &current);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  cJSON *derived = derived_of(current);
  if (!derived || !workshop_material_item_is_variant(current)) {
    cJSON_Delete(current);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "only a variant can be saved as a new motif");
  }
  /* The project is checked up front, so a bad name cannot leave the item moved. */
  cJSON *project = NULL;
  result = workshop_material_project_get(project_id, &project);
  cJSON_Delete(project);
  cJSON *item = NULL;
  if (result == URSATZ_OK) {
    result = workshop_material_item_rename(id, name, &item);
    cJSON_Delete(item);
    item = NULL;
  }
  if (result == URSATZ_OK) {
    result = workshop_material_item_move(id, project_id, &item);
    cJSON_Delete(item);
    item = NULL;
  }
  if (result == URSATZ_OK) {
    cJSON_DeleteItemFromObjectCaseSensitive(derived, "standalone");
    result = cJSON_AddTrueToObject(derived, "standalone")
                 ? workshop_material_item_add_version(id, current, note && *note ? note : "Saved as new motif", &item)
                 : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
  }
  cJSON_Delete(current);
  if (result != URSATZ_OK) {
    cJSON_Delete(item);
    return store_failure(result, error);
  }
  *item_out = item;
  return URSATZ_OK;
}

/* A chain for the Composer from stored operations: the steps with their params, less the ones the seed drew (`open`),
   so a rerun draws them again. NULL when out of memory. */
static cJSON *chain_for_reroll(const cJSON *operations) {
  cJSON *chain = cJSON_CreateObject();
  cJSON *steps = chain ? cJSON_AddArrayToObject(chain, "steps") : NULL;
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, operations) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(step, "params");
    const cJSON *open = cJSON_GetObjectItemCaseSensitive(step, "open");
    cJSON *entry = cJSON_CreateObject();
    cJSON *kept = cJSON_CreateObject();
    if (!steps || !entry || !kept || !cJSON_IsString(op) || !cJSON_AddStringToObject(entry, "op", op->valuestring)) {
      cJSON_Delete(entry);
      cJSON_Delete(kept);
      cJSON_Delete(chain);
      return NULL;
    }
    const cJSON *param = NULL;
    cJSON_ArrayForEach(param, params) {
      bool drawn = false;
      const cJSON *name = NULL;
      cJSON_ArrayForEach(name, open) {
        drawn = drawn || (cJSON_IsString(name) && param->string && strcmp(name->valuestring, param->string) == 0);
      }
      if (!drawn && !cJSON_AddItemToObject(kept, param->string, cJSON_Duplicate(param, 1))) {
        cJSON_Delete(entry);
        cJSON_Delete(kept);
        cJSON_Delete(chain);
        return NULL;
      }
    }
    if (!cJSON_AddItemToObject(entry, "params", kept)) {
      cJSON_Delete(entry);
      cJSON_Delete(kept);
      cJSON_Delete(chain);
      return NULL;
    }
    cJSON_AddItemToArray(steps, entry);
  }
  return chain;
}

/* Replaces the member name of object with value (moved in). */
static bool replace_member(cJSON *object, const char *name, cJSON *value) {
  cJSON_DeleteItemFromObjectCaseSensitive(object, name);
  if (value && cJSON_AddItemToObject(object, name, value)) {
    return true;
  }
  cJSON_Delete(value);
  return false;
}

/* Sets motif.harmony (the variant's own chords as text, or null) and motif.features (the labelled runs it kept). */
static bool set_variant_motif_members(cJSON *motif, const cJSON *variant) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(variant, "harmony");
  const cJSON *features = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "motive"), "features");
  cJSON *kept = cJSON_CreateArray();
  const cJSON *feature = NULL;
  cJSON_ArrayForEach(feature, features) {
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
    const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
    const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
    cJSON *entry = cJSON_IsString(label) && cJSON_IsNumber(first) && cJSON_IsNumber(last) ? cJSON_CreateObject() : NULL;
    if (entry && cJSON_AddStringToObject(entry, "label", label->valuestring) &&
        cJSON_AddNumberToObject(entry, "first", first->valuedouble) && cJSON_AddNumberToObject(entry, "last", last->valuedouble)) {
      cJSON_AddItemToArray(kept, entry);
    } else {
      cJSON_Delete(entry);
    }
  }
  return replace_member(motif, "harmony",
                        cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? cJSON_CreateString(harmony->valuestring)
                                                                                    : cJSON_CreateNull()) &&
         replace_member(motif, "features", kept);
}

/* Sets derived.harmony (the variant's own chords as text, or null), derived.base_harmony (its base harmony as text, or
   null), derived.tones and derived.warnings (from its analysis), derived.functions (the names it suits) and
   derived.remoteness (its distance from the basic motive, 0 to 1, or null) from a candidate. */
static bool apply_variant_extras(cJSON *derived, const cJSON *variant) {
  const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(variant, "harmony");
  const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(variant, "remoteness");
  cJSON *functions = cJSON_CreateArray();
  const cJSON *name = NULL;
  cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(variant, "functions")) {
    if (cJSON_IsString(name) && function_name_ok(name->valuestring)) {
      cJSON_AddItemToArray(functions, cJSON_CreateString(name->valuestring));
    }
  }
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(variant, "motive"), "analysis");
  const cJSON *base = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(analysis, "base_harmony"), "text");
  const cJSON *tones = cJSON_GetObjectItemCaseSensitive(analysis, "tones");
  const cJSON *warnings = cJSON_GetObjectItemCaseSensitive(analysis, "warnings");
  return replace_member(derived, "harmony",
                        cJSON_IsString(harmony) && harmony->valuestring[0] != '\0' ? cJSON_CreateString(harmony->valuestring)
                                                                                    : cJSON_CreateNull()) &&
         replace_member(derived, "base_harmony",
                        cJSON_IsString(base) && base->valuestring[0] != '\0' ? cJSON_CreateString(base->valuestring)
                                                                              : cJSON_CreateNull()) &&
         replace_member(derived, "tones", cJSON_IsArray(tones) ? cJSON_Duplicate(tones, 1) : cJSON_CreateArray()) &&
         replace_member(derived, "warnings", cJSON_IsArray(warnings) ? cJSON_Duplicate(warnings, 1) : cJSON_CreateArray()) &&
         replace_member(derived, "functions", functions) &&
         replace_member(derived, "remoteness",
                        cJSON_IsNumber(remoteness) && remoteness->valuedouble >= 0 && remoteness->valuedouble <= 1
                            ? cJSON_CreateNumber(remoteness->valuedouble)
                            : cJSON_CreateNull());
}

/* True when every step of the chain is the "exact" step the Composer names a motive that equals its source. */
static bool steps_are_exact(const cJSON *steps) {
  const cJSON *step = NULL;
  cJSON_ArrayForEach(step, steps) {
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
    if (!cJSON_IsString(op) || strcmp(op->valuestring, "exact") != 0) {
      return false;
    }
  }
  return true;
}

/* True when id is source_id or one of the items above it, following derived.source_item at most 32 hops. */
static bool item_is_in_lineage(const char *id, const char *source_id) {
  char current[128];
  snprintf(current, sizeof(current), "%s", source_id);
  for (int hops = 0; hops <= 32; hops++) {
    if (strcmp(current, id) == 0) {
      return true;
    }
    cJSON *body = NULL;
    if (workshop_material_item_get(current, NULL, &body) != URSATZ_OK) {
      return false;
    }
    const cJSON *next = cJSON_GetObjectItemCaseSensitive(derived_of(body), "source_item");
    bool more = cJSON_IsString(next) && strlen(next->valuestring) < sizeof(current);
    if (more) {
      snprintf(current, sizeof(current), "%s", next->valuestring);
    }
    cJSON_Delete(body);
    if (!more) {
      return false;
    }
  }
  return true;
}

/* The derivation record a recognition describes for the target: the source and its version, the chain, no seed, the
   target's own chords, and the dimensions kept and changed. NULL when the recognition lacks them or on no memory. */
static cJSON *derived_from_recognition(const char *source_id, int source_version, const cJSON *recognition,
                                       const cJSON *target_motif) {
  const cJSON *steps = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(recognition, "chain"), "steps");
  const cJSON *kept = cJSON_GetObjectItemCaseSensitive(recognition, "kept");
  const cJSON *changed = cJSON_GetObjectItemCaseSensitive(recognition, "changed");
  const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(recognition, "remoteness");
  if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(recognition, "found")) || !cJSON_IsArray(steps) ||
      !cJSON_IsArray(kept) || !cJSON_IsArray(changed)) {
    return NULL;
  }
  const char *harmony = workshop_motif_harmony(target_motif);
  cJSON *derived = cJSON_CreateObject();
  cJSON *functions = cJSON_CreateArray();
  const cJSON *name = NULL;
  cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(recognition, "functions")) {
    if (cJSON_IsString(name) && function_name_ok(name->valuestring)) {
      cJSON_AddItemToArray(functions, cJSON_CreateString(name->valuestring));
    }
  }
  bool ok = derived && functions && cJSON_AddStringToObject(derived, "source_item", source_id) &&
            cJSON_AddNumberToObject(derived, "source_version", source_version) &&
            cJSON_AddItemToObject(derived, "operations", cJSON_Duplicate(steps, 1)) &&
            cJSON_AddNullToObject(derived, "seed") &&
            cJSON_AddStringToObject(derived, "kind", steps_are_exact(steps) ? "exact" : "motive-form") &&
            cJSON_AddItemToObject(derived, "kept", cJSON_Duplicate(kept, 1)) &&
            cJSON_AddItemToObject(derived, "changed", cJSON_Duplicate(changed, 1)) &&
            (harmony ? cJSON_AddStringToObject(derived, "harmony", harmony) : cJSON_AddNullToObject(derived, "harmony")) &&
            cJSON_AddItemToObject(derived, "functions", functions) &&
            (cJSON_IsNumber(remoteness) ? cJSON_AddNumberToObject(derived, "remoteness", remoteness->valuedouble)
                                        : cJSON_AddNullToObject(derived, "remoteness")) &&
            cJSON_AddStringToObject(derived, "flag", "unflagged");
  if (!ok) {
    if (derived && !cJSON_GetObjectItemCaseSensitive(derived, "functions")) {
      cJSON_Delete(functions);
    }
    cJSON_Delete(derived);
    return NULL;
  }
  return derived;
}
/* A new version of the item (the target) whose derived record says it is a form of the source, from a recognition the recognize
   route gave: body {source: {item, version}, recognition}. CONFLICT when the item is the source or above it in the source's
   lineage, since the tree would then loop. */
UrsatzStatus workshop_variants_link_form(const char *id, const cJSON *body, cJSON **item_out, UrsatzError *error) {
  *item_out = NULL;
  const cJSON *reference = cJSON_GetObjectItemCaseSensitive(body, "source");
  const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(reference, "item");
  const cJSON *source_version = cJSON_GetObjectItemCaseSensitive(reference, "version");
  const cJSON *recognition = cJSON_GetObjectItemCaseSensitive(body, "recognition");
  if (!cJSON_IsString(source_item) || !cJSON_IsNumber(source_version) || source_version->valuedouble < 1 ||
      source_version->valuedouble != (int)source_version->valuedouble || !cJSON_IsObject(recognition) ||
      !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(recognition, "found"))) {
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "link-form needs a source {item, version} and a recognition that found a chain");
  }
  cJSON *current = NULL;
  UrsatzStatus result = workshop_material_item_get(id, NULL, &current);
  cJSON *source = NULL;
  if (result == URSATZ_OK) {
    result = workshop_material_item_get_version(source_item->valuestring, source_version->valueint, &source);
  }
  if (result != URSATZ_OK) {
    cJSON_Delete(current);
    return store_failure(result, error);
  }
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(current, "motif");
  if (!cJSON_IsObject(motif) || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(source, "motif"))) {
    cJSON_Delete(current);
    cJSON_Delete(source);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "only a motif can be linked as a form of a motif");
  }
  cJSON_Delete(source);
  if (item_is_in_lineage(id, source_item->valuestring)) {
    cJSON_Delete(current);
    return ursatz_error_set(error, URSATZ_ERR_CONFLICT, NULL, "a motif cannot be a form of itself or of one of its own forms");
  }
  cJSON *derived = derived_from_recognition(source_item->valuestring, source_version->valueint, recognition, motif);
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(recognition, "derivation");
  char note[501];
  bounded_note(note, sizeof(note), "linked: ", cJSON_IsString(derivation) ? derivation->valuestring : "form of another motif");
  if (!derived) {
    cJSON_Delete(current);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "the recognition lacks a chain, kept and changed");
  }
  cJSON *item = NULL;
  /* A linked motif already exists on its own, so it is a standalone motif (R-192), not a variant of the source. */
  bool linked = cJSON_AddTrueToObject(derived, "standalone") != NULL;
  result = linked && replace_member(motif, "derived", derived) ? workshop_material_item_add_version(id, current, note, &item)
                                                              : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
  if (!linked) {
    cJSON_Delete(derived);
  }
  cJSON_Delete(current);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  *item_out = item;
  return URSATZ_OK;
}

/* Reruns the stored chain of the derived motif id on the source's stored version with a new seed and stores the result
   as a new version (the reroll job). */
static void reroll_run(const char *id, const UrsatzJob *composer_job, WorkshopJobResult *out) {
  cJSON *current = NULL;
  UrsatzStatus result = workshop_material_item_get(id, NULL, &current);
  if (result != URSATZ_OK) {
    {
    job_material_failure(out, result);
    return;
  }
  }
  cJSON *motif = cJSON_GetObjectItemCaseSensitive(current, "motif");
  cJSON *derived = derived_of(current);
  const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
  const cJSON *source_version = cJSON_GetObjectItemCaseSensitive(derived, "source_version");
  const cJSON *stored_seed = cJSON_GetObjectItemCaseSensitive(derived, "seed");
  const cJSON *form_item = cJSON_GetObjectItemCaseSensitive(motif, "form");
  if (!derived || !cJSON_IsString(source_item) || !cJSON_IsNumber(source_version) || !cJSON_IsString(form_item)) {
    cJSON_Delete(current);
    job_failure(out, 400, "only a derived motif can be rerolled");
    return;
  }
  const char *form = form_item->valuestring;
  cJSON *source = NULL;
  result = workshop_material_item_get_version(source_item->valuestring, source_version->valueint, &source);
  if (result != URSATZ_OK) {
    cJSON_Delete(current);
    job_failure(out, 404, "the source motif or its version is gone");
    return;
  }
  cJSON *chain = chain_for_reroll(cJSON_GetObjectItemCaseSensitive(derived, "operations"));
  cJSON *basic_body = workshop_variants_lineage_root(cJSON_GetObjectItemCaseSensitive(source, "motif"));
  const cJSON *shown = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
  int status = 500;
  UrsatzError failure = {0};
  workshop_error_text(&failure, "out of memory");
  cJSON *document = NULL;
  const cJSON *variant = NULL;
  double base = cJSON_IsNumber(stored_seed) ? stored_seed->valuedouble : 0;
  double seed = 0;
  for (int attempt = 0; chain && attempt < 16; attempt++) {
    seed = fmod(base + 1000003.0 + attempt, max_variant_seed() + 1);
    cJSON_Delete(document);
    document = workshop_motif_run_variants(cJSON_GetObjectItemCaseSensitive(source, "motif"), basic_body, NULL, NULL, false, chain, 1, seed, composer_job,
                            &status, &failure);
    if (!document) {
      break;
    }
    variant = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "variants"), 0);
    const char *degrees = variant_ok(variant) ? variant_text(variant, form, true) : NULL;
    if (!degrees) {
      status = 500;
      workshop_error_text(&failure, "composer returned no variant");
      cJSON_Delete(document);
      document = NULL;
      break;
    }
    if (!cJSON_IsString(shown) || strcmp(degrees, shown->valuestring) != 0) {
      break;
    }
    variant = NULL;
  }
  cJSON_Delete(source);
  cJSON_Delete(chain);
  cJSON_Delete(basic_body);
  if (!document || !variant) {
    bool same = document != NULL;
    cJSON_Delete(document);
    cJSON_Delete(current);
    workshop_job_result_failure_at(out, same ? 409 : status, same ? "rerolling gives the same motif" : failure.message,
                                     same ? NULL : failure.path);
    return;
  }
  const cJSON *chain_out = cJSON_GetObjectItemCaseSensitive(variant, "chain");
  const cJSON *kind = cJSON_GetObjectItemCaseSensitive(variant, "kind");
  const cJSON *derivation = cJSON_GetObjectItemCaseSensitive(variant, "derivation");
  const char *notes = variant_text(variant, form, false);
  bool ok = notes && cJSON_IsString(kind) && cJSON_IsString(derivation) &&
            replace_member(motif, "notes", cJSON_CreateString(notes)) &&
            replace_member(motif, "degrees", cJSON_CreateString(variant_text(variant, form, true))) &&
            replace_member(motif, "analysis", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "motive"), 1)) &&
            replace_member(derived, "operations", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(chain_out, "steps"), 1)) &&
            replace_member(derived, "seed", cJSON_CreateNumber(seed)) &&
            replace_member(derived, "kind", cJSON_CreateString(kind->valuestring)) &&
            replace_member(derived, "kept", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "kept"), 1)) &&
            replace_member(derived, "changed", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(variant, "changed"), 1)) &&
            apply_variant_extras(derived, variant) && set_variant_motif_members(motif, variant) &&
            common_json_set_string(derived, "flag", "unflagged");
  char note[501];
  if (ok) {
    bounded_note(note, sizeof(note), "rerolled: ", derivation->valuestring);
  }
  cJSON *item = NULL;
  result = ok ? workshop_material_item_add_version(id, current, note, &item) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
  cJSON_Delete(document);
  cJSON_Delete(current);
  if (result != URSATZ_OK) {
    {
    job_material_failure(out, result);
    return;
  }
  }
  job_ok_with(out, "item", item);
}
/* The variants and each-op runs of a job: the answer is the Composer's document plus "ok" and "seed". */
static void variants_run(const WorkshopVariantsJob *input, const UrsatzJob *composer_job, WorkshopJobResult *out) {
  bool each_op = input->op == WORKSHOP_VARIANTS_EACH_OP;
  const cJSON *chain = cJSON_GetObjectItemCaseSensitive(input->body, "chain");
  const cJSON *functions = cJSON_GetObjectItemCaseSensitive(input->body, "functions");
  const cJSON *distance_item = cJSON_GetObjectItemCaseSensitive(input->body, "distance");
  chain = cJSON_IsNull(chain) ? NULL : chain;
  functions = cJSON_IsNull(functions) ? NULL : functions;
  const char *distance = cJSON_IsString(distance_item) ? distance_item->valuestring : NULL;
  cJSON *item_body = NULL;
  UrsatzStatus result = workshop_material_item_get(input->id, NULL, &item_body);
  if (result != URSATZ_OK) {
    job_material_failure(out, result);
    return;
  }
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(item_body, "motif");
  int status = 400;
  UrsatzError failure = {0};
  workshop_error_text(&failure, "only a motif has variants");
  cJSON *basic_body = cJSON_IsObject(motif) ? workshop_variants_lineage_root(motif) : NULL;
  cJSON *document = cJSON_IsObject(motif)
                        ? (each_op ? workshop_motif_run_variants(motif, basic_body, NULL, NULL, true, NULL, 0, input->seed, composer_job, &status,
                                                  &failure)
                                   : workshop_motif_run_variants(motif, basic_body, functions, distance, false, chain, input->count, input->seed,
                                                  composer_job, &status, &failure))
                        : NULL;
  cJSON_Delete(basic_body);
  cJSON_Delete(item_body);
  if (!document) {
    workshop_job_result_failure_at(out, status, failure.message, failure.path);
    return;
  }
  const cJSON *run_seed = cJSON_GetObjectItemCaseSensitive(document, "seed");
  if (!cJSON_AddBoolToObject(document, "ok", 1) ||
      (!run_seed && !cJSON_AddNumberToObject(document, "seed", input->seed >= 0 ? input->seed : 0))) {
    cJSON_Delete(document);
    job_failure(out, 500, "out of memory");
    return;
  }
  workshop_job_result_json(out, 200, document);
}

UrsatzStatus workshop_variants_job_run(void *input, const UrsatzJob *composer_job, WorkshopJobResult *out) {
  const WorkshopVariantsJob *job = input;
  if (job->op == WORKSHOP_VARIANTS_REROLL) {
    reroll_run(job->id, composer_job, out);
  } else {
    variants_run(job, composer_job, out);
  }
  return out->http_status >= 200 && out->http_status < 300 ? URSATZ_OK : URSATZ_ERR_INTERNAL;
}
static const int kMaxTreeDepth = 32;

/* A tree node {id, name, current_version, standalone, derived, children: []} (derived is a copy, or null; standalone
   is true for a promoted motif) with every entry of index
   whose derived.source_item is id below it. ancestors lists the ids above, so a loop in the stored records ends. */
static cJSON *variant_node(const cJSON *index, const char *id, const char *name, int version, const cJSON *derived,
                           const char **ancestors, int depth) {
  cJSON *node = cJSON_CreateObject();
  cJSON *children = cJSON_CreateArray();
  cJSON *copy = derived ? cJSON_Duplicate(derived, 1) : cJSON_CreateNull();
  if (!node || !children || !copy || !cJSON_AddStringToObject(node, "id", id) ||
      !cJSON_AddStringToObject(node, "name", name) || !cJSON_AddNumberToObject(node, "current_version", version) ||
      !cJSON_AddBoolToObject(node, "standalone",
                             cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"))) ||
      !cJSON_AddItemToObject(node, "derived", copy)) {
    if (!node || !cJSON_GetObjectItemCaseSensitive(node, "derived")) {
      cJSON_Delete(copy);
    }
    cJSON_Delete(children);
    cJSON_Delete(node);
    return NULL;
  }
  if (!cJSON_AddItemToObject(node, "children", children)) {
    cJSON_Delete(children);
    cJSON_Delete(node);
    return NULL;
  }
  if (depth >= kMaxTreeDepth) {
    return node;
  }
  ancestors[depth] = id;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, index) {
    const cJSON *entry_id = cJSON_GetObjectItemCaseSensitive(entry, "id");
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "derived"), "source_item");
    if (!cJSON_IsString(entry_id) || !cJSON_IsString(source) || strcmp(source->valuestring, id) != 0) {
      continue;
    }
    bool seen = false;
    for (int i = 0; i <= depth; i++) {
      seen = seen || strcmp(ancestors[i], entry_id->valuestring) == 0;
    }
    if (seen) {
      continue;
    }
    const cJSON *entry_name = cJSON_GetObjectItemCaseSensitive(entry, "name");
    const cJSON *entry_version = cJSON_GetObjectItemCaseSensitive(entry, "current_version");
    cJSON *child = variant_node(index, entry_id->valuestring, cJSON_IsString(entry_name) ? entry_name->valuestring : "",
                                cJSON_IsNumber(entry_version) ? entry_version->valueint : 0,
                                cJSON_GetObjectItemCaseSensitive(entry, "derived"), ancestors, depth + 1);
    if (!child) {
      cJSON_Delete(node);
      return NULL;
    }
    cJSON_AddItemToArray(children, child);
  }
  return node;
}
/* The item and, recursively, every motif derived from it, in *tree. */
UrsatzStatus workshop_variants_tree(const char *id, cJSON **tree_out, UrsatzError *error) {
  *tree_out = NULL;
  cJSON *meta = NULL;
  cJSON *body = NULL;
  UrsatzStatus result = workshop_material_item_get(id, &meta, &body);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  cJSON *index = NULL;
  if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "motif"))) {
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "only a motif has variants");
  }
  result = workshop_material_derived_motifs(&index);
  const char *name = workshop_param_string(meta, "name", &(bool){false});
  const cJSON *version = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
  const char *ancestors[33];
  cJSON *tree = result == URSATZ_OK
                    ? variant_node(index, id, name ? name : "", cJSON_IsNumber(version) ? version->valueint : 0,
                                   derived_of(body), ancestors, 0)
                    : NULL;
  cJSON_Delete(meta);
  cJSON_Delete(body);
  cJSON_Delete(index);
  if (result != URSATZ_OK) {
    return store_failure(result, error);
  }
  if (!tree) {
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
  }
  *tree_out = tree;
  return URSATZ_OK;
}
