// Soinnka: salida de audio en PSP. Un hilo mezcla hasta 8 voces sueltas (más los ambientes en bucle) a 44100 Hz,
// estéreo de 16 bits, y las entrega por un canal reservado alternando DOS búferes de salida.
#include <pspkernel.h>
#include <pspaudio.h>
#include <stdlib.h>
#include <string.h>
#include <pspiofilemgr.h>
#include "snd.h"

#define NVOICE 8
#define BLOCK 512            // muestras por bloque (11,6 ms); múltiplo de 64 como pide el hardware
#define SOFT_KNEE 24000      // desde aquí el limitador empieza a comprimir
#define SOFT_RANGE 8767      // lo que queda hasta 32767
#define RETRIG 1500          // el mismo efecto no se repite antes de tantas muestras (34 ms): evita sumar el mismo golpe

typedef struct {
    volatile int on;
    int id, pos;
    float gl, gr;            // ganancia de cada oído
} Voice;

static short *g_smp[SFX_COUNT];
static int g_len[SFX_COUNT];
static Voice g_v[NVOICE];
static float g_loop_tgt[SFX_COUNT], g_loop_cur[SFX_COUNT];
static int g_loop_pos[SFX_COUNT];
static unsigned g_clock, g_last[SFX_COUNT];
static volatile float g_master = 1.0f;
static int g_ready, g_ch = -1;
static short g_out[2][BLOCK * 2] __attribute__((aligned(64)));

// [armas2] Música: un hilo de baja prioridad lee el archivo a trozos a un anillo; el hilo de audio la mezcla (22.050 Hz -> 44.100 Hz)
#define MUS_RING 32768                   // muestras (64 KB)
#define MUS_CHUNK 4096                   // muestras por lectura
static short g_mring[MUS_RING] __attribute__((aligned(64)));
static short g_mtmp[MUS_CHUNK] __attribute__((aligned(64)));
static volatile unsigned g_mw, g_mr;     // total de muestras escritas y leídas
static volatile float g_mus_tgt, g_mus_m = 0.7f * 0.7f;
static float g_mus_cur;
static volatile int g_mus_state;
static SceUID g_mfd = -1;

static int music_thread(SceSize args, void *argp)
{
    while (1) {
        if (g_mw - g_mr + MUS_CHUNK <= MUS_RING) {
            int n = sceIoRead(g_mfd, g_mtmp, MUS_CHUNK * 2);
            if (n < MUS_CHUNK * 2) {   // fin del archivo: vuelve al principio (bucle)
                sceIoLseek(g_mfd, 0, PSP_SEEK_SET);
                if (n < 2) { sceKernelDelayThread(50000); continue; }
                n &= ~1;
            }
            int cnt = n / 2;
            for (int i = 0; i < cnt; i++) g_mring[(g_mw + i) & (MUS_RING - 1)] = g_mtmp[i];
            g_mw += cnt;
            g_mus_state = 2;
            sceKernelDelayThread(2000);
        } else sceKernelDelayThread(30000);
    }
    return 0;
}

void snd_music_open(const char *path)
{
    if (g_mfd >= 0) return;
    g_mfd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (g_mfd < 0) return;   // sin archivo, el juego anda igual sin música
    g_mus_state = 1;
    SceUID th = sceKernelCreateThread("pepel_mus", music_thread, 0x28, 0x4000, THREAD_ATTR_USER, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    else { sceIoClose(g_mfd); g_mfd = -1; g_mus_state = 0; }
}
void snd_music_level(float v) { g_mus_tgt = v < 0 ? 0 : v > 1 ? 1 : v; }
void snd_music_master(int v)
{
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    g_mus_m = (v / 10.0f) * (v / 10.0f);
}
int snd_music_state(void) { return g_mus_state; }

void snd_set_master(int v)
{
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    g_master = (v / 10.0f) * (v / 10.0f);   // curva cuadrática: más fino a volumen bajo
}

void snd_play(int id, float vol, float pan)
{
    if (!g_ready || id < 0 || id >= SFX_COUNT || !g_smp[id] || snd_is_loop(id)) return;
    if (vol <= 0.01f) return;
    if (vol > 1) vol = 1;
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    if ((unsigned)(g_clock - g_last[id]) < RETRIG && g_last[id]) return;
    g_last[id] = g_clock ? g_clock : 1;
    int k = -1, best = -1;
    for (int i = 0; i < NVOICE; i++) {
        if (!g_v[i].on) { k = i; break; }
        int prog = g_v[i].pos * 100 / (g_len[g_v[i].id] ? g_len[g_v[i].id] : 1);   // si no hay libre, se pisa la más avanzada
        if (prog > best) { best = prog; k = i; }
    }
    Voice *v = &g_v[k];
    v->on = 0;
    v->id = id;
    v->pos = 0;
    v->gl = vol * (pan > 0 ? 1 - pan : 1);
    v->gr = vol * (pan < 0 ? 1 + pan : 1);
    v->on = 1;
}

void snd_loop(int id, float vol)
{
    if (!g_ready || id < SFX_LOOP_FIRST || id >= SFX_COUNT) return;
    g_loop_tgt[id] = vol < 0 ? 0 : vol > 1 ? 1 : vol;
}

// Mezcla un bloque en `o` (estéreo intercalado)
static void mix_block(short *o)
{
    static int acc[BLOCK * 2];
    memset(acc, 0, sizeof(acc));
    float m = g_master;
    for (int i = 0; i < NVOICE; i++) {
        Voice *v = &g_v[i];
        if (!v->on) continue;
        const short *s = g_smp[v->id];
        int len = g_len[v->id], n = len - v->pos < BLOCK ? len - v->pos : BLOCK;
        int gl = (int)(v->gl * m * 256), gr = (int)(v->gr * m * 256);
        for (int j = 0; j < n; j++) {
            int x = s[v->pos + j];
            acc[j * 2] += x * gl >> 8;
            acc[j * 2 + 1] += x * gr >> 8;
        }
        v->pos += n;
        if (v->pos >= len) v->on = 0;
    }
    for (int id = SFX_LOOP_FIRST; id < SFX_COUNT; id++) {
        float tgt = g_loop_tgt[id], cur = g_loop_cur[id];
        if (cur <= 0.0005f && tgt <= 0.0005f) { g_loop_cur[id] = 0; continue; }
        const short *s = g_smp[id];
        int len = g_len[id], pos = g_loop_pos[id];
        float step = (tgt - cur) / BLOCK;
        for (int j = 0; j < BLOCK; j++) {
            cur += step;
            int x = (int)(s[pos] * cur * m);
            acc[j * 2] += x;
            acc[j * 2 + 1] += x;
            if (++pos >= len) pos = 0;
        }
        g_loop_cur[id] = tgt;
        g_loop_pos[id] = pos;
    }
    if (g_mfd >= 0) {   // [armas2] la música, bajo los efectos
        float tgt = g_mus_tgt * g_mus_m * 0.8f, cur = g_mus_cur;
        unsigned r = g_mr;
        if (g_mw - r >= BLOCK / 2 + 1 && (tgt > 0.0005f || cur > 0.0005f)) {
            float step = (tgt - cur) / BLOCK;
            for (int j = 0; j < BLOCK; j++) {
                cur += step;
                int a0 = g_mring[(r + j / 2) & (MUS_RING - 1)], a1 = g_mring[(r + j / 2 + 1) & (MUS_RING - 1)];
                int x = (int)(((j & 1) ? (a0 + a1) / 2 : a0) * cur * 1.0f);
                acc[j * 2] += x;
                acc[j * 2 + 1] += x;
            }
            g_mr = r + BLOCK / 2;
            g_mus_cur = tgt;
        } else if (g_mw - r >= BLOCK / 2 + 1) g_mr = r + BLOCK / 2;   // en silencio sigue avanzando
    }
    for (int j = 0; j < BLOCK * 2; j++) {
        // limitador suave: hasta 24000 pasa igual; más arriba se comprime hacia 32767 sin recorte seco (varias voces fuertes a la vez)
        int x = acc[j], a = x < 0 ? -x : x;
        if (a > SOFT_KNEE) {
            int u = a - SOFT_KNEE;
            a = SOFT_KNEE + (SOFT_RANGE * u) / (SOFT_RANGE + u);
            x = x < 0 ? -a : a;
        }
        o[j] = (short)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
    }
    g_clock += BLOCK;
}

static int audio_thread(SceSize args, void *argp)
{
    int cur = 0;
    while (1) {
        mix_block(g_out[cur]);
        sceAudioOutputBlocking(g_ch, PSP_AUDIO_VOLUME_MAX, g_out[cur]);   // devuelve cuando el canal acepta el siguiente bloque
        cur ^= 1;                                                          // el otro búfer se llena mientras éste suena
    }
    return 0;
}

void snd_init(void)
{
    if (g_ready) return;
    for (int id = 0; id < SFX_COUNT; id++) g_len[id] = snd_synth(id, &g_smp[id]);
    g_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK, PSP_AUDIO_FORMAT_STEREO);
    if (g_ch < 0) return;   // sin canal de audio el juego sigue mudo
    g_ready = 1;
    SceUID th = sceKernelCreateThread("pepel_snd", audio_thread, 0x18, 0x4000, THREAD_ATTR_USER, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    else g_ready = 0;
}
