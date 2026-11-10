// jefes2.inc - tres jefes más con su zona: La Viuda blanca (Lago helado), El Capataz (Fuerte) y El Fermentado (Vertedero químico).
// Se incluye desde game.c después de jefes.inc. Los ganchos de game.c llevan la marca [jefes2].

enum { JA_NONE, JA_WIND, JA_RUSH, JA_HOWL = 5, JA_BITE, JA_SPIT, JA_AIM, JA_BURST, JA_NADE };   // JA_WIND y JA_RUSH coinciden con BA_WIND y BA_RUSH (así sale la línea roja del aviso)
static int jz_gate[3][2], jz_wx0;   // Fuerte: portón abierto, sus casillas y la columna del muro oeste
typedef struct { float x, y, t; int on; } JzGr;
static JzGr jz_gr[6];
static float jz_cvx, jz_cvy, jz_px, jz_py, jz_fog;   // resbalón del hielo y niebla del vertedero
#define JZ_EY 40   // fila del camino de entrada
static void draw_enemy(const Enemy *e);

static int jz_kind(void) { return g_zone == 1 ? (g_x.flags & (XF_ZLAGO | XF_ZFUERTE | XF_ZVERT)) : 0; }
static void jz_put(char c, int x, int y) { if (x_inb(x, y) && g_zo[y][x] == ' ' && g_zg[y][x] != 'Z') g_zo[y][x] = c; }
static void jz_enemy(char c, int x, int y) { if (x_inb(x, y) && g_zo[y][x] == ' ' && g_zg[y][x] != 'Z') { g_zo[y][x] = c; x_nen++; } }
// reparte n cosas al azar fuera del camino central (las filas del camino quedan libres: así todo se alcanza)
static void jz_sc(char c, int n, int x0, int x1, int y0, int y1, int enemy)
{
    for (int t = 0; t < 600 && n > 0; t++) {
        int x = brange(x0, x1), y = brange(y0, y1);
        if (y >= JZ_EY - 2 && y <= JZ_EY + 3) continue;
        if (x < 9 || x > MW - 14 || !x_inb(x, y) || g_zg[y][x] == '=' || g_zg[y][x] == 'Z') continue;
        if (g_zo[y][x] != ' ') continue;
        if (enemy) x_nen++;
        g_zo[y][x] = c;
        n--;
    }
}
static float jz_q(int x, int y, float cx, float cy, float ra, float rb) { float dx = (x - cx) / ra, dy = (y - cy) / rb; return dx * dx + dy * dy; }

static int jz_gen(void)
{
    int kind = g_x.flags & (XF_ZLAGO | XF_ZFUERTE | XF_ZVERT);
    if (!kind) return 0;
    const int ey = JZ_EY;
    g_brs = (g_x.seed ^ 0x51ED270Bu) | 1;
    memset(x_keep, 0, sizeof(x_keep));
    x_nb = x_nt = x_nd = x_nen = x_pud = g_nstb = 0; g_x_fix = 0;
    x_rx = 6; x_ry = 5; x_emin2 = 169;
    memset(jz_gr, 0, sizeof(jz_gr)); jz_open = 0;
    for (int y = 0; y < MH; y++) { memset(g_zg[y], '.', MW); memset(g_zo[y], ' ', MW); g_zg[y][MW] = 0; g_zo[y][MW] = 0; }
    for (int x = 0; x < MW; x++) { g_zo[0][x] = g_zo[MH - 1][x] = 'T'; if (brnd() % 100 < 45) g_zo[1][x] = g_zo[MH - 2][x] = 'T'; }
    for (int y = 0; y < MH; y++) { g_zo[y][0] = g_zo[y][MW - 1] = 'T'; if (brnd() % 100 < 45) g_zo[y][1] = g_zo[y][MW - 2] = 'T'; }
    x_ex = 3; x_ey = ey;
    for (int y = ey - 2; y <= ey + 3; y++) for (int x = 1; x <= 8; x++) g_zo[y][x] = ' ';
    for (int y = ey - 2; y <= ey + 3; y++) for (int x = MW - 12; x < MW - 1; x++) g_zo[y][x] = ' ';
    int road = kind == XF_ZFUERTE ? 58 : kind == XF_ZLAGO ? 33 : MW - 14;
    x_fill(1, ey, road, ey + 1, '=');
    x_fill(2, ey, 3, ey + 1, 'Z');
    x_fill(MW - 12, ey, MW - 5, ey + 1, '=');   // el segundo punto de retirada, en el extremo este
    x_fill(MW - 6, ey, MW - 5, ey + 1, 'Z');
    g_ret2_tx = MW - 6; g_ret2_ty = ey;

    if (kind == XF_ZLAGO) {   // lago congelado en el centro, orillas con rocas (sacos) y pinos muertos
        const float cx = 64, cy = 40, ra = 30, rb = 18;
        for (int y = 2; y < MH - 2; y++)
            for (int x = 2; x < MW - 2; x++) {
                float q = jz_q(x, y, cx, cy, ra, rb);
                if (q <= 1.0f) { g_zg[y][x] = 'I'; continue; }
                if (g_zg[y][x] == '=' || (y >= ey - 2 && y <= ey + 3)) continue;
                if (q <= 1.6f && brnd() % 7 == 0) jz_put('s', x, y);
                else if (q > 1.4f && q < 4 && brnd() % 9 == 0) jz_put('T', x, y);
            }
        jz_sc('s', 6, 40, 90, 27, 53, 0);   // rocas sueltas sobre el hielo
        for (int k = 0; k < 3; k++) jz_put('F', 40 + k * 24, 24 + (k & 1) * 32);
        jz_enemy('!', (int)cx, ey);
        for (int k = 0; k < 5; k++)
            for (int t = 0; t < 80; t++) {
                int x = brange(36, 92), y = brange(24, 56);
                float q = jz_q(x, y, cx, cy, ra, rb);
                if (y >= ey - 2 && y <= ey + 3) continue;
                if (q > 0.45f && q < 1.2f && g_zo[y][x] == ' ') { jz_enemy('9', x, y); break; }
            }
        jz_sc('c', 4, 36, 94, 22, 58, 0); jz_sc('r', 2, 36, 94, 22, 58, 0);
    } else if (kind == XF_ZFUERTE) {   // muros de contenedores, portón al oeste, torres y patio con el Capataz
        const int x0 = 60, x1 = 112, y0 = 22, y1 = 58;
        jz_wx0 = x0;
        for (int x = x0; x <= x1; x++) g_zo[y0][x] = g_zo[y1][x] = (x & 1) ? 'K' : 'C';
        for (int y = y0; y <= y1; y++) g_zo[y][x0] = g_zo[y][x1] = (y & 1) ? 'K' : 'C';
        for (int k = 0; k < 3; k++) { jz_gate[k][0] = x0; jz_gate[k][1] = ey - 1 + k; g_zo[ey - 1 + k][x0] = 'K'; }
        // torres en las esquinas y chatarra por fuera
        static const int tw[8][2] = {{59, 21}, {59, 59}, {113, 21}, {113, 59}, {110, 24}, {110, 56}, {62, 24}, {62, 56}};
        for (int k = 0; k < 8; k++) jz_put('O', tw[k][0], tw[k][1]);
        static const int outs[10][2] = {{47, 36}, {47, 45}, {55, 36}, {55, 45}, {52, 33}, {52, 47}, {57, 28}, {57, 52}, {44, 30}, {44, 50}};
        for (int k = 0; k < 10; k++) jz_put('s', outs[k][0], outs[k][1]);
        jz_put('F', 52, 35); jz_put('F', 52, 45); jz_put('L', 58, 37); jz_put('L', 58, 44);
        static const int ga[5][2] = {{50, 36}, {50, 44}, {54, 34}, {54, 46}, {46, 40}};
        for (int k = 0; k < 5; k++) jz_enemy('a', ga[k][0], ga[k][1]);
        jz_enemy('8', 56, 30); jz_enemy('8', 56, 50);
        // el patio: contenedores de cobertura (las filas del camino quedan libres), barriles, fuegos y faroles
        static const int cov[12][2] = {{68, 29}, {69, 29}, {68, 51}, {69, 51}, {80, 32}, {81, 32}, {80, 48}, {81, 48}, {92, 35}, {92, 36}, {92, 45}, {92, 46}};
        for (int k = 0; k < 12; k++) jz_put((k & 1) ? 'K' : 'C', cov[k][0], cov[k][1]);
        static const int bar[6][2] = {{75, 34}, {75, 46}, {86, 29}, {86, 51}, {100, 30}, {100, 50}};
        for (int k = 0; k < 6; k++) jz_put('b', bar[k][0], bar[k][1]);
        jz_put('F', 66, 32); jz_put('F', 66, 48); jz_put('F', 104, 34); jz_put('F', 104, 46);
        jz_put('L', 72, 26); jz_put('L', 72, 54); jz_put('L', 96, 26); jz_put('L', 96, 54);
        static const int ia[4][2] = {{84, 34}, {84, 46}, {96, 37}, {96, 44}};
        for (int k = 0; k < 4; k++) jz_enemy('a', ia[k][0], ia[k][1]);
        jz_enemy('8', 108, 27); jz_enemy('8', 108, 53);
        jz_put('r', 110, 37); jz_put('r', 110, 44); jz_put('c', 64, 27); jz_put('c', 64, 53); jz_put('c', 76, 27); jz_put('c', 76, 53);
        jz_enemy('%', 105, ey);
    } else {   // vertedero: barriles, charcos de ácido, niebla verde que irradia
        const float cx = 72, cy = 40, ra = 24, rb = 16;
        for (int y = 3; y < MH - 3; y++)
            for (int x = 3; x < MW - 3; x++) {
                float q = jz_q(x, y, cx, cy, ra, rb);
                if (q <= 1 && g_zg[y][x] != '=' && brnd() % 100 < 38) g_zg[y][x] = '~';
            }
        jz_sc('b', 55, 12, 116, 4, 76, 0);
        jz_sc('C', 8, 12, 116, 4, 76, 0); jz_sc('K', 6, 12, 116, 4, 76, 0);
        for (int k = 0; k < 4; k++) jz_put('F', 50 + k * 16, k & 1 ? 30 : 50);
        jz_put('L', 56, 34); jz_put('L', 88, 46); jz_put('L', 72, 28);
        jz_enemy('$', (int)cx, ey);
        jz_sc('d', 6, 34, 110, 8, 72, 1); jz_sc('m', 3, 34, 110, 8, 72, 1); jz_sc('6', 3, 40, 110, 8, 72, 1);
        jz_sc('c', 4, 14, 114, 6, 74, 0); jz_sc('r', 2, 40, 114, 6, 74, 0);
    }
    g_ret_tx = x_ex; g_ret_ty = x_ey;
    g_nev = 0; g_dogx = -1; g_fx = -1; g_ev_ntk = 0;
    g_x_nen = x_nen; g_x_nbld = 0;
    x_n = x_flood();
    return 1;
}

static void jz_open_gate(void)
{
    if (jz_open) return;
    jz_open = 1;
    for (int k = 0; k < 3; k++) { int x = jz_gate[k][0], y = jz_gate[k][1]; g_zo[y][x] = ' '; g_o[y][x] = O_NONE; }
}

static void jz_spawn(Enemy *e)
{
    if (e->type == E_CAPATAZ) { e->fac = F_BAND; e->var = 1; }
}

static const char *jz_name(const Enemy *b, const char *def)
{
    return b->type == E_VIUDA ? "La Viuda blanca" : b->type == E_CAPATAZ ? "El Capataz" : b->type == E_FERMEN ? "El Fermentado" : def;
}

// El Capataz se dibuja como el bandido del fusil (más un resplandor naranja que lo marca como jefe)
static int jz_draw(const Enemy *e)
{
    if (e->type != E_CAPATAZ) return 0;
    Enemy c = *e;
    c.type = E_BAN; c.var = 1;
    if (e->state != ES_DEAD) r_glow(e->x - g_ox, e->y - g_oy - 8, 24, RGBA(255, 150, 60, 50), RGBA(255, 150, 60, 0));
    draw_enemy(&c);
    return 1;
}

// [arregla] Los invocados se crean de antemano (muertos y escondidos) al armar la zona, y en la pelea se reutilizan: así g_ne no cambia
static void jz_reserve(int type, int n, float x, float y)
{
    for (int k = 0; k < n && g_ne < MAXE; k++) {
        spawn_enemy(type, x, y);
        Enemy *r = &g_e[g_ne - 1];
        r->summon = 2; r->hp = 0; r->state = ES_DEAD; r->dead_t = 0;
    }
}
static Enemy *jz_use(int type, float x, float y)
{
    for (int i = 0; i < g_ne; i++) {
        Enemy *w = &g_e[i];
        if (w->type != type || w->summon != 2 || w->state != ES_DEAD || (w->dead_t > 0 && g_time - w->dead_t < 1.5f)) continue;
        memset(w, 0, sizeof(*w));
        w->type = type; w->summon = 2;
        w->x = w->hx = w->tx = x; w->y = w->hy = w->ty = y;
        w->ang = frange(0, 2 * PI); w->hp = EHP[type] * duro_ehp(); w->armor = ARMOR[type];
        w->strafe = rnd() & 1 ? 1 : -1; w->t = frange(0.5f, 3); w->fac = fac_of(type); w->var = i & 1;
        jz_spawn(w);
        return w;
    }
    return NULL;
}

static void jz_wolves(Enemy *b, int n)
{
    int alive = 0;
    for (int i = 0; i < g_ne; i++) alive += g_e[i].type == E_WOLF && g_e[i].state != ES_DEAD;
    for (int k = 0; k < n && alive < 7 && g_ne < MAXE; k++) {
        float a = b->ang + 1.0f + 2.1f * k, x = b->x + cosf(a) * 26, y = b->y + sinf(a) * 26;
        if (box_hits(x, y, 5)) { x = b->x; y = b->y; }
        Enemy *w = jz_use(E_WOLF, x, y);   // [arregla] se reutiliza un hueco reservado
        if (!w) break;
        w->state = ES_COMBAT; w->tx = P.x; w->ty = P.y; w->cd = 0.4f;
        alive++;
    }
}

static void jz_reinf(Enemy *b)
{
    for (int k = 0; k < 3 && g_ne < MAXE; k++) {
        float x = b->x - 34 - 12 * k, y = b->y + (k - 1) * 22;
        if (box_hits(x, y, 5)) { x = b->x - 16; y = b->y; }
        Enemy *w = jz_use(E_BAN, x, y);   // [arregla]
        if (!w) continue;
        w->state = ES_COMBAT; w->tx = P.x; w->ty = P.y; w->cd = 0.6f;
    }
    msg("El Capataz llama a sus hombres.");
}

static void jz_ai(Enemy *e, int self, float dt)
{
    float d, fx, fy;
    int ti = foe_pick(e, self, e->state == ES_IDLE ? 190 : 300, 0, &d);
    int sees = ti != -2;
    if (!sees) { fx = P.x; fy = P.y; d = dist(e->x, e->y, fx, fy); }
    else { fx = foe_x(ti); fy = foe_y(ti); }
    float to = atan2f(fy - e->y, fx - e->x);
    const int vi = e->type == E_VIUDA, ca = e->type == E_CAPATAZ;
    if (e->state == ES_IDLE) {
        e->t -= dt;
        if (dist(e->x, e->y, e->tx, e->ty) > 4) e_move(e, e->tx, e->ty, 12, dt, 1);
        else if (e->t <= 0) { e->t = frange(2, 5); e->tx = e->hx + frange(-24, 24); e->ty = e->hy + frange(-18, 18); }
        if (sees) { e->state = ES_COMBAT; e->cd = 1.0f; e->call_t = vi ? 3.0f : 0; e->atk = JA_NONE; e->see_t = 0; snd_at(vi ? SFX_GROWL : SFX_BOSS_ROAR, 1, e->x, e->y, 320); }
        return;
    }
    if (e->state == ES_ALERT) {
        e->t += dt;
        e_chase(e, e->tx, e->ty, 44, dt, 1);
        if (sees) { e->state = ES_COMBAT; e->cd = 0.8f; e->see_t = 0; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 320); }
        else if (e->t > 8 || dist(e->x, e->y, e->tx, e->ty) < 8) { e->state = ES_IDLE; e->tx = e->hx; e->ty = e->hy; e->t = 2; }
        return;
    }
    if (sees) { e->see_t = 0; e->tx = fx; e->ty = fy; }
    else if (e->atk == JA_NONE && (e->see_t += dt) > 6) { e->state = ES_ALERT; e->t = 0; return; }
    e->call_t -= dt;
    if (ca && !e->mob && e->hp < EHP[E_CAPATAZ] * 0.5f) { e->mob = 1; jz_reinf(e); }
    if (e->type == E_FERMEN && !e->var && e->hp < EHP[E_FERMEN] * 0.5f) {   // se parte en dos mitades más chicas
        e->var = 1; e->armor = 0.1f; e->hp = EHP[E_FERMEN] * 0.22f;
        float sx = e->x + 14, sy = e->y;
        if (box_hits(sx, sy, 6)) { sx = e->x; sy = e->y; }
        Enemy *n = jz_use(E_FERMEN, sx, sy);   // [arregla] hueco reservado
        if (n) {
        n->var = 1; n->armor = 0.1f; n->hp = e->hp; n->state = ES_COMBAT; n->tx = fx; n->ty = fy; n->cd = 0.8f; n->summon = 2;
        acid_splat(e->x, e->y); acid_splat(sx, sy);
        e->atk = JA_NONE; e->cd = 0.8f;
        msg("El Fermentado se parte en dos.");
        }   // [arregla]
    }
    switch (e->atk) {
    case JA_NONE:
        e->cd -= dt;
        if (sees) e->ang = turn(e->ang, to, (ca ? 4 : 5) * dt);
        if (vi) {
            if (d > 24 || !sees) e_chase(e, e->tx, e->ty, 100, dt, 1);
            if (!sees || e->cd > 0) break;
            if (e->call_t <= 0) { e->atk = JA_HOWL; e->atk_t = 0.9f; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 360); }
            else if (d < 30) { e->atk = JA_BITE; e->atk_t = 0.3f; }
            else if (d > 50 && d < 280) { e->atk = JA_WIND; e->atk_t = 0.65f; e->atk_a = to; e->burst = 0; }
        } else if (ca) {
            if (e->strafe_t > 0) e->strafe_t -= dt; else { e->strafe_t = frange(1.2f, 2.6f); e->strafe = -e->strafe; }
            float sa = to + 1.5708f * e->strafe;   // se mueve de lado, sin alejarse del patio
            if (dist(e->x, e->y, e->hx, e->hy) < 70 && d > 40) { e->moving = 1; e->walk += dt * 2; move(&e->x, &e->y, cosf(sa) * 24 * dt, sinf(sa) * 24 * dt, 5); }
            else e_move(e, e->hx, e->hy, 30, dt, 0);
            if (!sees || e->cd > 0) break;
            if (d > 55 && (rnd() % 100) < 40) { e->atk = JA_NADE; e->atk_t = 0.7f; e->tx = fx; e->ty = fy; }
            else { e->atk = JA_AIM; e->atk_t = 0.9f; }
        } else {   // Fermentado: lento, escupe en abanico y deja charcos
            if (d > 40 || !sees) e_chase(e, e->tx, e->ty, e->var ? 38 : 28, dt, 1);
            if (e->moving && (e->burst_t -= dt) <= 0) { e->burst_t = 1.1f; acid_splat(e->x, e->y); }
            if (d < 20 && (e->strafe_t -= dt) <= 0) { e->strafe_t = 0.9f; foe_hit(e, ti, 10); if (ti == -1) add_rad(5); }
            if (!sees || e->cd > 0) break;
            if (d < 280) { e->atk = JA_SPIT; e->atk_t = 0.7f; }
        }
        break;
    case JA_WIND:
        e->ang = turn(e->ang, e->atk_a, 9 * dt);
        if ((e->atk_t -= dt) <= 0) { e->atk = JA_RUSH; e->atk_t = 0.75f; }
        break;
    case JA_RUSH: {
        float ox = e->x, oy = e->y, st = 290 * dt;
        move(&e->x, &e->y, cosf(e->atk_a) * st, sinf(e->atk_a) * st, 6);
        e->ang = e->atk_a; e->walk += dt * 2.4f; e->moving = 1;
        if (!e->burst && sees && d < 22) {
            e->burst = 1;
            foe_hit(e, ti, 20);
            if (ti == -1 && g_state == GS_PLAY) push_player(e->atk_a, 70);
            if (g_shake < 7) g_shake = 7;
        }
        if ((e->atk_t -= dt) <= 0 || dist(ox, oy, e->x, e->y) < st * 0.3f) { e->atk = JA_NONE; e->cd = 0.9f; }
        break;
    }
    case JA_HOWL:   // el aullido: llama a los lobos y, si estás cerca, te deja helado un instante
        if ((e->atk_t -= dt) <= 0) {
            if (d < 150 && g_state == GS_PLAY && ti == -1) { g_pstun = 0.6f; msg("El aullido te deja helado."); }
            jz_wolves(e, 3);
            if (g_shake < 6) g_shake = 6;
            e->call_t = frange(11, 15); e->cd = 0.8f; e->atk = JA_NONE;
        }
        break;
    case JA_BITE:
        if ((e->atk_t -= dt) <= 0) { if (dist(e->x, e->y, fx, fy) < 36) foe_hit(e, ti, 16); e->cd = 0.6f; e->atk = JA_NONE; }
        break;
    case JA_SPIT:   // abanico de ácido
        e->ang = turn(e->ang, to, 5 * dt);
        if ((e->atk_t -= dt) <= 0) {
            int n = e->var ? 3 : 5;
            for (int k = 0; k < n; k++) { e->ang = to + (k - (n - 1) * 0.5f) * 0.3f; e_spit(e); }
            e->ang = to;
            e->cd = e->var ? 1.8f : 2.3f; e->atk = JA_NONE;
        }
        break;
    case JA_AIM:   // aviso de la ráfaga: se ve la línea
        e->ang = turn(e->ang, to, 5 * dt);
        if ((e->atk_t -= dt) <= 0) { e->atk = JA_BURST; e->atk_t = 2.4f; e->burst_t = 0; snd_at(SFX_BOSS_ROAR, 0.8f, e->x, e->y, 300); }
        break;
    case JA_BURST:
        e->ang = turn(e->ang, to, 1.1f * dt);
        if ((e->burst_t -= dt) <= 0) { e->burst_t = e->mob ? 0.08f : 0.11f; e_fire(e, 0.06f, 250, 1.4f, 6.0f); }
        if ((e->atk_t -= dt) <= 0) { e->atk = JA_NONE; e->cd = e->mob ? 1.2f : 1.8f; }
        break;
    case JA_NADE:
        if ((e->atk_t -= dt) <= 0) {
            for (int k = 0; k < 2; k++)
                for (int i = 0; i < 6; i++)
                    if (!jz_gr[i].on) { jz_gr[i] = (JzGr){e->tx + (k ? frange(-28, 28) : 0), e->ty + (k ? frange(-28, 28) : 0), 1.1f + 0.25f * k, 1}; break; }
            snd_at(SFX_WHOOSH, 0.9f, e->x, e->y, 300);
            e->atk = JA_NONE; e->cd = 2.0f;
        }
        break;
    }
    float dp = dist(e->x, e->y, P.x, P.y);   // no encimarse con el jugador
    if (dp < 12 && dp > 0.01f) { float push = (12 - dp) * 0.5f; move(&e->x, &e->y, -(P.x - e->x) / dp * push, -(P.y - e->y) / dp * push, 5); }
}

// Lo que corre cada cuadro en la zona: portón del Fuerte, granadas, hielo, niebla y charcos
static void jz_update(float dt)
{
    int k = jz_kind();
    if (!k) return;
    if (k == XF_ZFUERTE) {
        if (!jz_open) {
            int out = 0;
            for (int i = 0; i < g_ne; i++) { const Enemy *e = &g_e[i]; if ((e->type == E_BAN || e->type == E_SNP) && e->state != ES_DEAD && e->hx < jz_wx0 * TS) out++; }
            if (!out) { jz_open_gate(); msg("El portón del Fuerte se abre."); }
        }
        for (int i = 0; i < 6; i++) {
            JzGr *g = &jz_gr[i];
            if (!g->on) continue;
            if ((g->t -= dt) <= 0) { g->on = 0; boom_at(g->x, g->y, 46, 28, 50); }
        }
    }
    if (k == XF_ZLAGO) {   // el hielo resbala: el movimiento se suaviza
        int tx = (int)(P.x / TS), ty = (int)(P.y / TS);
        float dx = P.x - jz_px, dy = P.y - jz_py;
        if (in_map(tx, ty) && g_zg[ty][tx] == 'I' && dt > 0 && dist(P.x, P.y, jz_px, jz_py) < 12) {
            jz_cvx = jz_cvx * 0.965f + dx * 0.035f; jz_cvy = jz_cvy * 0.965f + dy * 0.035f;
            move(&P.x, &P.y, jz_cvx - dx, jz_cvy - dy, 4);
        } else jz_cvx = jz_cvy = 0;
        jz_px = P.x; jz_py = P.y;
    }
    if (k == XF_ZVERT) {
        static const int pu[4][2] = {{62, 36}, {82, 44}, {72, 31}, {72, 49}};
        for (int i = 0; i < 4; i++) {   // charcos permanentes
            int j = 6 + i;
            g_pud[j].x = pu[i][0] * TS + 8; g_pud[j].y = pu[i][1] * TS + 8; if (g_pud[j].life < 3) g_pud[j].life = 6;
        }
        if (jz_q((int)(P.x / TS), (int)(P.y / TS), 72, 40, 24, 16) <= 1 && g_state == GS_PLAY) {   // la niebla verde irradia
            jz_fog += dt;
            if (jz_fog >= 1.0f) { jz_fog = 0; add_rad(mask_on() >= 0 ? 0.8f : 2.0f); }
        }
    }
}

static void jz_draw_world(void)
{
    int k = jz_kind();
    if (!k) return;
    for (int i = 0; i < 6; i++) {
        const JzGr *g = &jz_gr[i];
        if (!g->on) continue;
        float x = g->x - g_ox, y = g->y - g_oy, f = clampf(1 - g->t / 1.35f, 0, 1);
        r_glow(x, y, 46, RGBA(255, 60, 30, (int)(30 + 120 * f)), RGBA(255, 60, 30, 20));
        r_rect(floorf(x) - 2, floorf(y) - 2, 4, 4, RGBA(60, 70, 50, 255));
    }
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD) continue;
        float x = e->x - g_ox, y = e->y - g_oy;
        if (x < -40 || y < -40 || x > SCR_W + 40 || y > SCR_H + 40) continue;
        if (e->type == E_CAPATAZ && (e->atk == JA_AIM || e->atk == JA_BURST)) {
            float f = e->atk == JA_AIM ? clampf(1 - e->atk_t / 0.9f, 0, 1) : 1;
            r_line(x, y - 8, x + cosf(e->ang) * 150, y - 8 + sinf(e->ang) * 150, 2 + 3 * f, RGBA(255, 60, 30, (int)(50 + 110 * f)));
        }
        if (e->type == E_VIUDA && e->atk == JA_HOWL) r_glow(x, y - 12, 54, RGBA(150, 210, 255, 90), RGBA(150, 210, 255, 0));
        if (e->type == E_FERMEN) r_glow(x, y - 8, 28, RGBA(150, 255, 70, 60), RGBA(150, 255, 70, 0));
    }
    if (k == XF_ZVERT && jz_q((int)(P.x / TS), (int)(P.y / TS), 72, 40, 24, 16) <= 1.2f) {   // la niebla: manchas verdes que flotan
        for (int i = 0; i < 6; i++) {
            float a = g_time * 0.25f + i * 1.05f, x = SCR_W / 2 + cosf(a) * (60 + 20 * i), y = SCR_H / 2 + sinf(a * 1.3f) * (40 + 10 * i);
            r_glow(x, y, 50, RGBA(110, 255, 90, 26), RGBA(110, 255, 90, 0));
        }
    }
}

static void jz_loot(const Enemy *e, int main_item, int n_main)
{
    static const int pieza[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK};
    drop(e->x - 10, e->y + 2, ITEM_SCRAP, irange(30, 45));
    drop(e->x + 10, e->y + 2, ITEM_SCRAP, irange(20, 30));
    drop(e->x, e->y + 10, main_item, n_main);
    drop(e->x - 14, e->y + 9, ITEM_9MM_HP, irange(8, 12));
    drop(e->x - 5, e->y - 8, pieza[rnd() % 5], 1);
    drop(e->x + 6, e->y - 7, pieza[rnd() % 5], 1);
    drop(e->x, e->y + 17, ITEM_MECH, irange(2, 4));
    drop(e->x + 12, e->y - 9, ITEM_MED, 1);
    award_weapon(-1, e->x, e->y);
}

// Al morir uno de los tres: botín, trofeo y el trabajo queda hecho
static void jz_muere(const Enemy *e)
{
    if (e->type < E_VIUDA) return;
    if (e->type == E_FERMEN) {   // el Fermentado cae cuando mueren todas sus mitades
        for (int i = 0; i < g_ne; i++) if (g_e[i].type == E_FERMEN && g_e[i].state != ES_DEAD) return;
        jz_loot(e, ITEM_CHEM, irange(4, 6));
        drop(e->x + 14, e->y + 8, ITEM_BKD_T5, 1);
        if (g_job == 10 && !g_job_n) g_job_n = 1;
        msg("El Fermentado ha caído.");
    } else if (e->type == E_CAPATAZ) {
        jz_loot(e, ITEM_762_AP, irange(10, 14));
        drop(e->x + 14, e->y + 8, ITEM_12G_SLUG, irange(4, 6));
        drop(e->x + 8, e->y + 8, ITEM_BKD_T4, 1);
        if (g_job == 9 && !g_job_n) g_job_n = 1;
        msg("El Capataz ha caído.");
    } else {
        jz_loot(e, ITEM_FOOD, irange(3, 5));
        drop(e->x + 14, e->y + 8, ITEM_762, irange(6, 10));
        drop(e->x + 8, e->y + 8, ITEM_BKD_T3, 1);
        if (g_fjob == 4 && !g_fjn) g_fjn = 1;
        msg("La Viuda blanca ha caído.");
    }
}
