// [diseno] Ambientes, suelo vivo, paletas por tipo, mobiliario de calle y la Ciudad abandonada.
// Se incluye en game.c justo después de mapas.inc. Nada de esto se guarda: todo sale de la semilla de la expedición.

// ------------------------------------------------------------------ reloj y ambientes
// La luz sigue la hora del juego (0 a 24), que avanza dentro de las expediciones: 15 minutos reales = unas 4 horas.
// Partida nueva a las 17:00; dormir en un catre lleva a las 07:00. Encima de la hora hay un clima ocasional por semilla.
static float g_hora = 17;   // se guarda (campo hora al final de SaveData)
static float reloj_hora(void) { return g_hora; }
enum { CL_NADA, CL_NIEBLA, CL_LUNA, CL_LLUVIA };   // [lluvia]
#ifdef PEPEL_DEBUG
static int g_dbg_clima = -1;
#endif
static void dz_llu_tick(float dt);   // [lluvia]
static void dz_tick(float dt)
{
    dz_llu_tick(dt);   // [lluvia]
    if (g_zone == 1 || (g_zone >= 3 && g_zone != ZB)) { g_hora += dt * (4.0f / 900.0f); if (g_hora >= 24) g_hora -= 24; }
}
static int dz_clima(void)
{
#ifdef PEPEL_DEBUG
    if (g_dbg_clima >= 0) return g_dbg_clima;
#endif
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE)) return CL_NADA;
    int r = (int)(((g_x.seed * 2654435761u) >> 20) % 100);
    if (r >= 26 && r < 49 && !(g_x.flags & XF_SNOW)) return CL_LLUVIA;   // [lluvia] cerca de 1 de cada 4, nunca con nieve
    return r < 14 ? CL_NIEBLA : r < 26 ? CL_LUNA : CL_NADA;
}
// [lluvia] Estado: relámpago (segundos de destello que quedan), trueno pendiente y cuánto lleva lloviendo (los charcos crecen con eso)
static float g_rayo, g_trueno = -1, g_rayo_sig = 12, g_llu_t;
static u32 g_llu_rs = 2463534242u;
static int dz_llueve(void) { return g_zone == 1 && dz_clima() == CL_LLUVIA; }
static float dz_llu_rnd(void) { g_llu_rs ^= g_llu_rs << 13; g_llu_rs ^= g_llu_rs >> 17; g_llu_rs ^= g_llu_rs << 5; return (g_llu_rs & 0xFFFF) / 65536.0f; }
static void dz_llu_tick(float dt)
{
    if (!dz_llueve()) { g_rayo = 0; g_trueno = -1; g_llu_t = 0; return; }
    g_llu_t += dt;
    if (g_rayo > 0) g_rayo -= dt;
    if ((g_rayo_sig -= dt) <= 0) { g_rayo = 0.28f; g_trueno = 0.4f + 2.2f * dz_llu_rnd(); g_rayo_sig = 14 + 26 * dz_llu_rnd(); }
    if (g_trueno >= 0 && (g_trueno -= dt) < 0) { snd_play(SFX_BOOM, 0.28f, 0); g_trueno = -1; }   // el trueno llega después de la luz
}
static int dz_fuera(void) { return g_zone == 1 && !(g_x.flags & XF_ZJEFE); }   // la hora solo cuenta en las expediciones
typedef struct { float h, f; int vr, vg, vb, wr, wg, wb, wa; } Franja;
// Puntos de la curva del día: hora, factor de luz, color del velo y lavado de pantalla
static const Franja FR[] = {
    {0, .62f, 5, 6, 12, 0, 0, 0, 0}, {4.6f, .62f, 5, 6, 12, 0, 0, 0, 0}, {5.6f, .85f, 6, 14, 52, 40, 70, 200, 28},
    {7.2f, 1.45f, 40, 44, 52, 120, 130, 140, 14}, {10, 1.9f, 46, 50, 56, 120, 130, 140, 14}, {12, 1.85f, 46, 48, 52, 130, 130, 130, 12},
    {16, 1.45f, 52, 44, 40, 200, 140, 70, 14}, {17.6f, 1.05f, 50, 20, 14, 200, 40, 20, 18}, {19.6f, .78f, 44, 8, 6, 200, 40, 20, 18},
    {21, .62f, 5, 6, 12, 0, 0, 0, 0}, {24, .62f, 5, 6, 12, 0, 0, 0, 0}};
static Franja dz_franja(void)
{
    float h = g_hora;
    int k = 0;
    while (k < (int)(sizeof(FR) / sizeof(FR[0])) - 2 && h >= FR[k + 1].h) k++;
    const Franja *a = &FR[k], *b = &FR[k + 1];
    float t = (h - a->h) / (b->h - a->h);
    if (t < 0) t = 0; else if (t > 1) t = 1;
    Franja o;
#define LRP(c) (a->c + (b->c - a->c) * t)
    o.h = h; o.f = LRP(f); o.vr = (int)LRP(vr); o.vg = (int)LRP(vg); o.vb = (int)LRP(vb); o.wr = (int)LRP(wr); o.wg = (int)LRP(wg); o.wb = (int)LRP(wb); o.wa = (int)LRP(wa);
#undef LRP
    int cl = dz_clima();
    if (cl == CL_LLUVIA) {   // [lluvia] más oscuro y azulado; el relámpago ilumina todo un instante
        o.f *= 0.78f; o.vr = o.vr * 2 / 3; o.vg = (o.vg + 4) / 2; o.vb = (o.vb + 30) / 2 + 8; o.wr = 50; o.wg = 80; o.wb = 130; if (o.wa < 30) o.wa = 30;
        if (g_rayo > 0) o.f = 1.7f;
    }
    else if (cl == CL_NIEBLA) { o.f *= 0.85f; o.vr = (o.vr + 16) / 2; o.vg = (o.vg + 30) / 2; o.vb = (o.vb + 22) / 2; o.wr = 110; o.wg = 150; o.wb = 120; if (o.wa < 54) o.wa = 54; }
    else if (cl == CL_LUNA && o.f < 0.9f) { o.f *= 1.18f; o.vr = 12; o.vg = 34; o.vb = 28; o.wr = 90; o.wg = 170; o.wb = 140; o.wa = 20; }
    return o;
}
// Luz ambiente: fuera de las expediciones queda como la noche de siempre, algo más oscura
static float dz_amb_luz(float amb)
{
    if (!dz_fuera()) return amb * 0.78f;
    return amb * dz_franja().f;
}
static u32 dz_velo(int alpha)
{
    if (!dz_fuera()) return RGBA(5, 6, 12, alpha);
    Franja f = dz_franja();
    return RGBA(f.vr, f.vg, f.vb, alpha);
}
static void dz_lavado(void)
{
    if (!dz_fuera()) return;
    Franja f = dz_franja();
    if (f.wa > 0) r_rect(0, 0, SCR_W, SCR_H, RGBA(f.wr, f.wg, f.wb, f.wa));
    if (g_rayo > 0) r_rect(0, 0, SCR_W, SCR_H, RGBA(235, 240, 255, (int)(120 * g_rayo / 0.28f) / (indoors(P.x, P.y) ? 3 : 1)));   // [lluvia] destello
}
// De noche se ve menos (enemigos y tú); de día, más; la niebla recorta mucho
static float dz_vista(void)
{
    if (!dz_fuera()) return 1.0f;
    float v = 0.70f + 0.225f * dz_franja().f;
    return dz_clima() == CL_NIEBLA ? v * 0.62f : dz_clima() == CL_LLUVIA ? v * 0.88f : v;   // [lluvia]
}
static const char *dz_zname(void)
{
    static char pb[80];
    int hh = (int)g_hora, mm = (int)((g_hora - hh) * 60);
    int cl = dz_clima();
    snprintf(pb, sizeof(pb), "%s%s, %02d:%02d%s", XTNAME[g_x.type], (g_x.flags & XF_SNOW) ? ", nevado" : "", hh, mm, cl == CL_NIEBLA ? ", niebla espesa" : cl == CL_LLUVIA ? ", lluvia" : cl == CL_LUNA && (g_hora >= 21 || g_hora < 4.6f) ? ", luna" : "");
    return pb;
}
static void dz_hud(void)
{
    if (!dz_fuera()) return;
    char s[16];
    snprintf(s, sizeof(s), "%02d:%02d", (int)g_hora, (int)((g_hora - (int)g_hora) * 60));
    r_text_shadow(SCR_W - 8 - r_text_w(s), 18, s, RGBA(200, 196, 188, 255));
}

// ------------------------------------------------------------------ paleta por tipo
// Tinte del suelo y de los muros de cada tipo de mapa
static u32 dz_tinte_suelo(void)
{
    static const unsigned char T[XT_N + 1][3] = {
        [XT_TRAIN] = {236, 222, 206}, [XT_TOWN] = {226, 232, 244}, [XT_FOREST] = {206, 238, 198}, [XT_FACTORY] = {212, 220, 238},
        [XT_GRAVE] = {200, 204, 232}, [XT_MINE] = {244, 216, 182}, [XT_PINE] = {196, 226, 244}, [XT_CITY] = {232, 234, 226}};
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE)) return COL_WHITE;
    if (m3_is(g_x.type)) return m3_tinte();   // [mapas3]
    return RGBA(T[g_x.type][0], T[g_x.type][1], T[g_x.type][2], 255);
}
static u32 dz_tinte_muro(void)
{
    static const unsigned char T[XT_N + 1][3] = {
        [XT_TRAIN] = {232, 200, 176}, [XT_TOWN] = {214, 220, 236}, [XT_FOREST] = {198, 226, 190}, [XT_FACTORY] = {190, 204, 232},
        [XT_GRAVE] = {206, 200, 232}, [XT_MINE] = {236, 206, 164}, [XT_PINE] = {196, 220, 236}, [XT_CITY] = {236, 226, 214}};
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE)) return COL_WHITE;
    if (m3_is(g_x.type)) return m3_tinte();   // [mapas3]
    return RGBA(T[g_x.type][0], T[g_x.type][1], T[g_x.type][2], 255);
}

// ------------------------------------------------------------------ suelo vivo
static int dz_road(int tx, int ty) { return in_map(tx, ty) && g_g[ty][tx] == G_ROAD; }
// Flores, espigas, matas y basura, repartidas por casilla con un hash (no se guarda nada)
static void dz_flora(int tx, int ty, float x, float y, int g)
{
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE) || mn_is(g_x.type)) return;   // [mapnuc]
    u32 h = hash2(tx * 5 + 11, ty * 3 + 7), r = (h >> 3) % 100;
    int t = g_x.type;
    if (g == G_ROAD) {
        if (!(t == XT_CITY || t == XT_TOWN)) return;
        if (r < 6) { const Spr s = SPR[S_DZ_GRS]; r_spr(s, x + (h >> 9) % 10, y + 15 - s.h, COL_WHITE); }
        return;
    }
    if (g != G_GRASS && g != G_DIRT) return;
    int wall = ty > 0 && wall_like(g_o[ty - 1][tx]);
    int pc = (g == G_GRASS ? 20 : 6) + (wall ? 18 : 0);   // % de casillas con algo
    if (r >= pc) return;
    int k = (int)((h >> 12) % 10), s;
    switch (t) {
    case XT_FOREST: s = k < 3 ? S_DZ_FLB : k < 5 ? S_DZ_FLW : k < 7 ? S_DZ_FXP : k < 8 ? S_DZ_FLA : S_DZ_GRS; break;
    case XT_PINE: s = k < 4 ? S_DZ_FXB : k < 6 ? S_DZ_FXP : k < 7 ? S_DZ_FLW : S_DZ_GRS; break;
    case XT_TOWN: case XT_CITY: s = k < 2 ? S_DZ_FLA : k < 3 ? S_DZ_FLB : k < 4 ? S_DZ_FX1 : k < 6 ? S_DZ_GRT : k < 7 ? S_DZ_CAN1 : k < 8 ? S_DZ_CAN2 : S_DZ_GRS; break;
    case XT_TRAIN: case XT_FACTORY: s = k < 2 ? S_DZ_CAN1 : k < 3 ? S_DZ_CAN2 : k < 5 ? S_DZ_BOT : k < 8 ? S_DZ_GRT : S_DZ_GRS; break;
    case XT_GRAVE: s = k < 4 ? S_DZ_FLW : k < 6 ? S_DZ_FXB : S_DZ_GRS; break;
    default: s = k < 3 ? S_DZ_FX1 : k < 5 ? S_DZ_FX2 : k < 7 ? S_DZ_CAN1 : k < 8 ? S_DZ_BOT : S_DZ_GRT; break;   // mina
    }
    const Spr sp = SPR[s];
    r_spr(sp, x + 1 + (h >> 17) % (14 - (sp.w > 12 ? 12 : sp.w) + 1), y + 14 - sp.h + (h >> 21) % 3, COL_WHITE);
}
// Musgo y pasto asomando en las orillas de los caminos
static void dz_musgo(int tx, int ty, float x, float y)
{
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE)) return;
    u32 h = hash2(tx * 9 + 1, ty * 4 + 2);
    int up = dz_road(tx, ty - 1), dn = dz_road(tx, ty + 1), lf = dz_road(tx - 1, ty), rt = dz_road(tx + 1, ty);
    if (up && dn && lf && rt) return;
    for (int k = 0; k < 4; k++) {
        u32 q = (h >> (k * 5)) & 31;
        int px = (int)(q % 14) + 1, py = (int)((q * 7 + k * 3) % 14) + 1;
        if (!up && py < 5) py = 0;
        else if (!dn && py > 10) py = 14;
        else if (!lf && px < 5) px = 0;
        else if (!rt && px > 10) px = 14;
        else continue;
        r_rect(x + px, y + py, 2 + (q & 1), 2, RGBA(70 + (q & 3) * 8, 112 + (q & 7) * 4, 52, 215));
    }
}

// ------------------------------------------------------------------ calles de la ciudad (por código)
// Veredas, líneas del eje y pasos de cebra, solo en el mapa de ciudad
static void dz_calle(int tx, int ty, float x, float y, int g)
{
    if (g_zone != 1 || g_x.type != XT_CITY) return;
    int up = dz_road(tx, ty - 1), dn = dz_road(tx, ty + 1), lf = dz_road(tx - 1, ty), rt = dz_road(tx + 1, ty);
    if (g == G_ROAD) {
        r_rect(x, y, TS, TS, RGBA(34, 36, 44, 78));   // asfalto más oscuro
        if (up && dn && lf != rt) {   // calle de norte a sur
            int zebra = ty % 13 == 6;
            if (zebra) { for (int k = 0; k < 3; k++) r_rect(x + 2 + 5 * k, y + 2, 3, 12, RGBA(228, 226, 214, 190)); }
            else if (ty % 2 == 0) r_rect(lf ? x : x + 15, y + 2, 1, 12, RGBA(222, 196, 92, 210));
        } else if (lf && rt && up != dn) {   // de este a oeste
            int zebra = tx % 17 == 8;
            if (zebra) { for (int k = 0; k < 3; k++) r_rect(x + 2, y + 2 + 5 * k, 12, 3, RGBA(228, 226, 214, 190)); }
            else if (tx % 2 == 0) r_rect(x + 2, up ? y : y + 15, 12, 1, RGBA(222, 196, 92, 210));
        }
    } else if (g == G_GRASS || g == G_DIRT) {   // vereda: franja clara pegada al camino
        u32 vc = RGBA(176, 172, 160, 235), sc = RGBA(110, 106, 98, 235);
        if (rt) { r_rect(x + 10, y, 6, TS, vc); r_rect(x + 10, y, 1, TS, sc); }
        if (lf) { r_rect(x, y, 6, TS, vc); r_rect(x + 5, y, 1, TS, sc); }
        if (dn) { r_rect(x, y + 10, TS, 6, vc); r_rect(x, y + 10, TS, 1, sc); }
        if (up) { r_rect(x, y, TS, 6, vc); r_rect(x, y + 5, TS, 1, sc); }
    }
}
// Bolardos de adorno junto a las veredas (sin colisión: son bajitos)
static void dz_bolardo(int tx, int ty, float x, float y)
{
    if (g_zone != 1 || g_x.type != XT_CITY || g_g[ty][tx] == G_ROAD || g_o[ty][tx] != O_NONE) return;
    if (!(dz_road(tx + 1, ty) || dz_road(tx - 1, ty))) return;
    if (hash2(tx * 3, ty * 5 + 1) % 11 != 0) return;
    const Spr s = SPR[S_DZ_BO0 + (hash2(tx, ty) & 1)];
    r_spr(s, x + (TS - s.w) / 2, y + TS - s.h, COL_WHITE);
}

// ------------------------------------------------------------------ mobiliario de calle
enum { O_DTL = 70, O_DSTOP, O_DCLK, O_DMAP };   // semáforo, señal de pare, reloj de poste y plano del metro (macizos)
enum { CK_VEND = 11 };                          // máquina expendedora: un cajón que se abre una vez
static void dz_draw_obj(int tx, int ty, int o)
{
    int v = (int)(hash2(tx, ty) % 3), s = o == O_DTL ? S_DZ_TL0 : o == O_DSTOP ? S_DZ_ST0 : o == O_DCLK ? S_DZ_CK0 : S_DZ_MP0;
    Spr sp = SPR[s + v];
    r_spr(sp, tx * TS + (TS - sp.w) / 2 - g_ox, (ty + 1) * TS - sp.h - g_oy, COL_WHITE);
}
static void dz_draw_vend(int tx, int ty, int open)
{
    Spr sp = SPR[S_DZ_VD0 + (int)(hash2(tx, ty) % 3)];
    r_spr(sp, tx * TS + (TS - sp.w) / 2 - g_ox, (ty + 1) * TS - sp.h - g_oy, open ? RGBA(150, 150, 150, 255) : COL_WHITE);
}
// La máquina da una conserva o un agua, una sola vez
static void dz_vend_open(Crate *c)
{
    float x = c->tx * TS + 8, y = c->ty * TS - 10;
    c->open = 1;
    snd_play(SFX_CRATE, 0.9f, 0);
    int agua = (hash2(c->tx, c->ty) & 1);
    int got = inv_add(agua ? ITEM_WATER : ITEM_FOOD, 1);
    if (got) { pop(x, y, agua ? "Un agua" : "Una conserva", RGBA(150, 220, 130, 255)); msg(agua ? "La máquina aún tenía una botella de agua." : "La máquina aún tenía una conserva."); }
    else { c->open = 0; msg("La mochila está llena."); }
}
static int dz_libre(int x, int y)   // casilla abierta, de suelo exterior, con casi todo el entorno libre (no cierra pasos)
{
    if (!x_inb(x, y) || g_o[y][x] != O_NONE) return 0;
    int gg = g_g[y][x];
    if (gg != G_DIRT && gg != G_GRASS && gg != G_ROAD) return 0;
    if (abs(x - x_ex) < 3 && abs(y - x_ey) < 3) return 0;   // [arregla] la entrada de la expedición, no P (P sigue en la zona anterior al armar)
    for (int e = 0; e < g_ne; e++) if (abs((int)(g_e[e].x / TS) - x) < 2 && abs((int)(g_e[e].y / TS) - y) < 2) return 0;   // ni encima de un enemigo
    for (int c = 0; c < g_ncr; c++) if (abs(g_cr[c].tx - x) < 2 && abs(g_cr[c].ty - y) < 2) return 0;
    int n = 0;
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) if ((i || j) && g_o[y + j][x + i] == O_NONE && g_g[y + j][x + i] != G_DOOR) n++;
    return n >= 7;
}
// Después de armar el mapa de una expedición: pone el mobiliario donde corresponde
static void dz_post(void)
{
    int t = g_x.type;
    if (g_x.flags & XF_ZJEFE) return;
    int calles = t == XT_CITY || t == XT_TOWN || t == XT_FACTORY || t == XT_TRAIN || t == XT_MINE;
    if (!calles) return;
    int ntl = 0, nst = 0, nvd = 0, nmp = 0;
    int tl_max = t == XT_CITY ? 30 : t == XT_TOWN ? 8 : 4;
    for (int y = 2; y < MH - 2; y++)
        for (int x = 2; x < MW - 2; x++) {
            if (g_g[y][x] == G_ROAD || !dz_libre(x, y)) continue;
            u32 h = hash2(x * 13 + 5, y * 7 + 3);
            int rt = dz_road(x + 1, y), lf = dz_road(x - 1, y), dn = dz_road(x, y + 1), up = dz_road(x, y - 1);
            if (ntl < tl_max && ((rt && dn && dz_road(x + 1, y + 1)) || (lf && dn && dz_road(x - 1, y + 1))) && (t == XT_CITY || t == XT_TOWN)) {
                g_o[y][x] = O_DTL; ntl++; continue;   // semáforo en la esquina del cruce
            }
            if (nst < (t == XT_CITY ? 14 : 6) && (rt || lf) && !up && !dn && h % 61 == 0) { g_o[y][x] = O_DSTOP; nst++; continue; }
            if (nmp < (t == XT_CITY ? 5 : 2) && (rt || lf) && h % 157 == 3) { g_o[y][x] = O_DMAP; nmp++; continue; }
            if (nvd < (t == XT_CITY ? 5 : 3) && g_ncr < MAXCR - 2 && y > 0 && g_o[y - 1][x] == O_WALL && g_g[y][x] != G_ROAD &&
                !(g_g[y][x - 1] == G_DOOR || g_g[y][x + 1] == G_DOOR) && h % 29 == 5) {
                g_o[y][x] = O_CRATE; g_cr[g_ncr++] = (Crate){x, y, CK_VEND, 0}; nvd++; continue;   // máquina junto al edificio
            }
        }
    if (g_plz_x >= 0) {   // plaza: reloj cerca del centro (con su farol) y un plano del metro a cada lado
        int cx = g_plz_x, cy = g_plz_y, hecho = 0;
        for (int k = 0; k < 25 && !hecho; k++) {
            int x = cx + k % 5 - 2, y = cy + k / 5 - 2;
            if (g_o[y][x] == O_NONE && g_g[y][x] == G_ROAD) {
                g_o[y][x] = O_DCLK; hecho = 1; g_plz_x = x; g_plz_y = y;
                if (g_nlamp < MAXLAMP - 4) g_lamp[g_nlamp++] = (Lamp){x * TS + 8, y * TS + 8, 0, 0};
            }
        }
        if (g_o[cy - 3][cx + 4] == O_NONE) g_o[cy - 3][cx + 4] = O_DMAP;
        if (g_o[cy + 3][cx - 4] == O_NONE) g_o[cy + 3][cx - 4] = O_DMAP;
    }
}

// [lluvia] Dibujo: pocas decenas de gotas en pantalla, salpicaduras y charcos fijos al mundo (sin recorrer la grilla)
static void dz_llu_draw(int gotas)   // 0: charcos (antes de la oscuridad), 1: gotas y salpicaduras (después)
{
    if (!dz_llueve() || indoors(P.x, P.y)) return;
    if (gotas) {
    for (int i = 0; i < 44; i++) {
        float p = fmodf(g_time * (2.6f + (i % 4) * 0.3f) + i * 0.377f, 1.0f);
        float x = fmodf(i * 83.3f + 41, SCR_W + 60) - p * 70, y = p * (SCR_H + 24) - 12;
        for (int k = 0; k < 3; k++) r_rect(floorf(x + k * 2), floorf(y - k * 5), 1, 5, RGBA(175, 200, 240, 60 + k * 45));
    }
    for (int j = 0; j < 18; j++) {   // salpicaduras: un anillito que se abre y se apaga
        float ph = fmodf(g_time * 3.1f + j * 0.211f, 1.0f);
        float sx = fmodf(j * 127.3f + floorf(g_time * 3.1f + j * 0.211f) * 71.9f, SCR_W), sy = 40 + fmodf(j * 61.7f + floorf(g_time * 3.1f + j * 0.211f) * 37.1f, SCR_H - 50);
        int a = (int)(150 * (1 - ph)), w = 2 + (int)(ph * 5);
        r_rect(floorf(sx - w), floorf(sy), w * 2, 1, RGBA(190, 210, 240, a));
        if (ph < 0.4f) r_rect(floorf(sx), floorf(sy - 2 - ph * 4), 1, 2, RGBA(210, 225, 250, a));
    }
    return; }
    float k = g_llu_t / 25.0f; if (k > 1) k = 1;   // los charcos aparecen poco a poco
    int cx0 = (int)(g_ox / 96), cy0 = (int)(g_oy / 96);
    for (int cy = cy0; cy <= cy0 + SCR_H / 96 + 1; cy++)
        for (int cx = cx0; cx <= cx0 + SCR_W / 96 + 1; cx++) {
            u32 h = ((u32)(cx * 73856093) ^ (u32)(cy * 19349663) ^ g_x.seed) * 2654435761u;
            if ((h >> 28) > 8 || ((h >> 20) & 15) / 16.0f > k) continue;
            float wx = cx * 96 + 12 + ((h >> 4) & 63), wy = cy * 96 + 12 + ((h >> 10) & 63);
            int g = ground_at(wx, wy);
            if (g != G_ROAD && g != G_DIRT && g != G_GRASS) continue;
            int w = 12 + (int)((h >> 16) & 11), x = (int)(wx - g_ox) - w / 2, y = (int)(wy - g_oy);
            r_rect(x + 2, y - 3, w - 4, 1, RGBA(40, 62, 100, 120));
            r_rect(x, y - 2, w, 3, RGBA(46, 70, 112, 170));
            r_rect(x + 2, y + 1, w - 4, 1, RGBA(40, 62, 100, 120));
            r_rect(x + w / 4, y - 2, w / 3, 1, RGBA(170, 200, 240, 120));   // brillo
        }
}
