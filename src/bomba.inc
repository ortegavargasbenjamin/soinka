// [bomba] La bomba de Kolos: 15 minutos para saquear fuera de la base; al llegar a cero todo explota.
// g_bomba_t = segundos que quedan (0 = parada: en Estación Kolos y en Mi búnker no corre).
#define BOMBA_SEG 900
static float g_bomba_flash;   // destello blanco al explotar
static int g_bomba_muerte;    // la muerte fue por la bomba (lo explica la pantalla final)

static int bomba_fuera(void) { return g_zone != 0 && g_zone != ZB; }   // [bomba] zonas donde corre la cuenta

// Margen extra: el Ojo de ceniza +2 min, el Filtro negro +3 min
static float bomba_margen(void) { return (art_on(A_OJO) ? 120.0f : 0) + (g_bag[ITEM_R_FILTRO] ? 180.0f : 0); }

static void bomba_explota(void)
{
    g_bomba_flash = 1.6f;
    boom_at(P.x, P.y, 90, 0, 0);
    snd_play(SFX_BOOM, 1, 0);
    g_shake = 16;
    g_bomba_t = 0;
    if (bomba_fuera() && g_state != GS_DEAD && g_state != GS_FIN) {   // [arregla] con cualquier pantalla abierta también; sin dioses ni amuleto: todo explota
        P.hp = 0; g_state = GS_DEAD; g_state_t = 0; g_bomba_muerte = 1;
    }
}

static void bomba_update(float dt)
{
    if (g_bomba_flash > 0) g_bomba_flash -= dt;
    if (g_state == GS_PLAY) g_bomba_muerte = 0;
    if (!bomba_fuera()) { g_bomba_t = 0; return; }   // en la base se detiene; la próxima salida empieza de nuevo
    if (g_bomba_t <= 0) {
        g_bomba_t = BOMBA_SEG + bomba_margen();
        msg("Tienes 15 minutos antes del bombardeo. Kolos bombardea el sector para contener la Ceniza.");
        return;
    }
    float antes = g_bomba_t;
    g_bomba_t -= dt;
    if (g_bomba_t <= 0) { bomba_explota(); return; }
#define BCRUCE(s) (antes > (s) && g_bomba_t <= (s))
    if (BCRUCE(300)) msg("Quedan 5 minutos para el bombardeo. Piensa en volver.");
    if (BCRUCE(120)) { snd_play(SFX_SIREN, 0.9f, 0); msg("Suena la sirena: quedan 2 minutos. Vuelve a la base."); }
    if (BCRUCE(60)) msg("Queda 1 minuto. Corre a un punto de retirada.");
    if (BCRUCE(30)) msg("Quedan 30 segundos. Todo va a explotar.");
#undef BCRUCE
}

static void bomba_hud(void)
{
    char s[40];
    if (g_bomba_t > 0) {
        int t = (int)g_bomba_t;
        snprintf(s, sizeof(s), "Bomba %d:%02d", t / 60, t % 60);
        int on = t >= 120 || ((int)(g_time * 3) & 1);
        r_text_shadow(SCR_W - 8 - r_text_w(s), 6, s, t < 120 ? (on ? RGBA(255, 70, 60, 255) : RGBA(150, 40, 36, 255)) : RGBA(236, 214, 150, 255));
        if (t <= 60) {   // tinte rojizo en los bordes
            int a = t <= 30 ? 90 : 55;
            r_rect(0, 0, SCR_W, 14, RGBA(200, 20, 10, a)); r_rect(0, SCR_H - 14, SCR_W, 14, RGBA(200, 20, 10, a));
            r_rect(0, 14, 14, SCR_H - 28, RGBA(200, 20, 10, a)); r_rect(SCR_W - 14, 14, 14, SCR_H - 28, RGBA(200, 20, 10, a));
        }
    }
    if (g_bomba_flash > 0) r_rect(0, 0, SCR_W, SCR_H, RGBA(255, 255, 255, (int)(255 * clampf(g_bomba_flash / 1.2f, 0, 1))));
}
