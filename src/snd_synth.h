// Soinnka: síntesis de los efectos de sonido (C puro, sin nada de PSP, para poder probarla también en el PC).
#ifndef SND_SYNTH_H
#define SND_SYNTH_H

#define SND_RATE 44100

// Identificadores de efecto. Los que están desde SFX_LOOP_FIRST suenan en bucle (ambientes).
enum {
    SFX_SHOT_PISTOL, SFX_SHOT_SHOTGUN, SFX_SHOT_RIFLE, SFX_SHOT_SIL,
    SFX_RELOAD, SFX_JAM, SFX_CASING, SFX_WALL,
    SFX_ZBITE, SFX_GROWL, SFX_AXE, SFX_BOSS_ROAR,
    SFX_SWING, SFX_MEL_HIT, SFX_SPIT, SFX_SPLAT,
    SFX_HURT, SFX_HEART, SFX_CRATE, SFX_PICKUP,
    SFX_EAT, SFX_DRINK, SFX_BAND,
    SFX_MENU_MOVE, SFX_MENU_OK, SFX_TRADE, SFX_JOB_DONE, SFX_SAVE,
    SFX_STEP_OUT, SFX_STEP_IN, SFX_SIREN,
    SFX_BOOM, SFX_CLINK, SFX_BEEP, SFX_ZAP, SFX_WHOOSH, SFX_TRAP,
    SFX_BARK, SFX_DGROWL, SFX_WHINE, SFX_HORN,
    SFX_FIRE, SFX_GEIGER, SFX_WIND,
    SFX_DRONE,   // fondo grave de la introducción (bucle)
    SFX_COUNT
};
#define SFX_LOOP_FIRST SFX_FIRE

extern const char *const SFX_NAME[SFX_COUNT];
int snd_is_loop(int id);

// Genera el efecto `id` en mono, 16 bits, SND_RATE Hz. Devuelve la cantidad de muestras (0 si falla) y deja
// en *out un búfer reservado con malloc (lo libera quien lo pide).
int snd_synth(int id, short **out);

#endif
