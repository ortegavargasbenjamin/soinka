// artef.inc -- [artef] artefactos con ventaja fuerte y costo claro, y piezas de armas únicas. Se incluye desde game.c.
// Los diez artefactos (A_*) usan los objetos ITEM_ART0..5 y ITEM_ART6..9 y las prendas de GEAR (por ITEM, con gear_of).
// La radiación acumulada hace daño desde RAD_DANO (50): sube la barra con una marca y avisa; el antirradiación es un suministro.

static float g_uq_ban_t;      // cuánto queda del aviso destacado de pieza única
static char g_uq_ban[72];
static int g_uq_open;         // la lista "Piezas únicas" del taller está abierta
static int g_rad_avisada;     // ya se avisó del daño por radiación

// ------------------------------------------------------------------ artefactos: efectos
static const char *const ART_PLUS[A_N] = {
    "Regenera 2,5 de vida por segundo", "Sin frío y 10 % más rápido", "Recibes 35 % menos daño",
    "No sangras y las curas rinden el doble", "El aguante no se gasta al correr", "+2 min antes de la bomba y la radiación baja sola",
    "+40 de vida máxima", "+30 % de daño con armas y golpes", "50 % más chatarra y la recoges de más lejos", "+15 kg de carga"};
static const char *const ART_COST[A_N] = {
    "Suma mucha radiación sin parar", "El hambre corre 70 % más", "Vas 15 % más lento",
    "20 menos de vida máxima", "La sed corre el doble", "Tus pasos suenan 2,5 veces más",
    "Suma radiación sin parar", "Suma radiación sin parar", "Los enemigos te ven de más lejos", "El hambre corre 50 % más"};

static int art_on(int i)
{
    int gi = gear_of(ART_ITEM(i));
    if (gi < 0) return 0;
    return g_eq[EQ_ART1] == gi + 1 || g_eq[EQ_ART2] == gi + 1 || (g_eq[EQ_ART3] == gi + 1 && rank() >= 3);
}
static float hp_max(void) { return 100 + (art_on(A_CORAZON) ? 40 : 0) - (art_on(A_LAGRIMA) ? 20 : 0); }
static float art_dmg(void) { return art_on(A_COLMILLO) ? 1.3f : 1; }
static float art_taken(void) { return art_on(A_PLOMO) ? 0.65f : 1; }
static float art_speed(void) { return (art_on(A_ASCUA) ? 1.1f : 1) * (art_on(A_PLOMO) ? 0.85f : 1); }
static float art_heal(void) { return art_on(A_LAGRIMA) ? 2 : 1; }
static float art_thirst(void) { return art_on(A_PULMON) ? 2 : 1; }
static float art_hunger(void) { return (art_on(A_ASCUA) ? 1.7f : 1) * (art_on(A_SEMILLA) ? 1.5f : 1); }
static float art_see(void) { return art_on(A_IMAN) ? 1.4f : 1; }
float artef_kg(void) { return art_on(A_SEMILLA) ? 15.0f : 0.0f; }   // [artef] carga extra que suma la Semilla hueca (la mochila por peso la suma)

// Cuál artefacto nace en un campo de anomalías. Los comunes siempre; Corazón, Piel de plomo y Colmillo solo desde peligro 5 (a veces)
static int artef_com(unsigned r)
{
    static const unsigned char COM[7] = {A_LATIDO, A_ASCUA, A_LAGRIMA, A_PULMON, A_OJO, A_IMAN, A_SEMILLA};
    return ART_ITEM(COM[r % 7]);
}
static int artef_rare(unsigned r)
{
    static const unsigned char RAR[3] = {A_CORAZON, A_COLMILLO, A_PLOMO};
    return ART_ITEM(RAR[r % 3]);
}
static int artef_pick(unsigned r)
{
    if (g_x.danger >= 5 && r % 100 < 25) return artef_rare(r / 100);
    return artef_com(r / 100);
}

// Cada segundo de juego: lo que suma o resta a la vida y la radiación
static void artef_tick(float dt)
{
    if (art_on(A_LATIDO)) { P.hp += 2.5f * dt; add_rad(2.0f * dt); }
    if (art_on(A_CORAZON)) add_rad(0.8f * dt);
    if (art_on(A_COLMILLO)) add_rad(0.8f * dt);
    if (art_on(A_OJO)) P.rad = clampf(P.rad - 1.2f * dt, 0, 100);
    if (P.hp > hp_max()) P.hp = hp_max();
}

// ------------------------------------------------------------------ piezas únicas
#define UQ_N 8
static const short UQ_ITEM[UQ_N] = {ITEM_UP0, ITEM_UP1, ITEM_UP2, ITEM_UP3, ITEM_UP4, ITEM_UP5, ITEM_UP6, ITEM_UP7};
static const char *const UQ_NAME[UQ_N] = {"Susurro de la Bestia", "Mirada de hierro", "Cargador de la Viuda", "Culata del Capataz",
                                          "Boca del Fermentado", "Núcleo del guardián", "Visor del piloto", "Linterna farolera"};
static const char *const UQ_HINT[UQ_N] = {"La lleva La Bestia, la jefa de las expediciones.", "La guarda Cabeza de Hierro.",
                                          "La guarda La Viuda blanca.", "La guarda El Capataz.", "La guarda El Fermentado.",
                                          "La custodia el guardián del piso 10 del Pozo.", "El blindado que cuida el helicóptero caído.",
                                          "El puesto farolero la entrega con 5 de reputación."};
static const u32 UQ_GOLD = RGBA(255, 214, 96, 255);

static int uq_n(void) { int n = 0; for (int i = 0; i < UQ_N; i++) n += (g_uniq >> i) & 1; return n; }
static int uq_is_part(int pi) { return pi >= 0 && pi < PARTS_N && PARTS[pi].item >= ITEM_UP0 && PARTS[pi].item <= ITEM_UP7; }
static u32 artef_pcol(int pi, u32 normal) { return uq_is_part(pi) ? UQ_GOLD : normal; }

// El arma en mano lleva puesta esa pieza (por su objeto)
static int uniq_part_on(int item)
{
    if (P.weapon < 0 || P.weapon >= W_COUNT) return 0;
    for (int k = 0; k < SLOT_COUNT; k++) {
        int p = g_w[P.weapon].part[k];
        if (p && p <= PARTS_N && PARTS[p - 1].item == item) return 1;
    }
    return 0;
}

// Entrega la pieza única n (una sola vez en toda la partida): a la mochila o, si no cabe, al suelo
static void uniq_grant(int n)
{
    char s[96];
    if (n < 0 || n >= UQ_N || ((g_uniq >> n) & 1)) return;
    g_uniq |= 1 << n;
    if (inv_add(UQ_ITEM[n], 1) < 1) drop(P.x, P.y + 6, UQ_ITEM[n], 1);
    snprintf(g_uq_ban, sizeof(g_uq_ban), "PIEZA ÚNICA: %s", UQ_NAME[n]);
    g_uq_ban_t = 6;
    snprintf(s, sizeof(s), "Pieza única %d/%d: %s. Instálala en el taller.", uq_n(), UQ_N, UQ_NAME[n]);
    msg(s);
    snd_play(SFX_TRADE, 0.9f, 0);
}

// Al morir un enemigo: piezas únicas de los jefes, del guardián del Pozo y del helicóptero; a veces un artefacto raro
static void artef_muere(const Enemy *e)
{
    if (e->ev == EVN_HELI && e->type == E_ARM) uniq_grant(6);
    if (!is_boss(e)) return;
    if (e->ev != EVN_POZO) switch (e->type) {   // [arregla] los del Pozo no dan las piezas 0 y 1
    case E_BOSS: uniq_grant(0); break;
    case E_BOSS2: uniq_grant(1); break;
    case E_VIUDA: uniq_grant(2); break;
    case E_CAPATAZ: uniq_grant(3); break;
    case E_FERMEN: uniq_grant(4); break;
    default: break;
    }
    if (g_zone == 2 && g_pz == 10) uniq_grant(5);
    if (frand() < 0.45f) drop(e->x + 12, e->y - 6, artef_rare(rnd()), 1);   // los jefes y el Pozo sueltan los artefactos raros
}

static void artef_update(float dt)
{
    if (g_uq_ban_t > 0) g_uq_ban_t -= dt;
    if (g_state == GS_FARO && g_frep >= 5) uniq_grant(7);   // el puesto farolero la entrega
    if (P.rad >= RAD_DANO && !g_rad_avisada) { g_rad_avisada = 1; msg("La radiación ya te hace daño. Busca antirradiación."); }
    if (P.rad < RAD_DANO - 10) g_rad_avisada = 0;
}

// La linterna farolera alumbra más largo (como la linterna táctica)
static void artef_light(void)
{
    if (!is_melee(P.weapon) && uniq_part_on(ITEM_UP7)) add_light(P.x, P.y, 250, 0.9f, P.ang, 0.65f);
}

// Mirada de hierro: un punto rojo sobre los enemigos cercanos tras los muros
static void artef_draw_world(void)
{
    if (g_state != GS_PLAY || !uniq_part_on(ITEM_UP1)) return;
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e) || dist(P.x, P.y, e->x, e->y) > 150 || los(P.x, P.y, e->x, e->y)) continue;
        float x = e->x - g_ox, y = e->y - g_oy - 8, p = 1.5f * sinf(g_time * 6 + i);
        r_rect(x - 3 - p, y - 1, 6 + 2 * p, 2, RGBA(255, 70, 60, 220));
        r_rect(x - 1, y - 3 - p, 2, 6 + 2 * p, RGBA(255, 70, 60, 220));
    }
}

// ------------------------------------------------------------------ pantallas
// Barra de radiación: marca donde empieza el daño; sobre ese valor late en rojo. Vida máxima y aviso de pieza única
static void artef_hud(void)
{
    char s[40];
    r_rect(20 + 70 * RAD_DANO / 100, 249, 1, 8, RGBA(240, 90, 80, 210));
    if (P.rad >= RAD_DANO) {
        int a = 80 + (int)(70 * (0.5f + 0.5f * sinf(g_time * 6)));
        r_rect(20, 251, 70 * clampf(P.rad / 100, 0, 1), 4, RGBA(230, 60, 50, a));
        r_text(94, 249, "!", RGBA(240, 90, 80, 255));
    }
    if (hp_max() != 100) {
        snprintf(s, sizeof(s), "%d/%d", (int)ceilf(P.hp), (int)hp_max());
        r_text(24, 221, s, hp_max() > 100 ? RGBA(120, 214, 110, 255) : RGBA(240, 90, 80, 255));
    }
    if (g_uq_ban_t > 0) {
        int a = (int)(255 * clampf(g_uq_ban_t, 0, 1));
        r_rect(90, 58, 300, 22, RGBA(0, 0, 0, a * 3 / 4));
        r_text_center(SCR_W / 2, 62, g_uq_ban, RGBA(255, 214, 96, a));
    }
}

// En la mochila: ventaja en verde y costo en rojo del artefacto elegido (devuelve 1 si dibujó algo)
static int artef_info(int item)
{
    int ai = -1;
    for (int i = 0; i < A_N; i++) if (item >= 0 && ART_ITEM(i) == item) ai = i;
    if (ai < 0) return 0;
    int w1 = r_text_w(ART_PLUS[ai]), w2 = r_text_w(ART_COST[ai]), x = SCR_W / 2 - (w1 + w2 + 12) / 2;
    r_text(x, 231, ART_PLUS[ai], RGBA(120, 214, 110, 255));
    r_text(x + w1 + 12, 231, ART_COST[ai], RGBA(240, 90, 80, 255));
    return 1;
}

// Lista "Piezas únicas n/8" del taller (Select la abre y la cierra)
static int artef_ws_input(void)
{
    if (g_uq_open) {
        if (in.pressed & (PSP_CTRL_SELECT | PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)) g_uq_open = 0;
        return 1;
    }
    if (in.pressed & PSP_CTRL_SELECT) { g_uq_open = 1; return 1; }
    return 0;
}
static void artef_ws_draw(void)
{
    char s[64];
    snprintf(s, sizeof(s), "PIEZAS ÚNICAS %d/%d", uq_n(), UQ_N);
    if (!g_uq_open) { text_right(204, 62, "Select: únicas", UI_DIM); return; }
    ui_panel(20, 36, 440, 196, s);
    for (int i = 0; i < UQ_N; i++) {
        float y = 58 + i * 20;
        int has = (g_uniq >> i) & 1;
        ui_row(28, y, 424, 18, 0);
        r_text(36, y + 4, has ? UQ_NAME[i] : "???", has ? UQ_GOLD : UI_DIM);
        text_right(444, y + 4, has ? "conseguida" : UQ_HINT[i], has ? RGBA(120, 214, 110, 255) : UI_DIM);
    }
    r_text_center(SCR_W / 2, 222, "Select o Círculo: volver", UI_DIM);
}
