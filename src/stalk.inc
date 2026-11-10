// [stalk] Arte unido: lo mejor de StalkerLike que no se usaba (arboles, casas de pueblo, rejas, cofre,
// objetos de patio y bandidos), apagado de tono en tools/make_atlas.py para que case con el resto.
// Solo dibujo: no cambia que es solido, que se abre ni como se mueve nada.

// Tipo de mapa de la expedicion cargada (-1 fuera de una expedicion)
static int st_tipo(void) { return g_zone == 1 ? g_x.type : -1; }

// Mapas con arte de pueblo (rejas de madera, casas de tablas): todo lo de afuera menos la ciudad y la fabrica
static int st_pueblo(void)
{
    int t = st_tipo();
    return t >= 0 && t != XT_CITY && t != XT_FACTORY && t != XT_TRAIN;
}

// Arbol nuevo segun el tipo de mapa: el bosque de pinos casi solo pinos, el bosque todos, y en los demas
// se mezclan con los arboles secos de antes (para que no se pierda lo que ya habia). -1 = el de antes.
static int st_arbol(int tx, int ty)
{
    int t = st_tipo();
    if (t < 0 || t == XT_CITY || t == XT_FACTORY || snowy()) return -1;
    u32 h = hash2(tx * 3 + 1, ty * 5 + 2);
    if (t == XT_PINE) return h % 4 == 0 ? S_ST_TREE3 : S_ST_TREE0;
    if (t == XT_FOREST) return S_ST_TREE0 + h % 6;
    if (t == XT_GRAVE) return h % 3 ? -1 : S_ST_TREE2 + (h >> 3) % 2 * 3;   // el cementerio sigue casi seco
    return h % 2 ? -1 : S_ST_TREE3 + (h >> 3) % 3;   // pueblos y vias: chicos
}

// Lo que se dibuja entero aqui (devuelve 1): los arboles nuevos, que son mas altos y anchos que una casilla
static int st_obj(int tx, int ty, int o)
{
    if (o != O_TREE || is_bush(tx, ty)) return 0;
    int s = st_arbol(tx, ty);
    if (s < 0) return 0;
    Spr sp = SPR[s];
    float bx = tx * TS + 8, by = (ty + 1) * TS;
    u32 col = COL_WHITE;
    if (fabsf(P.x - bx) < sp.w / 2 && P.y < by - 4 && P.y > by - sp.h - 6) col = RGBA(255, 255, 255, 110);   // detras del arbol se aclara
    float sx = bx - g_ox, sy = by - g_oy;   // a pantalla
    r_spr(SPR[S_SHADOW], sx - 6, sy - 3, RGBA(255, 255, 255, 200));
    r_spr(sp, floorf(sx - sp.w / 2), sy + 1 - sp.h, col);
    return 1;
}

// Cambia el dibujo de una casilla ya elegida por draw_object: muros de casa, rejas y cofres
static int st_spr(int tx, int ty, int o, int s)
{
    int t = st_tipo();
    if (t < 0 || t == XT_CITY) return s;
    if (o == O_WALL && s == S_WALL && (t == XT_MINE || t == XT_TOWN)) {
        int mina = t == XT_MINE;
        if (ty + 1 < MH && wall_like(g_o[ty + 1][tx])) return mina ? S_ST_HMINA_R : S_ST_HBARR_R;   // techo corrido
        int ven = hash2(tx * 7 + 5, ty * 3 + 9) % 3 == 0;
        return mina ? (ven ? S_ST_HMINA1 : S_ST_HMINA0) : (ven ? S_ST_HBARR1 : S_ST_HBARR0);
    }
    if (o == O_FENCE) {
        int conc = t == XT_FACTORY || t == XT_TRAIN;
        if (s == S_FENCE_H) return conc ? S_ST_CFENCE_H : S_ST_WFENCE_H;
        if (s == S_FENCE_V) return conc ? S_ST_CFENCE_V : S_ST_WFENCE_V;
    }
    if (o == O_CRATE && (s == S_CRATE || s == S_CRATE_OPEN)) return s == S_CRATE ? S_ST_CHEST : S_ST_CHEST_OPEN;
    return s;
}

// Objetos de patio sobre el suelo libre de los pueblos: baldes sueltos y pallets de cajones junto a las casas
static void st_suelo(int tx, int ty, float x, float y)
{
    if (!st_pueblo() || snowy() || g_o[ty][tx] != O_NONE) return;
    int g = g_g[ty][tx];
    if (g != G_DIRT && g != G_GRASS) return;
    u32 h = hash2(tx * 11 + 7, ty * 17 + 3) % 1000;
    if (ty > 0 && g_o[ty - 1][tx] == O_WALL && h < 90) { Spr p = SPR[S_ST_PALET]; r_spr(p, x + (TS - p.w) / 2, y + TS - p.h, COL_WHITE); }
    else if (h < 8) { Spr b = SPR[S_ST_BALDE]; r_spr(b, x + 3, y + TS - b.h, COL_WHITE); }
}

// Bandidos y tiradores con su propio cuerpo (las hojas Bandit1-3) en vez del jugador teñido.
// Las hojas traen solo el perfil que mira a la derecha: se espeja hacia la izquierda.
static int st_bandido(const Enemy *e, float x, float y)
{
    if (e->type != E_BAN && e->type != E_SNP) return 0;
    int hoja = e->type == E_SNP ? 0 : e->var ? 2 : 1;   // tirador: el encapuchado negro; los otros dos segun el arma
    int I = S_ST_B1_I0 + hoja * 11, W = I + 4, D = I + 10;
    int flip = cosf(e->ang) < 0;
    if (e->state == ES_DEAD) {
        if (dying(e)) {   // cae de lado girando, como antes
            float k = (g_time - e->dead_t) * DIE_FPS / 5;
            Spr b = SPR[I];
            r_rot(b, floorf(x), floorf(y + 3 - b.h / 2 * (1 - k) - 4 * k), (flip ? -1 : 1) * 1.5708f * k, 1, COL_WHITE);
        } else {
            Spr d = SPR[D];
            r_spr_f(d, floorf(x) - d.w / 2, floorf(y) + 4 - d.h, flip, RGBA(150, 138, 138, 255));
        }
        return 1;
    }
    u32 hc = e->flash > 0 ? RGBA(255, 130, 120, 255) : COL_WHITE;
    float c = cosf(e->ang), s = sinf(e->ang);
    Spr g = SPR[WDEF[e->type == E_SNP ? W_BOLT : e->var ? W_AK : W_MAKAROV].spr];
    float reach = 1 + g.w * 0.5f, gx = x + c * reach, gy = y - BODY_H - 2 + s * reach * 0.8f;
    int back = body_view(e->ang) == 1;
    Spr body = SPR[e->moving ? W + (int)(e->walk * 10) % 6 : I + (int)(g_time * 4) % 4];
    r_spr(SPR[S_SHADOW], x - 6, y, COL_WHITE);
    if (back) r_rot_f(g, gx, gy, e->ang + g_gun_off, flip, hc);
    r_spr_f(body, floorf(x) - body.w / 2, floorf(y) + 3 - body.h, flip, hc);
    if (!back) r_rot_f(g, gx, gy, e->ang + g_gun_off, flip, hc);
    return 1;
}

#ifdef PEPEL_DEBUG
static void dbg_logf(const char *fmt, ...);
// Pruebas: stobj=O se para al sur del objeto de tipo O mas cercano al centro del mapa (sin el borde),
// con la casilla de abajo libre; asi las capturas muestran casas, arboles, rejas y cofres de cerca.
static int st_dbg(const char *name, int a)
{
    if (strcmp(name, "stobj")) return 0;
    int best = -1, bx = 0, by = 0;
    for (int y = 6; y < MH - 6; y++)
        for (int x = 8; x < MW - 8; x++) {
            if (g_o[y][x] != a || (a == O_TREE && is_bush(x, y)) || solid(x, y + 2) || solid(x, y + 3)) continue;
            int d = abs(x - MW / 2) + abs(y - MH / 2);
            if (best < 0 || d < best) { best = d; bx = x; by = y; }
        }
    if (best >= 0) { P.x = bx * TS + 8; P.y = (by + 3) * TS + 8; P.lock = -1; P.ang = -1.5708f; }
    dbg_logf("T stobj %d: %d en %d,%d", a, best >= 0, bx, by);
    return 1;
}
#endif
