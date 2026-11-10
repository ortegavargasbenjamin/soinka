// [urbe] La ciudad y su metro. Bocas de metro en la superficie (O_STUP marcado en g_mt_x/g_mt_y), la zona ZM (el metro, se arma por
// semilla como el segundo piso de una casa: sin caché, solo máscaras de lo abierto y lo muerto), fachadas altas de la ciudad.
enum { O_UCOL = 140, O_UBEN, O_UTRN, O_UBTH, O_UBUS, O_UPAR, O_UKIO };   // [qurbe] + bus abandonado (4 casillas), paradero y kiosco de la calle   // columna, banca, torniquete y boletería del metro (macizos)
#define MT_MAX 3
static int g_nmt, g_mt_x[MT_MAX], g_mt_y[MT_MAX];            // bocas arriba
static int g_mt_sx[MT_MAX], g_mt_sy[MT_MAX], g_mt_ns;        // escalera de cada estación y cuántas hay
static int g_mt_cur;                                         // por cuál boca se bajó
static float g_mt_bx, g_mt_by;                               // dónde se quedó el jugador arriba (para guardar estando abajo)
static unsigned char g_mt_open[MAXCR], g_mt_dead[MAXE];      // memoria del metro: cajones abiertos y enemigos muertos
static short g_mt_deco[260][3]; static int g_mt_nd;          // decorado del metro: x, y, objeto (o -1 = máquina expendedora)
static short g_mt_red[24][2]; static int g_mt_nred;          // luces rojas de emergencia
static unsigned char g_ub_h[MHMAX][MWMAX], g_ub_c[MHMAX][MWMAX];   // fachada alta: pisos extra y color, por casilla del muro de arriba
static int g_mt_ex, g_mt_ey;                                 // llegada a la estación 0 (para las pruebas)
static void exped_gen(void);

static void urbe_reset(void) { memset(g_mt_open, 0, sizeof(g_mt_open)); memset(g_mt_dead, 0, sizeof(g_mt_dead)); }
static int urbe_mouth_at(int tx, int ty) { for (int k = 0; k < g_nmt; k++) if (g_mt_x[k] == tx && g_mt_y[k] == ty) return k; return -1; }

// En la superficie de la ciudad: 2 o 3 bocas junto a una calle, lejos de la entrada y entre sí, siempre alcanzables
static void urbe_mouths(void)
{
    g_nmt = 0;
    if (g_x.type != XT_CITY) return;
    x_n = x_flood();
    int want = 2 + (int)(brnd() % 2);
    for (int t = 0; t < 3000 && g_nmt < want; t++) {
        int x = brange(4, MW - 5), y = brange(4, MH - 5), ok = 1;
        if (g_zg[y][x] == '=' || g_zg[y][x] == '_' || g_zg[y][x] == 'D' || !x_free(x, y) || !x_reach[y][x]) continue;
        if (!(g_zg[y][x - 1] == '=' || g_zg[y][x + 1] == '=' || g_zg[y - 1][x] == '=' || g_zg[y + 1][x] == '=')) continue;
        if ((x - x_ex) * (x - x_ex) + (y - x_ey) * (y - x_ey) < 400) continue;
        for (int k = 0; k < g_nmt; k++) if ((x - g_mt_x[k]) * (x - g_mt_x[k]) + (y - g_mt_y[k]) * (y - g_mt_y[k]) < 2500) ok = 0;
        if (ok && x_try(x, y, '3', 1)) { g_mt_x[g_nmt] = x; g_mt_y[g_nmt] = y; g_nmt++; }
    }
}

// [qurbe] Buses abandonados sobre la calzada, paraderos y kioscos junto a ella (ciudad y barrio)
static void urbe_street_props(void)
{
    int city = g_x.type == XT_CITY;
    int nbus = city ? brange(7, 10) : brange(2, 3), npar = city ? brange(9, 13) : brange(3, 5), nkio = city ? brange(6, 9) : brange(3, 4);
    x_n = x_flood();
    for (int i = 0; i < nbus; i++)
        for (int t = 0; t < 60; t++) {
            int x = brange(6, MW - 11), y = brange(4, MH - 6), ok = 1;
            for (int k = 0; k < 4 && ok; k++) ok = g_zg[y][x + k] == '=' && (g_zg[y + 1][x + k] == '=' || g_zg[y - 1][x + k] == '=') && g_zo[y][x + k] == ' ';
            if (!ok) continue;
            int placed = 0;
            for (int k = 0; k < 4; k++) { if (!x_try(x + k, y, '[', 0)) break; placed++; }
            if (placed == 4) break;
            for (int k = 0; k < placed; k++) g_zo[y][x + k] = ' ';
            x_n = x_flood();
        }
    for (int i = 0; i < npar + nkio; i++)
        for (int t = 0; t < 120; t++) {
            int x = brange(5, MW - 6), y = brange(5, MH - 6);
            if (g_zg[y][x] == '=' || g_zg[y][x] == '_' || g_zg[y][x] == 'D' || g_zg[y][x] == 'r') continue;
            if (!(g_zg[y][x - 1] == '=' || g_zg[y][x + 1] == '=' || g_zg[y - 1][x] == '=' || g_zg[y + 1][x] == '=')) continue;
            if (x_try(x, y, i < npar ? ']' : '|', 0)) break;
        }
}

// Después de armar la superficie: el tablero del plano junto a cada boca y las fachadas altas
static void urbe_post_surface(void)
{
    memset(g_ub_h, 0, sizeof(g_ub_h));
    if (g_x.type != XT_CITY) return;
    for (int k = 0; k < g_nmt; k++) {
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; d++) {
            int x = g_mt_x[k] + D[d][0], y = g_mt_y[k] + D[d][1];
            if (in_map(x, y) && g_o[y][x] == O_NONE && g_g[y][x] != G_ROAD && g_g[y][x] != G_DOOR) { g_o[y][x] = O_DMAP; break; }
        }
    }
    for (int b = 0; b < x_nb; b++) {
        u32 h = hash2(x_b[b].x0 * 7 + 1, x_b[b].y0 * 5 + 3);
        int fl = (int)(h % 3);   // pisos extra dibujados sobre el muro (con el techo: 2 y 3 pisos de altura)
        for (int x = x_b[b].x0; x <= x_b[b].x1; x++) { g_ub_h[x_b[b].y0][x] = (unsigned char)(fl + 1); g_ub_c[x_b[b].y0][x] = (unsigned char)((h >> 8) % 4); }
    }
}

// Fachada alta sobre el muro de arriba de un edificio de la ciudad: pisos de ventanas, algunas con luz
static void urbe_fachada(int tx, int ty, int sph)
{
    int fl = g_ub_h[ty][tx];
    if (!fl || g_zone != 1) return;
    static const unsigned char C[4][3] = {{92, 80, 72}, {78, 84, 88}, {96, 88, 70}, {70, 66, 78}};
    const unsigned char *c = C[g_ub_c[ty][tx]];
    float x = tx * TS - g_ox, top = (ty + 1) * TS - sph - g_oy, hh = fl * 13;
    r_rect(x, top - hh, TS, hh, RGBA(c[0], c[1], c[2], 255));
    r_rect(x, top - hh, TS, 2, RGBA(c[0] + 30, c[1] + 30, c[2] + 30, 255));
    for (int r = 0; r < fl; r++) {
        float wy = top - (r + 1) * 13 + 4;
        u32 h = hash2(tx * 3 + r, ty * 11 + 1);
        int lit = h % 6 == 0, brk = h % 5 == 1;
        r_rect(x + 3, wy, 10, 7, RGBA(14, 12, 12, 255));
        r_rect(x + 4, wy + 1, 8, 5, lit ? RGBA(236, 192, 96, 255) : brk ? RGBA(24, 28, 32, 255) : RGBA(52, 78, 106, 255));
        r_rect(x + 8, wy + 1, 1, 5, RGBA(14, 12, 12, 255));
    }
    r_rect(x, top - 1, TS, 1, RGBA(20, 16, 14, 255));
}

// La boca de metro de arriba: escalera que baja, barandas y una "M" en un cartel azul
static void urbe_draw_mouth(int tx, int ty)
{
    float x = tx * TS - g_ox, y = ty * TS - g_oy;
    r_rect(x, y + 2, 16, 14, RGBA(40, 40, 44, 255));
    r_rect(x + 2, y + 4, 12, 11, RGBA(6, 6, 8, 255));
    for (int k = 0; k < 4; k++) r_rect(x + 3, y + 5 + k * 3, 10, 2, RGBA(46 + k * 14, 44 + k * 12, 40 + k * 10, 255));
    r_rect(x, y - 6, 2, 14, RGBA(150, 140, 60, 255)); r_rect(x + 14, y - 6, 2, 14, RGBA(150, 140, 60, 255));
    r_rect(x + 1, y - 18, 14, 12, RGBA(10, 40, 110, 255));
    r_rect(x + 1, y - 18, 14, 1, RGBA(120, 160, 230, 255));
    r_text_shadow(x + 4, y - 17, "M", RGBA(240, 240, 250, 255));
}

static void urbe_draw_obj(int tx, int ty, int o)
{
    float x = tx * TS - g_ox, y = ty * TS - g_oy;
    if (o == O_UCOL) {
        r_rect(x + 3, y + 13, 10, 3, RGBA(60, 58, 56, 255));
        r_rect(x + 4, y - 14, 8, 28, RGBA(104, 102, 96, 255));
        r_rect(x + 4, y - 14, 2, 28, RGBA(148, 146, 138, 255));
        r_rect(x + 10, y - 14, 2, 28, RGBA(70, 68, 64, 255));
    } else if (o == O_UBEN) {
        r_rect(x + 1, y + 4, 14, 4, RGBA(86, 60, 36, 255));
        r_rect(x + 1, y + 9, 14, 3, RGBA(120, 86, 52, 255));
        r_rect(x + 2, y + 12, 2, 4, RGBA(40, 36, 34, 255)); r_rect(x + 12, y + 12, 2, 4, RGBA(40, 36, 34, 255));
    } else if (o == O_UTRN) {
        r_rect(x + 6, y + 2, 4, 14, RGBA(90, 92, 96, 255));
        r_rect(x + 1, y + 6, 14, 2, RGBA(150, 152, 156, 255));
        r_rect(x + 7, y + 1, 2, 2, RGBA(60, 200, 90, 255));
    } else if (o == O_UBUS) {   // [qurbe] bus abandonado: 4 casillas en fila; se dibuja por tramos según los vecinos
        int l = tx > 0 && g_o[ty][tx - 1] == O_UBUS, r = tx + 1 < MW && g_o[ty][tx + 1] == O_UBUS;
        int x0 = tx - (l ? 1 : 0) - (l && tx > 1 && g_o[ty][tx - 2] == O_UBUS ? 1 : 0);
        int c = (int)(hash2(x0 / 2, ty) % 3);
        u32 cb = c == 0 ? RGBA(112, 92, 44, 255) : c == 1 ? RGBA(70, 86, 80, 255) : RGBA(96, 52, 44, 255);
        u32 cl = c == 0 ? RGBA(150, 128, 66, 255) : c == 1 ? RGBA(100, 122, 112, 255) : RGBA(132, 78, 64, 255);
        int ax = x + (l ? 0 : 1), aw = 16 - (l ? 0 : 1) - (r ? 0 : 1);
        r_rect(ax, y - 6, aw, 21, cb);
        r_rect(ax, y - 6, aw, 3, cl);                                                      // techo
        r_rect(ax, y + 9, aw, 2, RGBA(26, 22, 20, 255));                                   // faldón sucio
        r_rect(x + (l ? 0 : 2), y - 1, 16 - (l ? 0 : 2) - (r ? 0 : 2), 6, RGBA(18, 20, 24, 255));    // franja de ventanas
        if (hash2(tx, ty + 9) % 3) r_rect(x + 3, y, 4, 4, RGBA(56, 70, 82, 255));
        if (hash2(tx + 4, ty) % 3) r_rect(x + 9, y, 4, 4, RGBA(56, 70, 82, 255));
        if (!l) { r_rect(x + 1, y - 1, 2, 7, RGBA(30, 34, 40, 255)); r_rect(x + 1, y + 7, 3, 2, RGBA(210, 190, 110, 200)); }   // parabrisas y farol
        if (!r) { r_rect(x + 13, y - 1, 2, 7, RGBA(30, 34, 40, 255)); r_rect(x + 12, y + 7, 3, 2, RGBA(150, 40, 36, 255)); }
        r_rect(x + 3, y + 12, 4, 4, RGBA(14, 12, 12, 255)); r_rect(x + 10, y + 12, 4, 4, RGBA(14, 12, 12, 255));   // ruedas
    } else if (o == O_UPAR) {   // [qurbe] paradero: techo, banca y cartel
        r_rect(x + 1, y - 14, 14, 3, RGBA(88, 92, 96, 255)); r_rect(x + 1, y - 14, 14, 1, RGBA(150, 154, 158, 255));
        r_rect(x + 2, y - 11, 2, 21, RGBA(54, 56, 60, 255)); r_rect(x + 12, y - 11, 2, 21, RGBA(54, 56, 60, 255));
        r_rect(x + 4, y - 9, 8, 12, RGBA(30, 44, 54, 150));
        r_rect(x + 2, y + 5, 12, 3, RGBA(110, 80, 50, 255));
        r_rect(x + 5, y - 8, 6, 4, RGBA(210, 170, 50, 255));
    } else if (o == O_UKIO) {   // [qurbe] kiosco cerrado: caseta con toldo desteñido y postigo
        r_rect(x, y - 8, 16, 24, RGBA(78, 58, 40, 255));
        r_rect(x, y - 8, 16, 2, RGBA(116, 90, 62, 255));
        r_rect(x - 1, y - 6, 18, 5, hash2(tx, ty) & 1 ? RGBA(124, 44, 40, 255) : RGBA(52, 84, 74, 255));
        r_rect(x + 2, y, 12, 7, RGBA(22, 18, 16, 255));
        r_rect(x + 3, y + 1, 10, 5, RGBA(60, 50, 40, 255));
        r_rect(x + 5, y + 9, 6, 1, RGBA(150, 140, 110, 255));
    } else {
        r_rect(x, y - 4, 16, 20, RGBA(64, 58, 52, 255));

        r_rect(x + 2, y, 12, 6, RGBA(20, 18, 16, 255)); r_rect(x + 3, y + 1, 10, 4, RGBA(200, 170, 80, 255));
        r_rect(x, y - 4, 16, 2, RGBA(110, 100, 90, 255));
    }
}

// Franja amarilla y baldosas del andén (gancho en el dibujo del suelo)
static void urbe_suelo(int tx, int ty, float x, float y)
{
    if (g_zone != ZM || g_g[ty][tx] != G_FLOOR) return;
    if (g_g[ty + 1][tx] == G_RAIL) { r_rect(x, y + 12, TS, 4, RGBA(210, 180, 40, 255)); r_rect(x, y + 10, TS, 2, RGBA(40, 38, 30, 255)); }
    else if (ty > 0 && g_g[ty - 1][tx] == G_RAIL) { r_rect(x, y, TS, 4, RGBA(210, 180, 40, 255)); r_rect(x, y + 4, TS, 2, RGBA(40, 38, 30, 255)); }
    else if (((tx + ty) & 1) == 0) r_rect(x, y, TS, TS, RGBA(255, 255, 255, 14));
}

static void urbe_emissive(void)
{
    if (g_zone == ZM) {
        for (int i = 0; i < g_mt_nred; i++) {
            float x = g_mt_red[i][0] * TS + 8 - g_ox, y = g_mt_red[i][1] * TS + 4 - g_oy;
            if (x < -60 || y < -60 || x > SCR_W + 60 || y > SCR_H + 60) continue;
            r_glow(x, y, 38 + 3 * sinf(g_time * 3 + i), RGBA(255, 30, 20, 70), RGBA(255, 20, 10, 0));
            r_rect(x - 2, y - 2, 4, 3, RGBA(255, 70, 50, 255));
        }
    } else if (g_zone == 1) {
        for (int k = 0; k < g_nmt; k++) {
            float x = g_mt_x[k] * TS + 8 - g_ox, y = g_mt_y[k] * TS - g_oy - 6;
            if (x < -50 || y < -50 || x > SCR_W + 50 || y > SCR_H + 50) continue;
            r_glow(x, y, 30, RGBA(70, 130, 255, 60), RGBA(70, 130, 255, 0));
        }
    }
}

static void urbe_map_marks(int x0, int y0, int MAP_PX)
{
    if (g_zone != 1) return;
    for (int k = 0; k < g_nmt; k++) {
        int x = g_mt_x[k], y = g_mt_y[k];
        if (!g_seen[y][x]) continue;
        r_rect(x0 + x * MAP_PX - 3, y0 + y * MAP_PX - 3, 9, 9, RGBA(20, 60, 160, 255));
        r_text_shadow(x0 + x * MAP_PX - 2, y0 + y * MAP_PX - 3, "M", RGBA(255, 255, 255, 255));
    }
}

// ---------------------------------------------------------------- el metro
static void mt_rect(int x0, int y0, int x1, int y1, char g)
{
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (x > 0 && y > 0 && x < MW - 1 && y < MH - 1) { g_zg[y][x] = g; g_zo[y][x] = ' '; }
}
static void mt_deco(int x, int y, int o) { if (g_mt_nd < 260) { g_mt_deco[g_mt_nd][0] = (short)x; g_mt_deco[g_mt_nd][1] = (short)y; g_mt_deco[g_mt_nd][2] = (short)o; g_mt_nd++; } }
static void mt_red(int x, int y) { if (g_mt_nred < 24) { g_mt_red[g_mt_nred][0] = (short)x; g_mt_red[g_mt_nred][1] = (short)y; g_mt_nred++; } }

#define ST_W 30
#define ST_H 11
static void metro_gen(void)
{
    exped_gen();   // otra vez, solo para saber cuántas bocas hay (con la misma semilla sale igual)
    int n = g_nmt < 2 ? 2 : g_nmt, sx[MT_MAX], sy[MT_MAX];
    g_brs = (g_x.seed ^ 0x4D455452u) | 1;
    memset(x_keep, 0, sizeof(x_keep));
    x_nt = x_nd = x_nen = 0; g_mt_nd = g_mt_nred = 0; g_mt_ns = n;
    for (int y = 0; y < MH; y++) {
        memset(g_zg[y], '.', MW); g_zg[y][MW] = 0;
        memset(g_zo[y], '#', MW); g_zo[y][MW] = 0;
    }
    for (int k = 0; k < n; k++) { sx[k] = 8 + k * (n == 3 ? 64 : 128); sy[k] = brange(8, MH - ST_H - 8); }
    for (int k = 0; k + 1 < n; k++) {   // túneles con vías: horizontal, un quiebre vertical y horizontal
        int cy0 = sy[k] + 5, cy1 = sy[k + 1] + 5, mx = (sx[k] + ST_W + sx[k + 1]) / 2;
        int ya = cy0 < cy1 ? cy0 : cy1, yb = cy0 < cy1 ? cy1 : cy0;
        mt_rect(sx[k] + ST_W - 1, cy0 - 2, mx + 2, cy0 + 2, '.'); mt_rect(sx[k] + ST_W - 1, cy0 - 1, mx + 2, cy0 + 1, 'r');
        mt_rect(mx - 2, ya, mx + 2, yb, '.'); mt_rect(mx - 1, ya, mx + 1, yb, 'r');
        mt_rect(mx - 2, cy1 - 2, sx[k + 1] + 1, cy1 + 2, '.'); mt_rect(mx - 2, cy1 - 1, sx[k + 1] + 1, cy1 + 1, 'r');
        for (int x = sx[k] + ST_W + 6; x < mx; x += 14) mt_red(x, cy0 - 2);
        if (k == n - 2) {   // el tren descarrilado: dos vagones que se atraviesan por sus puertas, con botín
            for (int w = 0; w < 2; w++) {
                int x0 = mx + 3 + w * 8, x1 = x0 + 6;
                for (int y = cy1 - 2; y <= cy1 + 2; y++) for (int x = x0; x <= x1; x++) {
                    int edge = y == cy1 - 2 || y == cy1 + 2 || x == x0 || x == x1;
                    g_zg[y][x] = '_'; g_zo[y][x] = edge ? '#' : ' ';
                }
                g_zg[cy1][x0] = 'D'; g_zo[cy1][x0] = ' '; g_zg[cy1][x1] = 'D'; g_zo[cy1][x1] = ' ';
                g_zo[cy1 - 1][x0 + 2] = w ? 'r' : 'c'; g_zo[cy1 + 1][x0 + 4] = 'c';
                if (w) g_zo[cy1 - 1][x1 - 1] = 'c';
                mt_red(x0 + 3, cy1 - 2);
            }
            g_zo[cy1 + 1][mx + 1] = 'b'; g_zo[cy1 - 1][mx + 17] = 'b';   // restos del choque
        }
    }
    for (int k = 0; k < n; k++) {
        mt_rect(sx[k], sy[k], sx[k] + ST_W - 1, sy[k] + ST_H - 1, '_');
        mt_rect(sx[k], sy[k] + 4, sx[k] + ST_W - 1, sy[k] + 6, 'r');
        g_mt_sx[k] = sx[k] + 14; g_mt_sy[k] = sy[k];
        g_zo[sy[k]][sx[k] + 14] = '4';
        for (int x = sx[k] + 4; x < sx[k] + ST_W - 2; x += 6) { mt_deco(x, sy[k] + 3, O_UCOL); mt_deco(x, sy[k] + 7, O_UCOL); }
        mt_deco(sx[k] + 8, sy[k] + 1, O_UBEN); mt_deco(sx[k] + 20, sy[k] + 1, O_UBEN); mt_deco(sx[k] + 11, sy[k] + 9, O_UBEN); mt_deco(sx[k] + 23, sy[k] + 9, O_UBEN);
        mt_deco(sx[k] + 11, sy[k] + 1, O_UTRN); mt_deco(sx[k] + 12, sy[k] + 1, O_UTRN); mt_deco(sx[k] + 16, sy[k] + 1, O_UTRN); mt_deco(sx[k] + 17, sy[k] + 1, O_UTRN);
        mt_deco(sx[k] + 26, sy[k] + 1, O_UBTH); mt_deco(sx[k] + 3, sy[k] + 1, -1); mt_deco(sx[k] + 27, sy[k] + 9, -1);
        mt_red(sx[k] + 5, sy[k]); mt_red(sx[k] + 24, sy[k]); mt_red(sx[k] + 14, sy[k] + 10);
    }
    x_ex = g_mt_sx[0]; x_ey = g_mt_sy[0] + 1; g_mt_ex = x_ex; g_mt_ey = x_ey;
    x_rx = x_ry = 1; x_emin2 = 169;
    x_n = x_flood();
    int nc = 12 + 5 * n;
    for (int t = 0; t < nc; t++) x_place(t % 5 == 0 ? 'r' : t % 7 == 3 ? 'N' : 'c', -1, 0, 1);
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < 5; i++) x_enemy(i % 3 == 0 ? 'm' : i % 3 == 1 ? 'd' : 'a', sx[k] + 15, sy[k] + 5, 14);
        if (g_x.danger >= 2) x_enemy('5', sx[k] + 15, sy[k] + 5, 14);
    }
    for (int k = 0; k + 1 < n; k++) { int mx = (sx[k] + ST_W + sx[k + 1]) / 2; for (int i = 0; i < 4; i++) x_enemy(i & 1 ? 'm' : 'd', mx, sy[k + 1] + 5, 12); }
    x_n = x_flood();
    for (int k = 0; k < n; k++) if (!x_reach[g_mt_sy[k] + 1][g_mt_sx[k]]) { x_connect(g_mt_sx[k], g_mt_sy[k] + 1); g_x_fix++; x_n = x_flood(); }   // [qurbe] toda estación se alcanza desde la primera
    for (int i = 0; i < x_nt; i++) if (!x_near(x_tx[i], x_ty[i])) { g_zo[x_ty[i]][x_tx[i]] = ' '; g_x_fix++; x_n = x_flood(); }
    x_rx = 6; x_ry = 5; x_emin2 = 169;
}

// Después de volcar el metro a las grillas vivas: decorado y máquinas expendedoras
static void urbe_post_metro(void)
{
    for (int i = 0; i < g_mt_nd; i++) {
        int x = g_mt_deco[i][0], y = g_mt_deco[i][1], o = g_mt_deco[i][2];
        if (!in_map(x, y) || g_o[y][x] != O_NONE) continue;
        if (o < 0) { if (g_ncr < MAXCR - 2) { g_o[y][x] = O_CRATE; g_cr[g_ncr++] = (Crate){x, y, CK_VEND, 0}; } }
        else g_o[y][x] = (unsigned char)o;
    }
}

static void urbe_stash(void)
{
    for (int i = 0; i < MAXCR; i++) g_mt_open[i] = i < g_ncr ? (unsigned char)g_cr[i].open : 0;
    for (int i = 0; i < MAXE; i++) g_mt_dead[i] = i < g_ne ? g_e[i].state == ES_DEAD : 0;
}
static void urbe_apply(void)
{
    for (int i = 0; i < g_ncr && i < MAXCR; i++) if (g_mt_open[i]) g_cr[i].open = 1;
    for (int i = 0; i < g_ne && i < MAXE; i++) if (g_mt_dead[i]) { g_e[i].state = ES_DEAD; g_e[i].hp = 0; g_e[i].dead_t = 0; }
}
// Llegada abajo (por la boca g_mt_cur) y salida arriba (por la boca de la estación más cercana)
static void urbe_arrive_down(float *x, float *y)
{
    int k = g_mt_cur < g_mt_ns ? g_mt_cur : 0;
    *x = g_mt_sx[k] * TS + 8; *y = (g_mt_sy[k] + 1) * TS + 8;
}
static void urbe_arrive_up(float *x, float *y)
{
    static const int D[4][2] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
    int k = g_mt_cur < g_nmt ? g_mt_cur : 0;
    if (g_nmt == 0) return;
    for (int d = 0; d < 4; d++) {
        int tx = g_mt_x[k] + D[d][0], ty = g_mt_y[k] + D[d][1];
        if (in_map(tx, ty) && !solid(tx, ty)) { *x = tx * TS + 8; *y = ty * TS + 8; return; }
    }
}
static int urbe_station_near(void)
{
    int b = 0; float bd = 1e9f;
    for (int k = 0; k < g_mt_ns; k++) { float d = dist(P.x, P.y, g_mt_sx[k] * TS + 8, g_mt_sy[k] * TS + 8); if (d < bd) { bd = d; b = k; } }
    return b;
}
