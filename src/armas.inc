// armas.inc -- más modificaciones de armas, botar armas y reparar. Se incluye desde game.c ([armas]).
// Las piezas nuevas (ranura SLOT_ACC y otras) están en la tabla PARTS; aquí van sus efectos que no son un multiplicador,
// el botar y recoger armas del suelo (ITEM_WDROP) y la reparación (kit de reparación y chatarrero).

static int g_arm_conf;      // arma (+1) que espera la segunda pulsación de Cuadrado para botarse
static float g_arm_until;   // hasta cuándo vale esa confirmación (g_time)

// El objeto de la pieza puesta en una ranura, o -1
static int armas_part(int wi, int slot)
{
    int p = g_w[wi].part[slot];
    return p && p <= PARTS_N ? PARTS[p - 1].item : -1;
}
static int armas_acc(int wi) { return armas_part(wi, SLOT_ACC); }

static void armas_reset(void)
{
    memset(g_wdrop, 0, sizeof(g_wdrop));
    g_arm_conf = 0;
}

// Linterna táctica: otro cono más largo y ancho encima del de siempre
static void armas_light(void)
{
    if (!is_melee(P.weapon) && armas_acc(P.weapon) == ITEM_LIGHT) add_light(P.x, P.y, 235, 0.9f, P.ang, 0.6f);
}

// La culata acolchada gasta menos el arma
static float armas_wear(int wi) { return armas_part(wi, SLOT_STOCK) == ITEM_PADSTOCK ? 0.6f : 1; }

// Bayoneta: R con el arma de fuego y un enemigo a menos de 24 px da un golpe en vez de disparar
static void armas_bayonet(void)
{
    if (!(in.pressed & PSP_CTRL_RTRIGGER) || is_melee(P.weapon) || armas_acc(P.weapon) != ITEM_BAYO) return;
    if (P.fire_cd > 0 || P.reload_t > 0) return;
    int best = -1;
    float bd = 24;
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e)) continue;
        float d = dist(P.x, P.y, e->x, e->y);
        if (d < bd && los(P.x, P.y, e->x, e->y)) { bd = d; best = i; }
    }
    if (best < 0) return;
    Enemy *e = &g_e[best];
    WeaponState *w = &g_w[P.weapon];
    float a = atan2f(e->y - P.y, e->x - P.x), k = e_mass(e);
    float dm = 24 * (0.6f + 0.4f * w->cond) * (g_sk[SK_CARNI] ? 1.25f : 1) * (e->type == E_ARM ? 1 : 1 - e->armor * 0.5f);
    P.ang = a;
    hurt_enemy(e, dm, P.x, P.y, 1);
    sparks_n(e->x, e->y - 6, 4, 1.1f);
    if (e->state != ES_DEAD) { e->kx = cosf(a) * 40 * k; e->ky = sinf(a) * 40 * k; }
    snd_play(SFX_MEL_HIT, 1, 0);
    noise(P.x, P.y, 25);
    w->cond = clampf(w->cond - 0.01f, 0.05f, 1);
    P.fire_cd = 0.6f;
}

// ---------------------------------------------------------------- botar y recoger armas
static void armas_drop_weapon(int wi)
{
    char s[96];
    int owned = 0, sl = -1;
    if (!g_w[wi].owned) return;
    for (int i = 0; i < W_COUNT; i++) owned += g_w[i].owned;
    if (owned <= 1) { msg("Debes quedarte con al menos un arma."); return; }
    if (g_arm_conf != wi + 1 || g_time > g_arm_until) {   // primera pulsación: pide confirmar
        g_arm_conf = wi + 1;
        g_arm_until = g_time + 3;
        snprintf(s, sizeof(s), "Cuadrado otra vez para botar: %s.", WDEF[wi].name);
        msg(s);
        return;
    }
    g_arm_conf = 0;
    for (int i = 0; i < WDROP_N; i++) if (!g_wdrop[i].used) { sl = i; break; }
    Pick *k = sl >= 0 ? drop(P.x + frange(-5, 5), P.y + frange(0, 4), ITEM_WDROP, sl + 1) : NULL;
    if (!k) { msg("No hay lugar en el suelo."); return; }
    k->on = 3;
    g_wdrop[sl].used = 1; g_wdrop[sl].w = wi; g_wdrop[sl].st = g_w[wi];
    memset(&g_w[wi], 0, sizeof(g_w[wi]));
    g_w[wi].cond = 1;
    if (P.weapon == wi) {
        for (int i = 0; i < W_COUNT; i++) if (g_w[i].owned) { P.weapon = i; break; }
        P.jammed = 0; P.jam_t = 0; P.reload_t = 0; P.fire_cd = 0.25f;
    }
    g_ws_w = P.weapon; g_ws_slot = g_ws_list = g_ws_opt = 0;
    snprintf(s, sizeof(s), "Botaste: %s. Se recoge con Cruz.", WDEF[wi].name);
    msg(s);
}

// Gancho del bucle de lo que hay en el suelo: un arma botada se recoge con Cruz, con su estado y sus piezas
static void armas_wpick(void *pk)
{
    Pick *k = pk;
    char s[96];
    int sl = k->n - 1;
    if (sl < 0 || sl >= WDROP_N || !g_wdrop[sl].used) { k->on = 0; return; }
    WDrop *d = &g_wdrop[sl];
    if (k->on == 1) {
        k->on = 2;
        snprintf(s, sizeof(s), "Cruz: recoger %s.", WDEF[d->w].name);
        msg(s);
    }
    if (!(in.pressed & PSP_CTRL_CROSS)) return;
    if (g_w[d->w].owned) { msg("Ya tienes esa arma."); return; }
    g_w[d->w] = d->st;
    g_w[d->w].owned = 1;
    if (g_w[d->w].mag > w_mag(d->w)) g_w[d->w].mag = w_mag(d->w);
    d->used = 0;
    k->on = 0;
    snprintf(s, sizeof(s), "Recogiste: %s.", WDEF[d->w].name);
    msg(s);
}

// ---------------------------------------------------------------- reparar
static void armas_repair_weapon(int wi)
{
    char s[96];
    if (!g_w[wi].owned) return;
    if (g_w[wi].cond >= 0.995f) { snprintf(s, sizeof(s), "%s ya está al 100 %%.", WDEF[wi].name); msg(s); return; }
    if (g_bag[ITEM_REPAIR] <= 0) { msg("No tienes kits de reparación."); return; }
    g_bag[ITEM_REPAIR]--;
    g_w[wi].cond = 1;
    snprintf(s, sizeof(s), "Reparas %s: queda al 100 %%.", WDEF[wi].name);
    msg(s);
}
static void armas_repair_gear(int slot)
{
    char s[96];
    int gi = slot == EQ_ARMOR || slot == EQ_MASK ? g_eq[slot] - 1 : -1;
    if (gi < 0) { msg("Elige la armadura o la máscara puesta."); return; }
    if (g_gcond[gi] > 99) { snprintf(s, sizeof(s), "%s está como nuevo.", GEAR[gi].name); msg(s); return; }
    if (g_bag[ITEM_REPAIR] <= 0) { msg("No tienes kits de reparación."); return; }
    g_bag[ITEM_REPAIR]--;
    g_gcond[gi] = 100;
    snprintf(s, sizeof(s), "Reparas: %s queda al 100 %%.", GEAR[gi].name);
    msg(s);
}

// El chatarrero repara por chatarra según lo que le falte al arma
static int armas_scrap_cost(int wi) { return g_w[wi].cond >= 0.995f ? 0 : 2 + (int)((1 - g_w[wi].cond) * 20 + 0.99f); }
static void armas_repair_scrap(int wi)
{
    char s[96];
    int c = armas_scrap_cost(wi);
    if (!g_w[wi].owned) return;
    if (!c) { snprintf(s, sizeof(s), "%s ya está al 100 %%.", WDEF[wi].name); msg(s); return; }
    if (g_bag[ITEM_SCRAP] < c) { snprintf(s, sizeof(s), "Reparar cuesta %d de chatarra.", c); msg(s); return; }
    g_bag[ITEM_SCRAP] -= c;
    g_w[wi].cond = 1;
    snprintf(s, sizeof(s), "El chatarrero repara %s por %d de chatarra.", WDEF[wi].name, c);
    msg(s);
}

// Mochila: pestaña ARMAS (Cuadrado bota, Triángulo repara) y panel EQUIPO (Triángulo repara). 1 = ya se atendió la tecla
static int armas_inv_input(void)
{
    if (g_inv_tab == 2 && !g_ws_list) {
        if (!g_w[g_ws_w].owned) g_ws_w = P.weapon;
        if (in.pressed & PSP_CTRL_SQUARE) { armas_drop_weapon(g_ws_w); return 1; }
        if (in.pressed & PSP_CTRL_TRIANGLE) { armas_repair_weapon(g_ws_w); return 1; }
    }
    if (g_inv_tab == 0 && g_inv_focus && (in.pressed & PSP_CTRL_TRIANGLE)) { armas_repair_gear(g_inv_eq); return 1; }
    return 0;
}
// Taller del chatarrero (fuera de la mochila): Cuadrado repara el arma elegida por chatarra
static int armas_scav_input(void)
{
    if (g_state == GS_INV || g_ws_list || !(in.pressed & PSP_CTRL_SQUARE)) return 0;
    armas_repair_scrap(g_ws_w);
    return 1;
}
static void armas_ws_extra(int wi)
{
    char s[48];
    if (g_state == GS_INV) snprintf(s, sizeof(s), "Triáng.: reparar (kits %d)", g_bag[ITEM_REPAIR]);
    else if (armas_scrap_cost(wi)) snprintf(s, sizeof(s), "Cuad.: reparar, %d chatarra", armas_scrap_cost(wi));
    else snprintf(s, sizeof(s), "Arma en perfecto estado");
    r_text(30, 219, s, UI_DIM);
}

// ---------------------------------------------------------------- bosque de pinos (XT_PINE)
// Se llama en el generador de expediciones tras los árboles: suma arbustos (los 'T' que el dibujo muestra como arbusto, que se
// atraviesan) y abre claros redondos en el bosque. Los senderos que serpentean, la cabaña (el primer edificio) y la torre de
// caza (el segundo, de 3 x 3, con mejor botín) los hace el generador común
static void armas_pine(int type, int ey)
{
    if (type != XT_PINE) return;
    for (int y = 3; y < MH - 3; y++)
        for (int x = 3; x < MW - 3; x++) {
            char g = g_zg[y][x];
            if (g_zo[y][x] != ' ' || g == '=' || g == '_' || g == 'D' || g == '~' || g == 'r' || g == 'Z' || x_keep[y][x]) continue;
            if (abs(x - x_ex) < 7 && abs(y - ey) < 7) continue;
            if (hash2(x, y) % 3 == 2 && (int)(brnd() % 100) < 12) g_zo[y][x] = 'T';
        }
    for (int c = 0; c < 5; c++) {   // claros
        int cx = brange(14, MW - 14), cy = brange(8, MH - 9), r = brange(3, 5);
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++)
                if (x_inb(x, y) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r && g_zo[y][x] == 'T') g_zo[y][x] = ' ';
    }
}

// [arregla] Al descartar la expedición: libera los huecos de armas botadas que quedaron en zonas que ya no existen
static void armas_gc(int scan_k)
{
    unsigned char ref[WDROP_N]; memset(ref, 0, sizeof(ref));
    for (int z = 0; z < NZ; z++) {
        if (!g_zc[z].vis) continue;
        for (int i = 0; i < MAXK; i++) { const Pick *k = &g_zc[z].k[i]; if (k->item == ITEM_WDROP && k->on && k->n >= 1 && k->n <= WDROP_N) ref[k->n - 1] = 1; }
    }
    if (scan_k) for (int i = 0; i < MAXK; i++) { const Pick *k = &g_k[i]; if (k->item == ITEM_WDROP && k->on && k->n >= 1 && k->n <= WDROP_N) ref[k->n - 1] = 1; }
    for (int i = 0; i < WDROP_N; i++) if (!ref[i]) g_wdrop[i].used = 0;
}
