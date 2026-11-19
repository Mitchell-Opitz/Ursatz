#ifndef WORKSHOP_ID_SERVICE_H
#define WORKSHOP_ID_SERVICE_H

/* Returns a new id "<prefix>-<16 lowercase hex>" for stores to use as an
   opaque key. The hex comes from /dev/urandom; if that cannot be opened it
   comes from a hash of time, clock, process id and a counter. The caller
   frees the result. Returns NULL on allocation failure or a NULL prefix. */
char *workshop_new_id(const char *prefix);

#endif
