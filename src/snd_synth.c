// Soinnka: síntesis de efectos. Ruido filtrado, tonos con caída y capas sumadas; ambiente seco y lúgubre.
#include "snd_synth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI2 6.2831853f

const char *const SFX_NAME[SFX_COUNT] = {
    "disparo_pistola", "disparo_escopeta", "disparo_fusil", "disparo_silenciado",
    "recarga", "arma_trabada", "casquillo", "impacto_muro",
    "mordida_zombi", "gruñido_zombi", "hacha", "rugido_jefe",
    "golpe_al_aire", "golpe_cuerpo_a_cuerpo", "escupitajo_acido", "charco_acido",
    "jugador_herido", "latido", "abrir_cajon", "recoger",
    "comer", "beber", "vendarse",
    "menu_mover", "menu_confirmar", "comprar_vender", "trabajo_cumplido", "guardado",
    "paso_exterior", "paso_interior", "sirena",
    "explosion", "tuerca", "pitido_detector", "chispazo", "llamarada", "cepo_mina",
    "ladrido", "gruñido_perro", "quejido_perro", "corneta",
    "fogata", "geiger", "viento",
    "fondo_introduccion",
};

int snd_is_loop(int id) { return id >= SFX_LOOP_FIRST && id < SFX_COUNT; }

// generador pseudoaleatorio propio (xorshift): el resultado es siempre el mismo
static unsigned g_rs = 2463534242u;
static float rnd(void)   // -1 a 1
{
    g_rs ^= g_rs << 13; g_rs ^= g_rs >> 17; g_rs ^= g_rs << 5;
    return (g_rs >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
static float rnd01(void) { return rnd() * 0.5f + 0.5f; }

// ---- capas -------------------------------------------------------------------------------------------------
// Ruido pasado por un pasabajos que barre de lp0 a lp1 Hz (y un pasaaltos fijo en hp Hz, 0 = sin), con ataque `att`
// y caída exponencial `dec`, sumado desde t0 durante `dur` segundos.
static void noise_l(float *b, int n, float t0, float dur, float amp, float lp0, float lp1, float hp, float att, float dec)
{
    int i0 = (int)(t0 * SND_RATE), cnt = (int)(dur * SND_RATE);
    float lo = 0, hs = 0, ah = hp > 0 ? 1 - expf(-PI2 * hp / SND_RATE) : 0;
    for (int i = 0; i < cnt && i0 + i < n; i++) {
        float t = (float)i / SND_RATE, k = (float)i / cnt;
        float f = lp0 * powf(lp1 / lp0, k);
        float a = 1 - expf(-PI2 * f / SND_RATE);
        lo += (rnd() - lo) * a;
        float y = lo;
        if (hp > 0) { hs += (y - hs) * ah; y -= hs; }
        float env = expf(-t / dec) * (att > 0 && t < att ? t / att : 1);
        float fade = (cnt - i) < 64 ? (float)(cnt - i) / 64 : 1;   // sin clic al cortar
        b[i0 + i] += y * amp * env * fade * 2.2f;
    }
}

// Tono con caída de f0 a f1 (exponencial) y un poco de segundo armónico `harm`
static void tone_l(float *b, int n, float t0, float dur, float amp, float f0, float f1, float harm, float dec)
{
    int i0 = (int)(t0 * SND_RATE), cnt = (int)(dur * SND_RATE);
    float ph = 0;
    for (int i = 0; i < cnt && i0 + i < n; i++) {
        float t = (float)i / SND_RATE, k = (float)i / cnt;
        ph += PI2 * f0 * powf(f1 / f0, k) / SND_RATE;
        float env = expf(-t / dec) * (t < 0.002f ? t / 0.002f : 1);
        float fade = (cnt - i) < 64 ? (float)(cnt - i) / 64 : 1;
        b[i0 + i] += (sinf(ph) + harm * sinf(2 * ph + 0.7f)) * amp * env * fade;
    }
}

// Voz grave rasposa (gruñidos y rugidos): diente de sierra con vibrato, modulado en amplitud y filtrado
static void voice_l(float *b, int n, float t0, float dur, float amp, float f0, float f1, float vib, float am_hz, float nmix, float lp)
{
    int i0 = (int)(t0 * SND_RATE), cnt = (int)(dur * SND_RATE);
    float ph = 0, lo = 0, a = 1 - expf(-PI2 * lp / SND_RATE);
    for (int i = 0; i < cnt && i0 + i < n; i++) {
        float t = (float)i / SND_RATE, k = (float)i / cnt;
        float f = f0 + (f1 - f0) * k + vib * sinf(PI2 * 6.5f * t);
        ph += f / SND_RATE; ph -= floorf(ph);
        float saw = ph * 2 - 1;
        float am = 1 - 0.5f * (0.5f + 0.5f * sinf(PI2 * am_hz * t));
        lo += ((saw + nmix * rnd()) - lo) * a;
        float env = sinf(3.14159f * k); env = powf(env < 0 ? 0 : env, 0.6f);
        b[i0 + i] += lo * am * env * amp * 1.4f;
    }
}

// ---- cada efecto ---------------------------------------------------------------------------------------------
typedef struct { float dur, peak; int loop; } Spec;
static const Spec SPEC[SFX_COUNT] = {
    [SFX_SHOT_PISTOL] = {0.45f, 0.88f, 0}, [SFX_SHOT_SHOTGUN] = {0.85f, 0.95f, 0}, [SFX_SHOT_RIFLE] = {0.55f, 0.90f, 0},
    [SFX_SHOT_SIL] = {0.30f, 0.45f, 0},
    [SFX_RELOAD] = {1.05f, 0.55f, 0}, [SFX_JAM] = {0.20f, 0.55f, 0}, [SFX_CASING] = {0.60f, 0.22f, 0}, [SFX_WALL] = {0.30f, 0.50f, 0},
    [SFX_ZBITE] = {0.35f, 0.75f, 0}, [SFX_GROWL] = {0.95f, 0.65f, 0}, [SFX_AXE] = {0.55f, 0.55f, 0}, [SFX_BOSS_ROAR] = {1.40f, 0.95f, 0},
    [SFX_SWING] = {0.30f, 0.40f, 0}, [SFX_MEL_HIT] = {0.32f, 0.85f, 0}, [SFX_SPIT] = {0.40f, 0.50f, 0}, [SFX_SPLAT] = {0.35f, 0.45f, 0},
    [SFX_HURT] = {0.45f, 0.80f, 0}, [SFX_HEART] = {0.65f, 0.75f, 0}, [SFX_CRATE] = {0.65f, 0.55f, 0}, [SFX_PICKUP] = {0.30f, 0.40f, 0},
    [SFX_EAT] = {0.70f, 0.55f, 0}, [SFX_DRINK] = {0.95f, 0.45f, 0}, [SFX_BAND] = {0.65f, 0.40f, 0},
    [SFX_MENU_MOVE] = {0.08f, 0.30f, 0}, [SFX_MENU_OK] = {0.22f, 0.45f, 0}, [SFX_TRADE] = {0.55f, 0.45f, 0},
    [SFX_JOB_DONE] = {1.30f, 0.50f, 0}, [SFX_SAVE] = {0.70f, 0.40f, 0},
    [SFX_STEP_OUT] = {0.16f, 0.50f, 0}, [SFX_STEP_IN] = {0.16f, 0.50f, 0}, [SFX_SIREN] = {2.90f, 0.60f, 0},
    [SFX_BOOM] = {1.10f, 1.0f, 0}, [SFX_CLINK] = {0.35f, 0.45f, 0}, [SFX_BEEP] = {0.10f, 0.35f, 0}, [SFX_ZAP] = {0.45f, 0.75f, 0},
    [SFX_WHOOSH] = {0.60f, 0.65f, 0}, [SFX_TRAP] = {0.25f, 0.70f, 0},
    [SFX_FIRE] = {2.0f, 0.45f, 1}, [SFX_GEIGER] = {2.0f, 0.55f, 1}, [SFX_WIND] = {3.0f, 0.55f, 1},
    [SFX_DRONE] = {8.0f, 0.60f, 1},
    [SFX_BARK] = {0.22f, 0.70f, 0}, [SFX_DGROWL] = {0.70f, 0.55f, 0}, [SFX_WHINE] = {0.60f, 0.50f, 0}, [SFX_HORN] = {1.60f, 0.70f, 0},
};

static void fx_shot_pistol(float *b, int n)
{
    noise_l(b, n, 0, 0.30f, 1.0f, 6000, 800, 90, 0, 0.055f);
    tone_l(b, n, 0, 0.15f, 0.7f, 170, 60, 0.3f, 0.04f);
    noise_l(b, n, 0.02f, 0.40f, 0.20f, 1500, 350, 0, 0.01f, 0.14f);   // eco del cuarto
}
static void fx_shot_shotgun(float *b, int n)
{
    noise_l(b, n, 0, 0.5f, 1.2f, 4500, 300, 0, 0, 0.15f);
    tone_l(b, n, 0, 0.3f, 1.0f, 95, 32, 0.4f, 0.10f);
    noise_l(b, n, 0.03f, 0.8f, 0.40f, 900, 180, 0, 0.02f, 0.30f);
}
static void fx_shot_rifle(float *b, int n)
{
    noise_l(b, n, 0, 0.30f, 1.0f, 7000, 1200, 160, 0, 0.07f);
    noise_l(b, n, 0, 0.02f, 0.9f, 9000, 6000, 2000, 0, 0.008f);   // chasquido de la bala
    tone_l(b, n, 0, 0.12f, 0.55f, 230, 90, 0.2f, 0.045f);
    noise_l(b, n, 0.02f, 0.5f, 0.25f, 1800, 300, 0, 0.01f, 0.2f);
}
static void fx_shot_sil(float *b, int n)
{
    noise_l(b, n, 0, 0.12f, 0.6f, 2600, 800, 120, 0, 0.03f);
    tone_l(b, n, 0, 0.08f, 0.6f, 150, 70, 0.2f, 0.03f);
    noise_l(b, n, 0.035f, 0.02f, 0.3f, 8000, 5000, 3000, 0, 0.006f);   // el mecanismo
}
static void fx_reload(float *b, int n)
{
    noise_l(b, n, 0.00f, 0.05f, 0.8f, 4500, 1500, 900, 0, 0.012f); tone_l(b, n, 0.00f, 0.04f, 0.4f, 900, 500, 0.5f, 0.015f);      // sale el cargador
    noise_l(b, n, 0.30f, 0.15f, 0.2f, 3000, 1500, 1200, 0.03f, 0.10f);                                                           // roce
    noise_l(b, n, 0.58f, 0.06f, 0.9f, 3500, 900, 500, 0, 0.018f); tone_l(b, n, 0.58f, 0.06f, 0.6f, 320, 120, 0.3f, 0.03f);       // entra
    noise_l(b, n, 0.82f, 0.04f, 0.9f, 5000, 1800, 1000, 0, 0.010f); tone_l(b, n, 0.82f, 0.03f, 0.4f, 1100, 600, 0.5f, 0.012f);   // cerrojo atrás
    noise_l(b, n, 0.93f, 0.04f, 0.8f, 5000, 1800, 1000, 0, 0.010f); tone_l(b, n, 0.93f, 0.03f, 0.4f, 800, 500, 0.5f, 0.012f);    // y adelante
}
static void fx_jam(float *b, int n)
{
    noise_l(b, n, 0.00f, 0.03f, 0.8f, 7000, 2500, 2500, 0, 0.008f); tone_l(b, n, 0, 0.03f, 0.5f, 1800, 900, 0.3f, 0.01f);
    noise_l(b, n, 0.08f, 0.03f, 0.5f, 7000, 2500, 2500, 0, 0.007f); tone_l(b, n, 0.08f, 0.03f, 0.3f, 1500, 800, 0.3f, 0.01f);
}
static void fx_casing(float *b, int n)
{
    static const float T[4] = {0.26f, 0.32f, 0.36f, 0.395f}, A[4] = {1.0f, 0.55f, 0.35f, 0.2f}, F[4] = {4300, 3900, 4600, 4100};
    for (int k = 0; k < 4; k++) {
        tone_l(b, n, T[k], 0.12f, 0.5f * A[k], F[k], F[k] * 0.97f, 0.35f, 0.025f);
        tone_l(b, n, T[k], 0.08f, 0.25f * A[k], F[k] * 1.51f, F[k] * 1.5f, 0, 0.015f);
    }
}
static void fx_wall(float *b, int n)
{
    noise_l(b, n, 0, 0.12f, 0.8f, 5000, 1400, 800, 0, 0.03f);
    tone_l(b, n, 0, 0.06f, 0.3f, 2400, 1200, 0.2f, 0.02f);
    tone_l(b, n, 0.01f, 0.20f, 0.12f, 3200, 2000, 0, 0.07f);   // rebote
}
static void fx_zbite(float *b, int n)
{
    noise_l(b, n, 0, 0.2f, 1.0f, 1300, 300, 0, 0, 0.07f);
    tone_l(b, n, 0, 0.15f, 0.8f, 125, 48, 0.3f, 0.08f);
    noise_l(b, n, 0, 0.08f, 0.35f, 3500, 900, 600, 0, 0.03f);   // lo húmedo
}
static void fx_growl(float *b, int n)
{
    voice_l(b, n, 0, 0.9f, 0.9f, 88, 62, 9, 27, 0.5f, 520);
    noise_l(b, n, 0.1f, 0.7f, 0.12f, 700, 250, 0, 0.2f, 0.4f);
}
static void fx_axe(float *b, int n)
{
    noise_l(b, n, 0, 0.5f, 0.8f, 500, 3200, 150, 0.12f, 0.16f);
    tone_l(b, n, 0.05f, 0.4f, 0.15f, 140, 220, 0.5f, 0.15f);
}
static void fx_swing(float *b, int n)   // golpe al aire: un soplido que sube y baja
{
    noise_l(b, n, 0, 0.28f, 0.9f, 900, 3800, 200, 0.10f, 0.14f);
    noise_l(b, n, 0.08f, 0.2f, 0.5f, 3800, 700, 300, 0.05f, 0.10f);
}
static void fx_mel_hit(float *b, int n)   // golpe que da en carne: seco, grave y un crujido
{
    noise_l(b, n, 0, 0.12f, 1.0f, 2200, 300, 400, 0, 0.04f);
    tone_l(b, n, 0, 0.18f, 0.95f, 150, 45, 0.3f, 0.07f);
    noise_l(b, n, 0.02f, 0.06f, 0.5f, 5000, 1500, 800, 0, 0.015f);
    noise_l(b, n, 0.05f, 0.25f, 0.18f, 700, 200, 0, 0.1f, 0.16f);
}
static void fx_spit(float *b, int n)   // el escupitajo: gorgoteo que sube
{
    noise_l(b, n, 0, 0.30f, 0.7f, 600, 2400, 300, 0.15f, 0.12f);
    tone_l(b, n, 0.02f, 0.25f, 0.5f, 220, 520, 0.6f, 0.12f);
}
static void fx_splat(float *b, int n)   // el ácido cae al suelo
{
    noise_l(b, n, 0, 0.14f, 0.9f, 3000, 500, 500, 0, 0.04f);
    noise_l(b, n, 0.03f, 0.30f, 0.4f, 1200, 300, 0, 0.1f, 0.2f);
}
static void fx_roar(float *b, int n)
{
    voice_l(b, n, 0, 1.3f, 1.0f, 58, 36, 5, 16, 0.6f, 600);
    voice_l(b, n, 0.05f, 1.2f, 0.5f, 118, 70, 8, 22, 0.8f, 800);
    noise_l(b, n, 0, 1.3f, 0.30f, 400, 120, 0, 0.2f, 0.7f);
}
static void fx_hurt(float *b, int n)
{
    tone_l(b, n, 0, 0.2f, 0.9f, 190, 70, 0.3f, 0.10f);
    noise_l(b, n, 0, 0.2f, 0.7f, 1600, 400, 0, 0, 0.08f);
    voice_l(b, n, 0.03f, 0.3f, 0.45f, 140, 95, 4, 13, 0.3f, 700);
}
static void fx_heart(float *b, int n)
{
    tone_l(b, n, 0.00f, 0.22f, 1.0f, 58, 38, 0.2f, 0.07f); noise_l(b, n, 0.00f, 0.10f, 0.4f, 300, 100, 0, 0.005f, 0.05f);
    tone_l(b, n, 0.23f, 0.22f, 0.70f, 52, 36, 0.2f, 0.06f); noise_l(b, n, 0.23f, 0.10f, 0.3f, 300, 100, 0, 0.005f, 0.05f);
}
static void fx_crate(float *b, int n)
{
    tone_l(b, n, 0.00f, 0.38f, 0.14f, 380, 740, 0.6f, 0.30f);   // chirrido de bisagra
    noise_l(b, n, 0.00f, 0.38f, 0.25f, 1800, 900, 500, 0.15f, 0.20f);
    noise_l(b, n, 0.40f, 0.12f, 0.8f, 1200, 250, 0, 0, 0.05f); tone_l(b, n, 0.40f, 0.15f, 0.7f, 125, 55, 0.3f, 0.06f);   // golpe de la tapa
}
static void fx_pickup(float *b, int n)
{
    noise_l(b, n, 0.00f, 0.08f, 0.5f, 5000, 1800, 1500, 0.004f, 0.03f);
    tone_l(b, n, 0.02f, 0.10f, 0.20f, 1300, 1500, 0.3f, 0.04f);
    tone_l(b, n, 0.08f, 0.15f, 0.15f, 1750, 1700, 0.2f, 0.05f);
}
static void fx_eat(float *b, int n)
{
    static const float T[3] = {0.0f, 0.22f, 0.43f}, A[3] = {0.9f, 1.0f, 0.7f};
    for (int k = 0; k < 3; k++) {
        noise_l(b, n, T[k], 0.12f, A[k], 2600, 700, 300, 0.004f, 0.04f);
        tone_l(b, n, T[k], 0.08f, 0.25f * A[k], 190, 90, 0.3f, 0.035f);
    }
}
static void fx_drink(float *b, int n)
{
    static const float T[3] = {0.00f, 0.30f, 0.56f};
    for (int k = 0; k < 3; k++) {
        tone_l(b, n, T[k], 0.12f, 0.5f, 230, 430, 0.3f, 0.06f);
        noise_l(b, n, T[k], 0.16f, 0.2f, 900, 400, 0, 0.02f, 0.08f);
    }
    noise_l(b, n, 0.80f, 0.12f, 0.15f, 3000, 1000, 800, 0.01f, 0.05f);   // el aire al terminar
}
static void fx_band(float *b, int n)
{
    noise_l(b, n, 0.00f, 0.30f, 0.5f, 6500, 2500, 1500, 0.02f, 0.22f);   // se rasga la tela
    noise_l(b, n, 0.32f, 0.25f, 0.25f, 3000, 1500, 700, 0.05f, 0.15f);   // roce al envolver
}
static void fx_menu_move(float *b, int n)
{
    tone_l(b, n, 0, 0.06f, 0.6f, 520, 400, 0.3f, 0.015f);
    noise_l(b, n, 0, 0.02f, 0.15f, 6000, 3000, 2500, 0, 0.004f);
}
static void fx_menu_ok(float *b, int n)
{
    tone_l(b, n, 0, 0.18f, 0.6f, 330, 290, 0.4f, 0.07f);
    tone_l(b, n, 0, 0.18f, 0.5f, 165, 150, 0.3f, 0.09f);
    noise_l(b, n, 0, 0.02f, 0.2f, 5000, 2000, 1500, 0, 0.005f);
}
static void fx_trade(float *b, int n)
{
    tone_l(b, n, 0.00f, 0.4f, 0.4f, 2400, 2350, 0, 0.12f); tone_l(b, n, 0.00f, 0.3f, 0.2f, 3650, 3600, 0, 0.08f);
    tone_l(b, n, 0.07f, 0.4f, 0.3f, 3100, 3050, 0, 0.12f); tone_l(b, n, 0.07f, 0.3f, 0.15f, 4700, 4650, 0, 0.08f);
    noise_l(b, n, 0.00f, 0.02f, 0.3f, 9000, 5000, 4000, 0, 0.006f);
    noise_l(b, n, 0.20f, 0.10f, 0.25f, 1500, 500, 300, 0.005f, 0.04f);   // el cajón de monedas
}
static void fx_job_done(float *b, int n)
{
    static const float F[3] = {220, 262, 330};   // acorde menor, sombrío
    for (int k = 0; k < 3; k++) {
        tone_l(b, n, 0.13f * k, 1.0f, 0.4f, F[k], F[k] * 0.995f, 0.35f, 0.38f);
        tone_l(b, n, 0.13f * k, 1.0f, 0.15f, F[k] * 0.5f, F[k] * 0.5f, 0, 0.5f);
    }
}
static void fx_save(float *b, int n)
{
    tone_l(b, n, 0.00f, 0.4f, 0.45f, 330, 328, 0.3f, 0.16f);
    tone_l(b, n, 0.16f, 0.5f, 0.40f, 247, 246, 0.3f, 0.20f);
    noise_l(b, n, 0, 0.02f, 0.15f, 4000, 1500, 1000, 0, 0.005f);
}
static void fx_step_out(float *b, int n)
{
    noise_l(b, n, 0, 0.10f, 0.7f, 950, 330, 0, 0.006f, 0.035f);
    noise_l(b, n, 0.01f, 0.05f, 0.15f, 5000, 2500, 1800, 0, 0.015f);   // grava
}
static void fx_step_in(float *b, int n)
{
    tone_l(b, n, 0, 0.08f, 0.7f, 120, 70, 0.3f, 0.03f);
    noise_l(b, n, 0, 0.06f, 0.35f, 2000, 600, 300, 0, 0.02f);
    noise_l(b, n, 0.005f, 0.02f, 0.15f, 6000, 3000, 2000, 0, 0.006f);
}
static void fx_siren(float *b, int n)
{
    float ph = 0, lo = 0;
    int cnt = (int)(2.9f * SND_RATE) < n ? (int)(2.9f * SND_RATE) : n;
    for (int i = 0; i < cnt; i++) {
        float t = (float)i / SND_RATE;
        float f = 360 + 300 * (0.5f - 0.5f * cosf(PI2 * t / 1.45f));   // sube y baja dos veces
        ph += PI2 * f / SND_RATE;
        float y = sinf(ph) + 0.45f * sinf(2 * ph) + 0.2f * sinf(3 * ph);
        lo += (y - lo) * 0.35f;
        float env = t < 0.15f ? t / 0.15f : t > 2.5f ? (2.9f - t) / 0.4f : 1;
        b[i] += lo * env * 0.5f;
    }
    noise_l(b, n, 0, 2.9f, 0.04f, 900, 600, 0, 0.3f, 3.0f);   // algo de aire
}
static void fx_fire(float *b, int n)
{
    noise_l(b, n, 0, 2.25f, 0.55f, 380, 380, 40, 0, 100.0f);      // rumor grave del fuego
    noise_l(b, n, 0, 2.25f, 0.06f, 3500, 3500, 1500, 0, 100.0f);  // siseo
    for (int k = 0; k < 28; k++) {   // chasquidos
        float t = rnd01() * 2.1f, a = 0.2f + 0.8f * rnd01() * rnd01();
        noise_l(b, n, t, 0.02f, a * 0.9f, 6000, 1500, 900, 0, 0.004f + 0.004f * rnd01());
    }
}
static void fx_geiger(float *b, int n)
{
    float t = 0;
    for (;;) {
        t += -logf(1.0f - 0.999f * rnd01()) / 11.0f;   // llegadas al azar, unas 11 por segundo
        if (t > 1.95f) break;
        float a = 0.6f + 0.4f * rnd01();
        noise_l(b, n, t, 0.006f, a, 9000, 4000, 2500, 0, 0.0014f);
        tone_l(b, n, t, 0.008f, 0.35f * a, 2900, 2400, 0, 0.002f);
    }
}
static void fx_wind(float *b, int n)
{
    float lo = 0, lo2 = 0, hs = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SND_RATE, u = PI2 * t / 3.0f;
        float f = 420 + 260 * sinf(u) + 120 * sinf(2 * u + 1.1f);
        float a = 1 - expf(-PI2 * f / SND_RATE);
        lo += (rnd() - lo) * a;
        lo2 += (lo - lo2) * a;             // dos polos: más suave
        hs += (lo2 - hs) * 0.004f;         // quita lo más grave
        float am = 0.65f + 0.35f * sinf(u + 1.3f) * sinf(2 * u);
        b[i] += (lo2 - hs) * am * 9.0f;
    }
}


// objetos de mano y anomalías
static void fx_boom(float *b, int n)
{
    noise_l(b, n, 0, 0.9f, 1.0f, 2600, 120, 0, 0.004f, 0.30f);
    tone_l(b, n, 0, 0.7f, 1.0f, 95, 28, 0.4f, 0.20f);
    noise_l(b, n, 0.02f, 0.25f, 0.5f, 6000, 900, 800, 0, 0.05f);   // el estallido
}
static void fx_clink(float *b, int n)
{
    tone_l(b, n, 0, 0.30f, 0.8f, 3100, 2900, 0.2f, 0.09f);
    tone_l(b, n, 0.04f, 0.25f, 0.5f, 4700, 4300, 0, 0.07f);
    noise_l(b, n, 0, 0.03f, 0.4f, 7000, 4000, 2000, 0, 0.01f);
}
static void fx_beep(float *b, int n) { tone_l(b, n, 0, 0.08f, 0.9f, 1900, 1900, 0, 0.05f); }
static void fx_zap(float *b, int n)
{
    noise_l(b, n, 0, 0.35f, 0.9f, 7000, 2500, 1500, 0, 0.12f);
    tone_l(b, n, 0, 0.30f, 0.6f, 1400, 180, 0.6f, 0.14f);
    tone_l(b, n, 0.05f, 0.2f, 0.4f, 2600, 400, 0.5f, 0.08f);
}
static void fx_whoosh(float *b, int n)
{
    noise_l(b, n, 0, 0.55f, 0.9f, 400, 3500, 120, 0.12f, 0.20f);
    noise_l(b, n, 0.05f, 0.4f, 0.4f, 1800, 600, 0, 0.02f, 0.15f);
}
static void fx_trap(float *b, int n)
{
    tone_l(b, n, 0, 0.12f, 0.9f, 900, 240, 0.7f, 0.04f);
    noise_l(b, n, 0, 0.06f, 0.8f, 5000, 1500, 900, 0, 0.02f);
    tone_l(b, n, 0.08f, 0.15f, 0.5f, 520, 200, 0.4f, 0.05f);
}

static void fx_bark(float *b, int n)   // ladrido corto: golpe de voz que cae
{
    voice_l(b, n, 0, 0.16f, 0.9f, 420, 230, 4, 0, 0.35f, 1800);
    noise_l(b, n, 0, 0.05f, 0.4f, 2500, 900, 300, 0, 0.04f);
}
static void fx_dgrowl(float *b, int n)   // gruñido bajo y largo
{
    voice_l(b, n, 0, 0.65f, 0.8f, 110, 85, 7, 22, 0.55f, 600);
}
static void fx_whine(float *b, int n)   // quejido agudo que baja
{
    tone_l(b, n, 0, 0.25f, 0.6f, 780, 980, 0.2f, 0.12f);
    tone_l(b, n, 0.28f, 0.3f, 0.5f, 900, 520, 0.2f, 0.18f);
}
static void fx_horn(float *b, int n)   // corneta de aviso: dos notas graves
{
    tone_l(b, n, 0, 0.6f, 0.8f, 150, 150, 0.7f, 0.2f);
    tone_l(b, n, 0.7f, 0.8f, 0.8f, 112, 112, 0.7f, 0.3f);
}

// fondo de la introducción: bordón grave que respira (ciclos enteros en 8 s para que el bucle empalme) y dos golpes sordos
static void __attribute__((unused)) fx_drone(float *b, int n)
{
    // Son 8 s de sonido: con sinf por muestra el arranque se alargaba varios segundos (argumentos grandes = camino lento).
    // Se usa una tabla de senos de 1024 puntos y fases enteras de 32 bits, que además empalman exacto en el bucle.
    static float tab[1024];
    for (int k = 0; k < 1024; k++) tab[k] = sinf((float)k * (6.2831853f / 1024));
    const float K = 4294967296.0f / SND_RATE;
    const unsigned d1 = (unsigned)(55 * K), d2 = (unsigned)(82.5f * K), d3 = (unsigned)(110.25f * K);
    const unsigned da = (unsigned)(0.25f * K), dm = (unsigned)(0.375f * K);   // respiración (2 ciclos en 8 s) y vaivén (3 ciclos)
    unsigned p1 = 0, p2 = 0, p3 = 0, pa = 0, pm = 0;
    for (int i = 0; i < n; i++) {
        float am = 0.7f + 0.3f * tab[pa >> 22];
        int mod = (int)(tab[pm >> 22] * 163.0f);   // 1 radián = 163 puntos de la tabla
        b[i] += (0.55f * tab[p1 >> 22] + 0.30f * tab[((p2 >> 22) + (unsigned)mod) & 1023] + 0.12f * tab[p3 >> 22]) * am * 0.8f;
        p1 += d1; p2 += d2; p3 += d3; pa += da; pm += dm;
    }
    tone_l(b, n, 2.0f, 1.0f, 0.7f, 70, 32, 0.3f, 0.25f);
    tone_l(b, n, 6.0f, 1.0f, 0.7f, 64, 30, 0.3f, 0.25f);
}

typedef void (*FxFn)(float *, int);
static const FxFn FX[SFX_COUNT] = {
    [SFX_SHOT_PISTOL] = fx_shot_pistol, [SFX_SHOT_SHOTGUN] = fx_shot_shotgun, [SFX_SHOT_RIFLE] = fx_shot_rifle, [SFX_SHOT_SIL] = fx_shot_sil,
    [SFX_RELOAD] = fx_reload, [SFX_JAM] = fx_jam, [SFX_CASING] = fx_casing, [SFX_WALL] = fx_wall,
    [SFX_ZBITE] = fx_zbite, [SFX_GROWL] = fx_growl, [SFX_AXE] = fx_axe, [SFX_BOSS_ROAR] = fx_roar,
    [SFX_SWING] = fx_swing, [SFX_MEL_HIT] = fx_mel_hit, [SFX_SPIT] = fx_spit, [SFX_SPLAT] = fx_splat,
    [SFX_HURT] = fx_hurt, [SFX_HEART] = fx_heart, [SFX_CRATE] = fx_crate, [SFX_PICKUP] = fx_pickup,
    [SFX_EAT] = fx_eat, [SFX_DRINK] = fx_drink, [SFX_BAND] = fx_band,
    [SFX_MENU_MOVE] = fx_menu_move, [SFX_MENU_OK] = fx_menu_ok, [SFX_TRADE] = fx_trade, [SFX_JOB_DONE] = fx_job_done, [SFX_SAVE] = fx_save,
    [SFX_STEP_OUT] = fx_step_out, [SFX_STEP_IN] = fx_step_in, [SFX_SIREN] = fx_siren,
    [SFX_BOOM] = fx_boom, [SFX_CLINK] = fx_clink, [SFX_BEEP] = fx_beep, [SFX_ZAP] = fx_zap, [SFX_WHOOSH] = fx_whoosh, [SFX_TRAP] = fx_trap,
    [SFX_FIRE] = fx_fire, [SFX_GEIGER] = fx_geiger, [SFX_WIND] = fx_wind,
    [SFX_BARK] = fx_bark, [SFX_DGROWL] = fx_dgrowl, [SFX_WHINE] = fx_whine, [SFX_HORN] = fx_horn,
};

int snd_synth(int id, short **out)
{
    *out = NULL;
    if (id < 0 || id >= SFX_COUNT || !FX[id]) return 0;
    const Spec *sp = &SPEC[id];
    int n = (int)(sp->dur * SND_RATE);
    int xf = sp->loop && id != SFX_GEIGER ? SND_RATE / 4 : 0;   // los bucles de ruido se empalman con un fundido
    float *b = calloc((size_t)(n + xf), sizeof(float));
    short *o = malloc((size_t)n * sizeof(short));
    if (!b || !o) { free(b); free(o); return 0; }
    g_rs = 2463534242u ^ (unsigned)(id * 2654435761u);
    FX[id](b, n + xf);
    for (int i = 0; i < xf; i++) {   // fundido de igual potencia con la cola que sobra: el final empalma con el inicio
        float th = 1.5707963f * i / xf;
        b[i] = b[i] * sinf(th) + b[n + i] * cosf(th);
    }
    float pk = 0;
    for (int i = 0; i < n; i++) { if (fabsf(b[i]) > pk) pk = fabsf(b[i]); }
    float g = pk > 1e-6f ? sp->peak / pk : 0;
    for (int i = 0; i < n; i++) o[i] = (short)(b[i] * g * 32767.0f);
    free(b);
    *out = o;
    return n;
}
