#ifndef MUSIC_CONTAINERS_CONTAINER_H
#define MUSIC_CONTAINERS_CONTAINER_H

/*
 * Container: base category for objects that group/organize other
 * objects. Concrete grouping types built on this category (this pass:
 * Voice; later passes: Layer, Staff, Part, Measure, Phrase, Section,
 * Work) each define their own fields and constructor -- Container
 * itself defines none.
 *
 * A pure grouping base type: it makes no assumption about what a
 * concrete container groups, how membership is represented, or how
 * many members exist. Voice (this pass) has no membership field at
 * all -- see voice.h. The membership mechanism for containers that do
 * need one is deferred to a later Container/Structure spec (not yet
 * available); nothing in this module should be extended to anticipate
 * it.
 *
 * No dependency beyond what concrete subtypes need on their own:
 * Container itself does not import identity/, core/, time/, pitch/,
 * or events/, and has no create/destroy or accessor functions -- there
 * is no state here to construct or tear down.
 */

typedef struct Container Container;

#endif