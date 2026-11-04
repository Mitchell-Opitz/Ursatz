#ifndef MUSIC_PITCH_MIDI_PITCH_H
#define MUSIC_PITCH_MIDI_PITCH_H

#include <stdbool.h>

#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"

/*
 * The kernel's one reading of a MIDI note number as a pitch class, a written
 * pitch and a key signature. Every caller that needs one of these calls it here
 * instead of keeping its own copy.
 */

/* 0 to 11 for any int, negative numbers included (-1 is 11). */
int pitch_class_of_midi(int midi);

/*
 * The written pitch of a MIDI note in a key given as its number of sharps (positive) or flats
 * (negative): sharps for key_fifths >= 0, flats below. Octave 4 C is 60, so the octave is
 * floor(midi / 12) - 1, also for negative numbers. Black keys are C#/Db, D#/Eb, F#/Gb, G#/Ab, A#/Bb.
 * Returns false (*out untouched) for a NULL out or an allocation failure. The caller destroys *out.
 */
bool pitch_spelling_of_midi(int midi, int key_fifths, PitchSpelling **out);

/*
 * The conventional key signature with the given number of sharps (positive) or flats (negative):
 * sharps in the order F C G D A E B, flats in the reverse. fifths 0 is the empty signature.
 * Returns false (*out untouched) for fifths outside -7 to 7, a NULL out, or an allocation failure.
 * The caller destroys *out.
 */
bool key_signature_from_fifths(int fifths, KeySignature **out);

/*
 * The name of the key tonic with that pitch class (0 to 11, any int is reduced): C, Db, D, Eb, E, F,
 * F#, G, Ab, A, Bb, B. A static string; never NULL.
 */
const char *key_tonic_name(int tonic_pitch_class);

#endif
