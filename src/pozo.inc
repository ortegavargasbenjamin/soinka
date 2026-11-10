// [pozo] El Pozo (búnker profundo por pisos), niveles infernales (peligro 10 a 15 tras el final) y reliquias.
// Se incluye desde game.c después de open_crate. El piso 1 es el búnker generado de siempre (zona 2); del 2 en adelante son
// la misma zona 2 armada con una semilla fija por piso (bunker_gen) y más dura cuanto más abajo. Cada 5 pisos hay un
// ascensor (Cruz: guarda el piso; Cruz otra vez: sube a la escotilla) y un guardián. En la escotilla de una expedición,
// L + Cruz baja directo al último ascensor guardado.

#define PZ_KEEP 4
static int g_pz_to, g_pz_dir, g_pz_dx = -1, g_pz_dy = -1, g_pz_lx = -1, g_pz_ly = -1;
static float g_pz_stim;
static ZoneCache g_pzc[PZ_KEEP];
static int g_pzc_fl[PZ_KEEP];

static int pz_floor(void) { return g_pz >= 2 ? g_pz : 1; }
static u32 pz_seed(int f) { return ((u32)f * 2654435761u ^ 0x9E3779B9u) | 1; }
static int pz_inf(void) { return g_zone == 1 && g_x.danger >= 10; }   // expedición infernal

enum { PR_JAM, PR_ABRIGO, PR_FILTRO, PR_ESTIM, PR_AMULETO, PR_N };
static const short PZ_REL[PR_N] = {ITEM_R_CARGADOR, ITEM_R_ABRIGO, ITEM_R_FILTRO, ITEM_R_ESTIM, ITEM_R_AMULETO};
static int has_rel(int i) { return g_bag[PZ_REL[i]] > 0; }

static const char *pozo_nombre(void)
{
    static char pb[40];
    snprintf(pb, sizeof(pb), "Pozo, piso %d", g_pz);
    return pb;
}

// En el Pozo pide piso liso ('_'); afuera (expediciones infernales) sirve cualquier suelo alcanzable sin objeto: el pasto y la nieve no tienen '_'
static int pz_any;   // 1 mientras se colocan jefes sueltos en la superficie (g_zone aún no es la zona que se arma)
static int pz_cell_ok(int x, int y) { return x_inb(x, y) && x_reach[y][x] && g_zo[y][x] == ' ' && (pz_any || g_zg[y][x] == '_'); }

// Después de armar el piso: escalera de bajada (la casilla alcanzable con muro arriba más lejana), ascensor cada 5 pisos y,
// del piso 2 en adelante, más enemigos, más fuertes y cajones ricos
static void pozo_decor(void)
{
    const int d = g_pz >= 2 ? g_pz : 1;
    g_pz_dx = g_pz_dy = g_pz_lx = g_pz_ly = -1;
    x_ex = g_bun_ex; x_ey = g_bun_ey;
    x_flood();
    int bd = -1, bx = -1, by = -1;
    for (int y = 3; y < MH - 1; y++)
        for (int x = 2; x < MW - 2; x++) {
            if (!pz_cell_ok(x, y) || g_zo[y - 1][x] != '#' || g_zo[y - 1][x - 1] == '4' || g_zo[y - 1][x + 1] == '4') continue;
            int s = abs(x - x_ex) + abs(y - x_ey);
            if (s > bd) { bd = s; bx = x; by = y; }
        }
    if (bx >= 0) { g_zo[by - 1][bx] = '4'; g_pz_dx = bx; g_pz_dy = by; }
    if (d >= 2 && d % 5 == 0) {   // el ascensor, contra el muro cerca de la entrada
        int bs = 999;
        for (int y = 3; y < MH - 1; y++)
            for (int x = 2; x < MW - 2; x++) {
                if (!pz_cell_ok(x, y) || g_zo[y - 1][x] != '#' || (x == bx && y == by)) continue;
                int s = abs(x - x_ex) + abs(y - x_ey);
                if (s < 3 || s > 12 || !x_ring_open(x, y)) continue;
                if (abs(s - 6) < bs) { bs = abs(s - 6); g_pz_lx = x; g_pz_ly = y; }
            }
        if (g_pz_lx >= 0) g_zo[g_pz_ly][g_pz_lx] = 'G';
    }
    if (d < 2) return;
    // enemigos: los del piso suben de categoría con la profundidad
    for (int y = 1; y < MH - 1; y++)
        for (int x = 1; x < MW - 1; x++) {
            char c = g_zo[y][x];
            if (c != 'd' && c != 'm') continue;
            int r = (int)(hash2(x + 31 * d, y + 7 * d) % 100);
            if (r < 3 * d && d >= 8) c = '7';
            else if (r < 5 * d && d >= 4) c = '6';
            else if (r < 7 * d && d >= 3) c = '5';
            else if (r < 9 * d) c = 'm';
            if (c == 'm' && d >= 6 && (int)(hash2(x * 3, y * 5 + d) % 100) < 3 * d - 12) c = 'B';
            g_zo[y][x] = c;
        }
    g_brs = pz_seed(d) ^ 0x5bd1e995u;
    int extra = 2 * d + 4; if (extra > 28) extra = 28;
    for (int t = 0; t < 600 && extra > 0; t++) {
        int x = brange(2, MW - 3), y = brange(2, MH - 3);
        if (!pz_cell_ok(x, y) || abs(x - x_ex) + abs(y - x_ey) < 9 || (abs(x - bx) < 2 && abs(y - by) < 2)) continue;
        int r = (int)(brnd() % 100);
        g_zo[y][x] = d >= 8 && r < 15 ? '7' : d >= 5 && r < 35 ? '6' : r < 30 ? '5' : r < 65 ? 'm' : 'd';
        extra--;
    }
    int rich = 1 + d / 4; if (rich > 6) rich = 6;   // más cajones ricos, contra el muro
    for (int t = 0; t < 400 && rich > 0; t++) {
        int x = brange(2, MW - 3), y = brange(3, MH - 3);
        if (!pz_cell_ok(x, y) || g_zo[y - 1][x] != '#' || !x_ring_open(x, y)) continue;
        g_zo[y][x] = 'r'; rich--;
    }
}

// Después de poner a los enemigos: vida según la profundidad, guardián de cada 5 pisos, y jefes sueltos en las expediciones infernales
static int pz_spot(int cx, int cy, int rmin, int rmax)   // una casilla alcanzable a esa distancia (manhattan) de (cx,cy); devuelve y*256+x o -1
{
    for (int r = rmin; r <= rmax; r++)
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++)
                if (abs(x - cx) + abs(y - cy) == r && pz_cell_ok(x, y)) return y * 256 + x;
    return -1;
}
static void pozo_post(int z)
{
    if (z == 2 && g_pz >= 2) {
        const int d = g_pz;
        float k = 1 + 0.06f * d; if (k > 4) k = 4;
        for (int i = 0; i < g_ne; i++) g_e[i].hp *= k;
        if (d % 5 == 0) {   // el guardián, cerca de la bajada
            x_ex = g_bun_ex; x_ey = g_bun_ey; x_flood();
            int p = g_pz_dx >= 0 ? pz_spot(g_pz_dx, g_pz_dy, 3, 8) : -1;
            if (p >= 0 && g_ne < MAXE) {
                int n0 = g_ne;
                spawn_enemy((d / 5) % 2 ? E_BOSS : E_BOSS2, (p % 256) * TS + 8, (p / 256) * TS + 8);
                if (g_ne > n0) { g_e[n0].ev = EVN_POZO; g_e[n0].hp *= 1 + 0.1f * d; }
            }
        }
    } else if (z == 1 && g_fin_seen && g_x.danger >= 10) {
        const int n = g_x.danger - 9;
        for (int i = 0; i < g_ne; i++) g_e[i].hp *= 1 + 0.1f * n;
        int nb = g_x.danger >= 12 ? 2 : 1, has1 = 0, has2 = 0;
        for (int i = 0; i < g_ne; i++) { has1 |= g_e[i].type == E_BOSS; has2 |= g_e[i].type == E_BOSS2; }
        x_ex = g_ret_tx; x_ey = g_ret_ty; x_flood();
        pz_any = 1;
        for (int b = 0; b < nb; b++) {
            int ty = b ? E_BOSS2 : E_BOSS;
            if ((ty == E_BOSS && has1) || (ty == E_BOSS2 && has2)) continue;
            if (g_ne > MAXE - 4) g_ne = MAXE - 4;   // sin sitio: el jefe ocupa el lugar de los últimos enemigos (y queda aire para el otro)
            int placed = 0;
            for (int t = 0; t < 600 && !placed; t++) {
                int x = 4 + (int)(rnd() % (u32)(MW - 8)), y = 4 + (int)(rnd() % (u32)(MH - 8));
                if (!pz_cell_ok(x, y) || abs(x - x_ex) + abs(y - x_ey) < (t < 250 ? 45 : t < 350 ? 20 : 6)) continue;
                int n0 = g_ne;
                spawn_enemy(ty, x * TS + 8, y * TS + 8);
                if (g_ne > n0) { g_e[n0].ev = EVN_POZO; g_e[n0].hp *= 1 + 0.1f * n; placed = 1; }
            }
            if (!placed) {   // último recurso: una casilla alcanzable cerca del punto de retirada del este
                int p = pz_spot(g_ret2_tx, g_ret2_ty, 3, 12);
                if (p >= 0) { int n0 = g_ne; spawn_enemy(ty, (p % 256) * TS + 8, (p / 256) * TS + 8); if (g_ne > n0) g_e[n0].ev = EVN_POZO; g_e[n0].hp *= 1 + 0.1f * n; }
            }
        }
        pz_any = 0;
    }
}

// ---- memoria de los pisos mientras estás abajo (se borra al salir del Pozo)
static void pozo_stash(void)
{
    int f = pz_floor(), s = f % PZ_KEEP;
    ZoneCache *c = &g_pzc[s];
    g_pzc_fl[s] = f;
    c->vis = 1; c->ncr = g_ncr; c->ne = g_ne;
    memcpy(c->cr, g_cr, sizeof(Crate) * g_ncr);
    memcpy(c->e, g_e, sizeof(Enemy) * g_ne);
    memcpy(c->k, g_k, sizeof(g_k));
    memcpy(c->seen, g_seen, sizeof(g_seen));
}
static int pozo_restore(void)   // 1 = ya quedó resuelto (con lo recordado o recién armado); 0 = que siga el camino de siempre
{
    int f = pz_floor(), s = f % PZ_KEEP;
    const ZoneCache *c = &g_pzc[s];
    if (g_pzc_fl[s] == f && c->vis && c->ncr == g_ncr && c->ne == g_ne) {
        memcpy(g_cr, c->cr, sizeof(Crate) * g_ncr);
        memcpy(g_e, c->e, sizeof(Enemy) * g_ne);
        memcpy(g_k, c->k, sizeof(g_k));
        memcpy(g_seen, c->seen, sizeof(g_seen));
        return 1;
    }
    return g_pz >= 2 && !g_pz_fresh;
}

static void pozo_switch(void)   // antes de armar la zona de destino
{
    if (g_zto != 2) { memset(g_pzc_fl, 0, sizeof(g_pzc_fl)); for (int i = 0; i < PZ_KEEP; i++) g_pzc[i].vis = 0; }
    g_pz = g_zto == 2 && g_pz_to >= 2 ? g_pz_to : 0;
    g_pz_fresh = 0;
}
static void pozo_arrive(float *x, float *y)   // sube desde el piso de abajo: junto a la bajada
{
    if (g_zto == 2 && g_pz_dir > 0 && g_pz_dx >= 0) { *x = g_pz_dx * TS + 8; *y = g_pz_dy * TS + 8; }
}
static void pozo_after(void)
{
    g_pz_dir = 0; g_pz_to = 0;
    if (g_zone == 2 && g_pz == 0 && g_pz_lift >= 5) { char s[80]; snprintf(s, sizeof(s), "Ascensor al piso %d: en la escotilla, L + Cruz.", g_pz_lift); msg(s); }
}
static void pozo_go(int fl, int dir)
{
    g_pz_to = fl; g_pz_dir = dir;
    zone_go(2, 0, 0, 0);
}

// Cruz junto a la bajada, la subida del Pozo, el ascensor o la escotilla (con L): devuelve 1 si lo resolvió
static int pozo_cross(int aim)
{
    if (g_state != GS_PLAY || (g_zone != 1 && g_zone != 2)) return 0;
    int px = (int)(P.x / TS), py = (int)(P.y / TS), best = 0;
    float bd = 26;
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++) {
            if (!in_map(x, y)) continue;
            int o = g_o[y][x], k = g_zone == 2 ? (o == O_STDN ? 1 : o == O_STAIRS && g_pz >= 2 ? 2 : o == O_MACH ? 3 : 0) : (o == O_HATCH ? 4 : 0);
            if (!k) continue;
            float d = dist(P.x, P.y, x * TS + 8, y * TS + 8);
            if (d < bd) { bd = d; best = k; }
        }
    if (best == 1) { pozo_go(pz_floor() + 1, -1); return 1; }
    if (best == 2) { pozo_go(g_pz - 1, 1); return 1; }
    if (best == 4) {
        if (aim && g_pz_lift >= 5) { pozo_go(g_pz_lift, -1); return 1; }
        return 0;
    }
    if (best == 3) {
        if (g_pz_lift < g_pz) {
            char s[96];
            g_pz_lift = g_pz;
            snprintf(s, sizeof(s), "Ascensor del piso %d guardado. Cruz otra vez: subir a la escotilla.", g_pz);
            msg(s);
        } else { zone_go(1, -1, -1, 0); g_zx_hatch = 1; }
        return 1;
    }
    return 0;
}

// ---- botín: más tiradas con la profundidad y reliquias en cajones ricos de peligro 10+ y en los guardianes
static void pz_reliq(float x, float *y)
{
    int cand[PR_N], n = 0;
    for (int i = 0; i < PR_N; i++) if (!g_bag[PZ_REL[i]] && !g_stash[PZ_REL[i]]) cand[n++] = i;
    if (!n) { loot(ITEM_SCRAP, 40, x, y); return; }
    int it = PZ_REL[cand[rnd() % (u32)n]];
    loot(it, 1, x, y);
    msg("Una reliquia. Mientras la lleves en la mochila, actúa sola.");
}
static void pozo_extra_loot(const Crate *c, float x, float *y)
{
    if (g_zone == 2 && g_pz >= 2) {
        int n = g_pz / 4; if (n > 5) n = 5;
        for (int i = 0; i < n; i++) { const LootDef *l = loot_roll(0); loot(l->item, l->lo < l->hi ? irange(l->lo, l->hi) : l->lo, x, y); }
        if (c->kind == CK_RICH && g_pz >= 6) loot(rnd() % 2 ? ITEM_MED : ITEM_9MM_AP, 2, x, y);
    }
    if (c->kind == CK_RICH && g_fin_seen && pz_inf() && rnd() % 100 < 35) pz_reliq(x, y);
}
static void pozo_muere(const Enemy *e)
{
    if (is_boss(e) && g_zone == 2 && g_pz >= 2 && g_pz % 5 == 0) {
        float y = e->y;
        pz_reliq(e->x, &y);
        drop(e->x + 8, e->y + 10, ITEM_SCRAP, 20 + g_pz);
    }
}

// ---- daño: más fuerte abajo y en las expediciones infernales (+10 % por nivel sobre 9)
static float pozo_dmg(void)
{
    if (g_zone == 2 && g_pz >= 2) { float k = 1 + 0.03f * (g_pz - 1); return k > 2.5f ? 2.5f : k; }
    if (pz_inf()) return 1 + 0.1f * (g_x.danger - 9);
    return 1;
}
static int pozo_revive(void)   // el amuleto: una vez
{
    if (!has_rel(PR_AMULETO)) return 0;
    g_bag[ITEM_R_AMULETO] = 0;
    P.hp = 60; P.bleed = 0; P.rad = 0;
    g_shake = 6;
    msg("El amuleto se rompe y te devuelve a la vida.");
    return 1;
}
static void pozo_update(float dt)
{
    if (g_state != GS_PLAY) return;
    if (g_pz_stim > 0) {   // estimulante activo: el aguante rinde el doble
        g_pz_stim -= dt;
        P.stam = clampf(P.stam + 15 * dt, 0, 100);
    } else if (has_rel(PR_ESTIM) && P.stam < 15) {
        g_bag[ITEM_R_ESTIM] = 0; g_pz_stim = 120;
        msg("Estimulante: el aguante rinde el doble por 2 minutos.");
    }
}
static void pozo_danger(Exped *X)
{
    if (!g_fin_seen) return;
    if (g_pz_inf < 6) g_pz_inf++;
    X->danger = 9 + g_pz_inf;
}
static void pozo_hud(void)
{
    if (g_zone == 2 && g_pz >= 2 && g_state == GS_PLAY) r_text(SCR_W - 92, 18, pozo_nombre(), RGBA(236, 214, 150, 230));
    else if (pz_inf() && g_state == GS_PLAY) { char s[24]; snprintf(s, sizeof(s), "Peligro %d", g_x.danger); r_text(SCR_W - 70, 18, s, RGBA(230, 90, 70, 230)); }
}

#ifdef PEPEL_DEBUG
// Prueba: 30 pisos del Pozo (bajada y subida alcanzables, ascensor y guardián cada 5) y 30 expediciones infernales (peligro 10 a 15). Resumen en dbg_log.txt
static void pozo_test(void)
{
    int bad = 0, ok = 0, sv = g_pz, fsv = g_fin_seen, neA = 0, neB = 0, guard = 0, lifts = 0, shown = 0, ibad = 0, iok = 0;
    char ln[220];
    Exped keep = g_x;
    unsigned t0 = sceKernelGetSystemTimeLow(), tmax = 0;
    for (int f = 2; f <= 31; f++) {
        g_pz = f;
        unsigned ta = sceKernelGetSystemTimeLow();
        zone_build(2);
        ta = sceKernelGetSystemTimeLow() - ta; if (ta > tmax) tmax = ta;
        int why = 0, nb = 0;
        x_ex = g_bun_ex; x_ey = g_bun_ey; x_flood();
        if (g_pz_dx < 0 || g_o[g_pz_dy - 1][g_pz_dx] != O_STDN || !x_reach[g_pz_dy][g_pz_dx]) why |= 1;   // bajada alcanzable
        if (g_o[g_bun_ey - 1][g_bun_ex] != O_STAIRS || !x_reach[g_bun_ey][g_bun_ex]) why |= 2;            // subida alcanzable
        for (int i = 0; i < g_ne; i++) nb += is_boss(&g_e[i]);
        if (f % 5 == 0) {
            lifts++;
            if (g_pz_lx < 0 || g_o[g_pz_ly][g_pz_lx] != O_MACH || !x_near(g_pz_lx, g_pz_ly)) why |= 4;   // ascensor
            if (nb != 1) why |= 8;                                                                          // guardián
            guard += nb;
        } else if (nb) why |= 16;
        if (g_ne < 1 || g_ne >= MAXE) why |= 32;
        if (f == 2) neA = g_ne;
        if (f == 31) neB = g_ne;
        if (why) { bad++; if (shown++ < 5) { snprintf(ln, sizeof(ln), "POZO mal piso %d: %d\n", f, why); dbg_log_line(ln); } } else ok++;
    }
    snprintf(ln, sizeof(ln), "POZO 30 pisos: bien %d mal %d | enemigos piso 2 %d piso 31 %d | ascensores %d guardianes %d | max %u ms por piso, total %u ms\n", ok, bad, neA, neB, lifts, guard, tmax / 1000, (sceKernelGetSystemTimeLow() - t0) / 1000);
    dbg_log_line(ln);
    g_fin_seen = 1;
    for (int i = 0; i < 30; i++) {   // expediciones infernales
        g_x = (Exped){(0x9E3779B1u * (u32)(i + 77)) | 1, 20 + i, i % XT_N, XF_HATCH, 10 + i % 6};
        g_x.ev0 = 1 + i % 4;
        zone_build(1);
        int nb1 = 0, nb2 = 0;
        for (int k = 0; k < g_ne; k++) { nb1 += g_e[k].type == E_BOSS; nb2 += g_e[k].type == E_BOSS2; }
        int need = g_x.danger >= 12 ? 2 : 1;
        if (nb1 + nb2 < need || g_ne >= MAXE) {
            ibad++;
            snprintf(ln, sizeof(ln), "INFERNAL mal: i %d peligro %d tipo %d jefes %d+%d ne %d\n", i, g_x.danger, g_x.type, nb1, nb2, g_ne);
            dbg_log_line(ln);
        } else iok++;
    }
    snprintf(ln, sizeof(ln), "INFERNAL 30 expediciones peligro 10 a 15: bien %d mal %d\n", iok, ibad);
    dbg_log_line(ln);
    g_fin_seen = fsv; g_pz = sv; g_x = keep;
}
#endif

// [arregla] Partida nueva o cargada: los pisos del Pozo vuelven a empezar
static void pozo_clear(void)
{
    memset(g_pzc_fl, 0, sizeof(g_pzc_fl));
    for (int i = 0; i < PZ_KEEP; i++) g_pzc[i].vis = 0;
    g_pz_stim = 0;
}
