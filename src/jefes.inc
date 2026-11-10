// jefes.inc - zonas de jefe (Pantano y Base militar infectada), aliados de la puerta y el perro Hollín en esa batalla.
// Se incluye desde game.c después de draw_person. Los ganchos de game.c son llamadas de una línea.

#define JF_MAXAL 3
typedef struct { int on; float x, y, hp, ang, walk, cd, flash; int moving; } JfAl;
static JfAl g_jfal[JF_MAXAL];
static int g_jf_ask;      // 1 mientras se espera la respuesta en la puerta
static int g_jf_num;      // número de expedición ya decidida (para no preguntar otra vez al volver de un piso)
static int g_jf_fac = F_KOLOS;   // bando que dio el trabajo
#define JF_HP 70.0f

static const char *jf_boss_name(void) { return (g_x.flags & XF_ZLAGO) ? "La Viuda blanca" : (g_x.flags & XF_ZFUERTE) ? "El Capataz" : (g_x.flags & XF_ZVERT) ? "El Fermentado" : (g_x.flags & XF_ZBASE) ? "Cabeza de Hierro" : "La Bestia del charco"; }   // [jefes2]
static const char *jefes_nombre(void) { return (g_x.flags & XF_ZLAGO) ? "Lago helado" : (g_x.flags & XF_ZFUERTE) ? "Fuerte" : (g_x.flags & XF_ZVERT) ? "Vertedero químico" : (g_x.flags & XF_ZBASE) ? "Base militar infectada" : "Pantano"; }   // [jefes2]
static int jf_zona(void) { return g_zone == 1 && (g_x.flags & XF_ZJEFE); }

// Al decidir la expedición: la caza mayor es el Pantano; el trabajo de Cabeza de Hierro es la Base militar. Solo así se llega.
static void jefes_exped(Exped *X)
{
    if (X->flags & XF_BEAST) X->flags |= XF_ZJEFE;
    if (g_job == 8 && !g_job_n) { X->type = XT_FACTORY; X->flags &= ~XF_BEAST; X->flags |= XF_BOSS2 | XF_ZJEFE | XF_ZBASE; }
    if (!(X->flags & XF_ZJEFE)) {   // [jefes2] los tres jefes nuevos: cada uno con su trabajo
        if (g_job == 9 && !g_job_n) { X->type = XT_TOWN; X->flags &= ~(XF_SNOW | XF_BOSS2); X->flags |= XF_ZJEFE | XF_ZFUERTE | XF_BANDITS; }
        else if (g_job == 10 && !g_job_n) { X->type = XT_FACTORY; X->flags &= ~(XF_SNOW | XF_BOSS2); X->flags |= XF_ZJEFE | XF_ZVERT; }
        else if (g_fjob == 4 && !g_fjn) { X->type = XT_FOREST; X->flags &= ~XF_BOSS2; X->flags |= XF_ZJEFE | XF_ZLAGO | XF_SNOW; }
    }
    if (X->flags & XF_ZJEFE) X->flags &= ~XF_DOG;
}

static void jf_clear(void) { memset(g_jfal, 0, sizeof(g_jfal)); g_jf_ask = 0; }
static int jf_alive(void) { int n = 0; for (int i = 0; i < JF_MAXAL; i++) n += g_jfal[i].on; return n; }

// El perro vive en la base: solo sale a la puerta de una zona de jefe
static void jf_dog_home(void)
{
    if (!g_dog.have || g_dog.wounded) return;
    g_dog.home = 1;
    if (g_zone == 0) dog_kennel();
}

// Tras cambiar de zona: en una zona de jefe nueva aparecen los aliados y la pregunta
static void jefes_zona(void)
{
    jf_clear();
    jf_dog_home();
    if (!jf_zona()) return;
    if (g_jf_num == g_x.num) return;   // ya respondida (volviste de un piso)
    g_jf_num = g_x.num;
    g_jf_fac = (g_x.flags & XF_ZLAGO) ? F_FARO : F_KOLOS;   // [jefes2]
    int n = 2 + (int)(rnd() % 2);
    // Los aliados esperan unas casillas más adentro (al este de la entrada), lejos del punto de retirada, para no taparlo
    static const float OX[3] = {44, 44, 62}, OY[3] = {-12, 12, 0};
    for (int i = 0; i < n; i++) {
        JfAl *a = &g_jfal[i];
        a->on = 1; a->x = P.x + OX[i]; a->y = P.y + OY[i]; a->hp = JF_HP; a->ang = P.ang;
        for (int k = 0; k < 3 && solid((int)(a->x / TS), (int)(a->y / TS)); k++) a->x -= 14;   // si cae en muro, se acerca
        if (solid((int)(a->x / TS), (int)(a->y / TS))) { a->x = P.x; a->y = P.y; }
    }
    if (g_dog.have && !g_dog.wounded) { g_dog.x = P.x + 8; g_dog.y = P.y + 14; g_dog.zone = g_zone; }
    g_jf_ask = 1;
}

// Gancho de carga de partida: sin aliados ni pregunta; el perro, en casa
static void jefes_carga(void) { jf_clear(); g_jf_num = g_x.num; jf_dog_home(); }

// Respuesta a la pregunta de la puerta. Devuelve 1 mientras el mundo debe quedar quieto
static int jefes_ask(float dt)
{
    (void)dt;
    if (!g_jf_ask) return 0;
    if (in.pressed & PSP_CTRL_CROSS) {
        g_jf_ask = 0;
        if (g_dog.have && !g_dog.wounded) { g_dog.home = 0; g_dog.st = DG_FOLLOW; g_dog.zone = g_zone; dog_to_player(); }
        msg("Los aliados entran contigo.");
        return 1;   // este cuadro el mundo sigue quieto: la misma Cruz no debe activar el punto de retirada
    } else if (in.pressed & PSP_CTRL_CIRCLE) {
        jf_clear();
        msg("Vuelves a la base. El trabajo sigue asignado.");
        zone_go(0, 5 * TS + 8, 19 * TS + 8, 0);
        return 1;
    }
    return g_jf_ask;
}

static int jf_pick(const JfAl *a)
{
    int w = -1; float bd = 140;
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e) || fr_fac(e->fac)) continue;
        float d = dist(a->x, a->y, e->x, e->y);
        if (d < bd && los(a->x, a->y, e->x, e->y)) { bd = d; w = i; }
    }
    return w;
}

static void jefes_update(float dt)
{
    if (g_state != GS_PLAY || !jf_zona()) return;
    jz_update(dt);   // [jefes2]
    for (int i = 0; i < JF_MAXAL; i++) {
        JfAl *a = &g_jfal[i];
        if (!a->on) continue;
        a->moving = 0; a->cd -= dt; a->flash -= dt;
        if (g_jf_ask) continue;
        float dp = dist(a->x, a->y, P.x, P.y);
        if (dp > 220) { a->x = P.x; a->y = P.y; dp = 0; }
        int t = jf_pick(a);
        if (t >= 0) {
            Enemy *e = &g_e[t];
            a->ang = atan2f(e->y - a->y, e->x - a->x);
            if (a->cd <= 0) {
                a->cd = 0.55f + 0.1f * i;
                hurt_enemy(e, 8, a->x, a->y, 0);
                snd_at(SFX_SHOT_PISTOL, 0.5f, a->x, a->y, 260);
                for (int k = 1; k <= 4; k++) { float f = k * 0.22f; pop(a->x + (e->x - a->x) * f, a->y + (e->y - a->y) * f - 6, ".", RGBA(255, 230, 150, 255)); }
            }
        }
        if (dp > 34 || (t < 0 && dp > 26)) {   // te siguen a corta distancia
            float ox = a->x, oy = a->y, an = atan2f(P.y - a->y + (i - 1) * 10, P.x - a->x + (i - 1) * 10), sp = dp > 90 ? 120 : 78;
            move(&a->x, &a->y, cosf(an) * sp * dt, sinf(an) * sp * dt, 3);
            if (dist(ox, oy, a->x, a->y) < sp * dt * 0.4f) { float s = ((int)(g_time * 3) & 1) ? 1.4f : -1.4f; move(&a->x, &a->y, cosf(an + s) * sp * dt, sinf(an + s) * sp * dt, 3); }
            if (t < 0) a->ang = an;
            a->moving = 1; a->walk += dt * sp / 9;
        }
        // el contacto de los enemigos los hiere
        for (int k = 0; k < g_ne; k++) {
            const Enemy *e = &g_e[k];
            if (e->state != ES_COMBAT || fr_fac(e->fac) || is_post(e)) continue;
            if (dist(a->x, a->y, e->x, e->y) < (is_boss(e) ? 20 : 11)) { a->hp -= (is_boss(e) ? 22 : 9) * dt; a->flash = 0.1f; }
        }
        if (a->hp <= 0) { a->on = 0; pop(a->x, a->y - 14, "Aliado caído", RGBA(230, 120, 110, 255)); }
    }
}

// Al morir un jefe: los aliados se despiden y dan reputación a su bando por cada uno vivo
static void jefes_muere(const Enemy *e)
{
    if (!is_boss(e) || !jf_zona() || e->ev == EVN_POZO) return;   // [arregla]
    if (e->type == E_BOSS2 && g_job == 8 && g_job_n < 1) g_job_n = 1;
    jz_muere(e);   // [jefes2]
    int n = jf_alive();
    if (n > 0) {
        char s[96];
        if (g_jf_fac == F_FARO) g_frep += n; else g_rep += n;
        snprintf(s, sizeof(s), "Los aliados se despiden: +%d de reputación.", n);
        msg(s);
    }
    memset(g_jfal, 0, sizeof(g_jfal));
}

static void jefes_draw_world(void)
{
    jz_draw_world();   // [jefes2]
    for (int i = 0; i < JF_MAXAL; i++) {
        const JfAl *a = &g_jfal[i];
        if (!a->on) continue;
        float x = a->x - g_ox, y = a->y - g_oy;
        if (x < -16 || y < -16 || x > SCR_W + 16 || y > SCR_H + 24) continue;
        draw_person_b(S_GRD_FI0, x, y, a->ang, a->moving, a->walk, WDEF[W_MAKAROV].spr, a->flash > 0 ? RGBA(255, 120, 110, 255) : RGBA(150, 205, 255, 255));
    }
}

static void jefes_draw_ui(void)
{
    if (!g_jf_ask) return;
    char s[96];
    snprintf(s, sizeof(s), "¿Vamos por %s?", jf_boss_name());
    r_rect(60, 96, SCR_W - 120, 56, RGBA(8, 8, 8, 210));
    r_rect(60, 96, SCR_W - 120, 1, RGBA(214, 150, 70, 255));
    r_text_center(SCR_W / 2, 104, jefes_nombre(), RGBA(214, 150, 70, 255));
    r_text_center(SCR_W / 2, 120, s, COL_WHITE);
    r_text_center(SCR_W / 2, 136, "Cruz: sí      Círculo: no", RGBA(200, 200, 190, 255));
}

#ifdef PEPEL_DEBUG
static void dbg_log_line(const char *ln);
// Prueba del generador para las zonas de jefe: 20 mapas de cada una; el jefe está, y él y la entrada son alcanzables. Devuelve cuántos salieron mal
static int jefes_test(void)
{
    static unsigned char reach[MHMAX][MWMAX];
    static short q[MWMAX * MHMAX];
    int mal = 0;
    char ln[160];
    for (int z = 0; z < 5; z++) {   // [jefes2] 0 Pantano, 1 Base, 2 Lago, 3 Fuerte, 4 Vertedero
        int mz = 0;
        for (int i = 0; i < 20; i++) {
            int fl = XF_ZJEFE | XF_SAWED | (i % 3 == 0 ? XF_HATCH : 0) | (z == 0 ? XF_BEAST : z == 1 ? XF_BOSS2 | XF_ZBASE : z == 2 ? XF_ZLAGO | XF_SNOW : z == 3 ? XF_ZFUERTE | XF_BANDITS : XF_ZVERT);   // [jefes2]
            g_x = (Exped){(0x7F4A7C15u * (u32)(i + 7 + 100 * z)) | 1, 2 + i % 10, z == 0 || z == 2 ? XT_FOREST : z == 3 ? XT_TOWN : XT_FACTORY, fl, 3 + i % 6};
            g_x.ev0 = 1 + i % 4;
            zone_build(1);
            if (z == 3) jz_open_gate();   // [jefes2] el portón cerrado se abre al matar a los guardias de afuera
            int ex = g_ret_tx, ey = g_ret_ty, bad = !in_map(ex, ey) || solid(ex, ey);
            memset(reach, 0, sizeof(reach));
            int h = 0, t = 0;
            if (!bad) { reach[ey][ex] = 1; q[t++] = (short)(ey * MW + ex); }
            while (h < t) {
                int c = q[h++], x = c % MW, y = c / MW;
                for (int k = 0; k < 4; k++) {
                    int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
                    if (in_map(nx, ny) && !reach[ny][nx] && !solid(nx, ny)) { reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }
                }
            }
            int nb = 0;
            for (int e = 0; e < g_ne; e++) {
                const Enemy *en = &g_e[e];
                if (en->type != (z == 0 ? E_BOSS : z == 1 ? E_BOSS2 : z == 2 ? E_VIUDA : z == 3 ? E_CAPATAZ : E_FERMEN)) continue;
                nb++;
                int tx = (int)(en->x / TS), ty = (int)(en->y / TS);
                if (!in_map(tx, ty) || !(reach[ty][tx] || reach[ty][tx > 0 ? tx - 1 : 0] || reach[ty][tx + 1 < MW ? tx + 1 : tx] || reach[ty > 0 ? ty - 1 : 0][tx] || reach[ty + 1 < MH ? ty + 1 : ty][tx])) bad = 1;
            }
            if (nb != 1) bad = 1;
            mz += bad;
        }
        mal += mz;
        snprintf(ln, sizeof(ln), "JEFES zona %s: 20 mapas, mal %d\n", z == 0 ? "Pantano" : z == 1 ? "Base militar" : z == 2 ? "Lago helado" : z == 3 ? "Fuerte" : "Vertedero", mz);
        dbg_log_line(ln);
    }
    return mal;
}
#endif
