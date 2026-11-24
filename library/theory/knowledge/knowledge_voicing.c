#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K3 voicing: content.members and content.fits. */

#define VOICING_SEMITONE_MAX 35

KnowledgeStatus knowledge_voicing_read(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    VoicingEntry *voicing = &slot->voicing;
    const cJSON *content = knowledge_member(entry, "content");
    const cJSON *members = cJSON_IsObject(content) ? knowledge_member(content, "members") : NULL;
    if (members == NULL) {
        knowledge_fail(message, message_size, path, id, "content.members", "missing");
        return KNOWLEDGE_ERR_FIELD;
    }
    int count = cJSON_IsArray(members) ? cJSON_GetArraySize(members) : -1;
    if (count < 2 || count > CHORD_TYPE_MEMBER_MAX) {
        knowledge_fail(message, message_size, path, id, "content.members", "not 2 to 8 members");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, members) {
        const char *role_text = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "role")) : NULL;
        const cJSON *semitones = cJSON_IsObject(item) ? knowledge_member(item, "semitones") : NULL;
        ChordMemberRole role = CHORD_MEMBER_ROOT;
        if (role_text == NULL || !knowledge_chord_role(role_text, &role)) {
            knowledge_fail(message, message_size, path, id, "content.members", "a member has no known role");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (!knowledge_in_range(semitones, 0, VOICING_SEMITONE_MAX)) {
            knowledge_fail(message, message_size, path, id, "content.members", "a member's semitones are not an integer from 0 to 35");
            return KNOWLEDGE_ERR_FIELD;
        }
        size_t at = voicing->member_count;
        if (at == 0 && (role != CHORD_MEMBER_ROOT || semitones->valueint != 0)) {
            knowledge_fail(message, message_size, path, id, "content.members", "the first member is not the root at 0");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (at > 0 && semitones->valueint <= voicing->members[at - 1].semitones) {
            knowledge_fail(message, message_size, path, id, "content.members", "the members do not rise: a member is not above the one before");
            return KNOWLEDGE_ERR_FIELD;
        }
        voicing->members[at].role = role;
        voicing->members[at].semitones = semitones->valueint;
        voicing->member_count++;
    }
    const cJSON *fits = knowledge_member(content, "fits");
    int fit_count = cJSON_IsArray(fits) ? cJSON_GetArraySize(fits) : -1;
    if (fit_count < 1 || fit_count > CHORD_TYPE_MEMBER_MAX) {
        knowledge_fail(message, message_size, path, id, "content.fits", fits == NULL ? "missing" : "not 1 to 8 chord type ids");
        return KNOWLEDGE_ERR_FIELD;
    }
    cJSON_ArrayForEach(item, fits) {
        const char *target = knowledge_string_of(item);
        if (target == NULL) {
            knowledge_fail(message, message_size, path, id, "content.fits", "a fit is not a chord type id");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->fit_targets[voicing->fit_count] = knowledge_own(slot, target);
        if (slot->fit_targets[voicing->fit_count] == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        voicing->fit_count++;
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_voicing_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size) {
    if (!slot->has_voicing) {
        return KNOWLEDGE_OK;
    }
    for (size_t i = 0; i < slot->voicing.fit_count; i++) {
        const Slot *found = knowledge_find_slot(snapshot, slot->fit_targets[i]);
        if (found == NULL || !found->has_chord_type) {
            knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.fits", "a fit is not a chord type entry");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->voicing.fits[i] = &found->chord_type;
    }
    return KNOWLEDGE_OK;
}
