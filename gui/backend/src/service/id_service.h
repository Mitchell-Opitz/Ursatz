#ifndef URSATZ_GUI_ID_SERVICE_H
#define URSATZ_GUI_ID_SERVICE_H

/* Returns a new id "<prefix>-<16 lowercase hex>" for stores to use as an
   opaque key. The hex comes from /dev/urandom; if that cannot be opened it
   comes from a hash of time, clock, process id and a counter. The caller
   frees the result. Returns NULL on allocation failure or a NULL prefix. */
char *ursatz_gui_new_id(const char *prefix);

#endif
