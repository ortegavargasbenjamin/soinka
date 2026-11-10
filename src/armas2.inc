// [armas2] Armas nuevas, minijefes sueltos y música de fondo. Se incluye desde game.c después de armas.inc.

// Las piezas de las armas nuevas calzan donde calza su arma "hermana"
static void armas2_fits(void)
{
    static const struct { short nueva, base; } H[] = {
        {W_VIBORA, W_MAKAROV}, {W_CHACAL, W_REVOLVER}, {W_ZUMBA, W_SMG}, {W_TORM, W_SMG},
        {W_CUERVO, W_PUMP}, {W_HALCON, W_AK}, {W_CENTINELA, W_BOLT}, {W_APLANA, W_AK},
    };
    for (int i = 0; i < PARTS_N; i++)
        for (unsigned k = 0; k < sizeof(H) / sizeof(H[0]); k++)
            if (PARTS[i].fits & FITS(H[k].base)) PARTS[i].fits |= FITS(H[k].nueva);
}

// ------------------------------------------------------------------ minijefes sueltos [armas2]
// Cuatro enemigos grandes que aparecen solos en las expediciones (1 desde peligro 3, 2 desde peligro 7). Usan la IA de su tipo base
// y suman un ataque propio (mj_update). No son jefes de trabajo: no pasan por is_boss, g_boss_dead ni jefes_muere.
static void mj_spawn_one(int k, float x, float y)
{
    if (g_ne >= MAXE) return;
    spawn_enemy(MJ[k].base, x, y);
    Enemy *e = &g_e[g_ne - 1];
    e->mini = k + 1;
    e->hp = e->hpmax = MJ[k].hp * duro_ehp();
    e->armor = MJ[k].armor;
    e->irr = k == 3;
    if (k == 1) e->var = 1;   // el teniente lleva fusil
    e->call_t = k == 1 || k == 2 ? 0 : 3;
}

// Los coloca en el mapa de la expedición, lejos de la entrada y a cielo abierto
static void mj_place(void)
{
    int n = g_x.danger >= 7 ? 2 : g_x.danger >= 3 ? 1 : 0, first = (int)(rnd() % 4);
    for (int m = 0; m < n; m++) {
        for (int t = 0; t < 400; t++) {
            int tx = 6 + (int)(rnd() % (u32)(MW - 12)), ty = 6 + (int)(rnd() % (u32)(MH - 12));
            if (abs(tx - g_ret_tx) + abs(ty - g_ret_ty) < 45 || abs(tx - g_ret2_tx) + abs(ty - g_ret2_ty) < 25) continue;
            int bad = 0;
            for (int j = -1; j <= 1 && !bad; j++) for (int i = -1; i <= 1; i++) if (solid(tx + i, ty + j) || g_zg[ty + j][tx + i] == '~') { bad = 1; break; }
            float cx = tx * TS + TS / 2, cy = ty * TS + TS / 2;
            if (bad || indoors(cx, cy)) continue;
            mj_spawn_one((first + m * 2) % 4, cx, cy);
            break;
        }
    }
}

static void mj_call(Enemy *e, int type, int n)
{
    for (int i = 0; i < n && g_ne < MAXE - 1; i++) {
        float a = i * 2.1f + frange(0, 1);
        spawn_enemy(type, e->x + cosf(a) * 16, e->y + sinf(a) * 16);
        Enemy *c = &g_e[g_ne - 1];
        c->state = ES_ALERT; c->tx = P.x; c->ty = P.y; c->t = 0;
    }
}

// Devuelve 1 si el ataque especial se queda con el cuadro (no corre la IA normal)
static int mj_update(Enemy *e, int self, float dt)
{
    (void)self;
    if (e->state != ES_COMBAT || g_state != GS_PLAY) return 0;
    float d = dist(e->x, e->y, P.x, P.y), to_p = atan2f(P.y - e->y, P.x - e->x);
    int vis = d < 230 && los(e->x, e->y, P.x, P.y);
    switch (e->mini) {
    case 1:   // embestida
        if (e->atk == BA_WIND) {
            e->ang = turn(e->ang, e->atk_a, 8 * dt);
            if ((e->atk_t -= dt) <= 0) { e->atk = BA_RUSH; e->atk_t = 0.65f; e->burst = 0; }
            return 1;
        }
        if (e->atk == BA_RUSH) {
            float ox = e->x, oy = e->y, st = 215 * dt;
            move(&e->x, &e->y, cosf(e->atk_a) * st, sinf(e->atk_a) * st, 6);
            e->ang = e->atk_a; e->walk += dt * 2; e->moving = 1;
            if (!e->burst && d < 20) { e->burst = 1; hurt_player(26); push_player(e->atk_a, 60); if (g_shake < 7) g_shake = 7; }
            if ((e->atk_t -= dt) <= 0 || dist(ox, oy, e->x, e->y) < st * 0.3f) { e->atk = BA_NONE; e->call_t = frange(4, 6); }
            return 1;
        }
        e->call_t -= dt;
        if (e->call_t <= 0 && vis && d > 55 && d < 200) { e->atk = BA_WIND; e->atk_t = 0.8f; e->atk_a = to_p; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 300); }
        return 0;
    case 2:   // llama a su escolta
    case 3:   // llama a la jauría
        e->call_t -= dt;
        if (e->call_t <= 0 && vis) {
            int vivos = 0, tipo = e->mini == 2 ? E_BAN : E_WOLF;
            for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && g_e[i].type == tipo && !g_e[i].mini && dist(g_e[i].x, g_e[i].y, e->x, e->y) < 180) vivos++;
            if (vivos < 4) {
                mj_call(e, tipo, e->mini == 2 ? 2 : 3);
                snd_at(e->mini == 2 ? SFX_SHOT_PISTOL : SFX_BOSS_ROAR, 1, e->x, e->y, 300);
                msg(e->mini == 2 ? "El teniente llama a su escolta." : "El alfa aúlla: viene la jauría.");
            }
            e->call_t = frange(20, 26);
        }
        return 0;
    case 4:   // radiación alrededor y escupitajos ácidos
        if (d < 110) add_rad(2.4f * dt);
        e->call_t -= dt;
        if (e->call_t <= 0 && vis && d > 45) { e->ang = to_p; e_spit(e); e->call_t = frange(3, 4.5f); }
        return 0;
    }
    return 0;
}

// Botín de un minijefe: caja militar o arma nueva, piezas y, a veces, un artefacto
static void mj_loot(Enemy *e)
{
    Crate t = {0, 0, CK_MIL, 0};
    weapon_find(&t, e->x, e->y);
    static const int pieza[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK};
    drop(e->x - 9, e->y + 6, pieza[rnd() % 5], 1);
    drop(e->x + 9, e->y + 6, pieza[rnd() % 5], 1);
    drop(e->x, e->y + 10, rnd() & 1 ? ITEM_762_AP : ITEM_9MM_HP, irange(6, 10));
    drop(e->x - 12, e->y - 4, ITEM_12G_SLUG, irange(3, 5));
    drop(e->x + 12, e->y - 4, ITEM_SCRAP, irange(18, 30));
    if (frand() < 0.3f) drop(e->x, e->y - 12, ITEM_ART6 + (int)(rnd() % 4), 1);
    msg("El minijefe cae y deja un buen botín.");
}

// ------------------------------------------------------------------ música [armas2]
// Suena en el menú y en la base; en las expediciones, más baja. El volumen propio está en Opciones ("Música").
static void music_tick(void)
{
    int fuera = g_state != GS_TITLE && g_zone != 0;
    snd_music_level(fuera ? 0.30f : 0.65f);
#ifdef PEPEL_DEBUG
    static int logged;
    if (!logged && snd_music_state() == 2) { logged = 1; dbg_logf("T musica: music.raw abierto y leyendo"); }
#endif
}
