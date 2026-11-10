// Mi búnker: la base del jugador bajo la Estación Kolos (zona ZB). Se incluye desde game.c.
// Sala central y dormitorio de entrada; seis salas selladas con escombros que se abren pagando; cajas con su propio
// contenido (se abren con la pantalla del alijo); decoraciones que se ponen y se recogen en el modo decorar (SELECT).
// Todo el estado vive en g_bkd (casillas con decoración o caja), g_bkbox (contenido de cada caja) y g_bk_rooms (bits de salas abiertas).

static unsigned char g_bkd[BK_H][BK_W];   // 0 vacío, 1..10 decoración (BKD_), 20 + n la caja n
static short g_bkbox[BK_NBOX][ITEM_MAX];  // lo que guarda cada caja
static int g_bk_rooms;                    // bit i = la sala i está abierta
static int g_bk_hx = -1, g_bk_hy = -1;    // la escotilla de la estación (si ya se armó la base)
static int g_bk_confirm = -1;             // sala cuyo costo se mostró (segunda Cruz = abrir)
static int g_bk_cx, g_bk_cy, g_bk_sel;    // modo decorar: cursor y objeto elegido
static int g_bk_real[ITEM_MAX];           // el alijo de la estación mientras hay una caja abierta

enum { BKD_NONE, BKD_LAMP, BKD_RUG, BKD_SHELF, BKD_POSTER, BKD_PLANT, BKD_RADIO, BKD_TABLE, BKD_CHAIR, BKD_T1, BKD_T2,
       BKD_T3, BKD_T4, BKD_T5,   // [jefes2] trofeos de La Viuda blanca, El Capataz y El Fermentado
       BKD_N };
#define BKD_BOX 20
#define BK_STAIR_X 28
#define BK_STAIR_Y 16

typedef struct { const char *name; short x0, y0, x1, y1, dx, dy; } BkRoom;
static const BkRoom BKR[BK_NR] = {
    {"Sala central", 27, 15, 36, 24, 0, 0},
    {"Dormitorio", 27, 26, 36, 33, 31, 25},
    {"Almacén", 16, 15, 25, 24, 26, 19},
    {"Taller", 38, 15, 47, 24, 37, 19},
    {"Armería", 27, 6, 36, 13, 31, 14},
    {"Perrera", 16, 26, 25, 33, 26, 30},
    {"Sala de trofeos", 16, 6, 25, 13, 26, 9},
    {"Sala del generador", 38, 26, 47, 33, 37, 30},
};
#define BK_GEN 7   // la sala del generador

static int bk_room_open(int i) { return i < 2 || (g_bk_rooms >> i) & 1; }
static int bk_door_vert(int i) { return BKR[i].dx == 26 || BKR[i].dx == 37; }
static int bk_nopen(void) { int n = 0; for (int i = 2; i < BK_NR; i++) n += (g_bk_rooms >> i) & 1; return n; }
static int bk_box_cap(void) { int c = 2 + 2 * bk_nopen(); return c > BK_NBOX ? BK_NBOX : c; }
static int bk_box_count(void) { int n = 0; for (int y = 0; y < BK_H; y++) for (int x = 0; x < BK_W; x++) n += g_bkd[y][x] >= BKD_BOX; return n; }
static void bk_cost(int *scrap, int *mech, int *rag) { int n = bk_nopen(); *scrap = 40 + 30 * n; *mech = 1 + n; *rag = 2 + n; }
static int bk_blocks(int v) { return v == BKD_SHELF || v == BKD_TABLE || v >= BKD_BOX; }

static void bk_reset(void)
{
    memset(g_bkd, 0, sizeof(g_bkd)); memset(g_bkbox, 0, sizeof(g_bkbox));
    g_bk_rooms = 0; g_bk_open = -1; g_bk_confirm = -1;
    g_bkd[22][28] = BKD_BOX; g_bkd[22][30] = BKD_BOX + 1;   // dos cajas y una lámpara para empezar
    g_bkd[16][33] = BKD_LAMP;
}

static void bk_save(void *sv)
{
    SaveData *s = sv;
    memcpy(s->bk_grid, g_bkd, sizeof(g_bkd));
    memcpy(s->bk_box, g_bkbox, sizeof(g_bkbox));
    s->bk_rooms = g_bk_rooms;
    if (g_bk_open >= 0) {   // con una caja abierta: el alijo va con lo suyo y la caja con lo que tiene en pantalla
        for (int i = 0; i < ITEM_COUNT; i++) { s->bk_box[g_bk_open][i] = (short)g_stash[i]; s->stash[i] = g_bk_real[i]; }
    }
}

static void bk_load(const void *sv)
{
    const SaveData *s = sv;
    g_bk_open = -1; g_bk_confirm = -1;
    memcpy(g_bkd, s->bk_grid, sizeof(g_bkd));
    memcpy(g_bkbox, s->bk_box, sizeof(g_bkbox));
    g_bk_rooms = s->bk_rooms & 0xFC;
    int n = 0;   // si la partida es de antes del búnker (todo en cero), se empieza como nuevo
    for (int y = 0; y < BK_H; y++) for (int x = 0; x < BK_W; x++) n += g_bkd[y][x] != 0;
    int any = 0;
    for (int b = 0; b < BK_NBOX; b++) for (int i = 0; i < ITEM_MAX; i++) any |= g_bkbox[b][i] != 0;
    if (!n && !any && !g_bk_rooms) bk_reset();
    for (int b = 0; b < BK_NBOX; b++) for (int i = 0; i < ITEM_MAX; i++) if (g_bkbox[b][i] < 0 || i >= ITEM_COUNT) g_bkbox[b][i] = 0;
    for (int y = 0; y < BK_H; y++) for (int x = 0; x < BK_W; x++)
        if (g_bkd[y][x] >= BKD_BOX + BK_NBOX || (g_bkd[y][x] >= BKD_N && g_bkd[y][x] < BKD_BOX)) g_bkd[y][x] = 0;
}

// Cierra la caja abierta: lo que quedó en pantalla vuelve a la caja y el alijo de la estación a su lugar
static void bk_close(void)
{
    if (g_bk_open < 0) return;
    for (int i = 0; i < ITEM_COUNT; i++) { g_bkbox[g_bk_open][i] = (short)g_stash[i]; g_stash[i] = g_bk_real[i]; }
    g_bk_open = -1;
}

static void bk_box_open(int idx)
{
    for (int i = 0; i < ITEM_COUNT; i++) { g_bk_real[i] = g_stash[i]; g_stash[i] = g_bkbox[idx][i]; }
    g_bk_open = idx;
    kolos_open(KW_STASH);
}

// El plano: todo muro, salas talladas; la puerta de una sala sellada lleva escombros
static void bk_gen(void)
{
    for (int y = 0; y < BK_H; y++) {
        for (int x = 0; x < BK_W; x++) { g_zg[y][x] = '_'; g_zo[y][x] = '#'; }
        g_zg[y][BK_W] = 0; g_zo[y][BK_W] = 0;
    }
    for (int i = 0; i < BK_NR; i++) {
        const BkRoom *r = &BKR[i];
        for (int y = r->y0; y <= r->y1; y++) for (int x = r->x0; x <= r->x1; x++) g_zo[y][x] = '.';
        if (i == 0) continue;
        for (int k = 0; k < 2; k++) {
            int x = r->dx + (bk_door_vert(i) ? 0 : k), y = r->dy + (bk_door_vert(i) ? k : 0);
            g_zo[y][x] = bk_room_open(i) ? '.' : 'R';
        }
    }
    g_zo[28][29] = 'q'; g_zo[28][34] = 'q';   // los catres del dormitorio
}

static void bk_lamps(void)   // las luces fijas: lámparas puestas y, con el generador, una por sala abierta
{
    g_nlamp = 0;
    for (int y = 0; y < BK_H; y++) for (int x = 0; x < BK_W; x++)
        if (g_bkd[y][x] == BKD_LAMP && g_nlamp < MAXLAMP) g_lamp[g_nlamp++] = (Lamp){x * TS + 8, y * TS + 8, 1, 0};
    if (bk_room_open(BK_GEN))
        for (int i = 0; i < BK_NR; i++) {
            if (!bk_room_open(i) || g_nlamp >= MAXLAMP) continue;
            g_lamp[g_nlamp++] = (Lamp){(BKR[i].x0 + BKR[i].x1 + 1) / 2 * TS, (BKR[i].y0 + BKR[i].y1 + 1) / 2 * TS, 1, 0};
        }
}

static void bk_apply_obj(int x, int y)
{
    int v = g_bkd[y][x];
    if (v >= BKD_BOX) g_o[y][x] = O_BK_BOX;
    else if (bk_blocks(v)) g_o[y][x] = O_BK_BLK;
}

// Después de armar el plano: lo puesto por el jugador, la escalera y las luces
static void bk_post(void)
{
    for (int y = 0; y < BK_H; y++) for (int x = 0; x < BK_W; x++) if (g_bkd[y][x]) bk_apply_obj(x, y);
    g_o[BK_STAIR_Y][BK_STAIR_X] = O_BK_STAIR;
    bk_lamps();
}

// En la estación: la escotilla del búnker, en un claro de 3 x 3 sobre piso
static void bk_kolos_hatch(void)
{
    for (int pass = 0; pass < 2; pass++)
        for (int y = 2; y < MH - 2; y++)
            for (int x = 2; x < MW - 2; x++) {
                int ok = 1;
                for (int dy = -1; dy <= 1 && ok; dy++) for (int dx = -1; dx <= 1; dx++) {
                    int g = g_g[y + dy][x + dx];
                    if (g_o[y + dy][x + dx] != O_NONE || g == G_EXIT || g == G_ZONE || g == G_RAD || (pass == 0 ? g != G_FLOOR : g == G_DOOR)) { ok = 0; break; }
                }
                if (!ok) continue;
                g_o[y][x] = O_BK_HATCH; g_bk_hx = x; g_bk_hy = y;
                return;
            }
}

static float bk_amb(void) { return g_zone == ZB && !bk_room_open(BK_GEN) ? 0.4f : 1; }   // sin generador, penumbra

static void bk_arrive(float *x, float *y) { *x = BK_STAIR_X * TS + 8; *y = (BK_STAIR_Y + 1) * TS + 8; }

static void bk_leave(void)
{
    float x = 5 * TS + 8, y = 19 * TS + 8;
    if (g_bk_hx >= 0) { x = g_bk_hx * TS + 8; y = (g_bk_hy + 1) * TS + 8; }
    zone_go(0, x, y, 1);
}

// Lo que se ve y se hace desde Cruz. Devuelve 1 si hizo algo
static int bk_spot(int *tx, int *ty)
{
    int px = (int)(P.x / TS), py = (int)(P.y / TS), best = 0;
    float bd = 26;
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++) {
            if (!in_map(x, y)) continue;
            int o = g_o[y][x], k = o == O_BK_HATCH ? 1 : o == O_BK_STAIR ? 2 : o == O_BK_BOX ? 3 : (o == O_RUBBLE && g_zone == ZB) ? 4 : 0;
            if (!k) continue;
            float d = dist(P.x, P.y, x * TS + 8, y * TS + 8);
            if (d < bd) { bd = d; best = k; *tx = x; *ty = y; }
        }
    return best;
}

static int bk_door_room(int x, int y)
{
    for (int i = 2; i < BK_NR; i++)
        for (int k = 0; k < 2; k++)
            if (x == BKR[i].dx + (bk_door_vert(i) ? 0 : k) && y == BKR[i].dy + (bk_door_vert(i) ? k : 0)) return i;
    return -1;
}

static int bk_cross(void)
{
    int tx, ty, sp = bk_spot(&tx, &ty);
    if (!sp) { g_bk_confirm = -1; return 0; }
    if (sp == 1) { zone_go(ZB, 0, 0, 1); return 1; }
    if (sp == 2) { bk_leave(); return 1; }
    if (sp == 3) { int bi = g_bkd[ty][tx] - BKD_BOX; if (bi >= 0 && bi < BK_NBOX) bk_box_open(bi); return 1; }   // [arregla]
    int r = bk_door_room(tx, ty), s, m, g;
    if (r < 0) return 0;
    bk_cost(&s, &m, &g);
    char t[96];
    if (g_bk_confirm != r || g_msg_t <= 0) {
        g_bk_confirm = r;
        snprintf(t, sizeof(t), "%s: %d chatarra, %d piezas, %d trapos. Cruz otra vez: abrir.", BKR[r].name, s, m, g);
        msg(t);
        return 1;
    }
    if (g_bag[ITEM_SCRAP] < s || g_bag[ITEM_MECH] < m || g_bag[ITEM_RAG] < g) { msg("Te falta material para despejar los escombros."); return 1; }
    g_bag[ITEM_SCRAP] -= s; g_bag[ITEM_MECH] -= m; g_bag[ITEM_RAG] -= g;
    g_bk_rooms |= 1 << r; g_bk_confirm = -1;
    for (int k = 0; k < 2; k++) g_o[BKR[r].dy + (bk_door_vert(r) ? k : 0)][BKR[r].dx + (bk_door_vert(r) ? 0 : k)] = O_NONE;
    bk_lamps(); bake_lights();
    snd_play(SFX_PICKUP, 0.8f, 0);
    snprintf(t, sizeof(t), "Despejaste: %s.%s", BKR[r].name, r == BK_GEN ? " El generador ilumina todo." : "");
    msg(t);
    save_game();
    return 1;
}

static int bk_hint(void)
{
    int tx, ty, sp = bk_spot(&tx, &ty);
    if (!sp) return 0;
    const char *t = sp == 1 ? "Cruz: bajar a tu búnker" : sp == 2 ? "Cruz: subir a la estación" : sp == 3 ? "Cruz: abrir la caja" : "Cruz: ver el costo";
    r_text_center(clampf(tx * TS + 8 - g_ox, 96, SCR_W - 96), ty * TS - 18 - g_oy, t, RGBA(236, 230, 210, 255));
    return 1;
}

// ------------------------------------------------------------------ dibujo
static void bk_draw_decor(int v, float x, float y)   // x, y: esquina de la casilla en pantalla
{
    switch (v) {
    case BKD_LAMP:
        r_rect(x + 7, y + 6, 2, 8, RGBA(70, 66, 60, 255)); r_rect(x + 4, y + 12, 8, 2, RGBA(50, 46, 42, 255));
        r_rect(x + 4, y + 2, 8, 5, RGBA(246, 214, 130, 255)); r_rect(x + 5, y + 3, 6, 2, RGBA(255, 240, 190, 255));
        r_glow(x + 8, y + 6, 14, RGBA(255, 214, 130, 40), RGBA(255, 214, 130, 0));
        break;
    case BKD_RUG:
        r_rect(x + 1, y + 3, 14, 10, RGBA(110, 36, 34, 255)); r_rect(x + 2, y + 4, 12, 8, RGBA(150, 56, 48, 255));
        r_rect(x + 4, y + 6, 8, 4, RGBA(214, 170, 90, 255)); r_rect(x + 5, y + 7, 6, 2, RGBA(110, 36, 34, 255));
        break;
    case BKD_SHELF:
        r_rect(x + 1, y + 1, 14, 14, RGBA(34, 24, 16, 255)); r_rect(x + 2, y + 2, 12, 12, RGBA(104, 76, 50, 255));
        r_rect(x + 2, y + 6, 12, 1, RGBA(44, 30, 20, 255)); r_rect(x + 2, y + 10, 12, 1, RGBA(44, 30, 20, 255));
        r_rect(x + 3, y + 3, 3, 3, RGBA(180, 60, 50, 255)); r_rect(x + 8, y + 4, 4, 2, RGBA(90, 140, 170, 255));
        r_rect(x + 4, y + 7, 5, 3, RGBA(200, 190, 150, 255)); r_rect(x + 3, y + 11, 8, 2, RGBA(130, 150, 90, 255));
        break;
    case BKD_POSTER:
        r_rect(x + 3, y + 2, 10, 12, RGBA(30, 26, 22, 255)); r_rect(x + 4, y + 3, 8, 10, RGBA(214, 200, 160, 255));
        r_rect(x + 5, y + 5, 6, 1, RGBA(120, 50, 40, 255)); r_rect(x + 5, y + 7, 6, 4, RGBA(70, 90, 120, 255));
        break;
    case BKD_PLANT:
        r_rect(x + 5, y + 10, 6, 5, RGBA(120, 70, 46, 255)); r_rect(x + 6, y + 6, 4, 5, RGBA(60, 130, 60, 255));
        r_rect(x + 3, y + 4, 4, 4, RGBA(80, 160, 70, 255)); r_rect(x + 9, y + 3, 4, 5, RGBA(70, 150, 66, 255));
        break;
    case BKD_RADIO:
        r_rect(x + 2, y + 6, 12, 8, RGBA(30, 28, 26, 255)); r_rect(x + 3, y + 7, 10, 6, RGBA(120, 100, 70, 255));
        r_rect(x + 4, y + 8, 4, 4, RGBA(60, 54, 44, 255)); r_rect(x + 10, y + 8, 2, 2, RGBA(220, 90, 60, 255));
        r_rect(x + 11, y + 2, 1, 5, RGBA(190, 190, 200, 255));
        break;
    case BKD_TABLE:
        r_rect(x + 1, y + 5, 14, 3, RGBA(142, 104, 66, 255)); r_rect(x + 1, y + 8, 14, 1, RGBA(60, 42, 28, 255));
        r_rect(x + 2, y + 9, 2, 6, RGBA(90, 64, 40, 255)); r_rect(x + 12, y + 9, 2, 6, RGBA(90, 64, 40, 255));
        break;
    case BKD_CHAIR:
        r_rect(x + 4, y + 3, 8, 6, RGBA(120, 86, 54, 255)); r_rect(x + 4, y + 9, 8, 2, RGBA(150, 110, 70, 255));
        r_rect(x + 4, y + 11, 2, 4, RGBA(80, 56, 36, 255)); r_rect(x + 10, y + 11, 2, 4, RGBA(80, 56, 36, 255));
        break;
    case BKD_T1:   // cabeza de la Bestia: cráneo verdoso con cuernos
        r_rect(x + 3, y + 4, 10, 9, RGBA(20, 40, 24, 255)); r_rect(x + 4, y + 5, 8, 7, RGBA(110, 170, 100, 255));
        r_rect(x + 5, y + 7, 2, 2, RGBA(240, 60, 40, 255)); r_rect(x + 9, y + 7, 2, 2, RGBA(240, 60, 40, 255));
        r_rect(x + 2, y + 2, 2, 4, RGBA(220, 210, 180, 255)); r_rect(x + 12, y + 2, 2, 4, RGBA(220, 210, 180, 255));
        r_rect(x + 5, y + 10, 6, 1, RGBA(30, 50, 30, 255));
        break;
    case BKD_T2:   // casco de Cabeza de Hierro
        r_rect(x + 3, y + 3, 10, 10, RGBA(24, 24, 30, 255)); r_rect(x + 4, y + 4, 8, 8, RGBA(150, 156, 170, 255));
        r_rect(x + 4, y + 7, 8, 2, RGBA(20, 20, 24, 255)); r_rect(x + 7, y + 4, 2, 8, RGBA(110, 116, 130, 255));
        r_rect(x + 5, y + 2, 6, 1, RGBA(190, 196, 210, 255));
        break;
    case BKD_T3:   // piel de La Viuda blanca: alfombra de pelo blanco con la cabeza
        r_rect(x + 1, y + 4, 14, 10, RGBA(236, 240, 250, 255)); r_rect(x + 2, y + 5, 12, 8, RGBA(214, 222, 238, 255));
        r_rect(x + 5, y + 1, 6, 5, RGBA(230, 236, 248, 255)); r_rect(x + 6, y + 2, 1, 1, RGBA(60, 120, 220, 255)); r_rect(x + 9, y + 2, 1, 1, RGBA(60, 120, 220, 255));
        r_rect(x + 0, y + 13, 3, 2, RGBA(236, 240, 250, 255)); r_rect(x + 13, y + 13, 3, 2, RGBA(236, 240, 250, 255));
        break;
    case BKD_T4:   // mazo del Capataz: mango de madera y cabeza de hierro, colgado
        r_rect(x + 7, y + 2, 2, 12, RGBA(120, 84, 50, 255)); r_rect(x + 3, y + 1, 10, 5, RGBA(70, 72, 80, 255));
        r_rect(x + 4, y + 2, 8, 3, RGBA(130, 134, 146, 255)); r_rect(x + 6, y + 12, 4, 2, RGBA(90, 60, 36, 255));
        break;
    case BKD_T5:   // frasco del Fermentado: vidrio con algo verde que burbujea
        r_rect(x + 4, y + 4, 8, 11, RGBA(120, 150, 150, 255)); r_rect(x + 5, y + 7, 6, 7, RGBA(120, 220, 70, 255));
        r_rect(x + 6, y + 2, 4, 2, RGBA(90, 80, 60, 255)); r_rect(x + 7, y + 8, 1, 1, RGBA(200, 255, 150, 255)); r_rect(x + 9, y + 11, 1, 1, RGBA(200, 255, 150, 255));
        break;
    }
}

static void bk_draw_obj(int tx, int ty, int o)
{
    float x = tx * TS - g_ox, y = ty * TS - g_oy;
    if (o == O_BK_STAIR) {
        r_rect(x + 1, y + 1, 14, 14, RGBA(10, 8, 8, 255));
        for (int k = 0; k < 4; k++) r_rect(x + 2, y + 3 + k * 3, 12, 2, RGBA(90 + k * 25, 80 + k * 22, 66 + k * 18, 255));
    } else if (o == O_BK_HATCH) {
        r_rect(x + 1, y + 2, 14, 12, RGBA(20, 22, 26, 255)); r_rect(x + 2, y + 3, 12, 10, RGBA(104, 110, 120, 255));
        r_rect(x + 2, y + 3, 12, 2, RGBA(150, 156, 168, 255)); r_rect(x + 6, y + 7, 4, 2, RGBA(236, 150, 60, 255));
    } else if (o == O_BK_BOX) {
        r_rect(x + 1, y + 4, 14, 11, RGBA(20, 14, 10, 255)); r_rect(x + 2, y + 5, 12, 9, RGBA(124, 92, 58, 255));
        r_rect(x + 2, y + 5, 12, 3, RGBA(150, 112, 70, 255)); r_rect(x + 2, y + 8, 12, 1, RGBA(58, 42, 28, 255));
        r_rect(x + 7, y + 8, 2, 3, RGBA(210, 200, 120, 255));
    } else if (o == O_BK_BLK) bk_draw_decor(g_bkd[ty][tx], x, y);
}

static void bk_draw_flat(void)   // lo que no estorba: va debajo de quienes pasan
{
    if (g_zone != ZB) return;
    int x0 = (int)(g_ox / TS) - 1, x1 = x0 + SCR_W / TS + 3, y0 = (int)(g_oy / TS) - 1, y1 = y0 + SCR_H / TS + 3;
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < BK_H; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < BK_W; x++) {
            int v = g_bkd[y][x];
            if (v && !bk_blocks(v)) bk_draw_decor(v, x * TS - g_ox, y * TS - g_oy);
        }
}

// ------------------------------------------------------------------ modo decorar
static int bk_item_ok(int it) { return it == ITEM_BK_BOX || (it >= ITEM_BKD_LAMP && it <= ITEM_BKD_T5); }

static void bk_sel_fix(int dir)   // el objeto elegido: uno de la mochila; dir mueve al siguiente (o al anterior)
{
    int n = ITEM_BKD_T5 - ITEM_BK_BOX + 1, d = dir ? dir : 1;
    for (int k = dir ? 1 : 0; k <= n; k++) {
        int it = ITEM_BK_BOX + ((g_bk_sel - ITEM_BK_BOX + d * k) % n + n) % n;
        if (g_bag[it] > 0 && bk_item_ok(it)) { g_bk_sel = it; return; }   // [arregla] saltar las reliquias
    }
}

static int bk_deco_open(void)
{
    if (g_zone != ZB) return 0;
    g_bk_cx = (int)((P.x + cosf(P.ang) * 20) / TS); g_bk_cy = (int)((P.y + sinf(P.ang) * 20) / TS);
    g_bk_cx = g_bk_cx < 1 ? 1 : g_bk_cx > BK_W - 2 ? BK_W - 2 : g_bk_cx; g_bk_cy = g_bk_cy < 1 ? 1 : g_bk_cy > BK_H - 2 ? BK_H - 2 : g_bk_cy;
    if (!bk_item_ok(g_bk_sel)) g_bk_sel = ITEM_BK_BOX;
    bk_sel_fix(0);
    g_state = GS_BK; g_msg_t = 0;
    return 1;
}

static int bk_free_tile(int x, int y)
{
    if (x < 1 || y < 1 || x >= BK_W - 1 || y >= BK_H - 1) return 0;
    if (g_zo[y][x] != '.' || g_o[y][x] != O_NONE || g_bkd[y][x]) return 0;
    for (int i = 0; i < BK_NR; i++) {   // dentro de una sala abierta y fuera de las puertas
        if (x < BKR[i].x0 || x > BKR[i].x1 || y < BKR[i].y0 || y > BKR[i].y1) continue;
        if (!bk_room_open(i)) return 0;
        return 1;
    }
    return 0;
}

static void bk_deco_input(void)
{
    int dx = ((in.pressed & PSP_CTRL_RIGHT) != 0) - ((in.pressed & PSP_CTRL_LEFT) != 0);
    int dy = ((in.pressed & PSP_CTRL_DOWN) != 0) - ((in.pressed & PSP_CTRL_UP) != 0);
    int px = (int)(P.x / TS), py = (int)(P.y / TS);
    g_bk_cx += dx; g_bk_cy += dy;
    g_bk_cx = g_bk_cx < px - 8 ? px - 8 : g_bk_cx > px + 8 ? px + 8 : g_bk_cx;
    g_bk_cy = g_bk_cy < py - 5 ? py - 5 : g_bk_cy > py + 5 ? py + 5 : g_bk_cy;
    g_bk_cx = g_bk_cx < 1 ? 1 : g_bk_cx > BK_W - 2 ? BK_W - 2 : g_bk_cx; g_bk_cy = g_bk_cy < 1 ? 1 : g_bk_cy > BK_H - 2 ? BK_H - 2 : g_bk_cy;
    if (in.pressed & PSP_CTRL_LTRIGGER) bk_sel_fix(-1);
    if (in.pressed & PSP_CTRL_RTRIGGER) bk_sel_fix(1);
    if (in.pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_SELECT)) { g_state = GS_PLAY; save_game(); return; }
    if (!(in.pressed & PSP_CTRL_CROSS)) return;
    int x = g_bk_cx, y = g_bk_cy, v = g_bkd[y][x];
    if (v) {   // recoger
        int it = v >= BKD_BOX ? ITEM_BK_BOX : ITEM_BKD_LAMP + v - 1;
        if (v >= BKD_BOX) {
            for (int i = 0; i < ITEM_COUNT; i++) if (g_bkbox[v - BKD_BOX][i]) { msg("Vacía la caja antes de recogerla."); return; }
        }
        if (inv_add(it, 1) <= 0) { msg("La mochila está llena."); return; }
        g_bkd[y][x] = 0; g_o[y][x] = O_NONE;
        snd_play(SFX_PICKUP, 0.6f, 0);
    } else {   // poner
        int it = g_bk_sel;
        if (!bk_item_ok(it) || g_bag[it] <= 0) { msg("No tienes nada que poner."); return; }
        if (!bk_free_tile(x, y) || (x == px && y == py)) { msg("Ahí no se puede poner."); return; }
        int nv;
        if (it == ITEM_BK_BOX) {
            if (bk_box_count() >= bk_box_cap()) { msg("No caben más cajas: abre más salas."); return; }
            int used[BK_NBOX] = {0};
            for (int yy = 0; yy < BK_H; yy++) for (int xx = 0; xx < BK_W; xx++) if (g_bkd[yy][xx] >= BKD_BOX) used[g_bkd[yy][xx] - BKD_BOX] = 1;
            int n = 0;
            while (n < BK_NBOX && used[n]) n++;
            if (n >= BK_NBOX) return;
            nv = BKD_BOX + n;
        } else nv = it - ITEM_BKD_LAMP + 1;
        inv_take(it, 1);
        g_bkd[y][x] = (unsigned char)nv; bk_apply_obj(x, y);
        snd_play(SFX_PICKUP, 0.6f, 0);
    }
    bk_sel_fix(0);
    bk_lamps(); bake_lights();
}

static void bk_deco_draw(void)
{
    float x = g_bk_cx * TS - g_ox, y = g_bk_cy * TS - g_oy;
    u32 c = bk_free_tile(g_bk_cx, g_bk_cy) || g_bkd[g_bk_cy][g_bk_cx] ? RGBA(120, 240, 130, 255) : RGBA(240, 90, 80, 255);
    r_rect(x - 1, y - 1, 18, 1, c); r_rect(x - 1, y + 16, 18, 1, c); r_rect(x - 1, y - 1, 1, 18, c); r_rect(x + 16, y - 1, 1, 18, c);
    r_rect(0, 224, SCR_W, 48, RGBA(0, 0, 0, 170));
    char s[96];
    snprintf(s, sizeof(s), "Poner: %s (%d)   Cajas %d/%d", bk_item_ok(g_bk_sel) ? ITEMS[g_bk_sel].name : "nada", bk_item_ok(g_bk_sel) ? g_bag[g_bk_sel] : 0, bk_box_count(), bk_box_cap());
    r_text_center(SCR_W / 2, 228, s, UI_LIGHT);
    r_text_center(SCR_W / 2, 242, "Flechas: cursor   L/R: objeto   Cruz: poner o recoger   Círculo: salir", RGBA(170, 166, 158, 255));
    ui_msg(212);
}

// El dueño de los trofeos: los jefes sueltan el suyo al morir
static void bk_boss_drop(int type, float x, float y)
{
    if (type == E_BOSS) drop(x + 8, y - 4, ITEM_BKD_T1, 1);
    if (type == E_BOSS2) drop(x + 8, y - 4, ITEM_BKD_T2, 1);
}
