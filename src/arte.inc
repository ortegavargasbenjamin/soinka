// [arte] Suelo propio de cada tipo de mapa: 4 variantes por hash y manchones grandes (barro, aceite, rejilla)
// que hacen de transicion. Devuelve el sprite o -1 si no toca (nieve, jefes, zonas fuera de expedicion).
#define AR(n) S_AR_##n##0
static int ar_suelo(int g, u32 h, int tx, int ty)
{
    if (g_zone != 1 || (g_x.flags & XF_ZJEFE) || snowy()) return -1;
    if (g == G_DOOR || g == G_RAIL || g == G_RAD) return -1;
    int v = (int)(h % 4), blob = (int)(hash2(tx / 3 + 50, ty / 3 + 70) % 5);   // manchon de 3x3 casillas
    int road = g == G_ROAD || g == G_EXIT || g == G_ZONE;
    switch (g_x.type) {
    case XT_CITY: return road ? AR(ASF) + v : (blob < 3 ? AR(LOS) : AR(HOR)) + v;
    case XT_FACTORY:
        if (g == G_FLOOR) { int nr = ty > 0 && g_g[ty - 1][tx] == G_DOOR; return nr ? AR(PEL) + v : blob == 0 ? AR(ACE) + v : blob == 1 && v == 0 ? AR(REJ) : AR(HOR) + v; }
        return road ? AR(ASF) + v : blob < 2 ? AR(BAL) + v : blob < 4 ? AR(TIE) + v : AR(LOS) + v;   // [fab2] nave de hormigon claro; afuera asfalto, grava y tierra
    case XT_TRAIN: return road ? AR(ADO) + v : (blob == 0 ? AR(TIE) : AR(BAL)) + v;
    case XT_TOWN: return road ? AR(ADO) + v : (blob < 2 ? AR(BAR) : AR(TIE)) + v;
    case XT_FOREST: case XT_PINE: return road ? AR(BAR) + v : blob == 0 ? AR(BAR) + v : AR(HOJ) + v;
    case XT_GRAVE: return road ? AR(GRA) + v : (blob < 3 ? AR(PAS) : AR(GRA)) + v;
    case XT_MINE: return road ? AR(GRA) + v : AR(ROC) + v;
    case XT_HOSP: return road ? AR(ASF) + v : g == G_FLOOR ? AR(LOS) + v : (blob < 3 ? AR(PAS) : AR(GRA)) + v;   // [mapas3]
    case XT_PORT: return road ? AR(HOR) + v : g == G_FLOOR ? AR(ACE) + v : (blob < 3 ? AR(BAL) : AR(GRA)) + v;   // [mapas3]
    case XT_NUC: return road ? AR(ASF) + v : g == G_FLOOR ? AR(ACE) + v : (blob < 3 ? AR(HOR) : AR(GRA)) + v;   // [mapnuc]
    case XT_CURSED: return mn_suelo_spr(g, h);   // [mapnuc]
    case XT_ROAD: return road ? AR(ASF) + v : g == G_FLOOR ? AR(LOS) + v : (blob < 2 ? AR(PAS) : blob < 4 ? AR(TIE) : AR(HOJ)) + v;   // [mapas3]
    }
    return -1;
}

// ------------------------------------------------------------------ [arte] fabrica abandonada
static int ar_fab(void) { return g_zone == 1 && g_x.type == XT_FACTORY && !snowy(); }
// chimenea en el muro de arriba de una nave (1 de cada 8 casillas)
static int ar_chim(int tx, int ty) { return ty > 0 && ty + 1 < MH && g_o[ty - 1][tx] == O_NONE && g_g[ty + 1][tx] == G_FLOOR && g_g[ty - 1][tx] != G_FLOOR && hash2(tx * 7 + 9, ty * 3 + 4) % 6 == 0; }   // [fab2] solo en el muro de arriba
// farola de sodio en la fachada
static int ar_sodio(int tx, int ty) { return !ar_chim(tx, ty) && hash2(tx * 5 + 2, ty * 11 + 1) % 7 == 3; }
// humo: bocanadas grises que suben y se abren
static void ar_humo(float cx, float cy, float t0, int n, int a0)
{
    for (int k = 0; k < n; k++) {
        float t = fmodf(g_time * 0.35f + k / (float)n + t0, 1.0f), sz = 4 + 12 * t;
        r_rect(floorf(cx + sinf(g_time * 0.8f + k * 1.9f + t0 * 7) * (2 + 8 * t) - sz / 2), floorf(cy - t * 70), sz, sz * 0.8f, RGBA(86, 84, 88, (int)(a0 * (1 - t))));
    }
}
// [fab2] partes de la nave que se deducen de la grilla (nada se guarda): muro norte, porton, pasarela, cadenas
static int ar_kind(int tx, int ty) { static const unsigned char K[10] = {0, 0, 1, 1, 2, 2, 3, 4, 5, 5}; return K[hash2(tx * 3 + 1, ty * 5 + 2) % 10]; }
static int ar_flo(int tx, int ty) { return in_map(tx, ty) && g_g[ty][tx] == G_FLOOR; }
static int ar_norte(int tx, int ty) { return ar_flo(tx, ty + 1) && !ar_flo(tx, ty - 1); }   // muro de arriba de una nave
static int ar_porton(int tx, int ty) { return (tx > 0 && g_g[ty][tx - 1] == G_DOOR) || (tx + 1 < MW && g_g[ty][tx + 1] == G_DOOR); }
static int ar_pasa(int tx, int ty)   // fila de la pasarela elevada: la cuarta de la nave, en dos de cada tres naves
{
    return ty >= 3 && ar_flo(tx, ty) && g_o[ty - 3][tx] == O_WALL && ar_flo(tx, ty - 1) && ar_flo(tx, ty - 2) && ar_flo(tx, ty + 1) && hash2(77, ty - 3) % 3 != 0;
}
static int ar_cadena(int tx, int ty) { return ar_flo(tx, ty) && g_o[ty][tx] == O_NONE && !ar_pasa(tx, ty) && !ar_pasa(tx, ty + 1) && hash2(tx * 5 + 7, ty * 3 + 11) % 15 == 0; }
static float ar_dia(void)   // 1 de dia, 0 de noche, con rampas al amanecer y al atardecer
{
    float a = (g_hora - 6.5f) / 1.5f, b = (18.5f - g_hora) / 1.5f;
    return clampf(a, 0, 1) * clampf(b, 0, 1);
}
// vapor blanco que escapa a chorros de una tuberia rota
static void ar_vapor(float cx, float cy, float t0, int dir)
{
    for (int k = 0; k < 6; k++) {
        float t = fmodf(g_time * 0.9f + k / 6.0f + t0, 1.0f), sz = 2 + 9 * t;
        r_rect(floorf(cx + dir * t * 22 - sz / 2), floorf(cy - t * 26 - sz / 2), sz, sz * 0.8f, RGBA(196, 198, 204, (int)(130 * (1 - t))));
    }
}
static int ar_obj(int tx, int ty, int o)
{
    if (!ar_fab()) return 0;
    float x = tx * TS - g_ox, y = (ty + 1) * TS - g_oy;
    u32 h = hash2(tx * 3 + 1, ty * 5 + 2);
    if (o == O_WALL) {
        int k = ar_kind(tx, ty), port = ar_porton(tx, ty);
        if (ar_norte(tx, ty) && !port) r_spr(SPR[S_FAB_TECHO0 + (int)(h >> 8) % 3], x, y - 28 - 15, dz_tinte_muro());   // [fab2] diente de sierra
        r_spr(SPR[port ? S_FAB_PORT : S_FAB_W0 + k], x, y - 28, dz_tinte_muro());   // [fab2] porton corrido junto a la puerta
        if (ar_chim(tx, ty)) {
            r_spr(SPR[S_FAB_CHIM], x, y - 18 - 84, COL_WHITE);
            ar_humo(x + 8, y - 102, hash2(tx, ty) % 100 / 100.0f, 5, 120);
        } else if (ar_sodio(tx, ty)) {
            r_rect(x + 7, y - 17, 2, 3, RGBA(30, 30, 32, 255)); r_rect(x + 5, y - 14, 6, 2, RGBA(236, 170, 70, 255));
        } else if (k == 2 && !port && (h >> 5) % 3 == 0) ar_vapor(x + 13, y - 10, (h % 40) / 40.0f, (h & 1) ? 1 : -1);   // [fab2] tuberia rota
        return 1;
    }
    if (o == O_FENCE) {
        int hz = (tx > 0 && g_o[ty][tx - 1] == O_FENCE) || (tx + 1 < MW && g_o[ty][tx + 1] == O_FENCE);
        r_spr(SPR[hz ? S_FAB_VALLA0 : S_FAB_VALLA1], x, y - 22, COL_WHITE);
        return 1;
    }
    if (o == O_STANK) {
        int nl = 0, nu = 0;
        while (tx - nl > 0 && g_o[ty][tx - nl - 1] == O_STANK) nl++;
        while (ty - nu > 0 && g_o[ty - nu - 1][tx] == O_STANK) nu++;
        if ((nl & 1) || (nu & 1)) return 1;   // las otras casillas del bloque no dibujan
        float by = y + TS;   // base del bloque de 2x2
        int silo = hash2(tx, ty) & 1;
        r_spr(SPR[silo ? S_FAB_SILO : S_FAB_TORRE], x, by - 48, COL_WHITE);
        if (!silo) ar_humo(x + 16, by - 46, hash2(tx, ty) % 50 / 50.0f, 6, 90);
        return 1;
    }
    if (o == O_MACH) {
        int k = (int)(h % 9);   // [fab2] 0-3 prensa, armario, motor, estanque; 4-6 caldera, torno, casilleros; 7-8 cinta
        if (k >= 7) { r_spr(SPR[S_FAB_CINTA0 + ((int)(g_time * 4) & 1)], x, y - 14, COL_WHITE); if (k == 8) r_rect(x + 4, y - 18, 8, 5, RGBA(120, 86, 52, 255)); }
        else if (k >= 4) r_spr(SPR[S_FAB_N0 + k - 4], x, y - 24, COL_WHITE);
        else r_spr(SPR[S_FAB_M0 + k], x, y - 22, COL_WHITE);
        return 1;
    }
    return 0;
}
// [fab2] el suelo de la nave: lineas amarillas de transito, marco de seguridad de las maquinas, charcos de aceite y sombras
static void ar_piso(int tx, int ty, float x, float y, int g)
{
    if (g == G_FLOOR) {
        if (ar_flo(tx, ty - 1) && ty >= 2 && g_o[ty - 2][tx] == O_WALL) r_rect(x, y + 2, TS, 2, RGBA(176, 146, 44, 190));
        if (ar_flo(tx, ty + 1) && ty + 2 < MH && g_o[ty + 2][tx] == O_WALL) r_rect(x, y + 12, TS, 2, RGBA(176, 146, 44, 190));
        if (g_o[ty][tx] == O_MACH) {   // marco de peligro alrededor de la maquina
            for (int k = 0; k < 4; k++) { u32 c = k & 1 ? RGBA(20, 20, 22, 220) : RGBA(184, 150, 40, 220); r_rect(x + k * 4, y, 4, 1, c); r_rect(x + k * 4, y + TS - 1, 4, 1, c); r_rect(x, y + k * 4, 1, 4, c); r_rect(x + TS - 1, y + k * 4, 1, 4, c); }
        } else if (hash2(tx * 11 + 5, ty * 17 + 3) % 11 == 0) {   // charco que refleja la luz
            r_rect(x + 2, y + 6, 12, 5, RGBA(10, 12, 18, 170)); r_rect(x + 4, y + 5, 7, 7, RGBA(10, 12, 18, 170));
            r_rect(x + 5, y + 7, 4, 1, RGBA(150, 170, 190, 50 + (int)(40 * sinf(g_time * 1.7f + tx))));
            r_rect(x + 9, y + 9, 2, 1, RGBA(230, 160, 80, 60));
        }
        if (ar_pasa(tx, ty)) r_rect(x, y - 8, TS, 6, RGBA(0, 0, 0, 80));   // sombra de la pasarela
        if (ar_cadena(tx, ty)) r_rect(x + 6, y + 10, 5, 2, RGBA(0, 0, 0, 90));
    } else if (g != G_ROAD && g != G_DOOR && g_o[ty][tx] == O_NONE && hash2(tx * 13 + 1, ty * 7 + 9) % 9 == 0) {   // chatarra y tambores en el patio
        u32 q = hash2(tx, ty * 5);
        if (q % 3 == 0) { r_rect(x + 3, y + 4, 9, 9, RGBA(16, 14, 12, 200)); r_rect(x + 4, y + 4, 7, 7, RGBA(112, 60, 34, 255)); r_rect(x + 5, y + 5, 5, 5, RGBA(80, 40, 24, 255)); r_rect(x + 5, y + 7, 5, 1, RGBA(140, 80, 44, 255)); }   // tambor tumbado visto desde arriba
        else if (q % 3 == 1) { r_rect(x + 1, y + 8, 12, 4, RGBA(84, 52, 36, 255)); r_rect(x + 1, y + 8, 12, 1, RGBA(130, 86, 56, 255)); r_rect(x + 4, y + 5, 9, 2, RGBA(70, 70, 74, 255)); r_rect(x + 2, y + 12, 13, 1, RGBA(0, 0, 0, 120)); }   // planchas oxidadas
        else { for (int k = 0; k < 4; k++) r_rect(x + 2 + k * 3, y + 3 + k, 1, 10, RGBA(110, 70, 48, 255)); r_rect(x + 1, y + 13, 14, 1, RGBA(0, 0, 0, 110)); }   // fierros
    } else if (g == G_ROAD) {   // linea de pintura discontinua al medio de los pasillos de dos casillas
        if (tx + 1 < MW && tx > 0 && g_g[ty][tx + 1] == G_ROAD && g_g[ty][tx - 1] != G_ROAD && (ty & 1)) r_rect(x + 15, y + 3, 2, 10, RGBA(176, 150, 52, 120));
        if (ty + 1 < MH && ty > 0 && g_g[ty + 1][tx] == G_ROAD && g_g[ty - 1][tx] != G_ROAD && (tx & 1)) r_rect(x + 3, y + 15, 10, 2, RGBA(176, 150, 52, 120));
    }
}
// [fab2] tuberias elevadas sobre un pasillo, de muro a muro, con soportes, bridas y una fuga de vapor
static void ar_tubos(int tx, int ty, float yt)
{
    int l = 0, r = 0;
    while (l < 7 && tx - l - 1 > 0 && g_o[ty][tx - l - 1] != O_WALL) l++;
    while (r < 7 && tx + 2 + r < MW - 1 && g_o[ty][tx + 2 + r] != O_WALL) r++;
    float xa = (tx - l) * TS - g_ox - (l < 7 ? 2 : 0), xb = (tx + 2 + r) * TS - g_ox + (r < 7 ? 2 : 0), w = xb - xa;
    if (l == 7) r_rect(xa, yt - 44, 3, 52, RGBA(34, 34, 38, 255));
    if (r == 7) r_rect(xb - 3, yt - 44, 3, 52, RGBA(34, 34, 38, 255));
    float p0 = (tx - 1) * TS - g_ox + 8, p1 = (tx + 2) * TS - g_ox + 4;   // soportes a cada lado del pasillo
    r_rect(p0, yt - 44, 4, 52, RGBA(22, 22, 24, 255)); r_rect(p0 + 1, yt - 44, 1, 52, RGBA(96, 96, 100, 255));
    r_rect(p1, yt - 44, 4, 52, RGBA(22, 22, 24, 255)); r_rect(p1 + 1, yt - 44, 1, 52, RGBA(96, 96, 100, 255));
    r_rect(xa, yt - 46, w, 6, RGBA(16, 14, 14, 255)); r_rect(xa, yt - 45, w, 4, RGBA(116, 70, 44, 255)); r_rect(xa, yt - 45, w, 1, RGBA(160, 104, 70, 255));
    r_rect(xa, yt - 38, w, 4, RGBA(16, 18, 20, 255)); r_rect(xa, yt - 37, w, 2, RGBA(76, 96, 104, 255)); r_rect(xa, yt - 37, w, 1, RGBA(120, 140, 148, 255));
    for (float fx = xa + 10; fx < xb - 4; fx += 22) { r_rect(fx, yt - 47, 2, 8, RGBA(30, 28, 28, 255)); r_rect(fx, yt - 39, 2, 6, RGBA(30, 30, 32, 255)); }
    r_rect(p0 - 2, yt - 34, (p1 + 6) - (p0 - 2), 2, RGBA(26, 26, 28, 255));   // travesano
    if (hash2(tx, ty) % 2 == 0) ar_vapor(xa + w * 0.55f, yt - 44, (tx % 10) / 10.0f, -1);
}
// sobre todo lo demas: grua portico, tuberias elevadas, pasarelas, cadenas, dinteles de los portones y haces de luz
static void ar_over(void)
{
    if (!ar_fab()) return;
    int tx0 = (int)g_ox / TS - 8, ty0 = (int)g_oy / TS - 2, tx1 = tx0 + SCR_W / TS + 16, ty1 = ty0 + SCR_H / TS + 6;
    float dia = ar_dia();
    for (int ty = ty0 < 3 ? 3 : ty0; ty <= ty1 && ty < MH - 1; ty++)
        for (int tx = tx0 < 2 ? 2 : tx0; tx <= tx1 && tx < MW - 3; tx++) {
            float x = tx * TS - g_ox, yt = ty * TS - g_oy;
            int g = g_g[ty][tx];
            if (g == G_ROAD && g_g[ty][tx + 1] == G_ROAD && g_g[ty][tx - 1] != G_ROAD && g_g[ty][tx + 2] != G_ROAD && ty % 9 == 4) { ar_tubos(tx, ty, yt); continue; }
            if (x < -16 || x > SCR_W + 16) continue;
            if (g == G_FLOOR) {
                if (ar_pasa(tx, ty)) {   // pasarela de rejilla con baranda, a 26 px del suelo
                    if (tx % 4 == 0) { r_rect(x + 6, yt - 12, 3, 20, RGBA(26, 28, 30, 255)); r_rect(x + 6, yt - 12, 1, 20, RGBA(92, 96, 100, 255)); }
                    r_rect(x, yt - 18, TS, 6, RGBA(16, 18, 20, 255)); r_rect(x, yt - 17, TS, 3, RGBA(84, 90, 96, 255));
                    for (int k = 1; k < 16; k += 3) r_rect(x + k, yt - 16, 1, 2, RGBA(30, 32, 36, 255));
                    r_rect(x, yt - 14, TS, 2, RGBA(150, 122, 40, 255));   // canto amarillo
                    r_rect(x, yt - 28, TS, 1, RGBA(160, 160, 164, 255)); r_rect(x, yt - 23, TS, 1, RGBA(110, 110, 114, 255));
                    r_rect(x, yt - 28, 1, 10, RGBA(120, 120, 124, 255)); r_rect(x + 8, yt - 28, 1, 10, RGBA(120, 120, 124, 255));
                } else if (ar_cadena(tx, ty)) {   // cadena con gancho que se mece
                    float cx = floorf(x + 8 + sinf(g_time * 1.3f + tx) * 1.2f);
                    for (int k = 0; k < 12; k++) r_rect(cx - (k & 1), yt - 46 + k * 4, 1 + (k & 1) * 2, 3 - (k & 1), RGBA(96, 96, 102, 255));
                    r_rect(cx - 1, yt + 2, 3, 2, RGBA(70, 70, 76, 255)); r_rect(cx + 1, yt + 4, 1, 3, RGBA(130, 130, 136, 255)); r_rect(cx - 2, yt + 7, 3, 1, RGBA(130, 130, 136, 255)); r_rect(cx - 2, yt + 5, 1, 2, RGBA(130, 130, 136, 255));
                }
                if (dia > 0.05f && g_o[ty - 1][tx] == O_WALL && ar_norte(tx, ty - 1) && tx % 4 == 1) {   // haz de luz del lucernario con polvo
                    for (int k = 0; k < 14; k++) r_rect(floorf(x + k * 1.6f), yt - 24 + k * 4, 12, 4, RGBA(255, 232, 180, (int)(44 * dia)));
                    for (int k = 0; k < 5; k++) {
                        float t = fmodf(g_time * 0.06f + k * 0.21f + tx * 0.13f, 1.0f);
                        r_rect(floorf(x + 3 + t * 22 + sinf(g_time + k) * 2), floorf(yt - 22 + t * 54), 1, 1, RGBA(255, 244, 220, (int)(210 * dia)));
                    }
                }
            }
            if (g == G_DOOR && (g_o[ty][tx - 1] == O_WALL || g_o[ty][tx + 1] == O_WALL)) {   // dintel del porton con franja de peligro
                float y = yt + TS;
                r_rect(x - 2, y - 30, TS + 4, 5, RGBA(18, 18, 20, 255));
                for (int k = 0; k < 5; k++) r_rect(x - 1 + k * 4, y - 29, 2, 3, RGBA(170, 136, 40, 255));
                r_rect(x + 6, y - 34, 4, 4, RGBA(90, 20, 18, 255));   // baliza
            }
            if (g != G_ROAD || g_g[ty][tx + 1] != G_ROAD || g_g[ty][tx - 1] == G_ROAD || g_g[ty][tx + 2] == G_ROAD) continue;
            if (hash2(tx * 13 + 3, (ty / 6) * 7) % 5 != 0 || ty % 6 != 0) continue;
            float x0 = (tx - 1) * TS - g_ox, x1 = (tx + 2) * TS - g_ox + 8, y = (ty + 1) * TS - g_oy;
            r_rect(x0, y - 44, 4, 44, RGBA(18, 18, 20, 255)); r_rect(x0 + 1, y - 44, 2, 44, RGBA(150, 126, 52, 255));
            r_rect(x1 - 4, y - 44, 4, 44, RGBA(18, 18, 20, 255)); r_rect(x1 - 3, y - 44, 2, 44, RGBA(150, 126, 52, 255));
            r_rect(x0 - 3, y - 50, x1 - x0 + 6, 7, RGBA(18, 18, 20, 255)); r_rect(x0 - 2, y - 49, x1 - x0 + 4, 5, RGBA(170, 142, 56, 255));
            r_rect(x0 - 2, y - 49, x1 - x0 + 4, 1, RGBA(206, 176, 78, 255));
            float cx = x0 + 6 + (x1 - x0 - 16) * (0.5f + 0.5f * sinf(g_time * 0.4f + tx));
            r_rect(cx, y - 52, 8, 5, RGBA(70, 70, 74, 255)); r_rect(cx + 3, y - 47, 1, 24, RGBA(40, 40, 44, 255));
            r_rect(cx + 1, y - 23, 5, 4, RGBA(150, 150, 154, 255));
        }
}
// lo que brilla: farolas de sodio que parpadean, balizas rojas en chimeneas y portones, chispas de los tableros
static void ar_emisivo(void)
{
    if (!ar_fab()) return;
    int tx0 = (int)g_ox / TS - 2, ty0 = (int)g_oy / TS - 6, tx1 = tx0 + SCR_W / TS + 5, ty1 = ty0 + SCR_H / TS + 10;
    float noche = 1 - ar_dia() * 0.6f;
    for (int ty = ty0 < 1 ? 1 : ty0; ty <= ty1 && ty < MH; ty++)
        for (int tx = tx0 < 1 ? 1 : tx0; tx <= tx1 && tx < MW - 1; tx++) {
            float x = tx * TS - g_ox, y = (ty + 1) * TS - g_oy;
            if (g_g[ty][tx] == G_DOOR && (g_o[ty][tx - 1] == O_WALL || g_o[ty][tx + 1] == O_WALL)) {   // [fab2] baliza giratoria del porton
                float a = g_time * 5 + tx;
                r_rect(x + 6, y - 34, 4, 4, RGBA(255, 60, 40, 255));
                r_glow(x + 8 + cosf(a) * 14, y - 32 + sinf(a) * 4, 16, RGBA(255, 30, 20, (int)(110 * noche * (0.5f + 0.5f * cosf(a)))), RGBA(255, 30, 20, 0));
                r_glow(x + 8, y - 32, 7, RGBA(255, 90, 60, (int)(200 * noche)), RGBA(255, 40, 30, 0));
                continue;
            }
            if (tx % 6 == 3 && ar_pasa(tx, ty)) {   // [fab2] lampara de sodio colgada bajo la pasarela
                u32 h = hash2(tx + (int)(g_time * 5), ty * 3);
                int a = (h % 11 == 0) ? 20 : 130 + (int)(h % 40);
                r_rect(x + 6, y - 28, 4, 2, RGBA(255, 190, 90, 255));
                r_glow(x + 8, y - 22, 34, RGBA(255, 150, 50, (int)(a * 0.55f * noche)), RGBA(255, 110, 30, 0));
                continue;
            }
            if (g_o[ty][tx] != O_WALL) continue;
            if (ar_chim(tx, ty)) {
                if (fmodf(g_time + (tx & 7) * 0.3f, 1.6f) < 0.7f) r_glow(x + 8, y - 102, 12, RGBA(255, 40, 30, 200), RGBA(255, 40, 30, 0));
            } else if (ar_sodio(tx, ty)) {
                u32 h = hash2(tx + (int)(g_time * 6), ty);
                int a = (h % 9 == 0) ? 30 : 120 + (int)(h % 40);   // de vez en cuando se apaga un instante
                r_glow(x + 8, y - 10, 30, RGBA(255, 160, 60, (int)(a * 2 / 3 * noche)), RGBA(255, 120, 40, 0));
                r_glow(x + 8, y + 8, 22, RGBA(255, 150, 60, (int)(a / 3 * noche)), RGBA(255, 120, 40, 0));   // la mancha en el suelo
            } else if (ar_kind(tx, ty) == 3 && !ar_porton(tx, ty)) {   // [fab2] chispas del tablero cada tanto
                u32 h = hash2(tx * 3 + 1, ty * 5 + 2);
                float ph = fmodf(g_time * 0.45f + (h % 97) / 97.0f, 1.0f);
                if (ph < 0.08f) {
                    int f = (int)(g_time * 30);
                    for (int k = 0; k < 7; k++) {
                        u32 q = hash2(f + k * 7, tx + k);
                        r_rect(x + 8 + (int)(q % 13) - 6, y - 16 + (int)((q >> 4) % 9) - 2 + ph * 60, 1, 1, RGBA(255, 240, 160, 255));
                    }
                    r_glow(x + 8, y - 14, 14, RGBA(200, 220, 255, 170), RGBA(120, 160, 255, 0));
                }
            }
        }
}

// [arte] Borde de transicion: donde el camino se junta con el terreno, un filo claro arriba y a la izquierda y una sombra abajo y a la derecha
static void ar_borde(int tx, int ty, float x, float y, int g)
{
    if (g_zone != 1 || snowy() || (g_x.flags & XF_ZJEFE)) return;
    if (ar_fab()) ar_piso(tx, ty, x, y, g);   // [fab2]
    if (!(g == G_ROAD)) return;
    int up = ty > 0 && g_g[ty - 1][tx] != G_ROAD && g_g[ty - 1][tx] != G_EXIT && g_g[ty - 1][tx] != G_ZONE;
    int dn = ty + 1 < MH && g_g[ty + 1][tx] != G_ROAD && g_g[ty + 1][tx] != G_EXIT && g_g[ty + 1][tx] != G_ZONE;
    int lf = tx > 0 && g_g[ty][tx - 1] != G_ROAD && g_g[ty][tx - 1] != G_EXIT && g_g[ty][tx - 1] != G_ZONE;
    int rt = tx + 1 < MW && g_g[ty][tx + 1] != G_ROAD && g_g[ty][tx + 1] != G_EXIT && g_g[ty][tx + 1] != G_ZONE;
    if (g_x.type == XT_CITY || g_x.type == XT_FACTORY) {   // [fab2] solera: filo claro de 2 px y sombra al lado
        u32 lo = RGBA(160, 156, 144, 150), sd = RGBA(0, 0, 0, 110);
        if (up) { r_rect(x, y, TS, 2, lo); r_rect(x, y + 2, TS, 1, sd); }
        if (dn) { r_rect(x, y + TS - 3, TS, 1, lo); r_rect(x, y + TS - 2, TS, 2, sd); }
        if (lf) { r_rect(x, y, 2, TS, lo); r_rect(x + 2, y, 1, TS, sd); }
        if (rt) { r_rect(x + TS - 3, y, 1, TS, lo); r_rect(x + TS - 2, y, 2, TS, sd); }
        return;
    }
    // [fab2] borde de tierra irregular: dientes de terreno que se meten en el camino, con un filo de luz
    u32 tc = RGBA(24, 18, 12, 150), lt = RGBA(128, 110, 84, 70);
    for (int k = 0; k < 4; k++) {
        int d = 1 + (int)(hash2(tx * 4 + k, ty * 7 + 3) % 3), e = 1 + (int)(hash2(tx * 9 + 1, ty * 4 + k) % 3);
        if (up) { r_rect(x + k * 4, y, 4, d, tc); r_rect(x + k * 4, y + d, 4, 1, lt); }
        if (dn) r_rect(x + k * 4, y + TS - d, 4, d, tc);
        if (lf) { r_rect(x, y + k * 4, e, 4, tc); r_rect(x + e, y + k * 4, 1, 4, lt); }
        if (rt) r_rect(x + TS - e, y + k * 4, e, 4, tc);
    }
}
