// Soinnka: sonido. Los efectos se sintetizan al arrancar (snd_synth.c) y un hilo de audio los mezcla y los saca.
#ifndef SND_H
#define SND_H

#include "snd_synth.h"

void snd_init(void);                                    // sintetiza todo y arranca el hilo de audio
void snd_play(int id, float vol, float pan);            // efecto suelto: volumen 0..1, paneo -1 (izquierda) a 1 (derecha)
void snd_loop(int id, float vol);                       // ambiente en bucle: vol 0 lo apaga (el volumen sube y baja suave)
void snd_set_master(int v);                             // volumen general de 0 a 10 (el de las opciones)
void snd_music_open(const char *path);                  // [armas2] música de fondo (22.050 Hz mono 16 bits crudo), leída a trozos del disco; sin archivo no suena
void snd_music_level(float v);                          // [armas2] nivel de la música según la escena (0 a 1)
void snd_music_master(int v);                           // [armas2] volumen de la música de 0 a 10 (el de las opciones)
int snd_music_state(void);                              // [armas2] 0 sin archivo, 1 abierto, 2 ya sonó (leyó trozos)                             // volumen general de 0 a 10 (el de las opciones)

#endif
