#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_MODES_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_MODES_H

/*
 * The steps of the four tonality modes (StandardTonalityMode order: major, natural minor, harmonic minor, melodic minor), generated
 * from their omt.material.* knowledge entries when the library is built (cmake/embed_knowledge.py). The one route to a mode's
 * steps besides the snapshot, until P58 passes the snapshot to where a key is made (R-579).
 */
extern const int knowledge_tonality_mode_steps[4][7];

#endif
