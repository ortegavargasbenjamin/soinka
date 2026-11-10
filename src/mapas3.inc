// [mapas3] Tres expediciones nuevas: Hospital (XT_HOSP), Puerto (XT_PORT) y Autopista (XT_ROAD).
// Se incluye en game.c justo después de mapas.inc y usa las funciones x_* del generador. Cada tipo arma su terreno propio
// antes de los edificios comunes (m3_terrain), pone sus cajones y adornos tras la primera inundación (m3_props) y suma
// enemigos propios (m3_enemies). Los objetos nuevos (O_M3*) se dibujan por código con rectángulos, sin atlas.
enum { O_M3WAT = 160, O_M3CRN, O_M3HUL, O_M3BED, O_M3TOL, O_M3PMP, O_M3NET, O_M3TNT, O_M3MED, O_M3RUB, O_M3END };

static int m3_is(int t) { return t == XT_HOSP || t == XT_PORT || t == XT_ROAD || mn_is(t); }   // [mapnuc]
static const char *m3_name(int t) { if (mn_is(t)) return mn_name(t); return t == XT_HOSP ? "Hospital" : t == XT_PORT ? "Puerto" : "Autopista"; }
static int m3_grass(int t) { return t == XT_NUC ? 8 : t == XT_CURSED ? 30 : t == XT_HOSP ? 22 : t == XT_PORT ? 4 : 62; }   // % de pasto del suelo
static char m3_obj_of(char c)   // letra del plano de objetos -> objeto
{
    switch (c) {
    case '&': return O_M3WAT; case '@': return O_M3CRN; case '^': return O_M3HUL; case '+': return O_M3BED; case '<': return O_M3TOL;
    case ':': return O_M3PMP; case ';': return O_M3NET; case '"': return O_M3TNT; case '?': return O_M3MED; case '/': return O_M3RUB;
    }
    return mn_obj_of(c);   // [mapnuc]
}

static int m3_hx0, m3_hy0, m3_hx1, m3_hy1;          // el hospital
static int m3_wy, m3_sx0, m3_sy0, m3_sx1, m3_sy1;   // la orilla del puerto y el barco
static int m3_crx[3], m3_cry;                       // grúas (x de la pata izquierda) y su fila
static int m3_hwy = -1, m3_tollx, m3_gasx, m3_gasy, m3_ovx;   // autopista: fila central, peaje, estación de servicio, paso sobre nivel
static struct { short l, t, r, b, k, north; } m3_room[16];
static int m3_nroom;

static int m3_ey_pick(int t, int ey) { return t == XT_ROAD ? brange(46, 68) : t == XT_PORT ? brange(10, 60) : ey; }
static int m3_ey2_pick(int t, int ey2, int ey) { return t == XT_ROAD ? ey : t == XT_PORT ? brange(8, 56) : ey2; }
static int m3_top_pick(int t, int top) { return t == XT_PORT ? 1 : top; }

static void m3_set(int x, int y, char g, char o) { if (x_inb(x, y)) { if (g) g_zg[y][x] = g; g_zo[y][x] = o; } }
static void m3_door(int x, int y, int ox, int oy)   // puerta con el paso libre a los dos lados
{
    if (!x_inb(x, y) || !x_inb(x + ox, y + oy) || !x_inb(x - ox, y - oy)) return;
    g_zo[y][x] = ' '; g_zg[y][x] = 'D';
    x_keep[y][x] = x_keep[y + oy][x + ox] = x_keep[y - oy][x - ox] = 1;
    if (x_nd < 128) { x_dx[x_nd] = x; x_dy[x_nd++] = y; }
}
static void m3_bld(int x0, int y0, int x1, int y1, int side)   // un edificio común con una puerta en el lado side (0 arriba, 1 abajo, 2 izq., 3 der.)
{
    for (int j = y0; j <= y1; j++) for (int i = x0; i <= x1; i++) m3_set(i, j, '_', (i == x0 || i == x1 || j == y0 || j == y1) ? '#' : ' ');
    if (x_nb < MAXB_X) x_b[x_nb++] = (XBld){x0, y0, x1, y1};
    int mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
    if (side == 0) m3_door(mx, y0, 0, 1); else if (side == 1) m3_door(mx, y1, 0, 1); else if (side == 2) m3_door(x0, my, 1, 0); else m3_door(x1, my, 1, 0);
    if (x1 - x0 >= 4) g_zo[(y0 + y1) / 2][(x0 + x1) / 2 + 1] = '*';
}
static int m3_free_rect(int x0, int y0, int x1, int y1, int margin)   // todo libre (suelo no camino, sin objetos) con margen
{
    for (int j = y0 - margin; j <= y1 + margin; j++)
        for (int i = x0 - margin; i <= x1 + margin; i++) {
            if (!x_inb(i, j)) return 0;
            char g = g_zg[j][i];
            if (g_zo[j][i] != ' ' || g == '=' || g == 'Z' || g == '~' || g == '_' || g == 'D' || g == 'r' || x_keep[j][i]) return 0;
        }
    return 1;
}

// ------------------------------------------------------------------ hospital
static void m3_hosp(void)
{
    const int W = 54, H = 30;
    int x0 = brange(60, 84), y0 = brange(36, 52), x1 = x0 + W - 1, y1 = y0 + H - 1;
    m3_hx0 = x0; m3_hy0 = y0; m3_hx1 = x1; m3_hy1 = y1;
    x_fill(x0 - 6, y0 - 6, x1 + 6, y1 + 6, '=');   // el estacionamiento
    for (int y = y0 - 6; y <= y1 + 6; y++) for (int x = x0 - 6; x <= x1 + 6; x++) if (x_inb(x, y)) g_zo[y][x] = ' ';
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) m3_set(x, y, '_', (x == x0 || x == x1 || y == y0 || y == y1) ? '#' : ' ');
    for (int x = x0 + 1; x < x1; x++) { g_zo[y0 + 12][x] = '#'; g_zo[y0 + 17][x] = '#'; }   // las paredes del pasillo
    static const char NK[6] = {1, 2, 3, 1, 4, 5}, SK[6] = {1, 6, 7, 2, 1, 4};   // 1 camillas, 2 farmacia, 3 quirófano, 4 morgue, 5 consultorio, 6 espera, 7 recepción
    m3_nroom = 0;
    for (int s = 0; s < 2; s++) {
        int north = s == 0, ty = north ? y0 + 1 : y0 + 18, by = north ? y0 + 11 : y0 + 28, wallr = north ? y0 + 12 : y0 + 17;
        for (int k = 0; k < 6; k++) {
            int L = x0 + 9 * k + 1, R = k < 5 ? x0 + 9 * k + 8 : x1 - 1, kind = north ? NK[k] : SK[k];
            if (k < 5) for (int y = ty; y <= by; y++) g_zo[y][x0 + 9 * (k + 1)] = '#';
            m3_door(L + 3, wallr, 0, 1);
            if (k < 5 && (k + s) % 2 == 0) m3_door(x0 + 9 * (k + 1), north ? y0 + 6 : y0 + 23, 1, 0);
            int far = north ? ty : by, st = north ? 1 : -1;
            g_zo[ty + 5][L + 3] = '*';
            if (kind == 1) { for (int j = 0; j < 3; j++) { m3_set(L + 1, ty + 2 + 3 * j, 0, '+'); m3_set(L + 6, ty + 2 + 3 * j, 0, '+'); } }
            else if (kind == 2) {
                for (int x = L; x <= R; x++) m3_set(x, far, 0, '?');
                for (int j = 1; j <= 2; j++) for (int x = L + 1; x <= R - 1; x++) if (x != L + 3 && x != L + 4) m3_set(x, far + 4 * st * j, 0, '?');
            } else if (kind == 3) { m3_set(L + 3, ty + 5, 0, '+'); m3_set(L + 4, ty + 5, 0, '+'); g_zo[ty + 5][L + 3] = '+'; g_zo[ty + 3][L + 3] = '*'; }
            else if (kind == 4) { for (int j = 0; j < 3; j++) for (int x = L + 1; x <= L + 6 && x <= R; x += 5) m3_set(x, far + 3 * st * j, 0, '+'); }
            else if (kind == 6 || kind == 7) { for (int j = 0; j < 3; j++) { m3_set(L + 1, ty + 2 + 3 * j, 0, 'h'); m3_set(L + 6, ty + 2 + 3 * j, 0, 'h'); } }
            if (m3_nroom < 16) m3_room[m3_nroom++] = (typeof(m3_room[0])){(short)L, (short)ty, (short)R, (short)by, (short)kind, (short)north};
        }
    }
    for (int j = 0; j < 7; j++) g_zo[y0 + 14][x0 + 4 + 8 * j] = '*';   // lámparas del pasillo
    for (int dy = 14; dy <= 15; dy++) { m3_door(x0, y0 + dy, 1, 0); m3_door(x1, y0 + dy, 1, 0); }   // entradas de los lados
    m3_door(x0 + 22, y1, 0, 1); m3_door(x0 + 23, y1, 0, 1);   // entrada principal, a la recepción
    // la reja del recinto, con cuatro portones y el camino que llega a cada uno
    int fx0 = x0 - 9, fy0 = y0 - 9, fx1 = x1 + 9, fy1 = y1 + 9;
    x_fill(x0 + 10, fy0, x0 + 14, y0 - 6, '='); x_fill(x1 - 14, fy1, x1 - 10, fy1, '='); x_fill(x1 - 14, y1 + 6, x1 - 10, fy1, '=');
    x_fill(fx0, y0 + 13, x0 - 6, y0 + 16, '='); x_fill(x1 + 6, y0 + 13, fx1, y0 + 16, '=');
    for (int x = fx0; x <= fx1; x++) for (int s = 0; s < 2; s++) {
        int y = s ? fy1 : fy0;
        if (x_inb(x, y) && g_zg[y][x] != '=' && g_zo[y][x] == ' ') g_zo[y][x] = 'f';
    }
    for (int y = fy0; y <= fy1; y++) for (int s = 0; s < 2; s++) {
        int x = s ? fx1 : fx0;
        if (x_inb(x, y) && g_zg[y][x] != '=' && g_zo[y][x] == ' ') g_zo[y][x] = 'f';
    }
}

// ------------------------------------------------------------------ puerto
static void m3_port(void)
{
    int wy = brange(68, 74), px[4];
    m3_wy = wy;
    for (int y = wy + 1; y < MH - 1; y++) for (int x = 1; x < MW - 1; x++) { g_zg[y][x] = '.'; g_zo[y][x] = '&'; }
    x_fill(1, wy - 2, MW - 2, wy, '=');   // el muelle de hormigón
    for (int y = wy - 2; y <= wy; y++) for (int x = 1; x < MW - 1; x++) g_zo[y][x] = ' ';
    for (int k = 0; k < 4; k++) {   // cuatro espigones
        px[k] = 20 + 42 * k + brange(0, 6);
        for (int y = wy + 1; y <= wy + 14; y++) for (int x = px[k]; x <= px[k] + 3; x++) m3_set(x, y, '=', ' ');
    }
    // el barco encallado entre los espigones 1 y 2: casco, cubierta, puente de mando y una rampa desde el muelle
    int sx0 = px[1] + 8, sx1 = sx0 + 27, sy0 = wy + 5, sy1 = sy0 + 7;
    m3_sx0 = sx0; m3_sy0 = sy0; m3_sx1 = sx1; m3_sy1 = sy1;
    for (int y = sy0; y <= sy1; y++) for (int x = sx0; x <= sx1; x++) {
        int edge = x == sx0 || x == sx1 || y == sy0 || y == sy1;
        m3_set(x, y, edge ? '.' : '_', edge ? '^' : ' ');
    }
    m3_set(sx1, sy0, '.', '&'); m3_set(sx1, sy1, '.', '&'); m3_set(sx1 - 1, sy0, '.', '^');
    for (int y = wy + 1; y <= sy0; y++) for (int x = sx0 + 16; x <= sx0 + 17; x++) { m3_set(x, y, y == sy0 ? '_' : '=', ' '); x_keep[y][x] = 1; }   // la rampa
    for (int y = sy0 + 2; y <= sy0 + 5; y++) for (int x = sx0 + 2; x <= sx0 + 8; x++) g_zo[y][x] = (x == sx0 + 2 || x == sx0 + 8 || y == sy0 + 2 || y == sy0 + 5) ? '#' : ' ';
    m3_door(sx0 + 8, sy0 + 3, 1, 0);
    for (int b = 0; b < 3; b++) { int bx = sx0 + 11 + 5 * b; for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) g_zo[sy0 + 4 + j][bx + i] = (b + j) % 2 ? 'C' : 'K'; }
    for (int y = sy0 + 4; y <= sy0 + 5; y++) for (int x = sx0 + 11; x <= sx0 + 25; x++) x_keep[y][x] = 0;
    // grúas pórtico en el muelle: dos patas altas con una viga
    m3_cry = wy - 4;
    for (int k = 0; k < 3; k++) {
        int cx = 24 + 56 * k + brange(0, 8);
        m3_crx[k] = cx;
        m3_set(cx, m3_cry, 0, '@'); m3_set(cx + 8, m3_cry, 0, '@');
    }
    // patios de contenedores: bloques de 4x2 con pasillos de 2 y 3 casillas entre ellos
    static const int FX[4][2] = {{16, 52}, {62, 98}, {108, 144}, {152, 186}}, FY[2][2] = {{14, 30}, {38, 56}};
    for (int fyi = 0; fyi < 2; fyi++)
        for (int fxi = 0; fxi < 4; fxi++) {
            if ((int)(brnd() % 100) < 12) continue;   // algún patio se queda vacío
            for (int by = FY[fyi][0]; by + 1 <= FY[fyi][1]; by += 5)
                for (int bx = FX[fxi][0]; bx + 3 <= FX[fxi][1]; bx += 6) {
                    if ((int)(brnd() % 100) < 18 || !m3_free_rect(bx, by, bx + 3, by + 1, 1)) continue;
                    char ch = brnd() % 2 ? 'C' : 'K';
                    for (int j = 0; j < 2; j++) for (int i = 0; i < 4; i++) g_zo[by + j][bx + i] = ch;
                }
        }
}

// ------------------------------------------------------------------ autopista
static void m3_road(int ey)
{
    int hw = ey;
    m3_hwy = hw;
    x_fill(1, hw - 2, MW - 4, hw + 3, '=');   // seis carriles
    for (int y = hw - 2; y <= hw + 3; y++) for (int x = 1; x < MW - 3; x++) if (g_zo[y][x] == 'T') g_zo[y][x] = ' ';
    // el peaje: tres casetas con carriles entre ellas, una oficina y vallas que encauzan
    int tx = brange(34, 46);
    m3_tollx = tx;
    for (int k = 0; k < 3; k++) m3_set(tx, hw - 1 + 2 * k, 0, '<');
    for (int j = 1; j <= 5; j++) { m3_set(tx, hw - 2 - j, 0, 'f'); m3_set(tx, hw + 3 + j, 0, 'f'); }
    m3_bld(tx + 3, hw - 11, tx + 8, hw - 6, 1);
    // la estación de servicio: tienda con puerta a la autopista, surtidores y un tanque
    int gx = brange(86, 108), north = hw > 44, gy0 = north ? hw - 17 : hw + 12;
    m3_gasx = gx; m3_gasy = north ? hw - 8 : hw + 8;
    x_fill(gx - 2, north ? hw - 11 : hw + 4, gx + 17, north ? hw - 3 : hw + 11, '=');
    for (int y = north ? hw - 11 : hw + 4; y <= (north ? hw - 3 : hw + 11); y++) for (int x = gx - 2; x <= gx + 17; x++) if (x_inb(x, y)) g_zo[y][x] = ' ';
    m3_bld(gx, gy0, gx + 11, gy0 + 5, north ? 1 : 0);
    for (int k = 0; k < 4; k++) m3_set(gx + 2 + 3 * k, north ? hw - 7 : hw + 8, 0, ':');
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) m3_set(gx + 14 + i, gy0 + 1 + j, 0, 'O');
    // el paso sobre nivel caído: una losa maciza sobre la autopista y tramos que siguen por el campo
    int ox = brange(122, 148);
    m3_ovx = ox;
    for (int y = hw - 4; y <= hw + 5; y++) for (int x = ox; x <= ox + 5; x++) m3_set(x, y, 0, (y + x) % 7 == 0 ? '#' : '/');
    for (int s = 0; s < 2; s++)
        for (int y = 1; y <= 20; y++) {
            int yy = s ? hw + 5 + y : hw - 4 - y;
            for (int x = ox; x <= ox + 5; x++) if (x_inb(x, yy) && g_zg[yy][x] != '=' && g_zo[yy][x] == ' ' && (int)(brnd() % 100) < 82 - y * 2) g_zo[yy][x] = '/';
        }
}

static void m3_terrain(int type)
{
    mn_terrain(type);   // [mapnuc]
    if (type == XT_HOSP) m3_hosp();
    else if (type == XT_PORT) m3_port();
    else if (type == XT_ROAD) m3_road(x_ey);
}

// ------------------------------------------------------------------ cajones y adornos (tras la primera inundación)
static int m3_crate(int l, int t, int r, int b, char ch)
{
    for (int k = 0; k < 24; k++) if (x_try(brange(l, r), brange(t, b), ch, 1)) return 1;
    return 0;
}
static void m3_row(int x, int y, int len, char ch)   // un camión: len casillas seguidas o nada
{
    for (int k = 0; k < len; k++)
        if (!x_try(x + k, y, ch, 0)) { for (int j = 0; j < k; j++) { g_zo[y][x + j] = ' '; } x_n = x_flood(); return; }
}
static void m3_props(int type)
{
    int t;
    mn_props(type);   // [mapnuc]
    if (type == XT_HOSP) {
        for (int i = 0; i < m3_nroom; i++) {
            int l = m3_room[i].l, tp = m3_room[i].t, r = m3_room[i].r, b = m3_room[i].b, k = m3_room[i].k;
            if (k == 2) { for (t = 0; t < 4; t++) m3_crate(l, tp, r, b, 'r'); for (t = 0; t < 3; t++) m3_crate(l, tp, r, b, 'c'); }
            else if (k == 3) { m3_crate(l, tp, r, b, 'r'); m3_crate(l, tp, r, b, 'c'); }
            else { m3_crate(l, tp, r, b, 'c'); if (k == 1) m3_crate(l, tp, r, b, 'c'); }
        }
        for (int i = 0; i < 6; i++) for (t = 0; t < 30; t++) if (x_block('"', brange(m3_hx0 - 26, m3_hx1 + 22), brange(m3_hy0 - 14, m3_hy1 + 12), 3, 2)) break;
        for (int i = 0; i < 12; i++)   // ambulancias y autos en el estacionamiento
            for (t = 0; t < 40; t++) {
                int x = brange(m3_hx0 - 5, m3_hx1 + 4), y = brange(m3_hy0 - 5, m3_hy1 + 5);
                if ((x >= m3_hx0 - 1 && x <= m3_hx1 + 1 && y >= m3_hy0 - 1 && y <= m3_hy1 + 1) || g_zg[y][x] != '=') continue;
                if (x_pair(x, y)) break;
            }
    } else if (type == XT_PORT) {
        for (int i = 0; i < 14; i++) for (t = 0; t < 30; t++) if (x_prop(brange(4, MW - 5), brange(m3_wy - 3, m3_wy + 14), ';')) break;
        for (int i = 0; i < 6; i++) for (t = 0; t < 30; t++) if (x_try(brange(m3_sx0 + 2, m3_sx1 - 2), brange(m3_sy0 + 1, m3_sy1 - 1), i < 3 ? 'r' : 'c', 1)) break;
        for (int i = 0; i < 8; i++) for (t = 0; t < 30; t++) if (x_block('C', brange(9, MW - 7), brange(m3_wy - 3, m3_wy - 1), 2, 1)) break;
    } else if (type == XT_ROAD) {
        for (int i = 0; i < 16; i++) for (t = 0; t < 20; t++) {   // camiones: una caja larga y la cabina
            int x = brange(8, MW - 12), y = brange(m3_hwy - 2, m3_hwy + 3);
            if (abs(x - m3_tollx) < 3 || abs(x - m3_ovx) < 8) continue;
            int before = x_nt; (void)before;
            m3_row(x, y, 4, i % 3 ? 'C' : 'K');
            if (g_zo[y][x] != ' ') break;
        }
        for (int i = 0; i < 70; i++) for (t = 0; t < 20; t++) {
            int x = brange(6, MW - 8), y = brange(m3_hwy - 2, m3_hwy + 3);
            if (abs(x - m3_tollx) < 3 || abs(x - m3_ovx) < 8) continue;
            if (x_pair(x, y)) break;
        }
        for (int i = 0; i < 24; i++) for (t = 0; t < 30; t++) {   // autos abandonados en el campo, junto a la orilla
            int x = brange(6, MW - 8), y = brange(6, MH - 8);
            if (g_zg[y][x] == '=' || abs(y - m3_hwy) > 16) continue;
            if (x_pair(x, y)) break;
        }
    }
}

// ------------------------------------------------------------------ enemigos propios
static void m3_enemies(int type)
{
    mn_enemies(type);   // [mapnuc]
    if (type == XT_HOSP) {   // muchos zombis adentro: dos o tres por sala y el pasillo lleno
        for (int i = 0; i < m3_nroom; i++) {
            int n = m3_room[i].k == 2 || m3_room[i].k == 3 ? 3 : 2;
            for (int k = 0; k < n; k++) x_enemy(brnd() % 4 == 0 ? 'm' : 'd', (m3_room[i].l + m3_room[i].r) / 2, (m3_room[i].t + m3_room[i].b) / 2, 4);
        }
        for (int j = 0; j < 7; j++) { x_enemy('d', m3_hx0 + 4 + 8 * j, m3_hy0 + 14, 2); if (j % 2) x_enemy('m', m3_hx0 + 4 + 8 * j, m3_hy0 + 15, 2); }
        for (int j = 0; j < 5; j++) x_enemy('d', brange(m3_hx0 - 5, m3_hx1 + 5), brange(m3_hy0 - 5, m3_hy1 + 5), 3);
    } else if (type == XT_PORT) {   // la tripulación del barco y los estibadores
        for (int k = 0; k < 6; k++) x_enemy(k % 3 == 0 ? 'm' : 'd', (m3_sx0 + m3_sx1) / 2 + brange(-8, 8), (m3_sy0 + m3_sy1) / 2, 3);
        for (int k = 0; k < 6; k++) x_enemy('d', m3_crx[k % 3] + 4, m3_cry + 2, 3);
    } else if (type == XT_ROAD) {   // gente atrapada en los autos y en la estación
        for (int k = 0; k < 5; k++) x_enemy('d', m3_gasx + 6, m3_gasy, 4);
        for (int k = 0; k < 4; k++) x_enemy(k % 2 ? 'm' : 'd', m3_ovx + 3, m3_hwy + brange(-3, 3), 4);
        for (int k = 0; k < 4; k++) x_enemy('d', m3_tollx + 1, m3_hwy, 3);
    }
}

// ------------------------------------------------------------------ botín propio
static const short *m3_pool(void)
{
    static const short HOS[] = {ITEM_MED, ITEM_BAND, ITEM_D_ANTIB, ITEM_BAND, ITEM_MED, ITEM_D_ANTIB, ITEM_D_ALCOHOL, ITEM_BAND};
    static const short POR[] = {ITEM_D_HERR, ITEM_D_COMBUS, ITEM_D_ELEC, ITEM_D_BATERIA, ITEM_D_HERR, ITEM_D_CINTA, ITEM_D_COMBUS, ITEM_D_ELEC};
    static const short ROD[] = {ITEM_D_COMBUS, ITEM_FOOD, ITEM_D_HERR, ITEM_D_COMBUS, ITEM_FOOD, ITEM_D_CINTA, ITEM_D_COBRE, ITEM_D_COMBUS};
    if (mn_is(g_x.type)) return mn_pool();   // [mapnuc]
    return g_x.type == XT_HOSP ? HOS : g_x.type == XT_PORT ? POR : ROD;
}

// ------------------------------------------------------------------ dibujo
static u32 m3_tinte(void) { if (mn_is(g_x.type)) return mn_tinte(); return g_x.type == XT_HOSP ? RGBA(214, 226, 226, 255) : g_x.type == XT_PORT ? RGBA(196, 214, 228, 255) : RGBA(232, 230, 214, 255); }

// sobre el suelo: agua del puerto, damero del hospital y marcas de la autopista
static void m3_suelo(int tx, int ty, float x, float y, int g)
{
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE) || !m3_is(g_x.type)) return;
    if (mn_is(g_x.type)) { mn_suelo(tx, ty, x, y, g); return; }   // [mapnuc]
    if (g_x.type == XT_PORT && g_o[ty][tx] == O_M3WAT) {
        int sh = ty > 0 && g_o[ty - 1][tx] != O_M3WAT;   // orilla: hay muelle o casco arriba
        r_rect(x, y, TS, TS, RGBA(20, 36, 48, 255));
        u32 h = hash2(tx, ty);
        float w = sinf(g_time * 1.3f + tx * 0.9f + ty * 0.6f);
        r_rect(x + 2, y + 5 + (int)(w * 2), 7, 1, RGBA(50, 84, 100, 200));
        r_rect(x + 8 + (h & 3), y + 11 - (int)(w * 2), 5, 1, RGBA(40, 70, 86, 200));
        if (sh) { r_rect(x, y, TS, 3, RGBA(10, 18, 24, 255)); r_rect(x, y + 3, TS, 1, RGBA(70, 110, 120, 140)); }
        return;
    }
    if (g_x.type == XT_HOSP && g == G_FLOOR) {
        if (((tx + ty) & 1) == 0) r_rect(x, y, TS, TS, RGBA(190, 214, 208, 26));
        if (g_o[ty][tx] == O_NONE && ty >= m3_hy0 + 13 && ty <= m3_hy0 + 16 && ty < m3_hy1) r_rect(x, y + TS / 2, TS, 1, RGBA(230, 230, 200, 28));
        return;
    }
    if (g_x.type == XT_ROAD && g == G_ROAD && m3_hwy >= 0 && ty >= m3_hwy - 2 && ty <= m3_hwy + 3) {
        if (ty == m3_hwy + 1) { r_rect(x, y - 1, TS, 1, RGBA(212, 176, 52, 200)); r_rect(x, y + 1, TS, 1, RGBA(212, 176, 52, 200)); }
        else if (ty == m3_hwy - 2) r_rect(x, y + 1, TS, 1, RGBA(190, 190, 184, 170));
        else if (ty == m3_hwy + 3) r_rect(x, y + TS - 2, TS, 1, RGBA(190, 190, 184, 170));
        else if ((tx & 3) < 2 && (ty == m3_hwy - 1 || ty == m3_hwy + 2)) r_rect(x, ty == m3_hwy - 1 ? y + TS - 1 : y + TS - 1, TS, 1, RGBA(170, 170, 164, 140));
    }
}

static void m3_draw_obj(int tx, int ty, int o)
{
    float x = tx * TS - g_ox, y = ty * TS - g_oy, yb = y + TS;
    u32 h = hash2(tx, ty);
    switch (o) {
    case O_M3HUL:   // casco de acero oxidado con remaches y línea de flotación
        r_rect(x, yb - 22, TS, 22, RGBA(24, 20, 22, 255));
        r_rect(x, yb - 21, TS, 19, RGBA(88, 44, 38, 255));
        r_rect(x, yb - 21, TS, 3, RGBA(124, 112, 100, 255));
        r_rect(x, yb - 12, TS, 1, RGBA(60, 30, 28, 255));
        r_rect(x + 3 + (h & 7), yb - 16, 2, 2, RGBA(128, 78, 56, 255));
        r_rect(x + 8, yb - 8, 3, 2, RGBA(150, 96, 60, 255));
        if (ty + 1 < MH && g_o[ty + 1][tx] == O_M3WAT) r_rect(x, yb - 4, TS, 4, RGBA(30, 30, 34, 255));
        break;
    case O_M3CRN: {   // pata de una grúa pórtico y, entre las dos patas, la viga con el carro y un contenedor colgado
        r_rect(x + 3, yb - 3, 10, 3, RGBA(18, 18, 20, 255));
        r_rect(x + 5, yb - 70, 6, 68, RGBA(20, 18, 14, 255));
        r_rect(x + 6, yb - 70, 4, 68, RGBA(168, 138, 54, 255));
        for (int k = 0; k < 8; k++) r_rect(x + 6, yb - 66 + k * 8, 4, 1, RGBA(100, 82, 34, 255));
        r_rect(x + 6, yb - 70, 1, 68, RGBA(210, 180, 84, 255));
        int px = 0;
        for (int d = 1; d <= 10 && !px; d++) if (tx + d < MW && g_o[ty][tx + d] == O_M3CRN) px = d;
        if (px) {
            float bx = x + 5, bw = px * TS + 6;
            r_rect(bx, yb - 78, bw, 9, RGBA(20, 18, 14, 255));
            r_rect(bx + 1, yb - 77, bw - 2, 6, RGBA(180, 148, 56, 255));
            r_rect(bx + 1, yb - 77, bw - 2, 1, RGBA(214, 184, 90, 255));
            float cx = bx + bw * (0.3f + 0.4f * (0.5f + 0.5f * sinf(g_time * 0.3f + tx)));
            r_rect(cx, yb - 80, 12, 6, RGBA(60, 60, 66, 255));
            r_rect(cx + 6, yb - 74, 1, 28, RGBA(30, 30, 34, 255));
            r_rect(cx, yb - 46, 13, 8, RGBA(20, 14, 12, 255));
            r_rect(cx + 1, yb - 45, 11, 6, h & 1 ? RGBA(120, 62, 44, 255) : RGBA(52, 82, 98, 255));
            r_rect(bx + bw - 3, yb - 78, 28, 4, RGBA(168, 138, 54, 255));   // el brazo hacia el agua
        }
        break;
    }
    case O_M3BED:   // camilla de hospital: marco, colchón, almohada, manta y ruedas
        r_rect(x, yb - 12, TS, 11, RGBA(20, 22, 24, 255));
        r_rect(x + 1, yb - 11, 14, 8, RGBA(150, 156, 160, 255));
        r_rect(x + 2, yb - 10, 12, 6, RGBA(214, 218, 214, 255));
        r_rect(x + 2, yb - 10, 4, 3, RGBA(240, 240, 232, 255));
        r_rect(x + 6, yb - 9, 8, 5, h % 3 == 0 ? RGBA(150, 70, 66, 255) : RGBA(96, 140, 150, 255));
        r_rect(x + 2, yb - 3, 2, 3, RGBA(30, 30, 34, 255)); r_rect(x + 12, yb - 3, 2, 3, RGBA(30, 30, 34, 255));
        break;
    case O_M3TOL:   // caseta de peaje con techo rojo y ventanilla
        r_rect(x + 1, yb - 26, 14, 26, RGBA(20, 20, 22, 255));
        r_rect(x + 2, yb - 22, 12, 21, RGBA(138, 130, 112, 255));
        r_rect(x + 4, yb - 18, 8, 7, RGBA(34, 52, 62, 255)); r_rect(x + 4, yb - 18, 8, 1, RGBA(110, 150, 160, 255));
        r_rect(x, yb - 29, TS, 6, RGBA(150, 54, 44, 255)); r_rect(x, yb - 27, TS, 1, RGBA(230, 226, 214, 255));
        r_rect(x + 2, yb - 4, 12, 3, RGBA(58, 56, 52, 255));
        break;
    case O_M3PMP:   // surtidor
        r_rect(x + 3, yb - 21, 10, 21, RGBA(18, 14, 14, 255));
        r_rect(x + 4, yb - 20, 8, 17, RGBA(174, 44, 38, 255));
        r_rect(x + 5, yb - 18, 6, 5, RGBA(26, 66, 56, 255)); r_rect(x + 5, yb - 18, 6, 1, RGBA(120, 200, 160, 255));
        r_rect(x + 3, yb - 3, 10, 3, RGBA(40, 40, 44, 255));
        r_rect(x + 12, yb - 12, 2, 8, RGBA(20, 20, 20, 255));
        break;
    case O_M3NET:
        if (h & 1) {   // boya
            r_rect(x + 4, yb - 12, 8, 11, RGBA(16, 14, 14, 255));
            r_rect(x + 5, yb - 11, 6, 9, RGBA(210, 100, 46, 255)); r_rect(x + 5, yb - 8, 6, 2, RGBA(228, 224, 210, 255));
        } else {   // red enredada
            r_rect(x + 1, yb - 9, 14, 8, RGBA(18, 22, 18, 255));
            r_rect(x + 2, yb - 8, 12, 6, RGBA(58, 78, 60, 255));
            for (int k = 0; k < 3; k++) { r_rect(x + 2 + k * 4, yb - 8, 1, 6, RGBA(28, 40, 30, 255)); r_rect(x + 2, yb - 7 + k * 2, 12, 1, RGBA(28, 40, 30, 255)); }
        }
        break;
    case O_M3TNT: {   // carpa de triaje: lona clara con una cruz roja
        int top = ty == 0 || g_o[ty - 1][tx] != O_M3TNT;
        r_rect(x, yb - 22, TS, 22, RGBA(24, 24, 22, 255));
        r_rect(x, yb - 21, TS, 20, RGBA(168, 172, 158, 255));
        if (top) { r_rect(x, yb - 21, TS, 4, RGBA(210, 212, 200, 255)); r_rect(x, yb - 17, TS, 1, RGBA(110, 112, 100, 255)); }
        if (h % 3 == 0) { r_rect(x + 6, yb - 14, 4, 10, RGBA(190, 40, 38, 255)); r_rect(x + 3, yb - 11, 10, 4, RGBA(190, 40, 38, 255)); }
        r_rect(x, yb - 2, TS, 2, RGBA(110, 110, 100, 255));
        break;
    }
    case O_M3MED:   // estantería de farmacia: cajas blancas, frascos y cruces verdes
        r_rect(x, yb - 22, TS, 22, RGBA(18, 20, 20, 255));
        r_rect(x + 1, yb - 21, 14, 20, RGBA(136, 142, 140, 255));
        for (int k = 0; k < 3; k++) {
            float by = yb - 21 + k * 7;
            r_rect(x + 1, by + 5, 14, 2, RGBA(80, 86, 86, 255));
            r_rect(x + 2, by + 1, 4, 4, RGBA(228, 230, 222, 255)); r_rect(x + 3, by + 2, 2, 1, RGBA(60, 150, 90, 255));
            r_rect(x + 7, by + 1, 3, 4, ((h >> k) & 1) ? RGBA(200, 120, 50, 255) : RGBA(70, 110, 170, 255));
            r_rect(x + 11, by + 2, 3, 3, RGBA(220, 222, 214, 255));
        }
        break;
    case O_M3RUB:   // losa de hormigón caída con fierros
        r_rect(x, yb - 14, TS, 14, RGBA(20, 20, 20, 255));
        r_rect(x + 1, yb - 13, 14, 11, RGBA(112, 110, 104, 255));
        r_rect(x + 1, yb - 13, 14, 3, RGBA(146, 144, 136, 255));
        r_rect(x + 3 + (h & 5), yb - 9, 5, 4, RGBA(82, 80, 76, 255));
        r_rect(x + 2 + (h & 3), yb - 17, 1, 8, RGBA(150, 82, 44, 255)); r_rect(x + 10, yb - 16, 1, 6, RGBA(150, 82, 44, 255));
        break;
    default: break;
    }
}

// ------------------------------------------------------------------ pruebas (solo en la versión de pruebas)
#ifdef PEPEL_DEBUG
static void dbg_logf(const char *fmt, ...);
static void zone_build(int z);
static void m3_test(void)
{
    static unsigned char reach[MHMAX][MWMAX];
    static short q[MWMAX * MHMAX];
    static const int TY[3] = {XT_HOSP, XT_PORT, XT_ROAD};
    Exped keep = g_x;
    for (int ti = 0; ti < 3; ti++) {
        int good = 0, bad = 0;
        for (int i = 0; i < 10; i++) {
            g_x = (Exped){(0x9E3779B1u * (u32)(i + 1 + 40 * ti)) | 1, 1 + (i * 2) % 12, TY[ti], (i % 3 == 0 ? XF_HATCH : 0) | (i % 4 == 0 ? XF_JOB : 0) | (i % 2 ? XF_SAWED : 0), i % 10};
            g_x.ev0 = 1 + i % 4; g_x.ev1 = g_x.danger >= 4 ? 1 + (i + 2) % 4 : 0;
            if (g_x.ev1 == g_x.ev0) g_x.ev1 = 1 + g_x.ev1 % 4;
            zone_build(1);
            int why = 0, ex = g_ret_tx, ey = g_ret_ty, h = 0, t = 0;
            memset(reach, 0, sizeof(reach));
            reach[ey][ex] = 1; q[t++] = (short)(ey * MW + ex);
            while (h < t) {
                int c = q[h++], x = c % MW, y = c / MW;
                for (int k = 0; k < 4; k++) {
                    int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
                    if (in_map(nx, ny) && !reach[ny][nx] && !solid(nx, ny)) { reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }
                }
            }
#define M3NEAR(x, y) (reach[y][x] || ((x) > 0 && reach[y][(x) - 1]) || ((x) < MW - 1 && reach[y][(x) + 1]) || ((y) > 0 && reach[(y) - 1][x]) || ((y) < MH - 1 && reach[(y) + 1][x]))
            int loot = 0;
            for (int c = 0; c < g_ncr; c++) {
                if (g_cr[c].kind == CK_LOOT || g_cr[c].kind == CK_RICH) loot++;
                if (!M3NEAR(g_cr[c].tx, g_cr[c].ty)) why |= 1;
            }
            for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (g_g[y][x] == G_DOOR && !reach[y][x]) why |= 2;
            if (!reach[g_ret2_ty][g_ret2_tx] || !reach[g_ret3_ty][g_ret3_tx]) why |= 4;
            for (int e = 0; e < g_ne; e++) if (solid((int)(g_e[e].x / TS), (int)(g_e[e].y / TS))) why |= 8;
            if (loot < 30) why |= 16;
            if (g_ncr > MAXCR || g_nlamp >= MAXLAMP || g_nfire >= MAXFIRE || g_ne > MAXE || g_ne < 20) why |= 32;
            if (g_x_nbld < 14) why |= 64;
            if (why) { bad++; dbg_logf("MAPAS3 fallo tipo %d semilla %d codigo %d (bld %d loot %d ne %d lamp %d)", TY[ti], i, why, g_x_nbld, loot, g_ne, g_nlamp); }
            else good++;
        }
        dbg_logf("MAPAS3 tipo %d: bien %d mal %d", TY[ti], good, bad);
    }
    g_x = keep;
}
static int m3_tpgo(int tx, int ty)   // lleva al jugador a la casilla libre más cercana
{
    for (int r = 0; r < 8; r++)
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++)
            if (in_map(tx + dx, ty + dy) && !solid(tx + dx, ty + dy)) { P.x = (tx + dx) * TS + 8; P.y = (ty + dy) * TS + 8; P.lock = -1; return 1; }
    return 0;
}
static int m3_dbg(const char *name, int a)
{
    if (!strcmp(name, "m3test")) { m3_test(); return 1; }
    if (!strcmp(name, "m3tp")) {   // 1-3 hospital: portón, pasillo, farmacia; 4-5 puerto: grúa, barco; 6-8 autopista: peaje, estación, paso caído
        switch (a) {
        case 1: m3_tpgo(m3_hx0 - 7, m3_hy0 + 14); break;
        case 2: m3_tpgo(m3_hx0 + 20, m3_hy0 + 14); break;
        case 3: for (int i = 0; i < m3_nroom; i++) if (m3_room[i].k == 2) m3_tpgo((m3_room[i].l + m3_room[i].r) / 2, (m3_room[i].t + m3_room[i].b) / 2); break;
        case 4: m3_tpgo(m3_crx[1] + 4, m3_cry + 3); break;
        case 5: m3_tpgo(m3_sx0 + 16, m3_wy + 1); break;
        case 6: m3_tpgo(m3_tollx - 4, m3_hwy); break;
        case 7: m3_tpgo(m3_gasx + 6, m3_gasy + 1); break;
        case 8: m3_tpgo(m3_ovx - 5, m3_hwy); break;
        case 9: m3_tpgo(m3_hx0 + 22, m3_hy1 + 3); break;
        }
        return 1;
    }
    return 0;
}
#endif
