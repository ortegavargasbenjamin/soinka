// [duro] Más dificultad, más botín y suministros para la base. Se incluye desde game.c en tres momentos (DURO_PART 0, 1 y 2):
//   0: macros con las filas nuevas de las tablas (objetos, tienda, botín, recetas, pestaña)
//   1: el estado, la tabla de modos y los ganchos del juego (después de msg())
//   2: la pantalla de PEDIDOS y el panel de suministros (antes de KTAB)
// Lo que hace cada gancho está junto a su función.

#if DURO_PART == 0
// ---------------------------------------------------------------- filas de tablas (macros)
#define DURO_ITEM_ENUM ITEM_D_HERR, ITEM_D_COBRE, ITEM_D_ELEC, ITEM_D_BATERIA, ITEM_D_CINTA, ITEM_D_ANTIB, ITEM_D_ALCOHOL, ITEM_D_CAFE, \
    ITEM_D_TABACO, ITEM_D_MAPA, ITEM_D_COMBUS, ITEM_D_FILTRO,
#define DURO_N 12   // objetos nuevos
#define DURO_ITEM_ROWS \
    [ITEM_D_HERR] = {"Herramientas", "herramienta", "herramientas", "Material: kits de reparación. Se vende bien", S_IT_MECH, S_I_PART, 6, 1, RGBA(190, 200, 215, 255), 0}, \
    [ITEM_D_COBRE] = {"Cable de cobre", "cable de cobre", "cables de cobre", "Material: munición con pólvora. Se vende bien", S_IT_RAG, S_I_PART, 10, 1, RGBA(230, 150, 90, 255), 0}, \
    [ITEM_D_ELEC] = {"Electrónica", "pieza electrónica", "piezas electrónicas", "Material delicado: lo piden los pedidos", S_IT_DET, S_I_PART, 6, 1, RGBA(120, 220, 140, 255), 0}, \
    [ITEM_D_BATERIA] = {"Batería", "batería", "baterías", "Material: lo piden los pedidos. Se vende caro", S_IT_NUT, S_I_PART, 6, 1, RGBA(250, 230, 100, 255), 0}, \
    [ITEM_D_CINTA] = {"Cinta aislante", "cinta aislante", "cintas aislantes", "Material: kits de reparación", S_IT_RAG, S_I_PART, 10, 1, RGBA(120, 120, 130, 255), 0}, \
    [ITEM_D_ANTIB] = {"Antibióticos", "antibióticos", "antibióticos", "Cortan el sangrado y dan 15 % de resistencia 3 min", S_IT_MED, S_I_MED, 4, 1, RGBA(150, 230, 240, 255), 0}, \
    [ITEM_D_ALCOHOL] = {"Alcohol", "alcohol", "alcohol", "Desinfecta: corta el sangrado. Material de vendas", S_IT_WATER, S_I_WATER, 5, 1, RGBA(250, 200, 110, 255), 0}, \
    [ITEM_D_CAFE] = {"Café", "café", "café", "Aguante sin límite durante 60 s", S_IT_FOOD, S_I_FOOD, 5, 1, RGBA(170, 120, 80, 255), 0}, \
    [ITEM_D_TABACO] = {"Tabaco", "tabaco", "tabaco", "Calma el hambre y recupera el aguante", S_IT_RAG, S_I_PART, 8, 1, RGBA(210, 180, 120, 255), 0}, \
    [ITEM_D_MAPA] = {"Mapa viejo", "mapa viejo", "mapas viejos", "Descubre el mapa de SELECT de la expedición", S_IT_SEW, S_I_PART, 3, 1, RGBA(230, 215, 170, 255), 0}, \
    [ITEM_D_COMBUS] = {"Combustible", "bidón de combustible", "bidones de combustible", "Para el generador del búnker: déjalo en el panel", S_IT_CHEM, S_I_PART, 5, 1, RGBA(240, 90, 70, 255), 0}, \
    [ITEM_D_FILTRO] = {"Filtro de agua", "filtro de agua", "filtros de agua", "Para el búnker: da agua limpia 5 regresos", S_IT_FILTER, S_I_PART, 3, 1, RGBA(110, 180, 255, 255), 0},
#define DURO_SHOP_ROWS \
    {"Herramientas", S_IT_MECH, ITEM_D_HERR, 1, 35, 1, 2}, {"Cable de cobre", S_IT_RAG, ITEM_D_COBRE, 1, 22, 1, 3}, \
    {"Electrónica", S_IT_DET, ITEM_D_ELEC, 1, 50, 2, 2}, {"Batería", S_IT_NUT, ITEM_D_BATERIA, 1, 40, 1, 2}, \
    {"Cinta aislante", S_IT_RAG, ITEM_D_CINTA, 1, 16, 0, 3}, {"Antibióticos", S_IT_MED, ITEM_D_ANTIB, 1, 45, 1, 2}, \
    {"Alcohol", S_IT_WATER, ITEM_D_ALCOHOL, 1, 18, 0, 3}, {"Café", S_IT_FOOD, ITEM_D_CAFE, 1, 16, 0, 3}, \
    {"Tabaco", S_IT_RAG, ITEM_D_TABACO, 1, 14, 0, 3}, {"Mapa viejo", S_IT_SEW, ITEM_D_MAPA, 1, 40, 1, 2}, \
    {"Combustible", S_IT_CHEM, ITEM_D_COMBUS, 1, 55, 1, 3}, {"Filtro de agua", S_IT_FILTER, ITEM_D_FILTRO, 1, 65, 1, 2},
#define DURO_LOOT_ROWS \
    {ITEM_D_ANTIB, 2, 1, 1}, {ITEM_D_ALCOHOL, 3, 1, 1}, {ITEM_D_CAFE, 3, 1, 2}, {ITEM_D_TABACO, 3, 1, 2}, {ITEM_D_CINTA, 3, 1, 1}, \
    {ITEM_D_HERR, 2, 1, 1}, {ITEM_D_COBRE, 3, 1, 2}, {ITEM_D_BATERIA, 2, 1, 1}, {ITEM_D_ELEC, 1, 1, 1, 1}, {ITEM_D_MAPA, 1, 1, 1, 1}, \
    {ITEM_D_COMBUS, 1, 1, 1, 1}, {ITEM_D_FILTRO, 1, 1, 1, 1},
#define DURO_RECIPE_ROWS \
    {"Balas 9x18 x12 (pólvora)", S_IT_9MM, ITEM_9MM, 12, {{ITEM_POWDER, 1}, {ITEM_D_COBRE, 1}}}, \
    {"Balas 7,62 x15 (pólvora)", S_IT_762, ITEM_762, 15, {{ITEM_POWDER, 2}, {ITEM_D_COBRE, 1}}}, \
    {"Kit de reparación (herram.)", S_IT_CLEAN, ITEM_REPAIR, 1, {{ITEM_D_HERR, 1}, {ITEM_D_CINTA, 1}}}, \
    {"Vendas x3 (alcohol)", S_IT_BAND, ITEM_BAND, 3, {{ITEM_D_ALCOHOL, 1}, {ITEM_RAG, 2}}},
#define DURO_KTAB_ROW {"PEDIDOS", "Arriba/abajo: elegir   Cruz: entregar   Círculo: salir", dped_input, dped_draw, NULL, KW_COM},

#elif DURO_PART == 1
// ---------------------------------------------------------------- tabla de modos
typedef struct {
    float dmg, hp, sight, hear, count;   // enemigos: daño que hacen, vida, vista, oído y cantidad por expedición
    float ammo, med, food;               // botín: munición, medicinas y comida (1 = como antes)
    float need, bleed, storm;            // hambre y sed, sangrado (vida que quita), tiempo hasta la Ceniza
    float heal_t;                        // segundos que tarda curarse con venda o botiquín (0 = al instante)
} DuroMul;
// Los enemigos quedan como estaban (pedido del dueño: "la dificultad de los enemigos está bien"): lo duro viene de la escasez
static const DuroMul DURO_TAB[MODE_COUNT] = {
    [MODE_SURV] = {1, 1, 1, 1, 1, 0.65f, 0.70f, 0.80f, 1.25f, 1.5f, 0.80f, 1.5f},
    [MODE_EXPL] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},   // Explorador: como era
    [MODE_IRON] = {1, 1, 1, 1, 1, 0.65f, 0.70f, 0.80f, 1.25f, 1.5f, 0.80f, 1.5f},   // Hierro = Superviviente más su borrado
};
#define DM (DURO_TAB[g_mode])

// estado (va al guardado en SaveData.duro, en este orden)
static int g_dret;         // regresos hechos (los pedidos cambian con cada uno)
static int g_dped_done;    // bits de los pedidos ya entregados
static int g_dlit;         // el generador del búnker tiene combustible
static int g_dfuel;        // bidones en el panel de suministros
static int g_dfilt;        // filtros de repuesto en el panel
static int g_dfilt_left;   // regresos que le quedan al filtro puesto
static float g_dh_t;       // curándose: segundos que faltan (0 = no)
static int g_dh_go;
static float g_dcafe_t, g_dres_t;   // café (aguante sin límite) y antibióticos (resistencia)
static char g_dret_l1[112], g_dret_l2[112];   // lo que dice el resumen del regreso
static int g_dret_c1, g_dret_c2;            // 0 = bien, 1 = falta algo

static void duro_reset(void) { g_dret = g_dped_done = g_dlit = g_dfuel = g_dfilt = g_dfilt_left = g_dh_go = 0; g_dh_t = g_dcafe_t = g_dres_t = 0; g_dret_l1[0] = g_dret_l2[0] = 0; }
static void duro_save(int *d) { d[0] = g_dret; d[1] = g_dped_done; d[2] = g_dlit; d[3] = g_dfuel; d[4] = g_dfilt; d[5] = g_dfilt_left; }
static void duro_load(const int *d) { duro_reset(); g_dret = d[0]; g_dped_done = d[1]; g_dlit = d[2]; g_dfuel = d[3]; g_dfilt = d[4]; g_dfilt_left = d[5]; }

// ---------------------------------------------------------------- ganchos de dificultad
static float duro_dmg(void) { return DM.dmg * (g_dres_t > 0 ? 0.85f : 1); }   // hurt_player
static float duro_ehp(void) { return DM.hp; }                                  // spawn_enemy
static float duro_sight(void) { return DM.sight; }                             // foe_pick
static float duro_hear(void) { return DM.hear; }                               // noise
static float duro_storm(void) { return DM.storm; }                             // al reiniciar el reloj de la Ceniza

static void spawn_enemy(int type, float x, float y);
static void duro_extra_enemy(int type, float x, float y)   // spawn_z: en las expediciones, de cada enemigo hay una probabilidad de uno más
{
    if (EHP[type] >= 200 || DM.count <= 1 || g_ne >= MAXE - 6) return;
    if (frand() < DM.count - 1) spawn_enemy(type, x, y);
}

// cantidad de botín ajustada por el modo: redondeo al azar (1 x 0,7 = 70 % de que salga)
static int duro_loot_n(int item, int n)
{
    float m = 1;
    if (item >= ITEM_9MM && item <= ITEM_12G_SLUG) m = DM.ammo;
    else if (item == ITEM_BAND || item == ITEM_MED || item == ITEM_ARAD) m = DM.med;
    else if (item == ITEM_FOOD) m = DM.food;
    if (n <= 1) return n;   // lo que viene de a uno (venda, conserva, pieza) no se pierde: la escasez viene de menos tiradas por cajón
    m *= 0.7f;               // los montones (balas, chatarra de botín) bajan además un 30 %
    if (m >= 1) return n;
    return (int)(n * m + frand());
}

// curarse tarda: la primera pulsación arranca la barra, al terminar se cura de verdad
static int duro_heal_start(void)   // primera línea de heal(): 1 = ya se encargó
{
    if (g_dh_t > 0) return 1;
    if (g_dh_go) return 0;
    if (DM.heal_t <= 0) return 0;
    if ((g_bag[ITEM_BAND] <= 0 && g_bag[ITEM_MED] <= 0) || (P.hp >= hp_max() && P.bleed <= 0)) return 0;   // [arregla] nada que hacer: heal() lo explica
    g_dh_t = DM.heal_t;
    msg("Te curas... (si te pegan, se corta)");
    return 1;
}
static void duro_hit(float dmg)   // hurt_player: un golpe corta la cura
{
    if (g_dh_t > 0 && dmg > 0) { g_dh_t = 0; msg("Te pegaron: la cura se cortó."); }
}
static void heal(void);
static void duro_update(float dt, float slow)   // update_player, tras el hambre y la sed
{
    float ex = DM.need - 1;
    if (ex > 0) {   // hambre y sed más rápidas: se descuenta el resto
        P.thirst = clampf(P.thirst - dt / 6 * slow * ex, 0, 100);
        P.hunger = clampf(P.hunger - dt / 9 * slow * ex, 0, 100);
    }
    if (P.bleed > 0 && DM.bleed > 1) P.hp -= 1.6f * (DM.bleed - 1) * dt;   // el sangrado quita más
    if (g_dh_t > 0) {
        if ((g_dh_t -= dt) <= 0) { g_dh_t = 0; g_dh_go = 1; heal(); g_dh_go = 0; }
    }
    if (g_dcafe_t > 0) { g_dcafe_t -= dt; P.stam = 100; }
    if (g_dres_t > 0) g_dres_t -= dt;
}

// ---------------------------------------------------------------- objetos nuevos
static int duro_use(int item)   // use_item: 0 = no es mío, 1 = usado, 2 = no se pudo usar
{
    if (item < ITEM_D_HERR || item > ITEM_D_FILTRO) return 0;
    if (g_bag[item] <= 0) { msg("No te queda."); return 2; }
    switch (item) {
    case ITEM_D_ANTIB:
        g_bag[item]--; P.bleed = 0; P.hp = clampf(P.hp + 10, 0, 100); g_dres_t = 180; snd_play(SFX_BAND, 0.8f, 0);
        msg("Tomas antibióticos: sin sangrado y 15 % de resistencia 3 minutos.");
        return 1;
    case ITEM_D_ALCOHOL:
        if (P.bleed <= 0 && P.hp >= 100) { msg("No estás herido."); return 2; }
        g_bag[item]--; P.bleed = 0; P.hp = clampf(P.hp + 3, 0, 100); snd_play(SFX_BAND, 0.8f, 0);
        msg("Desinfectas la herida: el sangrado se corta.");
        return 1;
    case ITEM_D_CAFE:
        g_bag[item]--; g_dcafe_t = 60; P.stam = 100; snd_play(SFX_DRINK, 0.8f, 0);
        msg("Café caliente: aguante sin límite durante un minuto.");
        return 1;
    case ITEM_D_TABACO:
        g_bag[item]--; P.hunger = clampf(P.hunger + 8, 0, 100); P.stam = 100; P.hp = clampf(P.hp - 1, 1, 100); snd_play(SFX_EAT, 0.6f, 0);
        msg("Fumas: el hambre se calma y recuperas el aguante.");
        return 1;
    case ITEM_D_MAPA:
        if (g_zone != 1) { msg("El mapa viejo se lee en una expedición."); return 2; }
        g_bag[item]--; memset(g_seen, 1, sizeof(g_seen));
        msg("Lees el mapa viejo: la expedición queda marcada en SELECT.");
        return 1;
    case ITEM_D_COMBUS: case ITEM_D_FILTRO:
        msg("Déjalo en el panel de suministros de tu búnker, junto a la escalera.");
        return 2;
    }
    msg("Material: sirve para fabricar y para los pedidos del comerciante.");
    return 2;
}

// Botín extra según el tipo de mapa: fábrica y trenes, herramientas y combustible; pueblo, comida y medicinas; mina, pólvora
static int loot(int item, int n, float x, float *y);
static int duro_extra_loot(int rich, int body, float x, float *y)   // open_crate, antes de la chatarra
{
    static const short IND[] = {ITEM_D_HERR, ITEM_D_COBRE, ITEM_D_COMBUS, ITEM_D_ELEC, ITEM_D_BATERIA, ITEM_D_CINTA, ITEM_D_HERR, ITEM_D_COBRE};
    static const short TWN[] = {ITEM_FOOD, ITEM_D_CAFE, ITEM_D_ANTIB, ITEM_BAND, ITEM_D_ALCOHOL, ITEM_D_TABACO, ITEM_D_CAFE, ITEM_MED};
    static const short MIN[] = {ITEM_POWDER, ITEM_D_COBRE, ITEM_POWDER, ITEM_D_BATERIA, ITEM_D_HERR, ITEM_D_CINTA, ITEM_POWDER, ITEM_D_COMBUS};
    static const short WLD[] = {ITEM_D_TABACO, ITEM_D_ALCOHOL, ITEM_D_MAPA, ITEM_D_CAFE, ITEM_D_FILTRO, ITEM_D_CINTA, ITEM_D_ANTIB, ITEM_D_COBRE};
    const short *pool = WLD;
    int over = 0;
    if (g_zone != 1) return 0;
    switch (g_x.type) {
    case XT_FACTORY: case XT_TRAIN: pool = IND; break;
    case XT_TOWN: pool = TWN; break;
    case XT_MINE: pool = MIN; break;
    case XT_HOSP: case XT_PORT: case XT_ROAD: case XT_NUC: case XT_CURSED: pool = m3_pool(); break;   // [mapas3] [mapnuc]
    default: break;
    }
    float p = (body ? 0.15f : rich ? 1.0f : 0.40f) + 0.02f * g_x.danger;
    if (g_x.type == XT_NUC) p += 0.30f;   // [mapnuc] la planta: botín extra de antirradiación y artefactos
    if (g_x.type == XT_HOSP) p += 0.25f;   // [mapas3] el hospital rebosa de medicinas
    for (int k = 0; k < (rich ? 2 : 1); k++) {
        if (frand() >= p) continue;
        int it = pool[rnd() % 8];
        over |= loot(it, it == ITEM_POWDER || it == ITEM_D_COBRE ? irange(1, 2) : 1, x, y);
    }
    return over;
}

// ---------------------------------------------------------------- suministros del búnker
static int g_bk_rooms;   // (se declara en bunker.inc: aquí solo se lee; la sala del generador es la 7)
static int duro_gen_open(void) { return (g_bk_rooms >> 7) & 1; }
static float duro_amb(void) { return g_zone == ZB && duro_gen_open() && !g_dlit ? 0.4f : 1; }   // generador abierto pero sin combustible: penumbra

static void duro_return(void)   // base_return: cada regreso gasta combustible y rinde el filtro
{
    g_dret++; g_dped_done = 0;
    g_dret_l1[0] = g_dret_l2[0] = 0; g_dret_c1 = g_dret_c2 = 0;
    if (duro_gen_open()) {
        if (g_dfuel > 0) {
            g_dfuel--; g_dlit = 1;
            snprintf(g_dret_l1, sizeof(g_dret_l1), g_dfuel ? "Generador: gastó 1 bidón (quedan %d)." : "Generador: gastó 1 bidón. Ya no quedan: falta combustible.", g_dfuel);
            g_dret_c1 = g_dfuel == 0;
        } else {
            g_dlit = 0;
            snprintf(g_dret_l1, sizeof(g_dret_l1), "Falta combustible: el búnker está a oscuras.");
            g_dret_c1 = 1;
        }
    }
    if (g_dfilt_left <= 0 && g_dfilt > 0) { g_dfilt--; g_dfilt_left = 5; }
    if (g_dfilt_left > 0) {
        g_dfilt_left--;
        if (stash_add(ITEM_WATER, 1)) snprintf(g_dret_l2, sizeof(g_dret_l2), "Filtro de agua: 1 agua limpia en el alijo (dura %d regresos más).", g_dfilt_left);
        else { snprintf(g_dret_l2, sizeof(g_dret_l2), "Filtro de agua: el alijo está lleno."); g_dret_c2 = 1; }
        if (g_dfilt_left == 0 && g_dfilt == 0 && !g_dret_c2) { snprintf(g_dret_l2, sizeof(g_dret_l2), "Filtro de agua: 1 agua limpia; se agotó, falta otro."); g_dret_c2 = 1; }
    }
}
static void duro_ret_draw(void)   // draw_return: dos líneas bajo el huerto
{
    if (g_dret_l1[0]) r_text(130, 144, g_dret_l1, g_dret_c1 ? RGBA(236, 150, 130, 255) : RGBA(150, 214, 130, 255));
    if (g_dret_l2[0]) r_text(130, 154, g_dret_l2, g_dret_c2 ? RGBA(236, 150, 130, 255) : RGBA(130, 190, 240, 255));
}
static void duro_sleep(void)   // sleep_apply, al final: dormir con luz en el búnker cura todo
{
    if (g_zone == ZB && g_dlit) { P.hp = hp_max(); P.bleed = 0; P.rad = 0; msg("Dormiste con el generador encendido: quedas como nuevo."); }
}

// el panel: junto a la escalera del búnker (casilla 30,16)
#define DURO_PX 30
#define DURO_PY 16
static int duro_near_panel(void) { return g_zone == ZB && g_state == GS_PLAY && dist(P.x, P.y, DURO_PX * TS + 8, DURO_PY * TS + 8) < 28; }
static int duro_panel_cross(void)
{
    char s[120];
    if (!duro_near_panel()) return 0;
    int nf = g_bag[ITEM_D_COMBUS], nw = g_bag[ITEM_D_FILTRO];
    if (g_dfuel + nf > 9) nf = 9 - g_dfuel;
    if (g_dfilt + nw > 3) nw = 3 - g_dfilt;
    if (nf < 0) nf = 0;
    if (nw < 0) nw = 0;
    g_bag[ITEM_D_COMBUS] -= nf; g_dfuel += nf; g_bag[ITEM_D_FILTRO] -= nw; g_dfilt += nw;
    if (nf && duro_gen_open() && !g_dlit && g_dfuel > 0) { g_dfuel--; g_dlit = 1; msg("Arrancas el generador: el búnker tiene luz."); return 1; }
    snprintf(s, sizeof(s), "%s Panel: %d bidones, %d filtros de repuesto%s.", nf || nw ? "Dejas lo que traías." : "No traes nada.", g_dfuel, g_dfilt,
             g_dfilt_left ? ", filtro puesto" : "");
    msg(s);
    return 1;
}
static void duro_hud(void)   // el panel dibujado y la barra de la cura
{
    if (g_zone == ZB) {
        float x = DURO_PX * TS - g_ox + 2, y = DURO_PY * TS - g_oy + 1;
        r_rect(x, y, 12, 14, RGBA(60, 64, 70, 255));
        r_rect(x + 1, y + 1, 10, 6, RGBA(30, 34, 40, 255));
        r_rect(x + 2, y + 9, 3, 3, g_dlit ? RGBA(110, 230, 120, 255) : RGBA(220, 80, 70, 255));
        r_rect(x + 7, y + 9, 3, 3, g_dfilt_left ? RGBA(110, 180, 255, 255) : RGBA(90, 90, 96, 255));
        if (duro_near_panel() && !g_opt[OPT_HELP]) r_text_center(DURO_PX * TS + 8 - g_ox, DURO_PY * TS - 18 - g_oy, "Cruz: panel de suministros", RGBA(236, 230, 210, 255));
    }
    if (g_dh_t > 0) {
        float fx = P.x - g_ox - 14, fy = P.y - g_oy - 24, f = 1 - g_dh_t / DM.heal_t;
        r_rect(fx - 1, fy - 1, 30, 5, RGBA(0, 0, 0, 200));
        r_rect(fx, fy, 28 * clampf(f, 0, 1), 3, RGBA(120, 220, 130, 255));
    }
}

static void dped_input(void);
static void dped_draw(void);

#elif DURO_PART == 2
// ---------------------------------------------------------------- pedidos del comerciante
static const short DPED_PRICE[DURO_N] = {35, 22, 50, 40, 16, 45, 18, 16, 14, 40, 55, 65};   // lo mismo que cuestan en la tienda
static int g_dped_sel;
static u32 dped_hash(int i) { u32 h = (u32)(g_dret + 1) * 2654435761u + (u32)(i + 3) * 40503u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
static int dped_item(int i) { return ITEM_D_HERR + (int)((dped_hash(0) % DURO_N + 4 * i) % DURO_N); }   // tres objetos distintos, a 4 de distancia
static int dped_qty(int i) { return 2 + (int)(dped_hash(i + 10) % 4); }                                   // de 2 a 5
static int dped_pay(int i) { return dped_qty(i) * DPED_PRICE[dped_item(i) - ITEM_D_HERR] * 5 / 2; }       // 2,5 veces el precio

static void dped_input(void)
{
    if (in.pressed & PSP_CTRL_UP) g_dped_sel = (g_dped_sel + 2) % 3;
    if (in.pressed & PSP_CTRL_DOWN) g_dped_sel = (g_dped_sel + 1) % 3;
    if (in.pressed & PSP_CTRL_CROSS) {
        int i = g_dped_sel, it = dped_item(i), q = dped_qty(i);
        char s[96];
        if (g_dped_done & (1 << i)) msg("Ese pedido ya está entregado. Cambian en la próxima expedición.");
        else if (inv_count(it) < q) { snprintf(s, sizeof(s), "Te faltan %s: pide %d.", ITEMS[it].many, q); msg(s); }
        else {
            inv_take(it, q);
            inv_add(ITEM_SCRAP, dped_pay(i));
            g_dped_done |= 1 << i;
            g_rep++;
            snd_play(SFX_PICKUP, 0.8f, 0);
            snprintf(s, sizeof(s), "Pedido entregado: +%d chatarra y +1 de reputación.", dped_pay(i));
            msg(s);
        }
    }
}

static void dped_draw(void)
{
    char s[96];
    ui_panel(14, 60, 452, 168, "PEDIDOS");
    r_text(24, 74, "El comerciante necesita esto. Cambia en cada expedición.", UI_DIM);
    for (int i = 0; i < 3; i++) {
        int it = dped_item(i), q = dped_qty(i), done = (g_dped_done >> i) & 1, have = inv_count(it), sel = i == g_dped_sel;
        float y = 90 + i * 44;
        ui_row(22, y, 436, 40, sel);
        Spr ic = SPR[ITEMS[it].icon];
        r_spr(ic, 38 - ic.w / 2, y + 20 - ic.h / 2, done ? RGBA(255, 255, 255, 90) : ITEMS[it].tint);
        snprintf(s, sizeof(s), "%d x %s", q, ITEMS[it].name);
        r_text(58, y + 6, s, done ? UI_DIM : sel ? COL_WHITE : UI_LIGHT);
        snprintf(s, sizeof(s), "Paga %d chatarra y +1 de reputación", dped_pay(i));
        r_text(58, y + 22, s, done ? UI_DIM : RGBA(236, 214, 150, 255));
        if (done) text_right(448, y + 6, "Entregado", RGBA(150, 214, 130, 255));
        else { snprintf(s, sizeof(s), "Llevas %d", have); text_right(448, y + 6, s, have >= q ? RGBA(150, 214, 130, 255) : RGBA(236, 150, 130, 255)); }
    }
}
#endif
