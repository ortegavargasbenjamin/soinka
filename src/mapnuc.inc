// [mapnuc] Dos expediciones nuevas: Planta atómica (XT_NUC) y Tierra maldita (XT_CURSED).
// Se incluye en game.c justo antes de mapas3.inc (que las engancha con sus funciones m3_*) y usa las funciones x_* del generador.
//  - Planta atómica: dos torres de enfriamiento, el reactor (con sala de control y núcleo), piscinas de combustible, tuberías, un depósito
//    de barriles, cerco con señales y zonas de radiación alta ('~' = G_RAD). Peligro +3, más cajas militares y cajones ricos.
//    Sus piezas se dibujan por código con rectángulos (sin atlas), como las de mapas3.
//  - Tierra maldita: suelo, agua con orillas, adornos y obstáculos del paquete CursedLand (sprites MN_* del atlas, bloque [mapnuc] de
//    tools/make_atlas.py); el río y los lagos son MN_WAT (solidos), y los puentes son tramos de camino marcados en mn_br.
enum { O_NTW = 175, O_NPL, O_NPI, O_NBR, O_NCP, O_NSG, O_NRC, O_NCU, O_MNEND };   // torre, piscina, tubería, barril, tablero, señal, núcleo, obstáculo maldito

#define MN_WAT 160   // = O_M3WAT (mapas3.inc se incluye después); el agua del río y los lagos
static int mn_is(int t) { return t == XT_NUC || t == XT_CURSED; }
static const char *mn_name(int t) { return t == XT_NUC ? "Planta atómica" : "Tierra maldita"; }
static char mn_obj_of(char c)
{
    switch (c) {
    case '(': return O_NTW; case '`': return O_NPL; case ')': return O_NPI; case '{': return O_NBR;
    case '}': return O_NCP; case '\\': return O_NSG; case '>': return O_NRC; case '-': return O_NCU;
    }
    return 0;
}
static int mn_rich(void) { return g_x.type == XT_NUC ? 35 : 0; }   // puntos extra de % de cajón rico

static unsigned char mn_br[MHMAX][MWMAX];   // puentes de la tierra maldita
static int mn_cx0, mn_cy0, mn_cx1, mn_cy1;   // el recinto de la planta
static int mn_bx0, mn_by0, mn_bx1, mn_by1;   // el reactor
static int mn_tw[2][4];                      // torres: x0, y0, x1, y1

static void mn_set(int x, int y, char g, char o) { if (x_inb(x, y)) { if (g) g_zg[y][x] = g; g_zo[y][x] = o; } }
static void mn_door(int x, int y, int ox, int oy)
{
    if (!x_inb(x, y) || !x_inb(x + ox, y + oy) || !x_inb(x - ox, y - oy)) return;
    g_zo[y][x] = ' '; g_zg[y][x] = 'D';
    x_keep[y][x] = x_keep[y + oy][x + ox] = x_keep[y - oy][x - ox] = 1;
    if (x_nd < 128) { x_dx[x_nd] = x; x_dy[x_nd++] = y; }
}
static void mn_bld(int x0, int y0, int x1, int y1, int side)   // edificio comun con una puerta
{
    for (int j = y0; j <= y1; j++) for (int i = x0; i <= x1; i++) mn_set(i, j, '_', (i == x0 || i == x1 || j == y0 || j == y1) ? '#' : ' ');
    if (x_nb < MAXB_X) x_b[x_nb++] = (XBld){x0, y0, x1, y1};
    int mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
    if (side == 0) mn_door(mx, y0, 0, 1); else if (side == 1) mn_door(mx, y1, 0, 1); else if (side == 2) mn_door(x0, my, 1, 0); else mn_door(x1, my, 1, 0);
    g_zo[my][mx + 1] = '*';
}
static void mn_hot(int cx, int cy, int r)   // charco de radiación alta: solo sobre suelo abierto
{
    for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
        if (!x_inb(x, y) || (x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r + (int)(brnd() % 3)) continue;
        if ((g_zg[y][x] == '.' || g_zg[y][x] == ',' || g_zg[y][x] == '_') && !x_keep[y][x]) g_zg[y][x] = '~';
    }
}

// ------------------------------------------------------------------ planta atómica
static void mn_nuc(void)
{
    const int X0 = 62, X1 = 134, Y0 = 20, Y1 = 98, MX = 98, MY = 59;
    mn_cx0 = X0; mn_cy0 = Y0; mn_cx1 = X1; mn_cy1 = Y1;
    // el suelo del recinto, de hormigón limpio de objetos
    for (int y = Y0; y <= Y1; y++) for (int x = X0; x <= X1; x++) if (x_inb(x, y)) { if (g_zg[y][x] != '=') g_zg[y][x] = '.'; g_zo[y][x] = ' '; }
    // los cuatro caminos hasta los portones y la cruz interior
    x_fill(X0 - 10, MY - 1, X1 + 10, MY, '='); x_fill(MX - 1, Y0 - 10, MX, Y1 + 10, '=');
    // el reactor
    mn_bx0 = 88; mn_by0 = 48; mn_bx1 = 108; mn_by1 = 66;
    for (int j = mn_by0; j <= mn_by1; j++) for (int i = mn_bx0; i <= mn_bx1; i++) mn_set(i, j, '_', (i == mn_bx0 || i == mn_bx1 || j == mn_by0 || j == mn_by1) ? '#' : ' ');
    if (x_nb < MAXB_X) x_b[x_nb++] = (XBld){mn_bx0, mn_by0, mn_bx1, mn_by1};
    for (int j = mn_by0 + 1; j < mn_by1; j++) g_zo[j][96] = '#';   // tabique de la sala de control
    mn_door(96, 57, 1, 0); mn_door(98, mn_by0, 0, 1); mn_door(98, mn_by1, 0, 1); mn_door(mn_bx1, 58, 1, 0); mn_door(mn_bx0, 57, 1, 0);
    mn_door(100, mn_by0, 0, 1);
    for (int i = 90; i <= 94; i++) { g_zo[mn_by0 + 1][i] = '}'; g_zo[mn_by1 - 1][i] = '}'; }   // tableros
    g_zo[53][91] = '}'; g_zo[53][93] = '}'; g_zo[61][91] = '}'; g_zo[61][93] = '}';
    g_zo[57][92] = '*'; g_zo[57][103] = '*';
    // el núcleo: 3x3 con el anillo de radiación alrededor
    for (int j = 52; j <= 62; j++) for (int i = 98; i <= 108; i++) {
        int dx = i - 103, dy = j - 57;
        if (abs(dx) <= 1 && abs(dy) <= 1) { g_zo[j][i] = '>'; g_zg[j][i] = '_'; }
        else if (dx * dx + dy * dy <= 12 && g_zo[j][i] == ' ') g_zg[j][i] = '~';
    }
    for (int j = 49; j <= 51; j++) g_zg[j][103] = '_';
    for (int i = 97; i < mn_bx1; i++) x_keep[57][i] = x_keep[58][i] = x_keep[49][i] = x_keep[65][i] = 1;
    // las dos torres de enfriamiento (octógono de 12x8)
    static const int TX[2] = {66, 119};
    for (int k = 0; k < 2; k++) {
        int x0 = TX[k], y0 = 24, x1 = x0 + 11, y1 = y0 + 7;
        mn_tw[k][0] = x0; mn_tw[k][1] = y0; mn_tw[k][2] = x1; mn_tw[k][3] = y1;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            int cx = x == x0 || x == x1, cy = y == y0 || y == y1;
            if (cx && cy) continue;
            if ((x == x0 + 1 || x == x1 - 1) && cy) continue;
            mn_set(x, y, '.', '(');
        }
    }
    // piscinas de combustible gastado
    for (int k = 0; k < 2; k++) { int x0 = k ? 116 : 68; for (int y = 40; y <= 45; y++) for (int x = x0; x <= x0 + 11; x++) mn_set(x, y, '_', '`'); }
    // tuberías gruesas con pasos
    for (int y = 22; y <= 46; y++) if (y < 34 || y > 36) { g_zo[y][80] = ')'; g_zo[y][116] = ')'; }
    for (int x = 80; x <= 87; x++) g_zo[46][x] = ')';
    for (int x = 109; x <= 116; x++) g_zo[46][x] = ')';
    for (int x = X0 + 2; x <= X1 - 2; x++) if ((x - X0) % 20 > 3 && g_zg[Y1 - 2][x] != '=') g_zo[Y1 - 2][x] = ')';
    // dos edificios de servicio en el suroeste y el noreste
    mn_bld(66, 74, 77, 82, 0);
    mn_bld(82, 76, 92, 84, 0);
    mn_bld(126, 30, 133, 37, 2);
    // el depósito de barriles, al sureste
    for (int y = 72; y <= 94; y++) for (int x = 112; x <= 130; x++) {
        if ((x & 1) == 0 && y % 3 != 0 && g_zg[y][x] != '=' && (int)(brnd() % 100) < 80) g_zo[y][x] = '{';
    }
    // charcos de radiación alta
    mn_hot(74, 49, 3); mn_hot(122, 49, 3); mn_hot(118, 82, 2); mn_hot(122, 90, 2);
    for (int k = 0; k < 4; k++) mn_hot(brange(X0 + 6, X1 - 6), brange(Y0 + 6, Y1 - 6), brange(2, 3));
    for (int k = 0; k < 4; k++) mn_hot(brange(14, MW - 14), brange(10, MH - 10), 2);
    // el cerco, con señales de radiación; lo corta el camino y los portones
    for (int x = X0 - 1; x <= X1 + 1; x++) for (int s = 0; s < 2; s++) {
        int y = s ? Y1 + 1 : Y0 - 1;
        if (x_inb(x, y) && g_zg[y][x] != '=' && g_zg[y][x] != 'Z' && g_zo[y][x] == ' ') g_zo[y][x] = x % 9 == 0 ? '\\' : 'f';
    }
    for (int y = Y0 - 1; y <= Y1 + 1; y++) for (int s = 0; s < 2; s++) {
        int x = s ? X1 + 1 : X0 - 1;
        if (x_inb(x, y) && g_zg[y][x] != '=' && g_zg[y][x] != 'Z' && g_zo[y][x] == ' ') g_zo[y][x] = y % 9 == 0 ? '\\' : 'f';
    }
}

// ------------------------------------------------------------------ tierra maldita
static int mn_near_ret(int x, int y)
{
    return (abs(x - x_ex) < 10 && abs(y - x_ey) < 9) || (abs(x - g_ret2_tx) < 10 && abs(y - g_ret2_ty) < 9) || (abs(x - g_ret3_tx) < 10 && abs(y - g_ret3_ty) < 9);
}
static void mn_water(int x, int y)
{
    if (!x_inb(x, y) || y < 4 || y > MH - 5 || mn_near_ret(x, y)) return;
    char g = g_zg[y][x];
    if (g == 'Z' || g == '~' || g_zo[y][x] != ' ') return;
    if (g == '=') { mn_br[y][x] = 1; x_keep[y][x] = 1; return; }   // un camino que cruza: puente
    g_zg[y][x] = '.'; g_zo[y][x] = '&';
}
static void mn_cur(void)
{
    memset(mn_br, 0, sizeof(mn_br));
    int phase = brange(0, 60);
    for (int y = 8; y <= MH - 9; y++) {   // el río de norte a sur
        int cx = 96 + (int)(16.0f * sinf((y + phase) * 0.075f)), hw = 2 + ((y * 7 + phase) % 5 == 0);
        for (int x = cx - hw; x <= cx + hw; x++) mn_water(x, y);
    }
    for (int k = 0; k < 4; k++) {   // lagos
        int lx = k % 2 ? brange(110, 170) : brange(18, 80), ly = brange(14, MH - 15), rx = brange(7, 13), ry = brange(4, 7);
        for (int y = ly - ry; y <= ly + ry; y++) for (int x = lx - rx; x <= lx + rx; x++)
            if ((x - lx) * (x - lx) * ry * ry + (y - ly) * (y - ly) * rx * rx <= rx * rx * ry * ry) mn_water(x, y);
    }
    for (int k = 0; k < 2; k++) {   // dos puentes más, de tablas, a través del río
        int y = brange(14, MH - 18);
        for (int j = 0; j < 3; j++) for (int x = 60; x <= 140; x++)
            if (g_zo[y + j][x] == '&') { g_zo[y + j][x] = ' '; g_zg[y + j][x] = '='; mn_br[y + j][x] = 1; x_keep[y + j][x] = 1; }
    }
}

// ------------------------------------------------------------------ ganchos del generador
static void mn_terrain(int type) { if (type == XT_NUC) mn_nuc(); else if (type == XT_CURSED) mn_cur(); }

static int mn_crate(int l, int t, int r, int b, char ch)
{
    for (int k = 0; k < 30; k++) if (x_try(brange(l, r), brange(t, b), ch, 1)) return 1;
    return 0;
}
static void mn_props(int type)
{
    int t;
    if (type == XT_NUC) {
        for (t = 0; t < 5; t++) mn_crate(mn_bx0 + 1, mn_by0 + 1, mn_bx1 - 1, mn_by1 - 1, 'r');   // el reactor guarda lo mejor
        for (t = 0; t < 4; t++) mn_crate(90, 50, 95, 64, 'c');   // la sala de control
        for (t = 0; t < 8; t++) mn_crate(mn_cx0 + 2, mn_cy0 + 2, mn_cx1 - 2, mn_cy1 - 2, 'r');
        for (t = 0; t < 8; t++) mn_crate(112, 72, 130, 94, 'c');   // entre los barriles
        for (int i = 0; i < 30; i++) for (t = 0; t < 30; t++) if (x_prop(brange(6, MW - 8), brange(5, MH - 6), '{')) break;   // barriles sueltos por todo el mapa
        for (int i = 0; i < 10; i++) for (t = 0; t < 30; t++) if (x_prop(brange(6, MW - 8), brange(5, MH - 6), '\\')) break;
    } else if (type == XT_CURSED) {
        int n = 0;
        for (int c = 0; c < 34; c++) {   // bosquecillos de plantas, rocas y ruinas
            int cx = brange(8, MW - 9), cy = brange(6, MH - 7), m = brange(5, 9);
            for (int k = 0; k < m; k++) if (x_try(cx + brange(-5, 5), cy + brange(-4, 4), '-', 0)) n++;
        }
        for (t = 0; n < 200 && t < 600; t++) if (x_try(brange(6, MW - 8), brange(5, MH - 6), '-', 0)) n++;
    }
}

static void mn_enemies(int type)
{
    if (type == XT_NUC) {   // irradiados y acorazados: el recinto, el reactor y los barriles
        for (int k = 0; k < 5; k++) x_enemy(k % 2 ? '7' : 'm', 103, 57, 6);
        for (int k = 0; k < 4; k++) x_enemy('m', 92, 57, 3);
        for (int k = 0; k < 6; k++) x_enemy(k % 2 ? 'm' : '7', 121, 83, 8);
        for (int k = 0; k < 6; k++) x_enemy(k % 3 ? '7' : 'm', 72, 28, 8);
        for (int k = 0; k < 6; k++) x_enemy(k % 3 ? '7' : 'm', 125, 28, 8);
        for (int k = 0; k < 6; k++) x_enemy('m', 98, 59, 25);
        for (int k = 0; k < 6; k++) x_enemy(k % 2 ? '7' : 'm', brange(20, MW - 20), brange(10, MH - 10), 6);
    } else if (type == XT_CURSED) {   // criaturas: perros malditos y corredores
        for (int g = 0; g < 14; g++) {
            int cx = brange(24, MW - 12), cy = brange(8, MH - 9);
            for (int k = 0; k < 3; k++) x_enemy(g % 3 ? '9' : '5', cx, cy, 4);
        }
    }
}

static const short *mn_pool(void)
{
    static const short NUCP[] = {ITEM_ARAD, ITEM_ART0, ITEM_ARAD, ITEM_MED, ITEM_ART3, ITEM_ARAD, ITEM_CLEAN, ITEM_FILTER};
    static const short CURP[] = {ITEM_D_TABACO, ITEM_D_ALCOHOL, ITEM_D_MAPA, ITEM_D_CAFE, ITEM_D_FILTRO, ITEM_D_CINTA, ITEM_D_ANTIB, ITEM_D_COBRE};
    return g_x.type == XT_NUC ? NUCP : CURP;
}
static u32 mn_tinte(void) { return g_x.type == XT_NUC ? RGBA(200, 222, 204, 255) : RGBA(218, 206, 214, 255); }

// ------------------------------------------------------------------ suelo
// Sprites MN_* del atlas: GR0-3 suelo, GR4-5 camino, WA0-3 agua, OB0-OB_N obstáculos, DC0-DC4 adornos del suelo
#define MN_NOB 13
static int mn_suelo_spr(int g, u32 h)
{
    if (g == G_DOOR || g == G_RAIL || g == G_RAD) return -1;
    if (g_x.type != XT_CURSED) return -1;
    if (g == G_FLOOR) return -1;
    if (g == G_ROAD || g == G_EXIT || g == G_ZONE) return S_MN_GR4 + (int)(h % 2);
    return S_MN_GR0 + (int)(h % 4);
}
static int mn_water_at(int tx, int ty) { return in_map(tx, ty) && g_o[ty][tx] == MN_WAT; }
// sobre el suelo: agua con orillas, puentes y adornos (tierra maldita); brillo de radiación (planta)
static int mn_suelo(int tx, int ty, float x, float y, int g)
{
    if (g_x.type == XT_NUC) {
        if (g == G_RAD) { float p = 0.5f + 0.5f * sinf(g_time * 2.2f + tx + ty); r_rect(x, y, TS, TS, RGBA(60, 255, 90, (int)(30 + 40 * p))); }
        return 0;
    }
    if (g_x.type != XT_CURSED) return 0;
    u32 h = hash2(tx, ty);
    if (g_o[ty][tx] == MN_WAT) {
        r_spr(SPR[S_MN_WA0 + (int)(h % 4)], x, y, COL_WHITE);
        float w = sinf(g_time * 1.1f + tx * 0.7f + ty * 0.5f);
        r_rect(x + 3 + (int)(w * 2), y + 6, 6, 1, RGBA(120, 140, 180, 110));
        if (!mn_water_at(tx, ty - 1)) { r_rect(x, y, TS, 3, RGBA(30, 12, 14, 255)); r_rect(x, y + 3, TS, 1, RGBA(160, 90, 80, 150)); }
        if (!mn_water_at(tx - 1, ty)) r_rect(x, y, 2, TS, RGBA(30, 12, 14, 255));
        if (!mn_water_at(tx + 1, ty)) r_rect(x + TS - 2, y, 2, TS, RGBA(30, 12, 14, 255));
        if (!mn_water_at(tx, ty + 1)) { r_rect(x, y + TS - 2, TS, 2, RGBA(30, 12, 14, 255)); }
        return 1;
    }
    if (mn_br[ty][tx]) {   // puente de tablas con barandas hacia el agua
        int ns = mn_water_at(tx, ty - 1) || mn_water_at(tx, ty + 1);
        r_rect(x, y, TS, TS, RGBA(86, 58, 40, 255));
        for (int k = 0; k < 4; k++) { if (ns) r_rect(x, y + k * 4 + 3, TS, 1, RGBA(40, 24, 18, 255)); else r_rect(x + k * 4 + 3, y, 1, TS, RGBA(40, 24, 18, 255)); }
        r_rect(x, y, TS, 1, RGBA(120, 86, 60, 200));
        if (mn_water_at(tx - 1, ty) || (!ns && mn_water_at(tx, ty - 1))) r_rect(x, y, 2, TS, RGBA(30, 18, 14, 255));
        if (mn_water_at(tx + 1, ty)) r_rect(x + TS - 2, y, 2, TS, RGBA(30, 18, 14, 255));
        return 1;
    }
    if (g_o[ty][tx] == O_NONE && (g == G_DIRT || g == G_GRASS) && (h >> 5) % 40 == 0) {   // huesos, venas y pústulas en el suelo
        const Spr s = SPR[S_MN_DC0 + (int)((h >> 11) % 5)];
        r_spr(s, x + (TS - s.w) / 2, y + (TS - s.h) / 2, COL_WHITE);
    }
    return 0;
}

// ------------------------------------------------------------------ objetos
static int mn_same(int tx, int ty, int o) { return in_map(tx, ty) && g_o[ty][tx] == o; }
static void mn_draw_obj(int tx, int ty, int o)
{
    float x = tx * TS - g_ox, y = ty * TS - g_oy, yb = y + TS;
    u32 h = hash2(tx, ty);
    switch (o) {
    case O_NTW: {   // torre de enfriamiento: hormigón con franjas, boca oscura arriba y vapor
        int up = mn_same(tx, ty - 1, o), dn = mn_same(tx, ty + 1, o), lf = mn_same(tx - 1, ty, o), rt = mn_same(tx + 1, ty, o);
        r_rect(x, y, TS, TS, RGBA(112, 110, 102, 255));
        r_rect(x + (h & 7), y, 2, TS, RGBA(96, 94, 88, 255));
        r_rect(x + 9 + (h >> 3 & 3), y, 1, TS, RGBA(128, 126, 118, 255));
        if (!lf) r_rect(x, y, 4, TS, RGBA(66, 64, 60, 255));
        if (!rt) r_rect(x + TS - 3, y, 3, TS, RGBA(150, 148, 138, 255));
        if (!up) {
            r_rect(x, y, TS, 9, RGBA(20, 26, 24, 255)); r_rect(x, y + 9, TS, 2, RGBA(150, 148, 138, 255));
            float p = fmodf(g_time * 0.35f + (tx % 4) * 0.25f, 1.0f);
            for (int k = 0; k < 2; k++) {
                float q = fmodf(p + k * 0.5f, 1.0f);
                r_rect(x + 2 + k * 6, y - 4 - q * 34, 8 + q * 6, 5 + q * 3, RGBA(210, 214, 208, (int)(110 * (1 - q))));
            }
        } else if ((ty & 3) == 0) r_rect(x, y + 6, TS, 1, RGBA(70, 68, 62, 255));
        if (!dn) { r_rect(x, yb - 3, TS, 3, RGBA(52, 50, 46, 255)); r_rect(x, yb, TS, 3, RGBA(0, 0, 0, 70)); }
        break;
    }
    case O_NPL: {   // piscina de combustible: borde de hormigón y agua verde brillante con barras
        int up = mn_same(tx, ty - 1, o), dn = mn_same(tx, ty + 1, o), lf = mn_same(tx - 1, ty, o), rt = mn_same(tx + 1, ty, o);
        float p = 0.5f + 0.5f * sinf(g_time * 1.6f + tx * 0.8f + ty);
        r_rect(x, y, TS, TS, RGBA(24, 92 + (int)(30 * p), 56, 255));
        r_rect(x + 2 + (h & 7), y + 5 + (int)(p * 3), 5, 1, RGBA(120, 255, 150, 150));
        if (h % 3 == 0) { r_rect(x + 6, y + 3, 2, 9, RGBA(190, 200, 120, 255)); r_rect(x + 6, y + 3, 2, 2, RGBA(240, 255, 160, 255)); }
        if (!up) r_rect(x, y, TS, 4, RGBA(140, 138, 128, 255));
        if (!dn) r_rect(x, yb - 3, TS, 3, RGBA(108, 106, 98, 255));
        if (!lf) r_rect(x, y, 3, TS, RGBA(128, 126, 118, 255));
        if (!rt) r_rect(x + TS - 3, y, 3, TS, RGBA(128, 126, 118, 255));
        break;
    }
    case O_NPI: {   // tubería gruesa con bridas
        int hz = mn_same(tx - 1, ty, o) || mn_same(tx + 1, ty, o), vt = mn_same(tx, ty - 1, o) || mn_same(tx, ty + 1, o);
        u32 col = h % 4 == 0 ? RGBA(120, 74, 44, 255) : RGBA(84, 90, 92, 255);
        if (hz || !vt) {
            r_rect(x, yb - 13, TS, 11, RGBA(16, 16, 18, 255)); r_rect(x, yb - 12, TS, 9, col); r_rect(x, yb - 12, TS, 2, RGBA(150, 156, 156, 255));
            if (tx % 4 == 0) r_rect(x + 6, yb - 14, 3, 13, RGBA(50, 52, 54, 255));
        }
        if (vt) {
            r_rect(x + 2, y, 12, TS, RGBA(16, 16, 18, 255)); r_rect(x + 3, y, 10, TS, col); r_rect(x + 3, y, 2, TS, RGBA(150, 156, 156, 255));
            if (ty % 4 == 0) r_rect(x + 1, y + 6, 14, 3, RGBA(50, 52, 54, 255));
        }
        break;
    }
    case O_NBR: {   // barril radiactivo amarillo con el símbolo
        r_rect(x + 2, yb - 15, 12, 15, RGBA(14, 14, 10, 255));
        r_rect(x + 3, yb - 14, 10, 12, RGBA(176, 160, 40, 255));
        r_rect(x + 3, yb - 14, 2, 12, RGBA(214, 198, 80, 255));
        r_rect(x + 3, yb - 11, 10, 1, RGBA(70, 62, 14, 255)); r_rect(x + 3, yb - 5, 10, 1, RGBA(70, 62, 14, 255));
        r_rect(x + 7, yb - 9, 2, 2, RGBA(20, 20, 10, 255)); r_rect(x + 5, yb - 8, 1, 1, RGBA(20, 20, 10, 255)); r_rect(x + 10, yb - 8, 1, 1, RGBA(20, 20, 10, 255));
        if (h % 4 == 0) { r_rect(x + 2, yb - 2, 12, 2, RGBA(70, 240, 90, 160)); r_rect(x + 5, yb - 16, 6, 1, RGBA(70, 240, 90, 200)); }
        break;
    }
    case O_NCP: {   // tablero de control con luces
        r_rect(x, yb - 22, TS, 22, RGBA(14, 16, 16, 255));
        r_rect(x + 1, yb - 21, 14, 19, RGBA(70, 76, 76, 255));
        for (int k = 0; k < 3; k++) {
            int on = ((int)(g_time * 1.5f) + (int)h + k) % 3 != 0;
            r_rect(x + 2 + k * 4, yb - 19, 3, 3, on ? (k == 1 ? RGBA(240, 180, 50, 255) : RGBA(70, 230, 100, 255)) : RGBA(30, 40, 34, 255));
        }
        r_rect(x + 2, yb - 13, 12, 6, RGBA(18, 40, 30, 255)); r_rect(x + 3, yb - 12, 10, 1, RGBA(90, 220, 140, 200));
        r_rect(x + 3, yb - 5, 3, 2, RGBA(180, 50, 40, 255));
        break;
    }
    case O_NSG: {   // señal de radiación en un poste
        r_rect(x + 7, yb - 14, 2, 14, RGBA(24, 24, 24, 255));
        r_rect(x + 1, yb - 26, 14, 13, RGBA(16, 14, 8, 255));
        r_rect(x + 2, yb - 25, 12, 11, RGBA(236, 206, 40, 255));
        r_rect(x + 7, yb - 21, 2, 2, RGBA(20, 20, 10, 255)); r_rect(x + 5, yb - 19, 2, 2, RGBA(20, 20, 10, 255)); r_rect(x + 9, yb - 19, 2, 2, RGBA(20, 20, 10, 255));
        break;
    }
    case O_NRC: {   // núcleo del reactor: acero y resplandor verde
        int up = mn_same(tx, ty - 1, o), dn = mn_same(tx, ty + 1, o), lf = mn_same(tx - 1, ty, o), rt = mn_same(tx + 1, ty, o);
        float p = 0.5f + 0.5f * sinf(g_time * 2.0f);
        r_rect(x - 3, y - 3, TS + 6, TS + 6, RGBA(60, 255, 100, (int)(24 + 26 * p)));
        r_rect(x, y, TS, TS, RGBA(52, 58, 56, 255));
        if (up && dn && lf && rt) { r_rect(x + 1, y + 1, TS - 2, TS - 2, RGBA(40 + (int)(40 * p), 230, 90, 255)); r_rect(x + 5, y + 5, 6, 6, RGBA(200, 255, 190, 255)); }
        else { r_rect(x + 3, y + 3, TS - 6, TS - 6, RGBA(34, 150 + (int)(60 * p), 70, 255)); if (!up) r_rect(x, y, TS, 3, RGBA(96, 102, 98, 255)); if (!dn) r_rect(x, yb - 3, TS, 3, RGBA(30, 32, 30, 255)); }
        break;
    }
    case O_NCU: {   // obstáculo maldito: planta, roca o ruina del paquete
        static const unsigned char W[24] = {0, 0, 1, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 11, 12, 3, 5};
        const Spr s = SPR[S_MN_OB0 + W[(h >> 4) % 24]];
        r_spr(s, x + (TS - s.w) / 2, yb - s.h, COL_WHITE);
        break;
    }
    default: break;
    }
}
