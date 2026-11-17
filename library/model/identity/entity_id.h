#ifndef MUSIC_IDENTITY_ENTITY_ID_H
#define MUSIC_IDENTITY_ENTITY_ID_H

#include <stdbool.h>

typedef struct EntityID EntityID;

typedef enum {
    ENTITY_ID_OK = 0,
    ENTITY_ID_ERR_EMPTY,
    ENTITY_ID_ERR_ALLOC
} EntityIDResult;

EntityIDResult entity_id_create(const char *value, EntityID **out);
void entity_id_destroy(EntityID *id);
bool entity_id_equals(const EntityID *a, const EntityID *b);
const char *entity_id_value(const EntityID *id);

#endif