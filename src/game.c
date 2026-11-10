// PEPEL: el juego. Una zona ("Estación Kolos"), un trabajo, dos enemigos y la Ceniza.
// Todo se mide en pixeles del mundo; una casilla = 16 px; la pantalla muestra 30 x 17 casillas.
#include <pspctrl.h>
#include <pspkernel.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "r2d.h"
#include "atlas.h"
#include "map1.h"
#include "game.h"
#include "snd.h"

// Tamaño de las zonas: la capacidad de las grillas es MWMAX x MHMAX (128 x 80); cada zona tiene su tamaño activo
// en g_mw x g_mh (MW y MH). La base, el búnker y los pisos miden 64 x 40; la expedición, 128 x 80.
#undef MW
#undef MH
#define MWMAX 192   // [mapas]
#define MHMAX 120   // [mapas]
static int g_mw = 64, g_mh = 40;
#define MW g_mw
#define MH g_mh

#define TS 16
#define BODY_H 4   // altura de las manos sobre el punto de apoyo: ahi van el arma y las balas
#define COL_CASING RGBA(214, 176, 80, 255)   // la particula de este color es el casquillo: se dibuja con su sprite
#define PI 3.14159265f
#define WORLD_W (MW * TS)
#define WORLD_H (MH * TS)

// ------------------------------------------------------------------ utilidades
static u32 g_rng = 0x2F6E2B1u;
static u32 rnd(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static float frand(void) { return (rnd() & 0xFFFF) / 65536.0f; }
static float frange(float a, float b) { return a + (b - a) * frand(); }
static int irange(int a, int b) { return a + (int)(rnd() % (u32)(b - a + 1)); }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float angdiff(float a, float b)
{
    float d = b - a;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    return d;
}
static float turn(float a, float target, float step) { return a + clampf(angdiff(a, target), -step, step); }
static float dist(float x0, float y0, float x1, float y1) { return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)); }
static u32 hash2(int x, int y)
{
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ------------------------------------------------------------------ mapa
enum { G_DIRT, G_GRASS, G_ROAD, G_FLOOR, G_DOOR, G_RAIL, G_RAD, G_EXIT, G_ZONE };
enum { O_NONE, O_WALL, O_CONT, O_CONT2, O_FENCE, O_TREE, O_SAND, O_BARREL, O_CAR_L, O_CAR_R, O_CRATE,
       O_FBARREL, O_LAMP, O_CROSS, O_STAKE, O_COT, O_NPC, O_HATCH, O_STAIRS,
       O_MERCH, O_SCAV, O_BENCH, O_CHEST,
       O_RUBBLE, O_CHEST2, O_SHELF, O_BED, O_CLINIC, O_TOWER, O_GARDEN, O_TANK, O_BENCH2,
       O_MACH, O_STANK, O_TOMB, O_MINE, O_CART, O_SLAG, O_STUP, O_STDN };   // O_MACH a O_SLAG: maquinaria, tanque grande, lápida, boca de mina, vagoneta y escoria de las expediciones nuevas; O_STUP y O_STDN: escaleras de los edificios con pisos (subir y bajar). O_NPC: el Chofer; O_HATCH y O_STAIRS: la escotilla y la escalera del búnker; O_MERCH, O_SCAV, O_BENCH y O_CHEST: el comerciante, el chatarrero, su banco y el arcón (alijo) de la base; O_RUBBLE a O_BENCH2: los escombros y lo que suman las mejoras de la base (2.º arcón, estantería, cama, enfermería, torre, huerto, colector y banco grande)
enum { CK_LOOT, CK_JOB, CK_WEAPON, CK_BODY, CK_MASK, CK_MEDS, CK_REG, CK_RICH, CK_SURV, CK_DOGW, CK_FARO };
enum { CK_MIL = 12 };   // [mil] la caja militar (verde): siempre munición, a veces un arma (11 es CK_VEND, en diseno.inc)   // CK_SURV: el superviviente herido; CK_DOGW: el perro herido junto a un cuerpo; CK_MASK: un cuerpo que además lleva una máscara de gas; CK_MEDS: la caja de medicinas del búnker; CK_REG: el cajón del Registro del incendio; CK_RICH: cajón con mejor botín
typedef struct { int tx, ty, kind, open; } Crate;

static unsigned char g_g[MHMAX][MWMAX], g_o[MHMAX][MWMAX];
#define MAXCR 96   // tope de cajones y cuerpos por zona
static Crate g_cr[MAXCR];
static int g_ncr;

static int is_body(const Crate *c) { return c->kind == CK_BODY || c->kind == CK_MASK; }
static int in_map(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < MW && ty < MH; }
static int is_bush(int tx, int ty) { return in_map(tx, ty) && g_o[ty][tx] == O_TREE && hash2(tx, ty) % 3 == 2; }   // [armas] arbusto: se atraviesa más lento y esconde
static int solid(int tx, int ty) { return !in_map(tx, ty) || (g_o[ty][tx] != O_NONE && !is_bush(tx, ty)); }
static int wall_like(int o) { return o == O_WALL || o == O_CONT || o == O_CONT2; }
// La reja, los sacos y lo delgado o bajo (farol, cruz, estaca, barril con fuego, catre) frenan el paso pero no las balas ni la vista
static int blocks_shot(int tx, int ty)
{
    if (!in_map(tx, ty)) return 1;
    if (is_bush(tx, ty)) return 0;   // [armas]
    int o = g_o[ty][tx];
    return o != O_NONE && o != O_FENCE && o != O_SAND && (o < O_FBARREL || o == O_MACH || o == O_STANK || o == O_MINE);
}
static int ground_at(float x, float y)
{
    int tx = (int)floorf(x / TS), ty = (int)floorf(y / TS);
    return in_map(tx, ty) ? g_g[ty][tx] : G_DIRT;
}
static int indoors(float x, float y) { int g = ground_at(x, y); return g == G_FLOOR || g == G_DOOR; }

static int box_hits(float x, float y, float r)
{
    int x0 = (int)floorf((x - r) / TS), x1 = (int)floorf((x + r) / TS);
    int y0 = (int)floorf((y - r) / TS), y1 = (int)floorf((y + r) / TS);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (solid(tx, ty)) return 1;
    return 0;
}

static void move(float *x, float *y, float dx, float dy, float r)
{
    if (!box_hits(*x + dx, *y, r)) *x += dx;
    if (!box_hits(*x, *y + dy, r)) *y += dy;
}

static int los(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(sqrtf(dx * dx + dy * dy) / 5) + 1;
    for (int i = 1; i < n; i++)
        if (blocks_shot((int)floorf((x0 + dx * i / n) / TS), (int)floorf((y0 + dy * i / n) / TS))) return 0;
    return 1;
}

// ------------------------------------------------------------------ armas y objetos
// Tipos de munición: cómo pega una bala. Cada tipo de cada calibre es un objeto aparte de la mochila (ITEM_);
// el arma dice cuáles acepta (WDEF.ammo) y recuerda cuál lleva cargado (WeaponState.load).
// El blindaje del enemigo (0 a 0,8) es la parte del daño que frena: ver bullet_dmg()
enum { B_NORMAL, B_AP, B_HP, B_SHOT, B_SLUG, B_COUNT };
typedef struct {
    const char *tag;    // junto a la munición en el HUD: "8/24 PERF."
    const char *name;   // en los avisos: "Cargas balas perforantes."
    u32 col, trail;     // color de la etiqueta y de la bala en vuelo
    int pellets;        // lo que sigue, si no es 0, reemplaza lo del arma: proyectiles por tiro,
    float dmg, spread, range;   // daño, apertura y alcance
} BulletDef;
static const BulletDef BDEF[B_COUNT] = {
    [B_NORMAL] = {"NORM.", "balas normales", RGBA(170, 166, 158, 255), COL_WHITE},
    [B_AP] = {"PERF.", "balas perforantes", RGBA(150, 190, 240, 255), RGBA(190, 220, 255, 255)},   // ignora el blindaje; 10 % menos de daño
    [B_HP] = {"EXP.", "balas expansivas", RGBA(240, 150, 90, 255), RGBA(255, 190, 150, 255)},      // +35 % sin blindaje; el blindaje cuenta doble
    [B_SHOT] = {"PERD.", "perdigones", RGBA(170, 166, 158, 255), COL_WHITE},
    [B_SLUG] = {"BALA", "bala única", RGBA(150, 220, 130, 255), COL_WHITE, 1, 40, 0.035f, 230},     // un solo proyectil, más lejos y más justo
};
typedef struct { short item, kind; } AmmoSlot;   // un tipo de munición que acepta un arma: el objeto que gasta y cómo pega (B_)
#define AMMO_TYPES 3

// Armas: una fila por arma en WDEF. El cambio de arma (izquierda/derecha) recorre las que tienes
enum { W_MAKAROV, W_SAWED, W_AK,
       W_KNIFE, W_AXE, W_PIPE, W_MAUL,                  // cuerpo a cuerpo (contiguas: ver is_melee)
       W_REVOLVER, W_SMG, W_PUMP, W_BOLT,
       W_VIBORA, W_CHACAL, W_ZUMBA, W_TORM, W_CUERVO, W_HALCON, W_CENTINELA, W_APLANA,   // [armas2]
       W_COUNT };
static int is_melee(int wi) { return wi >= W_KNIFE && wi <= W_MAUL; }
#define W_MAX 24         // lugar en la partida guardada: alcanza para las que se sumen
typedef struct {
    const char *name;
    int mag;            // balas por cargador
    float rate;         // segundos entre tiros (con R mantenido el arma sigue disparando a ese ritmo)
    float dmg, spread;  // dano por proyectil, apertura en radianes
    float reload, speed, range;
    int pellets;        // proyectiles por tiro
    AmmoSlot ammo[AMMO_TYPES];   // los tipos de munición que acepta (el primero es el de siempre) y cuántos son
    int nammo;
    float kick;
    float wear, jam;    // desgaste por tiro y cuánto se traba (1 = lo normal para su estado)
    float noise;        // hasta dónde se oye el disparo, en px
    short spr, icon;    // sprite en la mano e icono
    short casing;       // lo que salta al disparar
    float flash;        // tamaño del fogonazo
    short pierce;       // a cuántos enemigos más atraviesa la bala
} WeaponDef;
// Cada arma tiene cuatro ranuras para piezas (tabla PARTS, más abajo). part[] guarda la pieza puesta en cada ranura
// (su lugar en PARTS + 1; 0 = vacía). Los valores del arma con sus piezas los da weapon_stats()
enum { SLOT_MUZZLE, SLOT_MAG, SLOT_SIGHT, SLOT_STOCK,
       SLOT_ACC,   // [armas] accesorio: linterna, láser, bípode o bayoneta
       SLOT_COUNT };
typedef struct {
    int owned, mag;
    float cond;
    int load;                             // tipo de munición cargado (su lugar en WDEF.ammo)
    unsigned char part[SLOT_COUNT];       // piezas instaladas
} WeaponState;
// [armas] Arma botada al suelo: el Pick lleva ITEM_WDROP y en n el lugar (+1) en esta tabla. Se guarda con la partida
typedef struct { int used, w; WeaponState st; } WDrop;
#define WDROP_N 8
static WDrop g_wdrop[WDROP_N];
static void armas_gc(int scan_k);          // [arregla]
static void armas_light(void);            // [armas] ganchos: se definen en armas.inc
static void armas_bayonet(void);
static float armas_wear(int wi);
static int armas_inv_input(void);
static int armas_scav_input(void);
static void armas_ws_extra(int wi);
static void armas_wpick(void *pick);
static void armas_reset(void);
static void armas_pine(int type, int ey);
#define PESO_ON 1   // [peso] manda el peso, no el tope por objeto
static float peso_extra_kg(void);   // [peso] ganchos: se definen en peso.inc
static int peso_room(int item);
static void peso_warn(int item, int room);
static float peso_pct(void);
static float peso_slow(void);
static void peso_hud(void);
static void peso_draw_bag(void);
static void peso_new_game(void);
static void peso_save(WeaponState *arm, int *flags);
static void peso_load(const WeaponState *arm, int flags);
static WeaponState g_arm[W_MAX];   // [qsist] el armero (se define en peso.inc)
static float peso_total(void);
static float peso_cap(void);
static int peso_first_weapon(void);
static void peso_found_cond(int w);
static int peso_crate_gun(float x, float y);
static void peso_exped_hint(void);
static void peso_map_marks(float x0, float y0);
static void peso_arm_input(void);
static void peso_arm_draw(void);

#define DURO_PART 0
#include "duro.inc"   // [duro] filas nuevas de tablas (objetos, tienda, botín, recetas)
#undef DURO_PART
// Objetos: todo lo que se lleva en la mochila o se guarda en el alijo sale de la tabla ITEMS. Para sumar uno
// (piezas de armas, armadura, kits, materiales): su ITEM_ en el enum y su fila en ITEMS. Con eso ya lo manejan la
// mochila, el alijo, el suelo y la partida guardada. Además: una fila en SHOP si se vende, una en LOOT si sale en
// cajones y cuerpos, y su caso en use_item() si se usa desde la mochila.
// El orden del enum es el orden en que se ven en la mochila y el alijo. Como la partida guarda las cantidades por
// número, al insertar o mover uno hay que subir SAVE_VER.
enum {
    ITEM_BAND, ITEM_MED, ITEM_FOOD, ITEM_WATER, ITEM_ARAD, ITEM_FILTER, ITEM_CLEAN, ITEM_SEW,
    ITEM_RAG, ITEM_CHEM, ITEM_MECH, ITEM_POWDER,                                                       // materiales de fabricación (tabla RECIPES)
    ITEM_9MM, ITEM_9MM_AP, ITEM_9MM_HP, ITEM_762, ITEM_762_AP, ITEM_762_HP, ITEM_12G, ITEM_12G_SLUG,   // munición: un objeto por tipo
    ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK,                                           // piezas de armas (tabla PARTS)
    ITEM_JACKET, ITEM_VEST, ITEM_HVEST, ITEM_MASK,                                                     // equipo: armaduras y máscara (tabla GEAR)
    ITEM_DET, ITEM_ART0, ITEM_ART1, ITEM_ART2, ITEM_ART3, ITEM_ART4, ITEM_ART5,                        // detector y artefactos (equipo, tabla GEAR)
    ITEM_NUT, ITEM_GREN, ITEM_MOLO, ITEM_MINE, ITEM_TRAP,                                              // objetos de mano (se lanzan o se ponen con L + Triángulo)
    ITEM_SCRAP,
    ITEM_BK_BOX,                                                                                       // [bunker] la caja
    ITEM_R_CARGADOR, ITEM_R_ABRIGO, ITEM_R_FILTRO, ITEM_R_ESTIM, ITEM_R_AMULETO,   // [pozo] reliquias
    ITEM_BKD_LAMP, ITEM_BKD_RUG, ITEM_BKD_SHELF, ITEM_BKD_POSTER, ITEM_BKD_PLANT, ITEM_BKD_RADIO, ITEM_BKD_TABLE, ITEM_BKD_CHAIR, ITEM_BKD_T1, ITEM_BKD_T2,   // [bunker] decoraciones (en este orden: BKD_ + 1)
    ITEM_BKD_T3, ITEM_BKD_T4, ITEM_BKD_T5,   // [jefes2] trofeos: Viuda, Capataz y Fermentado
    ITEM_LONGB, ITEM_BRAKE, ITEM_SCOPE, ITEM_DRUM, ITEM_PADSTOCK, ITEM_LIGHT, ITEM_LASER, ITEM_BIPOD, ITEM_BAYO, ITEM_REPAIR, ITEM_WDROP,   // [armas] piezas nuevas, kit de reparación y arma botada
    ITEM_PACK1, ITEM_PACK2,   // [peso] mochila de lona y mochila de expedición
    DURO_ITEM_ENUM   // [duro]
    ITEM_ART6, ITEM_ART7, ITEM_ART8, ITEM_ART9,   // [artef] Corazón de brasa, Colmillo rojo, Imán negro y Semilla hueca
    ITEM_UP0, ITEM_UP1, ITEM_UP2, ITEM_UP3, ITEM_UP4, ITEM_UP5, ITEM_UP6, ITEM_UP7,   // [artef] las 8 piezas únicas
    ITEM_COUNT
};
#define ITEM_MAX 128      // lugar en la partida guardada: alcanza para los que se sumen
#define IF_NOSTASH 1     // no se guarda en el alijo (la chatarra)
typedef struct {
    const char *name;      // en la mochila y en las listas: "Vendas"
    const char *unit;      // en el cartel del botín: "+1 venda"
    const char *many;      // en los avisos: "Mochila llena de vendas"
    const char *use;       // para qué sirve (se lee en la mochila)
    short icon, ground;    // icono en la interfaz y sprite de cuando está tirado en el suelo
    short cap, pack;       // tope de carga en la mochila (0 = sin tope) y tamaño del paquete (de a cuántos pasa al alijo)
    u32 tint;              // color del sprite en el suelo
    int flags;             // IF_*
} ItemDef;
static const ItemDef ITEMS[ITEM_COUNT] = {
    [ITEM_BAND] = {"Vendas", "venda", "vendas", "Cortan el sangrado y +8 de vida", S_IT_BAND, S_I_BAND, 5, 1, COL_WHITE, 0},
    [ITEM_MED] = {"Botiquines", "botiquín", "botiquines", "+45 de vida", S_IT_MED, S_I_MED, 3, 1, COL_WHITE, 0},
    [ITEM_FOOD] = {"Conservas", "conserva", "conservas", "Quita el hambre y +6 de vida", S_IT_FOOD, S_I_FOOD, 5, 1, COL_WHITE, 0},
    [ITEM_WATER] = {"Agua", "agua", "agua", "Quita la sed", S_IT_WATER, S_I_WATER, 5, 1, COL_WHITE, 0},
    [ITEM_ARAD] = {"Antirrad.", "antirradiación", "antirradiación", "Baja la radiación", S_IT_ARAD, S_I_ARAD, 3, 1, COL_WHITE, 0},
    [ITEM_9MM] = {"Balas 9x18", "balas 9x18", "balas 9x18", "Munición de la Makarov", S_IT_9MM, S_I_AMMO, 64, 8, COL_WHITE, 0},
    [ITEM_9MM_AP] = {"9x18 perf.", "9x18 perforantes", "9x18 perforantes", "Ignoran el blindaje, -10 % de daño", S_IT_9MM_AP, S_I_AMMO, 32, 8, RGBA(170, 200, 255, 255), 0},
    [ITEM_9MM_HP] = {"9x18 exp.", "9x18 expansivas", "9x18 expansivas", "+35 % de daño si no hay blindaje", S_IT_9MM_HP, S_I_AMMO, 32, 8, RGBA(255, 196, 130, 255), 0},
    [ITEM_762] = {"Balas 7,62", "balas 7,62", "balas 7,62", "Munición del fusil AK", S_IT_762, S_I_AMMO, 80, 10, RGBA(170, 230, 160, 255), 0},
    [ITEM_762_AP] = {"7,62 perf.", "7,62 perforantes", "7,62 perforantes", "Ignoran el blindaje, -10 % de daño", S_IT_762_AP, S_I_AMMO, 40, 10, RGBA(170, 200, 255, 255), 0},
    [ITEM_762_HP] = {"7,62 exp.", "7,62 expansivas", "7,62 expansivas", "+35 % de daño si no hay blindaje", S_IT_762_HP, S_I_AMMO, 40, 10, RGBA(255, 196, 130, 255), 0},
    [ITEM_12G] = {"Cal. 12", "cartuchos", "cartuchos", "Perdigones para la recortada", S_IT_12G, S_I_AMMO, 24, 4, RGBA(255, 150, 140, 255), 0},
    [ITEM_12G_SLUG] = {"Bala cal. 12", "cartuchos de bala", "cartuchos de bala", "Un proyectil: daño 40 y más alcance", S_IT_12G_SLUG, S_I_AMMO, 12, 4, RGBA(150, 230, 150, 255), 0},
    // piezas de armas: se instalan en el taller de Estación Kolos (lo que hace cada una está en PARTS)
    [ITEM_SIL] = {"Silenciador", "silenciador", "silenciadores", "Boca: ruido a 1/4, -5 % de daño", S_IT_SIL, S_I_PART, 2, 1, COL_WHITE, 0},
    [ITEM_COMP] = {"Compens.", "compensador", "compensadores", "Boca: -25 % de dispersión", S_IT_COMP, S_I_PART, 2, 1, COL_WHITE, 0},
    [ITEM_XMAG] = {"Carg. ampl.", "cargador ampliado", "cargadores ampliados", "Cargador: 8 a 12 o 20 a 30 balas", S_IT_XMAG, S_I_PART, 2, 1, COL_WHITE, 0},
    [ITEM_REFLEX] = {"Mira réflex", "mira réflex", "miras réflex", "Mira: +15 % alcance, afina con L", S_IT_REFLEX, S_I_PART, 3, 1, COL_WHITE, 0},
    [ITEM_STOCK] = {"Culata", "culata", "culatas", "Culata: -40 % dispersión al andar", S_IT_STOCK, S_I_PART, 2, 1, COL_WHITE, 0},
    // equipo: se pone y se quita desde la mochila (lo que hace cada prenda está en GEAR). Solo cabe una de cada una
    [ITEM_JACKET] = {"Chaqueta", "chaqueta reforzada", "chaquetas reforzadas", "Protege 15 % de golpes y balas", S_IT_JACKET, S_I_VEST, 1, 1, COL_WHITE, 0},
    [ITEM_VEST] = {"Ch. ligero", "chaleco ligero", "chalecos ligeros", "Protege 30 % de golpes y balas", S_IT_VEST, S_I_VEST, 1, 1, COL_WHITE, 0},
    [ITEM_HVEST] = {"Ch. pesado", "chaleco pesado", "chalecos pesados", "Protege 45 %; 8 % más lento", S_IT_HVEST, S_I_VEST, 1, 1, COL_WHITE, 0},
    [ITEM_MASK] = {"Máscara", "máscara de gas", "máscaras de gas", "Con filtro: Ceniza -75 %, rad. 1/2", S_IT_MASK, S_I_MASK, 1, 1, COL_WHITE, 0},
    [ITEM_FILTER] = {"Filtros", "filtro", "filtros", "Repone el filtro de la máscara", S_IT_FILTER, S_I_MASK, 3, 1, COL_WHITE, 0},
    // kits de reparación: se usan desde la mochila
    [ITEM_CLEAN] = {"Kit limpieza", "kit de limpieza", "kits de limpieza", "+25 de estado al arma en mano", S_IT_CLEAN, S_I_PART, 3, 1, COL_WHITE, 0},
    [ITEM_SEW] = {"Kit costura", "kit de costura", "kits de costura", "+40 de estado a la armadura", S_IT_SEW, S_I_PART, 3, 1, COL_WHITE, 0},
    [ITEM_RAG] = {"Trapos", "trapo", "trapos", "Material: vendas, kits", S_IT_RAG, S_I_PART, 12, 1, RGBA(214, 208, 190, 255), 0},
    [ITEM_CHEM] = {"Químicos", "químico", "químicos", "Material: botiquines, filtros", S_IT_CHEM, S_I_PART, 8, 1, RGBA(120, 220, 130, 255), 0},
    [ITEM_MECH] = {"Piezas mec.", "pieza mecánica", "piezas mecánicas", "Material: kits de limpieza y costura", S_IT_MECH, S_I_PART, 8, 1, RGBA(170, 180, 196, 255), 0},
    [ITEM_POWDER] = {"Pólvora", "pólvora", "pólvora", "Material: munición", S_IT_POWDER, S_I_PART, 8, 1, RGBA(150, 140, 130, 255), 0},
    [ITEM_DET] = {"Detector", "detector de anomalías", "detectores", "Pita más rápido cerca de anomalías y marca artefactos", S_IT_DET, S_I_PART, 1, 1, RGBA(130, 220, 150, 255), 0},
    [ITEM_ART0] = {"Latido ámbar", "Latido ámbar", "artefactos", "Regenera vida rápido; suma radiación", S_IT_ART0, S_I_PART, 1, 1, RGBA(255, 190, 80, 255), 0},
    [ITEM_ART1] = {"Ascua mansa", "Ascua mansa", "artefactos", "Sin frío y +10 % velocidad; más hambre", S_IT_ART1, S_I_PART, 1, 1, RGBA(240, 100, 80, 255), 0},
    [ITEM_ART2] = {"Piel de plomo", "Piel de plomo", "artefactos", "35 % menos daño; 15 % más lento", S_IT_ART2, S_I_PART, 1, 1, RGBA(190, 205, 225, 255), 0},
    [ITEM_ART3] = {"Lágrima cuajada", "Lágrima cuajada", "artefactos", "No sangras, curas x2; -20 vida máx.", S_IT_ART3, S_I_PART, 1, 1, RGBA(120, 180, 250, 255), 0},
    [ITEM_ART4] = {"Pulmón de vidrio", "Pulmón de vidrio", "artefactos", "Aguante sin gasto; sed el doble", S_IT_ART4, S_I_PART, 1, 1, RGBA(190, 150, 240, 255), 0},
    [ITEM_ART5] = {"Ojo de ceniza", "Ojo de ceniza", "artefactos", "Sin Ceniza, radiación baja; pasos fuertes", S_IT_ART5, S_I_PART, 1, 1, RGBA(140, 230, 120, 255), 0},
    // objetos de mano: se eligen con L + izquierda/derecha y se lanzan o se ponen con L + Triángulo
    [ITEM_NUT] = {"Tuercas", "tuerca", "tuercas", "Se lanza: llama a los enemigos y delata anomalías", S_IT_NUT, S_I_PART, 10, 5, RGBA(200, 206, 216, 255), 0},
    [ITEM_GREN] = {"Granadas", "granada", "granadas", "Estalla en radio, daña y empuja. A ti también", S_IT_GREN, S_I_PART, 5, 1, RGBA(130, 200, 110, 255), 0},
    [ITEM_MOLO] = {"Cócteles", "cóctel incendiario", "cócteles incendiarios", "Deja un charco de fuego unos segundos", S_IT_MOLO, S_I_PART, 5, 1, RGBA(255, 170, 70, 255), 0},
    [ITEM_MINE] = {"Minas", "mina", "minas", "Se pone en el suelo: estalla si la pisa un enemigo", S_IT_MINE, S_I_PART, 3, 1, RGBA(150, 120, 110, 255), 0},
    [ITEM_TRAP] = {"Cepos", "cepo", "cepos", "Atrapa y daña al primero que lo pise. Se recoge", S_IT_TRAP, S_I_PART, 3, 1, RGBA(170, 176, 190, 255), 0},
    [ITEM_SCRAP] = {"Chatarra", "chatarra", "chatarra", "La paga de los trabajos", S_I_SCRAP, S_I_SCRAP, 0, 1, COL_WHITE, IF_NOSTASH},
    [ITEM_BK_BOX] = {"Caja", "caja", "cajas", "Se pone en tu búnker y guarda unos 60 objetos", S_IT_MECH, S_I_PART, 12, 1, RGBA(190, 140, 90, 255), 0},   // [bunker]
    [ITEM_BKD_LAMP] = {"Lámpara", "lámpara", "lámparas", "Decoración: da luz en el búnker", S_IT_NUT, S_I_PART, 4, 1, RGBA(255, 220, 140, 255), 0},   // [bunker]
    [ITEM_BKD_RUG] = {"Alfombra", "alfombra", "alfombras", "Decoración: se pisa", S_IT_RAG, S_I_PART, 4, 1, RGBA(190, 80, 70, 255), 0},   // [bunker]
    [ITEM_BKD_SHELF] = {"Estante", "estante", "estantes", "Decoración: macizo, no se cruza", S_IT_MECH, S_I_PART, 4, 1, RGBA(150, 110, 70, 255), 0},   // [bunker]
    [ITEM_BKD_POSTER] = {"Afiche", "afiche", "afiches", "Decoración: un cartel", S_IT_RAG, S_I_PART, 4, 1, RGBA(220, 210, 170, 255), 0},   // [bunker]
    [ITEM_BKD_PLANT] = {"Planta", "planta", "plantas", "Decoración: algo verde", S_IT_CHEM, S_I_PART, 4, 1, RGBA(110, 190, 100, 255), 0},   // [bunker]
    [ITEM_BKD_RADIO] = {"Radio", "radio", "radios", "Decoración: una radio vieja", S_IT_DET, S_I_PART, 4, 1, RGBA(180, 150, 110, 255), 0},   // [bunker]
    [ITEM_BKD_TABLE] = {"Mesa", "mesa", "mesas", "Decoración: macizo, no se cruza", S_IT_MECH, S_I_PART, 4, 1, RGBA(170, 130, 90, 255), 0},   // [bunker]
    [ITEM_BKD_CHAIR] = {"Silla", "silla", "sillas", "Decoración: una silla", S_IT_MECH, S_I_PART, 4, 1, RGBA(150, 110, 80, 255), 0},   // [bunker]
    [ITEM_BKD_T1] = {"Cab. Bestia", "cabeza de la Bestia", "cabezas de la Bestia", "Trofeo: la cabeza de La Bestia", S_IT_ART3, S_I_PART, 1, 1, RGBA(130, 220, 120, 255), 0},   // [bunker]
    [ITEM_R_CARGADOR] = {"Cargador eterno", "Cargador eterno", "reliquias", "Reliquia: el arma nunca se traba", S_IT_XMAG, S_I_PART, 1, 1, RGBA(255, 210, 90, 255), 0},   // [pozo]
    [ITEM_R_ABRIGO] = {"Abrigo del Pozo", "Abrigo del Pozo", "reliquias", "Reliquia: el frío no te toca", S_IT_VEST, S_I_VEST, 1, 1, RGBA(255, 150, 90, 255), 0},   // [pozo]
    [ITEM_R_FILTRO] = {"Filtro negro", "Filtro negro", "reliquias", "Reliquia: +3 min antes de la bomba", S_IT_FILTER, S_I_PART, 1, 1, RGBA(200, 120, 255, 255), 0},   // [pozo]
    [ITEM_R_ESTIM] = {"Estimulante rojo", "Estimulante rojo", "reliquias", "Reliquia: con poco aguante, 2 min de aguante doble", S_IT_MED, S_I_PART, 1, 1, RGBA(255, 90, 90, 255), 0},   // [pozo]
    [ITEM_R_AMULETO] = {"Amuleto de hueso", "Amuleto de hueso", "reliquias", "Reliquia: te resucita una vez", S_IT_ART3, S_I_PART, 1, 1, RGBA(240, 240, 220, 255), 0},   // [pozo]
    [ITEM_BKD_T2] = {"Casco Hierro", "casco de Cabeza de Hierro", "cascos de Cabeza de Hierro", "Trofeo: el casco de Cabeza de Hierro", S_IT_VEST, S_I_VEST, 1, 1, RGBA(200, 206, 220, 255), 0},   // [bunker]
    // [armas]
    [ITEM_LONGB] = {"Cañón largo", "cañón largo", "cañones largos", "Boca: +25 % de alcance, más ruido", S_IT_SIL, S_I_PART, 2, 1, RGBA(190, 200, 215, 255), 0},
    [ITEM_BRAKE] = {"Freno de boca", "freno de boca", "frenos de boca", "Boca: -20 % de dispersión", S_IT_COMP, S_I_PART, 2, 1, RGBA(230, 190, 150, 255), 0},
    [ITEM_SCOPE] = {"Mira telesc.", "mira telescópica", "miras telescópicas", "Mira: +40 % de alcance, +10 % de daño", S_IT_REFLEX, S_I_PART, 2, 1, RGBA(150, 220, 240, 255), 0},
    [ITEM_DRUM] = {"Tambor", "cargador tambor", "cargadores tambor", "Cargador: el doble de balas", S_IT_XMAG, S_I_PART, 2, 1, RGBA(240, 200, 120, 255), 0},
    [ITEM_PADSTOCK] = {"Culata acolch.", "culata acolchada", "culatas acolchadas", "Culata: -30 % de dispersión al andar y menos desgaste", S_IT_STOCK, S_I_PART, 2, 1, RGBA(220, 170, 140, 255), 0},
    [ITEM_LIGHT] = {"Linterna tác.", "linterna táctica", "linternas tácticas", "Accesorio: más luz en el cono", S_IT_NUT, S_I_PART, 2, 1, RGBA(255, 240, 170, 255), 0},
    [ITEM_LASER] = {"Láser", "láser", "láseres", "Accesorio: -40 % de dispersión al andar", S_IT_DET, S_I_PART, 2, 1, RGBA(255, 110, 110, 255), 0},
    [ITEM_BIPOD] = {"Bípode", "bípode", "bípodes", "Accesorio: -35 % de dispersión quieto", S_IT_MECH, S_I_PART, 2, 1, RGBA(180, 190, 170, 255), 0},
    [ITEM_BAYO] = {"Bayoneta", "bayoneta", "bayonetas", "Accesorio: R de cerca da un golpe", S_IT_TRAP, S_I_PART, 2, 1, RGBA(210, 215, 225, 255), 0},
    [ITEM_REPAIR] = {"Kit repar.", "kit de reparación", "kits de reparación", "Arma, armadura o máscara al 100 %", S_IT_CLEAN, S_I_PART, 3, 1, RGBA(255, 190, 110, 255), 0},
    [ITEM_WDROP] = {"Arma", "arma", "armas", "Un arma tirada", S_IT_MECH, S_I_PART, 0, 1, COL_WHITE, IF_NOSTASH},
    [ITEM_BKD_T3] = {"Piel Viuda", "piel de La Viuda blanca", "pieles de La Viuda blanca", "Trofeo: la piel de La Viuda blanca", S_IT_ART0, S_I_PART, 1, 1, RGBA(210, 232, 255, 255), 0},   // [jefes2]
    [ITEM_BKD_T4] = {"Mazo Capataz", "mazo del Capataz", "mazos del Capataz", "Trofeo: el mazo del Capataz", S_IT_MECH, S_I_PART, 1, 1, RGBA(255, 180, 90, 255), 0},   // [jefes2]
    DURO_ITEM_ROWS   // [duro]
    [ITEM_BKD_T5] = {"Frasco Fermen.", "frasco del Fermentado", "frascos del Fermentado", "Trofeo: un frasco del Fermentado", S_IT_ART4, S_I_PART, 1, 1, RGBA(170, 255, 90, 255), 0},   // [jefes2]
    [ITEM_PACK1] = {"Mochila lona", "mochila de lona", "mochilas de lona", "Mochila: +8 kg de carga, cuenta sola", S_IT_VEST, S_I_VEST, 0, 1, RGBA(170, 190, 120, 255), 0},   // [peso]
    [ITEM_PACK2] = {"Mochila exped.", "mochila de expedición", "mochilas de expedición", "Mochila: +18 kg de carga, cuenta sola", S_IT_HVEST, S_I_VEST, 0, 1, RGBA(210, 160, 90, 255), 0},   // [peso]
    [ITEM_ART6] = {"Corazón de brasa", "Corazón de brasa", "artefactos", "+40 vida máx.; suma radiación", S_IT_ART1, S_I_PART, 1, 1, RGBA(255, 80, 50, 255), 0},
    [ITEM_ART7] = {"Colmillo rojo", "Colmillo rojo", "artefactos", "+30 % daño; suma radiación", S_IT_ART4, S_I_PART, 1, 1, RGBA(230, 40, 60, 255), 0},
    [ITEM_ART8] = {"Imán negro", "Imán negro", "artefactos", "+50 % chatarra, imán; te ven de lejos", S_IT_ART2, S_I_PART, 1, 1, RGBA(120, 120, 150, 255), 0},
    [ITEM_ART9] = {"Semilla hueca", "Semilla hueca", "artefactos", "+15 kg de carga; más hambre", S_IT_ART5, S_I_PART, 1, 1, RGBA(200, 230, 120, 255), 0},
    [ITEM_UP0] = {"Susurro", "susurro de la bestia", "susurro de la bestia", "Boca única: silenciador sin pérdida", S_IT_SIL, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP1] = {"Mirada", "mirada de hierro", "mirada de hierro", "Mira única: marca tras los muros", S_IT_REFLEX, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP2] = {"Tambor Viuda", "cargador de la viuda", "cargador de la viuda", "Cargador único: recarga al instante", S_IT_XMAG, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP3] = {"Culata Capat.", "culata del capataz", "culata del capataz", "Culata única: sin retroceso", S_IT_STOCK, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP4] = {"Boca Ferm.", "boca del fermentado", "boca del fermentado", "Boca única: más daño y alcance", S_IT_COMP, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP5] = {"Núcleo", "núcleo del guardián", "núcleo del guardián", "Accesorio único: daño y precisión", S_IT_DET, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP6] = {"Visor piloto", "visor del piloto", "visor del piloto", "Mira única: precisión y alcance", S_IT_REFLEX, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
    [ITEM_UP7] = {"Linterna Far.", "linterna farolera", "linterna farolera", "Accesorio único: luz larga y precisión", S_IT_NUT, S_I_PART, 1, 1, RGBA(255, 214, 96, 255), 0},   // [artef] única
};
_Static_assert(ITEM_COUNT <= ITEM_MAX, "ITEM_MAX quedó chico: subirlo (y SAVE_VER)");
static int g_bag[ITEM_COUNT], g_stash[ITEM_COUNT];   // cuántos hay de cada objeto en la mochila y en el alijo

static const WeaponDef WDEF[W_COUNT] = {
    [W_MAKAROV] = {"Makarov", 8, 0.24f, 17, 0.040f, 1.3f, 440, 250, 1,
                   {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                   1.5f, 0.004f, 1.0f, 230, S_PISTOL, S_W_PISTOL, S_CASING, 1.0f},
    [W_SAWED] = {"Recortada", 2, 0.45f, 12, 0.19f, 1.9f, 390, 135, 6,
                 {{ITEM_12G, B_SHOT}, {ITEM_12G_SLUG, B_SLUG}}, 2,
                 4.0f, 0.004f, 1.0f, 230, S_SHOTGUN, S_W_SHOTGUN, S_SHELL, 1.3f},
    // automático: unos 9 tiros por segundo. Se gasta más por tiro y se oye de más lejos que la pistola; como dispara
    // tanto, cada tiro se traba la mitad
    [W_AK] = {"Fusil AK", 20, 0.11f, 15, 0.055f, 2.1f, 520, 310, 1,
              {{ITEM_762, B_NORMAL}, {ITEM_762_AP, B_AP}, {ITEM_762_HP, B_HP}}, 3,
              2.2f, 0.005f, 0.5f, 300, S_RIFLE, S_W_AK, S_CASING, 1.15f},
    // cuerpo a cuerpo: sin munición y sin trabas. rate = segundos entre golpes, dmg = daño, range = alcance en px, noise = hasta dónde
    // se oye (casi nada), kick = sacudida de pantalla. El arco, el empuje y el aturdimiento están en MEL (más abajo)
    [W_KNIFE] = {"Cuchillo", 0, 0.28f, 14, 0, 0, 0, 20, 1, {{ITEM_BAND, B_NORMAL}}, 1, 1.0f, 0.004f, 0, 40, S_W_KNIFE, S_W_KNIFE, S_CASING, 0},
    [W_AXE] = {"Hacha", 0, 0.55f, 34, 0, 0, 0, 24, 1, {{ITEM_BAND, B_NORMAL}}, 1, 1.6f, 0.004f, 0, 45, S_W_AXE, S_W_AXE, S_CASING, 0},
    [W_PIPE] = {"Tubo de acero", 0, 0.5f, 20, 0, 0, 0, 27, 1, {{ITEM_BAND, B_NORMAL}}, 1, 1.8f, 0.003f, 0, 45, S_W_PIPE, S_W_PIPE, S_CASING, 0},
    [W_MAUL] = {"Mazo", 0, 1.0f, 55, 0, 0, 0, 29, 1, {{ITEM_BAND, B_NORMAL}}, 1, 4.5f, 0.005f, 0, 55, S_W_MAUL, S_W_MAUL, S_CASING, 0},
    // armas de fuego nuevas
    [W_REVOLVER] = {"Revólver", 6, 0.55f, 31, 0.022f, 1.9f, 470, 270, 1,
                    {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                    2.6f, 0.005f, 1.0f, 260, S_W_REVOLVER, S_W_REVOLVER, S_CASING, 1.2f},
    [W_SMG] = {"Subfusil", 25, 0.07f, 8, 0.075f, 1.7f, 430, 190, 1,
               {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
               1.0f, 0.0035f, 0.6f, 250, S_W_SMG, S_W_SMG, S_CASING, 0.9f},
    [W_PUMP] = {"Escopeta de corredera", 7, 0.7f, 12, 0.17f, 2.4f, 390, 160, 6,
                {{ITEM_12G, B_SHOT}, {ITEM_12G_SLUG, B_SLUG}}, 2,
                3.8f, 0.004f, 1.0f, 230, S_W_PUMP, S_W_PUMP, S_SHELL, 1.3f},
    [W_BOLT] = {"Fusil de cerrojo", 5, 0.95f, 64, 0.010f, 2.6f, 680, 640, 1,
                {{ITEM_762, B_NORMAL}, {ITEM_762_AP, B_AP}, {ITEM_762_HP, B_HP}}, 3,
                3.2f, 0.006f, 0.8f, 320, S_W_BOLT, S_W_BOLT, S_CASING, 1.2f, 1},
    // [armas2] ocho armas nuevas (sprites del paquete Pixel Gun Pack)
    [W_VIBORA] = {"Pistola Víbora", 15, 0.16f, 13, 0.045f, 1.4f, 450, 260, 1,
                  {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                  1.3f, 0.004f, 1.0f, 220, S_W_A2_VIBORA, S_W_A2_VIBORA, S_CASING, 1.0f},
    [W_CHACAL] = {"Chacal pesada", 9, 0.30f, 24, 0.035f, 1.7f, 460, 290, 1,
                  {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                  2.2f, 0.005f, 1.0f, 260, S_W_A2_CHACAL, S_W_A2_CHACAL, S_CASING, 1.2f},
    [W_ZUMBA] = {"Subfusil Zumbador", 30, 0.065f, 9, 0.070f, 1.6f, 440, 200, 1,
                 {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                 1.0f, 0.0035f, 0.6f, 240, S_W_A2_ZUMBA, S_W_A2_ZUMBA, S_CASING, 0.9f},
    [W_TORM] = {"Subfusil Tormenta", 40, 0.075f, 10, 0.085f, 2.0f, 430, 210, 1,
                {{ITEM_9MM, B_NORMAL}, {ITEM_9MM_AP, B_AP}, {ITEM_9MM_HP, B_HP}}, 3,
                1.2f, 0.004f, 0.6f, 255, S_W_A2_TORM, S_W_A2_TORM, S_CASING, 1.0f},
    [W_CUERVO] = {"Escopeta Cuervo", 6, 0.50f, 11, 0.150f, 2.6f, 400, 170, 8,
                  {{ITEM_12G, B_SHOT}, {ITEM_12G_SLUG, B_SLUG}}, 2,
                  3.6f, 0.004f, 1.0f, 240, S_W_A2_CUERVO, S_W_A2_CUERVO, S_SHELL, 1.3f},
    [W_HALCON] = {"Fusil Halcón", 30, 0.10f, 17, 0.045f, 2.0f, 540, 330, 1,
                  {{ITEM_762, B_NORMAL}, {ITEM_762_AP, B_AP}, {ITEM_762_HP, B_HP}}, 3,
                  2.0f, 0.005f, 0.5f, 310, S_W_A2_HALCON, S_W_A2_HALCON, S_CASING, 1.15f},
    [W_CENTINELA] = {"Fusil Centinela", 7, 0.80f, 78, 0.008f, 2.8f, 700, 700, 1,
                     {{ITEM_762, B_NORMAL}, {ITEM_762_AP, B_AP}, {ITEM_762_HP, B_HP}}, 3,
                     3.4f, 0.006f, 0.8f, 330, S_W_A2_CENTINELA, S_W_A2_CENTINELA, S_CASING, 1.25f, 2},
    [W_APLANA] = {"Aplanadora", 60, 0.085f, 19, 0.090f, 4.2f, 520, 340, 1,
                  {{ITEM_762, B_NORMAL}, {ITEM_762_AP, B_AP}, {ITEM_762_HP, B_HP}}, 3,
                  2.5f, 0.006f, 0.5f, 340, S_W_A2_APLANA, S_W_A2_APLANA, S_CASING, 1.3f, 1},
};
// Cuerpo a cuerpo: el arco del golpe (radianes), cuánto empuja (px que retrocede un enemigo normal), cuánto lo aturde (s) y si le
// pega a todos los que alcanza (1) o solo al más cercano (0)
typedef struct { float arc, push, stun; int all; } MelDef;
static const MelDef MEL[4] = {
    [W_KNIFE - W_KNIFE] = {1.2f, 10, 0.12f, 0}, [W_AXE - W_KNIFE] = {1.6f, 26, 0.22f, 0},
    [W_PIPE - W_KNIFE] = {1.5f, 46, 0.65f, 0}, [W_MAUL - W_KNIFE] = {2.4f, 72, 0.85f, 1},
};
_Static_assert(W_COUNT <= W_MAX, "W_MAX quedó chico: subirlo (y SAVE_VER)");

// Piezas de armas: una fila por pieza. Dice en qué ranura va, qué armas la aceptan y por cuánto multiplica cada
// valor del arma (1 = no lo cambia). Para sumar una pieza: su ITEM_ y su fila en ITEMS, y su fila aquí.
// Como la partida guarda las piezas puestas por su lugar en esta tabla, al insertar o mover una hay que subir SAVE_VER
#define FITS(w) (1 << (w))
typedef struct {
    const char *name;        // en el taller
    const char *desc[2];     // qué hace, en dos renglones
    short item, slot;        // el objeto de la mochila (ITEM_) y la ranura (SLOT_)
    int fits;                // armas que la aceptan: FITS(W_...) | ...
    float noise, dmg;        // ruido del disparo y daño
    float spread;            // dispersión, siempre
    float aim, move;         // dispersión al apuntar con L y en movimiento
    float range, mag;        // alcance y balas por cargador
} PartDef;
static const char *const SLOT_NAME[SLOT_COUNT] = {"BOCA", "CARGADOR", "MIRA", "CULATA", [SLOT_ACC] = "ACCESORIO"};   // [armas]
static PartDef PARTS[] = {   // [armas2] no const: armas2_fits() le suma las armas nuevas
    {"Silenciador", {"El disparo se oye a la cuarta parte", "de distancia. 5 % menos de daño."}, ITEM_SIL, SLOT_MUZZLE,
     FITS(W_MAKAROV) | FITS(W_AK) | FITS(W_REVOLVER) | FITS(W_SMG) | FITS(W_BOLT), 0.25f, 0.95f, 1, 1, 1, 1, 1},
    {"Compensador", {"25 % menos de dispersión.", ""}, ITEM_COMP, SLOT_MUZZLE,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP), 1, 1, 0.75f, 1, 1, 1, 1},
    {"Cargador ampliado", {"Más balas por cargador: la pistola", "de 8 a 12 y el fusil de 20 a 30."}, ITEM_XMAG, SLOT_MAG,
     FITS(W_MAKAROV) | FITS(W_AK) | FITS(W_SMG), 1, 1, 1, 1, 1, 1, 1.5f},
    {"Mira réflex", {"20 % menos de dispersión al apuntar", "con L y 15 % más de alcance."}, ITEM_REFLEX, SLOT_SIGHT,
     FITS(W_MAKAROV) | FITS(W_SAWED) | FITS(W_AK) | FITS(W_REVOLVER) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 1, 0.8f, 1, 1.15f, 1},
    {"Culata", {"40 % menos de dispersión al disparar", "en movimiento."}, ITEM_STOCK, SLOT_STOCK,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 1, 1, 0.6f, 1, 1},
    // [armas] piezas nuevas (al final: la partida guarda las puestas por su lugar en esta tabla)
#define ARM_FIRE (FITS(W_MAKAROV) | FITS(W_SAWED) | FITS(W_AK) | FITS(W_REVOLVER) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT))
    {"Cañón largo", {"25 % más de alcance, pero el", "disparo se oye más lejos."}, ITEM_LONGB, SLOT_MUZZLE,
     ARM_FIRE, 1.3f, 1, 1, 1, 1, 1.25f, 1},
    {"Freno de boca", {"20 % menos de dispersión al", "disparar seguido."}, ITEM_BRAKE, SLOT_MUZZLE,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 0.8f, 1, 1, 1, 1},
    {"Mira telescópica", {"40 % más de alcance de fijado", "y 10 % más de daño."}, ITEM_SCOPE, SLOT_SIGHT,
     FITS(W_AK) | FITS(W_BOLT), 1, 1.1f, 1, 1, 1, 1.4f, 1},
    {"Cargador tambor", {"El doble de balas por", "cargador."}, ITEM_DRUM, SLOT_MAG,
     FITS(W_SMG) | FITS(W_AK), 1, 1, 1, 1, 1, 1, 2},
    {"Culata acolchada", {"30 % menos de dispersión al", "andar y menos desgaste."}, ITEM_PADSTOCK, SLOT_STOCK,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 1, 1, 0.7f, 1, 1},
    {"Linterna táctica", {"Más luz y más larga en el", "cono de la linterna."}, ITEM_LIGHT, SLOT_ACC,
     ARM_FIRE, 1, 1, 1, 1, 1, 1, 1},
    {"Láser", {"40 % menos de dispersión al", "disparar en movimiento."}, ITEM_LASER, SLOT_ACC,
     ARM_FIRE, 1, 1, 1, 1, 0.6f, 1, 1},
    {"Bípode", {"35 % menos de dispersión", "cuando estás quieto."}, ITEM_BIPOD, SLOT_ACC,
     FITS(W_AK) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 0.65f, 1, 1.54f, 1, 1},
    {"Bayoneta", {"R a menos de 24 px da un golpe", "cuerpo a cuerpo."}, ITEM_BAYO, SLOT_ACC,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 1, 1, 1, 1, 1},
    // [artef] piezas únicas (una sola en toda la partida; en dorado en el taller)
    {"Susurro de la Bestia", {"Se oye a la octava parte y no", "quita nada de daño. Única."}, ITEM_UP0, SLOT_MUZZLE,
     ARM_FIRE, 0.12f, 1, 1, 1, 1, 1, 1},
    {"Mirada de hierro", {"Marca a los enemigos tras los muros", "cercanos. Más puntería y 15 % de daño."}, ITEM_UP1, SLOT_SIGHT,
     ARM_FIRE, 1, 1.15f, 1, 0.65f, 1, 1.3f, 1},
    {"Cargador de la Viuda", {"Recarga al instante y trae", "60 % más balas. Única."}, ITEM_UP2, SLOT_MAG,
     FITS(W_MAKAROV) | FITS(W_AK) | FITS(W_SMG), 1, 1, 1, 1, 1, 1, 1.6f},
    {"Culata del Capataz", {"Sin dispersión al moverte y 15 %", "menos al disparar. Única."}, ITEM_UP3, SLOT_STOCK,
     FITS(W_AK) | FITS(W_SAWED) | FITS(W_SMG) | FITS(W_PUMP) | FITS(W_BOLT), 1, 1, 0.85f, 1, 0.05f, 1, 1},
    {"Boca del Fermentado", {"30 % más de daño y 20 % más de", "alcance, y menos ruido. Única."}, ITEM_UP4, SLOT_MUZZLE,
     ARM_FIRE, 0.7f, 1.3f, 0.85f, 1, 1, 1.2f, 1},
    {"Núcleo del guardián", {"20 % más de daño, mucha menos", "dispersión y en movimiento. Única."}, ITEM_UP5, SLOT_ACC,
     ARM_FIRE, 1, 1.2f, 0.8f, 0.8f, 0.5f, 1, 1},
    {"Visor del piloto", {"Apunta muy fino, 50 % más de", "alcance y 10 % de daño. Única."}, ITEM_UP6, SLOT_SIGHT,
     ARM_FIRE, 1, 1.1f, 1, 0.55f, 1, 1.5f, 1},
    {"Linterna farolera", {"Luz larga y 25 % menos de", "dispersión, más 10 % de daño. Única."}, ITEM_UP7, SLOT_ACC,
     ARM_FIRE, 1, 1.1f, 0.75f, 1, 1, 1, 1},
};
#define PARTS_N ((int)(sizeof(PARTS) / sizeof(PARTS[0])))

// Los valores con que de verdad dispara un arma: los de WDEF, con lo que cambia el tipo de munición cargado (BDEF)
// y con las piezas puestas. Lo usan el disparo, la recarga, el HUD y el taller (que así muestra lo que cambiaría)
typedef struct {
    int mag, pellets;          // balas por cargador y proyectiles por tiro
    float rate, dmg;           // segundos entre tiros y daño por proyectil
    float spread, aim, move;   // dispersión, y por cuánto se multiplica al apuntar con L y en movimiento
    float range, reach;        // alcance de la bala, y cuánto más lejos se fija un blanco (1 = lo de siempre)
    float noise;               // hasta dónde se oye
} WeaponStats;
// Habilidades (se gastan puntos en la pestaña HABILIDADES de la mochila; cada una se compra una sola vez)
enum { SK_MULA, SK_MANOS, SK_PIEL, SK_PULSO, SK_CARNI, SK_GATO, SK_ESTOM, SK_REBUS, SK_ADIES, SK_N };
static int g_sk[SK_N];
static WeaponStats weapon_stats(int wi, const unsigned char *part, int load)
{
    const WeaponDef *d = &WDEF[wi];
    const BulletDef *bd = &BDEF[d->ammo[load].kind];
    WeaponStats s = {d->mag, bd->pellets ? bd->pellets : d->pellets, d->rate, bd->dmg > 0 ? bd->dmg : d->dmg,
                     bd->spread > 0 ? bd->spread : d->spread, 0.6f, 1.7f, bd->range > 0 ? bd->range : d->range, 1, d->noise};
    float mag = d->mag;
    for (int k = 0; k < SLOT_COUNT; k++) {
        if (!part[k] || part[k] > PARTS_N) continue;
        const PartDef *p = &PARTS[part[k] - 1];
        s.noise *= p->noise; s.dmg *= p->dmg; s.spread *= p->spread; s.aim *= p->aim; s.move *= p->move;
        s.range *= p->range; s.reach *= p->range; mag *= p->mag;
    }
    s.mag = (int)(mag + 0.5f);
    if (g_sk[SK_PULSO]) s.spread *= 0.8f;   // Pulso firme
    return s;
}

static int item_cap(int item)
{
    if (PESO_ON) return 0;   // [peso] manda el peso, no el tope por objeto
    return ITEMS[item].cap * (g_sk[SK_MULA] ? 3 : 2) / 2;
}
static int inv_count(int item) { return g_bag[item]; }
// Cuántos más caben en la mochila (lo que no tiene tope, siempre cabe)
static int inv_room(int item)
{
    if (PESO_ON) return peso_room(item);   // [peso]
    int cap = item_cap(item), room = cap - g_bag[item];
    return !cap ? 1 << 20 : room > 0 ? room : 0;
}
// Mete en la mochila lo que quepa de n y devuelve cuántos entraron
static int inv_add(int item, int n)
{
    int room = inv_room(item);
    peso_warn(item, room);   // [peso]
    if (n > room) n = room;
    if (n < 0) n = 0;
    g_bag[item] += n;
    return n;
}
// Saca de la mochila hasta n y devuelve cuántos sacó
static int inv_take(int item, int n)
{
    if (n > g_bag[item]) n = g_bag[item];
    if (n < 0) n = 0;
    g_bag[item] -= n;
    return n;
}
// Los objetos que hay, en orden, para mostrarlos: los de cantidad 0 no se listan. `where` dice dónde se mira
// (LIST_BAG, LIST_STASH o los dos); si incluye el alijo, quedan fuera los que no se guardan ahí. Devuelve cuántos son
#define LIST_BAG 1
#define LIST_STASH 2
static int inv_list(unsigned char *out, int where)
{
    int n = 0;
    for (int i = 0; i < ITEM_COUNT; i++) {
        if ((where & LIST_STASH) && (ITEMS[i].flags & IF_NOSTASH)) continue;
        if (((where & LIST_BAG) && g_bag[i] > 0) || ((where & LIST_STASH) && g_stash[i] > 0)) out[n++] = i;
    }
    return n;
}
// Para una lista que no cabe entera: deja `sel` dentro de los n que hay y corre `top` (la primera fila a la vista)
// para que la fila de `sel` se vea. cols = casilleros por fila (1 en las listas), rows = filas a la vista
static void list_keep(int *sel, int *top, int n, int cols, int rows)
{
    int last = n > 0 ? (n - 1) / cols : 0, max_top = last + 1 > rows ? last + 1 - rows : 0, row;
    if (*sel >= n) *sel = n > 0 ? n - 1 : 0;
    if (*sel < 0) *sel = 0;
    row = *sel / cols;
    if (row < *top) *top = row;
    if (row >= *top + rows) *top = row - rows + 1;
    if (*top > max_top) *top = max_top;
    if (*top < 0) *top = 0;
}

// Equipo del jugador: dos ranuras, ARMADURA y MÁSCARA. Una fila por prenda en GEAR. Se ponen y se quitan desde la
// mochila (Cruz sobre el objeto, o sobre lo puesto en el panel EQUIPO): la prenda puesta sale de la mochila.
// Cada prenda tiene un estado de 100 a 0 (g_gcond): el de la armadura baja con cada golpe y en 0 ya no protege; el de
// la máscara es lo que le queda al filtro. Como el estado va por prenda y no por unidad, solo se puede tener una de
// cada una (puesta, en la mochila o en el alijo): el comerciante no vende otra.
// La partida guarda lo puesto y los estados por su lugar en esta tabla: al insertar o mover una fila, subir SAVE_VER
enum { EQ_ARMOR, EQ_MASK, EQ_DET, EQ_ART1, EQ_ART2, EQ_ART3, EQ_COUNT };   // [artef] EQ_ART3: tercera ranura de artefactos, desde rango 3
//   // armadura, máscara, detector y dos ranuras de artefactos (cada artefacto va en cualquiera de las dos)
typedef struct {
    const char *name;    // el nombre completo, en los avisos
    short item, slot;    // el objeto de la mochila (ITEM_) y la ranura (EQ_)
    float prot;          // armadura: la parte del daño de golpes y balas que frena
    float slow;          // por cuánto multiplica la velocidad al caminar (1 = no pesa)
} GearDef;
static const GearDef GEAR[] = {
    {"Chaqueta reforzada", ITEM_JACKET, EQ_ARMOR, 0.15f, 1},
    {"Chaleco ligero", ITEM_VEST, EQ_ARMOR, 0.30f, 1},
    {"Chaleco pesado", ITEM_HVEST, EQ_ARMOR, 0.45f, 0.92f},
    {"Máscara de gas", ITEM_MASK, EQ_MASK, 0, 1},
    {"Detector de anomalías", ITEM_DET, EQ_DET, 0, 1},
    {"Latido ámbar", ITEM_ART0, EQ_ART1, 0, 1},    // los artefactos van seguidos, empezando en GI_ART0
    {"Ascua mansa", ITEM_ART1, EQ_ART1, 0, 1},
    {"Piel de plomo", ITEM_ART2, EQ_ART1, 0, 1},
    {"Lágrima cuajada", ITEM_ART3, EQ_ART1, 0, 1},
    {"Pulmón de vidrio", ITEM_ART4, EQ_ART1, 0, 1},
    {"Ojo de ceniza", ITEM_ART5, EQ_ART1, 0, 1},
    {"Corazón de brasa", ITEM_ART6, EQ_ART1, 0, 1},   // [artef]
    {"Colmillo rojo", ITEM_ART7, EQ_ART1, 0, 1},   // [artef]
    {"Imán negro", ITEM_ART8, EQ_ART1, 0, 1},   // [artef]
    {"Semilla hueca", ITEM_ART9, EQ_ART1, 0, 1},   // [artef]
};
#define GI_ART0 5
#define GEAR_N ((int)(sizeof(GEAR) / sizeof(GEAR[0])))
#define GEAR_MAX 16   // lugar en la partida guardada: alcanza para las que se sumen
_Static_assert(GEAR_N <= GEAR_MAX, "GEAR_MAX quedó chico: subirlo (y SAVE_VER)");
static int g_eq[EQ_COUNT];       // lo puesto en cada ranura: su lugar en GEAR + 1 (0 = nada)
static float g_gcond[GEAR_N];    // estado de cada prenda, de 0 a 100 (el de la máscara es su filtro)

// La prenda (su lugar en GEAR) que es ese objeto; -1 si el objeto no es equipo
static int gear_of(int item)
{
    for (int i = 0; i < GEAR_N; i++)
        if (GEAR[i].item == item) return i;
    return -1;
}
// Llevas puesta esa prenda
static int gear_worn(int gi) { for (int k = 0; k < EQ_COUNT; k++) if (g_eq[k] == gi + 1) return 1; return 0; }
// La prenda gi cabe en esa ranura (los artefactos van en cualquiera de las dos)
static int slot_ok(int gi, int slot) { int sl = GEAR[gi].slot; return sl == slot || (sl == EQ_ART1 && (slot == EQ_ART2 || slot == EQ_ART3)); }
// Llevas puesto el artefacto i (0 a 5): sus efectos están en update_player y hurt_player
// art_on(i) y los efectos de los artefactos: src/artef.inc [artef]
// Ya tienes esa prenda: puesta, en la mochila o en el alijo
static int gear_owned(int gi) { return gear_worn(gi) || g_bag[GEAR[gi].item] > 0 || g_stash[GEAR[gi].item] > 0; }
// La máscara puesta y con filtro: su lugar en GEAR; -1 si no hay o el filtro se agotó
static int mask_on(void) { int gi = g_eq[EQ_MASK] - 1; return gi >= 0 && g_gcond[gi] > 0 ? gi : -1; }

// ------------------------------------------------------------------ estado
enum { GS_TITLE, GS_PLAY, GS_PAUSE, GS_INV, GS_MAP, GS_DEAD, GS_KOLOS, GS_SLEEP, GS_OPT, GS_ZONE, GS_CHOFER, GS_FIN, GS_RET, GS_FARO, GS_ENDSEL };   // GS_RET: el resumen al volver de una expedición
enum { E_DOG, E_MAR, E_BIG, E_BAN, E_GRD, E_BOSS, E_RUN, E_SPIT, E_ARM, E_SNP, E_WOLF, E_BOSS2,
    E_VIUDA, E_CAPATAZ, E_FERMEN,   // [jefes2] La Viuda blanca, El Capataz y El Fermentado
    E_TYPES };   // zombi chico, del hacha, grande; bandido y guardia de Kolos (humanos); La Bestia del charco (jefe); corredor, escupidor, acorazado, francotirador bandido (humano), lobo blanco y Cabeza de Hierro (segundo jefe)
enum { F_CREA, F_KOLOS, F_BAND, F_FARO };   // bandos: criaturas, Kolos (el jugador y sus guardias) y bandidos. Cada uno es adversario de los otros dos
static const float ARMOR[E_TYPES] = {[E_DOG] = 0, [E_MAR] = 0.25f, [E_BIG] = 0.35f, [E_BAN] = 0.2f, [E_GRD] = 0.2f, [E_BOSS] = 0.3f, [E_SPIT] = 0.1f, [E_ARM] = 0.5f, [E_SNP] = 0.1f, [E_BOSS2] = 0.4f,
    [E_VIUDA] = 0.25f, [E_CAPATAZ] = 0.35f, [E_FERMEN] = 0.15f};   // [jefes2]   // blindaje de cada enemigo: el zombi del hacha y el grande llevan algo encima
static const float EHP[E_TYPES] = {[E_DOG] = 34, [E_MAR] = 50, [E_BIG] = 140, [E_BAN] = 60, [E_GRD] = 60, [E_BOSS] = 600, [E_RUN] = 20, [E_SPIT] = 40, [E_ARM] = 130, [E_SNP] = 55, [E_WOLF] = 42, [E_BOSS2] = 560,
    [E_VIUDA] = 520, [E_CAPATAZ] = 700, [E_FERMEN] = 760};   // [jefes2]         // vida de cada tipo
#define RAD_NEAR 6          // los zombis que nacen a menos de tantas casillas del charco radiactivo nacen irradiados
#define IRR_DMG 1.25f       // el irradiado pega un 25 % más fuerte
enum { ES_IDLE, ES_ALERT, ES_COMBAT, ES_DEAD };

typedef struct {
    float x, y, ang, hp, rad, bleed, stam;
    float hunger, thirst;   // hambre y sed: 100 = satisfecho, 0 = se muere de a poco
    float fire_cd, reload_t, jam_t, hurt_t, flash_t, walk, retarget, drip;
    float sq_t;     // cuánto lleva mantenido Cuadrado (-1 = no cuenta)
    int weapon, jammed, lock, moving, has_job;
    float kx, ky;   // empujón en curso (px/s): lo dan los golpes fuertes de los enemigos grandes y de los jefes
} Player;

typedef struct {
    int type, state, moving, burst;
    float x, y, ang, hp, hx, hy, tx, ty;
    float t, cd, walk, flash, muzzle, drip, strafe, strafe_t, see_t, burst_t, stuck, detour_t, detour_a;
    float dead_t;   // cuando murio (g_time): solo lo usa el dibujo, para la caida
    float armor;    // blindaje, de 0 a 0,8: la parte del daño de una bala que frena
    int irr;        // irradiado: pega más, alumbra de verde, suma radiación y suelta un químico al morir
    int fac;        // bando (F_): el guardia al que le disparas pasa a F_BAND
    int var;        // humanos: 0 = pistola, 1 = fusil
    int summon;     // zombi chico reservado que La Bestia llama (vive escondido hasta que lo llama; no reaparece)
    int atk;        // el jefe: ataque en curso (BA_)
    float atk_t, atk_a, call_t;   // el jefe: lo que le queda al ataque, hacia dónde va y cuánto falta para que llame refuerzos
    float kx, ky, stun;           // empujón en curso (px/s, lo da el cuerpo a cuerpo del jugador) y aturdimiento que le queda (s)
    int mini; float hpmax;        // [armas2] minijefe suelto (1 a 4; 0 = no) y su vida máxima
    int ev, mob;                  // de un evento de la expedición (EVN_...: 1 guardia de caravana, 2 bandido de caravana, 3 horda, 4 guardián del helicóptero); mob: el guardia amigo que pelea y se mueve (no se queda en su puesto)
} Enemy;
#define K_ACID 99   // Bullet.kind del escupitajo ácido (no es un tipo de munición)
enum { BA_NONE, BA_WIND, BA_RUSH, BA_SLAM, BA_CALL };   // ataques del jefe: aviso de embestida, embestida, golpe al suelo, llamado
#define BOSS_K 1.6f          // La Bestia se dibuja al 160 %
#define BOSS_NAME "La Bestia del charco"
#define BOSS2_NAME "Cabeza de Hierro"
// [armas2] minijefes sueltos: nombre, tipo base (su IA), vida, blindaje, escala y tinte
typedef struct { const char *name; short base; float hp, armor, scale; u32 tint; } MiniDef;
static const MiniDef MJ[4] = {
    {"Capitán Chamusco, bombero", E_BIG, 420, 0.60f, 1.30f, RGBA(255, 168, 96, 255)},
    {"Teniente Garrote", E_BAN, 300, 0.40f, 1.15f, RGBA(255, 214, 120, 255)},
    {"Alfa Colmillo", E_WOLF, 280, 0.20f, 1.65f, RGBA(255, 236, 200, 255)},
    {"El Brillante", E_MAR, 340, 0.30f, 1.30f, RGBA(170, 255, 120, 255)},
};
static int mj_update(Enemy *e, int self, float dt); static void mj_loot(Enemy *e); static void mj_place(void);

static int is_boss(const Enemy *e) { return e->type == E_BOSS || e->type == E_BOSS2 || e->type >= E_VIUDA; }   // [jefes2] incluye a los tres nuevos
static float hit_r(const Enemy *e) { return is_boss(e) ? 13 : e->type == E_ARM ? 8 : 7; }   // a cuánto de sus pies le pega una bala
static int is_human(const Enemy *e) { return e->type == E_BAN || e->type == E_GRD || e->type == E_SNP; }
static int fac_of(int type) { return type == E_BAN || type == E_SNP ? F_BAND : type == E_GRD ? F_KOLOS : F_CREA; }
// El guardia de Kolos que sigue de amigo: no se mueve de su puesto
static int fr_fac(int fac) { return fac == F_KOLOS || fac == F_FARO; }   // amigos del jugador mientras no les dispare: Kolos y los Faroleros
static int is_post(const Enemy *e) { return e->type == E_GRD && fr_fac(e->fac); }

typedef struct { float x, y, vx, vy, life, dmg; int mine, on, kind, fac, pierce, hit; } Bullet;   // kind: tipo de munición (B_); fac: bando de quien dispara (si no es tuya)
typedef struct { float x, y, vx, vy, life, max, size; u32 col; } Part;
typedef struct { float x, y, ang, scale; int spr; } Decal;
typedef struct { float x, y; int ceil, flick; } Lamp;
typedef struct { float x, y; int spr; } Decor;
typedef struct { float x, y; int item, n, on; } Pick;   // algo tirado en el suelo: qué objeto (ITEM_) y cuántos
typedef struct { float x, y, life; u32 col; char s[48]; } Pop;
typedef struct { float x, y, life; } Pud;   // charco de ácido que deja el Escupidor
static Pud g_pud[10];
// Objetos de mano (ver hand_use): lo que va por el aire, las trampas puestas, los charcos de fuego y los destellos de explosión
typedef struct { float x, y, vx, vy, t, tt, fuse; int kind, on; } Thrown;   // kind: H_; t = lo que le falta de vuelo; fuse = mecha de la granada ya en el suelo
typedef struct { float x, y, t; int kind, on, e; } Trap;                  // on: 1 = puesta, 2 = el cepo ya cerró; t: lo que falta para armarse (mina)
typedef struct { float x, y, life, tick; } Fire;
typedef struct { float x, y, t; } Boom;
static Thrown g_thr[6];
static Trap g_trp[12];
static Fire g_fir[12];
static Boom g_boom[4];
enum { H_NUT, H_GREN, H_MOLO, H_MINE, H_TRAP, H_N };
static const short HAND_ITEM[H_N] = {ITEM_NUT, ITEM_GREN, ITEM_MOLO, ITEM_MINE, ITEM_TRAP};
static int g_hand;           // el objeto de mano elegido (H_)
static float g_pstun;        // aturdimiento del jugador (s): no se mueve ni dispara
static float g_det_t;        // reloj del pitido del detector
static int g_sneak;          // caminas con el stick a medias: casi no haces ruido y los que no te han visto te notan de mucho más cerca
static float g_pud_tick;
static float g_swing_t, g_swing_dur = 0.2f, g_swing_a, g_swing_arc = 1.5f, g_gun_off;   // el golpe cuerpo a cuerpo en curso: lo que le queda, cuánto dura, hacia dónde va y su arco; y cuánto gira el arma en la mano al dibujarla
#ifdef PEPEL_DEBUG
static float g_dbg_hold;   // pruebas: segundos que queda "mantenido" el botón de disparo
static float g_dbg_walk, g_dbg_wx, g_dbg_wy;   // pruebas: caminar con el stick durante unos segundos
static int g_dbg_walls, g_dbg_swhits; static float g_dbg_wsp;   // pruebas: choques de empujados contra muros (y su velocidad) y golpes que dieron
#endif

#define MAXE 128   // [mapas]
#define MAXB 64
#define MAXP 160
#define MAXD 200
#define MAXK 40
#define MAXPOP 8

static int g_state = GS_TITLE;
static float g_time, g_state_t, g_play_t;
static Player P;
static WeaponState g_w[W_COUNT];
static int g_frep, g_fjob, g_fjn, g_fdone, g_notes, g_note_new, g_ending;   // Faroleros: reputación, trabajo activo (1 a 3), su avance, trabajos hechos (bits); notas halladas; aviso de nota nueva; final elegido
static int g_rep;                 // reputación en Estación Kolos (la chatarra es un objeto más: g_bag[ITEM_SCRAP])
static int g_inv_sel, g_inv_top;  // mochila: casillero elegido (su lugar en la lista) y primera fila a la vista
static int g_inv_focus, g_inv_eq; // mochila: el cursor está en el panel EQUIPO (1) y en cuál de sus ranuras
static int g_rest;      // descanso junto al fuego: 0 no, 1 descansa, 2 descansa y recupera vida
static int g_slept;     // al dormir: ya pasó el tiempo (se aplica una vez, a pantalla negra)
static Enemy g_e[MAXE];
static int g_ne;
static Bullet g_b[MAXB];
static Part g_p[MAXP];
static Decal g_d[MAXD];
static int g_nd, g_dnext;
static Pick g_k[MAXK];
static Pop g_pop[MAXPOP];
#define MAXFIRE 64   // [mapas]   // fuegos y faroles horneados
#define MAXLAMP 160   // [mapas]
#define MAXDEC 96   // [mapas]
static float g_fire[MAXFIRE][2];
static Lamp g_lamp[MAXLAMP];
static Decor g_dec[MAXDEC];
static int g_nfire, g_nlamp, g_ndec;
static char g_msg[96];
static float g_msg_t;
static float g_camx, g_camy, g_shake;
static float g_ox, g_oy;   // esquina de la camara, ya en pixeles enteros
static float g_storm_t, g_storm_left;
static int g_storm_on, g_storm_warned;
static float g_bomba_t;   // [bomba] segundos que quedan para la bomba (0 = parada)
static int g_kills, g_opened;

// ------------------------------------------------------------------ zonas
// Todas miden MW x MH, para reusar las grillas. La zona cargada vive en g_g, g_o, g_cr, g_e y g_k; al irte
// de ella se guarda su estado en g_zc (cajones abiertos, enemigos con su vida, lo caído al suelo y lo explorado)
// y al volver se arma de nuevo con el plano y se le pone encima lo guardado.
#define ZH 10   // [mapas] el segundo piso de una casa (se arma por semilla; no tiene memoria propia: ver g_hs_open)
#define ZM 11   // [urbe] el metro: estaciones unidas por túneles (zona transitoria, sin caché)
#define NZ 11   // 0 Estación Kolos (la base, zona segura), 1 la expedición, 2 el búnker (se arma en el juego), 3 a 8 los pisos de arriba de los edificios con escalera (3 por edificio, ver floor_gen)
// [bunker] Mi búnker: la zona ZB (9), su código está en bunker.inc. Ganchos cortos repartidos por este archivo
#define ZB 9
#define BK_W 64
#define BK_H 40
#define BK_NR 8       // salas del búnker (0 central, 1 dormitorio, 2 a 7 selladas)
#define BK_NBOX 12    // tope de cajas
#define O_BK_STAIR 90   // la escalera, la escotilla de la estación, las cajas y lo macizo (mesa, estante) del búnker
#define O_BK_HATCH 91
#define O_BK_BOX 92
#define O_BK_BLK 93
#define GS_BK 20        // el modo decorar
static int g_bk_open = -1;   // caja abierta en la pantalla del alijo (-1 = ninguna)
static void bk_gen(void); static void bk_post(void); static void bk_kolos_hatch(void);
static void bk_save(void *sv); static void bk_load(const void *sv); static void bk_reset(void); static void bk_close(void);
static int bk_cross(void); static int bk_hint(void); static float bk_amb(void);
static void bk_draw_obj(int tx, int ty, int o); static void bk_draw_flat(void); static void bk_arrive(float *x, float *y);
static int bk_deco_open(void); static void bk_deco_input(void); static void bk_deco_draw(void); static void bk_boss_drop(int type, float x, float y);
typedef struct { int vis, ncr, ne; Crate cr[MAXCR]; Enemy e[MAXE]; Pick k[MAXK]; unsigned char seen[MHMAX][MWMAX]; } ZoneCache;
static ZoneCache g_zc[NZ];
static int g_zone;                                  // zona cargada
static char g_zg[MHMAX][MWMAX + 1], g_zo[MHMAX][MWMAX + 1];     // plano de la zona cargada: suelo y objetos
static const char *const ZNAME[NZ] = {"Estación Kolos", "Expedición", "Búnker"};
// La zona 1 es la expedición en curso: se arma en el juego con una semilla nueva cada vez que se sale por el portón
enum { XT_TRAIN, XT_TOWN, XT_FOREST, XT_FACTORY, XT_GRAVE, XT_MINE,
       XT_PINE,   // [armas] bosque de pinos
       XT_CITY,   // [diseno] ciudad abandonada
       XT_HOSP,   // [mapas3] hospital
       XT_PORT,   // [mapas3] puerto
       XT_ROAD,   // [mapas3] autopista
       XT_NUC,    // [mapnuc] planta atómica
       XT_CURSED, // [mapnuc] tierra maldita
       XT_N };
static const char *const XTNAME[XT_N] = {"Patio de trenes", "Barrio en ruinas", "Bosque muerto", "Fábrica abandonada", "Cementerio", "Pueblo minero", [XT_PINE] = "Bosque de pinos", [XT_CITY] = "Ciudad abandonada", [XT_NUC] = "Planta atómica", [XT_CURSED] = "Tierra maldita"};   // [armas] [mapnuc]
static const char *m3_name(int t);   // [mapas3]
static const short *m3_pool(void);   // [mapas3]
enum { XF_HATCH = 1, XF_JOB = 2, XF_BEAST = 4, XF_BANDITS = 8, XF_MASKBODY = 16, XF_SAWED = 32, XF_SNOW = 64, XF_BOSS2 = 128, XF_DOG = 256, XF_FARO = 512 };   // lo que trae esta expedición
enum { XF_ZLAGO = 4096, XF_ZFUERTE = 8192, XF_ZVERT = 16384 };   // [jefes2] Lago helado, Fuerte y Vertedero químico
enum { XF_ZJEFE = 1024, XF_ZBASE = 2048 };   // [jefes] zona de jefe (Pantano / Base militar infectada)
#define JF_PERRO_ELEGIR 0   // [jefes] el perro ya no se lleva ni se deja: vive en la base
static const char *jefes_nombre(void);
static int jefes_ask(float dt);
static void jefes_update(float dt);
static void jefes_draw_world(void);
static void jefes_draw_ui(void);
static void jefes_zona(void);
static void jefes_carga(void);
typedef struct { u32 seed; int num, type, flags, danger; int ev0, ev1, evfound, evdone; } Exped;   // ev0 y ev1: los eventos al azar (EV_...; 0 = ninguno); evfound y evdone: bits por evento (lo descubierto y lo resuelto)
static Exped g_x;                       // la expedición en curso (todo lo que hace falta para armarla igual)
static void jefes_exped(Exped *X);   // [jefes]
static void jefes_muere(const Enemy *e);   // [jefes]
static void jz_reserve(int type, int n, float x, float y);   // [arregla]
static int jz_gen(void); static void jz_spawn(Enemy *e); static void jz_ai(Enemy *e, int self, float dt); static int jz_draw(const Enemy *e); static const char *jz_name(const Enemy *b, const char *def);   // [jefes2]
static void jz_muere(const Enemy *e); static void jz_update(float dt); static void jz_draw_world(void); static void jz_open_gate(void); static int jz_open;   // [jefes2]
static int g_j2done;   // [jefes2] jefes de Kolos ya cobrados (bit 0 Capataz, bit 1 Fermentado)
static int g_xtotal;                    // expediciones hechas
static int g_boss_dead;                 // La Bestia ya murió
static int g_hatch_tx = -1, g_hatch_ty = -1, g_ret_tx, g_ret_ty, g_ret2_tx, g_ret2_ty, g_ret3_tx = -1, g_ret3_ty = -1;   // [mapas] (el tercero, al norte o al sur) la escotilla (si hay) y los dos puntos de retirada de la expedición cargada (la entrada y el del extremo este)
static int g_zexp;                      // el fundido en curso es una salida de expedición
static int g_zc2_fl;   // [arregla] a qué piso del Pozo pertenece lo guardado en g_zc[2] (1 = el piso 1)
static void pozo_clear(void);   // [arregla]
static int g_pz, g_pz_lift, g_pz_inf, g_pz_fresh;   // [pozo] piso del Pozo (0 = fuera; 2 o más = Pozo)
static void pozo_decor(void); static void pozo_post(int z); static void pozo_stash(void); static int pozo_restore(void); static const char *pozo_nombre(void); static void pozo_switch(void); static void pozo_arrive(float *x, float *y); static void pozo_after(void); static int pozo_cross(int aim); static void pozo_muere(const Enemy *e); static void pozo_danger(Exped *X); static void pozo_extra_loot(const Crate *c, float x, float *y); static float pozo_dmg(void); static int pozo_revive(void); static void pozo_update(float dt); static void pozo_hud(void);   // [pozo]
// [artef] artefactos y piezas únicas (src/artef.inc)
enum { A_LATIDO, A_ASCUA, A_PLOMO, A_LAGRIMA, A_PULMON, A_OJO, A_CORAZON, A_COLMILLO, A_IMAN, A_SEMILLA, A_N };
#define ART_ITEM(i) ((i) < 6 ? ITEM_ART0 + (i) : ITEM_ART6 + (i) - 6)
static int art_idx(int item) { for (int i = 0; i < A_N; i++) if (ART_ITEM(i) == item) return i; return -1; }
#define RAD_DANO 50   // desde esta radiación te hiere
static int g_uniq;   // [artef] piezas únicas ya entregadas (bits 0 a 7)
static int art_on(int i); static float hp_max(void); static float art_dmg(void); static float art_taken(void); static float art_speed(void); static float art_heal(void); static float art_thirst(void); static float art_hunger(void); static float art_see(void);
float artef_kg(void); static int artef_pick(unsigned r); static int artef_com(unsigned r); static void artef_tick(float dt); static void artef_muere(const Enemy *e); static void artef_update(float dt); static void artef_light(void); static void artef_draw_world(void); static void artef_hud(void); static int artef_info(int item); static int artef_ws_input(void); static void artef_ws_draw(void); static u32 artef_pcol(int pi, u32 normal); static int uniq_part_on(int item); static int g_uq_open;
static const char *dz_zname(void);   // [diseno]
static const char *zname(int z)
{
    static char pb[40];
    if (z == 2 && g_pz >= 2) return pozo_nombre();   // [pozo]
    if (z == 1 && (g_x.flags & XF_ZJEFE)) return jefes_nombre();   // [jefes]
    if (z == ZB) return "Mi búnker";   // [bunker]
    if (z == ZH) return "Piso 2";   // [mapas]
    if (z == ZM) return "Metro";   // [urbe]
    if (z >= 3) { snprintf(pb, sizeof(pb), "Piso %d", (z - 3) % 3 + 2); return pb; }   // los pisos de arriba: 2, 3 y 4 (el 1 es la planta baja)
    if (z == 1 && g_zone == 1) return dz_zname();   // [diseno] tipo, nieve y ambiente
    if (z == 1 && (g_x.flags & XF_SNOW)) { snprintf(pb, sizeof(pb), "%s, nevado", g_x.type >= XT_HOSP ? m3_name(g_x.type) : XTNAME[g_x.type]); return pb; }   // [mapas3]
    return z == 1 ? (g_x.type >= XT_HOSP ? m3_name(g_x.type) : XTNAME[g_x.type]) : ZNAME[z];   // [mapas3]
}
static float g_cold;                    // frío del jugador (0 a 100): sube a la intemperie en una expedición nevada
typedef struct { float x, y, t; } Foot;   // una huella en la nieve: se borra sola
#define MAXFOOT 72
static Foot g_foot[MAXFOOT];
static int g_nfoot;
static int snowy(void) { return g_zone == 1 && (g_x.flags & XF_SNOW); }   // la zona cargada es una expedición nevada
// Paradas de viaje rápido: Estación Kolos (zona 0) y la Parada del este (zona 1, con el Chofer). Una se abre al visitarla por primera vez
typedef struct { const char *name; int zone, tx, ty; } StopDef;
#define NSTOP 2
#define TRAVEL_COST 15   // chatarra por viaje
static const StopDef STOPS[NSTOP] = {{"Estación Kolos", 0, 4, 19}, {"Parada del este", 1, Z1_STOP_X, Z1_STOP_Y}};
static int g_stop_seen[NSTOP];     // parada descubierta
static int g_chof_tx, g_chof_ty, g_has_chof;   // el Chofer de la zona cargada
static int g_trav_sel;             // destino elegido en la lista de viaje
static float g_ban_t;                               // cuánto falta para que se apague el nombre de la zona en pantalla

// Los valores efectivos de un arma tuya, con sus piezas y su munición; y sus balas por cargador
static WeaponStats w_stats(int wi) { return weapon_stats(wi, g_w[wi].part, g_w[wi].load); }
static int w_mag(int wi) { return w_stats(wi).mag; }

// Trabajos de Estación Kolos: se toman y se entregan en el portón, de a uno
typedef struct { const char *name, *desc; int goal, pay, rep; } JobDef;
#define JOB_N 10
static const JobDef JOBS[JOB_N] = {
    {"Repuestos", "Trae la caja de repuestos del edificio más lejano.", 1, 120, 2},
    {"Limpieza", "Mata 6 zombis.", 6, 60, 1},
    {"Rebusca", "Abre 5 cajones o cuerpos.", 5, 40, 1},
    {"Ajuste de cuentas", "Mata 4 bandidos.", 4, 90, 2},
    {"Caza mayor", "Mata a La Bestia, junto al charco del bosque.", 1, 300, 4},
    {"Medicinas del búnker", "Trae la caja de medicinas del búnker (escotilla).", 1, 200, 3},
    {"El origen de la Ceniza", "Trae el Registro del incendio, del fondo del búnker. Rango 3.", 1, 400, 4},
    {"Cabeza de Hierro", "Mata a Cabeza de Hierro, en la base militar infectada.", 1, 350, 4},   // [jefes]
    {"El Capataz", "Mata al Capataz de los bandidos, en su Fuerte. Rango 2.", 1, 420, 4},   // [jefes2]
    {"El Fermentado", "Mata al Fermentado, en el Vertedero químico. Rango 3.", 1, 450, 5},   // [jefes2]
};
// trabajo activo (0 = ninguno, 1..3), su avance, si los repuestos ya se entregaron y la fila elegida en el portón
static int g_job, g_job_n, g_job_done1, g_job_done5, g_kol_sel, g_kol_top;   // done1: repuestos entregados; done5: Caza mayor cobrada (una sola vez)

// Trabajos del búnker: la caja de medicinas y el Registro del incendio que llevas, si ya se entregaron, y el final
static int g_has_meds, g_has_reg, g_job_done6, g_job_done7, g_fin_seen;
static int g_jobs_done, g_scrap_gain;   // estadísticas del final: trabajos cumplidos y chatarra ganada
static u32 g_bseed = 1, g_bpend, g_brs;   // búnker: semilla de su distribución, la que toca tras la próxima tormenta y el generador
static int g_bstale, g_bun_ex, g_bun_ey;  // la tormenta terminó con el jugador adentro (se rehace al salir) y casilla de la escalera

static int boss_dead(void) { return g_boss_dead; }   // La Bestia solo sale en las expediciones de Caza mayor
static int job_prog(void) { return g_job == 1 ? (P.has_job ? 1 : 0) : g_job == 5 ? boss_dead() : g_job == 6 ? g_has_meds : g_job == 7 ? g_has_reg : g_job_n; }
static int job_ready(void) { return g_job && job_prog() >= JOBS[g_job - 1].goal; }
static void job_text(char *s, int n)
{
    if (!g_job) snprintf(s, n, "Pide trabajo al comerciante");
    else snprintf(s, n, "%s %d/%d%s", JOBS[g_job - 1].name, job_prog(), JOBS[g_job - 1].goal, job_ready() ? " - entrega con el comerciante" : "");
}

// Rango en Estación Kolos según la reputación: abre artículos del comerciante y mejora lo que paga al vender
static const int RANK_REP[] = {0, 2, 5, 9};   // reputación mínima de cada rango
#define RANK_MAX ((int)(sizeof(RANK_REP) / sizeof(RANK_REP[0])) - 1)
static int rank(void)
{
    int r = 0;
    while (r < RANK_MAX && g_rep >= RANK_REP[r + 1]) r++;
    return r;
}
static void rank_text(char *s, int n)   // "Rango 1 - reputación 3/5": la que hay y la que pide el rango siguiente
{
    int r = rank();
    if (r < RANK_MAX) snprintf(s, n, "Rango %d - reputación %d/%d", r, g_rep, RANK_REP[r + 1]);
    else snprintf(s, n, "Rango %d - reputación %d", r, g_rep);
}
static int g_kol_tab;   // pestaña abierta en Estación Kolos
// Con quién se habla en la base: cada uno abre sus pestañas (máscara de KolosTab.who)
enum { KW_COM = 1, KW_SCAV = 2, KW_STASH = 4 };
static int g_kol_who;
static int g_inv_tab;   // pestaña de la mochila: 0 OBJETOS, 1 FABRICAR, 2 ARMAS
static int g_fab_all = 1;   // 1: todas las recetas (el chatarrero); 0: solo las que no necesitan banco (la mochila)
static void kolos_open(int who);
static void fab_input(void);
static void fab_draw(void);
static void ws_input(void);
static void ws_draw(void);
// Taller: arma y ranura elegidas; con la lista de piezas abierta (g_ws_list), la opción elegida
static int g_ws_w, g_ws_slot, g_ws_list, g_ws_opt;

// Mejoras de la base (tabla UPS, pestaña MEJORAS del chatarrero). g_up[i] = 1 si ya está hecha. Se guardan con la partida
enum { UP_STASH1, UP_STASH2, UP_BED, UP_CLINIC, UP_TOWER, UP_GARDEN, UP_BENCH, UP_N };
#define UP_MAX 8   // lugar en la partida guardada
_Static_assert(UP_N <= UP_MAX, "UP_MAX quedó chico: subirlo (y SAVE_VER)");
static int g_up[UP_N];
static int ups_done(void) { int n = 0; for (int i = 0; i < UP_N; i++) n += g_up[i] != 0; return n; }   // cuántas mejoras tiene la base
static int g_clinic_used;   // la enfermería ya se usó en esta vuelta de expedición (se renueva al volver a la base)
static int g_xs_kills, g_xs_open;   // bajas y cajones del contador al salir de expedición (para el resumen del regreso)
static float g_xs_t;                // y el reloj de juego a esa hora
static int g_ret_pend, g_ret_time, g_ret_kills, g_ret_open, g_ret_food, g_ret_water;   // resumen del regreso: pendiente de mostrar, y sus cifras

// El alijo de Estación Kolos (g_stash): los mismos objetos de la mochila, con tope (stash_cap). La chatarra no se guarda ahí
static int g_stash_sel, g_stash_top, g_stash_col;   // fila elegida, primera fila a la vista y columna (0 = mochila, 1 = alijo)

// Tope del alijo: cada unidad de objeto cuenta 1 y la munición cuenta por paquete. 60 al empezar; las mejoras lo suben a 150 y a 400
static int stash_cap(void) { if (g_bk_open >= 0) return 60; return g_up[UP_STASH2] ? 400 : g_up[UP_STASH1] ? 150 : 60; }
static int stash_slots(int item, int n) { int p = ITEMS[item].pack > 0 ? ITEMS[item].pack : 1; return (n + p - 1) / p; }   // lo que ocupan n unidades
static int stash_used(void)
{
    int u = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (!(ITEMS[i].flags & IF_NOSTASH)) u += stash_slots(i, g_stash[i]);
    return u;
}
// Mete en el alijo hasta n unidades de item si caben (hasta donde llegue el tope) y devuelve cuántas metió
static int stash_add(int item, int n)
{
    int put = 0;
    while (put < n && stash_used() - stash_slots(item, g_stash[item]) + stash_slots(item, g_stash[item] + 1) <= stash_cap()) { g_stash[item]++; put++; }
    return put;
}

// Comerciante de Estación Kolos: una fila por artículo. Para sumar uno (piezas de armas, munición especial,
// armadura) basta agregar su fila; si entrega un objeto nuevo, también su ITEM_ y su fila en ITEMS.
typedef struct {
    const char *name;       // como se lee en la lista
    short spr, item, qty;   // icono, qué objeto (ITEM_) entrega y cuántos van en cada unidad
    short price, rank;      // precio en chatarra y rango que lo desbloquea
    short stock;            // existencias: se reponen cada vez que termina una tormenta de Ceniza
    short gun;              // 0 = entrega un objeto; GUN(W_...) = vende esa arma (se compra una sola vez y no se revende)
} ShopDef;
#define GUN(w) ((w) + 1)
static const ShopDef SHOP[] = {
    {"Balas 9x18 x8", S_IT_9MM, ITEM_9MM, 8, 20, 0, 5},
    {"Venda", S_IT_BAND, ITEM_BAND, 1, 15, 0, 5},
    {"Conserva", S_IT_FOOD, ITEM_FOOD, 1, 12, 0, 5},
    {"Agua", S_IT_WATER, ITEM_WATER, 1, 10, 0, 5},
    {"Kit de limpieza", S_IT_CLEAN, ITEM_CLEAN, 1, 35, 0, 3},
    {"Cartuchos cal. 12 x4", S_IT_12G, ITEM_12G, 4, 25, 1, 5},
    {"9x18 expansivas x8", S_IT_9MM_HP, ITEM_9MM_HP, 8, 30, 1, 3},
    {"7,62 expansivas x10", S_IT_762_HP, ITEM_762_HP, 10, 56, 1, 3},
    {"Botiquín", S_IT_MED, ITEM_MED, 1, 45, 1, 3},
    {"Cargador ampliado", S_IT_XMAG, ITEM_XMAG, 1, 90, 1, 1},
    {"Chaqueta reforzada", S_IT_JACKET, ITEM_JACKET, 1, 120, 1, 1},
    {"Máscara de gas", S_IT_MASK, ITEM_MASK, 1, 150, 1, 1},
    {"Filtro", S_IT_FILTER, ITEM_FILTER, 1, 30, 1, 3},
    {"Kit de costura", S_IT_SEW, ITEM_SEW, 1, 40, 1, 3},
    {"Antirradiación", S_IT_ARAD, ITEM_ARAD, 1, 35, 2, 5},
    {"Fusil AK", S_W_AK, 0, 1, 416, 2, 1, GUN(W_AK)},
    {"Hacha", S_W_AXE, 0, 1, 70, 1, 1, GUN(W_AXE)},
    {"Tubo de acero", S_W_PIPE, 0, 1, 55, 1, 1, GUN(W_PIPE)},
    {"Revólver", S_W_REVOLVER, 0, 1, 304, 2, 1, GUN(W_REVOLVER)},
    {"Escopeta de corredera", S_W_PUMP, 0, 1, 432, 3, 1, GUN(W_PUMP)},
    {"Mazo", S_W_MAUL, 0, 1, 210, 3, 1, GUN(W_MAUL)},
    {"Subfusil", S_W_SMG, 0, 1, 512, 3, 1, GUN(W_SMG)},
    {"Pistola Víbora", S_W_A2_VIBORA, 0, 1, 330, 2, 1, GUN(W_VIBORA)},   // [armas2]
    {"Chacal pesada", S_W_A2_CHACAL, 0, 1, 520, 3, 1, GUN(W_CHACAL)},   // [armas2]
    {"Subfusil Zumbador", S_W_A2_ZUMBA, 0, 1, 560, 3, 1, GUN(W_ZUMBA)},   // [armas2]
    {"Escopeta Cuervo", S_W_A2_CUERVO, 0, 1, 600, 3, 1, GUN(W_CUERVO)},   // [armas2]
    {"Fusil Halcón", S_W_A2_HALCON, 0, 1, 700, 3, 1, GUN(W_HALCON)},   // [armas2]
    {"Fusil de cerrojo", S_W_BOLT, 0, 1, 640, 3, 1, GUN(W_BOLT)},
    {"Balas 7,62 x10", S_IT_762, ITEM_762, 10, 40, 2, 5},
    {"9x18 perforantes x8", S_IT_9MM_AP, ITEM_9MM_AP, 8, 34, 2, 3},
    {"7,62 perforantes x10", S_IT_762_AP, ITEM_762_AP, 10, 62, 2, 3},
    {"Cal. 12 bala única x4", S_IT_12G_SLUG, ITEM_12G_SLUG, 4, 38, 2, 3},
    {"Compensador", S_IT_COMP, ITEM_COMP, 1, 110, 2, 1},
    {"Culata", S_IT_STOCK, ITEM_STOCK, 1, 100, 2, 1},
    {"Mira réflex", S_IT_REFLEX, ITEM_REFLEX, 1, 140, 2, 1},
    {"Chaleco ligero", S_IT_VEST, ITEM_VEST, 1, 240, 2, 1},
    {"Silenciador", S_IT_SIL, ITEM_SIL, 1, 180, 3, 1},
    {"Chaleco pesado", S_IT_HVEST, ITEM_HVEST, 1, 420, 3, 1},
    {"Tuercas x5", S_IT_NUT, ITEM_NUT, 5, 10, 0, 6},
    {"Granada", S_IT_GREN, ITEM_GREN, 1, 45, 1, 3},
    {"Cóctel incendiario", S_IT_MOLO, ITEM_MOLO, 1, 30, 1, 3},
    {"Mina", S_IT_MINE, ITEM_MINE, 1, 60, 2, 2},
    {"Cepo", S_IT_TRAP, ITEM_TRAP, 1, 55, 2, 2},
    {"Detector de anomalías", S_IT_DET, ITEM_DET, 1, 160, 2, 1},
    // los artefactos no se compran (existencias 0): se encuentran en los campos de anomalías y aquí se venden, caros
    {"Latido ámbar", S_IT_ART0, ITEM_ART0, 1, 520, 0, 0},
    {"Ascua mansa", S_IT_ART1, ITEM_ART1, 1, 380, 0, 0},
    {"Piel de plomo", S_IT_ART2, ITEM_ART2, 1, 560, 0, 0},
    {"Lágrima cuajada", S_IT_ART3, ITEM_ART3, 1, 420, 0, 0},
    {"Pulmón de vidrio", S_IT_ART4, ITEM_ART4, 1, 420, 0, 0},
    {"Ojo de ceniza", S_IT_ART5, ITEM_ART5, 1, 480, 0, 0},
    {"Caja", S_IT_MECH, ITEM_BK_BOX, 1, 60, 0, 3},   // [bunker]
    {"Lámpara", S_IT_NUT, ITEM_BKD_LAMP, 1, 25, 0, 3},   // [bunker]
    {"Alfombra", S_IT_RAG, ITEM_BKD_RUG, 1, 20, 0, 3},   // [bunker]
    {"Estante", S_IT_MECH, ITEM_BKD_SHELF, 1, 45, 0, 2},   // [bunker]
    {"Afiche", S_IT_RAG, ITEM_BKD_POSTER, 1, 10, 0, 3},   // [bunker]
    {"Planta", S_IT_CHEM, ITEM_BKD_PLANT, 1, 15, 0, 3},   // [bunker]
    {"Radio", S_IT_DET, ITEM_BKD_RADIO, 1, 40, 0, 2},   // [bunker]
    {"Mesa", S_IT_MECH, ITEM_BKD_TABLE, 1, 35, 0, 2},   // [bunker]
    {"Silla", S_IT_RAG, ITEM_BKD_CHAIR, 1, 18, 0, 3},   // [bunker]
    {"Kit de reparación", S_IT_CLEAN, ITEM_REPAIR, 1, 60, 0, 3},   // [armas]
    {"Cañón largo", S_IT_SIL, ITEM_LONGB, 1, 130, 2, 1},   // [armas]
    {"Freno de boca", S_IT_COMP, ITEM_BRAKE, 1, 100, 1, 1},   // [armas]
    {"Cargador tambor", S_IT_XMAG, ITEM_DRUM, 1, 200, 3, 1},   // [armas]
    {"Linterna táctica", S_IT_NUT, ITEM_LIGHT, 1, 70, 1, 1},   // [armas]
    {"Láser", S_IT_DET, ITEM_LASER, 1, 150, 2, 1},   // [armas]
    {"Bípode", S_IT_MECH, ITEM_BIPOD, 1, 120, 2, 1},   // [armas]
    {"Bayoneta", S_IT_TRAP, ITEM_BAYO, 1, 80, 1, 1},   // [armas]
    {"Mira telescópica", S_IT_REFLEX, ITEM_SCOPE, 1, 260, 3, 1},   // [armas]
    {"Mochila de lona", S_IT_VEST, ITEM_PACK1, 1, 90, 1, 1},   // [peso]
    {"Mochila de expedición", S_IT_HVEST, ITEM_PACK2, 1, 240, 2, 1},   // [peso]
    DURO_SHOP_ROWS   // [duro]
    {"Corazón de brasa", S_IT_ART1, ITEM_ART6, 1, 700, 0, 0},   // [artef]
    {"Colmillo rojo", S_IT_ART4, ITEM_ART7, 1, 700, 0, 0},   // [artef]
    {"Imán negro", S_IT_ART2, ITEM_ART8, 1, 450, 0, 0},   // [artef]
    {"Semilla hueca", S_IT_ART5, ITEM_ART9, 1, 400, 0, 0},   // [artef]
    {"Fusil Centinela", S_W_A2_CENTINELA, 0, 1, 780, 3, 1, GUN(W_CENTINELA)},   // [armas2]
};
#define SHOP_N ((int)(sizeof(SHOP) / sizeof(SHOP[0])))
#define SHOP_MAX 96                       // lugar en la partida guardada: alcanza para los que se sumen
_Static_assert(SHOP_N <= SHOP_MAX, "SHOP_MAX quedó chico: subirlo (y SAVE_VER)");
static unsigned char g_stock[SHOP_MAX];   // lo que le queda al comerciante de cada artículo
static int g_shop_sel, g_shop_top;        // fila elegida y primera fila a la vista

static int shop_restock(void)   // repone todo; devuelve 1 si faltaba algo
{
    int any = 0;
    for (int i = 0; i < SHOP_N; i++) {
        if (g_stock[i] != SHOP[i].stock) any = 1;
        g_stock[i] = SHOP[i].stock;
    }
    return any;
}
// Lo que paga el comerciante por una unidad: la mitad del precio, más un 5 % por cada rango.
// Por una armadura gastada paga menos, en proporción a su estado
static int shop_sell_price(const ShopDef *d)
{
    int pay = d->price * (50 + 5 * rank()) / 100, gi = d->gun ? -1 : gear_of(d->item);
    if (gi >= 0 && GEAR[gi].slot == EQ_ARMOR && gear_owned(gi)) pay = (int)(pay * g_gcond[gi] / 100);
    return pay;
}

static unsigned char g_seen[MHMAX][MWMAX];   // lo explorado del mapa (1 = ya se vio)
static int g_seen_tx, g_seen_ty;       // casilla del jugador la ultima vez que se descubrio
static int g_was_in;                   // el jugador estaba bajo techo en el cuadro anterior
// Modos de juego: se eligen al empezar una partida nueva y se guardan con ella
enum { MODE_SURV, MODE_EXPL, MODE_IRON, MODE_COUNT };
static const char *const MODE_NAME[MODE_COUNT] = {"Superviviente", "Explorador", "Hierro"};
static const char *const MODE_DESC[MODE_COUNT][2] = {
    {"Difícil: menos munición, medicinas y comida; hambre y sed +25 %;", "curarse tarda y el sangrado quita más. Al morir vuelves al último refugio."},
    {"Accesible: enemigos 30 % más débiles; hambre y sed 30 % más lentas;", "trabajos pagan 25 % más. Al morir vuelves al último refugio."},
    {"Como Superviviente, con muerte permanente:", "al morir se borra la partida. No hay vuelta atrás."},
};
static int g_mode = MODE_SURV;
static int g_title_page, g_menu_sel, g_mode_sel, g_dead_sel;   // título: página (0 menú, 1 modo), cursor del menú, del modo y de la muerte
static int g_has_save;                 // hay una partida guardada a la que volver
// Opciones: se guardan aparte (soinnka.cfg) y se aplican al momento. Cada una es un índice de su tabla de valores.
enum { OPT_DARK, OPT_SHAKE, OPT_RUN, OPT_HELP, OPT_DEAD, OPT_VOL, OPT_MUS, OPT_COUNT };   // [armas2] OPT_MUS al final: la música
static const char *const OPT_LABEL[OPT_COUNT] = {"Oscuridad", "Temblor de cámara", "Correr", "Ayuda de botones", "Zona muerta del stick", "Efectos", "Música"};
static const char *const OPT_VAL[OPT_COUNT][3] = {
    {"Clara", "Normal", "Cerrada"}, {"0 %", "50 %", "100 %"}, {"Mantener", "Alternar", ""},
    {"Sí", "No", ""}, {"Chica", "Normal", "Grande"}, {"", "", ""}, {"", "", ""},
};
static const int OPT_N[OPT_COUNT] = {3, 3, 2, 2, 3, 11, 11};
static const float OPT_AMBIENT[3] = {0.32f, 0.20f, 0.11f};   // Normal = AMBIENT
static const float OPT_SHK[3] = {0, 0.5f, 1};
static const float OPT_DZ[3] = {0.15f, 0.25f, 0.38f};        // Normal = el valor de siempre
static int g_opt[OPT_COUNT] = {1, 2, 0, 0, 1, 10, 7};
static int g_opt_vol = 10;     // volumen de efectos (0 a 10): el sonido se agrega después
static int g_opt_sel, g_opt_ret = GS_TITLE, g_run_on;
static char g_cfg[160];
#define CFG_MAGIC 0x4F4E4B53u
static void opt_apply(void) { g_opt_vol = g_opt[OPT_VOL]; snd_set_master(g_opt_vol); snd_music_master(g_opt[OPT_MUS]); }
#ifdef PEPEL_DEBUG
static void dbg_scenario(void);
static int g_dbg_sneak;   // pruebas: fuerza el andar sigiloso
static int g_dbg_boomfx;  // pruebas: mantiene un destello de explosión delante del jugador (para mirarlo)
static int g_dbg_god;   // pruebas: modo sin daño (SELECT en la pausa, o el archivo dbg_all junto al EBOOT al empezar partida)
#endif
static char g_sav[160], g_tmp[160];    // rutas de soinnka.sav y soinnka.tmp
static struct { u32 held, pressed; float ax, ay; } in;

static void msg(const char *s)
{
    strncpy(g_msg, s, sizeof(g_msg) - 1);
    g_msg[sizeof(g_msg) - 1] = 0;
    g_msg_t = 4.0f;
}

#define DURO_PART 1
#include "duro.inc"   // [duro] dificultad, objetos nuevos y suministros del búnker
#undef DURO_PART

static void pop(float x, float y, const char *s, u32 col)
{
    Pop *best = &g_pop[0];
    for (int i = 0; i < MAXPOP; i++) {
        if (g_pop[i].life <= 0) { best = &g_pop[i]; break; }
        if (g_pop[i].life < best->life) best = &g_pop[i];
    }
    best->x = x; best->y = y; best->life = 1.8f; best->col = col;
    strncpy(best->s, s, sizeof(best->s) - 1);
    best->s[sizeof(best->s) - 1] = 0;
}

static void part(float x, float y, float vx, float vy, float life, u32 col, float size)
{
    static int next;
    Part *p = &g_p[next];
    next = (next + 1) % MAXP;
    *p = (Part){x, y, vx, vy, life, life, size, col};
}

static void decal(float x, float y, int spr, float scale)
{
    g_d[g_dnext] = (Decal){x, y, frange(0, 2 * PI), scale, spr};
    g_dnext = (g_dnext + 1) % MAXD;
    if (g_nd < MAXD) g_nd++;
}

static void blood(float x, float y, int n)
{
    for (int i = 0; i < n; i++) {
        float a = frange(0, 2 * PI), s = frange(15, 70);
        part(x, y - 6, cosf(a) * s, sinf(a) * s, frange(0.2f, 0.5f), RGBA(150, 24, 26, 255), frand() < 0.4f ? 2 : 1);
    }
    decal(x + frange(-4, 4), y + frange(-4, 4), S_BLOOD0 + (int)(rnd() % 3), 1);
}

static void sparks_n(float x, float y, int n, float k)   // n chispas, k veces más rápidas y duraderas que las de un muro
{
    for (int i = 0; i < n; i++) {
        float a = frange(0, 2 * PI), s = frange(20, 60) * k;
        part(x, y, cosf(a) * s, sinf(a) * s, frange(0.1f, 0.25f) * k, RGBA(240, 210, 140, 255), 1);
    }
    part(x, y, 0, -6, 0.5f, RGBA(120, 116, 110, 150), 2);
}
static void sparks(float x, float y) { sparks_n(x, y, 4, 1); }

// Deja algo en el suelo. En Pick.on: 0 = libre, 1 = se recoge al pasar, 2 = ya se avisó que no cabe en la mochila,
// 3 = lo botó el jugador (no se recoge hasta que se aleje)
static Pick *drop(float x, float y, int item, int n)
{
    for (int i = 0; i < MAXK; i++)
        if (!g_k[i].on) { g_k[i] = (Pick){x, y, item, n, 1}; return &g_k[i]; }
    return NULL;
}

// ------------------------------------------------------------------ luz
// De noche casi todo esta a oscuras. Alumbran la linterna del jugador, sus fogonazos, los barriles
// con fuego y los faroles. Los zombis no llevan luz: solo se les ven los ojos. Los muros hacen sombra.
// La luz de lo que no se mueve (fuego y faroles) se calcula una sola vez al cargar la zona, en una
// grilla de 8 px; por cuadro solo se calculan la linterna y los fogonazos.
#define LG 8
#define LMW (MWMAX * TS / LG + 1)   // capacidad de la luz horneada
#define LMH (MHMAX * TS / LG + 1)
#define AMBIENT 0.20f   // la penumbra general (0 = negro total)
typedef struct { float x, y, r2, power, cdir, sdir, c0, c1; int cone; } Light;
#define MAXL 24
static Light g_l[MAXL];
static int g_nl;
static unsigned char g_lm[3][LMH][LMW];   // luz fija: 0 fuego, 1 faroles, 2 faroles que parpadean

// half > 0: un cono hacia dir (linterna); half <= 0: hacia todos lados
static Light mk_light(float x, float y, float r, float power, float dir, float half)
{
    Light l = {x, y, r * r, power, cosf(dir), sinf(dir), 0, 0, half > 0};
    if (half > 0) { l.c0 = cosf(half); l.c1 = cosf(half - 0.2f); }
    return l;
}

// Como los(), pero mas liviana: se usa miles de veces por cuadro
static int los_light(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)((fabsf(dx) + fabsf(dy)) * 0.125f) + 1;
    dx /= n;
    dy /= n;
    for (int i = 1; i < n; i++) {
        x0 += dx;
        y0 += dy;
        int tx = (int)x0 >> 4, ty = (int)y0 >> 4;
        if ((unsigned)tx >= MW || (unsigned)ty >= MH) return 0;
        int o = g_o[ty][tx];
        if (o != O_NONE && o != O_FENCE && o != O_SAND && o < O_FBARREL) return 0;
    }
    return 1;
}

// Cuanta luz de una fuente llega a un punto
static float light_one(const Light *l, float x, float y)
{
    float dx = x - l->x, dy = y - l->y, d2 = dx * dx + dy * dy;
    if (d2 >= l->r2) return 0;
    float f = (1 - d2 / l->r2) * l->power;
    if (l->cone && d2 > 100) {
        float dot = (dx * l->cdir + dy * l->sdir) / sqrtf(d2);
        if (dot <= l->c0) return 0;
        if (dot < l->c1) f *= (dot - l->c0) / (l->c1 - l->c0);
    }
    if (f < 0.02f || (d2 > 36 && !los_light(l->x, l->y, x, y))) return 0;
    return f;
}

// Al cargar la zona: la luz de los fuegos y los faroles, con sus sombras
static void bake_lights(void)
{
    memset(g_lm, 0, sizeof(g_lm));
    for (int k = 0; k < g_nfire + g_nlamp; k++) {
        const Lamp *lp = k >= g_nfire ? &g_lamp[k - g_nfire] : NULL;
        Light l = lp ? mk_light(lp->x, lp->y, lp->ceil ? 62 : 74, lp->ceil ? 0.7f : 0.78f, 0, 0)
                     : mk_light(g_fire[k][0], g_fire[k][1], 68, 0.86f, 0, 0);
        int m = lp ? (lp->flick ? 2 : 1) : 0, r = (int)sqrtf(l.r2) / LG + 1;
        int cx = (int)l.x / LG, cy = (int)l.y / LG;
        for (int vy = cy - r; vy <= cy + r; vy++)
            for (int vx = cx - r; vx <= cx + r; vx++) {
                if (vx < 0 || vy < 0 || vx >= LMW || vy >= LMH) continue;
                int v = g_lm[m][vy][vx] + (int)(light_one(&l, vx * LG, vy * LG) * 255);
                g_lm[m][vy][vx] = v > 255 ? 255 : v;
            }
    }
}

static float flicker(float seed)
{
    float v = sinf(floorf(g_time * 10) * 12.9898f + seed * 78.233f) * 43758.5453f;
    return v - floorf(v);
}
static float fire_k(void) { return 0.9f + 0.08f * sinf(g_time * 13) + 0.05f * sinf(g_time * 29); }   // el fuego tiembla
static int lamps_dim(void) { return flicker(1) < 0.3f; }                                             // los faroles malos parpadean

static void add_light(float x, float y, float r, float power, float dir, float half)
{
    if (g_nl >= MAXL) return;
    if (x < g_ox - r || y < g_oy - r || x > g_ox + SCR_W + r || y > g_oy + SCR_H + r) return;   // no llega a la pantalla
    g_l[g_nl++] = mk_light(x, y, r, power, dir, half);
}

// Las luces que se mueven, una vez por cuadro
static void gather_lights(void)
{
    g_nl = 0;
    add_light(P.x, P.y, 46, 0.5f, 0, 0);
    add_light(P.x, P.y, 160, 1.0f, P.ang, 0.52f);
    armas_light();   // [armas]
    artef_light();   // [artef]
    if (P.flash_t > 0) add_light(P.x, P.y, 96, 1.0f, 0, 0);
    if (g_zone == 0 && g_up[UP_TOWER]) add_light(3 * TS + 8, 16 * TS - 12, 78, 0.8f, 0, 0);     // la luz de la torre de vigía
    if (g_zone == 0 && g_up[UP_GARDEN]) add_light(52 * TS, 31 * TS, 84, 0.45f, 0, 0);          // y un farol bajo sobre el huerto
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD) continue;
        if (e->irr) add_light(e->x, e->y, 30, 0.45f, 0, 0);                                 // el irradiado alumbra un poco a su alrededor
        if (is_human(e) && !is_post(e)) add_light(e->x, e->y - 6, 125, 0.85f, e->ang, 0.45f);   // los bandidos llevan linterna
        if (e->muzzle > 0) add_light(e->x, e->y - 6, 60, 0.7f, 0, 0);                       // y el fogonazo de quien dispara alumbra
    }
}

// Luz de las fuentes que se mueven en un punto (sin la penumbra ni la luz fija)
static float light_at(float x, float y)
{
    float sum = 0;
    for (int i = 0; i < g_nl; i++) sum += light_one(&g_l[i], x, y);
    return sum;
}

// Toda la luz que llega a un punto: sirve para saber si algo esta alumbrado
static float light_total(float x, float y)
{
    int vx = (int)(x / LG + 0.5f), vy = (int)(y / LG + 0.5f);
    float l = light_at(x, y);
    if (vx >= 0 && vy >= 0 && vx < LMW && vy < LMH)
        l += (g_lm[0][vy][vx] * fire_k() + g_lm[1][vy][vx] + g_lm[2][vy][vx] * (lamps_dim() ? 0.15f : 1)) / 255.0f;
    return l;
}

// ------------------------------------------------------------------ carga de la zona
// Hay charco radiactivo a menos de RAD_NEAR casillas de ese punto (se mira el plano del mapa, que ya está cargado)
static int rad_near(float x, float y)
{
    int cx = (int)(x / TS), cy = (int)(y / TS);
    for (int j = cy - RAD_NEAR; j <= cy + RAD_NEAR; j++)
        for (int i = cx - RAD_NEAR; i <= cx + RAD_NEAR; i++)
            if (i >= 0 && j >= 0 && i < MW && j < MH && g_zg[j][i] == '~' && (i - cx) * (i - cx) + (j - cy) * (j - cy) < RAD_NEAR * RAD_NEAR) return 1;
    return 0;
}

static void spawn_enemy(int type, float x, float y)
{
    if (g_ne >= MAXE) return;
    Enemy *e = &g_e[g_ne++];
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->x = e->hx = e->tx = x;
    e->y = e->hy = e->ty = y;
    e->ang = frange(0, 2 * PI);
    e->hp = EHP[type];
    e->hp *= duro_ehp();   // [duro]
    e->armor = ARMOR[type];
    e->strafe = rnd() & 1 ? 1 : -1;
    e->t = frange(0.5f, 3);
    e->fac = fac_of(type);
    e->var = g_ne & 1;                     // humanos: uno con pistola y otro con fusil, por turnos
    if (type == E_GRD) e->ang = 0;         // los guardias miran hacia el camino
    if (type == E_SNP) e->var = 1;         // el francotirador lleva fusil
    jz_spawn(e);   // [jefes2]
    e->irr = !is_human(e) && type != E_BOSS && type != E_BOSS2 && type != E_WOLF && type < E_VIUDA && (rad_near(x, y) || rnd() % 10 == 0);   // cualquier zombi puede nacer irradiado: siempre junto al charco
}

// Descubre el mapa a 9 casillas o menos del jugador (solo cuando cambia de casilla)
static void explore(void)
{
    int tx = (int)(P.x / TS), ty = (int)(P.y / TS);
    if (tx == g_seen_tx && ty == g_seen_ty) return;
    g_seen_tx = tx; g_seen_ty = ty;
    for (int y = ty - 9; y <= ty + 9; y++)
        for (int x = tx - 9; x <= tx + 9; x++)
            if (in_map(x, y) && (x - tx) * (x - tx) + (y - ty) * (y - ty) <= 81) g_seen[y][x] = 1;
}

// ---- el búnker: salas rectangulares unidas por pasillos de 2 casillas, todo piso interior y muros. Sale de g_bseed:
// con la misma semilla queda igual (así se puede guardar). La semilla cambia cuando termina una tormenta de Ceniza.
static u32 brnd(void) { g_brs = g_brs * 1664525u + 1013904223u; return g_brs >> 8; }
static int brange(int lo, int hi) { return hi <= lo ? lo : lo + (int)(brnd() % (u32)(hi - lo + 1)); }
static void bput(int x, int y, char ch) { if (x > 0 && y > 0 && x < MW - 1 && y < MH - 1 && g_zo[y][x] == ' ') g_zo[y][x] = ch; }
static void bcarve(int x, int y) { if (x > 0 && y > 0 && x < MW - 1 && y < MH - 1) { g_zg[y][x] = '_'; g_zo[y][x] = ' '; } }

static void bunker_gen(void)
{
    typedef struct { int x0, y0, x1, y1; } Room;
    Room r[10];
    int n = 0, far = 0;
    for (int y = 0; y < MH; y++) {
        memset(g_zg[y], '.', MW); g_zg[y][MW] = 0;
        memset(g_zo[y], '#', MW); g_zo[y][MW] = 0;
    }
    g_brs = g_bseed;
    for (int t = 0; t < 200 && n < 9; t++) {   // salas al azar que no se toquen
        int w = brange(6, 12), h = brange(5, 8), x0 = brange(2, MW - 3 - w), y0 = brange(3, MH - 3 - h), ok = 1;
        for (int i = 0; i < n; i++)
            if (x0 <= r[i].x1 + 3 && x0 + w - 1 >= r[i].x0 - 3 && y0 <= r[i].y1 + 3 && y0 + h - 1 >= r[i].y0 - 3) ok = 0;
        if (ok) r[n++] = (Room){x0, y0, x0 + w - 1, y0 + h - 1};
    }
    for (int i = 1; i < n; i++)   // de oeste a este: la primera es la de la escalera
        for (int k = i; k > 0 && r[k].x0 < r[k - 1].x0; k--) { Room t = r[k]; r[k] = r[k - 1]; r[k - 1] = t; }
    for (int i = 0; i < n; i++)
        for (int y = r[i].y0; y <= r[i].y1; y++)
            for (int x = r[i].x0; x <= r[i].x1; x++) bcarve(x, y);
    for (int i = 1; i <= n && n > 1; i++) {   // pasillos: cada sala con la siguiente y la última con la primera
        const Room *a = &r[i - 1], *b = &r[i % n];
        int ax = (a->x0 + a->x1) / 2, ay = (a->y0 + a->y1) / 2, bx = (b->x0 + b->x1) / 2, by = (b->y0 + b->y1) / 2;
        for (int x = ax < bx ? ax : bx; x <= (ax < bx ? bx : ax) + 1; x++) { bcarve(x, ay); bcarve(x, ay + 1); }
        for (int y = ay < by ? ay : by; y <= (ay < by ? by : ay) + 1; y++) { bcarve(bx, y); bcarve(bx + 1, y); }
    }
    // la escalera de salida, contra el muro de arriba de la primera sala
    g_zo[r[0].y0][r[0].x0 + 1] = 'S';
    bcarve(r[0].x0 + 1, r[0].y0 + 1);
    g_bun_ex = r[0].x0 + 1; g_bun_ey = r[0].y0 + 1;
    for (int i = 1, bd = -1; i < n; i++) {   // la sala más lejana
        int d = abs((r[i].x0 + r[i].x1) / 2 - (r[0].x0 + r[0].x1) / 2) + abs((r[i].y0 + r[i].y1) / 2 - (r[0].y0 + r[0].y1) / 2);
        if (d > bd) { bd = d; far = i; }
    }
    int meds = n > 2 ? 1 + (int)(brnd() % (u32)(n - 1)) : far;   // la caja de medicinas va en otra sala que la del registro
    if (n > 2 && meds == far) meds = far % (n - 1) + 1;
    for (int i = 0; i < n; i++) {
        const Room *a = &r[i];
        int cx = (a->x0 + a->x1) / 2, cy = (a->y0 + a->y1) / 2, w = a->x1 - a->x0 + 1, h = a->y1 - a->y0 + 1;
        if (i == 0 || brnd() % 3) bput(cx, cy, '*');   // lámpara de techo
        if (i == 0) continue;
        if (i == meds) bput(a->x1, a->y0, 'N');   // las cajas de los trabajos primero: un cajón al azar no puede taparles el sitio
        if (i == far) bput(a->x1 - 1, a->y0, 'Q');
        for (int k = 0, nc = 1 + (int)(brnd() % 2); k < nc; k++) bput(a->x0 + brange(0, w - 1), a->y0, 'r');   // cajones con mejor botín, contra el muro
        for (int k = 0, nz = 2 + (w * h > 40); k < nz; k++)
            bput(a->x0 + brange(0, w - 1), a->y0 + 1 + brange(0, h - 2), brnd() % 3 ? 'd' : 'm');
        if (i == far) bput(cx, cy + 1, 'B');   // un zombi grande cuida el cajón del registro
    }
}

// ---- las expediciones: el mapa exterior que se arma cada vez que se sale por el portón. Todo sale de g_x (semilla,
// tipo, peligro y marcas): con lo mismo queda igual, así se puede guardar. Primero el terreno, los edificios y sus
// puertas; después se comprueba por inundación desde la entrada que todo se pueda pisar, y cada cosa que se pone
// (cajones, escotilla, props) vuelve a comprobarlo: lo que cortaría el paso no se pone.
static unsigned char x_reach[MHMAX][MWMAX], x_keep[MHMAX][MWMAX];
typedef struct { int x0, y0, x1, y1; } XBld;
#define MAXB_X 128   // [mapas] [urbe]   // edificios por expedición
static XBld x_b[MAXB_X];
static int x_nb, x_ex, x_ey, x_n, x_nen, x_nt, x_px, x_py, x_pud, x_nd;
static int x_tx[256], x_ty[256], x_dx[128], x_dy[128];   // [mapas]   // [qurbe] 256 registros (antes 96): las casas con segundo piso de la ciudad también se registran
static int x_rail_y[2], x_rail_x[2], x_rail_l[2];   // los tramos de vía del pueblo minero
static int x_rx = 6, x_ry = 5, x_emin2 = 169;   // caja libre alrededor de la entrada (donde no se pone nada) y distancia mínima al cuadrado de los enemigos; los pisos las achican
// Edificios con escalera de la expedición cargada: cuántos (0 a 2), cuál es cada uno (índice en x_b), cuántos pisos de arriba tiene (1 a 3) y dónde está su escalera
static int g_nstb, g_stb_bi[2], g_stb_nf[2], g_stb_x[2], g_stb_y[2];
#define HS_MAX 72   // [mapas] casas con segundo piso por expedición
static int g_nhs, g_hs_bi[HS_MAX], g_hs_x[HS_MAX], g_hs_y[HS_MAX], g_hs_cur;   // [mapas] cuántas, cuál edificio es cada una, dónde está su escalera y cuál está cargada arriba
static float g_hs_bx, g_hs_by;   // [mapas] dónde se quedó el jugador abajo al subir (para guardar estando arriba)
static unsigned short g_hs_open[HS_MAX], g_hs_dead[HS_MAX];   // [mapas] por casa: máscara de cajones abiertos y de enemigos muertos de su segundo piso
static int g_fl_x0, g_fl_y0, g_fl_x1, g_fl_y1;   // el contorno del edificio del piso cargado (para dibujar sus ventanas)
static int g_x_fix, g_x_nen, g_x_nbld;   // lo que costó arreglar, enemigos puestos y edificios (para las pruebas)

static int x_pass_slow(char c) { return c == ' ' || c == 'D' || c == 'E' || c == '*' || c == 'd' || c == 'm' || c == 'B' || c == 'a' || c == 'J' || c == 'z' || c == 'M' || (c >= '5' && c <= '9') || c == '0'; }
static int x_pass(char c)   // se pisa (tabla, porque las inundaciones la usan millones de veces)
{
    static unsigned char tab[256];
    static int init;
    if (!init) { for (int i = 0; i < 256; i++) tab[i] = (unsigned char)x_pass_slow((char)i); init = 1; }
    return tab[(unsigned char)c];
}
static int x_inb(int x, int y) { return x > 0 && y > 0 && x < MW - 1 && y < MH - 1; }
static int x_free(int x, int y) { return x_inb(x, y) && g_zo[y][x] == ' '; }
static int x_flood(void)   // casillas que se alcanzan caminando desde la entrada
{
    static short q[MWMAX * MHMAX];
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int h = 0, t = 0;
    memset(x_reach, 0, sizeof(x_reach));
    if (!x_pass(g_zo[x_ey][x_ex])) return 0;
    x_reach[x_ey][x_ex] = 1;
    q[t++] = (short)(x_ey * MW + x_ex);
    while (h < t) {
        int c = q[h++], x = c % MW, y = c / MW;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!x_reach[ny][nx] && x_pass(g_zo[ny][nx])) { x_reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }   // el borde del mapa nunca se pisa: no hace falta comprobar límites
        }
    }
    return t;
}
static int x_near(int x, int y)   // la casilla o una vecina se alcanza
{
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    if (x_reach[y][x]) return 1;
    for (int k = 0; k < 4; k++) if (in_map(x + dx[k], y + dy[k]) && x_reach[y + dy[k]][x + dx[k]]) return 1;
    return 0;
}
// Poner un obstáculo en (x,y) no corta nada si sus vecinas pisables (de las 4) siguen unidas entre sí rodeando la casilla, dentro de una ventana de 11 x 11.
// Es una comprobación local y barata: si pasa, no hace falta inundar todo el mapa; si no pasa, se inunda como siempre.
#define XW 5
static int x_ring_open(int x, int y)
{
    static const signed char DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    static unsigned char seen[2 * XW + 1][2 * XW + 1];
    static signed char qx[(2 * XW + 1) * (2 * XW + 1)], qy[(2 * XW + 1) * (2 * XW + 1)];
    int nb = 0, nbx[4], nby[4];
    for (int k = 0; k < 4; k++) if (x_pass(g_zo[y + DY[k]][x + DX[k]])) { nbx[nb] = x + DX[k]; nby[nb++] = y + DY[k]; }
    if (nb <= 1) return nb == 1;   // una sola vecina pisable: nada que unir (si no hay ninguna, la casilla no se alcanzaba)
    memset(seen, 0, sizeof(seen));
    int h = 0, t = 0, found = 1;
    seen[XW][XW] = 1;   // la casilla misma cuenta como bloqueada
    seen[nby[0] - y + XW][nbx[0] - x + XW] = 1;
    qx[t] = (signed char)(nbx[0] - x); qy[t++] = (signed char)(nby[0] - y);
    while (h < t) {
        int cx = qx[h], cy = qy[h++];
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < -XW || nx > XW || ny < -XW || ny > XW || !in_map(x + nx, y + ny) || seen[ny + XW][nx + XW]) continue;
            if (!x_pass(g_zo[y + ny][x + nx])) continue;
            seen[ny + XW][nx + XW] = 1;
            qx[t] = (signed char)nx; qy[t++] = (signed char)ny;
        }
    }
    for (int i = 1; i < nb; i++) if (!seen[nby[i] - y + XW][nbx[i] - x + XW]) found = 0;
    return found;
}
// Pone ch en (x,y) solo si queda todo lo demás alcanzable (reg: es un cajón o la escotilla, que además se tiene que poder usar)
static int x_try(int x, int y, char ch, int reg)
{
    if (!x_free(x, y) || x_keep[y][x] || !x_reach[y][x]) return 0;
    char g = g_zg[y][x];
    if (g == '~' || g == 'Z' || g == 'D' || (abs(x - x_ex) < x_rx && abs(y - x_ey) < x_ry) || (reg && x_nt >= 250)) return 0;
    if (!x_pass(ch) && x_ring_open(x, y)) {   // vía rápida: con las 8 casillas de alrededor libres, el obstáculo no puede cortar nada (ni dejar un cajón sin acceso)
        g_zo[y][x] = ch; x_reach[y][x] = 0; x_n--;
        for (int i = 0; i < x_nt; i++) if (!x_near(x_tx[i], x_ty[i])) { g_zo[y][x] = ' '; x_reach[y][x] = 1; x_n++; return 0; }   // un cajón vecino se quedaría sin acceso
        if (reg) { x_tx[x_nt] = x; x_ty[x_nt++] = y; }
        return 1;
    }
    g_zo[y][x] = ch;
    int n = x_flood(), ok = n == x_n - 1 && (!reg || x_near(x, y));
    for (int i = 0; ok && i < x_nt; i++) if (!x_near(x_tx[i], x_ty[i])) ok = 0;
    if (!ok) { g_zo[y][x] = ' '; x_n = x_flood(); return 0; }
    x_n = n;
    if (reg) { x_tx[x_nt] = x; x_ty[x_nt++] = y; }
    return 1;
}
static int x_prop(int x, int y, char ch) { return in_map(x, y) && g_zg[y][x] != '=' && g_zg[y][x] != 'r' && x_try(x, y, ch, 0); }
static int x_block(char ch, int x, int y, int w, int h)   // contenedores: un bloque entero o nada
{
    int ok = 1;
    for (int j = 0; j < h && ok; j++) for (int i = 0; i < w && ok; i++) ok = x_prop(x + i, y + j, ch);
    if (ok) return 1;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (in_map(x + i, y + j) && g_zo[y + j][x + i] == ch) g_zo[y + j][x + i] = ' ';
    x_n = x_flood();
    return 0;
}
static int x_pair(int x, int y)   // un auto: dos casillas
{
    if (!x_try(x, y, 'V', 0)) return 0;
    if (x_try(x + 1, y, 'v', 0)) return 1;
    g_zo[y][x] = ' ';
    x_n = x_flood();
    return 0;
}
// Busca dónde poner ch: bi >= 0 dentro de ese edificio, -1 en cualquier parte, -2 solo afuera; a dmin casillas o más de la entrada
static int x_place(char ch, int bi, int dmin, int reg)
{
    int tries = 250;
    if (bi >= 0) { tries = 4 * (x_b[bi].x1 - x_b[bi].x0 - 1) * (x_b[bi].y1 - x_b[bi].y0 - 1); if (tries > 250) tries = 250; }   // en un edificio chico no hace falta probar tantas veces
    for (int t = 0; t < tries; t++) {
        int x, y;
        if (bi >= 0) { x = brange(x_b[bi].x0 + 1, x_b[bi].x1 - 1); y = brange(x_b[bi].y0 + 1, x_b[bi].y1 - 1); }
        else { x = brange(1, MW - 2); y = brange(1, MH - 2); if (bi == -2 && g_zg[y][x] == '_') continue; }
        if ((x - x_ex) * (x - x_ex) + (y - x_ey) * (y - x_ey) < dmin * dmin) continue;
        if (x_try(x, y, ch, reg)) return 1;
    }
    return 0;
}
static int x_body(char ch)   // cuerpo en el suelo: no estorba el paso
{
    for (int t = 0; t < 200; t++) {
        int x = brange(1, MW - 2), y = brange(1, MH - 2);
        if (!x_free(x, y) || !x_reach[y][x] || x_keep[y][x] || g_zg[y][x] == '~' || g_zg[y][x] == 'D' || g_zg[y][x] == 'Z') continue;
        if ((x - x_ex) * (x - x_ex) + (y - x_ey) * (y - x_ey) < 36) continue;
        g_zo[y][x] = ch;
        return 1;
    }
    return 0;
}
static int x_enemy(char ch, int cx, int cy, int rad)   // enemigo cerca de (cx,cy), nunca a menos de 13 casillas de la entrada
{
    if (x_nen + (ch == 'J' ? 3 : 1) > MAXE - 12) return 0;   // [mapas]
    for (int t = 0; t < 40; t++) {
        int x = cx + brange(-rad, rad), y = cy + brange(-rad, rad);
        if (!x_free(x, y) || !x_reach[y][x] || g_zg[y][x] == 'D' || g_zg[y][x] == 'Z') continue;
        if ((x - x_ex) * (x - x_ex) + (y - x_ey) * (y - x_ey) < x_emin2) continue;
        g_zo[y][x] = ch;
        x_nen += ch == 'J' ? 3 : 1;
        return 1;
    }
    return 0;
}
static void x_fill(int x0, int y0, int x1, int y1, char ch)
{
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (x_inb(x, y)) g_zg[y][x] = ch;
}
// Abre un paso desde (sx,sy) hasta lo alcanzable: primero quitando lo que estorba, y si no, atravesando muros
static int x_connect(int sx, int sy)
{
    static short par[MWMAX * MHMAX], q[MWMAX * MHMAX];
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int s = sy * MW + sx;
    for (int walls = 0; walls < 2; walls++) {
        for (int i = 0; i < MW * MH; i++) par[i] = -1;
        int h = 0, t = 0;
        q[t++] = (short)s; par[s] = (short)s;
        while (h < t) {
            int c = q[h++], x = c % MW, y = c / MW;
            if (c != s && x_reach[y][x]) {
                for (int p = c; p != s;) {
                    p = par[p];
                    int px = p % MW, py = p / MW;
                    if (g_zo[py][px] == '#') g_zg[py][px] = 'D';
                    g_zo[py][px] = ' ';
                }
                return 1;
            }
            for (int k = 0; k < 4; k++) {
                int nx = x + dx[k], ny = y + dy[k];
                if (!x_inb(nx, ny) || par[ny * MW + nx] >= 0) continue;
                char ch = g_zo[ny][nx];
                if (ch == 'c' || ch == 'r' || ch == 'X' || ch == 'W' || ch == 'H' || (ch == '#' && !walls)) continue;
                par[ny * MW + nx] = (short)c;
                q[t++] = (short)(ny * MW + nx);
            }
        }
    }
    return 0;
}

static const unsigned char X_GRASS[XT_N] = {10, 15, 80, 6, 55, 8, [XT_PINE] = 72, [XT_CITY] = 5};   // [armas]   // % de pasto en el suelo de cada tipo

#ifdef PEPEL_DEBUG
static unsigned g_xph[7], g_xph_t;   // pruebas: lo que tarda cada fase del generador (microsegundos acumulados)
#define XPH(i) do { unsigned _n = sceKernelGetSystemTimeLow(); if (g_xph_t) g_xph[i] += _n - g_xph_t; g_xph_t = _n; } while (0)
#else
#define XPH(i) ((void)0)
#endif
// ---- eventos al azar de la expedición (1 por expedición, 2 desde peligro 4) y el perro herido. Qué eventos hay lo decide exped_start
// (g_x.ev0 y g_x.ev1) y dónde caen sale de la semilla: aquí solo se reserva el sitio (terreno, restos y cajones en las grillas);
// los personajes los pone ev_spawn al armar la zona, y lo que pasa con el tiempo está en ev_update
enum { EV_NONE, EV_SURV, EV_CARAVAN, EV_HELI, EV_HORDE, EV_N };
enum { EVN_NONE, EVN_GUARD, EVN_BANDIT, EVN_HORDE, EVN_HELI, EVN_FARO, EVN_POZO };   // [arregla] EVN_POZO: guardianes del Pozo y jefes sueltos infernales (no son los jefes de verdad)   // Enemy.ev: a qué evento pertenece
typedef struct { int kind, tx, ty, x2, y2; } Ev;   // el centro del evento; (x2,y2): el alijo que marca el superviviente / el final del camino de la horda
static Ev g_ev[2];
static int g_fx = -1, g_fy;   // el puesto farolero (-1: no hay)
static int g_nev, g_dogx = -1, g_dogy, g_ev_tk[8][2], g_ev_ntk;
static int g_ev_sx, g_ev_sy;
// Busca un sitio de (2r+1)^2 casillas libres, alcanzables, sin edificio ni radiación, a dmin casillas o más de la entrada y lejos de la retirada, la escotilla, las escaleras y de otro evento
static int ev_clear(char o) { return o == 'T' || o == 'f' || o == 'b' || o == 's' || o == 'i' || o == 'x' || o == 'I'; }   // lo que se puede quitar para hacer sitio
static int ev_spot(int r, int dmin)
{
    for (int t = 0; t < 1200; t++) {
        int cx = brange(r + 3, MW - r - 4), cy = brange(r + 3, MH - r - 4), ok = 1;
        if ((cx - x_ex) * (cx - x_ex) + (cy - x_ey) * (cy - x_ey) < dmin * dmin) continue;
        if ((cx - g_ret2_tx) * (cx - g_ret2_tx) + (cy - g_ret2_ty) * (cy - g_ret2_ty) < 12 * 12) continue;
        if (g_ret3_tx >= 0 && (cx - g_ret3_tx) * (cx - g_ret3_tx) + (cy - g_ret3_ty) * (cy - g_ret3_ty) < 12 * 12) continue;   // [mapas]
        for (int k = 0; k < g_ev_ntk && ok; k++) if ((cx - g_ev_tk[k][0]) * (cx - g_ev_tk[k][0]) + (cy - g_ev_tk[k][1]) * (cy - g_ev_tk[k][1]) < 14 * 14) ok = 0;
        for (int y = cy - 6; y <= cy + 6 && ok; y++)   // escotilla y escaleras: a 6 casillas o más (lo que exige la prueba)
            for (int x = cx - 6; x <= cx + 6 && ok; x++)
                if (in_map(x, y) && (g_zo[y][x] == 'H' || g_zo[y][x] == '3' || g_zo[y][x] == '4') && (x - cx) * (x - cx) + (y - cy) * (y - cy) < 8 * 8) ok = 0;
        for (int y = cy - r - 3; y <= cy + r + 3 && ok; y++)
            for (int x = cx - r - 3; x <= cx + r + 3 && ok; x++) {
                char o = g_zo[y][x], g = g_zg[y][x];
                int in = abs(x - cx) <= r && abs(y - cy) <= r;
                if (o == 'H' || o == '3' || o == '4' || g == 'Z') ok = 0;
                else if (in && ((o != ' ' && !ev_clear(o)) || x_keep[y][x] || g == '~' || g == '_' || g == 'D' || g == 'r')) ok = 0;
            }
        if (!ok || !x_reach[cy][cx]) continue;
        for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) if (g_zo[y][x] != ' ') g_zo[y][x] = ' ';   // árboles, rejas y barriles del sitio se quitan: el centro ya es alcanzable
        x_n = x_flood();
        g_ev_sx = cx; g_ev_sy = cy;
        if (g_ev_ntk < 8) { g_ev_tk[g_ev_ntk][0] = cx; g_ev_tk[g_ev_ntk][1] = cy; g_ev_ntk++; }
        return 1;
    }
    return 0;
}
static int ev_spot_far(int r)   // lo más lejos que se pueda de la entrada
{
    static const int D[3] = {38, 28, 18};
    for (int i = 0; i < 3; i++) if (ev_spot(r, D[i])) return 1;
    return 0;
}
typedef struct {
    int have, st, atk, wounded, home, skip, zone;   // lo tienes; st: orden (DG_); atk: ataca al blanco fijado; herido; en la base; no sale en la próxima expedición; zona en que está
    float x, y, hp, ang, walk, bite_cd, warn_t, bark_t, down_t, flash, growl_t;
    int moving;
} Dog;
enum { DG_FOLLOW, DG_STAY };
#define DOG_HP (g_sk[SK_ADIES] ? 105 : 70)   // Adiestrador: Hollín aguanta más
static Dog g_dog;
static unsigned char g_dbk[MAXCR];   // cajones por los que ya ladró en esta zona
static float g_ev_t;
static void dog_kennel(void);
static void dog_to_player(void);
static void ev_put(int x, int y, char ch) { if (in_map(x, y) && g_zo[y][x] == ' ') g_zo[y][x] = ch; }
static void ev_gen(void)
{
    int used = 0, ks[2] = {g_x.ev0, g_x.ev1};
    g_nev = 0; g_dogx = -1; g_fx = -1; g_ev_ntk = 0;
    x_n = x_flood();
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) { char c = g_zo[y][x]; used += c == 'c' || c == 'X' || c == 'W' || c == 'r' || c == 'N' || c == 'Q' || c == 'z' || c == 'M'; }
    if ((g_x.flags & XF_DOG) && used + 1 <= MAXCR && ev_spot_far(1)) { g_dogx = g_ev_sx; g_dogy = g_ev_sy; used++; }
    for (int i = 0; i < 2; i++) {
        int k = ks[i], cx, cy;
        Ev *v = &g_ev[g_nev];
        if (k <= EV_NONE || k >= EV_N || used + 3 > MAXCR) continue;
        memset(v, 0, sizeof(*v));
        v->kind = k;
        if (k == EV_SURV) {
            if (!ev_spot_far(1)) continue;
            v->tx = g_ev_sx; v->ty = g_ev_sy;
            if (!ev_spot(1, 24)) continue;   // el alijo que marca si lo ayudas
            v->x2 = g_ev_sx; v->y2 = g_ev_sy;
            g_zo[v->y2][v->x2] = 'r';
            used += 2;
        } else if (k == EV_CARAVAN) {
            if (!ev_spot_far(4)) continue;
            cx = v->tx = g_ev_sx; cy = v->ty = g_ev_sy;
            ev_put(cx, cy, 'V'); ev_put(cx + 1, cy, 'v');   // el carro
            ev_put(cx - 1, cy - 2, 'r'); ev_put(cx + 2, cy - 2, 'r'); ev_put(cx, cy + 2, 'c');
            used += 3;
        } else if (k == EV_HELI) {
            if (!ev_spot_far(4)) continue;
            cx = v->tx = g_ev_sx; cy = v->ty = g_ev_sy;
            for (int y = cy - 4; y <= cy + 4; y++)   // radiación alrededor de los restos
                for (int x = cx - 4; x <= cx + 4; x++)
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 17 && in_map(x, y)) g_zg[y][x] = '~';
            ev_put(cx - 1, cy, 'G'); ev_put(cx, cy, 'G'); ev_put(cx + 1, cy, 'G'); ev_put(cx, cy - 1, 'G');   // la panza y la cola
            ev_put(cx - 2, cy + 2, 'r'); ev_put(cx + 2, cy + 2, 'r');
            used += 2;
        } else {   // la horda: el camino cruza el mapa por una fila libre de un extremo al otro
            int ok = 0, y = 0;
            for (int t = 0; t < 80 && !ok; t++) {
                y = brange(10, MH - 11);
                ok = x_free(MW - 6, y) && x_reach[y][MW - 6] && x_free(8, y) && x_reach[y][8];
            }
            if (!ok) continue;
            int east = (int)(brnd() & 1);   // de este a oeste o al revés
            v->tx = east ? MW - 6 : 8; v->ty = y;
            v->x2 = east ? 8 : MW - 6; v->y2 = y;
            if (g_ev_ntk < 8) { g_ev_tk[g_ev_ntk][0] = v->tx; g_ev_tk[g_ev_ntk][1] = y; g_ev_ntk++; }
        }
        g_nev++;
        if (g_nev >= 2) break;
    }
    if ((g_x.flags & XF_FARO) && used + 2 <= MAXCR && ev_spot_far(4)) {   // el puesto farolero: farolas en las esquinas, un barril con fuego y el comerciante en el centro
        int cx = g_ev_sx, cy = g_ev_sy;
        g_fx = cx; g_fy = cy;
        ev_put(cx - 3, cy - 3, 'L'); ev_put(cx + 3, cy - 3, 'L'); ev_put(cx - 3, cy + 3, 'L'); ev_put(cx + 3, cy + 3, 'L'); ev_put(cx + 2, cy, 'F');
    }
    x_n = x_flood();
}

#include "mapas.inc"   // [mapas] trazados: cuadrícula, avenida con plaza y quebrada con puentes
#include "mapnuc.inc"   // [mapnuc] planta atómica y tierra maldita
#include "mapas3.inc"   // [mapas3] hospital, puerto y autopista
#include "diseno.inc"   // [diseno] ambientes, suelo vivo, mobiliario de calle y la ciudad
#include "urbe.inc"   // [urbe] bocas de metro, el metro (zona ZM) y fachadas altas
#include "arte.inc"   // [arte] suelo propio por tipo de mapa
static void exped_gen(void)
{
#ifdef PEPEL_DEBUG
    g_xph_t = 0;
#endif
    XPH(0);
    g_ret3_tx = g_ret3_ty = -1; g_nhs = 0;   // [mapas]
    if (jz_gen()) return;   // [jefes2] zonas de los jefes nuevos
    const int type = g_x.type, d = g_x.danger, fl = g_x.flags;
    int ey, n, t;
    g_brs = g_x.seed;
    memset(x_keep, 0, sizeof(x_keep));
    x_nb = x_nt = x_nd = x_nen = x_pud = g_nstb = 0;
    x_rx = 6; x_ry = 5; x_emin2 = 169;
    ey = type == XT_CITY ? 14 + 22 * (int)(brnd() % 5) : brange(10, MH - 12);   // [diseno] la ciudad: una calle de la cuadrícula
    ey = m3_ey_pick(type, ey);   // [mapas3]
    x_ex = 3; x_ey = ey;
    for (int y = 0; y < MH; y++) {
        for (int x = 0; x < MW; x++) {
            int r = (int)(brnd() % 100);
            g_zg[y][x] = r < (m3_is(type) ? m3_grass(type) : X_GRASS[type]) ? ',' : '.';   // [mapas3]
            g_zo[y][x] = ' ';
        }
        g_zg[y][MW] = 0; g_zo[y][MW] = 0;
    }
    // el borde de árboles, irregular
    for (int x = 0; x < MW; x++) {
        g_zo[0][x] = g_zo[MH - 1][x] = 'T';
        if (brnd() % 100 < 55) g_zo[1][x] = 'T';
        if (brnd() % 100 < 55) g_zo[MH - 2][x] = 'T';
        if (brnd() % 100 < 20) g_zo[2][x] = 'T';
        if (brnd() % 100 < 20) g_zo[MH - 3][x] = 'T';
    }
    for (int y = 0; y < MH; y++) {
        g_zo[y][0] = g_zo[y][MW - 1] = 'T';
        if (brnd() % 100 < 55) g_zo[y][1] = 'T';
        if (brnd() % 100 < 55) g_zo[y][MW - 2] = 'T';
        if (brnd() % 100 < 20) g_zo[y][2] = 'T';
        if (brnd() % 100 < 20) g_zo[y][MW - 3] = 'T';
    }
    // los caminos
    x_fill(1, ey, MW - 4 - (int)(brnd() % 4), ey + 1, '=');
    if (type == XT_TRAIN) {
        int bx = brange(22, 46), r1 = brange(3, MH / 2 - 8), r2 = brange(MH / 2 + 2, MH - 10);
        x_fill(bx, 3, bx + 1, MH - 4, '=');
        { int bx2 = bx + brange(34, 50); x_fill(bx2, 3, bx2 + 1, MH - 4, '='); x_fill(bx2 + brange(34, 50), 3, bx2 + brange(34, 50) + 1, MH - 4, '='); }   // [mapas] tres calles de norte a sur
        for (int k = 0; k < 2; k++)   // dos haces de vías de lado a lado
            for (int x = 2; x < MW - 2; x++) { if (g_zg[r1 + 3 * k][x] != '=') g_zg[r1 + 3 * k][x] = 'r'; if (g_zg[r2 + 3 * k][x] != '=') g_zg[r2 + 3 * k][x] = 'r'; }
    } else if (type == XT_TOWN) {
        int y2 = ey > MH / 2 ? ey - 22 : ey + 22;
        x_fill(14, y2, MW - 4, y2 + 1, '=');
        { int y3 = y2 > MH / 2 ? y2 - 36 : y2 + 36; x_fill(14, y3, MW - 4, y3 + 1, '='); }   // [mapas] una tercera calle de este a oeste
        for (int k = 0; k < 9; k++) { int vx = 16 + 18 * k + brange(0, 3); x_fill(vx, 3, vx + 1, MH - 4, '='); }
    } else if (type == XT_CITY) {   // [diseno] cuadrícula de calles de 2 casillas cada 22
        for (int k = 0; k < 5; k++) { int hy = 14 + 22 * k; if (hy != ey) x_fill(4, hy, MW - 5, hy + 1, '='); }
        for (int k = 0; k < 8; k++) { int vx = 16 + 22 * k + (k == 3 ? 3 : 0); x_fill(vx, 3, vx + 1, MH - 4, '='); }
    } else if (type == XT_FACTORY) {   // dos pasillos largos de norte a sur entre las naves
        int b1 = brange(17, 25), b2 = brange(38, 48), b3 = brange(68, 78), b4 = brange(96, 108), b5 = brange(126, 136), b6 = brange(154, 168);   // [mapas]
        x_fill(b5, 3, b5 + 1, MH - 4, '='); x_fill(b6, 3, b6 + 1, MH - 4, '=');
        { int y3 = ey > MH / 2 ? ey - 50 : ey + 50; x_fill(14, y3, MW - 4, y3 + 1, '='); }
        x_fill(b1, 3, b1 + 1, MH - 4, '=');
        x_fill(b2, 3, b2 + 1, MH - 4, '=');
        x_fill(b3, 3, b3 + 1, MH - 4, '=');
        x_fill(b4, 3, b4 + 1, MH - 4, '=');
        { int y2 = ey > MH / 2 ? ey - 24 : ey + 24; x_fill(14, y2, MW - 4, y2 + 1, '='); }
    } else if (type == XT_GRAVE) {   // un cruce de caminos de tierra
        int bx = brange(24, 40), bx2 = brange(70, 96), y2 = ey > MH / 2 ? ey - 24 : ey + 24;
        { int bx3 = brange(130, 160); x_fill(bx3, 3, bx3 + 1, MH - 4, '='); x_fill(bx2, y2 > MH / 2 ? y2 - 30 : y2 + 30, bx3 + 1, (y2 > MH / 2 ? y2 - 30 : y2 + 30) + 1, '='); }   // [mapas]
        x_fill(bx, 3, bx + 1, MH - 4, '=');
        x_fill(bx2, 3, bx2 + 1, MH - 4, '=');
        x_fill(bx, y2, bx2 + 1, y2 + 1, '=');
    } else if (m3_is(type)) {   // [mapas3] los trazados del hospital, el puerto y la autopista los arma m3_terrain
    } else {   // forest y pueblo minero: un camino que serpentea
        int y = ey;
        for (int x = 1; x < MW - 4; x++) {
            int r = (int)(brnd() % 4);
            if (r == 0 && y > 4) y--; else if (r == 1 && y < MH - 6) y++;
            x_fill(x, y, x, y + 1, '=');
        }
        {   // un segundo camino, desde la mitad del primero hacia el otro lado del mapa
            int y3 = ey > MH / 2 ? 8 + (int)(brnd() % 12) : MH - 20 + (int)(brnd() % 12), x3 = MW / 2, yy = ey;
            while (yy != y3) { x_fill(x3, yy, x3, yy + 1, '='); yy += yy < y3 ? 1 : -1; }
            for (int x = x3; x < MW - 4; x++) {
                int r = (int)(brnd() % 4);
                if (r == 0 && y3 > 4) y3--; else if (r == 1 && y3 < MH - 6) y3++;
                x_fill(x, y3, x, y3 + 1, '=');
            }
        }
        if (type == XT_MINE)   // y dos tramos cortos de vía
            for (int k = 0; k < 2; k++) {
                int ry = brange(4, MH - 6), rx = brange(12, MW - 24), rl = brange(10, 20);
                x_rail_y[k] = ry; x_rail_x[k] = rx; x_rail_l[k] = rl;
                for (int x = rx; x < rx + rl; x++) if (g_zg[ry][x] != '=') g_zg[ry][x] = 'r';
            }
    }
    // el charco radiactivo
    x_pud = type == XT_FOREST || (fl & XF_BEAST) || brnd() % 5 < 3;
    if (x_pud) {
        int pr = type == XT_FOREST ? 3 : 2;
        x_px = brange(40, MW - 12); x_py = brange(8, MH - 9);
        for (int dy = -pr - 1; dy <= pr + 1; dy++)
            for (int dx = -pr - 1; dx <= pr + 1; dx++)
                if (dx * dx + dy * dy <= pr * pr + (int)(brnd() % 3) && g_zg[x_py + dy][x_px + dx] != '=') g_zg[x_py + dy][x_px + dx] = '~';
    }
    // el punto de retirada: un cuadro marcado sobre el camino
    x_fill(2, ey, 3, ey + 1, 'Z');
    // el segundo punto de retirada, en el extremo opuesto: un cuadro marcado en un claro junto al borde este
    {
        int ey2 = m3_ey2_pick(type, brange(8, MH - 10), ey);   // [mapas3]
        x_fill(MW - 12, ey2, MW - 5, ey2 + 1, '=');   // un tramo de camino que llega hasta él
        x_fill(MW - 6, ey2, MW - 5, ey2 + 1, 'Z');
        g_ret2_tx = MW - 6; g_ret2_ty = ey2;
    }
    {   // [mapas] el tercer punto de retirada, en el norte o el sur, hacia el centro del mapa
        int top = m3_top_pick(type, ey >= MH / 2 || (ey > MH / 2 - 10 && brnd() % 2)), rx3 = brange(MW / 2 - 24, MW / 2 + 24), ry3 = top ? 4 : MH - 6;
        x_fill(rx3 - 6, ry3, rx3 + 6, ry3 + 1, '=');
        x_fill(rx3 - 1, top ? 4 : MH - 12, rx3, top ? 12 : MH - 5, '=');
        x_fill(rx3, ry3, rx3 + 1, ry3 + 1, 'Z');
        g_ret3_tx = rx3; g_ret3_ty = ry3;
    }
    lay_apply(type);   // [mapas]
    m3_terrain(type);   // [mapas3]
    XPH(0);
    // los edificios: muro, piso, una o dos puertas, y a veces una lámpara
    int nbt = type == XT_CITY ? brange(112, 124) : m3_is(type) ? brange(15, 19) : type == XT_PINE ? 3 : type == XT_TOWN ? brange(40, 50) : type == XT_FACTORY ? brange(16, 20) : type == XT_GRAVE || type == XT_MINE ? brange(18, 24) : brange(18, 26);   // [mapas]
    for (t = 0; t < (type == XT_CITY ? 14000 : 4000) && x_nb < nbt; t++) {   // [urbe]
        int m = type == XT_CITY ? 1 : t < 2000 ? 2 : 1, w, h;   // [urbe] la ciudad: manzanas pegadas, con callejones de una casilla

        if (type == XT_PINE && x_nb == 1) { w = 3; h = 3; }   // [armas] la torre de caza (la cabaña es la primera)
        else if (type == XT_PINE && x_nb == 0) { w = brange(6, 8); h = brange(5, 6); }   // [armas] la cabaña del guardabosques, con espacio para escalera
        else if (type == XT_TRAIN) { w = brange(8, 12); h = brange(5, 7); }
        else if (type == XT_TOWN) { w = brange(5, 8); h = brange(4, 6); }
        else if (type == XT_CITY) { w = brange(4, 7); h = brange(4, 6); }   // [urbe]   // [diseno] edificios altos
        else if (type == XT_PORT) { w = brange(8, 12); h = brange(5, 7); }   // [mapas3] bodegas
        else if (type == XT_FACTORY) { w = brange(9, 13); h = brange(6, 8); }   // naves grandes
        else if (type == XT_GRAVE) { if (x_nb == 0) { w = brange(7, 9); h = brange(5, 6); } else { w = brange(4, 5); h = 4; } }   // la capilla y los mausoleos
        else if (type == XT_MINE) { w = brange(4, 6); h = brange(4, 5); }   // casas chicas
        else { w = brange(5, 7); h = brange(4, 5); }
        int x0 = brange(10, MW - 4 - w), y0 = brange(4, MH - 4 - h), x1, y1, ok = 1;
        if (t < 1500) {   // al principio cada edificio va a un sector distinto del mapa (6 columnas por 3 filas), para repartirlos
            int nc = type == XT_CITY ? 12 : type == XT_TOWN ? 9 : 6, nr = type == XT_CITY ? 5 : type == XT_TOWN ? 5 : 3, sx = x_nb % nc, sy = (x_nb / nc) % nr, bw = (MW - 14) / nc, bh = (MH - 8) / nr;   // [urbe]   // [mapas] 6 columnas por 3 filas
            x0 = brange(10 + sx * bw, 10 + sx * bw + bw - w - 1); y0 = brange(4 + sy * bh, 4 + sy * bh + bh - h - 1);
        }
        x1 = x0 + w - 1; y1 = y0 + h - 1;
        for (int j = y0 - m; j <= y1 + m && ok; j++)
            for (int i = x0 - m; i <= x1 + m && ok; i++) {
                char g = in_map(i, j) ? g_zg[j][i] : '=';
                if (g == '=' || g == '~' || g == 'r' || g == '_' || g == 'Z' || g_zo[j][i] != ' ') ok = 0;
            }
        if (!ok) continue;
        for (int j = y0; j <= y1; j++)
            for (int i = x0; i <= x1; i++) { g_zg[j][i] = '_'; g_zo[j][i] = (i == x0 || i == x1 || j == y0 || j == y1) ? '#' : ' '; }
        x_b[x_nb++] = (XBld){x0, y0, x1, y1};
        int nd = (type == XT_FACTORY ? 3 : 1) + (int)(brnd() % 2), s0 = (int)(brnd() % 4);
        for (int k = 0; k < nd; k++) {
            int s = (s0 + (k ? 1 + (int)(brnd() % 3) : 0)) % 4;
            int ddx = s < 2 ? brange(x0 + 1, x1 - 1) : s == 2 ? x0 : x1;
            int ddy = s >= 2 ? brange(y0 + 1, y1 - 1) : s == 0 ? y0 : y1;
            int ox = s == 2 ? -1 : s == 3 ? 1 : 0, oy = s == 0 ? -1 : s == 1 ? 1 : 0;
            g_zo[ddy][ddx] = ' '; g_zg[ddy][ddx] = 'D';
            x_keep[ddy][ddx] = x_keep[ddy + oy][ddx + ox] = x_keep[ddy - oy][ddx - ox] = 1;
            if (x_nd < 128) { x_dx[x_nd] = ddx; x_dy[x_nd++] = ddy; }
        }
        if (brnd() % 10 < (type == XT_CITY ? 2 : 7)) g_zo[(y0 + y1) / 2][(x0 + x1) / 2] = '*';   // [urbe] pocas lámparas en la ciudad (muchos edificios)   // lámpara de techo
    }
    XPH(1);
    g_x_nbld = x_nb;
    // árboles sueltos
    n = type == XT_PINE ? 17 : type == XT_FOREST ? 13 : type == XT_TOWN ? 2 : type == XT_GRAVE ? 6 : 2;
    for (int y = 3; y < MH - 3; y++)
        for (int x = 3; x < MW - 3; x++) {
            char g = g_zg[y][x];
            if (g_zo[y][x] != ' ' || g == '=' || g == '_' || g == 'D' || g == '~' || g == 'r' || g == 'Z' || x_keep[y][x]) continue;
            if (abs(x - x_ex) < 7 && abs(y - ey) < 7) continue;
            if ((int)(brnd() % 100) < n) g_zo[y][x] = 'T';
        }
    armas_pine(type, ey);   // [armas] arbustos y claros del bosque de pinos
    // lo que rodea la entrada, libre
    for (int y = ey - 2; y <= ey + 3; y++) for (int x = 1; x <= 8; x++) if (x_inb(x, y)) g_zo[y][x] = ' ';
    for (int y = g_ret2_ty - 2; y <= g_ret2_ty + 3; y++) for (int x = MW - 10; x <= MW - 4; x++) if (x_inb(x, y)) g_zo[y][x] = ' ';   // y lo del otro punto de retirada
    for (int y = g_ret3_ty - 3; y <= g_ret3_ty + 4; y++) for (int x = g_ret3_tx - 5; x <= g_ret3_tx + 6; x++) if (x_inb(x, y) && g_zg[y][x] != '_') g_zo[y][x] = ' ';   // [mapas] y el tercero
    // todas las puertas tienen que alcanzarse: si no, se abre paso
    for (t = 0; t < 20; t++) {
        int bad = 0;
        x_n = x_flood();
        for (int i = 0; i < x_nd && !bad; i++)
            if (!x_reach[x_dy[i]][x_dx[i]]) { x_connect(x_dx[i], x_dy[i]); g_x_fix++; bad = 1; }
        if (!bad && !x_reach[g_ret2_ty][g_ret2_tx]) { x_connect(g_ret2_tx, g_ret2_ty); g_x_fix++; bad = 1; }
        if (!bad && !x_reach[g_ret3_ty][g_ret3_tx]) { x_connect(g_ret3_tx, g_ret3_ty); g_x_fix++; bad = 1; }   // [mapas]
        if (!bad) break;
    }
    x_n = x_flood();
    XPH(2);
    m3_props(type);   // [mapas3] cajones y adornos del hospital, el puerto y la autopista
    // props: contenedores, autos, rejas, sacos, barriles, barriles con fuego y faroles
    if (type == XT_TRAIN || type == XT_FACTORY) { n = type == XT_TRAIN ? brange(24, 32) : brange(10, 16); for (int i = 0; i < n; i++) for (t = 0; t < 30; t++) if (x_block(brnd() % 2 ? 'C' : 'K', brange(9, MW - 7), brange(3, MH - 5), 4, 2)) break; }
    n = type == XT_CITY ? brange(32, 42) : type == XT_TOWN ? brange(24, 32) : brange(9, 15);   // [diseno]
    for (int i = 0; i < n; i++)
        for (t = 0; t < 60; t++) {
            int x = brange(4, MW - 6), y = brange(3, MH - 4);
            if (type != XT_FOREST && t < 40 && g_zg[y][x] != '=') continue;
            if (g_zg[y][x] != '_' && x_pair(x, y)) break;
        }
    if (type == XT_CITY || type == XT_TOWN) urbe_street_props();   // [qurbe] buses, paraderos y kioscos
    if (type == XT_FACTORY) {   // maquinaria dentro de las naves y tanques grandes en los patios
        for (int b = 0; b < x_nb; b++) { n = brange(3, 5); for (int i = 0; i < n; i++) x_place('G', b, 0, 0); }
        n = brange(15, 20);
        for (int i = 0; i < n; i++) for (t = 0; t < 40; t++) { int x = brange(8, MW - 6), y = brange(3, MH - 5); if (g_zg[y][x] != '_' && x_block('O', x, y, 2, 2)) break; }
    } else if (type == XT_GRAVE) {   // hileras de tumbas (lápidas y cruces, una de por medio para poder pasar) y la capilla con sus cruces
        n = brange(12, 16);
        for (int i = 0; i < n; i++) {
            int x = brange(10, MW - 14), y = brange(3, MH - 10), len = brange(6, 9);
            for (int r = 0; r < 3; r++) for (int k = 0; k < len; k += 2) if (g_zg[y + r * 2][x + k] != '_') x_prop(x + k, y + r * 2, brnd() % 4 ? 'I' : 'x');
        }
        if (x_nb) { x_place('x', 0, 0, 0); x_place('x', 0, 0, 0); }
    } else if (type == XT_MINE) {   // bocas de mina (tres casillas con vigas), vagonetas sobre las vías y montones de escoria
        n = brange(7, 10);
        for (int i = 0; i < n; i++) for (t = 0; t < 40; t++) if (x_block('w', brange(10, MW - 8), brange(5, MH - 6), 3, 1)) break;
        for (int k = 0; k < 2; k++) for (int i = 0; i < 2; i++) x_try(x_rail_x[k] + brange(2, x_rail_l[k] - 3), x_rail_y[k], '1', 0);
        n = brange(20, 30);
        for (int i = 0; i < n; i++) for (t = 0; t < 30; t++) if (x_block('2', brange(6, MW - 8), brange(3, MH - 4), 2, 1)) break;
    }
    n = type == XT_GRAVE ? brange(16, 26) : brange(12, 20);
    for (int i = 0; i < n; i++) {   // rejas con huecos
        int len = brange(4, 9), x = brange(6, MW - 8), y = brange(3, MH - 4), hz = (int)(brnd() % 2);
        for (int k = 0; k < len; k++) if (brnd() % 6) x_prop(hz ? x + k : x, hz ? y : y + k, 'f');
    }
    for (int i = 0; i < 15; i++) {   // montones de sacos
        int x = brange(6, MW - 8), y = brange(3, MH - 4);
        for (int k = 0; k < 3; k++) x_prop(x + k, y, 's');
    }
    n = brange(16, 26);
    for (int i = 0; i < n; i++) for (t = 0; t < 20; t++) if (x_prop(brange(4, MW - 5), brange(3, MH - 4), 'b')) break;
    n = brange(15, 21);
    for (int i = 0; i < n; i++) for (t = 0; t < 20; t++) if (x_prop(brange(4, MW - 5), brange(3, MH - 4), 'F')) break;
    n = type == XT_FOREST ? brange(8, 13) : brange(24, 32);
    for (int i = 0; i < n; i++)
        for (t = 0; t < 60; t++) {
            int x = brange(4, MW - 5), y = brange(3, MH - 4);
            if (type != XT_FOREST && g_zg[y][x + 1] != '=' && g_zg[y][x - 1] != '=' && g_zg[y + 1][x] != '=' && g_zg[y - 1][x] != '=') continue;
            if (x_prop(x, y, 'L')) break;
        }
    XPH(3);
    // los campamentos de bandidos (hasta 2), cada uno junto a un fuego, lejos de la entrada y del otro, y con sitio
    int camp = (fl & XF_BANDITS) || (d >= 2 && (int)(brnd() % 100) < (d >= 5 ? 60 : 40) / (type == XT_GRAVE ? 3 : 1)), ncamp = 0, campx[3] = {0, 0, 0}, campy[3] = {0, 0, 0};
    int camp_want = camp ? 1 + ((d >= 2 || (fl & XF_BANDITS)) && (int)(brnd() % 100) < 60) + (d >= 4 && (int)(brnd() % 100) < 45) : 0;   // [mapas] hasta 3
    for (int c = 0; c < camp_want; c++) {
        int cx = 0, cy = 0, placed = 0;
        for (t = 0; t < 800 && !placed; t++) {
            cx = brange(8, MW - 12); cy = brange(5, MH - 6);
            int mind = t < 500 ? 900 : t < 700 ? 400 : 196, r = t < 500 ? 2 : 1;
            int ok = (cx - x_ex) * (cx - x_ex) + (cy - x_ey) * (cy - x_ey) >= mind;
            for (int o = 0; o < ncamp; o++) ok = ok && (cx - campx[o]) * (cx - campx[o]) + (cy - campy[o]) * (cy - campy[o]) >= 1600;   // [mapas]
            for (int j = cy - r; j <= cy + r && ok; j++)
                for (int i = cx - r; i <= cx + r && ok; i++)
                    if (!x_free(i, j) || !x_reach[j][i] || g_zg[j][i] == '~' || g_zg[j][i] == '_' || g_zg[j][i] == 'D' || g_zg[j][i] == 'Z' || x_keep[j][i]) ok = 0;
            if (ok && x_try(cx, cy, 'F', 0)) placed = 1;
        }
        if (!placed) continue;
        campx[ncamp] = cx; campy[ncamp] = cy; ncamp++;
        x_try(cx - 2, cy - 2, (int)(brnd() % 100) < 20 + 8 * d ? 'r' : 'c', 1);
        x_try(cx + 2, cy - 2, 'c', 1);
        x_try(cx - 2, cy + 2, 's', 0);
        x_try(cx + 2, cy + 2, 's', 0);
    }
    XPH(4);
    // cajones: uno por edificio y el resto repartido; con más peligro, más cajones buenos
    int nl = brange(36, 56), lootn = x_nt;   // [mapas]   // los del campamento ya cuentan
    for (int b = 0; b < x_nb && lootn < nl; b++) lootn += x_place((int)(brnd() % 100) < 6 + 4 * d + mn_rich() + ((type == XT_GRAVE || type == XT_PINE) && b ? 40 : 0) ? 'r' : 'c', b, 0, 1);   // los mausoleos guardan mejor botín
    for (t = 0; lootn < nl && t < 1500; t++) lootn += x_place((int)(brnd() % 100) < 6 + 4 * d + mn_rich() ? 'r' : 'c', t % 3 ? -2 : -1, 8, 1);
    // más lejos de la entrada, mejor botín: algunos cajones comunes pasan a ser buenos según su distancia
    for (int i = 0; i < x_nt; i++)
        if (g_zo[x_ty[i]][x_tx[i]] == 'c') {
            int fd = (x_tx[i] - x_ex) * 100 / (MW - 8);
            if (fd > 0 && (int)(brnd() % 100) < fd * 35 / 100) g_zo[x_ty[i]][x_tx[i]] = 'r';
        }
    // escaleras: 1 o 2 de los edificios más grandes tienen una, con 1 a 3 pisos arriba (ver floor_gen)
    {
        int ord[MAXB_X], want = 1 + (int)(brnd() % 2);
        for (int i = 0; i < x_nb; i++) ord[i] = i;
        for (int i = 1; i < x_nb; i++)
            for (int k = i; k > 0; k--) {
                const XBld *a = &x_b[ord[k]], *b = &x_b[ord[k - 1]];
                if ((a->x1 - a->x0) * (a->y1 - a->y0) <= (b->x1 - b->x0) * (b->y1 - b->y0)) break;
                int tmp = ord[k]; ord[k] = ord[k - 1]; ord[k - 1] = tmp;
            }
        for (int i = 0; i < x_nb && i < 4 && g_nstb < want; i++)
            if ((x_b[ord[i]].x1 - x_b[ord[i]].x0 - 1) * (x_b[ord[i]].y1 - x_b[ord[i]].y0 - 1) >= 6 && x_place('3', ord[i], 0, 1)) {   // el piso necesita al menos 3x2 casillas de interior
                g_stb_bi[g_nstb] = ord[i]; g_stb_nf[g_nstb] = 1 + (int)(brnd() % 3); g_nstb++; }
    }
    urbe_mouths();   // [urbe] bocas de metro (solo en la ciudad)
    // [mapas] segundo piso chico en la mayoría de las casas medianas (las grandes ya tienen sus pisos 2 a 4)
    for (int b = 0; b < x_nb && g_nhs < HS_MAX; b++) {
        int big = 0, w = x_b[b].x1 - x_b[b].x0 - 1, h = x_b[b].y1 - x_b[b].y0 - 1;
        for (int i = 0; i < g_nstb; i++) if (g_stb_bi[i] == b) big = 1;
        if (big || w < 3 || h < 2 || w * h < 6 || (int)(brnd() % 100) >= 78) continue;
        if (x_place('3', b, 0, 1)) g_hs_bi[g_nhs++] = b;
    }
    // las cosas especiales
    if (fl & XF_JOB) {   // el cajón del trabajo, en el edificio más lejano de la entrada
        unsigned char usedb[MAXB_X]; memset(usedb, 0, sizeof(usedb)); int placed = 0;   // [arregla] antes era una máscara de 64 bits   // [urbe]
        for (int k = 0; k < x_nb && !placed; k++) {
            int bi = -1, bd = -1;
            for (int b = 0; b < x_nb; b++) {
                int mx = (x_b[b].x0 + x_b[b].x1) / 2 - x_ex, my = (x_b[b].y0 + x_b[b].y1) / 2 - x_ey, dd = mx * mx + my * my;
                if (!usedb[b] && dd > bd) { bd = dd; bi = b; }
            }
            if (bi < 0) break;
            usedb[bi] = 1;
            placed = x_place('X', bi, 0, 1);
        }
        if (!placed) x_place('X', -2, 20, 1);
    }
    if ((fl & XF_SAWED) && x_nb && !x_place('W', (int)(brnd() % (u32)x_nb), 0, 1)) x_place('W', -1, 8, 1);
    if ((fl & XF_HATCH) && !x_place('H', -2, 40, 1)) x_place('H', -2, 10, 1);   // la escotilla, de preferencia lejos de la entrada
    if (fl & XF_MASKBODY) x_body('M');
    n = brange(14, 20);   // [mapas]
    for (int i = 0; i < n; i++) x_body('z');
    for (int b = 0; b < x_nb; b++) if (brnd() % 3 == 0) x_place('q', b, 0, 0);   // catres
    // por si algo quedó cortado: se quita
    x_n = x_flood();
    for (int i = 0; i < x_nt; i++)
        if (!x_near(x_tx[i], x_ty[i])) { g_zo[x_ty[i]][x_tx[i]] = ' '; g_x_fix++; x_n = x_flood(); }
    XPH(5);
    // enemigos: lejos de la entrada, en grupos, más y peores según el peligro
    int nz = 26 + 3 * d + (type == XT_GRAVE ? 8 : 0), nbig = 0;   // el cementerio: más zombis y pocos humanos
    nz = nz * 3 / 2; if (nz > 84) nz = 84;   // [mapas]
    if (fl & XF_BEAST) x_enemy('J', x_px, x_py, 5);
    if (fl & XF_BOSS2) {   // Cabeza de Hierro: lejos de la entrada
        for (t = 0; t < 200; t++) {
            int gx = brange(6, MW - 7), gy = brange(5, MH - 6);
            if ((gx - x_ex) * (gx - x_ex) + (gy - x_ey) * (gy - x_ey) < 900 && t < 150) continue;
            if (x_enemy('0', gx, gy, 3)) break;
        }
    }
    if (x_pud) {   // junto al charco nacen irradiados
        n = 2 + d / 3;
        for (int i = 0; i < n; i++) x_enemy(brnd() % 4 == 0 && d >= 2 ? 'm' : 'd', x_px, x_py, 5);
    }
    for (int c = 0; c < ncamp; c++) { n = 2 + d / 3; if ((fl & XF_BANDITS) && n < 4) n = 4; if (n > 5) n = 5; for (int i = 0; i < n; i++) x_enemy((d >= 4 || g_fjob == 3) && i == 1 ? '8' : 'a', campx[c], campy[c], 3); }   // y, de peligro 4, un francotirador por campamento

    // grupos por sector (4 columnas por 2 filas); cuanto más lejos de la entrada, más grandes y peores
    for (t = 0; nz > 0 && t < 1500; t++) {
        int sx = t % 6, sy = (t / 6) % 3, bw = (MW - 8) / 6, bh = (MH - 6) / 3;   // [mapas]
        int gx = t < 900 ? 4 + sx * bw + brange(0, bw - 1) : brange(4, MW - 5), gy = t < 900 ? 3 + sy * bh + brange(0, bh - 1) : brange(3, MH - 4), gs = brange(2, 4);
        if (!x_free(gx, gy) || !x_reach[gy][gx] || (gx - x_ex) * (gx - x_ex) + (gy - x_ey) * (gy - x_ey) < 196) continue;
        int fd = (gx - x_ex) * 100 / (MW - 8);   // 0 junto a la entrada, ~100 en el extremo opuesto
        if (fd < 0) fd = 0;
        if (fd > 55) gs++;
        int pack = d >= 1 && (int)(brnd() % 100) < 3 + 2 * d + fd / 16;   // una jauría de corredores: llegan todos juntos
        if (pack) gs++;
        for (int k = 0; k < gs && nz > 0; k++) {
            int r = (int)(brnd() % 100);
            char ch = 'd';
            if (pack) ch = '5';
            else if ((d >= 3 || fd > 60) && nbig < 2 + d / 2 && r < (d > 2 ? (d - 2) * 5 : 0) + fd / 6 + 2) { ch = 'B'; nbig++; }
            else if (r < 8 + 3 * d + fd / 4 + (type == XT_GRAVE ? 12 : 0)) ch = 'm';
            if (ch == 'd' || ch == 'm') {   // los nuevos: escupidores, acorazados, francotiradores y, en la nieve, lobos blancos
                int q = (int)(brnd() % 100);
                int ps = d >= 2 ? 3 + 2 * d + fd / 12 + (type == XT_FOREST || type == XT_GRAVE ? 5 : 0) : 0;
                int pa = d >= 3 ? 2 + d + fd / 20 + (type == XT_FACTORY || type == XT_TRAIN ? 6 : 0) : 0;
                int pn = d >= 4 ? d / 2 + fd / 25 : 0;
                if (((fl & XF_SNOW) || type == XT_PINE) && q < 35 + fd / 4) ch = '9';   // [armas] en el bosque de pinos hay lobos aunque no nieve
                else if (q < ps) ch = '6';
                else if (q < ps + pa) ch = '7';
                else if (q < ps + pa + pn) ch = '8';
            }
            if (x_enemy(ch, gx, gy, 2)) nz--;
        }
    }
    m3_enemies(type);   // [mapas3]
    XPH(6);
    g_ret_tx = x_ex; g_ret_ty = x_ey;
    ev_gen();
    g_x_nen = x_nen;
    for (int i = 0, n2 = g_nstb; i < n2; i++) {   // dónde quedó cada escalera (si la limpieza de arriba se llevó alguna, ese edificio no tiene pisos)
        const XBld *R = &x_b[g_stb_bi[i]];
        int fx = -1, fy = -1;
        for (int y = R->y0; y <= R->y1; y++) for (int x = R->x0; x <= R->x1; x++) if (g_zo[y][x] == '3') { fx = x; fy = y; }
        if (fx < 0) { for (int k = i; k < g_nstb - 1; k++) { g_stb_bi[k] = g_stb_bi[k + 1]; g_stb_nf[k] = g_stb_nf[k + 1]; } g_nstb--; n2--; i--; continue; }
        g_stb_x[i] = fx; g_stb_y[i] = fy;
    }
    for (int i = 0, n2 = g_nhs; i < n2; i++) {   // [mapas] y la escalera de cada casa (si algo se la llevó, esa casa no tiene piso)
        const XBld *R = &x_b[g_hs_bi[i]];
        int fx = -1, fy = -1;
        for (int y = R->y0; y <= R->y1; y++) for (int x = R->x0; x <= R->x1; x++) if (g_zo[y][x] == '3') { fx = x; fy = y; }
        if (fx < 0) { for (int k = i; k < g_nhs - 1; k++) g_hs_bi[k] = g_hs_bi[k + 1]; g_nhs--; n2--; i--; continue; }
        g_hs_x[i] = fx; g_hs_y[i] = fy;
    }
}

// El piso de arriba (zonas 3 a 8: 3 por edificio con escalera, pisos 2 a 4): un interior generado dentro del contorno del edificio,
// con un tabique (y a veces un segundo) con puertas, escalera de bajar contra el muro de arriba, escalera de subir si no es el último
// piso, cajones (mejor botín cuanto más alto; el último trae dos cajones buenos) y enemigos. Sale de la semilla de la expedición, el
// edificio y el piso, así que se arma igual cada vez. Usa las mismas funciones x_* de la expedición para garantizar que todo se alcance
static void floor_build(int bi, int salt, int p, int top, int house);
static void floor_gen(int z)
{
    exped_gen();   // otra vez, solo para saber cuáles son los edificios con escalera (con la misma semilla sale igual)
    if (z == ZH) {   // [mapas] el segundo piso de una casa
        if (g_hs_cur < 0 || g_hs_cur >= g_nhs) g_hs_cur = 0;
        if (g_nhs > 0) { floor_build(g_hs_bi[g_hs_cur], 100 + g_hs_cur, 2, 2, 1); return; }
        g_nstb = 1; g_stb_bi[0] = 0; g_stb_nf[0] = 1; z = 3;   // sin casas (no debería pasar): un piso cualquiera
    }
    int b = (z - 3) / 3, p = (z - 3) % 3 + 2;
    if (b >= g_nstb) { b = 0; if (g_nstb == 0) { g_nstb = 1; g_stb_bi[0] = 0; g_stb_nf[0] = 1; } }
    int top = g_stb_nf[b] + 1;
    if (p > top) p = top;
    floor_build(g_stb_bi[b], b * 3 + p + 1, p, top, 0);
}
static void floor_build(int bi, int salt, int p, int top, int house)
{
    int iw, ih, ix0, iy0, ix1, iy1, xs, xm = -1, t;
    const XBld R = x_b[bi];
    g_fl_x0 = R.x0; g_fl_y0 = R.y0; g_fl_x1 = R.x1; g_fl_y1 = R.y1;
    ix0 = R.x0 + 1; ix1 = R.x1 - 1; iy0 = R.y0 + 1; iy1 = R.y1 - 1;
    iw = ix1 - ix0 + 1; ih = iy1 - iy0 + 1;
    g_brs = (g_x.seed ^ (0x9E3779B1u * (u32)salt)) | 1;
    memset(x_keep, 0, sizeof(x_keep));
    x_nt = x_nd = x_nen = 0;
    for (int y = 0; y < MH; y++) {
        memset(g_zg[y], '.', MW); g_zg[y][MW] = 0;
        memset(g_zo[y], '#', MW); g_zo[y][MW] = 0;
    }
    for (int y = iy0; y <= iy1; y++) for (int x = ix0; x <= ix1; x++) { g_zg[y][x] = '_'; g_zo[y][x] = ' '; }
    xs = ix0 + brange(0, 1);   // la escalera de bajar contra el muro de arriba; se llega por la casilla de abajo
    x_ex = xs; x_ey = iy0 + 1;
    x_rx = x_ry = 1; x_emin2 = iw * ih < 12 ? 1 : 9;   // en los pisos chicos, los enemigos pueden nacer más cerca de la escalera
    for (t = 0; t < 2; t++) {   // el segundo intento es sin tabiques, por si el primero cortó el paso
        if (t == 0) {
            if (iw >= 6) {   // un tabique de arriba abajo con 1 o 2 puertas
                int g1 = brange(iy0, iy1), g2 = g1 + 2 <= iy1 ? g1 + 2 : g1 - 2 >= iy0 ? g1 - 2 : -1;
                xm = ix0 + brange(3, iw - 3);
                for (int y = iy0; y <= iy1; y++) g_zo[y][xm] = '#';
                g_zo[g1][xm] = ' '; g_zg[g1][xm] = 'D';
                if (g2 >= 0 && ih >= 4) { g_zo[g2][xm] = ' '; g_zg[g2][xm] = 'D'; }
                if (ih >= 5 && ix1 - xm >= 4) {   // y, a la derecha, un tabique corto de lado a lado con una puerta
                    int ym = brange(iy0 + 1, iy1 - 1);
                    if (ym != g1 && ym != g2) {
                        int gx = brange(xm + 1, ix1);
                        for (int x = xm + 1; x <= ix1; x++) g_zo[ym][x] = '#';
                        g_zo[ym][gx] = ' '; g_zg[ym][gx] = 'D';
                    }
                }
            }
        } else {
            for (int y = iy0; y <= iy1; y++) for (int x = ix0; x <= ix1; x++) { g_zg[y][x] = '_'; g_zo[y][x] = ' '; }
        }
        g_zo[iy0][xs] = '4';
        x_keep[x_ey][x_ex] = 1;
        x_n = x_flood();
        int cnt = 0;
        for (int y = iy0; y <= iy1; y++) for (int x = ix0; x <= ix1; x++) cnt += x_pass(g_zo[y][x]);
        if (x_n == cnt) break;   // todo el interior se alcanza
        g_zo[iy0][xs] = ' ';
    }
    // lámparas de techo: una por cada lado del tabique (o una al centro)
    if (xm >= 0) { bput((ix0 + xm - 1) / 2, (iy0 + iy1) / 2, '*'); bput((xm + 1 + ix1) / 2, (iy0 + iy1) / 2, '*'); }
    else bput((ix0 + ix1) / 2, (iy0 + iy1) / 2, '*');
    x_n = x_flood();
    // la escalera de subir (si hay otro piso encima) y los cajones
    if (p < top && !x_place('3', bi, 2, 1)) x_place('3', bi, 0, 1);
    x_place(p == top && !house ? 'r' : house && (int)(brnd() % 100) < 25 ? 'r' : 'c', bi, 0, 1);   // el primer cajón antes que los enemigos, para que en los pisos chicos nunca falte
    // enemigos antes que los demás cajones, para que en los pisos chicos siempre haya sitio: más y mejores cuanto más alto y más peligro
    x_n = x_flood();
    int ne = house ? ((int)(brnd() % 100) < 30 + 3 * g_x.danger ? 1 + (int)(brnd() % 2) : 0) : 2 + p + g_x.danger / 4;   // [mapas] en las casas, a veces
    if (ne > 7) ne = 7;
    if (ne > 1 + iw * ih / 5) ne = 1 + iw * ih / 5;   // en un piso chico tienen que sobrar casillas para los cajones
    for (t = 0; ne > 0 && t < 60; t++) {
        int r = (int)(brnd() % 100);
        char ch = r < 20 ? 'a' : r < 45 + 3 * g_x.danger ? 'm' : 'd';
        if (g_x.danger >= 3 && p == top && r > 92) ch = 'B';
        if (g_x.danger >= 2 && ch == 'd' && (int)(brnd() % 100) < 25) ch = '5';
        if (g_x.danger >= 4 && ch == 'm' && (int)(brnd() % 100) < 25) ch = '6';
        if (g_x.danger >= 4 && p == top && r < 6) ch = '7';
        if (x_enemy(ch, brange(ix0, ix1), brange(iy0, iy1), 2)) ne--;
    }
    for (int y = iy0; y <= iy1 && x_nen == 0 && !house; y++)   // por si el azar no encontró sitio: al menos uno, en cualquier casilla alcanzable que no sea la de llegada salvo la de llegada
        for (int x = ix0; x <= ix1 && x_nen == 0; x++)
            if (x_free(x, y) && x_reach[y][x] && (x - x_ex) * (x - x_ex) + (y - x_ey) * (y - x_ey) >= 1) { g_zo[y][x] = 'd'; x_nen++; }
    int nl = house ? 1 + (int)(brnd() % 3) : 2 + (iw * ih >= 24) + (p == top), rich = 30 + 15 * p;
    for (t = 1; t < nl; t++) x_place((int)(brnd() % 100) < rich ? 'r' : 'c', bi, 0, 1);
    if (p == top && !house) x_place('r', bi, 0, 1);   // el último piso: el mejor cajón, dos cajones buenos si caben
    for (int i = 0; i < 6 && i < (iw * ih) / 12; i++) x_place(brnd() % 3 ? 'b' : 's', bi, 0, 0);   // algún barril o saco, solo si sobra sitio
    // por si algo quedó cortado: se quita
    x_n = x_flood();
    for (int i = 0; i < x_nt; i++)
        if (!x_near(x_tx[i], x_ty[i])) { g_zo[x_ty[i]][x_tx[i]] = ' '; g_x_fix++; x_n = x_flood(); }
    x_rx = 6; x_ry = 5; x_emin2 = 169;
}

static void spawn_z(int z, int type, float x, float y)   // en el búnker todos los zombis nacen irradiados
{
    int n0 = g_ne;
    spawn_enemy(type, x, y);
    if (z == 2 && g_ne > n0) g_e[n0].irr = 1;
    if (z == 1) duro_extra_enemy(type, x, y);   // [duro]
}

// Lo que las mejoras cambian en el mapa de la base (zona 0). Se aplica al armar la zona (live = 0: sobre sus grillas de letras,
// antes de volcarlas) y al construir una mejora (live = 1: directo sobre las grillas vivas). Las letras son las de make_map.py
static int base_obj(char c)
{
    switch (c) {
    case 'R': return O_RUBBLE;
    case 'l': return O_CHEST2;
    case 'n': return O_SHELF;
    case 'u': return O_BED;
    case 'e': return O_CLINIC;
    case 'o': return O_TOWER;
    case 'p': return O_GARDEN;
    case 't': return O_TANK;
    case 'j': return O_BENCH2;
    }
    return O_NONE;
}
static void base_cell(int live, int x, int y, char oc)   // pone el objeto oc en la casilla (' ' la deja libre)
{
    g_zo[y][x] = oc;
    if (live) g_o[y][x] = base_obj(oc);
}
static void base_apply(int live)
{
    if (g_up[UP_STASH1]) base_cell(live, 11, 24, 'l');                              // el segundo arcón, junto al primero
    if (g_up[UP_STASH2]) { base_cell(live, 10, 26, 'n'); base_cell(live, 11, 26, 'n'); }   // la estantería, contra el muro del fondo
    if (g_up[UP_BED]) { base_cell(live, 14, 24, 'u'); base_cell(live, 38, 10, 'u'); }      // las dos camas, donde estaban los catres
    if (g_up[UP_CLINIC]) {                                                           // el cuarto tapado queda despejado y con su puesto
        for (int y = 13; y <= 15; y++)
            for (int x = 6; x <= 10; x++)
                if (g_zo[y][x] == 'R') base_cell(live, x, y, ' ');
        base_cell(live, 8, 16, ' ');
        base_cell(live, 8, 14, 'e');
    }
    if (g_up[UP_TOWER]) base_cell(live, 3, 16, 'o');                                 // la torre, junto al portón
    if (g_up[UP_GARDEN]) {                                                           // el charco queda sellado: tierra, huerto y colector
        for (int y = 27; y <= 36; y++)
            for (int x = 46; x <= 60; x++)
                if (g_zg[y][x] == '~') { g_zg[y][x] = '.'; if (live) g_g[y][x] = G_DIRT; }
        for (int x = 51; x <= 57; x++) { base_cell(live, x, 29, 'p'); base_cell(live, x, 33, 'p'); }
        base_cell(live, 49, 31, 't');
    }
    if (g_up[UP_BENCH]) { base_cell(live, 44, 13, 'j'); base_cell(live, 43, 14, 'j'); }   // el banco se agranda
}

// Si una casilla nueva quedó justo bajo el jugador, lo corre a la libre más cercana
static void unstick_player(void)
{
    int tx = (int)(P.x / TS), ty = (int)(P.y / TS);
    if (!solid(tx, ty)) return;
    for (int r = 1; r <= 3; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (!solid(tx + dx, ty + dy)) { P.x = (tx + dx) * TS + 8; P.y = (ty + dy) * TS + 8; return; }
}

// Arma la zona z en las grillas y listas vivas (suelo, objetos, cajones, enemigos y luces), como si fuera nueva
// Anomalías: campos de 2 a 3 anomalías de una misma clase, repartidos por la expedición (1 a 4 campos según el peligro). Se arman
// con la semilla de la expedición, así que salen iguales cada vez. Nunca sobre la entrada, los puntos de retirada, las escaleras,
// la escotilla ni los cajones, y siempre en terreno abierto de 5 x 5 casillas (no tapan ningún paso). Junto a cada campo nacen
// uno o dos artefactos, dentro o al borde (ITEM_ART0..5). Cómo actúan y se dibujan: anom_update y anom_draw
enum { AN_BRASERO, AN_CHISPA, AN_REMOLINO, AN_KINDS };
static const char *const AN_NAME[AN_KINDS] = {"Brasero", "Chispa", "Remolino"};
typedef struct { float x, y, r, cd, vis, zap; int kind, field, ping; } Anom;   // vis: cuánto más se ve (s); zap: destello en curso; ping: una tuerca cayó cerca
#define MAXAN 16
static Anom g_an[MAXAN];
static int g_nan, g_nfield;
static u32 an_s;
static u32 an_rnd(void) { an_s ^= an_s << 13; an_s ^= an_s >> 17; an_s ^= an_s << 5; return an_s; }
static float an_rf(float a, float b) { return a + (b - a) * ((an_rnd() >> 8) % 1000) / 1000.0f; }

// ¿Sirve el punto (cx, cy) como centro de un campo?
static int an_spot_ok(int cx, int cy)
{
#define AN_FAR(ax, ay, t) (((ax) - cx) * ((ax) - cx) + ((ay) - cy) * ((ay) - cy) >= (t) * (t))
    if (!x_reach[cy][cx]) return 0;
    for (int y = cy - 2; y <= cy + 2; y++)
        for (int x = cx - 2; x <= cx + 2; x++) {
            if (!in_map(x, y) || solid(x, y)) return 0;
            int g = g_g[y][x];
            if (g == G_FLOOR || g == G_DOOR || g == G_ZONE || g == G_EXIT || g == G_RAD) return 0;
        }
    if (!AN_FAR(g_ret_tx, g_ret_ty, 14) || !AN_FAR(g_ret2_tx, g_ret2_ty, 9) || (g_ret3_tx >= 0 && !AN_FAR(g_ret3_tx, g_ret3_ty, 9))) return 0;   // [mapas]
    if (g_hatch_tx >= 0 && !AN_FAR(g_hatch_tx, g_hatch_ty, 8)) return 0;
    for (int i = 0; i < g_nev; i++) if (!AN_FAR(g_ev[i].tx, g_ev[i].ty, 9)) return 0;
    if (g_dogx >= 0 && !AN_FAR(g_dogx, g_dogy, 9)) return 0;
    for (int i = 0; i < g_nstb; i++) if (!AN_FAR(g_stb_x[i], g_stb_y[i], 8)) return 0;
    for (int c = 0; c < g_ncr; c++) if (!AN_FAR(g_cr[c].tx, g_cr[c].ty, 6)) return 0;
    for (int q = 0; q < g_nan; q++) if (!AN_FAR((int)(g_an[q].x / TS), (int)(g_an[q].y / TS), 9)) return 0;
    return 1;
#undef AN_FAR
}

static void anom_gen(void)
{
    g_nan = g_nfield = 0;
    memset(g_an, 0, sizeof(g_an));
    an_s = (g_x.seed ^ 0xA7C3F1u) | 1;
    int nf = 1 + g_x.danger / 3;
    for (int tries = 0; g_nfield < nf && tries < 600; tries++) {
        int cx = 8 + (int)(an_rnd() % (u32)(MW - 16)), cy = 6 + (int)(an_rnd() % (u32)(MH - 12));
        if (!an_spot_ok(cx, cy)) continue;
        int kind = (int)((an_rnd() >> 4) % AN_KINDS), n = 2 + (int)((an_rnd() >> 4) % 2), first = g_nan;
        for (int i = 0; i < n && g_nan < MAXAN; i++) {
            float ox = i ? an_rf(-16, 16) : 0, oy = i ? an_rf(-16, 16) : 0;
            g_an[g_nan++] = (Anom){cx * TS + 8 + ox, cy * TS + 8 + oy, an_rf(20, 26), an_rf(0, 3), 0, 0, kind, g_nfield, 0};
        }
        int na = 1 + (int)((an_rnd() >> 4) % 2);
        for (int k = 0; k < na; k++) {
            const Anom *a = &g_an[first + (int)((an_rnd() >> 4) % (u32)(g_nan - first))];
            float ang = an_rf(0, 6.283f), rr = (k & 1) ? a->r + 5 : an_rf(4, 12);
            float px = a->x + cosf(ang) * rr, py = a->y + sinf(ang) * rr;
            if (box_hits(px, py, 3)) { px = a->x; py = a->y; }
            drop(px, py, artef_pick(an_rnd() >> 4), 1);   // [artef]
        }
        g_nfield++;
    }
}

static void ev_spawn(void);
static void farol_hostile(void);
static void faro_open(void);
static int farol_field_try(void);
// [mil] Convierte cajones comunes en cajas militares: 2 a 4 (más con el peligro; el doble en zonas de jefe), de preferencia junto a
// bandidos, dentro de edificios y lejos de la entrada. Elige por posición (sin azar) para que la misma semilla dé el mismo mapa.
static void mil_place(void)
{
    int want = 2 + g_x.danger / 3 + (int)(hash2(g_ncr, g_x.danger + 7) % 2);
    if (want > 4) want = 4;
    if (g_x.type == XT_NUC) want = 9 + (int)(hash2(g_ncr, 5) % 3);   // [mapnuc] la planta: muchas cajas militares
    if (g_x.flags & (XF_ZJEFE | XF_ZBASE)) want *= 2;
    for (int n = 0; n < want; n++) {
        int best = -1, bs = -1;
        for (int i = 0; i < g_ncr; i++) {
            const Crate *c = &g_cr[i];
            if (c->kind != CK_LOOT || c->open) continue;
            int sc = abs(c->tx - g_ret_tx) + abs(c->ty - g_ret_ty);
            for (int b = 0; b < x_nb; b++) if (c->tx >= x_b[b].x0 && c->tx <= x_b[b].x1 && c->ty >= x_b[b].y0 && c->ty <= x_b[b].y1) { sc += 40 + (x_b[b].x1 - x_b[b].x0) * (x_b[b].y1 - x_b[b].y0) / 2; break; }
            for (int e = 0; e < g_ne; e++) if (g_e[e].type == E_BAN && fabsf(g_e[e].x / TS - c->tx) + fabsf(g_e[e].y / TS - c->ty) < 10) { sc += 60; break; }
            sc += (int)(hash2(c->tx, c->ty) % 25);
            if (sc > bs) { bs = sc; best = i; }
        }
        if (best < 0) break;
        g_cr[best].kind = CK_MIL;
    }
}

static void zone_build(int z)
{
    // la expedición y los pisos de sus edificios (que usan las mismas coordenadas) miden 128 x 80; la base y el búnker, 64 x 40
    g_mw = z == 1 || (z >= 3 && z != ZB) ? 192 : 64;   // [mapas]
    g_mh = z == 1 || (z >= 3 && z != ZB) ? 120 : 40;   // [mapas]
    if (z == ZB) bk_gen();   // [bunker]
    else if (z == 2) { if (g_pz >= 2) { u32 sv = g_bseed; g_bseed = ((u32)g_pz * 2654435761u ^ 0x9E3779B9u) | 1; bunker_gen(); g_bseed = sv; } else bunker_gen(); pozo_decor(); }   // [pozo]
    else if (z == 1) exped_gen();
    else if (z == ZM) metro_gen();   // [urbe]
    else if (z >= 3 && z != ZB) floor_gen(z);
    else {
        for (int y = 0; y < MH; y++) {
            memcpy(g_zg[y], ZMAP_G[z][y], MW); g_zg[y][MW] = 0;
            memcpy(g_zo[y], ZMAP_O[z][y], MW); g_zo[y][MW] = 0;
        }
        if (z == 0) base_apply(0);   // las mejoras hechas cambian la base
    }
    memset(g_b, 0, sizeof(g_b));
    memset(g_p, 0, sizeof(g_p));
    memset(g_k, 0, sizeof(g_k));
    memset(g_pop, 0, sizeof(g_pop));
    g_ne = g_ncr = g_nd = g_dnext = g_nfire = g_nlamp = g_ndec = 0;
    memset(g_pud, 0, sizeof(g_pud)); g_swing_t = 0;
    memset(g_thr, 0, sizeof(g_thr)); memset(g_trp, 0, sizeof(g_trp)); memset(g_fir, 0, sizeof(g_fir)); memset(g_boom, 0, sizeof(g_boom));
    memset(g_foot, 0, sizeof(g_foot)); g_nfoot = 0;
    g_has_chof = 0;
    g_hatch_tx = g_hatch_ty = -1;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            char gc = g_zg[y][x], oc = g_zo[y][x];
            float cx = x * TS + TS / 2, cy = y * TS + TS / 2;
            g_g[y][x] = gc == ',' ? G_GRASS : gc == '=' ? G_ROAD : gc == '_' ? G_FLOOR : gc == 'D' ? G_DOOR :
                        gc == 'r' ? G_RAIL : gc == '~' ? G_RAD : gc == 'E' ? G_EXIT : gc == 'Z' ? G_ZONE : G_DIRT;
            int o = O_NONE;
            switch (oc) {
            case '#': o = O_WALL; break;
            case 'C': o = O_CONT; break;
            case 'K': o = O_CONT2; break;
            case 'f': o = O_FENCE; break;
            case 'T': o = O_TREE; break;
            case 's': o = O_SAND; break;
            case 'b': o = O_BARREL; break;
            case 'V': o = O_CAR_L; break;
            case 'v': o = O_CAR_R; break;
            case 'c': case 'X': case 'W': case 'r': case 'N': case 'Q':
                o = O_CRATE;
                g_cr[g_ncr++] = (Crate){x, y, oc == 'X' ? CK_JOB : oc == 'W' ? CK_WEAPON : oc == 'r' ? CK_RICH : oc == 'N' ? CK_MEDS :
                                        oc == 'Q' ? CK_REG : CK_LOOT, 0};
                break;
            case 'F': o = O_FBARREL; if (g_nfire < MAXFIRE) { g_fire[g_nfire][0] = cx; g_fire[g_nfire][1] = cy; g_nfire++; } break;
            case 'L': o = O_LAMP; if (g_nlamp < MAXLAMP) g_lamp[g_nlamp++] = (Lamp){cx, cy, 0, hash2(x, y) % 3 == 0}; break;
            case '*': if (g_nlamp < MAXLAMP) g_lamp[g_nlamp++] = (Lamp){cx, cy, 1, z == 2 ? hash2(x, y) % 2 == 0 : hash2(x, y) % 4 == 0}; break;
            case 'x': o = O_CROSS; break;
            case 'i': o = O_STAKE; break;
            case 'q': o = O_COT; break;
            case 'H': o = O_HATCH; g_hatch_tx = x; g_hatch_ty = y; break;
            case 'S': o = O_STAIRS; break;
            case 'G': o = O_MACH; break;
            case 'O': o = O_STANK; break;
            case 'I': o = O_TOMB; break;
            case 'w': o = O_MINE; break;
            case '1': o = O_CART; break;
            case '2': o = O_SLAG; break;
            case '3': o = O_STUP; break;
            case '4': o = O_STDN; break;
            case '[': o = O_UBUS; break;   // [qurbe]
            case ']': o = O_UPAR; break;
            case '|': o = O_UKIO; break;
            case 'Y': o = O_MERCH; break;
            case 'y': o = O_SCAV; break;
            case 'h': o = O_BENCH; break;
            case 'A': o = O_CHEST; break;
            case 'R': case 'l': case 'n': case 'u': case 'e': case 'o': case 'p': case 't': case 'j': o = base_obj(oc); break;   // escombros y mejoras de la base
            case 'U': o = O_NPC; g_has_chof = 1; g_chof_tx = x; g_chof_ty = y; break;
            case 'k': if (g_ndec < MAXDEC) g_dec[g_ndec++] = (Decor){cx, cy, S_SKEL}; break;
            case 'z': g_cr[g_ncr++] = (Crate){x, y, CK_BODY, 0}; break;
            case 'M': g_cr[g_ncr++] = (Crate){x, y, CK_MASK, 0}; break;
            case 'P': P.x = cx; P.y = cy; break;
            case 'd': spawn_z(z, E_DOG, cx, cy); break;
            case 'm': spawn_z(z, E_MAR, cx, cy); break;
            case 'B': spawn_z(z, E_BIG, cx, cy); break;
            case '5': spawn_z(z, E_RUN, cx, cy); break;
            case '6': spawn_z(z, E_SPIT, cx, cy); break;
            case '7': spawn_z(z, E_ARM, cx, cy); break;
            case '8': spawn_enemy(E_SNP, cx, cy); break;
            case '9': spawn_enemy(E_WOLF, cx, cy); break;
            case '0': spawn_enemy(E_BOSS2, cx, cy); break;
            case '!': spawn_enemy(E_VIUDA, cx, cy); jz_reserve(E_WOLF, 7, cx, cy); break;   // [jefes2] [arregla] lobos reservados
            case '%': spawn_enemy(E_CAPATAZ, cx, cy); jz_reserve(E_BAN, 3, cx, cy); break;   // [jefes2] [arregla] refuerzos reservados
            case '$': spawn_enemy(E_FERMEN, cx, cy); jz_reserve(E_FERMEN, 1, cx, cy); break;   // [jefes2] [arregla] mitad reservada
            case 'a': spawn_enemy(E_BAN, cx, cy); break;
            case 'g': spawn_enemy(E_GRD, cx, cy); break;
            default: o = m3_obj_of(oc); break;   // [mapas3]
            case 'J':   // La Bestia y, escondidos, los dos zombis chicos que llama
                spawn_enemy(E_BOSS, cx, cy);
                for (int k = 0; k < 2; k++) {
                    spawn_enemy(E_DOG, cx, cy);
                    if (g_ne > 0) { Enemy *r = &g_e[g_ne - 1]; r->summon = 1; r->irr = 1; r->hp = 0; r->state = ES_DEAD; }
                }
                break;
            }
            g_o[y][x] = o;
        }
    for (int i = 0; i < g_ncr; i++) {   // la caja y el registro que ya tienes o entregaste no se vuelven a ofrecer
        if (g_cr[i].kind == CK_MEDS && (g_has_meds || g_job_done6)) g_cr[i].open = 1;
        if (g_cr[i].kind == CK_REG && (g_has_reg || g_job_done7)) g_cr[i].open = 1;
    }
    if (z == 1) mil_place();   // [mil]
    if (z == 1) mj_place();   // [armas2] minijefes sueltos
    if (z == 1) { dz_post(); urbe_post_surface(); }   // [diseno] mobiliario de calle   // [urbe]
    if (z == ZM) urbe_post_metro();   // [urbe]
    if (z == ZB) bk_post(); else if (z == 0) bk_kolos_hatch();   // [bunker]
    if (z == 1) { ev_spawn(); anom_gen(); } else { g_nan = g_nfield = 0; }   // las anomalías son solo de las expediciones
    pozo_post(z);   // [pozo]
    bake_lights();
}

#ifdef PEPEL_DEBUG
// Pruebas: si existe el archivo dbg_all junto a la partida, la partida nueva empieza con todas las armas, la mochila al tope y sin daño
static void dbg_all_give(void)
{
    char path[170];
    snprintf(path, sizeof(path), "%s", g_sav);
    char *sl = strrchr(path, '/');
    if (!sl) return;
    strcpy(sl + 1, "dbg_all");
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return;
    sceIoClose(fd);
    for (int i = 0; i < W_COUNT; i++) { g_w[i].owned = 1; g_w[i].mag = w_mag(i); g_w[i].cond = 0.9f; }
    for (int i = 0; i < ITEM_COUNT; i++) {
        int gi = gear_of(i);
        if (gi >= 0) { if (!gear_owned(gi)) { g_bag[i] = 1; g_gcond[gi] = 100; } }
        else if (ITEMS[i].cap) g_bag[i] = ITEMS[i].cap;
    }
    g_dbg_god = 1;
    dbg_scenario();
}
#endif

static void new_game(void)
{
    armas_reset();   // [armas]
    memset(&P, 0, sizeof(P));
    memset(g_zc, 0, sizeof(g_zc));
    memset(&g_x, 0, sizeof(g_x));
    g_xtotal = 0; g_boss_dead = 0; g_hora = 17;   // [diseno]
    memset(&g_dog, 0, sizeof(g_dog)); g_nev = 0; g_dogx = -1; g_ev_t = 0;
    memset(g_up, 0, sizeof(g_up));
    duro_reset();   // [duro]
    g_clinic_used = g_ret_pend = 0; g_xs_t = 0; g_xs_kills = g_xs_open = 0;
    g_zone = 0;
    zone_build(0);
    memset(g_stop_seen, 0, sizeof(g_stop_seen));
    g_stop_seen[0] = 1;
    g_trav_sel = 0;
    P.hp = 100;
    P.stam = 100;
    P.hunger = P.thirst = 100;
    P.lock = -1;
    P.sq_t = -1;
    memset(g_w, 0, sizeof(g_w));
    g_w[W_MAKAROV] = (WeaponState){1, 8, 0.92f};
    g_w[W_KNIFE] = (WeaponState){1, 0, 0.95f};   // se parte con el cuchillo
    g_w[W_SAWED] = (WeaponState){0, 0, 0.80f};
    g_w[W_AK] = (WeaponState){0, 0, 1.0f};
    memset(g_bag, 0, sizeof(g_bag));
    memset(g_eq, 0, sizeof(g_eq));
    memset(g_gcond, 0, sizeof(g_gcond));
    g_bag[ITEM_9MM] = 16; g_bag[ITEM_BAND] = 2; g_bag[ITEM_FOOD] = 1; g_bag[ITEM_WATER] = 1;
    peso_new_game();   // [peso] partida nueva: solo el cuchillo, 1 venda, 1 conserva y 1 agua
    g_rep = 0;
    g_storm_t = 95; g_storm_on = 0; g_storm_warned = 0;
    g_bomba_t = 0;   // [bomba]
    pozo_clear(); g_zc2_fl = 0;   // [arregla] los pisos del Pozo no heredan lo de la partida anterior
    g_cold = 0;
    g_kills = g_opened = 0;
    g_job = g_job_n = g_job_done1 = g_job_done5 = g_kol_sel = g_kol_top = 0; g_j2done = 0; g_uniq = 0;   // [jefes2] [artef]
    g_has_meds = g_has_reg = g_job_done6 = g_job_done7 = g_fin_seen = g_jobs_done = g_scrap_gain = 0;
    memset(g_sk, 0, sizeof(g_sk)); g_frep = g_fjob = g_fjn = g_fdone = g_notes = g_note_new = 0;
    g_bseed = rnd() | 1; g_bpend = 0; g_bstale = 0;
    g_pz = g_pz_lift = g_pz_inf = 0;   // [pozo]
    shop_restock();
    g_shop_sel = g_shop_top = 0;
    memset(g_stash, 0, sizeof(g_stash));
    bk_reset();   // [bunker]
    g_stash_sel = g_stash_top = g_stash_col = 0;
    g_inv_sel = g_inv_top = g_inv_focus = g_inv_eq = 0;
    g_play_t = 0;
    g_shake = 0;
    g_rest = 0;
    memset(g_seen, 0, sizeof(g_seen));
    g_seen_tx = -1;
    explore();
    g_was_in = indoors(P.x, P.y);
    g_camx = clampf(P.x - SCR_W / 2, 0, WORLD_W - SCR_W);
    g_camy = clampf(P.y - SCR_H / 2, 0, WORLD_H - SCR_H);
#ifdef PEPEL_DEBUG
    dbg_all_give();
#endif
    msg("Pide trabajo al comerciante.");
}

// Guarda el estado vivo de la zona z en su memoria
static void zone_stash(int z)
{
    if (z == ZM) { urbe_stash(); return; }   // [urbe] el metro no tiene caché: solo sus máscaras
    if (z == ZH) {   // [mapas] el segundo piso de una casa no tiene memoria propia: solo dos máscaras por casa
        unsigned short o = 0, d = 0;
        for (int i = 0; i < g_ncr && i < 16; i++) if (g_cr[i].open) o |= (unsigned short)(1 << i);
        for (int i = 0; i < g_ne && i < 16; i++) if (g_e[i].state == ES_DEAD) d |= (unsigned short)(1 << i);
        if (g_hs_cur >= 0 && g_hs_cur < HS_MAX) { g_hs_open[g_hs_cur] = o; g_hs_dead[g_hs_cur] = d; }
        return;
    }
    ZoneCache *c = &g_zc[z];
    if (z == 2) g_zc2_fl = g_pz >= 2 ? g_pz : 1;   // [arregla]
    c->vis = 1; c->ncr = g_ncr; c->ne = g_ne;
    memcpy(c->cr, g_cr, sizeof(Crate) * g_ncr);
    memcpy(c->e, g_e, sizeof(Enemy) * g_ne);
    memcpy(c->k, g_k, sizeof(g_k));
    memcpy(c->seen, g_seen, sizeof(g_seen));
    if (z == 2) pozo_stash();   // [pozo]
}

// Carga la zona z; si ya la habías visitado, le pone encima lo que recordaba
static void zone_enter(int z)
{
    const ZoneCache *c = &g_zc[z < NZ ? z : 0];   // [urbe]
    zone_build(z);
    memset(g_seen, 0, sizeof(g_seen));
    if (z == ZM) { urbe_apply(); g_zone = z; g_seen_tx = -1; return; }   // [urbe]
    if (z == ZH) {   // [mapas] lo abierto y lo muerto de esta casa
        for (int i = 0; i < g_ncr && i < 16; i++) if ((g_hs_open[g_hs_cur] >> i) & 1) g_cr[i].open = 1;
        for (int i = 0; i < g_ne && i < 16; i++) if ((g_hs_dead[g_hs_cur] >> i) & 1) { g_e[i].state = ES_DEAD; g_e[i].hp = 0; g_e[i].dead_t = 0; }
        g_zone = z; g_seen_tx = -1;
        return;
    }
    if (z == 2 && pozo_restore()) { g_zone = z; g_seen_tx = -1; return; }   // [pozo]
    if (c->vis && c->ncr == g_ncr && c->ne == g_ne && (z != 2 || g_zc2_fl == (g_pz >= 2 ? g_pz : 1))) {   // [arregla] lo de g_zc[2] solo sirve para su piso
        memcpy(g_cr, c->cr, sizeof(Crate) * g_ncr);
        memcpy(g_e, c->e, sizeof(Enemy) * g_ne);
        memcpy(g_k, c->k, sizeof(g_k));
        memcpy(g_seen, c->seen, sizeof(g_seen));
    }
    g_zone = z;
    g_seen_tx = -1;
}

// Cambio de zona con fundido: la pantalla se oscurece, a negro se cambia de zona (una vez) y vuelve con el nombre
#define ZFADE 0.4f
#define ZHOLD 0.35f
static int g_zx_st, g_zx_b;   // llegada por escalera: 1 = subiendo (junto a la escalera de bajar del piso), 2 = bajando (junto a la de subir), y de cuál edificio con escalera
static int g_zto, g_zdone, g_zsave, g_zx_hatch;   // zona de destino, si ya se cambió y si hay que guardar al llegar
static float g_zx, g_zy;              // dónde aparece el jugador
static void zone_go(int z, float x, float y, int save)
{
    g_state = GS_ZONE; g_state_t = 0;
    g_zto = z; g_zx = x; g_zy = y; g_zdone = 0; g_zsave = save; g_zexp = 0; g_zx_hatch = 0; g_zx_st = 0;
}

// Cruz en una escalera de los edificios: dir +1 sube un piso, -1 baja. El piso 1 es la expedición; el 2 al 4, las zonas 3 a 8
static void stair_go(int dir, int tx, int ty)
{
    int b = 0, p = 1;
    if (g_zone == ZM) { g_mt_cur = urbe_station_near(); zone_go(1, 0, 0, 0); return; }   // [urbe] sube por la boca de esta estación
    if (g_zone == 1 && urbe_mouth_at(tx, ty) >= 0) { g_mt_cur = urbe_mouth_at(tx, ty); g_mt_bx = P.x; g_mt_by = P.y; zone_go(ZM, 0, 0, 0); return; }   // [urbe] baja al metro
    if (g_zone == ZH) { zone_go(1, 0, 0, 0); g_zx_st = 2; g_zx_b = 100 + g_hs_cur; return; }   // [mapas] de vuelta a la casa de abajo
    if (g_zone == 1)
        for (int k = 0; k < g_nhs; k++) {
            const XBld *R = &x_b[g_hs_bi[k]];
            if (tx < R->x0 || tx > R->x1 || ty < R->y0 || ty > R->y1) continue;
            g_hs_cur = k;
            g_hs_bx = P.x; g_hs_by = P.y;
            zone_go(ZH, 0, 0, 0); g_zx_st = 1; g_zx_b = 100 + k;
            return;
        }
    if (g_zone >= 3) { b = (g_zone - 3) / 3; p = (g_zone - 3) % 3 + 2; }
    else
        for (int i = 0; i < g_nstb; i++) {
            const XBld *R = &x_b[g_stb_bi[i]];
            if (tx >= R->x0 && tx <= R->x1 && ty >= R->y0 && ty <= R->y1) b = i;
        }
    int np = p + dir;
    zone_go(np <= 1 ? 1 : 3 + b * 3 + (np - 2), 0, 0, 0);
    g_zx_st = dir > 0 ? 1 : 2; g_zx_b = b;
}

#ifdef PEPEL_DEBUG
static int g_dbg_faro;   // pruebas: toda expedición trae puesto farolero
static int g_dbg_xe0 = -1, g_dbg_xe1 = -1, g_dbg_xdog = -1;   // pruebas: fuerzan los eventos y el perro de la próxima expedición
static int g_dbg_xt = -1, g_dbg_xs = -1, g_dbg_xd = -1, g_dbg_xb = -1;   // pruebas: fuerzan el tipo y la nieve de la próxima expedición (-1 = al azar)
#endif
// Sale por el portón: se decide la expedición (tipo, peligro y lo que trae según los trabajos) y se pasa a ella
static void exped_start(void)
{
    Exped *X = &g_x;
    memset(X, 0, sizeof(*X));
    X->num = ++g_xtotal;
    g_xs_t = g_play_t; g_xs_kills = g_kills; g_xs_open = g_opened;   // para el resumen del regreso
    X->seed = (rnd() ^ ((u32)(g_play_t * 1000) * 2654435761u)) | 1;
    X->danger = (X->num - 1 < 6 ? X->num - 1 : 6) + rank();   // las primeras salen suaves
    if (X->danger > 9) X->danger = 9;
    pozo_danger(X);   // [pozo]
    X->type = (int)(rnd() % XT_N);
    if (X->type == XT_NUC) X->type = XT_TOWN;   // [mapnuc] la planta solo sale por su sorteo de abajo
    if (X->type == XT_ROAD) X->type = XT_TOWN;   // la Autopista falla la prueba del generador (quedan sitios inalcanzables): fuera del azar hasta arreglarla
    if (X->num >= 4 && rnd() % 10 == 0) X->type = XT_NUC;   // [mapnuc] la planta atómica: 1 de cada 10, desde la cuarta expedición
    if (X->num > 1 && rnd() % 3 == 0) X->flags |= XF_SNOW;   // 1 de cada 3 sale nevada, nunca la primera
#ifdef PEPEL_DEBUG
    if (g_dbg_xt >= 0) X->type = g_dbg_xt;
    if (g_dbg_xs >= 0) { X->flags &= ~XF_SNOW; if (g_dbg_xs) X->flags |= XF_SNOW; }
#endif
#ifdef PEPEL_DEBUG
    if (g_dbg_xd >= 0) X->danger = g_dbg_xd;
    if (g_dbg_xb >= 0) { X->flags &= ~XF_BOSS2; if (g_dbg_xb) X->flags |= XF_BOSS2; }
#endif
    if (g_job == 5 && !g_boss_dead) { X->flags |= XF_BEAST; X->type = XT_FOREST; }
    jefes_exped(X);   // [jefes]
    if (g_job == 4 || g_fjob == 3) X->flags |= XF_BANDITS;   // el trabajo farolero del francotirador también pide campamento
    if (X->num >= 2 && rnd() % 3 == 0) X->flags |= XF_FARO;   // 1 de cada 3 desde la segunda: un puesto farolero
#ifdef PEPEL_DEBUG
    if (g_dbg_faro) X->flags |= XF_FARO;
#endif
    if (g_job == 1 && !P.has_job) X->flags |= XF_JOB;
    if ((g_job == 6 && !g_has_meds) || (g_job == 7 && !g_has_reg) || rnd() % 3 == 0) X->flags |= XF_HATCH;
    if (X->type == XT_NUC) { X->flags &= ~(XF_SNOW | XF_BEAST); X->danger += 3; if (X->danger > 9) X->danger = 9; }   // [mapnuc] el peligro sube +3
    if (X->type == XT_CURSED) X->flags &= ~XF_SNOW;   // [mapnuc]
    if (X->num == 1) X->flags |= XF_MASKBODY;
    if (X->num >= 2 && !g_dog.have) X->flags |= XF_DOG;   // desde la segunda: un perro herido junto a un cuerpo, hasta que lo tengas
    X->ev0 = 1 + (int)(rnd() % (EV_N - 1));               // un evento al azar; desde peligro 4, dos distintos
    if (X->danger >= 4) { X->ev1 = 1 + (int)(rnd() % (EV_N - 1)); if (X->ev1 == X->ev0) X->ev1 = 1 + X->ev1 % (EV_N - 1); }
#ifdef PEPEL_DEBUG
    if (g_dbg_xe0 >= 0) X->ev0 = g_dbg_xe0;
    if (g_dbg_xe1 >= 0) X->ev1 = g_dbg_xe1;
    if (g_dbg_xdog >= 0) { X->flags &= ~XF_DOG; if (g_dbg_xdog) X->flags |= XF_DOG; }
#endif
    if (g_dog.have && g_dog.skip) g_dog.skip = 0;          // el que volvió herido se queda en casa esta vez
    if (!g_w[W_SAWED].owned && !g_arm[W_SAWED].owned) X->flags |= XF_SAWED;   // [qsist] guardada en el armero cuenta como tenerla
    g_zc[1].vis = 0;                              // la expedición anterior ya no existe
    for (int z = 3; z < NZ; z++) g_zc[z].vis = 0; // ni sus pisos
    memset(g_hs_open, 0, sizeof(g_hs_open)); memset(g_hs_dead, 0, sizeof(g_hs_dead)); urbe_reset();   // [mapas] ni los pisos de sus casas   // [urbe]
    g_zc[2].vis = 0; g_bseed = rnd() | 1; g_bstale = 0;   // y el búnker de debajo se arma de nuevo
    armas_gc(1);   // [arregla] huecos de armas botadas de la expedición anterior
    zone_go(1, -1, -1, 0);
    g_zexp = 1;
    peso_exped_hint();   // [peso]
}

static int save_game(void);
#ifdef PEPEL_DEBUG
static void dbg_log_line(const char *ln);
#endif

// Al volver de una expedición a la base: se anotan las cifras del resumen, la enfermería se renueva y el huerto da 1 conserva y 1 agua al alijo (si caben)
static void base_return(void)
{
    g_ret_time = (int)(g_play_t - g_xs_t); if (g_ret_time < 0) g_ret_time = 0;
    g_ret_kills = g_kills - g_xs_kills; if (g_ret_kills < 0) g_ret_kills = 0;
    g_ret_open = g_opened - g_xs_open; if (g_ret_open < 0) g_ret_open = 0;
    g_clinic_used = 0;
    g_ret_food = g_ret_water = 0;
    if (g_up[UP_GARDEN]) { g_ret_food = stash_add(ITEM_FOOD, 1); g_ret_water = stash_add(ITEM_WATER, 1); }
    g_ret_pend = 1;
    duro_return();   // [duro] combustible del generador y filtro de agua
}

static void zone_switch(void)
{
    if (g_zone == 2 && g_bstale) { g_zc[2].vis = 0; g_bseed = g_bpend; g_bstale = 0; }   // terminó una tormenta con el jugador adentro: se rehace
    else zone_stash(g_zone);
    int from = g_zone;
    if (g_zone == 1 && g_zto == 0) {   // se vuelve a la base: la expedición se descarta
        g_zc[1].vis = 0; g_zc[2].vis = 0; g_bseed = rnd() | 1; g_bstale = 0;
        for (int z = 3; z < NZ; z++) g_zc[z].vis = 0;
        memset(g_hs_open, 0, sizeof(g_hs_open)); memset(g_hs_dead, 0, sizeof(g_hs_dead)); urbe_reset();   // [mapas] [urbe]
        base_return();
    }
    pozo_switch();   // [pozo]
    if (from == 1 && g_zto == 0) armas_gc(0);   // [arregla] la expedición se descartó: sus armas botadas ya no existen
    zone_enter(g_zto);
    if (from == 0 && g_zto == 1 && g_up[UP_TOWER]) memset(g_seen, 1, sizeof(g_seen));   // la torre de vigía: la expedición empieza con el mapa descubierto
    if (g_zto == ZB) bk_arrive(&g_zx, &g_zy);   // [bunker]
    else if (g_zto == 2) { g_zx = g_bun_ex * TS + 8; g_zy = g_bun_ey * TS + 8; pozo_arrive(&g_zx, &g_zy); }   // abajo: junto a la escalera   // [pozo]
    else if (g_zto == 1 && g_zx < 0) {   // -1: llega por el portón, al punto de retirada; -2: sube del búnker, junto a la escotilla
        g_zx = g_ret_tx * TS + 8; g_zy = g_ret_ty * TS + 8;
        if (g_zx_hatch && g_hatch_tx >= 0)
            for (int k = 0; k < 4; k++) {
                int tx = g_hatch_tx + (k == 2) - (k == 3), ty = g_hatch_ty + (k == 0) - (k == 1);
                if (in_map(tx, ty) && !solid(tx, ty)) { g_zx = tx * TS + 8; g_zy = ty * TS + 8; break; }
            }
    }
    if (g_zto == ZM) urbe_arrive_down(&g_zx, &g_zy);   // [urbe]
    else if (from == ZM && g_zto == 1) urbe_arrive_up(&g_zx, &g_zy);   // [urbe]
    if (g_zx_st && (g_nstb > 0 || g_zx_b >= 100)) {   // por escalera: junto a la escalera del otro piso (al subir, la de bajar; al bajar, la de subir)
        const XBld *R = &x_b[g_zx_b >= 100 && g_nhs > 0 ? g_hs_bi[g_zx_b - 100 < g_nhs ? g_zx_b - 100 : 0] : g_stb_bi[g_zx_b < g_nstb ? g_zx_b : 0]];   // [mapas] g_zx_b >= 100: una casa
        char want = g_zx_st == 1 ? '4' : '3';
        int found = 0;
        g_zx = (R->x0 + R->x1) / 2 * TS + 8; g_zy = (R->y0 + R->y1) / 2 * TS + 8;
        for (int y = R->y0; y <= R->y1 && !found; y++)
            for (int x = R->x0; x <= R->x1 && !found; x++) {
                if (g_zo[y][x] != want) continue;
                for (int k = 0; k < 4 && !found; k++) {
                    int tx = x + (k == 2) - (k == 3), ty = y + (k == 0) - (k == 1);
                    if (in_map(tx, ty) && !solid(tx, ty)) { g_zx = tx * TS + 8; g_zy = ty * TS + 8; found = 1; }
                }
            }
    }
    P.x = g_zx; P.y = g_zy;
    P.lock = -1; P.sq_t = -1;
    memset(g_b, 0, sizeof(g_b));
    explore();
    g_was_in = indoors(P.x, P.y);
    g_camx = clampf(P.x - SCR_W / 2, 0, WORLD_W - SCR_W);
    g_camy = clampf(P.y - SCR_H / 2, 0, WORLD_H - SCR_H);
    g_ban_t = 3.5f;
    g_msg_t = 0;
    jefes_zona();   // [jefes]
    pozo_after();   // [pozo]
    if (g_zsave) msg(save_game() ? "Llegaste. Partida guardada." : "No se pudo guardar la partida.");
}

// Casillas de salida de la zona: al pisarlas se pasa a la zona de al lado, 3 casillas adentro de su entrada
static void zone_portal(void)
{
    if (g_has_chof && g_zone == 1 && !g_stop_seen[1] && dist(P.x, P.y, g_chof_tx * TS + 8, g_chof_ty * TS + 8) < 70) {
        g_stop_seen[1] = 1;
        msg("Parada del este descubierta: el Chofer te puede llevar de vuelta a Estación Kolos.");
    }
}

// ------------------------------------------------------------------ guardar
// La partida va en soinnka.sav, al lado del EBOOT. Cabecera con marca, version y tamano: si algo no
// calza, el archivo se ignora. Las manchas de sangre no se guardan.
#define SAVE_MAGIC 0x4B4E4953u   // "SINK"
#define SAVE_VER 34   // 34: W_MAX 24 (ocho armas nuevas)
//        33   // 33: topes más altos (ITEM_MAX 128, SHOP_MAX 96, W_MAX 16), mochila por peso, artefactos nuevos, mapas más grandes
//        32: piezas nuevas y armas botadas, tres jefes nuevos (j2done), el Pozo y niveles infernales
//        31: búnker propio (salas, cajas y decoraciones), zonas de jefe y trabajo 8
//        30: el objeto de mano elegido
//        29: 29: habilidades, Faroleros (puesto XF_FARO, reputación y trabajos) y notas del diario
//        28: Hollín (el perro) y los eventos de la expedición (Exped con ev0, ev1, evfound y evdone; XF_DOG)
//        27: detector y artefactos (EQ_COUNT 5, GEAR_MAX 16, SHOP_MAX 64)
//        26: objetos de mano (ITEM_COUNT sube)
//        25   // 25: armas cuerpo a cuerpo y de fuego nuevas (W_MAX 12), enemigos nuevos y segundo jefe, tienda con 7 armas más (SHOP_MAX 40)
//        24: zonas de tamaño propio (la expedición 128 x 80), más topes
//        23: tipos nuevos de expedición, nieve (frío) y pisos de edificios (NZ 9)
//        24: zonas de tamaño propio (la expedición 128 x 80), más topes
//        22: 22: mejoras de la base (up, clinic_used) y los contadores del resumen del regreso
typedef struct {
    int vis, ncr, ne;
    unsigned char cr_open[MAXCR];                                  // cajones y cuerpos ya abiertos
    struct { float x, y, ang, hp, strafe; int dead, irr, fac; } e[MAXE];
    Pick k[MAXK];                                               // lo que quedo tirado en el suelo
    unsigned char seen[MHMAX][(MWMAX + 7) / 8];                 // lo explorado del mapa, un bit por casilla   // [mapas]
} ZoneSave;
typedef struct {
    u32 magic, ver, size;
    float x, y, ang, hp, rad, bleed, stam, hunger, thirst;
    int weapon, has_job;
    WeaponState w[W_MAX];                                       // las armas: si la tienes, su cargador, su estado, la munición cargada y sus piezas
    int rep;
    int bag[ITEM_MAX], stash[ITEM_MAX];                         // la mochila y el alijo: cantidad de cada objeto (ITEM_)
    float storm_t, storm_left, play_t;
    int storm_on, storm_warned, kills, opened;
    int job_done5;
    int job, job_n, job_done1;                                  // trabajo activo, su avance y si los repuestos ya se entregaron
    int zone;                                                   // zona en la que estás
    int stop_seen[NSTOP];                                       // paradas de viaje descubiertas
    u32 bseed, bpend;                                           // búnker: semilla de la distribución y la que toca tras la próxima tormenta
    int bstale;                                                 // la tormenta terminó con el jugador en el búnker (se rehace al salir)
    int has_meds, has_reg, job_done6, job_done7, fin_seen;      // trabajos del búnker y si ya se vio el final
    int jobs_done, scrap_gain;                                  // estadísticas del final
    ZoneSave zs[NZ];                                            // el estado de cada zona (la tuya también)
    unsigned char stock[SHOP_MAX];                              // existencias del comerciante
    int eq[EQ_COUNT];                                           // equipo puesto (su lugar en GEAR + 1)
    float gcond[GEAR_MAX];                                      // estado de cada prenda (el de la máscara es su filtro)
    int mode;                                                   // modo de juego (MODE_)
    Exped xp;                                                   // la expedición en curso (con esto se arma igual)
    int xtotal, boss_dead;                                      // expediciones hechas y si La Bestia ya murió
    int up[UP_MAX], clinic_used;                                // mejoras de la base hechas (UP_) y si la enfermería ya se usó en esta vuelta
    float xs_t; int xs_kills, xs_open;                          // reloj, bajas y cajones al salir de expedición (resumen del regreso)
    float cold;                                                 // frío (expediciones nevadas)
    Dog dog;                                                    // Hollín
    int sk[SK_N], frep, fjob, fjn, fdone, notes;                // habilidades compradas; Faroleros (reputación, trabajo, avance, hechos); notas halladas
    int hand;                                                   // objeto de mano elegido (H_)
    unsigned char bk_grid[BK_H][BK_W]; short bk_box[BK_NBOX][ITEM_MAX]; int bk_rooms;   // [bunker] decoración y cajas, contenido de cada caja, salas abiertas
    WDrop wdrop[WDROP_N];                                       // [armas] armas botadas en el suelo
    int j2done;                                                 // [jefes2] jefes de Kolos cobrados (bit 0 Capataz, bit 1 Fermentado)
    int pz_lift, pz_inf, pz_cur;   // [pozo] último ascensor, expediciones infernales hechas y piso del Pozo en que se guardó
    WeaponState peso_arm[W_MAX]; int peso_flags;   // [peso] armero (armas guardadas) y marcas
    int duro[8];   // [duro] regresos, pedidos entregados, generador con luz, bidones, filtros de repuesto y regresos del filtro puesto
    int uniq;   // [artef] piezas únicas entregadas (bits)
    float hora;   // [diseno] hora del juego (0 = partida vieja: 17:00)
    unsigned short hs_open[HS_MAX], hs_dead[HS_MAX];   // [mapas] segundos pisos de las casas: cajones abiertos y enemigos muertos (una máscara por casa)
    float bomba_t;   // [bomba] segundos que quedan para la bomba
    unsigned char mt_open[MAXCR], mt_dead[MAXE]; float mt_bx, mt_by;   // [urbe] memoria del metro y por dónde se bajó
} SaveData;
static SaveData g_sv;

static int save_read_from(const char *path)
{
    char extra;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, &g_sv, sizeof(g_sv));
    int more = sceIoRead(fd, &extra, 1);
    sceIoClose(fd);
    return n == (int)sizeof(g_sv) && more <= 0 && g_sv.magic == SAVE_MAGIC && g_sv.ver == SAVE_VER &&
           g_sv.size == sizeof(g_sv);
}

// Lee la partida a g_sv. Si un corte dejo solo el .tmp (ya completo), sirve igual
static int save_read(void) { return save_read_from(g_sav) || save_read_from(g_tmp); }

static int save_game(void)
{
    SaveData *s = &g_sv;
    memset(s, 0, sizeof(*s));
    s->magic = SAVE_MAGIC; s->ver = SAVE_VER; s->size = sizeof(*s);
    s->x = P.x; s->y = P.y; s->ang = P.ang; s->hp = P.hp; s->rad = P.rad; s->bleed = P.bleed; s->stam = P.stam;
    s->hunger = P.hunger; s->thirst = P.thirst;
    s->weapon = P.weapon; s->has_job = P.has_job;
    memcpy(s->w, g_w, sizeof(g_w));
    s->rep = g_rep; s->bomba_t = g_bomba_t;   // [bomba]
    for (int i = 0; i < ITEM_COUNT; i++) { s->bag[i] = g_bag[i]; s->stash[i] = g_stash[i]; }
    s->storm_t = g_storm_t; s->storm_left = g_storm_left; s->play_t = g_play_t;
    s->storm_on = g_storm_on; s->storm_warned = g_storm_warned; s->kills = g_kills; s->opened = g_opened;
    s->job = g_job; s->job_n = g_job_n; s->job_done1 = g_job_done1; s->job_done5 = g_job_done5;
    duro_save(s->duro);   // [duro]
    zone_stash(g_zone);   // lo vivo pasa a la memoria de su zona y de ahí al archivo
    s->zone = g_zone;
    for (int i = 0; i < NSTOP; i++) s->stop_seen[i] = g_stop_seen[i];
    s->bseed = g_bseed; s->bpend = g_bpend; s->bstale = g_bstale;
    s->has_meds = g_has_meds; s->has_reg = g_has_reg; s->job_done6 = g_job_done6; s->job_done7 = g_job_done7; s->fin_seen = g_fin_seen;
    s->jobs_done = g_jobs_done; s->scrap_gain = g_scrap_gain;
    bk_save(s);   // [bunker]
    for (int z = 0; z < NZ; z++) {
        const ZoneCache *c = &g_zc[z];
        ZoneSave *q = &s->zs[z];
        q->vis = c->vis;
        if (!c->vis) continue;
        q->ncr = c->ncr; q->ne = c->ne;
        for (int i = 0; i < c->ncr; i++) q->cr_open[i] = c->cr[i].open;
        for (int i = 0; i < c->ne; i++) {
            const Enemy *e = &c->e[i];
            q->e[i].x = e->x; q->e[i].y = e->y; q->e[i].ang = e->ang; q->e[i].hp = e->hp; q->e[i].strafe = e->strafe;
            q->e[i].dead = e->state == ES_DEAD;
            q->e[i].irr = e->irr;
            q->e[i].fac = e->fac;
        }
        memcpy(q->k, c->k, sizeof(q->k));
        memset(q->seen, 0, sizeof(q->seen));   // [mapas] empaquetado a un bit por casilla
        for (int y = 0; y < MHMAX; y++) for (int x = 0; x < MWMAX; x++) if (c->seen[y][x]) q->seen[y][x >> 3] |= (unsigned char)(1 << (x & 7));
    }
    memcpy(s->stock, g_stock, sizeof(s->stock));
    for (int i = 0; i < EQ_COUNT; i++) s->eq[i] = g_eq[i];
    for (int i = 0; i < GEAR_N; i++) s->gcond[i] = g_gcond[i];
    peso_save(s->peso_arm, &s->peso_flags);   // [peso]
    s->mode = g_mode;
    s->xp = g_x; s->xtotal = g_xtotal; s->boss_dead = g_boss_dead;
    for (int i = 0; i < UP_N; i++) s->up[i] = g_up[i];
    s->clinic_used = g_clinic_used; s->xs_t = g_xs_t; s->xs_kills = g_xs_kills; s->xs_open = g_xs_open;
    s->cold = g_cold;
    s->j2done = g_j2done;   // [jefes2]
    for (int i = 0; i < SK_N; i++) s->sk[i] = g_sk[i];
    s->frep = g_frep; s->fjob = g_fjob; s->fjn = g_fjn; s->fdone = g_fdone; s->notes = g_notes;
    s->dog = g_dog; s->hand = g_hand;
    memcpy(s->wdrop, g_wdrop, sizeof(g_wdrop));   // [armas]
    s->pz_lift = g_pz_lift; s->pz_inf = g_pz_inf; s->pz_cur = g_pz;   // [pozo]
    s->uniq = g_uniq;   // [artef]
    s->hora = g_hora;   // [diseno]
    memcpy(s->hs_open, g_hs_open, sizeof(g_hs_open)); memcpy(s->hs_dead, g_hs_dead, sizeof(g_hs_dead));   // [mapas]
    memcpy(s->mt_open, g_mt_open, sizeof(g_mt_open)); memcpy(s->mt_dead, g_mt_dead, sizeof(g_mt_dead)); s->mt_bx = g_mt_bx; s->mt_by = g_mt_by;   // [urbe]
    if (g_zone == ZM) { s->zone = 1; s->x = g_mt_bx; s->y = g_mt_by; }   // [urbe] guardado estando abajo: vuelves a la boca
    if (g_zone == ZH) { s->zone = 1; s->x = g_hs_bx; s->y = g_hs_by; }   // [mapas] guardado estando arriba en una casa: vuelves a la puerta de abajo

    // primero al .tmp y despues se reemplaza el .sav: un corte no deja una partida danada
    SceUID fd = sceIoOpen(g_tmp, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return 0;
    int n = sceIoWrite(fd, s, sizeof(*s));
    sceIoClose(fd);
    if (n != (int)sizeof(*s)) { sceIoRemove(g_tmp); return 0; }
    sceIoRemove(g_sav);
    sceIoRename(g_tmp, g_sav);
    g_has_save = 1;
    return 1;
}

// Cargar = partida nueva y encima lo guardado. Si el archivo no sirve, queda la partida nueva
static int load_game(void)
{
    new_game();
    if (!save_read()) {
#ifdef PEPEL_DEBUG
        dbg_log_line("T load: save_read fallo\n");
#endif
        g_has_save = 0; return 0; }
    const SaveData *s = &g_sv;
    g_x = s->xp; g_xtotal = s->xtotal; g_boss_dead = s->boss_dead != 0;
    for (int i = 0; i < SK_N; i++) g_sk[i] = s->sk[i] != 0;
    g_frep = s->frep; g_fjob = s->fjob; g_fjn = s->fjn; g_fdone = s->fdone; g_notes = s->notes;
    for (int i = 0; i < UP_N; i++) g_up[i] = s->up[i] != 0;   // antes de armar las zonas: la base sale con sus mejoras
    g_clinic_used = s->clinic_used != 0; g_xs_t = s->xs_t; g_xs_kills = s->xs_kills; g_xs_open = s->xs_open;
    g_cold = clampf(s->cold, 0, 100);
    g_j2done = s->j2done;   // [jefes2]
    g_uniq = s->uniq & 255;   // [artef]
    g_hora = s->hora > 0 && s->hora < 24 ? s->hora : 17;   // [diseno]
    if (g_x.type < 0 || g_x.type >= XT_N || g_x.danger < 0 || g_x.danger > 15) { g_x.type = 0; g_x.danger = 0; g_zc[1].vis = 0; }
    g_pz_lift = s->pz_lift; g_pz_inf = s->pz_inf; g_pz = s->pz_cur >= 2 && s->pz_cur < 1000 ? s->pz_cur : 0; g_pz_fresh = 1;   // [pozo]
    memcpy(g_hs_open, s->hs_open, sizeof(g_hs_open)); memcpy(g_hs_dead, s->hs_dead, sizeof(g_hs_dead));   // [mapas]
    memcpy(g_mt_open, s->mt_open, sizeof(g_mt_open)); memcpy(g_mt_dead, s->mt_dead, sizeof(g_mt_dead)); g_mt_bx = s->mt_bx; g_mt_by = s->mt_by;   // [urbe]
    g_bseed = s->bseed | 1; g_bpend = s->bpend; g_bstale = s->bstale != 0;   // antes de armar las zonas: el búnker sale de la semilla
    bk_load(s);   // [arregla] antes de armar las zonas: Mi búnker sale con lo guardado
    g_fin_seen = s->fin_seen != 0;   // [arregla] la generación infernal lo usa
    for (int z = 0; z < NZ; z++) {   // cada zona guardada: se arma, se le pone encima lo guardado y se deja en la memoria de zonas
        const ZoneSave *q = &s->zs[z];
        if (!q->vis) continue;
        zone_build(z);
        if (q->ncr != g_ncr || q->ne != g_ne) {
#ifdef PEPEL_DEBUG
            char dl[120]; snprintf(dl, sizeof(dl), "T load: zona %d no calza: cajones %d/%d enem %d/%d\n", z, q->ncr, g_ncr, q->ne, g_ne); dbg_log_line(dl);
#endif
            continue;
        }
        for (int i = 0; i < g_ncr; i++) g_cr[i].open = q->cr_open[i];
        for (int i = 0; i < g_ne; i++) {
            Enemy *e = &g_e[i];
            e->x = e->tx = q->e[i].x; e->y = e->ty = q->e[i].y;
            e->ang = q->e[i].ang; e->hp = q->e[i].hp; e->strafe = q->e[i].strafe;
            e->irr = q->e[i].irr != 0;
            e->fac = q->e[i].fac >= F_CREA && q->e[i].fac <= F_FARO ? q->e[i].fac : fac_of(e->type);   // guardias que ya son hostiles
            if (q->e[i].dead) e->state = ES_DEAD;
            else if (e->summon) { e->state = ES_COMBAT; e->tx = s->x; e->ty = s->y; }   // un llamado que seguía vivo
            else if (e->ev == EVN_HORDE && e->hp > 0) { e->state = ES_ALERT; e->t = 0; for (int k = 0; k < g_nev; k++) if (g_ev[k].kind == EV_HORDE) { e->tx = g_ev[k].x2 * TS + 8; e->ty = g_ev[k].y2 * TS + 8; } }   // una horda que ya partió
        }
        memcpy(g_k, q->k, sizeof(g_k));
        for (int y = 0; y < MHMAX; y++) for (int x = 0; x < MWMAX; x++) g_seen[y][x] = (q->seen[y][x >> 3] >> (x & 7)) & 1;   // [mapas]
        zone_stash(z);
    }
    if (s->zone < 0 || s->zone >= NZ || !g_zc[s->zone].vis) {
#ifdef PEPEL_DEBUG
        dbg_log_line("T load: zona no valida\n");
#endif
        new_game(); g_has_save = 0; return 0; }
    zone_enter(s->zone);
    for (int i = 0; i < NSTOP; i++) g_stop_seen[i] = i == 0 || s->stop_seen[i] != 0;
    g_has_meds = s->has_meds != 0; g_has_reg = s->has_reg != 0; g_job_done6 = s->job_done6 != 0; g_job_done7 = s->job_done7 != 0;
    g_fin_seen = s->fin_seen != 0; g_jobs_done = s->jobs_done; g_scrap_gain = s->scrap_gain;
    P.x = s->x; P.y = s->y; P.ang = s->ang; P.hp = s->hp; P.rad = s->rad; P.bleed = s->bleed; P.stam = s->stam;
    P.hunger = clampf(s->hunger, 0, 100); P.thirst = clampf(s->thirst, 0, 100);
    P.has_job = s->has_job;
    for (int i = 0; i < W_COUNT; i++) {
        g_w[i] = s->w[i];
        g_w[i].owned = g_w[i].owned != 0;
        g_w[i].cond = clampf(g_w[i].cond, 0.05f, 1);
        if (g_w[i].load < 0 || g_w[i].load >= WDEF[i].nammo) g_w[i].load = 0;
        for (int k = 0; k < SLOT_COUNT; k++) {   // una pieza que no existe, o que no va en esa ranura o esa arma, se quita
            int pi = g_w[i].part[k];
            if (pi && (pi > PARTS_N || PARTS[pi - 1].slot != k || !(PARTS[pi - 1].fits & FITS(i)))) g_w[i].part[k] = 0;
        }
        g_w[i].mag = g_w[i].mag < 0 ? 0 : g_w[i].mag > w_mag(i) ? w_mag(i) : g_w[i].mag;
    }
    peso_load(s->peso_arm, s->peso_flags);   // [peso] ya no se fuerza la Makarov ni el cuchillo: siempre queda al menos un arma
    P.weapon = s->weapon >= 0 && s->weapon < W_COUNT && g_w[s->weapon].owned ? s->weapon : peso_first_weapon();
    g_rep = s->rep; g_bomba_t = s->bomba_t > 0 ? s->bomba_t : 0;   // [bomba]
    for (int i = 0; i < ITEM_COUNT; i++) {
        g_bag[i] = s->bag[i] > 0 ? s->bag[i] : 0;
        g_stash[i] = s->stash[i] > 0 && !(ITEMS[i].flags & IF_NOSTASH) ? s->stash[i] : 0;
    }
    g_storm_t = s->storm_t; g_storm_left = s->storm_left; g_play_t = s->play_t;
    g_storm_on = s->storm_on; g_storm_warned = s->storm_warned; g_kills = s->kills; g_opened = s->opened;
    duro_load(s->duro);   // [duro]
    g_job = s->job >= 0 && s->job <= JOB_N ? s->job : 0; g_job_n = s->job_n; g_job_done1 = s->job_done1; g_job_done5 = s->job_done5;
    for (int i = 0; i < SHOP_N; i++) g_stock[i] = s->stock[i] < SHOP[i].stock ? s->stock[i] : SHOP[i].stock;
    for (int i = 0; i < EQ_COUNT; i++)   // lo puesto tiene que existir e ir en esa ranura
        g_eq[i] = s->eq[i] >= 1 && s->eq[i] <= GEAR_N && slot_ok(s->eq[i] - 1, i) ? s->eq[i] : 0;
    for (int i = 0; i < GEAR_N; i++) g_gcond[i] = clampf(s->gcond[i], 0, 100);
    g_mode = s->mode >= 0 && s->mode < MODE_COUNT ? s->mode : MODE_SURV;
    g_hand = s->hand >= 0 && s->hand < H_N ? s->hand : 0;
    memcpy(g_wdrop, s->wdrop, sizeof(g_wdrop));   // [armas]
    g_dog = s->dog;
    if (g_dog.have) {
        g_dog.have = 1; g_dog.hp = clampf(g_dog.hp, 0, DOG_HP); g_dog.zone = g_zone; g_dog.st = g_dog.st == DG_STAY ? DG_STAY : DG_FOLLOW;
        g_dog.home = g_dog.home != 0; g_dog.wounded = g_dog.wounded != 0; g_dog.atk = 0;
    } else memset(&g_dog, 0, sizeof(g_dog));
    g_seen_tx = -1;
    explore();
    g_was_in = indoors(P.x, P.y);
    g_camx = clampf(P.x - SCR_W / 2, 0, WORLD_W - SCR_W);
    g_camy = clampf(P.y - SCR_H / 2, 0, WORLD_H - SCR_H);
    if (g_dog.have) { if (g_dog.home) dog_kennel(); else dog_to_player(); }
    jefes_carga();   // [jefes]
    { char jt[96]; job_text(jt, sizeof(jt)); msg(jt); }
    return 1;
}

static void cfg_save(void)
{
    u32 buf[2 + OPT_COUNT];
    buf[0] = CFG_MAGIC; buf[1] = OPT_COUNT;
    for (int i = 0; i < OPT_COUNT; i++) buf[2 + i] = (u32)g_opt[i];
    SceUID fd = sceIoOpen(g_cfg, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, buf, sizeof(buf));
    sceIoClose(fd);
}

static void cfg_load(void)
{
    u32 buf[2 + OPT_COUNT];
    SceUID fd = sceIoOpen(g_cfg, PSP_O_RDONLY, 0777);
    if (fd < 0) return;
    int n = sceIoRead(fd, buf, sizeof(buf));
    sceIoClose(fd);
    if (n < (int)(sizeof(u32) * (2 + OPT_COUNT - 1)) || buf[0] != CFG_MAGIC || buf[1] > OPT_COUNT || n != (int)(sizeof(u32) * (2 + buf[1]))) return;   // [armas2] acepta el cfg viejo (sin Música)
    for (int i = 0; i < (int)buf[1]; i++) if (buf[2 + i] < (u32)OPT_N[i]) g_opt[i] = (int)buf[2 + i];
    opt_apply();
}

static void armas2_fits(void);   // [armas2]
void game_init(const char *dir)
{
    armas2_fits();   // [armas2]
    snd_init();
    if (!dir || !dir[0]) dir = "ms0:/PSP/GAME/PEPEL";
    { char mp[200]; snprintf(mp, sizeof(mp), "%s/music.raw", dir); snd_music_open(mp); }   // [armas2]
    snprintf(g_sav, sizeof(g_sav), "%s/soinnka.sav", dir);
    snprintf(g_tmp, sizeof(g_tmp), "%s/soinnka.tmp", dir);
    snprintf(g_cfg, sizeof(g_cfg), "%s/soinnka.cfg", dir);
    cfg_load();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    new_game();
    g_has_save = save_read();
    g_state = GS_TITLE;
}

// ------------------------------------------------------------------ botones
static void read_input(void)
{
    static u32 old;
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(&pad, 1);
    in.held = pad.Buttons;
    in.pressed = pad.Buttons & ~old;
    old = pad.Buttons;
    float x = (pad.Lx - 128) / 127.0f, y = (pad.Ly - 128) / 127.0f, m = sqrtf(x * x + y * y);
    float dz = OPT_DZ[g_opt[OPT_DEAD]];
    if (m < dz) x = y = 0;
    else {
        float k = clampf((m - dz) / (0.95f - dz), 0, 1);
        x = x / m * k;
        y = y / m * k;
    }
    in.ax = x;
    in.ay = y;
}

// ------------------------------------------------------------------ dano
// Sonido de algo que pasa en el mundo: baja con la distancia al jugador, se panea según esté a su izquierda o derecha
// y más allá de `range` px no suena
static void snd_at(int id, float vol, float x, float y, float range)
{
    float d = dist(x, y, P.x, P.y);
    if (d >= range) return;
    float k = 1 - d / range;
    snd_play(id, vol * k, clampf((x - P.x) / 110, -1, 1));
}
static float g_in_guard;   // tiempo restante sin aceptar pulsaciones nuevas tras un cambio de pantalla
static float g_step_t;   // reloj de los pasos del jugador

// Suma radiación al jugador (en la versión de pruebas, el modo sin daño la frena)
static void add_rad(float v)
{
#ifdef PEPEL_DEBUG
    if (g_dbg_god) return;
#endif
    P.rad = clampf(P.rad + v, 0, 100);
}

static void hurt_player(float dmg)
{
    if (g_state != GS_PLAY) return;
#ifdef PEPEL_DEBUG
    if (g_dbg_god && dmg < 900) return;
#endif
    int gi = g_eq[EQ_ARMOR] - 1;
    if (gi >= 0 && g_gcond[gi] > 0) {   // la armadura frena su parte del golpe y se gasta: más cuanto más fuerte
        float c0 = g_gcond[gi];
        g_gcond[gi] = clampf(c0 - (1.5f + dmg * 0.3f), 0, 100);
        dmg *= 1 - GEAR[gi].prot;
        if (g_gcond[gi] <= 0) msg("Tu armadura quedó inservible: ya no protege.");
        else if (c0 >= 25 && g_gcond[gi] < 25) msg("Tu armadura está muy gastada.");
    }
    if (g_mode == MODE_EXPL) dmg *= 0.7f;   // Explorador: golpes 30 % más flojos
    dmg *= art_taken();                     // [artef] Piel de plomo
    if (dmg < 900) { dmg *= duro_dmg(); duro_hit(dmg); }   // [duro]
    if (dmg < 900) dmg *= pozo_dmg();       // [pozo]
    P.hp -= dmg;
    P.hurt_t = 0.3f;
    snd_play(SFX_HURT, 0.9f, 0);
    if (g_shake < 3) g_shake = 3;
    blood(P.x, P.y, 5);
    if (P.hp <= 0 && !pozo_revive()) { P.hp = 0; g_state = GS_DEAD; g_state_t = 0; }   // [pozo]
}

// El ruido de un disparo llama a los que estan cerca
static void noise(float x, float y, float radius)
{
    radius *= duro_hear();   // [duro]
    if (dz_llueve() && !indoors(P.x, P.y)) radius *= 0.72f;   // [lluvia] la lluvia tapa tus pasos
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        if (e->state != ES_IDLE || is_post(e)) continue;   // los guardias no dejan su puesto por un ruido
        if (dist(x, y, e->x, e->y) > radius) continue;
        e->state = ES_ALERT;
        e->tx = x; e->ty = y;
        e->t = 0;
    }
}

// Un arma que se encuentra (jefe, francotirador, cajón): pasa a tu poder con algo de munición. w < 0: la primera de las mejores que aún no
// tienes. Si ya la tienes, queda chatarra. Devuelve 1 si diste un arma
static int award_weapon(int w, float x, float y)
{
    static const int BEST[] = {W_BOLT, W_HALCON, W_MAUL, W_APLANA, W_SMG, W_CENTINELA, W_ZUMBA, W_TORM, W_REVOLVER, W_CHACAL, W_PUMP, W_CUERVO, W_VIBORA};   // [armas2]
    char s[96];
    if (w < 0) for (unsigned i = 0; i < sizeof(BEST) / sizeof(BEST[0]); i++) if (!g_w[BEST[i]].owned && !g_arm[BEST[i]].owned) { w = BEST[i]; break; }   // [qsist]
    if (w < 0 || g_w[w].owned || g_arm[w].owned) { drop(x, y - 6, ITEM_SCRAP, 25); return 0; }
    g_w[w] = (WeaponState){1, WDEF[w].mag, 0.9f};
    peso_found_cond(w);   // [peso] las armas halladas vienen gastadas
    if (peso_total() > peso_cap() * 1.4f) {   // [qsist] no cabe en la mochila: va directo al armero
        g_arm[w] = g_w[w]; memset(&g_w[w], 0, sizeof(g_w[w]));
        if (!is_melee(w)) drop(x, y - 6, WDEF[w].ammo[0].item, 12);
        snprintf(s, sizeof(s), "Encuentras: %s. Vas muy cargado: queda en el armero.", WDEF[w].name);
        msg(s);
        return 1;
    }
    if (!is_melee(w)) drop(x, y - 6, WDEF[w].ammo[0].item, 12);
    snprintf(s, sizeof(s), "Encuentras: %s. Cambia de arma con izquierda o derecha.", WDEF[w].name);
    msg(s);
    return 1;
}

static void hurt_enemy(Enemy *e, float dmg, float fromx, float fromy, int by_player)
{
    e->hp -= dmg;
    e->flash = 0.12f;
    blood(e->x, e->y, 4);
    {   // salpicadura hacia atras del tiro
        float a = atan2f(e->y - fromy, e->x - fromx) + frange(-0.35f, 0.35f), r = frange(8, 22);
        decal(e->x + cosf(a) * r, e->y + sinf(a) * r, S_BLOOD0 + (int)(rnd() % 3), frange(0.5f, 0.9f));
    }
    if (e->hp > 0) {
        if (e->state != ES_COMBAT) {
            e->state = ES_COMBAT; e->cd = frange(0.3f, 0.6f);
            if (!is_human(e)) snd_at(is_boss(e) ? SFX_BOSS_ROAR : SFX_GROWL, 0.8f, e->x, e->y, 260);
        }
        e->tx = fromx; e->ty = fromy;
        e->see_t = 0;
        return;
    }
    e->state = ES_DEAD;
    e->dead_t = g_time;
    if (by_player) {   // las muertes entre ellos no cuentan para tus trabajos
        g_kills++;
        if (g_job == 2 && e->fac == F_CREA && g_job_n < JOBS[1].goal) g_job_n++;   // trabajo "Limpieza": zombis
        if (g_fjob == 3 && !g_fjn && e->type == E_SNP && e->fac == F_BAND) { g_fjn = 1; msg("Francotirador abatido. Cuéntaselo al puesto farolero."); }
        if (g_job == 4 && (e->type == E_BAN || e->type == E_SNP) && g_job_n < JOBS[3].goal) g_job_n++;   // trabajo "Ajuste de cuentas": bandidos
    }
    blood(e->x, e->y, 10);
    decal(e->x, e->y, S_BLOOD0 + (int)(rnd() % 3), 1.9f);
    for (int i = 0; i < 3; i++) decal(e->x + frange(-12, 12), e->y + frange(-12, 12), S_BLOOD0 + (int)(rnd() % 3), frange(0.5f, 1.0f));
    if (is_human(e)) {   // bandidos y guardias: munición y, a veces, una pieza de arma
        drop(e->x, e->y, e->var ? ITEM_762 : ITEM_9MM, e->var ? irange(3, 6) : irange(4, 8));
        if (e->type == E_BAN && frand() < 0.35f) {
            static const int pieza[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK};
            drop(e->x + 7, e->y + 4, pieza[rnd() % 5], 1);
        }
    }
    if (e->type == E_SNP) {   // el francotirador: munición de fusil, a veces perforante, y de vez en cuando su arma
        drop(e->x + 6, e->y + 4, frand() < 0.4f ? ITEM_762_AP : ITEM_762, irange(3, 6));
        if (frand() < 0.35f) drop(e->x - 6, e->y + 4, ITEM_REFLEX, 1);
        if (by_player && frand() < 0.12f) award_weapon(W_BOLT, e->x, e->y);
    }
    if (e->type == E_ARM) { drop(e->x, e->y, ITEM_SCRAP, irange(5, 10)); if (frand() < 0.6f) drop(e->x + 6, e->y + 4, ITEM_MECH, 1); }
    if (e->type == E_SPIT && frand() < 0.5f) drop(e->x, e->y, ITEM_CHEM, 1);
    if (e->type == E_WOLF && frand() < 0.5f) drop(e->x, e->y, ITEM_FOOD, 1);   // la carne del lobo
    if (e->mini) mj_loot(e);   // [armas2]
    if (e->ev != EVN_POZO) bk_boss_drop(e->type, e->x, e->y);   // [bunker] trofeos [arregla]
    if (e->type == E_BOSS2) {   // botín de Cabeza de Hierro: chatarra, munición, piezas, materiales y un arma
        static const int pieza[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK};
        drop(e->x - 10, e->y + 2, ITEM_SCRAP, irange(30, 40));
        drop(e->x + 10, e->y + 2, ITEM_SCRAP, irange(20, 30));
        drop(e->x, e->y + 10, ITEM_762_AP, irange(8, 12));
        drop(e->x - 14, e->y + 9, ITEM_9MM_HP, irange(8, 12));
        drop(e->x + 14, e->y + 9, ITEM_12G_SLUG, irange(4, 6));
        drop(e->x - 5, e->y - 8, pieza[rnd() % 5], 1);
        drop(e->x + 6, e->y - 7, pieza[rnd() % 5], 1);
        drop(e->x, e->y + 17, ITEM_MECH, irange(2, 3));
        drop(e->x + 12, e->y - 9, ITEM_MED, 1);
        award_weapon(-1, e->x, e->y);
        msg(by_player ? "Cabeza de Hierro ha caído." : "Cabeza de Hierro ha muerto.");
    }
    if (e->irr) drop(e->x - 6, e->y + 5, ITEM_CHEM, 1);   // el irradiado suelta un químico
    if (e->type == E_BOSS && e->ev != EVN_POZO) g_boss_dead = 1;   // [arregla]
    jefes_muere(e);   // [jefes]
    pozo_muere(e);   // [pozo]
    artef_muere(e);   // [artef]
    if (e->type == E_BOSS) {   // botín grande: chatarra, munición especial y piezas de arma
        static const int pieza[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK};
        drop(e->x - 10, e->y + 2, ITEM_SCRAP, irange(25, 35));
        drop(e->x + 10, e->y + 2, ITEM_SCRAP, irange(15, 25));
        drop(e->x, e->y + 10, ITEM_9MM_HP, irange(8, 12));
        drop(e->x - 14, e->y + 9, ITEM_762_AP, irange(6, 9));
        drop(e->x + 14, e->y + 9, ITEM_12G_SLUG, irange(3, 5));
        drop(e->x - 5, e->y - 8, pieza[rnd() % 5], 1);
        drop(e->x + 6, e->y - 7, pieza[rnd() % 5], 1);
        drop(e->x, e->y + 17, ITEM_CHEM, irange(2, 3));
        msg(by_player ? "La Bestia del charco ha caído." : "La Bestia del charco ha muerto.");
    }
    if (e->type == E_BIG) {
        drop(e->x, e->y, ITEM_SCRAP, irange(4, 9));
        if (frand() < 0.5f) drop(e->x + 7, e->y + 3, ITEM_12G, irange(1, 3));
    }
    if (e->type == E_MAR) {
        float r = frand();
        if (r < 0.55f) drop(e->x, e->y, ITEM_9MM, irange(3, 7));
        else if (r < 0.60f) drop(e->x, e->y, ITEM_762, irange(2, 4));
        else if (r < 0.75f) drop(e->x, e->y, ITEM_12G, irange(1, 2));
        else if (r < 0.90f) drop(e->x, e->y, ITEM_BAND, 1);
        if (frand() < 0.5f) drop(e->x + 6, e->y + 4, ITEM_SCRAP, irange(2, 6));
    }
    for (int i = 0; i < g_ne; i++)
        if (i == P.lock && &g_e[i] == e) P.lock = -1;
}

// ------------------------------------------------------------------ jugador
// Se le puede apuntar a lo que esta a la vista y alumbrado (o casi encima)
static int can_target(const Enemy *e, float max_d)
{
    if (e->state == ES_DEAD || is_post(e)) return 0;   // la mira nunca se fija en un guardia amigo
    float d = dist(P.x, P.y, e->x, e->y);
    if (d > max_d || !los(P.x, P.y, e->x, e->y)) return 0;
    return d < 64 || light_total(e->x, e->y) > 0.22f;
}

// El blanco mas cercano; con after >= 0, el que le sigue en distancia (para ir pasando con L)
static int pick_target(float max_d, int after)
{
    float da = after >= 0 ? dist(P.x, P.y, g_e[after].x, g_e[after].y) : -1;
    int best = -1, first = -1;
    float best_d = 1e9f, first_d = 1e9f;
    for (int i = 0; i < g_ne; i++) {
        if (i == after || !can_target(&g_e[i], max_d)) continue;
        float d = dist(P.x, P.y, g_e[i].x, g_e[i].y);
        if (d < first_d) { first_d = d; first = i; }
        if (d > da && d < best_d) { best_d = d; best = i; }
    }
    return best >= 0 ? best : first;
}

// El siguiente tipo de munición del arma `wi` del que llevas algo, después del cargado; -1 si no tienes de otro
static int ammo_next(int wi)
{
    const WeaponDef *d = &WDEF[wi];
    for (int k = 1; k < d->nammo; k++) {
        int j = (g_w[wi].load + k) % d->nammo;
        if (g_bag[d->ammo[j].item] > 0) return j;
    }
    return -1;
}
// Lo que dice cuando no quedan balas del tipo cargado
static void no_ammo_msg(void)
{
    msg(ammo_next(P.weapon) >= 0 ? "No te quedan de ese tipo. Mantén Cuadrado para cambiar de munición." : "No te queda munición para esta arma.");
}

static void start_reload(void)
{
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    if (P.reload_t > 0 || w->mag >= w_mag(P.weapon)) return;
    if (g_bag[d->ammo[w->load].item] <= 0) { no_ammo_msg(); return; }
    P.reload_t = d->reload * (g_sk[SK_MANOS] ? 0.75f : 1);   // Manos rápidas
    if (uniq_part_on(ITEM_UP2)) P.reload_t = 0.05f;   // [artef] Cargador de la Viuda: al instante
    snd_play(SFX_RELOAD, 0.7f, 0);
}

// Desgaste y trabas. Cada tiro gasta el arma (WDEF.wear: unos 250 tiros de nueva a inservible la Makarov y la
// recortada, 200 el fusil) y el kit de limpieza le devuelve 25 puntos, solo si está bajo 80 %.
// Probabilidad de que se trabe en un tiro, según su estado (de 0 a 1): sobre 80 % casi nunca (1 de cada 130 tiros o
// menos), de ahí hacia abajo cada vez más (1 de cada 38 en 60 %, 1 de cada 18 en 40 %) y bajo 30 % se traba seguido
// (1 de cada 5 o 6 tiros, y hasta 1 de cada 3). WDEF.jam lo ajusta por arma (el fusil, que dispara mucho, la mitad)
#define KIT_COND 0.80f    // el kit de limpieza solo se usa con el arma bajo este estado
#define BAD_COND 0.30f    // bajo este estado el arma se traba seguido
static float jam_chance(float cond)
{
    if (g_bag[ITEM_R_CARGADOR]) return 0;   // [pozo]
    float w = 1 - cond, p = 0.01f * w + 0.14f * w * w;
    if (cond < BAD_COND) p += 0.10f + (BAD_COND - cond) * 0.5f;
    return p;
}

// Daño de un impacto según el tipo de bala y el blindaje del enemigo
static float bullet_dmg(float dmg, int kind, float armor)
{
    dmg *= art_dmg();   // [artef] Colmillo rojo
    if (kind == B_AP) return dmg * 0.9f;                                       // perforante: ignora el blindaje, 10 % menos de daño
    if (kind == B_HP) return armor <= 0 ? dmg * 1.35f : dmg * (1 - fminf(armor * 2, 0.9f));   // expansiva: el blindaje cuenta doble
    return dmg * (1 - armor);
}

// ---- cuerpo a cuerpo y empujones
// Cuánto cuesta empujar a un enemigo (1 = normal): los grandes y los jefes casi no se mueven
static float e_mass(const Enemy *e)
{
    switch (e->type) {
    case E_BOSS: return 0.12f;
    case E_BOSS2: return 0.1f;
    case E_VIUDA: return 0.14f;   // [jefes2]
    case E_CAPATAZ: return 0.2f;   // [jefes2]
    case E_FERMEN: return 0.12f;   // [jefes2]
    case E_BIG: return 0.35f;
    case E_ARM: return 0.5f;
    default: return 1;
    }
}
// Empuja al jugador `px` píxeles hacia `ang` (se frena solo y no atraviesa muros)
static void push_player(float ang, float px) { P.kx += cosf(ang) * px * 8; P.ky += sinf(ang) * px * 8; }
// Un cuadro del empujón de un enemigo: avanza por el mapa sin atravesar muros; si choca con uno, se detiene y recibe un poco de daño extra
static void knock_step(Enemy *e, float dt)
{
    float sp = sqrtf(e->kx * e->kx + e->ky * e->ky);
    int n = (int)(sp * dt / 3) + 1, wall = 0;
    for (int i = 0; i < n && !wall; i++) {
        float nx = e->kx * dt / n, ny = e->ky * dt / n;
        if (!box_hits(e->x + nx, e->y, 5)) e->x += nx; else wall = 1;
        if (!box_hits(e->x, e->y + ny, 5)) e->y += ny; else wall = 1;
    }
    if (wall) {
        if (sp > 60) {
#ifdef PEPEL_DEBUG
            g_dbg_walls++; g_dbg_wsp = sp;
#endif
            sparks_n(e->x, e->y - 6, 6, 1.4f);
            snd_at(SFX_WALL, 0.8f, e->x, e->y, 240);
            hurt_enemy(e, 4 + sp * 0.025f, P.x, P.y, 1);
        }
        e->kx = e->ky = 0;
    } else {
        float f = expf(-7 * dt);
        e->kx *= f; e->ky *= f;
        if (sp * f < 8) e->kx = e->ky = 0;
    }
}
// Golpe sigiloso: el cuchillo contra un enemigo que aún no te ha visto (ES_IDLE) y que te da la espalda. Mata de un golpe;
// a los jefes y a los grandes les hace el triple de daño
static int stealth_ok(const Enemy *e)
{
    if (P.weapon != W_KNIFE || e->state != ES_IDLE || is_post(e)) return 0;
    return fabsf(angdiff(e->ang, atan2f(P.y - e->y, P.x - e->x))) > 2.0f;
}
// ¿Lo alcanza el golpe? Dentro del alcance y del arco, a la vista y sin guardias amigos
static int melee_reach(const Enemy *e, float a, float range, float arc, float *dd)
{
    if (e->state == ES_DEAD || is_post(e)) return 0;
    float dx = e->x - P.x, dy = e->y - P.y;
    *dd = sqrtf(dx * dx + dy * dy);
    if (*dd > range + hit_r(e)) return 0;
    if (*dd > 10 && fabsf(angdiff(a, atan2f(dy, dx))) > arc * 0.5f) return 0;
    return los(P.x, P.y, e->x, e->y);
}
// El golpe: un arco corto hacia el blanco del apuntado automático (o hacia donde miras). Daña, empuja hacia atrás y aturde un instante.
// El Acorazado recibe el daño completo (sus placas solo frenan balas); los jefes y los grandes casi no se mueven
static void melee_swing(void)
{
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    const MelDef *m = &MEL[P.weapon - W_KNIFE];
    float a = P.lock >= 0 ? atan2f(g_e[P.lock].y - P.y, g_e[P.lock].x - P.x) : P.ang, dd, bd = 1e9f, nd = 1e9f;
    for (int i = 0; i < g_ne; i++) {   // lo más cercano a tu alcance (aunque te muerda por un costado) manda sobre el blanco fijado
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e)) continue;
        float d2 = dist(P.x, P.y, e->x, e->y);
        if (d2 < d->range + hit_r(e) + 8 && d2 < nd && los(P.x, P.y, e->x, e->y)) { nd = d2; a = atan2f(e->y - P.y, e->x - P.x); }
    }
    float wear = 0.6f + 0.4f * w->cond;   // gastada, pega menos
    int best = -1, hit = 0;
    if (!m->all)
        for (int i = 0; i < g_ne; i++)
            if (melee_reach(&g_e[i], a, d->range, m->arc, &dd) && dd < bd) { bd = dd; best = i; }
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        if (!melee_reach(e, a, d->range, m->arc, &dd) || (!m->all && i != best)) continue;
        float ea = atan2f(e->y - P.y, e->x - P.x), k = e_mass(e);
        float dm = d->dmg * wear * art_dmg() * (g_sk[SK_CARNI] ? 1.25f : 1) * (e->type == E_ARM ? 1 : 1 - e->armor * 0.5f);
        int sneak = stealth_ok(e);
        if (sneak) {
            dm = is_boss(e) || e->type == E_BIG || e->type == E_ARM ? dm * 3 : e->hp + 1;
            pop(e->x, e->y - 18, "Golpe sigiloso", RGBA(236, 190, 120, 255));
        }
        hurt_enemy(e, dm, P.x, P.y, 1);
        hit++;
        sparks_n(e->x, e->y - 6, 4, 1.1f);
        if (e->state != ES_DEAD) {
            e->kx = cosf(ea) * m->push * 7 * k; e->ky = sinf(ea) * m->push * 7 * k;
            e->stun = m->stun * (k >= 0.5f ? 1 : k >= 0.3f ? 0.4f : 0);
        }
    }
#ifdef PEPEL_DEBUG
    g_dbg_swhits += hit;
#endif
    snd_play(hit ? SFX_MEL_HIT : SFX_SWING, hit ? 1 : 0.7f, 0);
    float sh = hit ? d->kick : d->kick * 0.3f;
    if (g_shake < sh) g_shake = sh;
    w->cond = clampf(w->cond - d->wear, 0.05f, 1);
    P.fire_cd = d->rate;
    P.ang = a;
    g_swing_t = g_swing_dur = fminf(0.24f, d->rate * 0.8f);
    g_swing_a = a; g_swing_arc = m->arc;
    noise(P.x, P.y, d->noise);
}

static void shoot(void)
{
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    float a = P.ang;
    if (P.lock >= 0) a = atan2f(g_e[P.lock].y - P.y, g_e[P.lock].x - P.x);   // apuntado automatico
    int kind = d->ammo[w->load].kind;
    WeaponStats st = w_stats(P.weapon);   // con el tipo de munición cargado y las piezas puestas
    float spread = st.spread * (P.moving ? st.move : 1.0f) * ((in.held & PSP_CTRL_LTRIGGER) ? st.aim : 1.0f);
    float mx = P.x + cosf(P.ang) * 9, my = P.y + sinf(P.ang) * 9;
    for (int k = 0; k < st.pellets; k++) {
        for (int i = 0; i < MAXB; i++) {
            if (g_b[i].on) continue;
            float ba = a + frange(-spread, spread), sp = d->speed * frange(0.92f, 1.08f);
            g_b[i] = (Bullet){mx, my, cosf(ba) * sp, sinf(ba) * sp, st.range / d->speed, st.dmg, 1, 1, kind, 0, d->pierce, 0};
            break;
        }
    }
    w->mag--;
    w->cond = clampf(w->cond - d->wear * armas_wear(P.weapon), 0.05f, 1);   // [armas]
    P.fire_cd = st.rate;
    P.flash_t = 0.06f;
    if (g_shake < d->kick) g_shake = d->kick;
    float ca = P.ang + PI / 2;   // el casquillo salta al costado
    part(P.x, P.y - BODY_H, cosf(ca) * 40, sinf(ca) * 40, 0.35f, COL_CASING, 1);
    noise(P.x, P.y, st.noise);
    if (st.noise < d->noise * 0.5f) snd_play(SFX_SHOT_SIL, 0.9f, 0);   // con silenciador
    else snd_play(d->pellets > 1 ? SFX_SHOT_SHOTGUN : d->speed > 500 ? SFX_SHOT_RIFLE : SFX_SHOT_PISTOL, 1, 0);
    snd_play(SFX_CASING, 0.6f, 0.4f);
}

// Se pone la prenda `gi` (está en la mochila). Si había otra en esa ranura, vuelve a la mochila (cabe: solo hay una de cada una)
static void gear_equip(int gi)
{
    const GearDef *d = &GEAR[gi];
    char s[96];
    int sl = d->slot;
    if (sl == EQ_ART1 && g_eq[EQ_ART1] && !g_eq[EQ_ART2]) sl = EQ_ART2;   // el segundo artefacto va a la otra ranura
    if (d->slot == EQ_ART1 && g_eq[EQ_ART1] && g_eq[EQ_ART2] && !g_eq[EQ_ART3] && rank() >= 3) sl = EQ_ART3;   // [artef] el tercero, desde rango 3
    int old = g_eq[sl] - 1;
    if (inv_take(d->item, 1) < 1) return;
    if (old >= 0) g_bag[GEAR[old].item]++;
    g_eq[sl] = gi + 1;
    if (d->slot == EQ_ARMOR || d->slot == EQ_MASK) snprintf(s, sizeof(s), d->slot == EQ_MASK ? "Te pones: %s (filtro %d %%)." : "Te pones: %s (estado %d %%).", d->name, (int)ceilf(g_gcond[gi]));
    else snprintf(s, sizeof(s), "Te pones: %s. %s.", d->name, ITEMS[d->item].use);
    msg(s);
}
#ifdef PEPEL_DEBUG
// Pruebas: escenario de guardado (alijo, piezas, equipo puesto, trabajo activo)
static void dbg_scenario(void)
{
    for (int i = 0; i < ITEM_COUNT; i++) if (ITEMS[i].cap && gear_of(i) < 0) g_stash[i] = 3;
    g_w[W_AK].owned = 1; g_w[W_AK].part[SLOT_MUZZLE] = 1; g_w[W_AK].part[SLOT_MAG] = 3; g_w[W_AK].part[SLOT_SIGHT] = 4;
    g_w[W_MAKAROV].part[SLOT_MUZZLE] = 1;
    for (int gi = 0; gi < GEAR_N; gi++) { g_gcond[gi] = 100; if (g_eq[GEAR[gi].slot] == 0) { g_bag[GEAR[gi].item] = 1; gear_equip(gi); } }
    g_job = 5; g_job_n = 0;   // Caza mayor: su avance es la Bestia muerta
}
#endif
// Se quita lo que lleva en esa ranura: vuelve a la mochila
static void gear_unequip(int slot)
{
    char s[96];
    int gi = g_eq[slot] - 1;
    if (gi < 0) { msg(slot == EQ_MASK ? "No llevas máscara." : slot == EQ_ARMOR ? "No llevas armadura." : "Esa ranura está vacía."); return; }
    g_bag[GEAR[gi].item]++;
    g_eq[slot] = 0;
    snprintf(s, sizeof(s), "Te quitas: %s. Queda en la mochila.", GEAR[gi].name);
    msg(s);
}

// Usa un objeto (con las flechas o desde la mochila). Si no se puede, avisa por qué y devuelve 0
static int use_item(int item)
{
    { int dr = duro_use(item); if (dr) return dr == 1; }   // [duro]
    int gear = gear_of(item);
    if (gear >= 0) {   // equipo: se pone
        if (g_bag[item] <= 0) return 0;
        gear_equip(gear);
        return 1;
    }
    for (int h = 0; h < H_N; h++)
        if (HAND_ITEM[h] == item) { g_hand = h; msg("Objeto de mano elegido. Lánzalo con L + Triángulo."); return 1; }
    switch (item) {
    case ITEM_BAND:
        if (g_bag[ITEM_BAND] <= 0) { msg("No te quedan vendas."); return 0; }
        if (P.bleed <= 0 && P.hp >= hp_max()) { msg("No estás herido."); return 0; }
        msg(P.bleed > 0 ? "Te vendas la herida." : "Te vendas.");
        g_bag[ITEM_BAND]--; P.bleed = 0; P.hp = clampf(P.hp + 8 * art_heal(), 0, hp_max()); snd_play(SFX_BAND, 0.8f, 0);
        return 1;
    case ITEM_MED:
        if (g_bag[ITEM_MED] <= 0) { msg("No te quedan botiquines."); return 0; }
        if (P.bleed <= 0 && P.hp >= hp_max()) { msg("No estás herido."); return 0; }
        g_bag[ITEM_MED]--; P.bleed = 0; P.hp = clampf(P.hp + 45 * art_heal(), 0, hp_max()); snd_play(SFX_BAND, 0.8f, 0);
        msg("Usas un botiquín.");
        return 1;
    case ITEM_FOOD:   // la conserva: +35 de hambre y +6 de vida
        if (g_bag[ITEM_FOOD] <= 0) { msg("No te quedan conservas."); return 0; }
        if (P.hunger >= 100 && P.hp >= hp_max()) { msg("No tienes hambre."); return 0; }
        g_bag[ITEM_FOOD]--; P.hunger = clampf(P.hunger + 35, 0, 100); P.hp = clampf(P.hp + 6 * art_heal(), 0, hp_max()); snd_play(SFX_EAT, 0.8f, 0);
        msg("Comes una conserva.");
        return 1;
    case ITEM_WATER:  // la botella de agua: +45 de sed
        if (g_bag[ITEM_WATER] <= 0) { msg("No te queda agua."); return 0; }
        if (P.thirst >= 100) { msg("No tienes sed."); return 0; }
        g_bag[ITEM_WATER]--; P.thirst = clampf(P.thirst + 45, 0, 100); snd_play(SFX_DRINK, 0.8f, 0);
        msg("Bebes agua.");
        return 1;
    case ITEM_ARAD:
        if (g_bag[ITEM_ARAD] <= 0) { msg("No tienes antirradiación."); return 0; }
        if (P.rad <= 1) { msg("No tienes radiación."); return 0; }
        g_bag[ITEM_ARAD]--; P.rad = clampf(P.rad - 45, 0, 100); snd_play(SFX_BAND, 0.8f, 0);
        msg("Te inyectas antirradiación.");
        return 1;
    case ITEM_CLEAN: {   // kit de limpieza: +25 puntos de estado al arma en mano, solo si está bajo 80 %
        WeaponState *w = &g_w[P.weapon];
        char s[96];
        if (g_bag[ITEM_CLEAN] <= 0) { msg("No te quedan kits de limpieza."); return 0; }
        if (w->cond >= KIT_COND) {
            snprintf(s, sizeof(s), "%s está en buen estado (%d %%): el kit sirve bajo %d %%.", WDEF[P.weapon].name, (int)(w->cond * 100), (int)(KIT_COND * 100));
            msg(s);
            return 0;
        }
        g_bag[ITEM_CLEAN]--; w->cond = clampf(w->cond + 0.25f, 0.05f, 1);
        snprintf(s, sizeof(s), "Limpias el arma: %s queda en %d %%.", WDEF[P.weapon].name, (int)(w->cond * 100));
        msg(s);
        return 1;
    }
    case ITEM_SEW: {     // kit de costura: +40 puntos de estado a la armadura puesta
        int gi = g_eq[EQ_ARMOR] - 1;
        char s[96];
        if (g_bag[ITEM_SEW] <= 0) { msg("No te quedan kits de costura."); return 0; }
        if (gi < 0) { msg("No llevas armadura puesta: póntela para remendarla."); return 0; }
        if (g_gcond[gi] > 99) { msg("La armadura está como nueva."); return 0; }
        g_bag[ITEM_SEW]--; g_gcond[gi] = clampf(g_gcond[gi] + 40, 0, 100);
        snprintf(s, sizeof(s), "Remiendas la armadura: %s queda en %d %%.", GEAR[gi].name, (int)ceilf(g_gcond[gi]));
        msg(s);
        return 1;
    }
    case ITEM_FILTER: {   // filtro nuevo para la máscara (puesta o en la mochila)
        int gi = gear_of(ITEM_MASK);
        if (g_bag[ITEM_FILTER] <= 0) { msg("No te quedan filtros."); return 0; }
        if (!gear_worn(gi) && g_bag[ITEM_MASK] <= 0) { msg("No llevas una máscara de gas."); return 0; }
        if (g_gcond[gi] > 99) { msg("El filtro de la máscara está nuevo."); return 0; }
        g_bag[ITEM_FILTER]--; g_gcond[gi] = 100;
        msg("Cambias el filtro de la máscara.");
        return 1;
    }
    }
    for (int i = 0; i < PARTS_N; i++)
        if (PARTS[i].item == item) { msg("Las piezas se instalan en el taller de Estación Kolos."); return 0; }
    msg("Eso no se usa desde la mochila.");
    return 0;
}

// Flecha Arriba: curarse con venda o botiquín (la conserva ya no cura: se come con Abajo)
static void heal(void)
{
    if (duro_heal_start()) return;   // [duro]
    int band = g_bag[ITEM_BAND] > 0, med = g_bag[ITEM_MED] > 0;
    if (P.bleed > 0 && band) use_item(ITEM_BAND);
    else if (P.hp < 65 && med) use_item(ITEM_MED);
    else if (P.hp < hp_max() && band) use_item(ITEM_BAND);
    else if (P.hp < hp_max() && med) use_item(ITEM_MED);
    else msg(P.hp >= hp_max() && P.bleed <= 0 ? "No estás herido." : "No tienes con qué curarte.");
}

// Flecha Abajo: come o bebe lo que más falta; si no falta nada y hay radiación, antirradiación.
// "Falta" es estar bajo NEED: así no se gasta una conserva por un par de puntos.
#define NEED 80
static void eat_drink(void)
{
    int hungry = P.hunger < NEED, thirsty = P.thirst < NEED;
    int f = hungry && g_bag[ITEM_FOOD] > 0, w = thirsty && g_bag[ITEM_WATER] > 0;
    if (w && (!f || P.thirst <= P.hunger)) use_item(ITEM_WATER);
    else if (f) use_item(ITEM_FOOD);
    else if (P.rad > 1 && g_bag[ITEM_ARAD] > 0) use_item(ITEM_ARAD);
    else if (hungry || thirsty) msg(hungry && thirsty ? "No tienes qué comer ni qué beber." : thirsty ? "No te queda agua." : "No te quedan conservas.");
    else msg(P.rad > 1 ? "No tienes antirradiación." : "No tienes hambre ni sed.");
}

// Hay algún enemigo en combate a menos de r px del jugador
static int threat_near(float r)
{
    for (int i = 0; i < g_ne; i++)
        if (g_e[i].state == ES_COMBAT && dist(P.x, P.y, g_e[i].x, g_e[i].y) < r) return 1;
    return 0;
}

static Crate *near_crate(void)
{
    Crate *best = NULL;
    float bd = 24;
    for (int i = 0; i < g_ncr; i++) {
        if (g_cr[i].open) continue;
        float d = dist(P.x, P.y, g_cr[i].tx * TS + 8, g_cr[i].ty * TS + 8);
        if (d < bd) { bd = d; best = &g_cr[i]; }
    }
    return best;
}

// Lo que queda al alcance de Cruz, lo más cercano: 1 = un cajón o cuerpo (en *c), 2 = un catre
// (su casilla en *tx, *ty), 0 = nada
static int near_use(Crate **c, int *tx, int *ty)
{
    int px = (int)(P.x / TS), py = (int)(P.y / TS), cot = 0;
    float dc = 1e9f, dq = 24;
    *c = near_crate();
    if (*c) dc = dist(P.x, P.y, (*c)->tx * TS + 8, (*c)->ty * TS + 8);
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++) {
            if (!in_map(x, y) || (g_o[y][x] != O_COT && g_o[y][x] != O_BED)) continue;
            float d = dist(P.x, P.y, x * TS + 8, y * TS + 8);
            if (d < dq) { dq = d; cot = 1; *tx = x; *ty = y; }
        }
    float ds = 1e9f;   // una escalera de los edificios más cerca que el cajón o el catre: manda la escalera
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++)
            if (in_map(x, y) && (g_o[y][x] == O_STUP || g_o[y][x] == O_STDN)) { float d = dist(P.x, P.y, x * TS + 8, y * TS + 8); if (d < ds) ds = d; }
    if (ds < 26 && ds <= (cot && dq < dc ? dq : dc)) return 0;
    if (cot && dq < dc) return 2;
    return *c ? 1 : 0;
}

// Lo especial al alcance de Cruz: 1 escotilla, 2 escalera, 3 Chofer, 4 comerciante, 5 chatarrero, 6 arcón o estantería (alijo), 7 enfermería, 8 escalera de subir un piso, 9 escalera de bajar un piso, 0 nada. Deja su casilla en *tx, *ty
static int near_spot(int *tx, int *ty)
{
    int px = (int)(P.x / TS), py = (int)(P.y / TS), best = 0;
    float bd = 26;
    for (int y = py - 2; y <= py + 2; y++)
        for (int x = px - 2; x <= px + 2; x++) {
            if (!in_map(x, y)) continue;
            int o = g_o[y][x], k = o == O_HATCH ? 1 : o == O_STAIRS ? 2 : o == O_NPC ? 3 : o == O_MERCH ? 4 : o == O_SCAV ? 5 : (o == O_CHEST || o == O_CHEST2 || o == O_SHELF) ? 6 : o == O_CLINIC ? 7 : o == O_STUP ? 8 : o == O_STDN ? 9 : 0;
            if (!k) continue;
            float d = dist(P.x, P.y, x * TS + 8, y * TS + 8);
            if (d < bd) { bd = d; best = k; *tx = x; *ty = y; }
        }
    return best;
}

// La enfermería de la base: una vez por vuelta de expedición deja la vida en 100 y quita el sangrado y la radiación
static void clinic_use(void)
{
    if (g_clinic_used) { msg("Ya usaste la enfermería en esta vuelta. Vuelve de una expedición."); return; }
    g_clinic_used = 1;
    P.hp = hp_max(); P.bleed = 0; P.rad = 0;
    snd_play(SFX_PICKUP, 0.8f, 0);
    msg("Te curan en la enfermería: vida al máximo, sin sangrado ni radiación.");
}

// Dormir en un catre: solo sin enemigos en combate cerca y con hambre y sed sobre 10. Si no, dice por qué
static void try_sleep(void)
{
    if (threat_near(150)) msg("No puedes dormir: hay enemigos cerca.");
    else if (P.hunger <= 10 || P.thirst <= 10)
        msg(P.thirst > 10 ? "No puedes dormir con tanta hambre." : P.hunger > 10 ? "No puedes dormir con tanta sed." : "No puedes dormir con tanta hambre y sed.");
    else { g_state = GS_SLEEP; g_state_t = 0; g_slept = 0; g_hora = 7; }   // [diseno] dormir lleva a la mañana siguiente
}

// Mete en la mochila lo que quepa (con su cartel) y devuelve cuántos sobraron
static int give(int item, int n, float x, float *y)
{
    char s[48];
    int take = inv_add(item, n);
    if (take <= 0) return n;
    snd_play(SFX_PICKUP, 0.7f, 0);
    if (item == ITEM_SCRAP) g_scrap_gain += take;
    snprintf(s, sizeof(s), "+%d %s", take, ITEMS[item].unit);
    pop(x, *y, s, item == ITEM_SCRAP ? RGBA(190, 190, 200, 255) : RGBA(236, 214, 150, 255));
    *y -= 11;
    return n - take;
}

// Lo que sobró al abrir un cajón o un cuerpo queda a los pies del jugador, para recogerlo cuando quepa
static void spill(int item, int n)
{
    Pick *k = drop(P.x + frange(-7, 7), P.y + frange(-4, 4), item, n);
    if (k) k->on = 2;   // el aviso lo da quien lo tiró
}

// Cuadrado mantenido: pasa al siguiente tipo de munición que tengas para el arma en mano y recarga con él.
// Lo que quedaba en el cargador vuelve a la mochila (lo que no quepa queda en el suelo)
static void ammo_switch(void)
{
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    char s[96];
    int next = ammo_next(P.weapon);
    if (P.jammed) { msg("Destraba el arma antes de cambiar de munición."); return; }
    if (is_melee(P.weapon)) { msg("Esta arma no usa munición."); return; }
    if (next < 0) { msg("No tienes otro tipo de munición para esta arma."); return; }
    if (w->mag > 0) {
        int item = d->ammo[w->load].item, back = inv_add(item, w->mag);
        if (back < w->mag) spill(item, w->mag - back);
        w->mag = 0;
    }
    w->load = next;
    P.reload_t = d->reload;
    snprintf(s, sizeof(s), "Cargas %s.", BDEF[d->ammo[next].kind].name);
    msg(s);
}

// Botín de un cajón o cuerpo: a la mochila y, lo que no quepa, al suelo. Devuelve 1 si algo sobró
static int loot(int item, int n, float x, float *y)
{
    n = duro_loot_n(item, n); if (n <= 0) return 0;   // [duro]
    if (item == ITEM_SCRAP && g_sk[SK_REBUS]) n = n * 3 / 2 + 1;   // Rebuscador
    if (item == ITEM_SCRAP && art_on(A_IMAN)) n = n * 3 / 2 + 1;   // [artef] Imán negro
    int left = give(item, n, x, y);
    if (left) spill(item, left);
    return left > 0;
}

// Lo que puede salir en un cajón o un cuerpo: una fila por objeto, con su peso (cuanto más alto, más seguido sale)
// y cuántos trae, de `lo` a `hi`
typedef struct { short item, weight, lo, hi, crate; } LootDef;   // crate: 1 = solo sale en cajones, no en cuerpos
static const LootDef LOOT[] = {
    {ITEM_9MM, 9, 2, 5}, {ITEM_762, 3, 2, 4}, {ITEM_12G, 4, 1, 2}, {ITEM_BAND, 13, 1, 1}, {ITEM_MED, 8, 1, 1},
    {ITEM_FOOD, 12, 1, 1}, {ITEM_WATER, 13, 1, 1}, {ITEM_ARAD, 6, 1, 1}, {ITEM_SCRAP, 5, 2, 6},
    // los tipos especiales de munición salen poco
    {ITEM_9MM_HP, 2, 2, 5}, {ITEM_9MM_AP, 2, 2, 4}, {ITEM_762_HP, 1, 2, 4}, {ITEM_762_AP, 1, 2, 4}, {ITEM_12G_SLUG, 2, 1, 2},
    {ITEM_CLEAN, 3, 1, 1},   // el kit de limpieza, poco frecuente
    {ITEM_NUT, 6, 2, 4}, {ITEM_MOLO, 3, 1, 1}, {ITEM_GREN, 2, 1, 1},   // objetos de mano
    {ITEM_MINE, 1, 1, 1, 1}, {ITEM_TRAP, 1, 1, 1, 1}, {ITEM_DET, 1, 1, 1, 1},                  // las trampas, solo en cajones
    // materiales de fabricación: frecuentes
    {ITEM_RAG, 12, 1, 2}, {ITEM_CHEM, 8, 1, 1}, {ITEM_MECH, 8, 1, 1}, {ITEM_POWDER, 7, 1, 2},
    // piezas de armas: raras, y solo en cajones
    {ITEM_XMAG, 1, 1, 1, 1}, {ITEM_COMP, 1, 1, 1, 1}, {ITEM_STOCK, 1, 1, 1, 1}, {ITEM_REFLEX, 1, 1, 1, 1}, {ITEM_SIL, 1, 1, 1, 1},
    {ITEM_REPAIR, 3, 1, 1},   // [armas]
    {ITEM_LONGB, 1, 1, 1, 1}, {ITEM_BRAKE, 1, 1, 1, 1}, {ITEM_SCOPE, 1, 1, 1, 1}, {ITEM_DRUM, 1, 1, 1, 1}, {ITEM_PADSTOCK, 1, 1, 1, 1},   // [armas]
    {ITEM_LIGHT, 1, 1, 1, 1}, {ITEM_LASER, 1, 1, 1, 1}, {ITEM_BIPOD, 1, 1, 1, 1}, {ITEM_BAYO, 1, 1, 1, 1},   // [armas]
    {ITEM_PACK1, 1, 1, 1, 1}, {ITEM_PACK2, 1, 1, 1, 1},   // [peso] mochilas, solo en cajones
    DURO_LOOT_ROWS   // [duro]
};
#define LOOT_N ((int)(sizeof(LOOT) / sizeof(LOOT[0])))
static const LootDef *loot_roll(int body)   // body: 1 = es un cuerpo (no trae lo que solo sale en cajones)
{
    int total = 0, i, last = 0;
    for (i = 0; i < LOOT_N; i++)
        if (!(body && LOOT[i].crate)) total += LOOT[i].weight;
    float r = frand() * total;
    for (i = 0; i < LOOT_N; i++) {
        if (body && LOOT[i].crate) continue;
        last = i;
        if ((r -= LOOT[i].weight) < 0) break;
    }
    return &LOOT[last];
}

// A veces, entre el botín de una expedición sale un arma que aún no tienes; qué armas dependen del peligro (más peligro, mejores)
static void weapon_find(const Crate *c, float x, float y)
{
    static const struct { short w, dmin; } POOL[] = {{W_AXE, 0}, {W_PIPE, 0}, {W_REVOLVER, 2}, {W_PUMP, 2}, {W_MAUL, 3}, {W_SMG, 3}, {W_BOLT, 5},
                                                  {W_VIBORA, 1}, {W_CHACAL, 3}, {W_ZUMBA, 3}, {W_TORM, 4}, {W_CUERVO, 3}, {W_HALCON, 4}, {W_CENTINELA, 6}, {W_APLANA, 7}};   // [armas2]
    int dg = g_zone == 2 ? 3 : g_x.danger, cand[16], n = 0;
    if (g_zone == 0) return;
    if (c->kind != CK_MIL && frand() >= (is_body(c) ? 0.05f : c->kind == CK_RICH ? 0.14f : 0.07f) + 0.012f * dg) return;   // [mil] la caja militar siempre pasa
    for (unsigned i = 0; i < sizeof(POOL) / sizeof(POOL[0]); i++)
        if (dg >= POOL[i].dmin && !g_w[POOL[i].w].owned && !g_arm[POOL[i].w].owned) cand[n++] = POOL[i].w;
    if (n) award_weapon(cand[rnd() % (u32)n], x, y);
    else if (c->kind == CK_MIL) award_weapon(-1, x, y);   // [mil]
}

static void open_crate(Crate *c)
{
    if (c->kind == CK_VEND) { dz_vend_open(c); return; }   // [diseno]
    float x = c->tx * TS + 8, y = c->ty * TS - 10;
    c->open = 1;
    snd_play(SFX_CRATE, 0.9f, 0);
    g_opened++;
    if (g_job == 3 && g_job_n < JOBS[2].goal) g_job_n++;   // trabajo "Rebusca"
    if (c->kind == CK_JOB) {
        P.has_job = 1;
        pop(x, y, "Caja de repuestos", RGBA(150, 220, 130, 255));
        msg("Tienes los repuestos. Llévaselos al comerciante de Estación Kolos.");
        return;
    }
    if (c->kind == CK_MEDS) {
        g_has_meds = 1;
        pop(x, y, "Caja de medicinas", RGBA(150, 220, 130, 255));
        msg("Tienes la caja de medicinas. Llévasela al comerciante de Estación Kolos.");
        return;
    }
    if (c->kind == CK_REG) {
        g_has_reg = 1;
        pop(x, y, "Registro del incendio", RGBA(150, 220, 130, 255));
        msg("Tienes el Registro del incendio. Llévaselo al comerciante de Estación Kolos.");
        return;
    }
    if (c->kind == CK_WEAPON && peso_crate_gun(x, y)) return;   // [peso] primera expedición: la Makarov
    if (c->kind == CK_WEAPON && !g_arm[W_SAWED].owned) {   // [qsist]
        g_w[W_SAWED].owned = 1;
        g_w[W_SAWED].mag = 2;
        {   // con el arma vienen 6 cartuchos: los que no quepan quedan en el suelo
            int take = inv_add(ITEM_12G, 6);
            if (take < 6) spill(ITEM_12G, 6 - take);
        }
        pop(x, y, "Escopeta recortada", RGBA(150, 220, 130, 255));
        msg("Encontraste una recortada. Cambia de arma con izquierda o derecha.");
        return;
    }
    if (c->kind == CK_MASK) {   // el cuerpo de la máscara: la lleva puesta. Si ya tienes una, te sirve su filtro
        int gi = gear_of(ITEM_MASK);
        if (!gear_owned(gi)) {
            g_gcond[gi] = 100;
            g_bag[ITEM_MASK] = 1;
            pop(x, y, "Máscara de gas", RGBA(150, 220, 130, 255));
            y -= 11;
            msg("Encontraste una máscara de gas. Póntela desde la mochila (Triángulo).");
        } else loot(ITEM_FILTER, 1, x, &y);
    }
    if (c->kind == CK_MIL) {   // [mil] caja militar: dos tipos de munición, a veces un arma, una pieza o una granada; nada de comida ni medicinas
        int pool[4], np = 0, over2 = 0;
        for (int w = 0; w < W_COUNT; w++) {
            if (is_melee(w) || !(g_w[w].owned || g_arm[w].owned)) continue;
            int it = WDEF[w].ammo[0].item, dup = 0;
            for (int k = 0; k < np; k++) dup |= pool[k] == it;
            if (!dup && np < 3) pool[np++] = it;
        }
        static const int BASE[3] = {ITEM_9MM, ITEM_762, ITEM_12G};
        for (int t = 0; np < 2 && t < 12; t++) { int it = BASE[rnd() % 3], dup = 0; for (int k = 0; k < np; k++) dup |= pool[k] == it; if (!dup) pool[np++] = it; }
        int a = (int)(rnd() % (u32)np), b2 = (a + 1 + (int)(rnd() % (u32)(np - 1 > 0 ? np - 1 : 1))) % np;
        for (int k = 0; k < 2; k++) {
            int it = pool[k ? b2 : a];
            over2 |= loot(it, it == ITEM_12G ? irange(3, 6) : irange(6, 14), x, &y);
        }
        if (frand() < 0.40f) weapon_find(c, x, y);
        if (frand() < 0.25f) { static const short PZ[5] = {ITEM_SIL, ITEM_COMP, ITEM_XMAG, ITEM_REFLEX, ITEM_STOCK}; over2 |= loot(PZ[rnd() % 5], 1, x, &y); }
        if (frand() < 0.30f) over2 |= loot(ITEM_GREN, 1, x, &y);
        if (over2) msg("No cabe todo en la mochila: lo que sobra quedó en el suelo.");
        return;
    }
    int rolls = is_body(c) ? (rnd() % 3 ? 1 : 2) : c->kind == CK_RICH ? irange(2, 3) : irange(1, 2), over = 0;   // menos tiradas: el dueño quiere escasez y variedad, no solo balas
    if (c->kind == CK_RICH) {   // el botín del búnker: siempre algo bueno (una pieza, munición perforante o un kit)
        static const short RICH[] = {ITEM_XMAG, ITEM_COMP, ITEM_STOCK, ITEM_REFLEX, ITEM_SIL, ITEM_9MM_AP, ITEM_762_AP, ITEM_CLEAN, ITEM_MED};
        int it = RICH[rnd() % (sizeof(RICH) / sizeof(RICH[0]))];
        over |= loot(it, it == ITEM_9MM_AP || it == ITEM_762_AP ? irange(3, 6) : 1, x, &y);
    }
    for (int i = 0; i < rolls; i++) {
        const LootDef *l = loot_roll(is_body(c));
        if (!is_body(c)) for (int t = 0; t < 3 && l->item >= ITEM_9MM && l->item <= ITEM_12G_SLUG; t++) l = loot_roll(0);   // [mil] las cajas normales ya no dan munición: la tiran las militares y los cuerpos
        over |= loot(l->item, l->lo < l->hi ? irange(l->lo, l->hi) : l->lo, x, &y);
    }
    over |= duro_extra_loot(c->kind == CK_RICH, is_body(c), x, &y);   // [duro]
    loot(ITEM_SCRAP, irange(1, 3), x, &y);
    pozo_extra_loot(c, x, &y);   // [pozo]
    if (over) msg("No cabe todo en la mochila: lo que sobra quedó en el suelo.");
    weapon_find(c, x, y);
}

#include "pozo.inc"   // [pozo]

// Al chocar con una esquina: hacia que lado (-1 o +1) del eje cruzado queda libre el paso a menos de
// NUDGE px; 0 si no hay. `cross` es el empuje del jugador en el eje cruzado: no se desliza en contra
// de un empuje claro. En *gap deja cuantos pixeles faltan para librar la esquina.
#define NUDGE 5
static int nudge_side(float dx, float dy, float cross, float r, int *gap)
{
    float push = fabsf(dx != 0 ? dx : dy);
    for (int k = 1; k <= NUDGE; k++)
        for (int s = -1; s <= 1; s += 2) {
            if (cross * s < -push * 0.5f) continue;
            if (!box_hits(P.x + dx + (dx != 0 ? 0 : s * k), P.y + dy + (dx != 0 ? s * k : 0), r)) { *gap = k; return s; }
        }
    return 0;
}

// Movimiento del jugador: como move(), pero si empuja contra un obstaculo y le faltan pocos pixeles
// para librar la esquina, se desliza solo hacia el lado libre en vez de quedar enganchado.
// Cada paso se revisa con box_hits: no atraviesa nada ni va mas rapido que el empuje original.
static void move_player(float dx, float dy, float r)
{
    int s, k, moved_x = 0;
    float my = dy;
    if (!box_hits(P.x + dx, P.y, r)) { P.x += dx; moved_x = 1; }
    else if (dx != 0 && (s = nudge_side(dx, 0, dy, r, &k)) != 0) {
        float ny = s * fminf(fabsf(dx), k);
        if (dy * ny <= 0 || fabsf(dy) < fabsf(ny)) my = ny;   // el deslizamiento reemplaza al empuje, no se suma
        if (!box_hits(P.x, P.y + my, r)) P.y += my;
        return;
    }
    if (!box_hits(P.x, P.y + my, r)) P.y += my;
    else if (dy != 0 && (s = nudge_side(0, dy, dx, r, &k)) != 0) {
        float nx = fminf(fabsf(dy), k);
        if (moved_x && dx * s > 0) nx -= fabsf(dx);   // ya avanzo hacia ese lado en este cuadro
        if (nx > 0 && !box_hits(P.x + s * nx, P.y, r)) P.x += s * nx;
    }
}

// Huellas en la nieve: el jugador y los enemigos vivos dejan una cada ~9 px andados; se borran solas
static void foot_update(float dt)
{
    static float lx[MAXE + 1], ly[MAXE + 1];
    for (int i = 0; i < MAXFOOT; i++) if (g_foot[i].t > 0) g_foot[i].t -= dt;
    if (!snowy()) return;
    for (int i = -1; i < g_ne && i < MAXE; i++) {
        float x = i < 0 ? P.x : g_e[i].x, y = i < 0 ? P.y : g_e[i].y, *px = &lx[i + 1], *py = &ly[i + 1];
        if (i >= 0 && g_e[i].state == ES_DEAD) { *px = x; *py = y; continue; }
        float d = dist(x, y, *px, *py);
        if (d > 9) {
            if (d < 40 && !indoors(x, y)) {
                float side = (g_nfoot & 1) ? 2.5f : -2.5f;   // un pie a cada lado de la marcha
                g_foot[g_nfoot++ % MAXFOOT] = (Foot){x - (y - *py) / d * side, y + (x - *px) / d * side, 24};
            }
            *px = x; *py = y;
        }
    }
}

// ------------------------------------------------------------------ Hollín (el perro compañero) y los eventos de la expedición
// El perro no es un Enemy: no está en g_e, no estorba el paso y cambia de zona contigo. Los enemigos pueden elegirlo como adversario
// (foe_pick lo devuelve como -3). Los golpes y las granadas tuyos no lo dañan; las anomalías y los enemigos, sí
static int dog_active(void) { return g_dog.have && !g_dog.wounded && !g_dog.home && g_state != GS_DEAD; }
static int dog_free(float x, float y) { return !box_hits(x, y, 3); }
// Lo pone junto al jugador, en una casilla libre
static void dog_to_player(void)
{
    for (int k = 0; k < 12; k++) {
        float a = P.ang + PI + k * 0.9f, r = 12 + 3 * (k / 6);
        float x = P.x + cosf(a) * r, y = P.y + sinf(a) * r;
        if (dog_free(x, y)) { g_dog.x = x; g_dog.y = y; return; }
    }
    g_dog.x = P.x; g_dog.y = P.y;
}
// Su rincón en la base: la primera casilla libre cerca del portón
static void dog_kennel(void)
{
    for (int r = 0; r < 6; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                int tx = 9 + dx, ty = 22 + dy;
                if (in_map(tx, ty) && !solid(tx, ty) && ground_at(tx * TS + 8, ty * TS + 8) != G_EXIT && dog_free(tx * TS + 8, ty * TS + 8)) { g_dog.x = tx * TS + 8; g_dog.y = ty * TS + 8; return; }
            }
}
static void dog_hurt(float dmg)
{
    Dog *d = &g_dog;
    if (!dog_active() || dmg <= 0) return;
    d->hp -= dmg;
    d->flash = 0.15f;
    blood(d->x, d->y, 3);
    if (d->hp > 0) { if (d->growl_t <= 0) { snd_at(SFX_WHINE, 0.7f, d->x, d->y, 260); d->growl_t = 1.5f; } return; }
    d->hp = 0; d->wounded = 1; d->zone = g_zone; d->down_t = 60; d->atk = 0;
    snd_at(SFX_WHINE, 1, d->x, d->y, 300);
    msg("Hollín cayó herido. Una venda lo levanta; si no, volverá solo a la base.");
}
static int dog_hit_test(float x, float y) { return dog_active() && dist(x, y, g_dog.x, g_dog.y) < 6; }
static float foe_x(int ti) { return ti == -1 ? P.x : ti == -3 ? g_dog.x : g_e[ti].x; }
static float foe_y(int ti) { return ti == -1 ? P.y : ti == -3 ? g_dog.y : g_e[ti].y; }

// L + Arriba: sígueme / quieto. L + Abajo: ataca al blanco fijado
static void dog_order(int up)
{
    if (!g_dog.have || g_dog.home || g_dog.wounded) return;
    if (up) {
        g_dog.st = g_dog.st == DG_FOLLOW ? DG_STAY : DG_FOLLOW;
        g_dog.atk = 0;
        msg(g_dog.st == DG_STAY ? "Hollín: quieto." : "Hollín: te sigue.");
        snd_at(SFX_BARK, 0.6f, g_dog.x, g_dog.y, 200);
    } else if (P.lock >= 0 && g_e[P.lock].state != ES_DEAD) {
        g_dog.atk = 1; g_dog.st = DG_FOLLOW;
        msg("Hollín: ¡ataca!");
        snd_at(SFX_BARK, 0.8f, g_dog.x, g_dog.y, 240);
    } else msg("Fija un blanco con L para mandar a Hollín a atacar.");
}
static int dog_pick_target(void)
{
    Dog *d = &g_dog;
    if (d->atk && P.lock >= 0 && g_e[P.lock].state != ES_DEAD) return P.lock;
    d->atk = 0;
    if (g_sneak || d->st == DG_STAY) return -1;   // en sigilo no ataca si no se lo ordenas; quieto tampoco sale a buscar
    int best = -1;
    float bd = 52;
    for (int i = 0; i < g_ne; i++) {
        const Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e) || fr_fac(e->fac)) continue;
        float dd = dist(d->x, d->y, e->x, e->y);
        if (dd < bd && dist(P.x, P.y, e->x, e->y) < 150 && los(d->x, d->y, e->x, e->y)) { bd = dd; best = i; }
    }
    return best;
}
static void dog_step(float tx, float ty, float speed, float dt)
{
    Dog *d = &g_dog;
    float a = atan2f(ty - d->y, tx - d->x), ox = d->x, oy = d->y;
    move(&d->x, &d->y, cosf(a) * speed * dt, sinf(a) * speed * dt, 3);
    if (dist(ox, oy, d->x, d->y) < speed * dt * 0.4f) {   // trabado: prueba de costado
        float s = ((int)(g_time * 3) & 1) ? 1.4f : -1.4f;
        move(&d->x, &d->y, cosf(a + s) * speed * dt, sinf(a + s) * speed * dt, 3);
    }
    if (dist(ox, oy, d->x, d->y) > 0.2f) { d->ang = a; d->moving = 1; d->walk += dt * speed / 9; }
}
static void dog_update(float dt)
{
    Dog *d = &g_dog;
    if (!d->have || g_state != GS_PLAY) return;
    d->moving = 0;
    d->flash -= dt; d->growl_t -= dt; d->bite_cd -= dt; d->warn_t -= dt; d->bark_t -= dt;
    if (g_zone != d->zone) {   // cambió de zona contigo
        d->zone = g_zone;
        memset(g_dbk, 0, sizeof(g_dbk));
        if (d->wounded) { d->wounded = 0; d->home = 1; d->skip = 1; d->hp = DOG_HP / 2; msg("Hollín herido volvió solo a la base y no saldrá en la próxima expedición."); }
        if (g_zone == 0 && d->home) dog_kennel();
        else if (!d->home) dog_to_player();
    }
    if (d->wounded) {
        if ((d->down_t -= dt) <= 0) { d->wounded = 0; d->home = 1; d->skip = 1; d->hp = DOG_HP / 2; msg("Hollín herido volvió solo a la base y no saldrá en la próxima expedición."); }
        return;
    }
    if (d->home) return;
    if (g_zone == 0) d->hp = fminf(DOG_HP, d->hp + 6 * dt);   // en la base se repone
    float dp = dist(d->x, d->y, P.x, P.y);
    if (dp > 260) { dog_to_player(); dp = dist(d->x, d->y, P.x, P.y); }
    int tgt = dog_pick_target();
    if (tgt >= 0) {
        Enemy *e = &g_e[tgt];
        float de = dist(d->x, d->y, e->x, e->y);
        if (de > 9) dog_step(e->x, e->y, 112, dt);
        else {
            d->ang = atan2f(e->y - d->y, e->x - d->x);
            if (d->bite_cd <= 0) { d->bite_cd = 0.7f; hurt_enemy(e, g_sk[SK_ADIES] ? 9 : 6, d->x, d->y, 0); snd_at(SFX_ZBITE, 0.5f, d->x, d->y, 200); }
        }
    } else if (d->st == DG_FOLLOW && dp > 24) dog_step(P.x - cosf(P.ang) * 10, P.y - sinf(P.ang) * 10, dp > 90 ? 128 : 80, dt);
    // avisa: gruñe y mira hacia los enemigos cercanos que tú aún no ves
    if (d->warn_t <= 0) {
        d->warn_t = 0.4f;
        int w = -1;
        float bd = 150;
        for (int i = 0; i < g_ne; i++) {
            const Enemy *e = &g_e[i];
            if (e->state == ES_DEAD || is_post(e) || fr_fac(e->fac) || e->state == ES_COMBAT) continue;
            float dd = dist(d->x, d->y, e->x, e->y);
            if (dd < bd && !los(P.x, P.y, e->x, e->y)) { bd = dd; w = i; }
        }
        if (w >= 0 && !d->moving) d->ang = atan2f(g_e[w].y - d->y, g_e[w].x - d->x);
        if (w >= 0 && d->growl_t <= 0) {
            d->growl_t = 4;
            snd_at(SFX_DGROWL, 0.8f, d->x, d->y, 300);
            pop(d->x, d->y - 14, "Grrr...", RGBA(230, 200, 150, 255));
        }
    }
    // marca con un ladrido corto los cajones sin abrir cercanos
    if (d->bark_t <= 0) {
        d->bark_t = 1.5f;
        for (int i = 0; i < g_ncr && i < MAXCR; i++) {
            const Crate *c = &g_cr[i];
            if (c->open || g_dbk[i] || is_body(c) || c->kind == CK_SURV || c->kind == CK_DOGW || c->kind == CK_FARO) continue;
            if (dist(d->x, d->y, c->tx * TS + 8, c->ty * TS + 8) > 80) continue;
            g_dbk[i] = 1; d->bark_t = 4;
            snd_at(SFX_BARK, 0.8f, d->x, d->y, 280);
            pop(c->tx * TS + 8, c->ty * TS - 8, "¡Guau!", RGBA(240, 220, 150, 255));
            break;
        }
    }
}

// Cruz sobre el perro: levantarlo con una venda o un botiquín, y en la base llevarlo o dejarlo en casa. Devuelve 1 si hizo algo
static int dog_cross(int free_hand)
{
    Dog *d = &g_dog;
    if (!d->have) return 0;
    float dp = dist(P.x, P.y, d->x, d->y);
    if (d->wounded && g_zone == d->zone && dp < 24) {
        if (g_bag[ITEM_BAND] > 0) inv_take(ITEM_BAND, 1);
        else if (g_bag[ITEM_MED] > 0) inv_take(ITEM_MED, 1);
        else { msg("Hollín está herido. Necesita una venda o un botiquín."); return 1; }
        d->wounded = 0; d->hp = DOG_HP * 0.6f;
        snd_play(SFX_BAND, 0.8f, 0);
        msg("Hollín se levanta y te mira.");
        return 1;
    }
    if (JF_PERRO_ELEGIR && g_zone == 0 && free_hand && dp < 22) {
        if (d->home) {
            if (d->skip) { msg("Hollín se está recuperando: saldrá contigo en la próxima expedición."); return 1; }
            d->home = 0; d->st = DG_FOLLOW; msg("Hollín te acompañará."); snd_at(SFX_BARK, 0.7f, d->x, d->y, 200);
        } else { d->home = 1; dog_kennel(); msg("Hollín se queda en casa."); }
        return 1;
    }
    return 0;
}

// Cruz sobre un cajón especial: el perro herido (se adopta con una conserva o una venda) y el superviviente herido
static void crate_special(Crate *c)
{
    float x = c->tx * TS + 8, y = c->ty * TS - 10;
    if (c->kind == CK_DOGW) {
        if (g_bag[ITEM_FOOD] > 0) inv_take(ITEM_FOOD, 1);
        else if (g_bag[ITEM_BAND] > 0) inv_take(ITEM_BAND, 1);
        else { msg("Un perro herido junto a un cuerpo. Dale una conserva o una venda."); return; }
        c->open = 1;
        memset(&g_dog, 0, sizeof(g_dog));
        g_dog.have = 1; g_dog.hp = DOG_HP; g_dog.zone = g_zone; g_dog.x = x; g_dog.y = y + 10;
        snd_at(SFX_BARK, 1, x, y, 300);
        pop(x, y, "Hollín", RGBA(240, 220, 150, 255));
        msg("El perro se queda contigo: se llama Hollín. L + Arriba: sígueme/quieto. L + Abajo: ataca.");
        return;
    }
    if (c->kind == CK_SURV) {
        int ei = 0;
        for (int i = 0; i < g_nev; i++) if (g_ev[i].kind == EV_SURV) ei = i;
        if (g_bag[ITEM_BAND] > 0) inv_take(ITEM_BAND, 1);
        else if (g_bag[ITEM_MED] > 0) inv_take(ITEM_MED, 1);
        else { msg("Un superviviente herido pide una venda o un botiquín."); return; }
        c->open = 1;
        g_x.evdone |= 1 << ei;
        g_rep += 2;
        snd_play(SFX_BAND, 0.8f, 0);
        if ((g_x.seed >> 5) & 1) {
            float yy = P.y - 30;
            give(ITEM_9MM, 12, P.x, &yy);
            msg("El superviviente te lo agradece y te da munición. +2 de reputación.");
        } else {
            g_x.evfound |= 4 << ei;
            for (int yy = g_ev[ei].y2 - 3; yy <= g_ev[ei].y2 + 3; yy++) for (int xx = g_ev[ei].x2 - 3; xx <= g_ev[ei].x2 + 3; xx++) if (in_map(xx, yy)) g_seen[yy][xx] = 1;
            msg("El superviviente te marca un alijo en el mapa. +2 de reputación.");
        }
        pop(x, y, "+2 reputación", RGBA(150, 220, 130, 255));
    }
}

// ---- los eventos: personajes al armar la zona (ev_spawn) y lo que pasa con el tiempo (ev_update)
static void ev_mk(int type, int tx, int ty, int evn, int mob)
{
    int n0 = g_ne;
    spawn_enemy(type, tx * TS + 8, ty * TS + 8);
    if (g_ne > n0) { g_e[n0].ev = evn; g_e[n0].mob = mob; }
}
static void ev_spawn(void)
{
    if (g_fx >= 0 && g_ncr < MAXCR) {   // el puesto farolero: su comerciante y 2 o 3 faroleros (hostiles si ya les disparaste)
        int n00 = g_ne, nf = 2 + (int)((g_x.seed >> 7) % 2);
        g_cr[g_ncr++] = (Crate){g_fx, g_fy, CK_FARO, 0};
        for (int k = 0; k < nf; k++) {
            int n0 = g_ne;
            ev_mk(E_GRD, g_fx + (k == 0 ? -2 : k == 1 ? 2 : 0), g_fy + (k == 2 ? -2 : 1), EVN_FARO, 0);
            if (g_ne > n0) g_e[n0].fac = (g_x.evdone & 4) ? F_BAND : F_FARO;
        }
        g_x_nen += g_ne - n00;
    }
    if (g_dogx >= 0 && g_ncr < MAXCR) g_cr[g_ncr++] = (Crate){g_dogx, g_dogy, CK_DOGW, g_dog.have ? 1 : 0};
    for (int i = 0; i < g_nev; i++) {
        const Ev *v = &g_ev[i];
        int cx = v->tx, cy = v->ty, n0 = g_ne;
        if (v->kind == EV_SURV) {
            if (g_ncr < MAXCR) g_cr[g_ncr++] = (Crate){cx, cy, CK_SURV, (g_x.evdone >> i) & 1};
        } else if (v->kind == EV_CARAVAN) {
            for (int k = 0; k < 4; k++) ev_mk(E_GRD, cx - 4, cy - 2 + k, EVN_GUARD, 1);
            for (int k = 0; k < 3; k++) ev_mk(E_BAN, cx + 4, cy - 1 + k, EVN_BANDIT, 0);
        } else if (v->kind == EV_HELI) {
            ev_mk(E_BIG, cx - 4, cy - 1, EVN_HELI, 0); ev_mk(E_BIG, cx + 4, cy + 1, EVN_HELI, 0);
            ev_mk(E_ARM, cx, cy + 4, EVN_HELI, 0);
            for (int k = 0; k < 3; k++) ev_mk(E_MAR, cx - 4 + 4 * k, cy - 4, EVN_HELI, 0);
        } else if (v->kind == EV_HORDE) {   // zombis reservados y escondidos en el punto de partida: despiertan cuando suena el aviso
            int n = 8 + g_x.danger, cap = MAXE - 4 - g_ne;
            if (n > 14) n = 14;
            if (n > cap) n = cap;
            for (int k = 0; k < n; k++) {
                int before = g_ne;
                ev_mk(k % 5 == 4 ? E_MAR : k % 7 == 6 ? E_RUN : E_DOG, cx, cy, EVN_HORDE, 0);
                if (g_ne > before) { Enemy *e = &g_e[before]; e->hp = 0; e->state = ES_DEAD; }
            }
        }
        g_x_nen += g_ne - n0;
    }
}
// Despierta la horda: suena la corneta y los zombis reservados parten por el camino hasta el otro extremo
static void horde_wake(int i)
{
    const Ev *v = &g_ev[i];
    int n = 0;
    for (int k = 0; k < g_ne; k++) {
        Enemy *e = &g_e[k];
        if (e->ev != EVN_HORDE || e->state != ES_DEAD || e->dead_t != 0) continue;
        float x = v->tx * TS + 8, y = v->ty * TS + 8;
        for (int t = 0; t < 8; t++) {
            float px = x + frange(-24, 24), py = y + frange(-24, 24);
            if (!box_hits(px, py, 5)) { x = px; y = py; break; }
        }
        e->x = e->hx = x; e->y = e->hy = y;
        e->hp = EHP[e->type]; e->state = ES_ALERT; e->t = 0;
        e->tx = v->x2 * TS + 8; e->ty = v->y2 * TS + 8;
        n++;
    }
    g_x.evfound |= 1 << i; g_x.evdone |= 1 << i;
    snd_play(SFX_HORN, 1, 0);
    msg(n ? "Suena una corneta lejana: una horda cruza el mapa. Escóndete bajo techo o pelea." : "Suena una corneta lejana.");
}
static void ev_update(float dt)
{
    if (g_zone != 1 || g_state != GS_PLAY || !g_x.num) return;
    if ((g_ev_t -= dt) > 0) return;
    g_ev_t = 0.5f;
    for (int i = 0; i < g_nev; i++) {
        const Ev *v = &g_ev[i];
        float d = dist(P.x, P.y, v->tx * TS + 8, v->ty * TS + 8);
        int bit = 1 << i;
        if (v->kind == EV_HORDE) {
            if (!((g_x.evdone >> i) & 1) && g_play_t - g_xs_t >= 150 + (float)(g_x.seed & 63)) horde_wake(i);
            continue;
        }
        if (!(g_x.evfound & bit) && d < (v->kind == EV_HELI ? 24 : 12) * TS) {
            g_x.evfound |= bit;
            msg(v->kind == EV_SURV ? "Un superviviente herido. Queda marcado en el mapa." : v->kind == EV_CARAVAN ? "Una caravana emboscada: guardias de Kolos pelean con bandidos." :
                "Un helicóptero caído: hay radiación y algo lo cuida.");
        }
        if (v->kind == EV_CARAVAN && (g_x.evfound & bit) && !((g_x.evdone >> i) & 1)) {   // se resolvió: no quedan bandidos de la caravana
            int bandits = 0, guards = 0;
            for (int k = 0; k < g_ne; k++) {
                if (g_e[k].state == ES_DEAD) continue;
                bandits += g_e[k].ev == EVN_BANDIT;
                guards += g_e[k].ev == EVN_GUARD && g_e[k].fac == F_KOLOS;
            }
            if (bandits == 0) {
                g_x.evdone |= bit;
                if (guards > 0) {
                    g_rep += 3;
                    for (int k = 0; k < g_ne; k++) if (g_e[k].ev == EVN_GUARD) g_e[k].mob = 0;
                    msg("La caravana sigue su camino. Los guardias de Kolos te lo agradecen: +3 de reputación.");
                } else msg("La caravana cayó, pero sus cajones siguen ahí.");
            }
        }
    }
    if (g_dogx >= 0 && !(g_x.evfound & 16) && dist(P.x, P.y, g_dogx * TS + 8, g_dogy * TS + 8) < 12 * TS) { g_x.evfound |= 16; msg("Un perro herido junto a un cuerpo. Queda marcado en el mapa."); }
    if (g_fx >= 0 && !(g_x.evfound & 32) && dist(P.x, P.y, g_fx * TS + 8, g_fy * TS + 8) < 14 * TS) { g_x.evfound |= 32; msg("Se ve la luz de un puesto farolero. Queda marcado en el mapa."); }
}

// ------------------------------------------------------------------ habilidades, Faroleros y diario
static const char *const SK_NAME[SK_N] = {"Mula", "Manos rápidas", "Piel curtida", "Pulso firme", "Carnicero", "Pies de gato", "Estómago de hierro", "Rebuscador", "Adiestrador"};
static const char *const SK_DESC[SK_N] = {"Más lugar en la mochila: aguantas 10 kg más de carga.", "Recargas un 25 % más rápido.", "El frío sube un 40 % más lento.",
    "Un 20 % menos de dispersión al disparar.", "Un 25 % más de daño con las armas cuerpo a cuerpo.", "Tus pasos suenan un 40 % más bajo.",
    "El hambre y la sed bajan un 25 % más lento.", "Más chatarra en los cajones y los cuerpos.", "Hollín tiene más vida y muerde más fuerte."};
// Un punto por cada rango nuevo y otro cada 3 expediciones (6 en total); los que no has gastado
static int sk_points(void)
{
    int t = rank() + g_xtotal / 3, sp = 0;
    if (t > 6) t = 6;
    for (int i = 0; i < SK_N; i++) sp += g_sk[i] != 0;
    return t - sp;
}
static int g_sk_sel;
static void sk_buy(void)
{
    if (g_sk[g_sk_sel]) { msg("Ya tienes esa habilidad."); return; }
    if (sk_points() <= 0) { msg("No tienes puntos de habilidad. Ganas uno por rango nuevo y otro cada 3 expediciones."); return; }
    g_sk[g_sk_sel] = 1;
    snd_play(SFX_JOB_DONE, 0.7f, 0);
    char s[96]; snprintf(s, sizeof(s), "Habilidad aprendida: %s.", SK_NAME[g_sk_sel]);
    msg(s);
    save_game();
}

// Los Faroleros: reputación propia, un puesto en algunas expediciones y tres trabajos
static const char *const FJ_NAME[4] = {"Luz para el puesto", "Campo apagado", "El ojo del camino", "La Viuda blanca"};
static const char *const FJ_DESC[4] = {"Trae un artefacto al puesto.", "Apaga un campo de anomalías con 2 trapos y 1 químico.", "Mata a un francotirador bandido.", "Mata a la loba gigante del Lago helado."};
static const short FJ_PAY[4] = {120, 160, 220, 380}, FJ_REP[4] = {2, 2, 3, 4};   // [jefes2] el cuarto: La Viuda blanca
static const short ART_PRICE[A_N] = {520, 380, 560, 420, 420, 480, 700, 700, 450, 400};   // [artef]
typedef struct { const char *name; short item, qty, price, frep; } FShop;
static const FShop FSHOP[] = {
    {"Tuercas x6", ITEM_NUT, 6, 20, 0}, {"Detector de anomalías", ITEM_DET, 1, 130, 0}, {"Balas 9x18 perforantes x6", ITEM_9MM_AP, 6, 60, 1},
    {"Postas de bala cal. 12 x4", ITEM_12G_SLUG, 4, 50, 1}, {"Balas 7,62 perforantes x4", ITEM_762_AP, 4, 90, 2}, {"Cóctel incendiario x2", ITEM_MOLO, 2, 60, 2},
    {"Artefacto del puesto", ITEM_ART0, 1, 300, 2},   // el artefacto cambia con la expedición; a veces no hay
};
#define FSHOP_N ((int)(sizeof(FSHOP) / sizeof(FSHOP[0])))
static int g_faro_tab, g_faro_sel;
static int fshop_item(int i) { return i == FSHOP_N - 1 ? artef_com(g_x.seed >> 4) : FSHOP[i].item; }
static int fshop_has(int i) { return i != FSHOP_N - 1 || ((g_x.seed >> 3) & 1); }   // el artefacto sale una de cada dos veces
static int fshop_list(int *out) { int n = 0; for (int i = 0; i < FSHOP_N; i++) if (fshop_has(i)) out[n++] = i; return n; }
static int fart_list(int *out) { int n = 0; for (int i = 0; i < A_N; i++) if (g_bag[ART_ITEM(i)] > 0) out[n++] = i; return n; }
static int fart_pay(int a) { return ART_PRICE[a] * (75 + 4 * (g_frep > 5 ? 5 : g_frep)) / 100; }   // mejor que en Kolos (50 a 65 %)
static int fjob_ready(void)
{
    if (g_fjob == 1) { int l[A_N]; return fart_list(l) > 0; }
    return g_fjn >= 1;
}
static void faro_open(void)
{
    if (g_x.evdone & 4) { msg("El comerciante farolero no te habla: le disparaste a uno de los suyos."); return; }
    g_faro_tab = 0; g_faro_sel = 0; g_state = GS_FARO; g_msg_t = 0;
}
static void farol_hostile(void)
{
    for (int k = 0; k < g_ne; k++) if (g_e[k].fac == F_FARO && g_e[k].state != ES_DEAD) { g_e[k].fac = F_BAND; g_e[k].state = ES_COMBAT; }
    g_x.evdone |= 4;
    g_frep = g_frep > 3 ? g_frep - 3 : 0;
    msg("Le diste a un farolero: el puesto se vuelve hostil. -3 de reputación farolera.");
}
// Apagar un campo de anomalías (trabajo farolero 2): Cruz junto a él, con 2 trapos y 1 químico
static int farol_field_try(void)
{
    if (g_fjob != 2 || g_fjn || g_zone != 1) return 0;
    for (int i = 0; i < g_nan; i++) {
        Anom *a = &g_an[i];
        if (a->r <= 0 || dist(P.x, P.y, a->x, a->y) > a->r + 26) continue;
        if (g_bag[ITEM_RAG] < 2 || g_bag[ITEM_CHEM] < 1) { msg("Para apagar el campo necesitas 2 trapos y 1 químico."); return 1; }
        inv_take(ITEM_RAG, 2); inv_take(ITEM_CHEM, 1);
        for (int k = 0; k < g_nan; k++) if (g_an[k].field == a->field) { g_an[k].r = 0; g_an[k].vis = 0; }
        g_fjn = 1;
        snd_play(SFX_WHOOSH, 0.8f, 0);
        msg("Apagaste el campo. Cuéntaselo al puesto farolero.");
        return 1;
    }
    return 0;
}
static void faro_buy(int i)
{
    const FShop *d = &FSHOP[i];
    int it = fshop_item(i), gi = gear_of(it);
    char s[96];
    if (g_frep < d->frep) snprintf(s, sizeof(s), "Aún no te lo venden: pide %d de reputación farolera.", d->frep);
    else if (gi >= 0 && gear_owned(gi)) snprintf(s, sizeof(s), "Ya tienes uno de esos.");
    else if (inv_room(it) < d->qty) snprintf(s, sizeof(s), "No te cabe en la mochila.");
    else if (g_bag[ITEM_SCRAP] < d->price) snprintf(s, sizeof(s), "Te falta chatarra: cuesta %d y tienes %d.", d->price, g_bag[ITEM_SCRAP]);
    else {
        g_bag[ITEM_SCRAP] -= d->price;
        inv_add(it, d->qty);
        if (gi >= 0) g_gcond[gi] = 100;
        snd_play(SFX_TRADE, 0.8f, 0);
        snprintf(s, sizeof(s), "Compraste (-%d de chatarra).", d->price);
    }
    msg(s);
}
static void faro_job(int j)
{
    char s[96];
    if ((g_fdone >> j) & 1) { msg("Ese trabajo ya está hecho."); return; }
    if (g_fjob == j + 1) {
        if (!fjob_ready()) { msg("Ese trabajo aún no está cumplido."); return; }
        if (j == 0) { int l[A_N]; fart_list(l); inv_take(ART_ITEM(l[0]), 1); }
        g_bag[ITEM_SCRAP] += FJ_PAY[j]; g_scrap_gain += FJ_PAY[j];
        g_frep += FJ_REP[j]; g_fdone |= 1 << j; g_fjob = 0; g_fjn = 0; g_jobs_done++;
        snprintf(s, sizeof(s), "Trabajo farolero cumplido: +%d chatarra, +%d de reputación farolera.", FJ_PAY[j], FJ_REP[j]);
        snd_play(SFX_JOB_DONE, 0.9f, 0);
        msg(s);
        save_game();
    } else if (g_fjob) msg("Ya tienes un trabajo farolero. Entrégalo primero.");
    else if (g_frep < j) { snprintf(s, sizeof(s), "Aún no confían en ti: pide %d de reputación farolera.", j); msg(s); }
    else { g_fjob = j + 1; g_fjn = 0; msg(j == 2 ? "Trabajo tomado. Se cumple en la próxima expedición: el francotirador está en un campamento." : "Trabajo tomado. Se cumple en tus expediciones."); }
}
static void faro_input(void)
{
    int l[FSHOP_N], n = 0;
    if (in.pressed & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { g_state = GS_PLAY; return; }
    if (in.pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) { g_faro_tab = (g_faro_tab + ((in.pressed & PSP_CTRL_RTRIGGER) ? 1 : 2)) % 3; g_faro_sel = 0; return; }
    n = g_faro_tab == 0 ? fshop_list(l) : g_faro_tab == 1 ? fart_list(l) : 4;   // [jefes2]
    if (n < 1) n = 1;
    if (in.pressed & PSP_CTRL_UP) g_faro_sel = (g_faro_sel + n - 1) % n;
    if (in.pressed & PSP_CTRL_DOWN) g_faro_sel = (g_faro_sel + 1) % n;
    if (g_faro_sel >= n) g_faro_sel = 0;
    if (g_faro_tab == 2 && (in.pressed & PSP_CTRL_SQUARE) && g_fjob == g_faro_sel + 1) {   // cancelar el trabajo farolero en curso
        if (fjob_ready()) { msg("Ese trabajo ya está cumplido: entrégalo con Cruz."); return; }
        g_fjob = 0; g_fjn = 0;
        if (g_frep > 0) g_frep--;
        msg("Trabajo cancelado: -1 de reputación farolera. Puedes volver a tomarlo.");
        return;
    }
    if (!(in.pressed & PSP_CTRL_CROSS)) return;
    if (g_faro_tab == 0) { if (fshop_list(l) > g_faro_sel) faro_buy(l[g_faro_sel]); }
    else if (g_faro_tab == 1) {
        if (fart_list(l) > g_faro_sel) {
            int a = l[g_faro_sel], pay = fart_pay(a);
            inv_take(ART_ITEM(a), 1); g_bag[ITEM_SCRAP] += pay; g_scrap_gain += pay;
            snd_play(SFX_TRADE, 0.8f, 0);
            char s[64]; snprintf(s, sizeof(s), "Vendiste el artefacto: +%d de chatarra.", pay); msg(s);
        } else msg("No llevas artefactos.");
    } else faro_job(g_faro_sel);
}

// ------------------------------------------------------------------ anomalías: efectos y dibujo
// Brasero: llamarada cada vez que alguien entra (y cada 1,8 s mientras sigue dentro): quema. Chispa: descarga cada 3 a 4,5 s en su radio:
// daña y aturde. Remolino: atrae hacia el centro; en el centro daña y lanza lejos con el empuje. Los enemigos también las sufren
static int an_in(const Anom *a, float x, float y, float rr) { return dist(a->x, a->y, x, y) < rr; }

// Daña (y aturde) a todos los que están dentro de rr: tú y los enemigos
static void an_hurt_all(const Anom *a, float rr, float dmg_p, float dmg_e, float stun_p, float stun_e)
{
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        if (e->state == ES_DEAD || is_post(e) || !an_in(a, e->x, e->y, rr)) continue;
        hurt_enemy(e, dmg_e, a->x, a->y, 0);
        if (e->state != ES_DEAD && stun_e > 0 && e_mass(e) >= 0.3f) e->stun = fmaxf(e->stun, stun_e);
    }
    if (dog_active() && an_in(a, g_dog.x, g_dog.y, rr)) dog_hurt(dmg_e);
    if (g_state == GS_PLAY && an_in(a, P.x, P.y, rr)) {
        hurt_player(dmg_p);
        if (stun_p > 0) g_pstun = fmaxf(g_pstun, stun_p);
    }
}

// Una tuerca cayó en (x, y): las anomalías de alrededor reaccionan y quedan a la vista unos segundos
static void anom_ping(float x, float y)
{
    for (int i = 0; i < g_nan; i++)
        if (dist(x, y, g_an[i].x, g_an[i].y) < g_an[i].r + 55) { g_an[i].vis = 7; g_an[i].ping = 1; }
}

static void anom_update(float dt)
{
    for (int i = 0; i < g_nan; i++) {
        Anom *a = &g_an[i];
        float r = a->r;
        int ping = a->ping;
        if (r <= 0) continue;   // campo apagado
        a->ping = 0;
        if (a->vis > 0) a->vis -= dt;
        if (a->zap > 0) a->zap -= dt;
        a->cd -= dt;
        if (a->kind == AN_BRASERO) {
            int any = g_state == GS_PLAY && an_in(a, P.x, P.y, r);
            for (int k = 0; k < g_ne && !any; k++) any = g_e[k].state != ES_DEAD && !is_post(&g_e[k]) && an_in(a, g_e[k].x, g_e[k].y, r);
            if ((any || ping) && a->cd <= 0) {
                a->cd = 1.8f; a->vis = fmaxf(a->vis, 1.4f); a->zap = 0.45f;
                an_hurt_all(a, r, 8, 10, 0, 0);
                snd_at(SFX_WHOOSH, 0.9f, a->x, a->y, 320);
                for (int k = 0; k < 10; k++) part(a->x + frange(-r * 0.6f, r * 0.6f), a->y + frange(-r * 0.4f, r * 0.4f), frange(-8, 8), frange(-50, -22), 0.7f, RGBA(255, 160, 60, 255), 2);
            }
        } else if (a->kind == AN_CHISPA) {
            if (ping && a->cd > 0.3f) a->cd = 0.3f;
            if (a->cd < 0.5f && a->cd > 0 && frand() < 0.3f) part(a->x + frange(-r, r), a->y + frange(-r, r) * 0.7f, 0, -10, 0.2f, RGBA(170, 210, 255, 255), 1);
            if (a->cd <= 0) {
                a->cd = frange(3.0f, 4.5f); a->vis = fmaxf(a->vis, 0.7f); a->zap = 0.3f;
                an_hurt_all(a, r * 1.15f, 9, 12, 0.7f, 1.0f);
                snd_at(SFX_ZAP, 0.9f, a->x, a->y, 340);
                sparks_n(a->x, a->y - 4, 8, 1.6f);
            }
        } else {
            float pull = r * 2.2f;
            if (g_state == GS_PLAY && an_in(a, P.x, P.y, pull)) {
                float d = dist(a->x, a->y, P.x, P.y), k = 1 - d / pull;
                if (d > 1) move(&P.x, &P.y, (a->x - P.x) / d * 40 * k * dt, (a->y - P.y) / d * 40 * k * dt, 5);
            }
            for (int k = 0; k < g_ne; k++) {
                Enemy *e = &g_e[k];
                if (e->state == ES_DEAD || is_post(e) || !an_in(a, e->x, e->y, pull)) continue;
                float d = dist(a->x, a->y, e->x, e->y), f = 1 - d / pull;
                if (d > 1) move(&e->x, &e->y, (a->x - e->x) / d * 40 * f * e_mass(e) * dt, (a->y - e->y) / d * 40 * f * e_mass(e) * dt, 5);
            }
            if (a->cd <= 0) {
                int hit = g_state == GS_PLAY && an_in(a, P.x, P.y, r * 0.5f);
                for (int k = 0; k < g_ne; k++) {
                    Enemy *e = &g_e[k];
                    if (e->state == ES_DEAD || is_post(e) || !an_in(a, e->x, e->y, r * 0.5f)) continue;
                    float ang = frange(0, 6.283f), m = e_mass(e);
                    hurt_enemy(e, 8, a->x, a->y, 0);
                    if (e->state != ES_DEAD) { e->kx = cosf(ang) * 70 * 7 * m; e->ky = sinf(ang) * 70 * 7 * m; e->stun = 0.4f; }
                    hit = 1;
                }
                if (g_state == GS_PLAY && an_in(a, P.x, P.y, r * 0.5f)) { hurt_player(7); push_player(frange(0, 6.283f), 75); }
                if (hit) { a->cd = 2.0f; a->vis = fmaxf(a->vis, 1.5f); snd_at(SFX_WHOOSH, 0.9f, a->x, a->y, 300); }
            }
        }
    }
}

static void ring(float x, float y, float r, float t, u32 col)
{
    for (int i = 0; i < 16; i++) {
        float a0 = i * PI / 8, a1 = (i + 1) * PI / 8;
        r_line(x + cosf(a0) * r, y + sinf(a0) * r * 0.8f, x + cosf(a1) * r, y + sinf(a1) * r * 0.8f, t, col);
    }
}

// Lo que se ve de las anomalías (sobre la oscuridad): casi nada hasta que algo las delata, y los artefactos que marca el detector
static void anom_draw(void)
{
    static const unsigned char C[AN_KINDS][3] = {{255, 140, 60}, {130, 195, 255}, {190, 170, 255}};
    if (g_zone != 1) return;
    for (int i = 0; i < g_nan; i++) {
        const Anom *a = &g_an[i];
        if (a->r <= 0) continue;
        float x = a->x - g_ox, y = a->y - g_oy;
        if (x < -70 || y < -70 || x > SCR_W + 70 || y > SCR_H + 70) continue;
        int cr = C[a->kind][0], cg = C[a->kind][1], cb = C[a->kind][2];
        r_glow(x, y, a->r, RGBA(cr, cg, cb, 6 + (int)(4 * sinf(g_time * 2 + i))), RGBA(cr, cg, cb, 0));   // el leve temblor del aire
        if (g_state == GS_PLAY) {
            if (a->kind == AN_BRASERO && frand() < 0.012f) part(a->x + frange(-a->r, a->r) * 0.6f, a->y, frange(-4, 4), frange(-24, -12), 0.8f, RGBA(255, 170, 80, 200), 1);
            if (a->kind == AN_CHISPA && frand() < 0.008f) part(a->x + frange(-a->r, a->r) * 0.7f, a->y + frange(-a->r, a->r) * 0.5f, 0, -6, 0.15f, RGBA(170, 210, 255, 255), 1);
        }
        if (a->kind == AN_REMOLINO)
            for (int k = 0; k < 3; k++) {
                float an = g_time * 1.6f + k * 2.1f + i;
                r_rect(x + cosf(an) * a->r * 0.7f, y + sinf(an) * a->r * 0.55f, 1, 1, RGBA(cr, cg, cb, 45));
            }
        if (a->vis > 0) {   // delatada: se ve su círculo, su nombre y su brillo
            float k = clampf(a->vis, 0, 1);
            r_glow(x, y, a->r * 1.2f, RGBA(cr, cg, cb, (int)(70 * k)), RGBA(cr, cg, cb, 0));
            ring(x, y, a->r, 1.2f, RGBA(cr, cg, cb, (int)(200 * k)));
            if (a->field >= 0 && (i == 0 || g_an[i - 1].field != a->field)) r_text_center(x, y - a->r * 0.8f - 12, AN_NAME[a->kind], RGBA(cr, cg, cb, (int)(230 * k)));
        }
        if (a->zap > 0) {
            float zk = a->zap / (a->kind == AN_BRASERO ? 0.45f : 0.3f);
            if (a->kind == AN_BRASERO) r_glow(x, y - 4, a->r * 1.5f, RGBA(255, 160, 60, (int)(200 * zk)), RGBA(255, 110, 40, 0));
            else if (a->kind == AN_CHISPA)
                for (int k = 0; k < 3; k++) {
                    float an = frange(0, 6.283f), mx = x + cosf(an) * a->r * 0.5f + frange(-4, 4), my = y + sinf(an) * a->r * 0.4f + frange(-4, 4);
                    r_line(x, y - 3, mx, my, 1.3f, RGBA(190, 225, 255, (int)(240 * zk)));
                    r_line(mx, my, x + cosf(an) * a->r, y + sinf(an) * a->r * 0.8f, 1.3f, RGBA(190, 225, 255, (int)(240 * zk)));
                }
        }
    }
    for (int i = 0; i < MAXK; i++) {   // los artefactos: un brillo suave de cerca y, con el detector, un anillo que los marca desde más lejos
        const Pick *k = &g_k[i];
        if (!k->on || art_idx(k->item) < 0) continue;
        float d = dist(P.x, P.y, k->x, k->y), x = k->x - g_ox, y = k->y - g_oy;
        const unsigned char *c = C[art_idx(k->item) % AN_KINDS];
        if (d < 70) r_glow(x, y, 9, RGBA(c[0], c[1], c[2], 70), RGBA(c[0], c[1], c[2], 0));
        if (g_eq[EQ_DET] && d < 140) {
            float p = 7 + 2 * sinf(g_time * 5 + i);
            ring(x, y, p, 1.1f, RGBA(255, 226, 120, 230));
            r_rect(x - 1, y - 1, 2, 2, RGBA(255, 240, 170, 255));
        }
    }
}

// ------------------------------------------------------------------ objetos de mano, trampas y fuego
// El objeto de mano es uno de los que llevas: L mantenido + izquierda/derecha lo cambia y L + Triángulo lo lanza (Tuerca, Granada,
// Cóctel) o lo pone en el suelo (Mina, Cepo), hacia el blanco fijado o hacia donde miras

static void hand_cycle(int step)
{
    for (int k = 1; k <= H_N; k++) {
        int h = ((g_hand + step * k) % H_N + H_N) % H_N;
        if (g_bag[HAND_ITEM[h]] > 0) { g_hand = h; snd_play(SFX_MENU_MOVE, 0.5f, 0); return; }
    }
    msg("No llevas objetos de mano.");
}

// Explosión: daña y empuja a los enemigos y a ti, y se oye de lejos
static void boom_at(float x, float y, float r, float dmg, float push)
{
    int b = 0;
    for (int i = 0; i < 4; i++) { if (g_boom[i].t <= 0) { b = i; break; } if (g_boom[i].t < g_boom[b].t) b = i; }
    g_boom[b] = (Boom){x, y, 0.5f};
    snd_at(SFX_BOOM, 1, x, y, 560);
    {
        float sh = 8 * (1 - clampf(dist(x, y, P.x, P.y) / 300, 0, 1));
        if (g_shake < sh) g_shake = sh;
    }
    sparks_n(x, y - 4, 16, 2.4f);
    for (int i = 0; i < 14; i++) {
        float a = frange(0, 6.283f), v = frange(20, 70);
        part(x, y - 4, cosf(a) * v, sinf(a) * v, frange(0.4f, 0.9f), i & 1 ? RGBA(96, 90, 84, 200) : RGBA(255, 170, 70, 255), 2);
    }
    noise(x, y, 430);
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        float d = dist(x, y, e->x, e->y);
        if (e->state == ES_DEAD || is_post(e) || d >= r || !los(x, y, e->x, e->y)) continue;
        float k = 1 - d / r, ea = atan2f(e->y - y, e->x - x), m = e_mass(e);
        hurt_enemy(e, dmg * (0.35f + 0.65f * k) * (1 - e->armor * 0.4f), x, y, 1);
        if (e->state != ES_DEAD) {
            e->kx = cosf(ea) * push * (0.4f + 0.6f * k) * 7 * m; e->ky = sinf(ea) * push * (0.4f + 0.6f * k) * 7 * m;
            e->stun = 0.6f * (m >= 0.5f ? 1 : m >= 0.3f ? 0.4f : 0);
        }
    }
    float dp = dist(x, y, P.x, P.y);
    if (dp < r && g_state == GS_PLAY && los(x, y, P.x, P.y)) {
        float k = 1 - dp / r;
        hurt_player(dmg * 0.7f * (0.35f + 0.65f * k));
        push_player(atan2f(P.y - y, P.x - x), push * 0.5f * (0.4f + 0.6f * k));
    }
}

// Un charco de fuego unos segundos (daña a todos los que se quedan dentro)
static void fire_at(float x, float y)
{
    for (int n = 0; n < 3; n++) {
        float fx = n ? x + frange(-9, 9) : x, fy = n ? y + frange(-7, 7) : y;
        if (n && box_hits(fx, fy, 3)) { fx = x; fy = y; }
        int k = 0;
        for (int i = 0; i < 12; i++) { if (g_fir[i].life <= 0) { k = i; break; } if (g_fir[i].life < g_fir[k].life) k = i; }
        g_fir[k] = (Fire){fx, fy, 6 + frange(-0.5f, 0.8f), 0};
    }
    snd_at(SFX_WHOOSH, 0.9f, x, y, 420);
    noise(x, y, 120);
}

// L + Triángulo: lanza o pone el objeto de mano
static void hand_use(void)
{
    int h = g_hand, item = HAND_ITEM[h];
    if (g_bag[item] <= 0) { msg("No te quedan de ese objeto."); return; }
    float a = P.lock >= 0 ? atan2f(g_e[P.lock].y - P.y, g_e[P.lock].x - P.x) : P.ang;
    if (h == H_MINE || h == H_TRAP) {   // se pone en el suelo, un paso adelante
        float x = P.x + cosf(a) * 12, y = P.y + sinf(a) * 12;
        int k = -1;
        if (box_hits(x, y, 4)) { x = P.x; y = P.y; }
        for (int i = 0; i < 12; i++) if (!g_trp[i].on) { k = i; break; }
        if (k < 0) { msg("Ya hay demasiadas trampas puestas."); return; }
        g_trp[k] = (Trap){x, y, h == H_MINE ? 1.2f : 0, h, 1, -1};
        g_bag[item]--;
        snd_play(SFX_TRAP, 0.7f, 0);
        P.fire_cd = fmaxf(P.fire_cd, 0.3f);
        return;
    }
    float d = P.lock >= 0 ? dist(P.x, P.y, g_e[P.lock].x, g_e[P.lock].y) : 90, sp = 170;
    int k = -1;
    for (int i = 0; i < 6; i++) if (!g_thr[i].on) { k = i; break; }
    if (k < 0) return;
    d = clampf(d, 24, 150);
    g_thr[k] = (Thrown){P.x + cosf(a) * 8, P.y + sinf(a) * 8, cosf(a) * sp, sinf(a) * sp, d / sp, d / sp, 0, h, 1};
    g_bag[item]--;
    snd_play(SFX_SWING, 0.6f, 0);
    P.fire_cd = fmaxf(P.fire_cd, 0.35f);
    P.ang = a;
}

// Cruz junto a un cepo: lo recoge
static int trap_pickup(void)
{
    for (int i = 0; i < 12; i++) {
        Trap *t = &g_trp[i];
        if (!t->on || t->kind != H_TRAP || dist(P.x, P.y, t->x, t->y) > 16) continue;
        if (inv_room(ITEM_TRAP) < 1) { msg("No te cabe en la mochila."); return 1; }
        inv_add(ITEM_TRAP, 1);
        t->on = 0;
        snd_play(SFX_PICKUP, 0.7f, 0);
        msg("Recoges el cepo.");
        return 1;
    }
    return 0;
}

// Cada cuadro: lo que vuela, la mecha de la granada, las trampas y el fuego
static void hand_world_update(float dt)
{
    for (int i = 0; i < 6; i++) {
        Thrown *t = &g_thr[i];
        if (!t->on) continue;
        int land = 0;
        if (t->t > 0) {
            t->t -= dt;
            float nx = t->x + t->vx * dt, ny = t->y + t->vy * dt;
            if (box_hits(nx, ny, 2)) land = 1; else { t->x = nx; t->y = ny; }
            if (t->kind == H_MOLO)   // el cóctel se rompe contra el primero que alcanza
                for (int k = 0; k < g_ne && !land; k++)
                    if (g_e[k].state != ES_DEAD && !is_post(&g_e[k]) && dist(t->x, t->y, g_e[k].x, g_e[k].y) < 8) land = 1;
            if (t->t <= 0) land = 1;
        } else if (t->kind == H_GREN) {
            t->fuse -= dt;
            if (t->fuse <= 0) { boom_at(t->x, t->y, 62, 85, 62); t->on = 0; }
            continue;
        }
        if (!land) continue;
        t->t = 0;
        if (t->kind == H_NUT) {
            snd_at(SFX_CLINK, 0.9f, t->x, t->y, 380);
            sparks_n(t->x, t->y - 2, 4, 0.8f);
            noise(t->x, t->y, 230);
            anom_ping(t->x, t->y);
            t->on = 0;
        } else if (t->kind == H_MOLO) {
            fire_at(t->x, t->y);
            t->on = 0;
        } else t->fuse = 0.9f;   // la granada rueda y se queda: estalla a poco
    }
    for (int i = 0; i < 12; i++) {
        Trap *t = &g_trp[i];
        if (!t->on) continue;
        if (t->kind == H_MINE) {
            if (t->t > 0) { t->t -= dt; continue; }
            for (int k = 0; k < g_ne; k++)
                if (g_e[k].state != ES_DEAD && !is_post(&g_e[k]) && dist(t->x, t->y, g_e[k].x, g_e[k].y) < 9) {
                    boom_at(t->x, t->y, 58, 90, 66);
                    t->on = 0;
                    break;
                }
        } else if (t->on == 1) {
            for (int k = 0; k < g_ne; k++) {
                Enemy *e = &g_e[k];
                if (e->state == ES_DEAD || is_post(e) || dist(t->x, t->y, e->x, e->y) > 8) continue;
                e->x = t->x; e->y = t->y; e->kx = e->ky = 0;
                snd_at(SFX_TRAP, 1, t->x, t->y, 300);
                hurt_enemy(e, 24, t->x, t->y, 1);
                if (e->state != ES_DEAD) e->stun = is_boss(e) ? 1.2f : e_mass(e) < 0.5f ? 3 : 4.5f;
                sparks_n(t->x, t->y - 2, 5, 1);
                t->on = 2;
                break;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        Fire *f = &g_fir[i];
        if (f->life <= 0) continue;
        f->life -= dt;
        if ((f->tick -= dt) > 0) continue;
        f->tick = 0.4f;
        for (int k = 0; k < g_ne; k++)
            if (g_e[k].state != ES_DEAD && !is_post(&g_e[k]) && dist(f->x, f->y, g_e[k].x, g_e[k].y) < 14) hurt_enemy(&g_e[k], 4, f->x, f->y, 1);
        if (g_state == GS_PLAY && dist(f->x, f->y, P.x, P.y) < 13) hurt_player(3);
    }
    for (int i = 0; i < 4; i++) if (g_boom[i].t > 0) g_boom[i].t -= dt;
#ifdef PEPEL_DEBUG
    if (g_dbg_boomfx) {
        float bx = P.x + 55 * cosf(P.ang), by = P.y + 55 * sinf(P.ang);
        g_boom[3] = (Boom){bx, by, 0.2f};
        for (int i = 0; i < 3; i++) { float a = frange(0, 6.283f), v = frange(20, 70); part(bx, by - 4, cosf(a) * v, sinf(a) * v, frange(0.4f, 0.9f), i & 1 ? RGBA(96, 90, 84, 200) : RGBA(255, 170, 70, 255), 2); }
    }
#endif
}

static void update_player(float dt)
{
    foot_update(dt);
#ifdef PEPEL_DEBUG
    if (g_dbg_walk > 0) { g_dbg_walk -= dt; in.ax = g_dbg_wx; in.ay = g_dbg_wy; }
#endif
    if (g_pstun > 0) { g_pstun -= dt; in.ax = in.ay = 0; in.held &= ~PSP_CTRL_RTRIGGER; }   // aturdido: ni te mueves ni disparas
    if (P.kx != 0 || P.ky != 0) {   // empujón de un golpe fuerte: se frena solo y se detiene en un muro
        float sp = sqrtf(P.kx * P.kx + P.ky * P.ky);
        int n = (int)(sp * dt / 3) + 1;
        for (int i = 0; i < n; i++) move(&P.x, &P.y, P.kx * dt / n, P.ky * dt / n, 5);
        float f = expf(-8 * dt);
        P.kx *= f; P.ky *= f;
        if (sp * f < 8) P.kx = P.ky = 0;
    }
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    float m = sqrtf(in.ax * in.ax + in.ay * in.ay);
    int aiming = (in.held & PSP_CTRL_LTRIGGER) != 0;
    if (g_opt[OPT_RUN] == 1) {   // correr alternando: un toque de Círculo lo enciende o lo apaga
        if (in.pressed & PSP_CTRL_CIRCLE) g_run_on ^= 1;
        if (P.stam <= 1) g_run_on = 0;
    } else g_run_on = 0;
    int run_btn = g_opt[OPT_RUN] == 1 ? g_run_on : (in.held & PSP_CTRL_CIRCLE) != 0;
    int sprint = run_btn && m > 0.3f && P.stam > 1 && !aiming && P.reload_t <= 0;
    if (peso_pct() > 100) sprint = 0;   // [peso] sobrecargado: no corres
    float sp = aiming ? 46 : sprint ? 112 : 72;
    if (P.rad > 80) sp *= 0.8f;
    if (g_eq[EQ_ARMOR]) sp *= GEAR[g_eq[EQ_ARMOR] - 1].slow;   // el chaleco pesado
    sp *= art_speed();                                         // [artef] Ascua mansa y Piel de plomo
    sp *= peso_slow();   // [peso]
    if (is_bush((int)(P.x / TS), (int)(P.y / TS))) sp *= 0.6f;   // [armas] entre arbustos se anda más lento
    unstick_player();   // [armas]
    P.moving = m > 0.1f;
    g_sneak = P.moving && m < 0.6f && !sprint;
#ifdef PEPEL_DEBUG
    if (g_dbg_sneak) g_sneak = 1;
#endif
    if (P.moving) {
        move_player(in.ax * sp * dt, in.ay * sp * dt, 5);
        P.walk += dt * (sprint ? 1.5f : 1);
        if ((g_step_t -= dt * (sprint ? 1.4f : 1)) <= 0) {   // pasos muy bajos; distintos bajo techo
            g_step_t = 0.36f;
            {   // el ruido de los pasos: correr se oye de lejos, caminar normal poco y con el stick a medias casi nada
                float nr = sprint ? 130 : g_sneak ? 10 : 55;
                noise(P.x, P.y, (art_on(A_OJO) ? nr * 2.5f : nr) * (g_sk[SK_GATO] ? 0.6f : 1));
            }
            snd_play(indoors(P.x, P.y) ? SFX_STEP_IN : SFX_STEP_OUT, 0.13f + 0.05f * frand(), frange(-0.15f, 0.15f));
        }
    }
    int weak = P.hunger < 25 || P.thirst < 25;   // con hambre o sed, el aguante vuelve a la mitad
    float reg = (weak ? 8 : 16) * (g_cold > 60 ? 0.5f : 1);   // con frío sobre 60, el aguante también vuelve a la mitad
    float stam0 = P.stam;   // [peso]
    P.stam = clampf(P.stam + (sprint ? (art_on(A_PULMON) ? 0 : -30) : reg) * dt, 0, 100);
    if (peso_pct() > 100 && P.stam > stam0) P.stam = stam0;   // [peso] sobrecargado: el aguante no se recupera
    if (g_eq[EQ_DET] && g_zone == 1) {   // el detector: pita más rápido cuanto más cerca está una anomalía
        float nd = 1e9f;
        for (int i = 0; i < g_nan; i++) if (g_an[i].r > 0) nd = fminf(nd, dist(P.x, P.y, g_an[i].x, g_an[i].y) - g_an[i].r);
        if (nd < 170 && (g_det_t -= dt) <= 0) { snd_play(SFX_BEEP, 0.45f, 0); g_det_t = clampf(0.12f + nd / 170, 0.12f, 1.2f); }
    }

    // apuntado automatico: el blanco es el enemigo a la vista mas cercano; L pasa al siguiente.
    // Al disparar o mantener L el personaje gira solo hacia el blanco; si no, mira hacia donde camina.
    int firing = (in.held & PSP_CTRL_RTRIGGER) != 0;
#ifdef PEPEL_DEBUG
    if (g_dbg_hold > 0) { g_dbg_hold -= dt; firing = 1; }
#endif
    float aim_err = 0;
    float reach = w_stats(P.weapon).reach;   // con más alcance (la mira), el blanco se fija desde más lejos
    if (P.lock >= 0 && !can_target(&g_e[P.lock], 215 * reach)) P.lock = -1;
    if ((P.retarget -= dt) <= 0 && P.lock < 0) { P.lock = pick_target(190 * reach, -1); P.retarget = 0.15f; }
    if (in.pressed & PSP_CTRL_LTRIGGER) {
        int n = pick_target(190 * reach, P.lock);
        if (n >= 0) P.lock = n;
    }
    if (P.lock >= 0 && (aiming || firing || P.fire_cd > -0.5f)) {
        float ta = atan2f(g_e[P.lock].y - P.y, g_e[P.lock].x - P.x);
        P.ang = turn(P.ang, ta, 16 * dt);
        aim_err = fabsf(angdiff(P.ang, ta));
    } else if (P.moving && !aiming) P.ang = turn(P.ang, atan2f(in.ay, in.ax), 11 * dt);

    // arma
    P.fire_cd -= dt;
    P.flash_t -= dt;
    armas_bayonet();   // [armas]
    if (P.reload_t > 0) {
        P.reload_t -= dt;
        if (P.reload_t <= 0) {
            w->mag += inv_take(d->ammo[w->load].item, w_mag(P.weapon) - w->mag);
        }
    } else if (P.jam_t > 0) {
        P.jam_t -= dt;
        if (P.jam_t <= 0) { P.jammed = 0; msg("Arma destrabada."); }
    } else if (firing && P.fire_cd <= 0 && (is_melee(P.weapon) || aim_err < 0.45f)) {   // primero gira hacia el blanco
        if (is_melee(P.weapon)) melee_swing();
        else if (P.jammed) {
            if (in.pressed & PSP_CTRL_RTRIGGER) { msg("¡Arma trabada! Cuadrado para destrabarla."); snd_play(SFX_JAM, 0.8f, 0); }
        } else if (w->mag <= 0) {   // cargador vacio: recarga sola si quedan balas
            if (g_bag[d->ammo[w->load].item] > 0) start_reload();
            else if (in.pressed & PSP_CTRL_RTRIGGER) no_ammo_msg();
        } else if (frand() < jam_chance(w->cond) * d->jam) {
            P.jammed = 1;
            P.fire_cd = 0.3f;
            snd_play(SFX_JAM, 0.8f, 0);
            msg("¡Arma trabada! Cuadrado para destrabarla.");
        } else shoot();
    }
    // Cuadrado: un toque recarga o destraba; mantenido medio segundo, pasa al siguiente tipo de munición
    if (in.pressed & PSP_CTRL_SQUARE) {
        P.sq_t = 0;
        if (P.jammed && P.jam_t <= 0) P.jam_t = 0.9f;
        else if (!P.jammed) start_reload();
    } else if ((in.held & PSP_CTRL_SQUARE) && P.sq_t >= 0) {
        if ((P.sq_t += dt) >= 0.5f) { P.sq_t = -1; ammo_switch(); }
    } else P.sq_t = -1;
    if (aiming && (in.pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) hand_cycle((in.pressed & PSP_CTRL_RIGHT) ? 1 : -1);   // L + izquierda/derecha: objeto de mano
    else if ((in.pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) && P.reload_t <= 0) {
        int step = (in.pressed & PSP_CTRL_RIGHT) ? 1 : W_COUNT - 1, n = P.weapon;   // recorre las armas que tienes
        do n = (n + step) % W_COUNT; while (!g_w[n].owned);
        if (n != P.weapon) { P.weapon = n; P.jammed = 0; P.jam_t = 0; P.fire_cd = 0.25f; }
        else msg("No tienes otra arma.");
    }
    if (aiming && (in.pressed & PSP_CTRL_TRIANGLE)) hand_use();   // L + Triángulo: lanza o pone el objeto de mano
    if ((in.pressed & PSP_CTRL_UP) && aiming) dog_order(1);   // L + Arriba y L + Abajo: órdenes a Hollín
    else if (in.pressed & PSP_CTRL_UP) heal();
    if ((in.pressed & PSP_CTRL_DOWN) && aiming) dog_order(0);
    else if (in.pressed & PSP_CTRL_DOWN) eat_drink();
    if ((in.pressed & PSP_CTRL_CROSS) && !trap_pickup() && !dog_cross(0)) {
        Crate *c;
        int qx, qy, what = near_use(&c, &qx, &qy);
        int sx, sy, sp = what ? 0 : near_spot(&sx, &sy);
        if (farol_field_try()) {}
        else if (!what && duro_panel_cross()) {}   // [duro]
        else if (!what && bk_cross()) {}   // [bunker]
        else if (!what && pozo_cross(aiming)) {}   // [pozo]
        else if (what == 1) { if (c->kind == CK_FARO) faro_open(); else if (c->kind == CK_DOGW || c->kind == CK_SURV) crate_special(c); else open_crate(c); }
        else if (what == 2) try_sleep();
        else if (sp == 1) zone_go(2, 0, 0, 0);   // la escotilla: baja al búnker (la llegada la fija zone_switch)
        else if (sp == 2) { zone_go(1, -1, -1, 0); g_zx_hatch = 1; }   // sube: aparece junto a la escotilla
        else if (sp == 3) { g_state = GS_CHOFER; g_trav_sel = 0; g_msg_t = 0; }
        else if (sp == 7) clinic_use();
        else if (sp == 8 || sp == 9) stair_go(sp == 8 ? 1 : -1, sx, sy);   // los edificios con pisos
        else if (sp >= 4 && sp <= 6) kolos_open(sp == 4 ? KW_COM : sp == 5 ? KW_SCAV : KW_STASH);   // los de la base: sus pestañas
        else if (g_zone == 0 && ground_at(P.x, P.y) == G_EXIT) exped_start();
        else if (dog_cross(1)) { }   // en la base: llevarlo o dejarlo en casa   // el portón: salir de expedición (mapa nuevo cada vez)
        else if (g_zone == 1 && ground_at(P.x, P.y) == G_ZONE) zone_go(0, 5 * TS + 8, 19 * TS + 8, 1);   // el punto de retirada: de vuelta a la base, guardando
    }

    // lo que hay en el suelo se recoge al pasar
    for (int i = 0; i < MAXK; i++) {   // solo lo que cabe: el resto sigue ahí y se avisa una vez por pasada
        Pick *k = &g_k[i];
        if (!k->on) continue;
        if (dist(P.x, P.y, k->x, k->y) > (art_on(A_IMAN) ? 30 : 12)) { k->on = 1; continue; }   // [artef] Imán negro: de más lejos
        if (k->item == ITEM_WDROP) { armas_wpick(k); continue; }   // [armas]
        if (k->on == 3) continue;   // lo que botaste no se recoge hasta que te alejes y regreses
        float y = P.y - 30;
        int left = give(k->item, k->n, P.x, &y);
        if (!left) { k->on = 0; continue; }
        k->n = left;
        if (k->on == 1) {
            char s[96];
            k->on = 2;
            snprintf(s, sizeof(s), "Vas muy cargado: de %s lo que sobra queda en el suelo.", ITEMS[k->item].many);   // [peso]
            msg(s);
        }
    }

    // cuerpo: hambre y sed, sangrado, radiacion y la Ceniza.
    // La sed baja 1 punto cada 6 s (el doble corriendo) y el hambre 1 cada 9 s; en 0, cualquiera de las dos mata de a poco
    float h0 = P.hunger, t0 = P.thirst;
    float slow = (g_mode == MODE_EXPL ? 0.7f : 1) * (g_sk[SK_ESTOM] ? 0.75f : 1);   // Explorador: hambre y sed bajan 30 % más lento
    P.thirst = clampf(P.thirst - dt / 6 * (sprint ? 2 : 1) * slow * art_thirst(), 0, 100);   // [artef] Pulmón de vidrio: más sed
    P.hunger = clampf(P.hunger - dt / 9 * slow * art_hunger(), 0, 100);   // [artef] Ascua mansa y Semilla hueca: más hambre
    duro_update(dt, slow);   // [duro] hambre y sed extra, sangrado, cura con barra, café y antibióticos
    if ((h0 > 0 && P.hunger <= 0) || (t0 > 0 && P.thirst <= 0)) msg(P.thirst <= 0 ? "Te mueres de sed." : "Te mueres de hambre.");
    else if ((h0 >= 25 && P.hunger < 25) || (t0 >= 25 && P.thirst < 25))
        msg(P.thirst >= 25 ? "Tienes hambre." : P.hunger >= 25 ? "Tienes sed." : "Tienes hambre y sed.");
    if (P.hunger <= 0 || P.thirst <= 0) P.hp -= 0.7f * dt;
    // descanso junto al fuego: a menos de 30 px de un barril encendido, sin enemigos en combate cerca y
    // sin hambre ni sed, la vida vuelve de a poco (hasta 70) y el sangrado se corta
    g_rest = 0;
    if (P.hunger > 25 && P.thirst > 25) {
        for (int i = 0; i < g_nfire && !g_rest; i++)
            if (dist(P.x, P.y, g_fire[i][0], g_fire[i][1]) < 30) g_rest = 1;
        if (g_rest && threat_near(150)) g_rest = 0;
    }
    if (g_rest) {
        P.bleed = 0;
        if (P.hp < 70) { P.hp = fminf(P.hp + 2.5f * dt, 70); g_rest = 2; }
    }
    P.hurt_t -= dt;
    if (art_on(A_LAGRIMA)) P.bleed = 0;   // Lágrima cuajada: no sangras
    artef_tick(dt);   // [artef] regeneración y radiación de los artefactos, tope de vida
    if (P.bleed > 0) {
        P.hp -= 1.6f * dt;
        P.bleed -= 0.1f * dt;
        if ((P.drip -= dt) <= 0) { P.drip = 0.35f; decal(P.x + frange(-2, 2), P.y + frange(-2, 2), S_BLOOD0 + (int)(rnd() % 3), 0.4f); }
        if (frand() < dt * 2) part(P.x, P.y, frange(-8, 8), frange(-8, 8), 0.4f, RGBA(150, 24, 26, 255), 1);
    }
    // la máscara con filtro: la Ceniza a la intemperie daña un 75 % menos y la radiación se acumula a la mitad.
    // El filtro (dura 100) gasta 1 por segundo bajo la tormenta y 0,5 por segundo en el charco radiactivo
    int in_rad = ground_at(P.x, P.y) == G_RAD, in_ash = 0, mask = mask_on();
#ifdef PEPEL_DEBUG
    if (g_dbg_god) in_rad = 0;   // pruebas: el modo sin daño también frena la radiación
#endif
#ifdef PEPEL_DEBUG
    if (g_dbg_god) in_ash = 0;   // pruebas: tampoco la Ceniza daña
#endif
    if (in_rad) P.rad = clampf(P.rad + 9 * (mask >= 0 ? 0.5f : 1) * dt, 0, 100);
    else P.rad = clampf(P.rad - 0.4f * dt, 0, 100);
    if (P.rad > RAD_DANO) P.hp -= (P.rad - RAD_DANO) * 0.07f * dt;   // [artef] la radiación acumulada duele desde 50
    if (in_ash) P.hp -= 6 * (mask >= 0 ? 0.25f : 1) * (art_on(A_OJO) ? 0 : 1) * dt;
    if (mask >= 0 && (in_ash || in_rad)) {
        g_gcond[mask] -= (in_ash ? 1.0f : 0) * dt + (in_rad ? 0.5f : 0) * dt;
        if (g_gcond[mask] <= 0) { g_gcond[mask] = 0; msg("El filtro de la máscara se agotó. Cámbialo desde la mochila."); }
    }
    {   // el frío: a la intemperie en una expedición nevada sube (lleno en ~90 s; con armadura puesta, un 40 % más lento);
        // bajo techo o a menos de 40 px de un barril con fuego baja rápido; en 100 quita 0,8 de vida por segundo
        int warm = indoors(P.x, P.y);
        for (int i = 0; i < g_nfire; i++) if (dist(P.x, P.y, g_fire[i][0], g_fire[i][1]) < 40) warm = 1;
        float c0 = g_cold;
        if (snowy() && !warm && !art_on(A_ASCUA) && !g_bag[ITEM_R_ABRIGO]) g_cold += (100.0f / 90) * (g_eq[EQ_ARMOR] ? 0.6f : 1) * (g_sk[SK_PIEL] ? 0.6f : 1) * dt;
        else g_cold -= 9 * dt;
        g_cold = clampf(g_cold, 0, 100);
        if (c0 <= 60 && g_cold > 60) msg("Tienes mucho frío. Busca un techo o un fuego.");
        int freeze = g_cold >= 99.9f;
#ifdef PEPEL_DEBUG
        if (g_dbg_god) freeze = 0;
#endif
        if (freeze) P.hp -= 0.8f * dt;
    }
    if (P.hp <= 0 && g_state == GS_PLAY && !pozo_revive()) { P.hp = 0; g_state = GS_DEAD; g_state_t = 0; }   // [pozo]

    // punto de control: al entrar bajo techo se guarda sola, salvo que te vengan persiguiendo de cerca
    explore();
    int in_now = indoors(P.x, P.y);
    if (in_now && !g_was_in && g_state == GS_PLAY) {
        if (!threat_near(150) && save_game()) { msg("Partida guardada."); snd_play(SFX_SAVE, 0.8f, 0); }
    }
    g_was_in = in_now;
}

// ------------------------------------------------------------------ enemigos
static void e_move(Enemy *e, float tx, float ty, float speed, float dt, int face)
{
    float dx = tx - e->x, dy = ty - e->y;
    if (dx * dx + dy * dy < 1) return;
    float a = atan2f(dy, dx);
    if (e->detour_t > 0) { e->detour_t -= dt; a = e->detour_a; }
    float ox = e->x, oy = e->y;
    move(&e->x, &e->y, cosf(a) * speed * dt, sinf(a) * speed * dt, 5);
    if (dist(ox, oy, e->x, e->y) < speed * dt * 0.35f) {   // trabado contra algo: rodear
        e->stuck += dt;
        if (e->stuck > 0.2f) {
            e->detour_a = a + (rnd() & 1 ? 1.45f : -1.45f);
            e->detour_t = frange(0.4f, 0.9f);
            e->stuck = 0;
        }
    } else e->stuck = 0;
    e->walk += dt;
    e->moving = 1;
    if (face) e->ang = turn(e->ang, a, 9 * dt);
}

// Campo de flujo hacia el jugador: pasos que le faltan a cada casilla para llegar a el (BFS por la
// grilla, hasta FLOW_MAX pasos). Los enemigos que persiguen sin el camino recto libre bajan por este
// campo y asi rodean los obstaculos en vez de tantear a ciegas.
#define FLOW_MAX 40
#define FLOW_NONE 255
static unsigned char g_flow[MHMAX][MWMAX];
static int g_flow_tx = -1, g_flow_ty = -1;
static float g_flow_time;

// Se recalcula cada 0,3 s o cuando el jugador cambia de casilla, y solo si algun enemigo lo necesita
static void flow_update(void)
{
    static unsigned short q[MWMAX * MHMAX];
    static const signed char DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    int px = (int)floorf(P.x / TS), py = (int)floorf(P.y / TS);
    float age = g_time - g_flow_time;
    if (px == g_flow_tx && py == g_flow_ty && age >= 0 && age < 0.3f) return;
    g_flow_tx = px;
    g_flow_ty = py;
    g_flow_time = g_time;
    memset(g_flow, FLOW_NONE, sizeof(g_flow));
    if (!in_map(px, py)) return;
    int head = 0, tail = 0;
    g_flow[py][px] = 0;
    q[tail++] = (unsigned short)(py * MW + px);
    while (head < tail) {
        int cx = q[head] % MW, cy = q[head] / MW, d = g_flow[cy][cx];
        head++;
        if (d >= FLOW_MAX) continue;
        for (int k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (solid(nx, ny) || g_flow[ny][nx] != FLOW_NONE) continue;
            g_flow[ny][nx] = (unsigned char)(d + 1);
            q[tail++] = (unsigned short)(ny * MW + nx);
        }
    }
}

// Camino recto libre para el cuerpo (no solo para la vista): nada solido en una franja de su ancho
static int walk_free(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(sqrtf(dx * dx + dy * dy) / 6) + 1;
    for (int i = 1; i <= n; i++)
        if (box_hits(x0 + dx * i / n, y0 + dy * i / n, 4)) return 0;
    return 1;
}

// Persecucion (ES_COMBAT y ES_ALERT): con el camino recto libre va derecho, como siempre; si no,
// camina a la casilla vecina con menos pasos hasta el jugador. Fuera del alcance del campo, como antes.
static void e_chase(Enemy *e, float tx, float ty, float speed, float dt, int face)
{
    static const signed char DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    if (los(e->x, e->y, tx, ty) && walk_free(e->x, e->y, tx, ty)) { e_move(e, tx, ty, speed, dt, face); return; }
    if (dist(tx, ty, P.x, P.y) > 24) { e_move(e, tx, ty, speed, dt, face); return; }   // el campo de flujo apunta al jugador
    flow_update();
    int cx = (int)floorf(e->x / TS), cy = (int)floorf(e->y / TS);
    int best = in_map(cx, cy) ? g_flow[cy][cx] : FLOW_NONE, bx = cx, by = cy;
    if (best != FLOW_NONE)
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (solid(nx, ny) || g_flow[ny][nx] >= best) continue;
            if (k >= 4 && (solid(nx, cy) || solid(cx, ny))) continue;   // en diagonal no se cortan esquinas
            best = g_flow[ny][nx];
            bx = nx;
            by = ny;
        }
    if (bx == cx && by == cy) { e_move(e, tx, ty, speed, dt, face); return; }
    float a = atan2f(by * TS + TS / 2 - e->y, bx * TS + TS / 2 - e->x);
    float ox = e->x, oy = e->y, st = speed * dt;
    move(&e->x, &e->y, cosf(a) * st, sinf(a) * st, 5);
    float rest = st - dist(ox, oy, e->x, e->y);
    if (rest > st * 0.65f) {   // roza una esquina: se centra en su casilla y pasa
        float mx = cx * TS + TS / 2 - e->x, my = cy * TS + TS / 2 - e->y, ml = sqrtf(mx * mx + my * my);
        if (ml > 0.01f) {
            float f = rest < ml ? rest / ml : 1;
            move(&e->x, &e->y, mx * f, my * f, 5);
        }
    }
    e->stuck = 0;
    e->detour_t = 0;
    e->walk += dt;
    e->moving = 1;
    if (face) e->ang = turn(e->ang, a, 9 * dt);
}

// [armas2] Fracción de la distancia de visión con la que te ven: a oscuras ~40 %, alumbrado hasta 100 %,
// de día un poco más, en sigilo menos. Los que ya están en combate te ven normal (los ruidos no pasan por aquí).
static float vis_jugador(const Enemy *e)
{
    if (e->state == ES_COMBAT) return 1.0f;
    float f = dz_fuera() ? dz_franja().f : 0.5f;
    float base = 0.36f + 0.10f * f;
    float lit = light_total(P.x, P.y) * 1.4f;
    if (lit > 1) lit = 1;
    float v = base + (1.0f - base) * lit;
    if (g_sneak) v *= 0.6f;
    return v;
}

// El adversario más cercano que `e` ve a menos de `sight`: -1 el jugador, k >= 0 otro enemigo, -2 ninguno.
// Cada bando ataca a los otros dos. La distancia queda en *dd. Con `back`, no ve lo que tiene a la espalda.
static int foe_pick(const Enemy *e, int self, float sight, int back, float *dd)
{
    sight *= duro_sight() * dz_vista();   // [duro] [diseno]
    int best = -2;
    float bd = sight;
    float pf = vis_jugador(e);   // [armas2] cuánto de lejos te ve (oscuridad, luz, hora, sigilo)
    for (int k = -1; k < g_ne; k++) {
        float cx, cy;
        if (k == -1) {
            if (fr_fac(e->fac)) continue;
            cx = P.x; cy = P.y;
        } else {
            const Enemy *o = &g_e[k];
            if (k == self || o->state == ES_DEAD || o->fac == e->fac || (fr_fac(o->fac) && fr_fac(e->fac))) continue;
            cx = o->x; cy = o->y;
        }
        float d = dist(e->x, e->y, cx, cy);
        if (d >= bd || (k == -1 && d >= sight * pf) || !los(e->x, e->y, cx, cy)) continue;   // [armas2]
        if (k == -1 && d > 60 && is_bush((int)(P.x / TS), (int)(P.y / TS))) continue;   // [armas] los arbustos te esconden
        if (back && d > (k == -1 && g_sneak ? 12 : 50) && fabsf(angdiff(e->ang, atan2f(cy - e->y, cx - e->x))) > 1.1f) continue;   // a la espalda no ve (y con el stick a medias, tampoco hasta casi tocarlo)
        best = k;
        bd = d;
    }
    if (dog_active() && !fr_fac(e->fac)) {   // Hollín también puede ser su adversario
        float d = dist(e->x, e->y, g_dog.x, g_dog.y);
        if (d < bd && los(e->x, e->y, g_dog.x, g_dog.y)) { best = -3; bd = d; }
    }
    *dd = bd;
    return best;
}

// Le pega a ese adversario (-1 = el jugador, -3 = Hollín)
static void foe_hit(const Enemy *from, int ti, float dmg)
{
    if (ti == -1) { if (g_state == GS_PLAY) hurt_player(dmg); }
    else if (ti >= 0) hurt_enemy(&g_e[ti], dmg, from->x, from->y, 0);
    else if (ti == -3) dog_hurt(dmg);
}

// Dispara una bala de este enemigo (de humano o de criatura): hacia donde mira, con algo de dispersión
static void e_fire(Enemy *e, float spread, float speed, float life, float dmg)
{
    if (e->fac == F_CREA) snd_at(SFX_AXE, 0.8f, e->x, e->y, 260);   // la criatura lanza el hacha
    else snd_at(e->var ? SFX_SHOT_RIFLE : SFX_SHOT_PISTOL, 0.8f, e->x, e->y, 300);
    float ba = e->ang + frange(-spread, spread);
    for (int i = 0; i < MAXB; i++) {
        if (g_b[i].on) continue;
        g_b[i] = (Bullet){e->x + cosf(e->ang) * 10, e->y + sinf(e->ang) * 10, cosf(ba) * speed, sinf(ba) * speed, life, dmg, 0, 1, 0, e->fac};
        break;
    }
    e->muzzle = 0.06f;
}

// El escupitajo del Escupidor: una bolsa de ácido lenta que, donde cae, deja un charco unos segundos
static void e_spit(Enemy *e)
{
    snd_at(SFX_SPIT, 0.8f, e->x, e->y, 260);
    float ba = e->ang + frange(-0.04f, 0.04f);
    for (int i = 0; i < MAXB; i++) {
        if (g_b[i].on) continue;
        g_b[i] = (Bullet){e->x + cosf(e->ang) * 8, e->y + sinf(e->ang) * 8, cosf(ba) * 105, sinf(ba) * 105, 2.6f, 7, 0, 1, K_ACID, e->fac, 0, 0};
        break;
    }
}
static void acid_splat(float x, float y)
{
    int k = 0;
    for (int i = 0; i < 10; i++) { if (g_pud[i].life <= 0) { k = i; break; } if (g_pud[i].life < g_pud[k].life) k = i; }
    g_pud[k] = (Pud){x, y, 6};
    snd_at(SFX_SPLAT, 0.8f, x, y, 240);
}

// El francotirador bandido: se queda lejos, apunta con una línea de mira roja (1,1 s siguiéndote y 0,4 s fija) y dispara un solo tiro fuerte
static void update_sniper(Enemy *e, int self, float dt)
{
    float d, fx, fy;
    int ti = foe_pick(e, self, 300, 0, &d);
    if (ti != -2) {
        fx = foe_x(ti); fy = foe_y(ti);
        e->tx = fx; e->ty = fy; e->see_t = 0;
        if (e->state != ES_COMBAT) { e->state = ES_COMBAT; e->cd = frange(1.0f, 2.0f); e->atk = 0; }
    } else { fx = e->tx; fy = e->ty; d = dist(e->x, e->y, fx, fy); }
    float to_t = atan2f(fy - e->y, fx - e->x);
    switch (e->state) {
    case ES_IDLE:
        e->t -= dt;
        if (e->t <= 0) { e->t = frange(2, 5); e->ang += frange(-1.0f, 1.0f); }
        break;
    case ES_ALERT:
        e->t += dt;
        e_chase(e, e->tx, e->ty, 40, dt, 1);
        if (e->t > 6 || dist(e->x, e->y, e->tx, e->ty) < 8) { e->state = ES_IDLE; e->tx = e->hx; e->ty = e->hy; e->t = 2; }
        break;
    case ES_COMBAT:
        e->cd -= dt;
        if (ti == -2) {
            if ((e->see_t += dt) > 5) { e->state = ES_ALERT; e->t = 0; e->atk = 0; }
            else if (!e->atk) e_chase(e, e->tx, e->ty, 40, dt, 1);
            break;
        }
        if (e->atk == 0) {
            e->ang = turn(e->ang, to_t, 6 * dt);
            if (d < 130) e_move(e, e->x - cosf(to_t) * 20, e->y - sinf(to_t) * 20, 46, dt, 0);   // busca distancia
            else if (d > 270) e_chase(e, fx, fy, 36, dt, 0);
            if (e->cd <= 0 && d < 290 && d > 60 && fabsf(angdiff(e->ang, to_t)) < 0.3f) { e->atk = 1; e->atk_t = 1.5f; }
        } else {
            if (e->atk_t > 0.4f) e->ang = turn(e->ang, to_t, 3.2f * dt);   // al final la mira queda fija: se puede esquivar
            if ((e->atk_t -= dt) <= 0) { e_fire(e, 0.0f, 520, 0.8f, 32); e->atk = 0; e->cd = frange(2.4f, 3.6f); }
        }
        break;
    }
}

// Bandidos y guardias: disparan con pistola o fusil. El bandido se mueve de costado y busca distancia; el guardia de
// Kolos se queda en su puesto. Ven en la oscuridad y le disparan al adversario más cercano que alcancen a ver.
static void update_human(Enemy *e, int self, float dt)
{
    if (e->type == E_SNP) { update_sniper(e, self, dt); return; }
    int post = is_post(e) && !e->mob;
    float d, fx, fy;
    int ti = foe_pick(e, self, 190, 0, &d);
    if (ti == -2) { fx = e->tx; fy = e->ty; d = dist(e->x, e->y, fx, fy); }
    else {
        fx = foe_x(ti);
        fy = foe_y(ti);
        e->tx = fx; e->ty = fy;
        e->see_t = 0;
        if (e->state != ES_COMBAT) { e->state = ES_COMBAT; e->cd = frange(0.4f, 0.9f); e->burst = 0; }
    }
    float to_t = atan2f(fy - e->y, fx - e->x);
    switch (e->state) {
    case ES_IDLE:
        e->t -= dt;
        if (post) { e->ang = turn(e->ang, 0, 2 * dt); break; }
        if (dist(e->x, e->y, e->tx, e->ty) > 4) e_move(e, e->tx, e->ty, 18, dt, 1);
        else if (e->t <= 0) {
            e->t = frange(1.5f, 4.5f);
            e->tx = e->hx + frange(-28, 28);
            e->ty = e->hy + frange(-28, 28);
        }
        break;
    case ES_ALERT:
        if (post) { e->state = ES_IDLE; break; }
        e->t += dt;
        e_chase(e, e->tx, e->ty, 50, dt, 1);
        if (e->t > 7 || dist(e->x, e->y, e->tx, e->ty) < 8) { e->state = ES_IDLE; e->tx = e->hx; e->ty = e->hy; e->t = 2; }
        break;
    case ES_COMBAT:
        e->cd -= dt;
        if (ti == -2) {   // lo perdió de vista
            if ((e->see_t += dt) > 4) { e->state = post ? ES_IDLE : ES_ALERT; e->t = 0; e->burst = 0; break; }
            if (!post) e_chase(e, e->tx, e->ty, 46, dt, 1);
        } else {
            e->ang = turn(e->ang, to_t, 7 * dt);
            if (!post) {
                if ((e->strafe_t -= dt) <= 0) { e->strafe = -e->strafe; e->strafe_t = frange(1.1f, 2.4f); }
                if (d > 140) e_chase(e, fx, fy, 50, dt, 0);
                else if (d < 75) e_move(e, e->x - cosf(to_t) * 20, e->y - sinf(to_t) * 20, 44, dt, 0);   // busca distancia
                else {
                    float a = to_t + PI / 2 * e->strafe;   // se mueve de costado
                    e_move(e, e->x + cosf(a) * 20, e->y + sinf(a) * 20, 34, dt, 0);
                }
            }
            if (e->cd <= 0 && e->burst == 0 && fabsf(angdiff(e->ang, to_t)) < 0.2f) { e->burst = e->var ? irange(2, 4) : irange(1, 2); e->burst_t = 0; }
        }
        if (e->burst > 0 && (e->burst_t -= dt) <= 0) {
            e_fire(e, 0.08f, 300, 0.85f, e->type == E_GRD ? 12 : e->var ? 10 : 8);
            e->burst_t = e->var ? 0.12f : 0.25f;
            if (--e->burst == 0) e->cd = e->var ? frange(1.2f, 2.0f) : frange(0.9f, 1.6f);
        }
        break;
    }
}

// Los dos zombis chicos irradiados que llama La Bestia (están reservados en el mapa, escondidos, y no reaparecen)
static int boss_call(Enemy *b, float tx, float ty)
{
    int n = 0;
    for (int i = 0; i < g_ne && n < 2; i++) {
        Enemy *s = &g_e[i];
        if (!s->summon || s->state != ES_DEAD) continue;
        float a = n ? PI : 0.4f + b->ang, x = b->x + cosf(a) * 20, y = b->y + sinf(a) * 20;
        if (box_hits(x, y, 5)) { x = b->x; y = b->y; }
        memset(s, 0, sizeof(*s));
        s->type = E_DOG;
        s->summon = s->irr = 1;
        s->fac = F_CREA;
        s->x = s->hx = x; s->y = s->hy = y;
        s->tx = tx; s->ty = ty;
        s->hp = EHP[E_DOG];
        s->state = ES_COMBAT;
        s->strafe = n ? 1 : -1;
        blood(x, y, 3);
        n++;
    }
    return n;
}

// La Bestia del charco. Tres ataques: embestida en línea recta (con un segundo de aviso), golpe al suelo que daña
// en un radio y suma radiación, y cada cierto tiempo llama a dos zombis chicos irradiados.
static void update_boss(Enemy *e, int self, float dt)
{
    int b2 = e->type == E_BOSS2;   // Cabeza de Hierro: embiste más rápido y empuja lejos; no llama refuerzos
    float d, fx, fy;
    int ti = foe_pick(e, self, e->state == ES_IDLE ? 170 : 280, 0, &d);
    int sees = ti != -2;
    if (!sees) { fx = P.x; fy = P.y; d = dist(e->x, e->y, fx, fy); }
    else { fx = foe_x(ti); fy = foe_y(ti); }
    float to_p = atan2f(fy - e->y, fx - e->x);
    switch (e->state) {
    case ES_IDLE:
        e->t -= dt;
        if (dist(e->x, e->y, e->tx, e->ty) > 4) e_move(e, e->tx, e->ty, 12, dt, 1);
        else if (e->t <= 0) { e->t = frange(2, 5); e->tx = e->hx + frange(-30, 30); e->ty = e->hy + frange(-24, 24); }
        if (sees) { e->state = ES_COMBAT; e->cd = 1.2f; e->call_t = b2 ? 1e9f : frange(5, 8); e->atk = BA_NONE; e->see_t = 0; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 320); }
        break;
    case ES_ALERT:
        e->t += dt;
        e_chase(e, e->tx, e->ty, 40, dt, 1);
        if (sees) { e->state = ES_COMBAT; e->cd = 1.0f; e->see_t = 0; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 320); }
        else if (e->t > 8 || dist(e->x, e->y, e->tx, e->ty) < 8) { e->state = ES_IDLE; e->tx = e->hx; e->ty = e->hy; e->t = 2; }
        break;
    case ES_COMBAT:
        if (sees) { e->see_t = 0; e->tx = fx; e->ty = fy; }
        else if (e->atk == BA_NONE && (e->see_t += dt) > 6) { e->state = ES_ALERT; e->t = 0; break; }
        e->call_t -= dt;
        switch (e->atk) {
        case BA_NONE:
            e->cd -= dt;
            if (sees) e->ang = turn(e->ang, to_p, 5 * dt);
            if (d > 26 || !sees) e_chase(e, e->tx, e->ty, 44, dt, 1);
            if (!sees || e->cd > 0) break;
            if (!b2 && e->call_t <= 0 && d > 30) { e->atk = BA_CALL; e->atk_t = 0.8f; snd_at(SFX_BOSS_ROAR, 1, e->x, e->y, 320); }
            else if (d < (b2 ? 44 : 38)) { e->atk = BA_SLAM; e->atk_t = 0.9f; }
            else if (d > (b2 ? 50 : 60) && d < (b2 ? 260 : 220)) { e->atk = BA_WIND; e->atk_t = b2 ? 0.75f : 1.0f; e->atk_a = to_p; e->burst = 0; }   // aviso de 1 s
            break;
        case BA_WIND:   // se planta y apunta: se ve la línea por donde va a embestir
            e->ang = turn(e->ang, e->atk_a, 8 * dt);
            if ((e->atk_t -= dt) <= 0) { e->atk = BA_RUSH; e->atk_t = b2 ? 0.8f : 0.7f; }
            break;
        case BA_RUSH: {   // embestida: recto y rápido; se detiene contra un muro
            float ox = e->x, oy = e->y, st = (b2 ? 255 : 190) * dt;
            move(&e->x, &e->y, cosf(e->atk_a) * st, sinf(e->atk_a) * st, 6);
            e->ang = e->atk_a;
            e->walk += dt * 2;
            e->moving = 1;
            if (!e->burst && sees && d < (b2 ? 22 : 18)) {   // golpea al adversario que embiste
                e->burst = 1;
                foe_hit(e, ti, b2 ? 34 : 30);
                if (ti == -1 && g_state == GS_PLAY) push_player(e->atk_a, b2 ? 96 : 32);
                if (g_shake < 7) g_shake = 7;
            }
            if ((e->atk_t -= dt) <= 0 || dist(ox, oy, e->x, e->y) < st * 0.3f) {
                if (g_shake < 6) g_shake = 6;
                e->atk = BA_NONE;
                e->cd = b2 ? 1.3f : 1.8f;
            }
            break;
        }
        case BA_SLAM:   // golpe al suelo: daña en un radio
            if ((e->atk_t -= dt) <= 0) {
                snd_at(SFX_ZBITE, 1, e->x, e->y, 320);
                for (int k = -1; k < g_ne; k++) {
                    float cx = k == -1 ? P.x : g_e[k].x, cy = k == -1 ? P.y : g_e[k].y;
                    if (k == -1 ? 0 : (k == self || g_e[k].state == ES_DEAD || g_e[k].fac == e->fac)) continue;
                    if (dist(e->x, e->y, cx, cy) > 54) continue;
                    if (k == -1) {
                        if (g_state != GS_PLAY) continue;
                        if (b2) { hurt_player(16); push_player(atan2f(P.y - e->y, P.x - e->x), 44); }
                        else { hurt_player(18); add_rad(8); }   // el golpe suma radiación
                    } else hurt_enemy(&g_e[k], 18, e->x, e->y, 0);
                }
                for (int k = 0; k < 18; k++) {   // polvo y chispas verdes en anillo
                    float a = k * PI / 9;
                    part(e->x, e->y - 2, cosf(a) * 70, sinf(a) * 50, 0.45f, k & 1 ? RGBA(130, 255, 90, 255) : RGBA(150, 140, 120, 255), 2);
                }
                if (g_shake < 8) g_shake = 8;
                e->atk = BA_NONE;
                e->cd = 2.0f;
            }
            break;
        case BA_CALL:   // alza la cabeza y llama a dos zombis chicos irradiados
            if ((e->atk_t -= dt) <= 0) {
                if (boss_call(e, fx, fy) > 0 && sees) msg("La Bestia llama a los suyos.");
                e->call_t = frange(14, 20);
                e->atk = BA_NONE;
                e->cd = 1.0f;
            }
            break;
        }
        break;
    }
    float dp = dist(e->x, e->y, P.x, P.y);   // no encimarse con el jugador
    if (dp < 12 && dp > 0.01f) {
        float push = (12 - dp) * 0.5f;
        move(&e->x, &e->y, -(P.x - e->x) / dp * push, -(P.y - e->y) / dp * push, 5);
    }
}

// Los que atacan cuerpo a cuerpo: velocidad al perseguir, al estar alerta y al vagar; golpe, alcance, tiempo entre golpes y a cuánto ven quietos
typedef struct { float chase, alert, wander, dmg, reach, cd, idle; } ChaseDef;
static const ChaseDef CHASE[E_TYPES] = {
    [E_DOG] = {74, 60, 22, 9, 12, 1.0f, 110}, [E_BIG] = {46, 36, 14, 22, 17, 1.6f, 130},
    [E_RUN] = {102, 80, 28, 6, 12, 0.75f, 120}, [E_WOLF] = {98, 78, 26, 11, 13, 0.9f, 140},
    [E_ARM] = {42, 36, 12, 20, 17, 1.5f, 110},
};
static void update_enemy(Enemy *e, int self, float dt)
{
    if (e->state == ES_DEAD) return;
    if (e->flash > 0) e->flash -= dt;
    if (e->muzzle > 0) e->muzzle -= dt;
    e->moving = 0;
    if (e->kx != 0 || e->ky != 0) { knock_step(e, dt); if (e->state == ES_DEAD) return; }
    if (e->stun > 0) { e->stun -= dt; return; }   // aturdido por un golpe
    if (e->mini && mj_update(e, self, dt)) return;   // [armas2]
    if (e->hp < EHP[e->type] / 2 && (e->drip -= dt) <= 0) {   // herido: va dejando un rastro
        e->drip = 0.5f;
        decal(e->x, e->y, S_BLOOD0 + (int)(rnd() % 3), 0.4f);
    }
    if (is_human(e)) { update_human(e, self, dt); return; }
    if (e->type >= E_VIUDA) { jz_ai(e, self, dt); return; }   // [jefes2]
    if (is_boss(e)) { update_boss(e, self, dt); return; }
    const ChaseDef *cd = &CHASE[e->type];
    if (e->ev == EVN_HORDE && e->state == ES_ALERT) e->t = 0;   // la horda no se cansa de caminar hacia el otro extremo
    int heavy = e->type == E_BIG || e->type == E_ARM, dog = cd->chase > 0, spit = e->type == E_SPIT;   // dog: el que ataca cuerpo a cuerpo
    float sight = (e->state == ES_IDLE ? (dog ? (g_sneak ? 22 : cd->idle) : 150) : 210) * art_see();   // [artef] Imán negro: te ven de más lejos
    float d, fx, fy;   // su adversario: el más cercano que ve (el jugador, bandidos o guardias)
    int ti = foe_pick(e, self, sight, e->state == ES_IDLE && !dog, &d);
    int sees = ti != -2;
    if (!sees) { fx = P.x; fy = P.y; d = dist(e->x, e->y, fx, fy); }
    else { fx = foe_x(ti); fy = foe_y(ti); }
    float to_p = atan2f(fy - e->y, fx - e->x);

    switch (e->state) {
    case ES_IDLE:
        e->t -= dt;
        if (dist(e->x, e->y, e->tx, e->ty) > 4) e_move(e, e->tx, e->ty, dog ? cd->wander : 18, dt, 1);
        else if (e->t <= 0) {
            e->t = frange(1.5f, 4.5f);
            e->tx = e->hx + frange(-36, 36);
            e->ty = e->hy + frange(-36, 36);
            if (!dog) e->ang += frange(-1.2f, 1.2f);
        }
        if (sees || (dog && d < (g_sneak ? 12 : 34))) {
            e->state = ES_COMBAT;
            snd_at(SFX_GROWL, 0.8f, e->x, e->y, 260);
            e->cd = dog ? 0 : frange(0.6f, 1.0f);
            e->see_t = 0;
        }
        break;
    case ES_ALERT:
        e->t += dt;
        e_chase(e, e->tx, e->ty, dog ? cd->alert : 50, dt, 1);
        if (sees) { e->state = ES_COMBAT; e->cd = dog ? 0 : frange(0.5f, 0.9f); e->see_t = 0; snd_at(SFX_GROWL, 0.8f, e->x, e->y, 260); }
        else if (e->t > 7 || dist(e->x, e->y, e->tx, e->ty) < 8) { e->state = ES_IDLE; e->tx = e->hx; e->ty = e->hy; e->t = 2; }
        break;
    case ES_COMBAT:
        e->cd -= dt;
        if (sees) { e->see_t = 0; e->tx = fx; e->ty = fy; }
        else if ((e->see_t += dt) > 5) { e->state = ES_ALERT; e->t = 0; break; }
        if (dog) {
            if (!heavy && e->cd > 0.45f) {   // despues de morder se aparta en circulo
                float a = to_p + PI * 0.62f * e->strafe;
                e_move(e, e->x + cosf(a) * 20, e->y + sinf(a) * 20, 56, dt, 1);
            } else e_chase(e, e->tx, e->ty, cd->chase, dt, 1);
            if (sees && d < cd->reach && e->cd <= 0) {
                float k = e->irr ? IRR_DMG : 1;
                foe_hit(e, ti, cd->dmg * k);
                snd_at(SFX_ZBITE, 0.9f, e->x, e->y, 240);
                if (ti == -1 && g_state == GS_PLAY) {
                    if (e->irr) add_rad(7);   // cada golpe del irradiado suma radiación
                    if (heavy) {   // el golpe del grande y del acorazado empuja
                        push_player(atan2f(P.y - e->y, P.x - e->x), 26);
                        if (g_shake < 5) g_shake = 5;
                    } else P.bleed += 1.2f;
                }
                e->cd = cd->cd;
                e->strafe = -e->strafe;
            }
        } else {
            if (sees) {
                e->ang = turn(e->ang, to_p, 6 * dt);
                if ((e->strafe_t -= dt) <= 0) { e->strafe = -e->strafe; e->strafe_t = frange(1.1f, 2.4f); }
                if (d > (spit ? 175 : 135)) e_chase(e, fx, fy, 50, dt, 0);
                else if (d < (spit ? 100 : 55)) e_move(e, e->x - cosf(to_p) * 20, e->y - sinf(to_p) * 20, 40, dt, 0);
                else {
                    float a = to_p + PI / 2 * e->strafe;
                    e_move(e, e->x + cosf(a) * 20, e->y + sinf(a) * 20, 30, dt, 0);
                }
                if (e->cd <= 0 && e->burst == 0 && fabsf(angdiff(e->ang, to_p)) < 0.25f) { e->burst = spit ? 1 : irange(1, 3); e->burst_t = 0; }
            } else e_chase(e, e->tx, e->ty, 46, dt, 1);
            if (e->burst > 0 && (e->burst_t -= dt) <= 0) {
                if (spit) e_spit(e); else e_fire(e, 0.075f, 290, 0.8f, e->irr ? 10 * IRR_DMG : 10);
                e->burst_t = 0.17f;
                if (--e->burst == 0) e->cd = spit ? frange(2.0f, 3.0f) : frange(1.1f, 2.0f);
            }
        }
        break;
    }
    // no encimarse con el jugador
    float dp = dist(e->x, e->y, P.x, P.y);
    if (dp < 9 && dp > 0.01f) {
        float push = (9 - dp);
        move(&e->x, &e->y, -(P.x - e->x) / dp * push, -(P.y - e->y) / dp * push, 5);
    }
}

static void update_bullets(float dt)
{
    for (int i = 0; i < MAXB; i++) {
        Bullet *b = &g_b[i];
        if (!b->on) continue;
        int was_acid = b->kind == K_ACID;
        float step = sqrtf(b->vx * b->vx + b->vy * b->vy) * dt;
        int n = (int)(step / 4) + 1;
        for (int s = 0; s < n && b->on; s++) {
            b->x += b->vx * dt / n;
            b->y += b->vy * dt / n;
            if (blocks_shot((int)floorf(b->x / TS), (int)floorf(b->y / TS))) { sparks(b->x, b->y - BODY_H); snd_at(SFX_WALL, 0.6f, b->x, b->y, 240); b->on = 0; break; }
            if (b->mine) {
                for (int k = 0; k < g_ne; k++) {
                    Enemy *e = &g_e[k];
                    if (e->state == ES_DEAD || b->hit == k + 1 || dist(b->x, b->y, e->x, e->y) > hit_r(e)) continue;
                    if (e->armor > 0) sparks_n(b->x, b->y - BODY_H - 3, 7, 1.6f);   // el blindaje suelta chispas
                    if (is_post(e) && e->fac == F_FARO) farol_hostile();   // le diste a un farolero: todos los del puesto se vuelven hostiles
                    else if (is_post(e)) {   // le diste a un guardia de Kolos: ahora es hostil y pierdes reputación
                        e->fac = F_BAND;
                        e->state = ES_COMBAT;
                        g_rep = g_rep > 3 ? g_rep - 3 : 0;
                        msg("Le diste a un guardia de Kolos: ahora es hostil. -3 de reputación.");
                    }
                    hurt_enemy(e, bullet_dmg(b->dmg, b->kind, e->armor), P.x, P.y, 1);
                    if (b->pierce > 0) { b->pierce--; b->hit = k + 1; b->dmg *= 0.75f; }   // el fusil de cerrojo atraviesa a un enemigo
                    else b->on = 0;
                    break;
                }
            } else if (!fr_fac(b->fac) && b->kind != K_ACID && dog_hit_test(b->x, b->y)) { dog_hurt(b->dmg * 0.6f); b->on = 0; }
            else if (!fr_fac(b->fac) && dist(b->x, b->y, P.x, P.y) < 6) { hurt_player(b->dmg); b->on = 0; }   // las de Kolos no te tocan
            else {   // la bala de un bando pega en quien sea adversario suyo
                for (int k = 0; k < g_ne; k++) {
                    Enemy *e = &g_e[k];
                    if (e->state == ES_DEAD || e->fac == b->fac || dist(b->x, b->y, e->x, e->y) > hit_r(e)) continue;
                    if (e->armor > 0) sparks_n(b->x, b->y - BODY_H - 3, 5, 1.2f);
                    hurt_enemy(e, bullet_dmg(b->dmg, B_NORMAL, e->armor), b->x - b->vx * 0.1f, b->y - b->vy * 0.1f, 0);
                    b->on = 0;
                    break;
                }
            }
        }
        if ((b->life -= dt) <= 0) b->on = 0;
        if (was_acid && !b->on) acid_splat(b->x, b->y);   // el ácido cae donde termina
    }
}

// Renueva una zona (la viva o una guardada): los muertos vuelven a su lugar de origen y se rellenan los cajones y
// cuerpos, menos el del trabajo, el del arma y el de la máscara. lejos > 0: los muertos a menos de lejos px del jugador no vuelven
static int renew_zone(int z, Enemy *en, int ne, Crate *cr, int ncr, float lejos)
{
    int renew = 0;
    if (z == 0) return 0;   // la base (Estación Kolos) no se renueva: sin reaparición y sus cajones son un botín de una sola vez
    for (int i = 0; i < ne; i++) {
        Enemy *e = &en[i];
        if (e->state != ES_DEAD || e->type == E_BOSS || e->type == E_BOSS2 || e->summon || (lejos > 0 && dist(P.x, P.y, e->hx, e->hy) <= lejos)) continue;   // el jefe no reaparece
        int type = e->type, irr = e->irr, var = e->var;
        float hx = e->hx, hy = e->hy;
        memset(e, 0, sizeof(*e));
        e->type = type;
        e->irr = irr;
        e->x = e->hx = e->tx = hx;
        e->y = e->hy = e->ty = hy;
        e->ang = frange(0, 2 * PI);
        e->hp = EHP[type];
        e->armor = ARMOR[type];
        e->strafe = rnd() & 1 ? 1 : -1;
        e->t = frange(0.5f, 3);
        e->fac = fac_of(type);
        e->var = var;
        if (type == E_GRD) e->ang = 0;
        if (type == E_SNP) e->var = 1;
        renew = 1;
    }
    for (int i = 0; i < ncr; i++)
        if (cr[i].open && cr[i].kind != CK_JOB && cr[i].kind != CK_WEAPON && cr[i].kind != CK_MASK &&
            cr[i].kind != CK_MEDS && cr[i].kind != CK_REG) { cr[i].open = 0; renew = 1; }
    return renew;
}

// Termina la tormenta de Ceniza y el mundo se renueva, en todas las zonas
static void storm_end(void)
{
    char s[96];
    int renew = 0;
    g_storm_on = 0;
    {   // el búnker se rehace con otra semilla (si estás adentro, al salir)
        u32 nxt = (g_bseed * 1664525u + 1013904223u + (u32)(g_play_t * 100)) | 1;
        if (g_zone == 2) { g_bpend = nxt; g_bstale = 1; }
        else { g_bseed = nxt; g_zc[2].vis = 0; }
    }
    renew |= renew_zone(g_zone, g_e, g_ne, g_cr, g_ncr, 200);
    for (int z = 0; z < 2; z++)   // las zonas en las que no estás también se renuevan
        if (z != g_zone && g_zc[z].vis) renew_zone(z, g_zc[z].e, g_zc[z].ne, g_zc[z].cr, g_zc[z].ncr, 0);
    snprintf(s, sizeof(s), "%s%s", renew ? "La Ceniza trajo algo nuevo." : "La Ceniza pasó.",
             shop_restock() ? " El comerciante repuso existencias." : "");
    msg(s);
}

// Lo que pasa mientras duerme, a pantalla negra: la tormenta de Ceniza termina (o, si no hay, su reloj
// avanza 60 s), +40 de vida, -25 de hambre, -30 de sed (con la cama de la base: +70, -12,5 y -15), y se guarda la partida
static void sleep_apply(void)
{
    char s[96];
    int storm = 0, siren = 0;
    storm_end();   // [bomba] dormir renueva el mundo
    int bed = g_zone == 0 && g_up[UP_BED];   // la cama de la base: +70 de vida y la mitad de hambre y sed
    P.hp = clampf(P.hp + (bed ? 70 : 40), 0, hp_max());
    P.hunger = clampf(P.hunger - (bed ? 12.5f : 25), 0, 100);
    P.thirst = clampf(P.thirst - (bed ? 15 : 30), 0, 100);
    snprintf(s, sizeof(s), "%s%s%s", storm ? "" : bed ? "Dormiste bien en tu cama." : "Dormiste un rato.",
             save_game() ? " Partida guardada." : "", siren ? "" : "");
    msg(s);
    duro_sleep();   // [duro] dormir en el búnker con luz cura todo
}

#include "bomba.inc"   // [bomba] la cuenta de la bomba

// Ambientes en bucle y latido, según dónde esté el jugador
static void snd_ambient(float dt)
{
    static float heart_t;
    if (P.hp < 35 && g_state == GS_PLAY) {   // latido: más rápido cuanto menos vida
        if ((heart_t -= dt) <= 0) { heart_t = 0.6f + 0.7f * P.hp / 35; snd_play(SFX_HEART, 0.8f, 0); }
    } else heart_t = 0;
    float fd = 1e9f;   // fogata: crece al acercarse a un barril con fuego
    for (int i = 0; i < g_nfire; i++) fd = fminf(fd, dist(P.x, P.y, g_fire[i][0], g_fire[i][1]));
    float fk = clampf(1 - fd / 150, 0, 1);
    snd_loop(SFX_FIRE, fk * fk * 0.7f);
    float gg = 0;      // contador Geiger: fuerte en el charco (más con más dosis, menos con máscara), un poco después
    if (ground_at(P.x, P.y) == G_RAD) gg = (0.45f + 0.4f * P.rad / 100) * (mask_on() >= 0 ? 0.7f : 1);
    else if (P.rad > 1) gg = 0.1f + 0.15f * P.rad / 100;
    snd_loop(SFX_GEIGER, gg);
    snd_loop(SFX_WIND, g_storm_on ? (indoors(P.x, P.y) ? 0.15f : 0.6f) : dz_llueve() ? (indoors(P.x, P.y) ? 0.1f : 0.3f) : 0);   // [lluvia]   // viento de la Ceniza
}

#define FAR_AI 28      // casillas: más lejos que esto, el enemigo duerme casi todos los cuadros
#define FAR_EVERY 6    // y se actualiza uno de cada FAR_EVERY
static void update_world(float dt)
{
    g_play_t += dt;
    dz_tick(dt);   // [diseno] el reloj del juego
    update_player(dt);
    if (g_state == GS_PLAY) zone_portal();
    // los enemigos lejos del jugador (más de FAR_AI casillas) no corren su IA completa cada cuadro: se actualizan cada FAR_EVERY cuadros, con el tiempo acumulado
    static unsigned s_ai_fr;
    s_ai_fr++;
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        if (e->state != ES_DEAD && (fabsf(e->x - P.x) > FAR_AI * TS || fabsf(e->y - P.y) > FAR_AI * TS)) {
            if ((s_ai_fr + (unsigned)i) % FAR_EVERY) continue;
            update_enemy(e, i, dt * FAR_EVERY);
        } else update_enemy(e, i, dt);
    }
    for (int i = 0; i < g_ne; i++)          // que no se amontonen
        for (int k = i + 1; k < g_ne; k++) {
            Enemy *a = &g_e[i], *b = &g_e[k];
            if (fabsf(a->x - b->x) > 10 || fabsf(a->y - b->y) > 10) continue;
            if (a->state == ES_DEAD || b->state == ES_DEAD) continue;
            float d = dist(a->x, a->y, b->x, b->y);
            if (d > 10 || d < 0.01f) continue;
            float px = (b->x - a->x) / d * (10 - d) * 0.5f, py = (b->y - a->y) / d * (10 - d) * 0.5f;
            move(&a->x, &a->y, -px, -py, 5);
            move(&b->x, &b->y, px, py, 5);
        }
    update_bullets(dt);
    if (g_swing_t > 0) g_swing_t -= dt;
    for (int i = 0; i < 10; i++) {   // los charcos de ácido queman a quien se queda dentro
        if (g_pud[i].life <= 0) continue;
        g_pud[i].life -= dt;
        if (g_state == GS_PLAY && dist(P.x, P.y, g_pud[i].x, g_pud[i].y) < 11 && (g_pud_tick += dt) >= 0.5f) { g_pud_tick = 0; hurt_player(4); }
    }
    if (g_zone == 1 && g_state == GS_PLAY) anom_update(dt);
    dog_update(dt);
    jefes_update(dt);   // [jefes]
    pozo_update(dt);   // [pozo]
    artef_update(dt);   // [artef]
    ev_update(dt);
    hand_world_update(dt);
    for (int i = 0; i < MAXP; i++) {
        Part *p = &g_p[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vx *= 0.9f;
        p->vy *= 0.9f;
    }
    for (int i = 0; i < MAXPOP; i++)
        if (g_pop[i].life > 0) { g_pop[i].life -= dt; g_pop[i].y -= 9 * dt; }

    bomba_update(dt);   // [bomba]
    snd_ambient(dt);
}

// ------------------------------------------------------------------ dibujo
static void update_camera(float dt)
{
    float tx = P.x + cosf(P.ang) * 26 - SCR_W / 2, ty = P.y - 8 + sinf(P.ang) * 20 - SCR_H / 2;
    float k = clampf(dt * 5, 0, 1);
    g_camx += (clampf(tx, 0, WORLD_W - SCR_W) - g_camx) * k;
    g_camy += (clampf(ty, 0, WORLD_H - SCR_H) - g_camy) * k;
    g_shake = g_shake > 0.1f ? g_shake * (1 - clampf(dt * 12, 0, 1)) : 0;
    float shk = g_shake * OPT_SHK[g_opt[OPT_SHAKE]];
    g_ox = floorf(g_camx + frange(-shk, shk));
    g_oy = floorf(g_camy + frange(-shk, shk));
}

// Vista 3/4: el suelo se ve desde arriba; lo demas tiene altura y se apoya en el borde de abajo de su casilla.
// Se dibuja por filas, de arriba hacia abajo, para que lo de adelante tape lo de atras.
static void gun_tip(float x, float y, float ang, int gun, float *tx, float *ty)
{
    float len = 1 + SPR[gun].w;
    *tx = x + cosf(ang) * len;
    *ty = y - BODY_H + sinf(ang) * len * 0.8f;
}

// Los cuerpos vienen en 3 vistas (frente, espalda, perfil); el perfil mira a la derecha y se espeja
static int body_view(float ang) { return fabsf(cosf(ang)) > 0.72f ? 2 : sinf(ang) > 0 ? 0 : 1; }

// Cuadro del cuerpo: por vista hay 6 cuadros de quieto y despues nrun de andar. Apoyado en los pies.
static void draw_body_k(int base, int nrun, float x, float y, float ang, int moving, float walk, float rate, u32 col, float k)
{
    int f = moving ? 6 + (int)(walk * rate) % nrun : (int)(g_time * 5) % 6;
    Spr body = SPR[base + body_view(ang) * (6 + nrun) + f];
    if (k == 1) { r_spr_f(body, floorf(x) - body.w / 2, floorf(y) + 3 - body.h, cosf(ang) < 0, col); return; }
    float w = body.w * k, h = body.h * k;
    if (cosf(ang) < 0) body = (Spr){body.x + body.w, body.y, -body.w, body.h, body.p};   // espejado: el ancho negativo da vuelta la textura
    r_spr_s(body, floorf(x - w / 2), floorf(y + 3 - h), w, h, col);
}
static void draw_body(int base, int nrun, float x, float y, float ang, int moving, float walk, float rate, u32 col)
{
    draw_body_k(base, nrun, x, y, ang, moving, walk, rate, col, 1);
}

// El jugador: el cuerpo sin manos y, aparte, el arma con sus manos girada hacia donde apunta
static void draw_person_b(int base, float x, float y, float ang, int moving, float walk, int gun, u32 col)
{
    float c = cosf(ang), s = sinf(ang);
    Spr g = SPR[gun];
    float reach = 1 + g.w * 0.5f, gx = x + c * reach, gy = y - BODY_H + s * reach * 0.8f;
    int back = body_view(ang) == 1, flip = c < 0;
    r_spr(SPR[S_SHADOW], x - 6, y, COL_WHITE);
    if (back) r_rot_f(g, gx, gy, ang + g_gun_off, flip, col);   // de espalda, el arma queda detras del cuerpo
    draw_body(base, 6, x, y, ang, moving, walk, 10, col);
    if (!back) r_rot_f(g, gx, gy, ang + g_gun_off, flip, col);
}
static void draw_person(float x, float y, float ang, int moving, float walk, int gun, u32 col)
{
    draw_person_b(S_PLY_FI0, x, y, ang, moving, walk, gun, col);
}
#include "jefes.inc"   // [jefes]
#include "jefes2.inc"   // [jefes2] La Viuda blanca, El Capataz y El Fermentado

// Mientras cae un zombi recien muerto (los que vienen muertos de la partida guardada tienen dead_t = 0)
#define DIE_FPS 10
static int dying(const Enemy *e)
{
    float k = (g_time - e->dead_t) * DIE_FPS;
    return e->dead_t > 0 && k >= 0 && k < 5;
}

// Cómo se dibuja cada enemigo: tamaño y color sobre los cuerpos del paquete (0 chico, 1 mediano, 2 grande)
static int body_class(int type) { return type >= E_VIUDA ? (type == E_VIUDA ? 0 : 2) : type == E_DOG || type == E_RUN || type == E_WOLF ? 0 : type == E_BIG || type == E_BOSS || type == E_BOSS2 || type == E_ARM ? 2 : 1; }
static float e_scale(const Enemy *e)
{
    if (e->mini) return MJ[e->mini - 1].scale;   // [armas2]
    switch (e->type) {
    case E_BOSS: return BOSS_K;
    case E_BOSS2: return 1.75f;
    case E_VIUDA: return 2.1f;   // [jefes2]
    case E_CAPATAZ: return 1;   // [jefes2]
    case E_FERMEN: return e->var ? 1.2f : 1.9f;   // [jefes2]
    case E_RUN: return 0.9f;
    case E_WOLF: return 1.2f;
    case E_ARM: return 1.2f;
    case E_SPIT: return 0.95f;
    default: return 1;
    }
}
static u32 e_tint(const Enemy *e)
{
    if (e->mini) return MJ[e->mini - 1].tint;   // [armas2]
    switch (e->type) {
    case E_BOSS: return RGBA(120, 240, 110, 255);
    case E_BOSS2: return RGBA(255, 150, 120, 255);
    case E_VIUDA: return RGBA(205, 230, 255, 255);   // [jefes2]
    case E_CAPATAZ: return RGBA(255, 190, 110, 255);   // [jefes2]
    case E_FERMEN: return RGBA(170, 255, 90, 255);   // [jefes2]
    case E_RUN: return RGBA(255, 176, 120, 255);
    case E_WOLF: return RGBA(232, 240, 255, 255);
    case E_ARM: return RGBA(150, 164, 190, 255);
    case E_SPIT: return RGBA(206, 255, 110, 255);
    default: return e->irr ? RGBA(150, 255, 150, 255) : COL_WHITE;   // el irradiado se ve verdoso
    }
}

#include "stalk.inc"   // [stalk] arte unido: arboles, casas de pueblo, rejas, cofre y bandidos

static void draw_enemy(const Enemy *e)
{
    if (jz_draw(e)) return;   // [jefes2]
    float x = e->x - g_ox, y = e->y - g_oy;
    if (x < -24 || y < -24 || x > SCR_W + 24 || y > SCR_H + 40) return;
    if (st_bandido(e, x, y)) return;   // [stalk] bandidos con su propio cuerpo
    if (is_human(e)) {   // bandidos y guardias: el cuerpo del jugador con otra ropa
        int base = e->type == E_BAN || e->type == E_SNP ? S_BAN_FI0 : S_GRD_FI0;
        if (e->state == ES_DEAD) {   // no tienen cuadros de caída: cae de lado, girando el cuerpo, y queda tendido
            float k = dying(e) ? (g_time - e->dead_t) * DIE_FPS / 5 : 1;
            Spr b = SPR[base + 24];   // de perfil, quieto
            int flip = cosf(e->ang) < 0;
            r_rot(b, floorf(x), floorf(y + 3 - b.h / 2 * (1 - k) - 4 * k), (flip ? -1 : 1) * 1.5708f * k, 1, k < 1 ? COL_WHITE : RGBA(150, 138, 138, 255));
            return;
        }
        u32 hc = e->flash > 0 ? RGBA(255, 130, 120, 255) : e->type == E_SNP ? RGBA(150, 160, 190, 255) : COL_WHITE;
        draw_person_b(base, x, y, e->ang, e->moving, e->walk, WDEF[e->type == E_SNP ? W_BOLT : e->var ? W_AK : W_MAKAROV].spr, hc);
        return;
    }
    if (e->state == ES_DEAD && (e->summon || e->ev == EVN_HORDE) && e->dead_t == 0) return;   // el llamado que aún no salió
    float k = e_scale(e);
    int cls = body_class(e->type);   // 0 chico, 1 mediano, 2 grande
    if (e->state == ES_DEAD) {   // cae de lado (6 cuadros del paquete) y queda tendido en el ultimo
        int f = dying(e) ? (int)((g_time - e->dead_t) * DIE_FPS) : 5, flip = cosf(e->ang) < 0;
        int dd = cls == 0 ? S_ZS_DIE0 : cls == 2 ? S_ZB_DIE0 : S_ZA_DIE0;
        int ds = cls == 0 ? S_ZS_SI0 : cls == 2 ? S_ZB_SI0 : S_ZA_SI0;
        Spr sp = SPR[dd + f];
        int cx = SPR[ds].w / 2;   // los pies quedan donde estaban de pie
        u32 tint = e_tint(e), dc = f < 2 ? RGBA(255, 130, 120, 255) : f < 5 ? tint : RGBA(150, 138, 138, 255);
        if (k == 1) {
            r_spr_f(sp, floorf(x) - (flip ? sp.w - cx : cx), floorf(y) + 3 - sp.h, flip, dc);
        } else {   // los más grandes o más chicos que lo normal
            if (flip) sp = (Spr){sp.x + sp.w, sp.y, -sp.w, sp.h, sp.p};
            float w = (sp.w < 0 ? -sp.w : sp.w) * k, h = sp.h * k, c = cx * k;
            r_spr_s(sp, floorf(x) - (flip ? w - c : c), floorf(y) + 3 - h, w, h, dc);
        }
        return;
    }
    u32 col = e->flash > 0 ? (is_boss(e) ? RGBA(255, 150, 130, 255) : RGBA(255, 130, 120, 255)) : e_tint(e);
    if (k == 1) r_spr(SPR[S_SHADOW], x - 6, y, COL_WHITE);
    else r_spr_s(SPR[S_SHADOW], x - 6 * k, y - 1, SPR[S_SHADOW].w * k, SPR[S_SHADOW].h * k, COL_WHITE);
    float rate = e->type == E_RUN ? 13 : e->type == E_WOLF ? 11 : cls == 0 ? 9 : cls == 2 ? 7 : 9;
    draw_body_k(cls == 0 ? S_ZS_FI0 : cls == 2 ? S_ZB_FI0 : S_ZA_FI0, cls == 0 ? 6 : 8, x, y, e->ang, e->moving, e->walk, rate, col, k);
}

// Lo que hay en una casilla, apoyado en su borde de abajo
static void draw_object(int tx, int ty)
{
    int o = g_o[ty][tx], s = -1;
    u32 col = COL_WHITE;
    if (o >= O_NTW && o < O_MNEND) { mn_draw_obj(tx, ty, o); return; }   // [mapnuc]
    if (o >= O_M3WAT && o < O_M3END) { m3_draw_obj(tx, ty, o); return; }   // [mapas3]
    if (o >= O_UCOL && o <= O_UKIO) { urbe_draw_obj(tx, ty, o); return; }   // [urbe]
    if (o >= O_BK_STAIR) { bk_draw_obj(tx, ty, o); return; }   // [bunker]
    if (ar_obj(tx, ty, o)) return;   // [arte] fabrica
    if (o >= O_DTL && o <= O_DMAP) { dz_draw_obj(tx, ty, o); return; }   // [diseno]
    if (st_obj(tx, ty, o)) return;   // [stalk] arboles nuevos
    if (wall_like(o)) col = dz_tinte_muro();   // [diseno]
    switch (o) {
    case O_WALL: s = S_WALL; break;
    case O_CONT: s = S_CONT; break;
    case O_CONT2: s = S_CONT2; break;
    case O_FENCE: s = (tx > 0 && g_o[ty][tx - 1] == O_FENCE) || (tx + 1 < MW && g_o[ty][tx + 1] == O_FENCE) ? S_FENCE_H : S_FENCE_V; break;
    case O_SAND: s = S_SANDBAG; break;
    case O_BARREL: s = S_BARREL; break;
    case O_FBARREL: s = S_FBARREL; break;
    case O_LAMP: s = S_LAMP; break;
    case O_CROSS: s = S_CROSS; break;
    case O_STAKE: s = S_STAKE; break;
    case O_COT: s = S_COT; break;
    case O_CAR_L: {   // el auto mide 50 px y sus casillas 32: va entero desde la izquierda, centrado, y asoma 9 px por lado
        Spr car = SPR[S_CAR0 + (hash2(tx, ty) & 1)];   // el rojo o el azul hecho chatarra; mira a un lado o al otro
        r_spr_f(car, tx * TS + TS - car.w / 2 - g_ox, (ty + 1) * TS - 29 - g_oy, (hash2(tx, ty) >> 1) & 1, col);   // ruedas 2 px sobre el borde
        return;
    }
    case O_NPC:   // el Chofer: el cuerpo del guardia de Kolos, quieto y mirando al sur
        draw_person_b(S_GRD_FI0, tx * TS + 8 - g_ox, ty * TS + 8 - g_oy, 1.5708f, 0, 0, WDEF[W_MAKAROV].spr, COL_WHITE);
        return;
    case O_MERCH:   // el comerciante: cuerpo de ropa ocre, quieto y mirando al sur
        draw_person_b(S_MER_FI0, tx * TS + 8 - g_ox, ty * TS + 8 - g_oy, 1.5708f, 0, 0, WDEF[W_MAKAROV].spr, COL_WHITE);
        return;
    case O_SCAV:    // el chatarrero: ropa verde azulada, quieto y mirando al este (hacia su banco)
        draw_person_b(S_CHA_FI0, tx * TS + 8 - g_ox, ty * TS + 8 - g_oy, 0, 0, 0, WDEF[W_SAWED].spr, COL_WHITE);
        return;
    case O_BENCH: {   // el banco de trabajo: tablón con patas, una morsa y unas piezas encima
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 6, 14, 3, RGBA(112, 84, 52, 255));
        r_rect(x + 1, y + 9, 14, 1, RGBA(58, 42, 28, 255));
        r_rect(x + 2, y + 10, 2, 5, RGBA(70, 52, 34, 255));
        r_rect(x + 12, y + 10, 2, 5, RGBA(70, 52, 34, 255));
        r_rect(x + 3, y + 3, 4, 3, RGBA(120, 124, 132, 255));
        r_rect(x + 9, y + 4, 5, 2, RGBA(170, 164, 150, 255));
        r_rect(x + 10, y + 3, 1, 1, RGBA(200, 90, 50, 255));
        return;
    }
    case O_CHEST: case O_CHEST2: {   // el arcón del alijo (y el segundo, con herrajes azulados): caja de madera con herrajes
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        if (o == O_CHEST2) {
            r_rect(x + 1, y + 4, 14, 11, RGBA(20, 14, 10, 255));
            r_rect(x + 2, y + 5, 12, 9, RGBA(112, 84, 54, 255));
            r_rect(x + 2, y + 5, 12, 3, RGBA(138, 104, 66, 255));
            r_rect(x + 2, y + 8, 12, 1, RGBA(58, 42, 28, 255));
            r_rect(x + 2, y + 5, 2, 9, RGBA(70, 96, 126, 255));
            r_rect(x + 12, y + 5, 2, 9, RGBA(70, 96, 126, 255));
            r_rect(x + 7, y + 7, 2, 3, RGBA(210, 190, 110, 255));
            return;
        }
        r_rect(x + 1, y + 4, 14, 11, RGBA(20, 14, 10, 255));
        r_rect(x + 2, y + 5, 12, 9, RGBA(126, 92, 56, 255));
        r_rect(x + 2, y + 5, 12, 3, RGBA(150, 112, 70, 255));
        r_rect(x + 2, y + 8, 12, 1, RGBA(58, 42, 28, 255));
        r_rect(x + 2, y + 5, 2, 9, RGBA(92, 94, 98, 255));
        r_rect(x + 12, y + 5, 2, 9, RGBA(92, 94, 98, 255));
        r_rect(x + 7, y + 7, 2, 3, RGBA(210, 190, 110, 255));
        return;
    }
    case O_RUBBLE: {   // escombros: un montón de ladrillos y vigas que tapa el paso
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        u32 h = hash2(tx, ty);
        r_rect(x + 1, y + 9, 14, 6, RGBA(18, 16, 16, 255));
        r_rect(x + 2, y + 6, 12, 8, RGBA(96, 90, 84, 255));
        r_rect(x + 4, y + 3, 7, 5, RGBA(122, 116, 108, 255));
        r_rect(x + 2 + (h & 3), y + 4 + ((h >> 2) & 1), 4, 4, RGBA(78, 74, 70, 255));
        r_rect(x + 9, y + 8, 4, 3, RGBA(150, 98, 70, 255));
        r_rect(x + 3, y + 10, 5, 2, RGBA(140, 108, 82, 255));
        r_rect(x + 1 + ((h >> 4) & 3), y + 7, 9, 2, RGBA(84, 62, 40, 255));   // una viga atravesada
        r_rect(x + 5, y + 4, 2, 1, RGBA(160, 154, 146, 255));
        return;
    }
    case O_SHELF: {   // la estantería: tres baldas con latas y cajas
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x, y - 6, 16, 22, RGBA(20, 14, 10, 255));
        r_rect(x + 1, y - 5, 14, 20, RGBA(96, 70, 44, 255));
        for (int k = 0; k < 3; k++) {
            float by = y - 5 + k * 7;
            r_rect(x + 1, by + 5, 14, 2, RGBA(58, 42, 28, 255));
            r_rect(x + 2, by + 1, 3, 4, RGBA(190, 70, 54, 255));
            r_rect(x + 6, by + 2, 4, 3, RGBA(180, 160, 96, 255));
            r_rect(x + 11, by + 1, 3, 4, k == 1 ? RGBA(90, 150, 170, 255) : RGBA(120, 170, 90, 255));
        }
        return;
    }
    case O_BED: {   // la cama: marco de madera, colchón claro, almohada y manta azul
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x, y + 1, 16, 15, RGBA(20, 14, 10, 255));
        r_rect(x + 1, y + 2, 14, 13, RGBA(104, 76, 48, 255));
        r_rect(x + 2, y + 3, 12, 11, RGBA(214, 208, 190, 255));
        r_rect(x + 3, y + 4, 10, 3, RGBA(240, 236, 224, 255));
        r_rect(x + 2, y + 8, 12, 6, RGBA(62, 92, 130, 255));
        r_rect(x + 2, y + 8, 12, 1, RGBA(92, 126, 168, 255));
        return;
    }
    case O_CLINIC: {   // el puesto de enfermería: camilla blanca con una cruz roja y un botiquín en la pared
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 4, 14, 11, RGBA(20, 18, 18, 255));
        r_rect(x + 2, y + 5, 12, 9, RGBA(226, 224, 218, 255));
        r_rect(x + 2, y + 5, 12, 2, RGBA(246, 244, 238, 255));
        r_rect(x + 7, y + 7, 2, 5, RGBA(200, 40, 40, 255));
        r_rect(x + 5, y + 8, 6, 2, RGBA(200, 40, 40, 255));
        r_rect(x + 3, y - 4, 10, 7, RGBA(30, 70, 60, 255));
        r_rect(x + 4, y - 3, 8, 5, RGBA(214, 220, 214, 255));
        r_rect(x + 7, y - 2, 2, 3, RGBA(200, 40, 40, 255));
        return;
    }
    case O_TOWER: {   // la torre de vigía: cuatro patas, una plataforma alta con techo y una baranda
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x - 1, y + 14, 18, 2, RGBA(18, 14, 10, 120));
        r_rect(x + 1, y - 18, 2, 33, RGBA(84, 62, 40, 255));
        r_rect(x + 13, y - 18, 2, 33, RGBA(84, 62, 40, 255));
        r_rect(x + 3, y - 6, 10, 2, RGBA(70, 52, 34, 255));
        r_rect(x + 3, y + 4, 10, 2, RGBA(70, 52, 34, 255));
        r_rect(x - 2, y - 24, 20, 3, RGBA(20, 14, 10, 255));
        r_rect(x - 2, y - 23, 20, 2, RGBA(126, 92, 56, 255));
        r_rect(x - 1, y - 32, 18, 9, RGBA(58, 52, 46, 255));
        r_rect(x + 1, y - 30, 14, 4, RGBA(240, 190, 90, 255));   // la luz de la cabina
        r_rect(x - 3, y - 35, 22, 4, RGBA(96, 40, 34, 255));
        r_rect(x - 2, y - 35, 20, 1, RGBA(140, 70, 54, 255));
        return;
    }
    case O_GARDEN: {   // el huerto: un surco de tierra oscura con plantas
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        u32 h = hash2(tx, ty);
        r_rect(x, y + 6, 16, 9, RGBA(52, 38, 26, 255));
        r_rect(x + 1, y + 7, 14, 2, RGBA(76, 56, 38, 255));
        for (int k = 0; k < 4; k++) {
            float px = x + 2 + k * 4;
            int hh = 3 + ((h >> (k * 2)) & 3);
            r_rect(px, y + 12 - hh, 2, hh, RGBA(70, 140, 60, 255));
            r_rect(px - 1, y + 11 - hh, 4, 2, RGBA(110, 176, 80, 255));
            if (k == (int)(h & 3)) r_rect(px, y + 12 - hh, 2, 2, RGBA(206, 64, 48, 255));
        }
        return;
    }
    case O_TANK: {   // el colector de agua: un tanque azul con embudo y un grifo
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 3, 14, 12, RGBA(14, 18, 24, 255));
        r_rect(x + 2, y + 4, 12, 10, RGBA(70, 112, 150, 255));
        r_rect(x + 2, y + 4, 12, 2, RGBA(110, 156, 196, 255));
        r_rect(x + 2, y + 9, 12, 1, RGBA(46, 76, 104, 255));
        r_rect(x + 4, y - 3, 8, 3, RGBA(150, 154, 160, 255));
        r_rect(x + 6, y, 4, 3, RGBA(110, 114, 120, 255));
        r_rect(x + 13, y + 11, 3, 2, RGBA(170, 170, 176, 255));
        return;
    }
    case O_BENCH2: {   // el banco de munición: yunque, prensa y cajas de latón
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 6, 14, 3, RGBA(112, 84, 52, 255));
        r_rect(x + 1, y + 9, 14, 1, RGBA(58, 42, 28, 255));
        r_rect(x + 2, y + 10, 2, 5, RGBA(70, 52, 34, 255));
        r_rect(x + 12, y + 10, 2, 5, RGBA(70, 52, 34, 255));
        r_rect(x + 2, y + 2, 7, 4, RGBA(70, 72, 78, 255));
        r_rect(x + 1, y + 4, 9, 2, RGBA(100, 104, 112, 255));
        r_rect(x + 10, y + 1, 2, 5, RGBA(150, 154, 160, 255));
        r_rect(x + 12, y + 3, 3, 3, RGBA(196, 156, 70, 255));
        r_rect(x + 12, y + 3, 3, 1, RGBA(236, 204, 120, 255));
        return;
    }
    case O_HATCH: {   // la escotilla: una tapa de metal con su manija
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 3, 14, 11, RGBA(24, 22, 20, 255));
        r_rect(x + 2, y + 4, 12, 9, RGBA(92, 90, 84, 255));
        r_rect(x + 2, y + 8, 12, 1, RGBA(56, 54, 50, 255));
        r_rect(x + 6, y + 5, 4, 2, RGBA(168, 160, 120, 255));
        return;
    }
    case O_STAIRS: {   // la escalera: un hueco oscuro con peldaños que suben
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 1, 14, 15, RGBA(8, 8, 10, 255));
        for (int k = 0; k < 4; k++) r_rect(x + 3, y + 3 + k * 3, 10, 2, RGBA(116 - k * 16, 110 - k * 16, 100 - k * 14, 255));
        r_rect(x + 1, y + 1, 14, 1, RGBA(150, 146, 138, 255));
        return;
    }
    case O_MACH: {   // maquinaria: cuerpo de acero con tablero, luces y un caño
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        u32 lc = hash2(tx, ty) & 1 ? RGBA(210, 70, 50, 255) : RGBA(90, 210, 110, 255);
        r_rect(x + 1, y + 3, 14, 13, RGBA(18, 18, 22, 255));
        r_rect(x + 2, y + 4, 12, 11, RGBA(84, 92, 98, 255));
        r_rect(x + 2, y + 4, 12, 3, RGBA(118, 128, 134, 255));
        r_rect(x + 2, y + 12, 12, 3, RGBA(58, 64, 70, 255));
        r_rect(x + 4, y + 8, 5, 3, RGBA(24, 30, 34, 255));
        r_rect(x + 5, y + 9, 3, 1, RGBA(120, 200, 160, 255));
        r_rect(x + 11, y + 8, 2, 2, lc);
        r_rect(x + 12, y, 2, 4, RGBA(116, 98, 74, 255));
        return;
    }
    case O_STANK: {   // tanque grande de 2x2: cilindro oxidado con aros; los bordes se cierran solo donde no hay otro tramo
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        int l = tx > 0 && g_o[ty][tx - 1] == O_STANK, r = tx + 1 < MW && g_o[ty][tx + 1] == O_STANK;
        int u = ty > 0 && g_o[ty - 1][tx] == O_STANK, dn = ty + 1 < MH && g_o[ty + 1][tx] == O_STANK;
        r_rect(x + (l ? 0 : 1), y + (u ? 0 : 2), 16 - (l ? 0 : 1) - (r ? 0 : 1), 16 - (u ? 0 : 2) - (dn ? 0 : 0), RGBA(104, 80, 62, 255));
        r_rect(x + (l ? 0 : 2), y + (u ? 0 : 3), 16 - (l ? 0 : 2) - (r ? 0 : 2), 16 - (u ? 0 : 3), RGBA(138, 106, 80, 255));
        r_rect(x + (l ? 0 : 2), y + (u ? 0 : 3), 16 - (l ? 0 : 2) - (r ? 0 : 2), 2, RGBA(168, 134, 104, 255));
        r_rect(x + (l ? 0 : 2), y + 8, 16 - (l ? 0 : 2) - (r ? 0 : 2), 1, RGBA(70, 52, 40, 255));
        if (!r) r_rect(x + 14, y + (u ? 0 : 3), 2, 16 - (u ? 0 : 3), RGBA(80, 60, 46, 255));
        if (!dn) r_rect(x, y + 14, 16, 2, RGBA(46, 34, 28, 255));
        return;
    }
    case O_TOMB: {   // lápida: losa redondeada de piedra con una cruz grabada, sobre un montón de tierra
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 3, y + 12, 10, 3, RGBA(58, 44, 34, 255));
        r_rect(x + 4, y + 3, 8, 10, RGBA(18, 18, 20, 255));
        r_rect(x + 5, y + 2, 6, 1, RGBA(18, 18, 20, 255));
        r_rect(x + 5, y + 3, 6, 10, RGBA(142, 144, 148, 255));
        r_rect(x + 5, y + 3, 6, 2, RGBA(176, 178, 182, 255));
        r_rect(x + 7, y + 5, 2, 6, RGBA(70, 72, 78, 255));
        r_rect(x + 6, y + 7, 4, 1, RGBA(70, 72, 78, 255));
        return;
    }
    case O_MINE: {   // boca de mina: hueco negro con vigas de madera (poste a cada extremo y un dintel)
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        int l = tx > 0 && g_o[ty][tx - 1] == O_MINE, r = tx + 1 < MW && g_o[ty][tx + 1] == O_MINE;
        r_rect(x, y + 4, 16, 12, RGBA(6, 6, 8, 255));
        r_rect(x, y + 1, 16, 4, RGBA(112, 80, 48, 255));
        r_rect(x, y + 1, 16, 1, RGBA(150, 112, 70, 255));
        r_rect(x, y + 4, 16, 1, RGBA(58, 40, 26, 255));
        if (!l) r_rect(x, y + 1, 3, 15, RGBA(96, 68, 40, 255));
        if (!r) r_rect(x + 13, y + 1, 3, 15, RGBA(96, 68, 40, 255));
        if (l && r) r_rect(x + 7, y + 5, 2, 2, RGBA(24, 22, 24, 255));
        return;
    }
    case O_CART: {   // vagoneta de mina: cajón oxidado con mineral y dos ruedas
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        r_rect(x + 1, y + 5, 14, 8, RGBA(22, 16, 12, 255));
        r_rect(x + 2, y + 6, 12, 6, RGBA(122, 76, 48, 255));
        r_rect(x + 2, y + 6, 12, 1, RGBA(160, 108, 70, 255));
        r_rect(x + 3, y + 3, 4, 3, RGBA(70, 66, 62, 255));
        r_rect(x + 8, y + 4, 4, 2, RGBA(88, 84, 78, 255));
        r_rect(x + 3, y + 12, 3, 3, RGBA(30, 30, 34, 255));
        r_rect(x + 10, y + 12, 3, 3, RGBA(30, 30, 34, 255));
        return;
    }
    case O_SLAG: {   // montón de escoria: piedras negras con destellos azulados
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        u32 h = hash2(tx, ty);
        r_rect(x + 1, y + 9, 14, 6, RGBA(20, 20, 24, 255));
        r_rect(x + 2, y + 6, 11, 5, RGBA(46, 46, 52, 255));
        r_rect(x + 5, y + 4, 6, 3, RGBA(60, 60, 68, 255));
        r_rect(x + 3 + (int)(h % 7), y + 7, 2, 1, RGBA(90, 120, 150, 255));
        r_rect(x + 6 + (int)(h / 7 % 6), y + 11, 2, 1, RGBA(110, 100, 90, 255));
        return;
    }
    case O_STUP: case O_STDN: {   // escaleras de los pisos: la de subir es de madera con peldaños claros; la de bajar, un hueco oscuro
        float x = tx * TS - g_ox, y = ty * TS - g_oy;
        if (o == O_STUP && g_zone == 1 && urbe_mouth_at(tx, ty) >= 0) { urbe_draw_mouth(tx, ty); return; }   // [urbe]
        if (o == O_STUP) {
            r_rect(x + 1, y + 1, 14, 15, RGBA(34, 24, 16, 255));
            for (int k = 0; k < 4; k++) r_rect(x + 2, y + 2 + k * 3, 12, 2, RGBA(110 + k * 22, 82 + k * 18, 50 + k * 12, 255));
            r_rect(x + 1, y + 1, 2, 15, RGBA(86, 60, 36, 255));
            r_rect(x + 13, y + 1, 2, 15, RGBA(86, 60, 36, 255));
        } else {
            r_rect(x + 1, y + 1, 14, 15, RGBA(6, 6, 8, 255));
            for (int k = 0; k < 4; k++) r_rect(x + 3, y + 3 + k * 3, 10, 2, RGBA(40 + k * 14, 36 + k * 12, 32 + k * 10, 255));
            r_rect(x + 1, y + 1, 14, 1, RGBA(120, 116, 108, 255));
        }
        return;
    }
    case O_CAR_R: return;   // ya lo dibujo la casilla de la izquierda
    case O_TREE: {
        s = S_TREE0 + hash2(tx, ty) % 3;   // arbol seco alto, arbol seco chico o arbusto
        float bx = tx * TS + 8, by = (ty + 1) * TS;   // si quedas detras del arbol, se aclara
        if (fabsf(P.x - bx) < 10 && P.y < by - 4 && P.y > by - SPR[s].h - 6) col = RGBA(255, 255, 255, 110);
        break;
    }
    case O_CRATE:
        for (int i = 0; i < g_ncr; i++)
            if (g_cr[i].tx == tx && g_cr[i].ty == ty && g_cr[i].kind == CK_VEND) { dz_draw_vend(tx, ty, g_cr[i].open); return; }   // [diseno]
        for (int i = 0; i < g_ncr; i++)
            if (g_cr[i].tx == tx && g_cr[i].ty == ty)
                s = g_cr[i].kind == CK_JOB || g_cr[i].kind == CK_MEDS || g_cr[i].kind == CK_REG ? (g_cr[i].open ? S_CRATE_JOB_OPEN : S_CRATE_JOB) : (g_cr[i].open ? S_CRATE_OPEN : S_CRATE);
        break;
    }
    int mil = 0;   // [mil]
    if (o == O_CRATE) for (int i = 0; i < g_ncr; i++) if (g_cr[i].tx == tx && g_cr[i].ty == ty && g_cr[i].kind == CK_MIL) { mil = 1; col = g_cr[i].open ? RGBA(110, 130, 80, 255) : RGBA(120, 170, 70, 255); }   // [mil] verde oliva
    s = st_spr(tx, ty, o, s);   // [stalk] casas de pueblo, rejas y cofres
    if (s < 0) return;
    Spr sp = SPR[s];
    r_spr(sp, tx * TS + (TS - sp.w) / 2 - g_ox, (ty + 1) * TS - sp.h - g_oy, col);
    if (mil) { float fx = tx * TS + (TS - sp.w) / 2 - g_ox, fy = (ty + 1) * TS - sp.h - g_oy + sp.h / 2 - 1; r_rect(fx, fy, sp.w, 2, RGBA(225, 235, 170, 255)); r_rect(fx + sp.w / 2 - 1, fy - 3, 2, 3, RGBA(225, 235, 170, 255)); }   // [mil] franja clara y asa
    if (o == O_WALL && g_zone >= 3 && g_zone != ZB && hash2(tx, ty) % 3 == 0 &&
        (((ty == g_fl_y0 || ty == g_fl_y1) && tx > g_fl_x0 && tx < g_fl_x1) || ((tx == g_fl_x0 || tx == g_fl_x1) && ty > g_fl_y0 && ty < g_fl_y1))) {   // ventanas en los muros exteriores del piso: un vidrio azul oscuro con marco y cruz
        float wx = tx * TS - g_ox + 4, wy = (ty + 1) * TS - g_oy - 11;
        r_rect(wx - 1, wy - 1, 10, 7, RGBA(20, 16, 14, 255));
        r_rect(wx, wy, 8, 5, RGBA(60, 92, 128, 255));
        r_rect(wx, wy, 8, 1, RGBA(110, 150, 190, 255));
        r_rect(wx + 3, wy, 1, 5, RGBA(20, 16, 14, 255));
    }
    if (o == O_WALL && g_zone == 1) urbe_fachada(tx, ty, sp.h);   // [urbe]
    if (o == O_WALL && g_zone == 1) {   // [mapas] la casa con segundo piso se ve más alta: ventanas arriba y una cornisa
        for (int k = 0; k < g_nhs; k++) {
            const XBld *R = &x_b[g_hs_bi[k]];
            if (tx < R->x0 || tx > R->x1 || ty < R->y0 || ty > R->y1) continue;
            if (hash2(tx, ty) % 3 != 1 && (((ty == R->y0 || ty == R->y1) && tx > R->x0 && tx < R->x1) || ((tx == R->x0 || tx == R->x1) && ty > R->y0 && ty < R->y1))) {
                float wx = tx * TS - g_ox + 4, wy = (ty + 1) * TS - g_oy - 17;
                r_rect(wx - 1, wy - 1, 10, 6, RGBA(20, 16, 14, 255));   // fila de ventanas del segundo piso
                r_rect(wx, wy, 8, 4, RGBA(70, 100, 132, 255));
                r_rect(wx, wy, 8, 1, RGBA(120, 156, 196, 255));
                r_rect(wx + 3, wy, 1, 4, RGBA(20, 16, 14, 255));
            }
            r_rect(tx * TS - g_ox, (ty + 1) * TS - sp.h - g_oy, TS, 2, RGBA(150, 124, 96, 255));   // la cornisa del segundo piso
            break;
        }
    }
    if (snowy() && wall_like(o)) {   // nieve sobre los techos: una franja blanca con el borde irregular
        float sx = tx * TS + (TS - sp.w) / 2 - g_ox, sy = (ty + 1) * TS - sp.h - g_oy;
        u32 h = hash2(tx, ty);
        r_rect(sx, sy, sp.w, 3, RGBA(228, 238, 250, 240));
        r_rect(sx + (int)(h % 5), sy + 3, 5, 1, RGBA(206, 220, 238, 230));
        r_rect(sx + 8 + (int)(h / 5 % 5), sy + 3, 3, 1, RGBA(206, 220, 238, 230));
    }
}

static float depth_of(int i)   // los muertos ya tendidos van primero: el resto les pasa por encima
{
    if (i == -2) return g_dog.y;
    if (i < 0) return P.y;
    return g_e[i].state == ES_DEAD && !dying(&g_e[i]) ? g_e[i].y - 1000 : g_e[i].y;
}

// ---- el dibujo
static void dog_draw(float x, float y, float ang, float walk, int moving, int lying, u32 tint)
{
    // 4 direcciones por código: cuerpo, cabeza, orejas, cola y patas que se alternan al caminar
    float dx = cosf(ang), dy = sinf(ang);
    int horiz = fabsf(dx) >= fabsf(dy), left = dx < 0, up = dy < 0;
    u32 body = tint == COL_WHITE ? RGBA(96, 80, 66, 255) : tint, dark = RGBA(54, 44, 38, 255), light = RGBA(150, 128, 100, 255);
    int ph = moving ? ((int)(walk * 2) & 1) : 0;
    x = floorf(x); y = floorf(y);
    r_spr(SPR[S_SHADOW], x - 6, y - 1, COL_WHITE);
    if (lying) { r_rect(x - 6, y - 4, 12, 5, body); r_rect(left ? x - 8 : x + 4, y - 5, 4, 4, dark); r_rect(x - 2, y - 5, 3, 1, light); return; }
    if (horiz) {
        float hx = left ? x - 6 : x + 3, tx = left ? x + 5 : x - 8;
        r_rect(x - 5, y - 8, 10, 5, body);                       // cuerpo
        r_rect(x - 5, y - 8, 10, 1, light);                      // lomo
        r_rect(hx, y - 11, 4, 4, body); r_rect(left ? hx : hx + 3, y - 12, 1, 2, dark);   // cabeza y oreja
        r_rect(left ? hx - 1 : hx + 4, y - 9, 1, 2, dark);       // hocico
        r_rect(tx, y - 10 - ph, 3, 1, dark);                     // cola
        r_rect(x - 4 + ph, y - 3, 2, 3, dark); r_rect(x + 2 - ph, y - 3, 2, 3, dark);     // patas
        r_rect(x - 3 - ph, y - 3, 1, 3, dark); r_rect(x + 1 + ph, y - 3, 1, 3, dark);
    } else if (up) {   // de espaldas
        r_rect(x - 3, y - 10, 6, 7, body); r_rect(x - 3, y - 10, 6, 1, light);
        r_rect(x - 2, y - 12, 4, 3, body); r_rect(x - 3, y - 12, 1, 2, dark); r_rect(x + 2, y - 12, 1, 2, dark);
        r_rect(x - 1, y - 4, 2, 2, dark);
        r_rect(x - 3, y - 3 + ph, 2, 3 - ph, dark); r_rect(x + 1, y - 3 + (1 - ph), 2, 2 + ph, dark);
    } else {           // de frente
        r_rect(x - 3, y - 9, 6, 6, body); r_rect(x - 2, y - 7, 4, 3, light);
        r_rect(x - 3, y - 13, 6, 5, body); r_rect(x - 4, y - 13, 1, 3, dark); r_rect(x + 3, y - 13, 1, 3, dark);
        r_rect(x - 2, y - 11, 1, 1, dark); r_rect(x + 1, y - 11, 1, 1, dark); r_rect(x - 1, y - 9, 2, 1, dark);
        r_rect(x - 3, y - 3 + ph, 2, 3 - ph, dark); r_rect(x + 1, y - 3 + (1 - ph), 2, 2 + ph, dark);
    }
}
// El perro en pantalla (en la base, en su rincón; herido, tendido)
static int dog_shown(void) { return g_dog.have && (g_dog.wounded ? g_zone == g_dog.zone : g_dog.home ? g_zone == 0 : g_zone == g_dog.zone); }
static void dog_draw_world(void)
{
    const Dog *d = &g_dog;
    float x = d->x - g_ox, y = d->y - g_oy;
    if (x < -16 || y < -16 || x > SCR_W + 16 || y > SCR_H + 24) return;
    if (d->home && g_zone == 0) r_rect(floorf(x) - 9, floorf(y) - 3, 18, 6, RGBA(110, 70, 50, 255));   // su colchoneta en el rincón
    dog_draw(x, y, d->ang, d->walk, d->moving, d->wounded || d->home, d->flash > 0 ? RGBA(230, 120, 110, 255) : COL_WHITE);
    if (d->wounded && ((int)(g_time * 2) & 1)) r_text_shadow(floorf(x) - 4, floorf(y) - 22, "+", RGBA(230, 90, 80, 255));
}
// Lo de los eventos que se dibuja con el suelo: el perro herido con su cuerpo y el superviviente sentado
static void ev_draw_ground(void)
{
    for (int i = 0; i < g_ncr; i++) {
        const Crate *c = &g_cr[i];
        if (c->kind != CK_DOGW && c->kind != CK_SURV && c->kind != CK_FARO) continue;
        float x = c->tx * TS - g_ox, y = c->ty * TS - g_oy;
        if (x < -24 || y < -24 || x > SCR_W + 24 || y > SCR_H + 24) continue;
        if (c->kind == CK_FARO) {   // el comerciante farolero: de pie junto a su farol
            r_glow(x + 8, y + 6, 46, RGBA(255, 190, 90, 34), RGBA(255, 190, 90, 0));
            r_rect(x + 4, y + 1, 8, 12, RGBA(86, 72, 58, 255)); r_rect(x + 5, y - 5, 6, 6, RGBA(200, 170, 140, 255));
            r_rect(x + 12, y + 4, 3, 4, RGBA(255, 200, 90, 255));
            if ((int)(g_time * 2) & 1) r_text_shadow(x + 5, y - 15, "!", RGBA(240, 210, 90, 255));
            continue;
        }
        if (c->kind == CK_DOGW) {
            if (c->open) continue;
            r_spr(SPR[S_BODY], x - 16, y, COL_WHITE);
            dog_draw(x + 8, y + 12, 0, 0, 0, 1, RGBA(120, 100, 84, 255));
            if ((int)(g_time * 2) & 1) r_text_shadow(x + 5, y - 8, "!", RGBA(240, 210, 90, 255));
        } else if (!c->open) {
            r_spr(SPR[S_BODY], x, y + 2, RGBA(150, 170, 200, 255));
            if ((int)(g_time * 2) & 1) r_text_shadow(x + 5, y - 8, "!", RGBA(240, 210, 90, 255));
        }
    }
}
// La columna de humo del helicóptero caído: se ve de lejos
static void ev_draw_smoke(void)
{
    for (int i = 0; i < g_nev; i++) {
        if (g_ev[i].kind != EV_HELI) continue;
        float hx = g_ev[i].tx * TS + 8 - g_ox, hy = g_ev[i].ty * TS - g_oy;
        if (hx < -30 || hx > SCR_W + 30 || hy < -10 || hy - 200 > SCR_H) continue;
        for (int k = 0; k < 16; k++) {
            float t = k / 16.0f, sw = sinf(g_time * 0.9f + k * 0.7f) * (3 + 10 * t), sz = 5 + 9 * t;
            r_rect(floorf(hx + sw - sz / 2), floorf(hy - 6 - t * 190 + sinf(g_time * 1.7f + k) * 2), sz, sz * 0.8f, RGBA(78, 76, 80, (int)(150 * (1 - t * 0.7f))));
        }
    }
}
static void draw_world(void)
{
    int tx0 = (int)g_ox / TS - 1, ty0 = (int)g_oy / TS - 1, tx1 = tx0 + SCR_W / TS + 2, ty1 = ty0 + SCR_H / TS + 3;
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 >= MW) tx1 = MW - 1;
    if (ty1 >= MH) ty1 = MH - 1;
    int anim = (int)(g_time * 2), sn = snowy();

    // 1. suelo
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            float x = tx * TS - g_ox, y = ty * TS - g_oy;
            u32 h = hash2(tx, ty);
            int s;
            switch (g_g[ty][tx]) {
            case G_GRASS: s = sn ? S_SNOW0 + h % 3 : S_GRASS0 + h % 3; break;
            case G_ROAD: case G_EXIT: case G_ZONE: s = sn ? S_SNOWROAD : h % 6 == 0 ? S_ROAD1 : S_ROAD0; break;
            case G_FLOOR: s = h % 5 == 0 ? S_FLOOR1 : S_FLOOR0; break;
            case G_DOOR: s = S_DOOR; break;
            case G_RAIL: s = S_RAIL; break;
            case G_RAD: s = S_RAD0 + ((anim + h) & 1); break;
            default: s = sn ? S_SNOW0 + h % 3 : S_DIRT0 + h % 3; break;
            }
            { int as = ar_suelo(g_g[ty][tx], h, tx, ty); if (as >= 0) s = as; }   // [arte]
            r_spr(SPR[s], x, y, sn ? COL_WHITE : dz_tinte_suelo());   // [diseno] paleta por tipo
            ar_borde(tx, ty, x, y, g_g[ty][tx]);   // [arte]
            dz_calle(tx, ty, x, y, g_g[ty][tx]);   // [diseno]
            urbe_suelo(tx, ty, x, y);   // [urbe]
            m3_suelo(tx, ty, x, y, g_g[ty][tx]);   // [mapas3]
            if (ar_fab()) {}   // [arte] la fabrica no lleva musgo ni flores
            else if (g_g[ty][tx] == G_ROAD) dz_musgo(tx, ty, x, y);   // [diseno]
            else if (g_g[ty][tx] == G_GRASS || g_g[ty][tx] == G_DIRT) dz_musgo(tx, ty, x, y);   // [diseno]
            if (!sn && !ar_fab() && g_o[ty][tx] == O_NONE) dz_flora(tx, ty, x, y, g_g[ty][tx]);   // [diseno]
            st_suelo(tx, ty, x, y);   // [stalk] objetos de patio
            dz_bolardo(tx, ty, x, y);   // [diseno]
            if (!sn && g_g[ty][tx] == G_ROAD && ty + 1 < MH && g_g[ty + 1][tx] == G_ROAD && tx % 3 == 0)
                r_rect(x + 3, y + 15, 9, 1, RGBA(200, 196, 180, 70));   // linea gastada del camino
            if (g_g[ty][tx] == G_EXIT || g_g[ty][tx] == G_ZONE) r_rect(x, y, TS, TS, RGBA(120, 200, 120, 26 + (int)(16 * sinf(g_time * 3))));
            if (ty > 0 && wall_like(g_o[ty - 1][tx]) && !wall_like(g_o[ty][tx]))
                r_rect(x, y, TS, 6, RGBA(0, 0, 0, 90));                 // sombra al pie del muro
            if (g_o[ty][tx] == O_NONE && !sn) {                         // restos repartidos por la zona (la nieve los tapa)
                static const int CL[6] = {S_STAIN0, S_BONES, S_TRASH0, S_ASH0, S_STAIN1, S_TRASH1};
                u32 h2 = hash2(tx * 7 + 3, ty * 13 + 5) % 100;
                int g = g_g[ty][tx];
                if ((g == G_DIRT || g == G_GRASS || g == G_ROAD) && h2 < 13) r_spr(SPR[CL[h2 % 6]], x, y, COL_WHITE);
                else if (g == G_FLOOR && h2 < 9) r_spr(SPR[h2 & 1 ? S_STAIN0 : S_STAIN1], x, y, COL_WHITE);
            }
        }

    if (sn)   // las huellas en la nieve: dos puntos oscuros que se aclaran hasta borrarse
        for (int i = 0; i < MAXFOOT; i++) {
            const Foot *f = &g_foot[i];
            if (f->t <= 0) continue;
            int a = (int)(120 * clampf(f->t / 8, 0, 1));
            r_rect(floorf(f->x - g_ox) - 1, floorf(f->y - g_oy), 3, 2, RGBA(58, 72, 100, a));
        }

    // 2. lo que esta tirado en el suelo: esqueletos, cuerpos y manchas de sangre
    for (int i = 0; i < g_ndec; i++) r_spr(SPR[g_dec[i].spr], g_dec[i].x - 8 - g_ox, g_dec[i].y - 8 - g_oy, COL_WHITE);
    for (int i = 0; i < g_ncr; i++)
        if (is_body(&g_cr[i]))
            r_spr(SPR[S_BODY], g_cr[i].tx * TS - g_ox, g_cr[i].ty * TS - g_oy, g_cr[i].open ? RGBA(150, 140, 140, 255) : COL_WHITE);
    if (g_zone == 1) ev_draw_ground();
    for (int i = 0; i < g_nd; i++) {
        float x = g_d[i].x - g_ox, y = g_d[i].y - g_oy;
        if (x < -16 || y < -16 || x > SCR_W + 16 || y > SCR_H + 16) continue;
        r_rot(SPR[g_d[i].spr], x, y, g_d[i].ang, g_d[i].scale, COL_WHITE);
    }

    bk_draw_flat();   // [bunker]
    // 3. por filas: primero los objetos de la fila, despues quienes estan parados en ella
    for (int ty = ty0; ty < MH && ty <= ty1 + 4; ty++) {   // +4: arboles y faroles de mas abajo asoman
        for (int tx = tx0 > 0 ? tx0 - 1 : 0; tx < MW && tx <= tx1 + 1; tx++)
            if (g_o[ty][tx] != O_NONE) draw_object(tx, ty);
        float y0 = ty * TS, y1 = y0 + TS;
        for (int i = 0; i < MAXK; i++)
            if (g_k[i].on && g_k[i].y >= y0 && g_k[i].y < y1)
                r_spr(SPR[ITEMS[g_k[i].item].ground], g_k[i].x - 4 - g_ox, g_k[i].y - 6 - g_oy + sinf(g_time * 4 + i) * 1.5f,
                      ITEMS[g_k[i].item].tint);
        int order[MAXE + 2], n = 0;
        for (int i = 0; i < g_ne; i++)
            if (g_e[i].y >= y0 && g_e[i].y < y1) order[n++] = i;
        if (P.y >= y0 && P.y < y1) order[n++] = -1;
        if (dog_shown() && g_dog.y >= y0 && g_dog.y < y1) order[n++] = -2;
        for (int a = 1; a < n; a++) {   // son pocos: orden por insercion, del fondo hacia adelante
            int v = order[a], b = a - 1;
            while (b >= 0 && depth_of(order[b]) > depth_of(v)) { order[b + 1] = order[b]; b--; }
            order[b + 1] = v;
        }
        for (int a = 0; a < n; a++) {
            if (order[a] >= 0) draw_enemy(&g_e[order[a]]);
            else if (order[a] == -2) dog_draw_world();
            else {
                if (g_swing_t > 0 && is_melee(P.weapon)) g_gun_off = angdiff(P.ang, g_swing_a) + (1 - g_swing_t / g_swing_dur - 0.5f) * g_swing_arc * 1.1f;
                draw_person(P.x - g_ox, P.y - g_oy, P.ang, P.moving, P.walk, WDEF[P.weapon].spr,
                            P.hurt_t > 0 ? RGBA(255, 120, 110, 255) : COL_WHITE);
                g_gun_off = 0;
            }
        }
    }

    jefes_draw_world();   // [jefes]
    artef_draw_world();   // [artef]
    ar_over();   // [arte] grua portico
    if (g_zone == 1) ev_draw_smoke();

    // 4. particulas
    for (int i = 0; i < MAXP; i++) {
        Part *p = &g_p[i];
        if (p->life <= 0 || p->col == COL_CASING) continue;   // el casquillo va con lo que brilla
        int a = (int)((p->col >> 24) * clampf(p->life / p->max * 2, 0, 1));
        r_rect(floorf(p->x - g_ox), floorf(p->y - g_oy), p->size, p->size, ALPHA(p->col, a));
    }
}

// La oscuridad: un velo por celdas de 8 px, mas transparente donde llega luz
static void draw_dark(void)
{
    static u32 col[(SCR_H / LG + 3) * (SCR_W / LG + 3)];
    int cols = SCR_W / LG + 1, rows = SCR_H / LG + 1;
    int gx0 = (int)(g_ox / LG), gy0 = (int)(g_oy / LG);
    float amb = OPT_AMBIENT[g_opt[OPT_DARK]] * (g_zone == 2 ? 0.45f : 1) * bk_amb();   // el búnker es más oscuro que afuera
    amb *= duro_amb();   // [duro] sin combustible, penumbra
    amb = dz_amb_luz(amb);   // [diseno] ambiente
    if (g_zone == ZM) amb = 0.10f;   // [urbe] el metro está a oscuras
    float kf = fire_k() / 255.0f * (dz_llueve() && !indoors(P.x, P.y) ? 0.65f : 1), kl = (lamps_dim() ? 0.15f : 1) / 255.0f;
    for (int r = 0; r <= rows; r++)
        for (int c = 0; c <= cols; c++) {
            int vx = gx0 + c, vy = gy0 + r;
            float l = amb + light_at(vx * LG, vy * LG);
            if (vx < LMW && vy < LMH) l += g_lm[0][vy][vx] * kf + g_lm[1][vy][vx] * (1 / 255.0f) + g_lm[2][vy][vx] * kl;
            if (ground_at(vx * LG + 1, vy * LG + 1) == G_RAD) l += 0.28f;   // el charco brilla solo
            if (l > 1) l = 1;
            col[r * (cols + 1) + c] = dz_velo((int)((1 - l) * 246));   // [diseno]
        }
    r_grid(gx0 * LG - g_ox, gy0 * LG - g_oy, cols, rows, LG, col);
    dz_lavado();   // [diseno]
}

// Lo que brilla por si solo va encima de la oscuridad: fuego, faroles, ojos, fogonazos y balas
static void draw_emissive(void)
{
    float tx, ty;
    anom_draw();
    urbe_emissive();   // [urbe]
    ar_emisivo();   // [arte] sodio y balizas
    for (int i = 0; i < g_ne; i++) {   // la marca del golpe sigiloso disponible
        const Enemy *e = &g_e[i];
        if (!stealth_ok(e) || dist(P.x, P.y, e->x, e->y) > 52) continue;
        float x = e->x - g_ox, y = e->y - g_oy - 26 + 1.5f * sinf(g_time * 6);
        r_glow(x, y, 7, RGBA(255, 150, 50, 90), RGBA(255, 120, 40, 0));
        r_text_shadow(x - r_text_w("!") / 2, y - 5, "!", RGBA(255, 190, 90, 255));
    }
    for (int i = 0; i < g_nfire; i++) {   // la llama sale de la boca del barril
        float x = g_fire[i][0] - g_ox, y = g_fire[i][1] - g_oy;
        if (x < -50 || y < -50 || x > SCR_W + 50 || y > SCR_H + 60) continue;
        r_glow(x, y - 10, 34 + 3 * sinf(g_time * 11 + i), RGBA(255, 150, 60, 54), RGBA(255, 110, 40, 0));
        r_spr(SPR[S_FIRE0 + ((int)(g_time * 9) + i) % 3], x - 4, y - 18, COL_WHITE);
        if (((int)(g_time * 20) + i * 7) % 9 == 0)
            part(g_fire[i][0] + frange(-3, 3), g_fire[i][1] - 20, frange(-6, 6), frange(-34, -18), 0.7f, RGBA(255, 170, 70, 255), 1);
    }
    for (int i = 0; i < 12; i++) {   // charcos de fuego (cócteles)
        const Fire *f = &g_fir[i];
        if (f->life <= 0) continue;
        float x = f->x - g_ox, y = f->y - g_oy, k = clampf(f->life / 1.2f, 0, 1);
        if (x < -40 || y < -40 || x > SCR_W + 40 || y > SCR_H + 40) continue;
        r_glow(x, y - 6, 22 + 2 * sinf(g_time * 11 + i), RGBA(255, 150, 60, (int)(80 * k)), RGBA(255, 110, 40, 0));
        r_spr(SPR[S_FIRE0 + ((int)(g_time * 9) + i) % 3], x - 4, y - 16, RGBA(255, 255, 255, (int)(255 * k)));
        if (frand() < 0.08f) part(f->x + frange(-5, 5), f->y - 12, frange(-6, 6), frange(-30, -16), 0.6f, RGBA(255, 170, 70, 255), 1);
    }
    for (int i = 0; i < 4; i++) {   // el destello de una explosión
        const Boom *b = &g_boom[i];
        if (b->t <= 0) continue;
        float k = b->t / 0.5f;
        r_glow(b->x - g_ox, b->y - g_oy - 4, 80 * (1.3f - k * 0.6f), RGBA(255, 210, 130, (int)(190 * k)), RGBA(255, 120, 40, 0));
    }
    for (int i = 0; i < 12; i++) {   // la luz de una mina puesta
        const Trap *t = &g_trp[i];
        if (t->on == 1 && t->kind == H_MINE && ((int)(g_time * 2) & 1)) r_rect(t->x - g_ox - 1, t->y - g_oy - 2, 2, 2, RGBA(255, 60, 40, 255));
    }
    for (int i = 0; i < 6; i++)   // granada con mecha
        if (g_thr[i].on && g_thr[i].kind == H_GREN && ((int)(g_time * 8) & 1)) r_rect(g_thr[i].x - g_ox - 1, g_thr[i].y - g_oy - 6, 2, 2, RGBA(255, 70, 40, 255));
    for (int i = 0; i < g_nlamp; i++) {
        float x = g_lamp[i].x - g_ox, y = g_lamp[i].y - g_oy;
        if (x < -40 || y < -40 || x > SCR_W + 40 || y > SCR_H + 60) continue;
        int on = !(g_lamp[i].flick && lamps_dim());
        if (g_lamp[i].ceil) {
            r_spr(SPR[S_CEIL], x - 3, y - 30, on ? COL_WHITE : RGBA(110, 110, 110, 255));
            r_glow(x, y - 23, 22, RGBA(255, 236, 190, on ? 50 : 8), RGBA(255, 236, 190, 0));
        } else r_glow(x, y - 26, 24, RGBA(255, 236, 190, on ? 56 : 8), RGBA(255, 236, 190, 0));   // el foco esta arriba del poste
    }
    if (g_zone == 0 && g_up[UP_TOWER]) r_glow(3 * TS + 8 - g_ox, 16 * TS - 26 - g_oy, 26, RGBA(255, 214, 140, 50), RGBA(255, 214, 140, 0));   // la cabina de la torre
    for (int i = 0; i < MAXP; i++) {   // las chispas se ven a oscuras
        Part *p = &g_p[i];
        if (p->life <= 0 || (p->col & 0xFF) < 200) continue;
        if (p->col == COL_CASING) {   // el casquillo (o el cartucho de la recortada) salta girando
            r_rot(SPR[WDEF[P.weapon].casing], floorf(p->x - g_ox), floorf(p->y - g_oy), p->life * 40, 1,
                  RGBA(255, 255, 255, (int)(255 * clampf(p->life / p->max * 2, 0, 1))));
            continue;
        }
        r_rect(floorf(p->x - g_ox), floorf(p->y - g_oy), p->size, p->size, ALPHA(p->col, (int)(255 * clampf(p->life / p->max * 2, 0, 1))));
    }
    for (int i = 0; i < g_ne; i++) {
        Enemy *e = &g_e[i];
        if (e->state == ES_DEAD) continue;
        float x = e->x - g_ox, y = e->y - g_oy;
        if (x < -20 || y < -20 || x > SCR_W + 20 || y > SCR_H + 30) continue;
        if (is_human(e)) {   // sin ojos rojos, pero el fogonazo de su arma sí brilla
            if (e->type == E_SNP && e->atk == 1) {   // la línea de mira: tenue mientras te sigue, viva y gruesa cuando ya quedó fija
                float ca = cosf(e->ang), sa = sinf(e->ang), len = 12, fix = e->atk_t < 0.4f;
                while (len < 330 && !blocks_shot((int)floorf((e->x + ca * len) / TS), (int)floorf((e->y + sa * len) / TS))) len += 6;
                r_line(x + ca * 10, y - 6 + sa * 8, x + ca * len, y - 6 + sa * len, fix ? 2.5f : 1.2f, RGBA(255, 40, 30, fix ? 230 : 130));
                r_glow(x + ca * len, y - 6 + sa * len, fix ? 7 : 4, RGBA(255, 60, 40, 160), RGBA(255, 60, 40, 0));
            }
            if (e->muzzle > 0) {
                gun_tip(x, y, e->ang, WDEF[e->type == E_SNP ? W_BOLT : e->var ? W_AK : W_MAKAROV].spr, &tx, &ty);
                r_rot(SPR[S_FLASH], tx + cosf(e->ang) * 4, ty + sinf(e->ang) * 3, e->ang, 0.9f, COL_WHITE);
            }
            continue;
        }
        if (is_boss(e)) {   // La Bestia (aura verde) y Cabeza de Hierro (aura roja): ojos que brillan y el aviso de lo que va a hacer
            int bv = body_view(e->ang), bl = cosf(e->ang) < 0, b2 = e->type == E_BOSS2;
            float bk = e_scale(e);
            r_glow(x, y - 20, 34 + 3 * sinf(g_time * 4), b2 ? RGBA(255, 70, 40, 44) : RGBA(90, 255, 70, 44), b2 ? RGBA(255, 70, 40, 0) : RGBA(90, 255, 70, 0));
            if (e->atk == BA_WIND) {   // línea roja por donde va a embestir
                float k = clampf(1 - e->atk_t, 0, 1);
                r_line(x, y - 4, x + cosf(e->atk_a) * 140, y - 4 + sinf(e->atk_a) * 140, 4 + 6 * k, RGBA(255, 50, 30, (int)(70 + 110 * k)));
            } else if (e->atk == BA_SLAM) {   // círculo en el suelo que se llena antes del golpe
                float k = clampf(1 - e->atk_t / 0.9f, 0, 1);
                r_glow(x, y, 54, RGBA(255, 60, 30, (int)(40 + 120 * k)), RGBA(255, 60, 30, 20));
            } else if (e->atk == BA_CALL) r_glow(x, y - 28, 30, RGBA(130, 255, 90, 110), RGBA(130, 255, 90, 0));
            if (bv != 1) {
                float ey = y - (bv == 0 ? 15 : 14) * bk, ex = floorf(x), px0 = bv == 0 ? ex - 5 : ex + (bl ? -5 : 5);
                for (int k = 0; k < (bv == 0 ? 2 : 1); k++) {
                    float px = bv == 0 ? px0 + k * 10 : px0;
                    r_glow(px, ey, 8, b2 ? RGBA(255, 120, 60, 150) : RGBA(170, 255, 80, 150), b2 ? RGBA(255, 120, 60, 0) : RGBA(170, 255, 80, 0));
                    r_rect(px - 2, ey - 1, 4, 3, b2 ? RGBA(255, 170, 110, 255) : RGBA(200, 255, 110, 255));
                }
            }
            continue;
        }
        // ojos rojos en la oscuridad: dos de frente, uno de perfil, ninguno de espalda
        int view = body_view(e->ang), left = cosf(e->ang) < 0;
        if (view == 1) continue;
        float ex = floorf(x), ey = floorf(y), px[2];
        if (body_class(e->type) == 0) {   // zombi chico (y corredor y lobo)
            ey -= e->type == E_WOLF ? 9 : e->type == E_RUN ? 7 : 8;
            px[0] = view == 0 ? ex - 1 : ex + (left ? -3 : 3);
            px[1] = view == 0 ? ex + 1 : px[0];
        } else {                  // zombi del hacha y zombi grande: mas altos, y al andar el cuadro es otro
            ey -= e->type == E_BIG || e->type == E_ARM ? 2 : 0;
            ey -= view == 0 ? (e->moving ? 13 : 12) : 11;
            px[0] = view == 0 ? ex - (e->moving ? 2 : 1) : ex + (left ? -2 : e->moving ? 2 : 1);
            px[1] = view == 0 ? px[0] + 3 : px[0];
        }
        for (int k = 0; k < (view == 0 ? 2 : 1); k++) {
            r_rect(px[k] - 1, ey - 1, 3, 3, RGBA(255, 40, 30, 46));
            r_rect(px[k], ey, 1, 1, RGBA(255, 96, 64, 245));
        }
        if (e->irr) r_glow(ex, y - 7, 15 + 2 * sinf(g_time * 5 + i), RGBA(110, 255, 90, 60), RGBA(110, 255, 90, 0));   // el halo verde se ve en lo oscuro
    }
    if (P.flash_t > 0) {
        gun_tip(P.x - g_ox, P.y - g_oy, P.ang, WDEF[P.weapon].spr, &tx, &ty);
        Spr fl = SPR[P.flash_t > 0.03f ? S_FLASH : S_FLASH1];   // el fogonazo y su resto
        float fk = WDEF[P.weapon].flash, fd = (fl.w * 0.5f - 3) * fk;   // la recortada escupe mas fuego
        r_rot(fl, tx + cosf(P.ang) * fd, ty + sinf(P.ang) * fd * 0.8f, P.ang, fk, COL_WHITE);
    }
    for (int i = 0; i < 10; i++)   // los charcos de ácido brillan verde y se apagan al final
        if (g_pud[i].life > 0) {
            float pk = clampf(g_pud[i].life / 1.5f, 0, 1), px2 = g_pud[i].x - g_ox, py2 = g_pud[i].y - g_oy;
            if (px2 < -20 || py2 < -20 || px2 > SCR_W + 20 || py2 > SCR_H + 20) continue;
            r_glow(px2, py2, 13, RGBA(150, 255, 60, (int)(95 * pk)), RGBA(110, 220, 40, (int)(30 * pk)));
            r_rect(px2 - 2 + 3 * sinf(g_time * 6 + i), py2 - 1, 2, 2, RGBA(210, 255, 140, (int)(200 * pk)));
        }
    for (int i = 0; i < 12; i++) {   // trampas puestas
        const Trap *t = &g_trp[i];
        if (!t->on) continue;
        Spr sp = SPR[t->kind == H_MINE ? S_IT_MINE : S_IT_TRAP];
        float sx = t->x - g_ox, sy = t->y - g_oy;
        if (sx < -20 || sy < -20 || sx > SCR_W + 20 || sy > SCR_H + 20) continue;
        r_spr(sp, sx - sp.w / 2, sy - sp.h / 2, t->on == 2 ? RGBA(150, 150, 150, 255) : COL_WHITE);
    }
    for (int i = 0; i < 6; i++) {   // lo que vuela: sube y baja en arco
        const Thrown *t = &g_thr[i];
        if (!t->on) continue;
        float fl = t->t > 0 ? sinf(3.14159f * (1 - t->t / t->tt)) * 12 : 0, sx = t->x - g_ox, sy = t->y - g_oy;
        r_rect(sx - 2, sy - 1, 4, 2, RGBA(0, 0, 0, 80));
        r_spr(SPR[S_I_PART], sx - 4, sy - 4 - fl, ITEMS[HAND_ITEM[t->kind]].tint);
    }
    if (g_swing_t > 0 && is_melee(P.weapon)) {   // el trazo del golpe: un arco que barre de un lado al otro
        float k = 1 - g_swing_t / g_swing_dur, rr = WDEF[P.weapon].range;
        float cx = P.x - g_ox, cy = P.y - BODY_H - g_oy;
        for (int i = 0; i < 8; i++) {
            float f = k * (i + 1) / 8, aa = g_swing_a - g_swing_arc * 0.5f + g_swing_arc * f;
            float al = (i + 1) / 8.0f * (1 - k * 0.6f);
            r_line(cx + cosf(aa) * rr * 0.4f, cy + sinf(aa) * rr * 0.4f * 0.8f, cx + cosf(aa) * rr, cy + sinf(aa) * rr * 0.8f, P.weapon == W_MAUL ? 3.5f : 2.2f, RGBA(236, 238, 246, (int)(200 * al)));
        }
    }
    for (int i = 0; i < MAXB; i++)   // las balas vuelan a la altura del pecho
        if (g_b[i].on) {
            float bx = g_b[i].x - g_ox, by = g_b[i].y - BODY_H - g_oy;
            if (g_b[i].kind == K_ACID) { r_glow(bx, by, 7, RGBA(170, 255, 70, 170), RGBA(170, 255, 70, 0)); r_rect(floorf(bx) - 1, floorf(by) - 1, 3, 3, RGBA(220, 255, 150, 255)); }
            else if (g_b[i].mine || g_b[i].fac != F_CREA) r_rot(SPR[S_BULLET], bx, by, atan2f(g_b[i].vy, g_b[i].vx), 1, BDEF[g_b[i].kind].trail);
            else r_spr_f(SPR[S_AXE0 + ((int)(g_time * 18) + i) % 9], floorf(bx) - 7, floorf(by) - 7, g_b[i].vx < 0, COL_WHITE);   // hacha que gira
        }

    // el blanco del apuntado automatico: tenue siempre, vivo al disparar o al mantener L
    if (g_state == GS_PLAY && P.lock >= 0) {
        Enemy *e = &g_e[P.lock];
        int hot = (in.held & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) != 0;
        float mid = body_class(e->type) == 0 ? 5 : e->type == E_BIG || e->type == E_ARM ? 9 : is_boss(e) ? 15 : 7;   // al medio del cuerpo: el zombi chico es mas bajo
        r_rot(SPR[S_RETICLE], e->x - g_ox, e->y - mid - g_oy, 0, hot ? 1 + 0.07f * sinf(g_time * 10) : 1.2f, RGBA(255, 255, 255, hot ? 255 : 120));
    }
    for (int i = 0; i < MAXPOP; i++)
        if (g_pop[i].life > 0) {
            int a = (int)(255 * clampf(g_pop[i].life, 0, 1));
            float w = r_text_w(g_pop[i].s);
            r_text_shadow(clampf(g_pop[i].x - g_ox - w / 2, 2, SCR_W - w - 2), g_pop[i].y - g_oy - 8, g_pop[i].s, ALPHA(g_pop[i].col, a));
        }
}

static void draw_ash(void)
{
    if (g_zone == 2 || g_zone >= 3) return;   // bajo tierra y en los pisos de arriba no cae Ceniza
    int inside = indoors(P.x, P.y), snow = snowy() && !g_storm_on;   // nevada sin tormenta: copos en vez de ceniza suelta
    if (g_storm_on) r_rect(0, 0, SCR_W, SCR_H, RGBA(150, 146, 138, inside ? 40 : 112));
    int n = g_storm_on ? (inside ? 90 : 190) : snow ? (inside ? 30 : 90) : 16;
    float vx = g_storm_on ? 170 : snow ? 9 : 20, vy = g_storm_on ? 64 : snow ? 30 : 13;
    for (int i = 0; i < n; i++) {
        float fx = fmodf(i * 53.7f + g_time * vx * (0.6f + (i % 5) * 0.2f) - g_ox * 0.7f, SCR_W + 16);
        float fy = fmodf(i * 91.3f + g_time * vy * (0.7f + (i % 3) * 0.3f) - g_oy * 0.7f, SCR_H + 16);
        if (fx < 0) fx += SCR_W + 16;
        if (fy < 0) fy += SCR_H + 16;
        if (g_storm_on) r_rect(floorf(fx) - 8, floorf(fy) - 8, 2 + i % 3, 1, RGBA(206, 202, 194, 120 + (i % 4) * 30));
        else if (snow) r_rect(floorf(fx + 3 * sinf(g_time * 1.3f + i)) - 8, floorf(fy) - 8, i % 3 == 0 ? 2 : 1, i % 3 == 0 ? 2 : 1, RGBA(228, 238, 252, 150 + (i % 4) * 25));
        else r_rect(floorf(fx) - 8, floorf(fy) - 8, i % 3 == 0 ? 2 : 1, i % 4 == 0 ? 2 : 1, RGBA(196, 192, 184, 80));
    }
}

static void draw_vignette(void)
{
    u32 k0 = RGBA(0, 0, 0, 0), k1 = RGBA(0, 0, 0, 120);
    r_grad(0, 0, SCR_W, 64, k1, k1, k0, k0);
    r_grad(0, SCR_H - 64, SCR_W, 64, k0, k0, k1, k1);
    r_grad(0, 0, 84, SCR_H, k1, k0, k1, k0);
    r_grad(SCR_W - 84, 0, 84, SCR_H, k0, k1, k0, k1);
    if (P.hurt_t > 0) r_rect(0, 0, SCR_W, SCR_H, RGBA(190, 20, 20, (int)(110 * P.hurt_t / 0.3f)));
    if (P.rad > RAD_DANO) r_rect(0, 0, SCR_W, SCR_H, RGBA(120, 190, 60, (int)(P.rad - RAD_DANO) / 2 + (int)(8 * sinf(g_time * 6))));
    if (P.hp < 35 && g_state == GS_PLAY) {   // malherido: el borde late en rojo
        float beat = fabsf(sinf(g_time * 4.2f));
        u32 r0 = RGBA(150, 0, 0, 0), r1 = RGBA(150, 0, 0, (int)(40 + 120 * beat * beat * beat));
        r_grad(0, 0, SCR_W, 50, r1, r1, r0, r0);
        r_grad(0, SCR_H - 50, SCR_W, 50, r0, r0, r1, r1);
        r_grad(0, 0, 60, SCR_H, r1, r0, r1, r0);
        r_grad(SCR_W - 60, 0, 60, SCR_H, r0, r1, r0, r1);
    }
}

static void bar(float x, float y, float w, float frac, u32 col)
{
    r_rect(x - 1, y - 1, w + 2, 6, RGBA(0, 0, 0, 170));
    r_rect(x, y, w, 4, RGBA(44, 40, 40, 255));
    r_rect(x, y, floorf(w * clampf(frac, 0, 1)), 4, col);
    r_rect(x, y, floorf(w * clampf(frac, 0, 1)), 1, RGBA(255, 255, 255, 60));
}

// Interfaz con las piezas del paquete StalkerLike: casilleros y paneles son planos, asi que se estiran sin deformarse
#define UI_LIGHT RGBA(227, 221, 209, 255)
#define UI_DIM RGBA(132, 142, 150, 255)
#define COL_HUNGER RGBA(206, 150, 70, 255)
#define COL_THIRST RGBA(70, 186, 206, 255)
#define COL_ARMOR RGBA(150, 160, 178, 255)
#define COL_FILTER RGBA(124, 176, 116, 255)
static void ui_fill(int spr, float x, float y, float w, float h, u32 col)
{
    Spr p = SPR[spr];
    p.x += 1; p.y += 1; p.w -= 2; p.h -= 2;   // sin rozar el borde del recorte
    r_spr_s(p, x, y, w, h, col);
}

static void ui_panel(float x, float y, float w, float h, const char *title)
{
    ui_fill(S_UI_EDGE, x, y, w, h, COL_WHITE);
    ui_fill(S_UI_BG, x + 1, y + 1, w - 2, h - 2, COL_WHITE);
    ui_fill(S_UI_EDGE, x, y + 16, w, 1, COL_WHITE);
    r_text(x + 8, y + 2, title, UI_LIGHT);
}

static void ui_mark_k(float x, float y, float w, float h, int k)   // las cuatro esquinas del marcador, k veces su tamaño
{
    for (int i = 0; i < 4; i++) {
        Spr q = SPR[S_UI_MARK];
        q.w = 8; q.h = 8; q.x += (i & 1) * 8; q.y += (i >> 1) * 8;
        r_spr_s(q, (i & 1) ? x + w - 8 * k : x, (i >> 1) ? y + h - 8 * k : y, 8 * k, 8 * k, COL_WHITE);
    }
}
static void ui_mark(float x, float y, float w, float h) { ui_mark_k(x, y, w, h, 2); }   // al doble: casilleros de la mochila

// Una fila de lista: un casillero ancho y bajo; la elegida lleva el marcador chico
static void ui_row(float x, float y, float w, float h, int sel)
{
    ui_fill(S_UI_SLOT, x, y, w, h, COL_WHITE);
    if (sel) ui_mark_k(x, y, w, h, 1);
}

// Barrita al borde de una lista que no cabe entera: dice por dónde va (si cabe todo, no se dibuja).
// top = primera fila a la vista, rows = filas a la vista, total = filas que hay
static void ui_scroll(float x, float y, float h, int top, int rows, int total)
{
    if (total <= rows) return;
    float bh = floorf(h * rows / total);
    r_rect(x, y + floorf((h - bh) * top / (total - rows)), 2, bh, UI_DIM);
}

static void text_right(float x, float y, const char *s, u32 col) { r_text(x - r_text_w(s), y, s, col); }

// El aviso, para las pantallas con paneles (el del HUD queda tapado)
static void ui_msg(float y)
{
    if (g_msg_t <= 0) return;
    int a = (int)(255 * clampf(g_msg_t, 0, 1));
    float mw = r_text_w(g_msg);
    r_rect(SCR_W / 2 - mw / 2 - 6, y, mw + 12, 15, RGBA(0, 0, 0, a * 170 / 255));
    r_text(SCR_W / 2 - mw / 2, y + 2, g_msg, RGBA(236, 230, 210, a));
}

static void dog_hud(void)
{
    const Dog *d = &g_dog;
    if (!d->have) return;
    const char *st = d->home ? "EN CASA" : d->wounded ? "HERIDO" : d->atk ? "ATACA" : d->st == DG_STAY ? "QUIETO" : "SIGUE";
    r_text_shadow(8, 206, "HOLLÍN", RGBA(236, 214, 160, 255));
    r_text_shadow(52, 206, st, d->wounded ? RGBA(240, 100, 90, 255) : RGBA(170, 166, 158, 255));
    bar(8, 219, 70, d->hp / DOG_HP, d->wounded ? RGBA(120, 60, 56, 255) : RGBA(220, 170, 90, 255));
}

static void draw_hud(void)
{
    char s[96];
    const WeaponDef *d = &WDEF[P.weapon];
    WeaponState *w = &g_w[P.weapon];
    peso_hud();   // [peso]

    // La Bestia en combate: barra grande arriba, con su nombre
    for (int i = 0; i < g_ne; i++) {
        const Enemy *b = &g_e[i];
        if (b->mini && b->state == ES_COMBAT) {   // [armas2] minijefe: misma barra, en naranja
            float w = 220, x0 = (SCR_W - w) / 2, hk = clampf(b->hp / b->hpmax, 0, 1);
            r_text_center(SCR_W / 2, 9, MJ[b->mini - 1].name, RGBA(246, 220, 180, 255));
            r_rect(x0 - 2, 23, w + 4, 12, RGBA(0, 0, 0, 190));
            r_rect(x0, 25, w, 8, RGBA(48, 36, 36, 255));
            r_rect(x0, 25, floorf(w * hk), 8, RGBA(222, 140, 50, 255));
            r_rect(x0, 25, floorf(w * hk), 2, RGBA(255, 255, 255, 70));
            break;
        }
        if (!is_boss(b) || b->state != ES_COMBAT) continue;
        float w = 280, x0 = (SCR_W - w) / 2, hk = clampf(b->hp / EHP[b->type], 0, 1);
        int b2 = b->type == E_BOSS2;
        r_text_center(SCR_W / 2, 9, jz_name(b, b2 ? BOSS2_NAME : BOSS_NAME), b2 ? RGBA(240, 206, 196, 255) : RGBA(214, 232, 200, 255));
        r_rect(x0 - 2, 23, w + 4, 12, RGBA(0, 0, 0, 190));
        r_rect(x0, 25, w, 8, RGBA(48, 36, 36, 255));
        r_rect(x0, 25, floorf(w * hk), 8, b2 ? RGBA(204, 84, 60, 255) : RGBA(120, 190, 60, 255));
        r_rect(x0, 25, floorf(w * hk), 2, RGBA(255, 255, 255, 70));
        break;
    }

    dog_hud();

    // abajo a la izquierda: vida, radiacion, aguante
    Spr hf = SPR[S_HP_FILL];   // el relleno se recorta segun la vida; parte despues del recuadro de la cruz
    hf.x += 16;
    hf.w = (short)ceilf((hf.w - 16) * clampf(P.hp / hp_max(), 0, 1));
    r_spr(SPR[S_HP_FRAME], 6, 230, RGBA(214, 208, 198, 255));   // un poco apagados: el rojo y el blanco puros encandilan de noche
    if (hf.w > 0) r_spr(hf, 22, 231, RGBA(216, 204, 204, P.hp < 30 && ((int)(g_time * 4) & 1) ? 130 : 255));
    r_spr(SPR[S_I_RAD], 8, 249, COL_WHITE);
    bar(20, 251, 70, P.rad / 100, RGBA(180, 196, 60, 255));
    r_spr(SPR[S_I_BOLT], 8, 260, COL_WHITE);
    bar(20, 262, 70, P.stam / 100, RGBA(96, 160, 200, 255));
    if (P.bleed > 0 && ((int)(g_time * 3) & 1)) r_spr(SPR[S_I_DROP], 98, 234, COL_WHITE);
    for (int i = 0; i < 2; i++) {   // al lado, mas chicas: hambre y sed; bajo 25 parpadean en rojo
        float v = i ? P.thirst : P.hunger, y = 249 + i * 11;
        int low = v < 25 && ((int)(g_time * 4) & 1);
        r_spr(SPR[i ? S_I_WATER : S_I_FOOD], 98, y, low ? RGBA(255, 255, 255, 90) : COL_WHITE);
        bar(110, y + 2, 44, v / 100, i ? COL_THIRST : COL_HUNGER);
        if (low) r_rect(110, y + 2, 44, 4, RGBA(230, 60, 50, 120));
    }
    for (int k = 0, j = 0; k < EQ_COUNT; k++) {   // discretos, a continuación: el estado de la armadura y lo que le queda al filtro, si van puestos
        int gi = g_eq[k] - 1;
        if (gi < 0) continue;
        if (k >= EQ_DET) { r_spr(SPR[ITEMS[GEAR[gi].item].icon], 212 + 14 * j++, 262, COL_WHITE); continue; }   // detector y artefactos: solo su icono
        float v = g_gcond[gi], y = 249 + k * 11;
        int low = v < 25 && ((int)(g_time * 4) & 1);
        r_spr(SPR[k == EQ_MASK ? S_I_MASK : S_I_VEST], 162, y, low ? RGBA(255, 255, 255, 90) : RGBA(255, 255, 255, 210));
        bar(174, y + 2, 30, v / 100, k == EQ_MASK ? COL_FILTER : COL_ARMOR);
        if (low) r_rect(174, y + 2, 30, 4, RGBA(230, 60, 50, 120));
    }
    if (snowy()) {   // el frío: solo cuando el mapa es nevado
        int hot = g_cold > 60, low = g_cold >= 99.9f && ((int)(g_time * 4) & 1);
        r_text_shadow(210, 247, "FRÍO", low ? RGBA(240, 90, 80, 255) : hot ? RGBA(240, 170, 110, 255) : RGBA(160, 204, 244, 255));
        bar(238, 251, 50, g_cold / 100, hot ? RGBA(240, 150, 90, 255) : RGBA(130, 190, 240, 255));
    }
    if (g_rest && g_state == GS_PLAY)   // aviso discreto, al lado de la vida
        r_text_shadow(98, 232, "Descansas junto al fuego", RGBA(226, 176, 110, 190 + (int)(40 * sinf(g_time * 3))));

    // abajo a la derecha: arma (su icono, al doble, en un casillero)
    Spr wi = SPR[d->icon];
    const AmmoSlot *am = &d->ammo[w->load];   // cargador / lo que llevas de ese tipo, y el tipo: "8/24 PERF."
    const char *tag = BDEF[am->kind].tag;
    ui_fill(S_UI_EDGE, 322, 243, 62, 24, COL_WHITE);
    ui_fill(S_UI_SLOT, 323, 244, 60, 22, COL_WHITE);
    r_spr_s(wi, 353 - wi.w, 255 - wi.h, wi.w * 2, wi.h * 2, P.jammed ? RGBA(255, 140, 130, 255) : COL_WHITE);
    if (is_melee(P.weapon)) {   // sin munición: el estado del arma
        snprintf(s, sizeof(s), "Estado %d%%", (int)(w->cond * 100));
        r_text_shadow(SCR_W - 10 - r_text_w(s), 256, s, w->cond < BAD_COND ? RGBA(240, 150, 90, 255) : UI_LIGHT);
    } else {
        snprintf(s, sizeof(s), "%d/%d", w->mag, g_bag[am->item]);
        float tw = r_text_w(s), tx = SCR_W - 10 - tw - 4 - r_text_w(tag);
        r_text_shadow(tx, 256, s, w->mag == 0 ? RGBA(240, 90, 80, 255) : COL_WHITE);
        r_text_shadow(tx + tw + 4, 256, tag, BDEF[am->kind].col);
        r_spr(SPR[S_I_BULLET], tx - 12, 258, COL_WHITE);
    }
    r_text_shadow(SCR_W - 10 - r_text_w(d->name), 243, d->name, w->cond < BAD_COND ? RGBA(240, 150, 90, 255) : RGBA(214, 204, 180, 255));   // anaranjado: muy gastada, se traba seguido
    {   // el objeto de mano, a la izquierda del arma: icono y cantidad; con L mantenido, su nombre
        int item = HAND_ITEM[g_hand], n = g_bag[item];
        Spr hi = SPR[ITEMS[item].icon];
        ui_fill(S_UI_EDGE, 286, 243, 34, 24, COL_WHITE);
        ui_fill(S_UI_SLOT, 287, 244, 32, 22, COL_WHITE);
        r_spr(hi, 296 - hi.w / 2, 255 - hi.h / 2, n > 0 ? COL_WHITE : RGBA(255, 255, 255, 80));
        snprintf(s, sizeof(s), "%d", n);
        r_text_shadow(310 - r_text_w(s) / 2 + 2, 257, s, n > 0 ? COL_WHITE : RGBA(240, 90, 80, 255));
        if (in.held & PSP_CTRL_LTRIGGER) r_text_shadow(286, 232, ITEMS[item].name, RGBA(236, 214, 150, 255));
    }
    if (P.jammed) r_text_shadow(SCR_W - 10 - r_text_w("TRABADA"), 230, "TRABADA", RGBA(240, 90, 80, 255));
    else if (P.reload_t > 0) r_text_shadow(SCR_W - 10 - r_text_w("recargando"), 230, "recargando", RGBA(200, 200, 200, 255));

    // arriba a la izquierda: el trabajo
    job_text(s, sizeof(s));
    r_text_shadow(8, 6, s, RGBA(214, 204, 180, 255));
    if (sk_points() > 0 && ((int)(g_time * 1.5f) & 1)) r_text_shadow(8, 18, "Punto de habilidad sin gastar (mochila: HABILIDADES)", RGBA(244, 212, 64, 255));

    bomba_hud();   // [bomba] arriba a la derecha: la cuenta de la bomba

    dz_hud();   // [diseno] la hora
    // barra de recarga o de destrabar sobre el jugador
    if (P.reload_t > 0 || P.jam_t > 0) {
        float f = P.reload_t > 0 ? 1 - P.reload_t / d->reload : 1 - P.jam_t / 0.9f;
        r_rect(P.x - g_ox - 9, P.y - g_oy - 29, 18, 3, RGBA(0, 0, 0, 170));
        r_rect(P.x - g_ox - 8, P.y - g_oy - 28, 16 * f, 1, RGBA(230, 220, 190, 255));
    }

    // al calor del fuego la vida vuelve: signos "+" que suben desde el jugador
    if (g_rest == 2 && g_state == GS_PLAY)
        for (int i = 0; i < 3; i++) {
            float k = g_time * 0.7f + i * 0.37f;
            k -= floorf(k);
            float x = floorf(P.x - g_ox) - 8 + i * 8, y = floorf(P.y - g_oy - 12 - k * 18);
            u32 col = RGBA(150, 220, 130, (int)(230 * (1 - k)));
            r_rect(x - 2, y, 5, 1, col);
            r_rect(x, y - 2, 1, 2, col);
            r_rect(x, y + 1, 1, 2, col);
        }

    if (g_ban_t > 0 && g_state == GS_PLAY) {   // el nombre de la zona al llegar
        int a = (int)(255 * clampf(g_ban_t, 0, 1));
        r_text_center(SCR_W / 2, 30, zname(g_zone), RGBA(236, 214, 150, a));
    }
    pozo_hud();   // [pozo]
    artef_hud();   // [artef]

    duro_hud();   // [duro] panel de suministros y barra de cura
    // ayuda para abrir o dormir (se apaga en Opciones)
    Crate *c;
    int qx, qy, what = g_opt[OPT_HELP] ? 0 : near_use(&c, &qx, &qy);
    if (g_opt[OPT_HELP]) {}
    else if (what == 1) r_text_center(c->tx * TS + 8 - g_ox, c->ty * TS - 18 - g_oy, is_body(c) ? "Cruz: registrar" : c->kind == CK_MIL ? "Cruz: abrir caja militar" : "Cruz: abrir", RGBA(236, 230, 210, 255));
    else if (what == 2) r_text_center(qx * TS + 8 - g_ox, qy * TS - 18 - g_oy, "Cruz: dormir", RGBA(236, 230, 210, 255));
    else if (bk_hint()) {}   // [bunker]
    else if (near_spot(&qx, &qy)) {
        static const char *const SPOT_HELP[] = {"", "Cruz: bajar al búnker", "Cruz: subir a la superficie", "Cruz: hablar con el Chofer",
                                                "Cruz: hablar con el comerciante", "Cruz: hablar con el chatarrero", "Cruz: abrir el alijo",
                                                "Cruz: usar la enfermería", "Cruz: subir", "Cruz: bajar"};
        int sp = near_spot(&qx, &qy);
        r_text_center(clampf(qx * TS + 8 - g_ox, 96, SCR_W - 96), qy * TS - 18 - g_oy,
                      sp == 7 && g_clinic_used ? "Enfermería: ya la usaste en esta vuelta" : SPOT_HELP[sp], RGBA(236, 230, 210, 255));
    }
    else if (!g_opt[OPT_HELP] && g_state == GS_PLAY && g_zone == 1 && ground_at(P.x, P.y) == G_ZONE) {   // ayuda del punto de retirada
        float hx = P.x - g_ox;
        r_text_center(hx < 110 ? 110 : hx, P.y - g_oy - 40, "Cruz: volver a Estación Kolos", RGBA(236, 230, 210, 255));
    }
    else if (!g_opt[OPT_HELP] && g_state == GS_PLAY && ground_at(P.x, P.y) == G_EXIT) {   // ayuda del portón
        float hx = P.x - g_ox;
        r_text_center(hx < 96 ? 96 : hx, P.y - g_oy - 40, "Cruz: salir de expedición", RGBA(236, 230, 210, 255));
    }

    if (g_msg_t > 0 && g_state != GS_INV && g_state != GS_KOLOS && g_state != GS_CHOFER) {   // en las pantallas con paneles lo dibuja ui_msg()
        int a = (int)(255 * clampf(g_msg_t, 0, 1));
        float mw = r_text_w(g_msg);
        r_rect(SCR_W / 2 - mw / 2 - 6, 213, mw + 12, 15, RGBA(0, 0, 0, a * 140 / 255));
        r_text(SCR_W / 2 - mw / 2, 215, g_msg, RGBA(236, 230, 210, a));
    }
}

static void panel(float x, float y, float w, float h)
{
    r_rect(x, y, w, h, RGBA(14, 12, 12, 225));
    r_rect(x, y, w, 1, RGBA(150, 60, 50, 255));
    r_rect(x, y + h - 1, w, 1, RGBA(150, 60, 50, 255));
    r_rect(x, y, 1, h, RGBA(150, 60, 50, 255));
    r_rect(x + w - 1, y, 1, h, RGBA(150, 60, 50, 255));
}

enum { T_CONT, T_NEW, T_OPT };
static int title_items(int *ids)
{
    int n = 0;
    if (g_has_save) ids[n++] = T_CONT;
    ids[n++] = T_NEW;
    ids[n++] = T_OPT;
    return n;
}

static void menu_row(float y, const char *t, int on)
{
    ui_row(SCR_W / 2 - 90, y, 180, 18, on);
    r_text_center(SCR_W / 2, y + 3, t, on ? COL_WHITE : UI_LIGHT);
}

static void draw_title(void)
{
    static const char *const NAME[] = {"Continuar", "Partida nueva", "Opciones"};
    r_rect(0, 0, SCR_W, SCR_H, RGBA(8, 6, 6, 150));
    const char *t = "SOINNKA";
    float w = r_text_w(t) * 5;
    r_text_s(SCR_W / 2 - w / 2 + 3, 43, t, RGBA(0, 0, 0, 200), 5);
    r_text_s(SCR_W / 2 - w / 2, 40, t, RGBA(196, 60, 48, 255), 5);
    r_text_center(SCR_W / 2, 104, "tierra de ceniza", RGBA(200, 194, 180, 255));
    if (g_title_page == 0) {
        int ids[3], n = title_items(ids);
        for (int i = 0; i < n; i++) menu_row(142 + i * 22, NAME[ids[i]], i == g_menu_sel);
        r_text_center(SCR_W / 2, 142 + n * 22 + 10, "Arriba/abajo: elegir   Cruz: aceptar", UI_DIM);
    } else {
        r_text_center(SCR_W / 2, 136, "ELIGE EL MODO", RGBA(214, 80, 66, 255));
        for (int i = 0; i < MODE_COUNT; i++) menu_row(152 + i * 20, MODE_NAME[i], i == g_mode_sel);
        r_text_center(SCR_W / 2, 220, MODE_DESC[g_mode_sel][0], UI_LIGHT);
        r_text_center(SCR_W / 2, 233, MODE_DESC[g_mode_sel][1], UI_LIGHT);
    }
    r_text_center(SCR_W / 2, 258, "versión 1.0", RGBA(130, 126, 120, 255));
}

static void draw_options(void)
{
    char v[24];
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
    panel(50, 34, 380, 212);
    r_text_center(SCR_W / 2, 44, "OPCIONES", RGBA(214, 80, 66, 255));
    for (int i = 0; i < OPT_COUNT; i++) {
        float y = 64 + i * 24;
        int on = i == g_opt_sel;
        const char *t = OPT_VAL[i][g_opt[i]];
        if (i == OPT_VOL || i == OPT_MUS) { snprintf(v, sizeof(v), "%d", g_opt[i]); t = v; }
        ui_row(60, y, 360, 20, on);
        r_text(72, y + 4, OPT_LABEL[i], on ? COL_WHITE : UI_LIGHT);
        text_right(408, y + 4, t, on ? COL_WHITE : UI_LIGHT);
        if (on) {
            if (g_opt[i] > 0) r_text(408 - r_text_w(t) - 14, y + 4, "<", UI_DIM);
            if (g_opt[i] < OPT_N[i] - 1) r_text(412, y + 4, ">", UI_DIM);
        }
    }
    r_text_center(SCR_W / 2, 232, "Arriba/abajo: elegir   Izq./der.: cambiar   Círculo: volver", UI_DIM);
}

static void draw_pause(void)
{
    static const char *L[] = {
        "Stick: moverse", "Círculo: correr",
        "L: cambiar de blanco", "R: disparar (apunta solo)",
        "Cuadrado: recargar o destrabar", "Cruz: abrir cajones, dormir",
        "Arriba: curarse", "Abajo: comer, beber, antirrad.",
        "Izquierda/Derecha: cambiar arma", "Triángulo: mochila",
        "SELECT: mapa", "Cuadrado mantenido: otra munición",
    };
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 140));
    panel(40, 44, 400, 176);
    r_text_center(SCR_W / 2, 54, "PAUSA", RGBA(214, 80, 66, 255));
    { char m[48]; snprintf(m, sizeof(m), "Modo: %s", MODE_NAME[g_mode]); r_text(52, 54, m, RGBA(190, 200, 150, 255)); }
    for (int i = 0; i < 12; i++) r_text(58 + (i & 1) * 196, 74 + (i / 2) * 17, L[i], RGBA(220, 214, 200, 255));
    { char m[96]; snprintf(m, sizeof(m), "Expediciones hechas: %d     Mejoras de la base: %d de %d", g_xtotal, ups_done(), UP_N); r_text_center(SCR_W / 2, 178, m, RGBA(236, 214, 150, 255)); }
    r_text_center(SCR_W / 2, 196, "START: seguir     Cruz: opciones", RGBA(170, 166, 158, 255));
}

// El resumen al volver de una expedición: tiempo afuera, bajas y cajones de esa salida, y lo que produjo el huerto
static void draw_return(void)
{
    char s[96];
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
    ui_panel(110, 56, 260, 128, "DE VUELTA EN KOLOS");
    snprintf(s, sizeof(s), "Tiempo afuera: %d:%02d", g_ret_time / 60, g_ret_time % 60);
    r_text(130, 82, s, UI_LIGHT);
    snprintf(s, sizeof(s), "Bajas: %d", g_ret_kills);
    r_text(130, 98, s, UI_LIGHT);
    snprintf(s, sizeof(s), "Cajones abiertos: %d", g_ret_open);
    r_text(130, 114, s, UI_LIGHT);
    if (g_up[UP_GARDEN]) {
        if (g_ret_food || g_ret_water) snprintf(s, sizeof(s), "El huerto dio: %s%s%s en el alijo", g_ret_food ? "1 conserva" : "", g_ret_food && g_ret_water ? " y " : "", g_ret_water ? "1 agua" : "");
        else snprintf(s, sizeof(s), "El huerto dio, pero el alijo está lleno");
        r_text(130, 134, s, RGBA(150, 214, 130, 255));
    }
    duro_ret_draw();   // [duro]
    r_text_center(SCR_W / 2, 166, "Cruz: seguir", RGBA(170, 166, 158, 255));
}

// ------------------------------------------------------------------ mapa
#define MAP_PX (MW > 64 ? 2 : 4)   // [mapas] pixeles por casilla: 4 en las zonas chicas, 2 para que quepa el mapa grande (192 x 120)
static u32 map_color(int tx, int ty)
{
    int o = g_o[ty][tx];
    if (o == O_WALL) return RGBA(104, 98, 94, 255);
    if (o == O_CONT || o == O_CONT2) return RGBA(118, 66, 46, 255);
    if (o == O_TREE) return RGBA(26, 52, 32, 255);
    switch (g_g[ty][tx]) {
    case G_ROAD: return RGBA(66, 64, 66, 255);
    case G_FLOOR: case G_DOOR: return RGBA(132, 112, 84, 255);
    case G_GRASS: return RGBA(54, 76, 44, 255);
    case G_RAD: return RGBA(128, 140, 40, 255);
    case G_RAIL: return RGBA(92, 80, 92, 255);
    case G_EXIT: return RGBA(60, 140, 70, 255);
    case G_ZONE: return RGBA(66, 64, 66, 255);
    }
    return RGBA(78, 62, 46, 255);   // tierra
}

static void map_mark(float x, float y, int size, u32 col)   // un punto con borde negro, centrado
{
    r_rect(x - size / 2 - 1, y - size / 2 - 1, size + 2, size + 2, RGBA(0, 0, 0, 255));
    r_rect(x - size / 2, y - size / 2, size, size, col);
}

// Los marcadores del mapa de SELECT (x0, y0: esquina del mapa)
static void ev_draw_map(float x0, float y0)
{
    if (g_zone != 1) return;
    for (int i = 0; i < g_nev; i++) {
        const Ev *v = &g_ev[i];
        if (!((g_x.evfound >> i) & 1)) continue;
        float px = x0 + v->tx * MAP_PX + MAP_PX / 2, py = y0 + v->ty * MAP_PX + MAP_PX / 2;
        if (v->kind == EV_HORDE) r_line(x0 + v->tx * MAP_PX, py, x0 + v->x2 * MAP_PX, y0 + v->y2 * MAP_PX, 1, RGBA(220, 70, 60, 200));
        u32 col = v->kind == EV_HORDE ? RGBA(230, 70, 60, 255) : v->kind == EV_HELI ? RGBA(230, 150, 60, 255) : v->kind == EV_CARAVAN ? RGBA(120, 190, 240, 255) : RGBA(150, 220, 130, 255);
        map_mark(px, py, 6, col);
        r_text_shadow(px + 5, py - 5, v->kind == EV_HORDE ? "HORDA" : v->kind == EV_HELI ? "HELI" : v->kind == EV_CARAVAN ? "CARAVANA" : "HERIDO", col);
        if (v->kind == EV_SURV && ((g_x.evfound >> (2 + i)) & 1)) map_mark(x0 + v->x2 * MAP_PX + MAP_PX / 2, y0 + v->y2 * MAP_PX + MAP_PX / 2, 5, RGBA(244, 212, 64, 255));
    }
    if (g_fx >= 0 && (g_x.evfound & 32)) {
        float px = x0 + g_fx * MAP_PX + MAP_PX / 2, py = y0 + g_fy * MAP_PX + MAP_PX / 2;
        map_mark(px, py, 6, RGBA(255, 200, 90, 255));
        r_text_shadow(px + 5, py - 5, "FAROL", RGBA(255, 200, 90, 255));
    }
    if (g_dogx >= 0 && (g_x.evfound & 16)) {
        for (int i = 0; i < g_ncr; i++) if (g_cr[i].kind == CK_DOGW && g_cr[i].open) return;
        map_mark(x0 + g_dogx * MAP_PX + MAP_PX / 2, y0 + g_dogy * MAP_PX + MAP_PX / 2, 5, RGBA(240, 220, 150, 255));
    }
}
static void draw_map(void)
{
    int big = MW > 64, mt = big ? 15 : 22;   // el mapa grande deja menos margen arriba y abajo
    float x0 = (SCR_W - MW * MAP_PX) / 2, y0 = (SCR_H - MH * MAP_PX) / 2;
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 140));
    panel(x0 - 6, y0 - mt, MW * MAP_PX + 12, MH * MAP_PX + mt + (big ? 15 : 22));
    if (g_zone == 1 && g_x.danger >= 10) { char t[24]; snprintf(t, sizeof(t), "Peligro %d", g_x.danger); r_text(x0 + MW * MAP_PX - 52, y0 - (big ? 11 : 15), t, RGBA(230, 90, 70, 255)); }   // [pozo]
    { char t[48]; snprintf(t, sizeof(t), "MAPA - %s", zname(g_zone)); r_text_center(SCR_W / 2, y0 - (big ? 11 : 15), t, RGBA(214, 80, 66, 255)); }
    r_rect(x0, y0, MW * MAP_PX, MH * MAP_PX, RGBA(0, 0, 0, 255));   // lo no explorado, negro
    {   // lo explorado va a una imagen de MW x MH píxeles y se dibuja de una vez (con el mapa entero visto, las tiras desbordaban la GE)
        static u32 __attribute__((aligned(16))) img[128 * 256];   // 256 de ancho (potencia de 2) x 128 de alto: cabe 192x120
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) img[y * 256 + x] = g_seen[y][x] ? map_color(x, y) : RGBA(0, 0, 0, 255);
        r_image(img, MW, MH, 256, x0, y0, MAP_PX);
    }
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (g_g[y][x] == G_EXIT || g_g[y][x] == G_ZONE) r_rect(x0 + x * MAP_PX, y0 + y * MAP_PX, MAP_PX, MAP_PX, RGBA(90, 226, 110, 255));
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (g_seen[y][x] && (g_o[y][x] == O_HATCH || g_o[y][x] == O_STAIRS || g_o[y][x] == O_STUP || g_o[y][x] == O_STDN || g_o[y][x] == O_NPC || g_o[y][x] == O_MERCH || g_o[y][x] == O_SCAV || g_o[y][x] == O_CHEST))
                map_mark(x0 + x * MAP_PX + MAP_PX / 2, y0 + y * MAP_PX + MAP_PX / 2, 4, g_o[y][x] >= O_NPC && g_o[y][x] <= O_CHEST ? RGBA(120, 190, 240, 255) : RGBA(236, 150, 60, 255));
    if (!P.has_job && !g_job_done1)
        for (int i = 0; i < g_ncr; i++)
            if (g_cr[i].kind == CK_JOB)
                map_mark(x0 + g_cr[i].tx * MAP_PX + MAP_PX / 2, y0 + g_cr[i].ty * MAP_PX + MAP_PX / 2, 4, RGBA(244, 212, 64, 255));
    ev_draw_map(x0, y0);
    urbe_map_marks(x0, y0, MAP_PX);   // [urbe]
    peso_map_marks(x0, y0);   // [peso]
    for (int i = 0; i < g_ncr; i++)   // [mil] cajas militares ya vistas y sin abrir: punto verde
        if (g_cr[i].kind == CK_MIL && !g_cr[i].open && g_seen[g_cr[i].ty][g_cr[i].tx])
            map_mark(x0 + g_cr[i].tx * MAP_PX + MAP_PX / 2, y0 + g_cr[i].ty * MAP_PX + MAP_PX / 2, 4, RGBA(110, 220, 90, 255));
    if ((int)(g_time * 4) & 1)
        map_mark(x0 + floorf(P.x * MAP_PX / TS), y0 + floorf(P.y * MAP_PX / TS), 4, COL_WHITE);
    r_text_center(SCR_W / 2, y0 + MH * MAP_PX + (big ? 3 : 7), "SELECT: cerrar", RGBA(170, 166, 158, 255));
}

// La mochila muestra solo lo que llevas (los objetos en 0 no ocupan casillero), de a INV_COLS x INV_ROWS casilleros;
// si hay más, corre por filas
#define INV_COLS 4
#define INV_ROWS 2
// Botar: deja a los pies del jugador una unidad del objeto (un paquete, en munición). No se recoge solo hasta que se aleje
static void inv_drop(int item)
{
    char s[64];
    int n = ITEMS[item].pack < g_bag[item] ? ITEMS[item].pack : g_bag[item];
    if (n <= 0) return;
    Pick *k = drop(P.x + frange(-5, 5), P.y + frange(0, 4), item, n);
    if (!k) { msg("No hay lugar en el suelo."); return; }
    k->on = 3;
    inv_take(item, n);
    snprintf(s, sizeof(s), "Botaste %d %s.", n, ITEMS[item].unit);
    msg(s);
}

#define INV_TAB_N 4
static void draw_skills(void);
static const char *const INV_TABS[INV_TAB_N] = {"OBJETOS", "FABRICAR", "ARMAS", "HABILIDADES"};

#include "armas.inc"   // [armas] más modificaciones, botar armas y reparar
#include "armas2.inc"   // [armas2] piezas de las armas nuevas y (más abajo) minijefes y música
#include "artef.inc"   // [artef] artefactos nuevos y piezas únicas

static void inv_input(void)
{
    if (armas_inv_input()) return;   // [armas]
    unsigned char list[ITEM_COUNT];
    int n;
    if (g_inv_tab == 2 && (g_ws_list || g_uq_open) && !(in.pressed & PSP_CTRL_TRIANGLE)) { ws_input(); return; }   // con la lista de piezas abierta, Círculo la cierra
    if (in.pressed & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { g_state = GS_PLAY; return; }
    if (in.pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {   // L y R cambian de pestaña
        g_inv_tab = (g_inv_tab + ((in.pressed & PSP_CTRL_RTRIGGER) ? 1 : INV_TAB_N - 1)) % INV_TAB_N;
        g_ws_w = P.weapon; g_ws_slot = g_ws_list = g_ws_opt = 0;
        return;
    }
    if (g_inv_tab == 3) {   // HABILIDADES
        if (in.pressed & PSP_CTRL_UP) g_sk_sel = (g_sk_sel + SK_N - 1) % SK_N;
        if (in.pressed & PSP_CTRL_DOWN) g_sk_sel = (g_sk_sel + 1) % SK_N;
        if (in.pressed & PSP_CTRL_CROSS) sk_buy();
        return;
    }
    if (g_inv_tab == 1) { g_fab_all = 0; fab_input(); g_fab_all = 1; return; }   // sin banco: solo las recetas sencillas
    if (g_inv_tab == 2) { ws_input(); return; }
    n = inv_list(list, LIST_BAG);
    if ((in.pressed & PSP_CTRL_SQUARE) && n && !g_inv_focus) { inv_drop(list[g_inv_sel]); n = inv_list(list, LIST_BAG); }
    list_keep(&g_inv_sel, &g_inv_top, n, INV_COLS, INV_ROWS);
    if (g_inv_focus) {   // en el panel EQUIPO: izquierda/derecha cambia de ranura, Cruz se quita lo puesto, Arriba vuelve a los casilleros
        if (in.pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) g_inv_eq = (g_inv_eq + 1) % EQ_COUNT;
        if (in.pressed & PSP_CTRL_UP) g_inv_focus = 0;
        if (in.pressed & PSP_CTRL_CROSS) {
            int gi = g_eq[g_inv_eq] - 1;
            if (gi >= 0 && inv_room(GEAR[gi].item) < 1) msg("No te cabe en la mochila.");
            else gear_unequip(g_inv_eq);
        }
        return;
    }
    if (in.pressed & PSP_CTRL_DOWN) {   // desde la última fila, Abajo pasa al panel EQUIPO
        if (!n || g_inv_sel / INV_COLS >= (n - 1) / INV_COLS) g_inv_focus = 1;
        else g_inv_sel = g_inv_sel + INV_COLS < n ? g_inv_sel + INV_COLS : n - 1;
    }
    if (n && !g_inv_focus) {
        if (in.pressed & PSP_CTRL_LEFT) g_inv_sel = (g_inv_sel + n - 1) % n;
        if (in.pressed & PSP_CTRL_RIGHT) g_inv_sel = (g_inv_sel + 1) % n;
        if ((in.pressed & PSP_CTRL_UP) && g_inv_sel >= INV_COLS) g_inv_sel -= INV_COLS;
        if (in.pressed & PSP_CTRL_CROSS) { use_item(list[g_inv_sel]); n = inv_list(list, LIST_BAG); }   // si se acabó, deja de listarse
    }
    list_keep(&g_inv_sel, &g_inv_top, n, INV_COLS, INV_ROWS);
}

static void draw_inv(void)
{
    char s[96];
    unsigned char list[ITEM_COUNT];
    int n = inv_list(list, LIST_BAG), sel = g_inv_sel, top = g_inv_top;
    list_keep(&sel, &top, n, INV_COLS, INV_ROWS);
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));

    // la tira de pestañas, arriba a la derecha (L y R las cambian)
    float tx0 = 398 - r_text_w("L") - r_text_w("R") - 10;   // termina antes del reloj de la Ceniza, arriba a la derecha
    for (int i = 0; i < INV_TAB_N; i++) tx0 -= r_text_w(INV_TABS[i]) + 16;
    {
        float x = tx0;
        r_text(x, 6, "L", UI_DIM);
        x += r_text_w("L") + 4;
        for (int i = 0; i < INV_TAB_N; i++) {
            float w = r_text_w(INV_TABS[i]) + 12;
            if (i == g_inv_tab) ui_fill(S_UI_SLOT, x, 4, w, 16, COL_WHITE);
            r_text(x + 6, 6, INV_TABS[i], i == g_inv_tab ? COL_WHITE : UI_DIM);
            x += w + 4;
        }
        r_text(x + 2, 6, "R", UI_DIM);
    }

    // el trabajo, arriba: el del HUD queda apagado bajo el velo (si no cabe junto a las pestañas, sin la indicación de dónde entregar)
    job_text(s, sizeof(s));
    if (8 + r_text_w(s) > tx0 - 6 && g_job) snprintf(s, sizeof(s), "%s %d/%d", JOBS[g_job - 1].name, job_prog(), JOBS[g_job - 1].goal);
    r_text_shadow(8, 6, s, RGBA(214, 204, 180, 255));

    if (g_inv_tab == 3) { draw_skills(); return; }
    if (g_inv_tab) {   // FABRICAR y ARMAS: las mismas pantallas del chatarrero, bajo un encabezado
        ui_panel(14, 22, 452, 36, "MOCHILA");
        snprintf(s, sizeof(s), "Chatarra %d", g_bag[ITEM_SCRAP]);
        text_right(458, 24, s, UI_LIGHT);
        r_text(24, 42, g_inv_tab == 1 ? "Recetas sencillas. La munición se fabrica con el chatarrero." : "Instala y quita piezas de tus armas, donde estés.", UI_DIM);
        if (g_inv_tab == 1) { g_fab_all = 0; fab_draw(); g_fab_all = 1; }
        else ws_draw();
        r_text_center(SCR_W / 2, 231, g_inv_tab == 1 ? "Flechas: elegir   Cruz: fabricar   L/R: pestaña   Triángulo: cerrar" :
                      g_ws_list ? "Arriba/abajo: elegir   Cruz: confirmar   Círculo: volver" :
                      "Izq./der.: arma  Arr./abajo: ranura  Cruz: piezas  Cuad.: botar  Triáng.: reparar", RGBA(170, 166, 158, 255));
        ui_msg(246);
        return;
    }

    // objetos: un casillero por cosa, con su icono (al doble), la cantidad y el nombre debajo.
    // Las esquinas marcan el elegido (se mueve con las flechas y Cruz lo usa o se lo pone); arriba dice para qué sirve
    peso_draw_bag();   // [peso]
    ui_panel(14, 22, 246, 146, "MOCHILA");   // entre el texto de arriba y las barras de abajo del HUD
    {
        const char *use = n ? ITEMS[list[sel]].use : "No llevas nada";
        if (g_inv_focus) use = g_eq[g_inv_eq] ? ITEMS[GEAR[g_eq[g_inv_eq] - 1].item].use : "Nada puesto en esa ranura";
        r_text(252 - r_text_w(use), 24, use, UI_DIM);
    }
    for (int k = 0; k < INV_COLS * INV_ROWS; k++) {
        int i = top * INV_COLS + k;
        float x = 24 + (k % INV_COLS) * 58, y = 44 + (k / INV_COLS) * 60;
        ui_fill(S_UI_SLOT, x, y, 52, 44, COL_WHITE);
        if (i >= n) continue;   // casillero vacío
        const ItemDef *d = &ITEMS[list[i]];
        Spr ic = SPR[d->icon];
        r_spr_s(ic, x + 26 - ic.w, y + 15 - ic.h, ic.w * 2, ic.h * 2, COL_WHITE);
        if (d->cap && item_cap(list[i])) snprintf(s, sizeof(s), "%d/%d", g_bag[list[i]], item_cap(list[i]));   // [peso] sin tope por objeto: "xN"
        else snprintf(s, sizeof(s), "x%d", g_bag[list[i]]);
        r_text(x + 49 - r_text_w(s), y + 30, s, UI_LIGHT);
        r_text_center(x + 26, y + 45, d->name, i == sel && !g_inv_focus ? COL_WHITE : UI_LIGHT);
        if (i == sel && !g_inv_focus) ui_mark(x, y, 52, 44);
    }
    ui_scroll(255, 44, 104, top, INV_ROWS, (n + INV_COLS - 1) / INV_COLS);

    // armas: un casillero ancho por cada una que tienes, con su icono (al doble), estado y cargador; las esquinas
    // marcan la que va en mano
    ui_panel(264, 22, 202, 146, "ARMAS");
    for (int k = 0; k < 3; k++) ui_fill(S_UI_SLOT, 272, 43 + k * 41, 186, 39, COL_WHITE);
    int skip = 0;   // con más de tres armas, la lista arranca para que la de la mano se vea
    { int pos = 0; for (int i = 0; i < P.weapon; i++) pos += g_w[i].owned; skip = pos > 2 ? pos - 2 : 0; }
    for (int i = 0, k = 0, seen = 0; i < W_COUNT && k < 3; i++) {
        if (!g_w[i].owned || seen++ < skip) continue;
        float x = 272, y = 43 + k++ * 41;
        Spr ic = SPR[WDEF[i].icon];
        int cond = (int)(g_w[i].cond * 100);
        r_spr_s(ic, x + 34 - ic.w, y + 19 - ic.h, ic.w * 2, ic.h * 2, COL_WHITE);
        r_text(x + 68, y + 2, WDEF[i].name, UI_LIGHT);
        snprintf(s, sizeof(s), "Estado %d%%", cond);
        r_text(x + 68, y + 13, s, cond < 30 ? RGBA(240, 90, 80, 255) : UI_DIM);
        if (is_melee(i)) r_text(x + 68, y + 24, "Cuerpo a cuerpo", UI_DIM);
        else {
            snprintf(s, sizeof(s), "Cargador %d/%d %s", g_w[i].mag, w_mag(i), BDEF[WDEF[i].ammo[g_w[i].load].kind].tag);
            r_text(x + 68, y + 24, s, g_w[i].mag ? UI_DIM : RGBA(240, 90, 80, 255));
        }
        if (i == P.weapon) {
            ui_mark(x + 1, y + 1, 184, 37);
            r_text(x + 180 - r_text_w("en mano"), y + 2, "en mano", RGBA(190, 200, 150, 255));
        }
    }

    // equipo: lo puesto en cada ranura (armadura y máscara), con su icono al doble, su nombre y su estado (el de la
    // máscara es lo que le queda al filtro). Abajo desde la última fila de casilleros trae el cursor aquí; Cruz lo quita
    ui_panel(14, 172, 246, 56, "EQUIPO");
    for (int k = 0; k < EQ_COUNT; k++) {
        static const char *const EL[EQ_COUNT] = {"Armad.", "Másc.", "Detec.", "Artef.", "Artef.", "Rango 3"};
        float x = 18 + k * 40, y = 190;
        int gi = g_eq[k] - 1;
        ui_fill(S_UI_SLOT, x, y, 38, 34, COL_WHITE);
        if (gi < 0) r_text(x + 19 - r_text_w(EL[k]) / 2, y + 12, EL[k], UI_DIM);
        else {
            Spr ic = SPR[ITEMS[GEAR[gi].item].icon];
            float v = g_gcond[gi];
            r_spr_s(ic, x + 19 - ic.w, y + (k < 2 ? 2 : 7) - (k < 2 ? 0 : 0), ic.w * 2, ic.h * 2, COL_WHITE);
            if (k < 2) bar(x + 3, y + 27, 32, v / 100, k == EQ_MASK ? COL_FILTER : COL_ARMOR);
        }
        if (g_inv_focus && k == g_inv_eq) ui_mark(x, y, 38, 34);
    }
    if (g_inv_focus && g_eq[g_inv_eq] > 0) {   // el nombre de lo elegido (y su estado, si lo tiene), en la franja del título
        int gi = g_eq[g_inv_eq] - 1;
        if (g_inv_eq < 2) snprintf(s, sizeof(s), "%s %d%%", GEAR[gi].name, (int)ceilf(g_gcond[gi])); else snprintf(s, sizeof(s), "%s", GEAR[gi].name);
        text_right(256, 174, s, UI_DIM);
    }

    // hambre y sed, con su numero; en la franja del título, el rango en Estación Kolos
    ui_panel(264, 172, 202, 56, "ESTADO");
    rank_text(s, sizeof(s));
    text_right(458, 174, s, RGBA(190, 200, 150, 255));
    for (int i = 0; i < 2; i++) {
        float v = i ? P.thirst : P.hunger, y = 193 + i * 16;
        r_spr(SPR[i ? S_I_WATER : S_I_FOOD], 272, y + 2, COL_WHITE);
        r_text(284, y, i ? "Sed" : "Hambre", UI_LIGHT);
        bar(330, y + 4, 96, v / 100, i ? COL_THIRST : COL_HUNGER);
        snprintf(s, sizeof(s), "%d", (int)ceilf(v));
        r_text(458 - r_text_w(s), y, s, v < 25 ? RGBA(240, 90, 80, 255) : UI_DIM);
    }

    if (!artef_info(g_inv_focus ? (g_eq[g_inv_eq] ? GEAR[g_eq[g_inv_eq] - 1].item : -1) : n ? list[sel] : -1))   // [artef] ventaja (verde) y costo (rojo) del artefacto
    r_text_center(SCR_W / 2, 231, g_inv_focus ? "Izq./der.: elegir   Cruz: quitar   Arriba: casilleros   L/R: pestaña" :
                  "Cruz: usar   Cuadrado: botar   L/R: pestaña   Triángulo: cerrar", RGBA(170, 166, 158, 255));
    ui_msg(246);   // lo que paso al usar algo
}

// Dormir: la pantalla se oscurece, queda negra un momento y vuelve
#define SLEEP_FADE 0.9f
#define SLEEP_DARK 0.9f
static void draw_sleep(void)
{
    float t = g_state_t, k = t < SLEEP_FADE ? t / SLEEP_FADE : t < SLEEP_FADE + SLEEP_DARK ? 1 : 1 - (t - SLEEP_FADE - SLEEP_DARK) / SLEEP_FADE;
    k = clampf(k, 0, 1);
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, (int)(255 * k)));
    if (k > 0.6f) r_text_center(SCR_W / 2, 128, "Duermes...", RGBA(170, 166, 158, (int)(255 * (k - 0.6f) / 0.4f)));
}

// Cambio de zona: velo negro que sube y baja, y el nombre de la zona de destino al centro
// El final: lo que dice el registro, las estadísticas de la partida y FIN. Después se puede seguir jugando
// Pantallas nuevas: el puesto farolero (GS_FARO), HABILIDADES y DIARIO (pestañas de la mochila) y la elección del final (GS_ENDSEL)
static void draw_skills(void)
{
    char s[128];
    ui_panel(14, 22, 452, 200, "HABILIDADES");
    snprintf(s, sizeof(s), "Puntos sin gastar: %d", sk_points());
    text_right(458, 24, s, sk_points() > 0 ? RGBA(244, 212, 64, 255) : UI_LIGHT);
    for (int i = 0; i < SK_N; i++) {
        float y = 44 + i * 14;
        if (i == g_sk_sel) ui_fill(S_UI_SLOT, 20, y - 2, 440, 14, COL_WHITE);
        r_text(28, y, SK_NAME[i], g_sk[i] ? RGBA(150, 220, 130, 255) : UI_LIGHT);
        text_right(452, y, g_sk[i] ? "aprendida" : "libre", g_sk[i] ? RGBA(150, 220, 130, 255) : UI_DIM);
    }
    r_text(28, 176, SK_DESC[g_sk_sel], RGBA(214, 204, 180, 255));
    r_text(28, 194, "Un punto por cada rango nuevo y otro cada 3 expediciones (6 en total).", UI_DIM);
    r_text_center(SCR_W / 2, 231, "Arriba/abajo: elegir   Cruz: aprender   L/R: pestaña   Triángulo: cerrar", RGBA(170, 166, 158, 255));
    ui_msg(246);
}
static void draw_faro(void)
{
    static const char *const TABS[3] = {"COMPRAR", "VENDER", "TRABAJOS"};
    char s[128];
    int l[FSHOP_N];
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
    ui_panel(14, 22, 452, 200, "PUESTO FAROLERO");
    float x = 236;
    for (int i = 0; i < 3; i++) {
        float w = r_text_w(TABS[i]) + 12;
        if (i == g_faro_tab) ui_fill(S_UI_SLOT, x, 4, w, 16, COL_WHITE);
        r_text(x + 6, 6, TABS[i], i == g_faro_tab ? COL_WHITE : UI_DIM);
        x += w + 4;
    }
    snprintf(s, sizeof(s), "Chatarra %d   Reputación farolera %d", g_bag[ITEM_SCRAP], g_frep);
    text_right(458, 24, s, UI_LIGHT);
    if (g_faro_tab == 0) {
        int n = fshop_list(l);
        for (int k = 0; k < n; k++) {
            const FShop *d = &FSHOP[l[k]];
            float y = 44 + k * 16;
            int lock = g_frep < d->frep;
            if (k == g_faro_sel) ui_fill(S_UI_SLOT, 20, y - 2, 440, 15, COL_WHITE);
            r_text(28, y, l[k] == FSHOP_N - 1 ? ITEMS[fshop_item(l[k])].name : d->name, lock ? UI_DIM : UI_LIGHT);
            if (lock) snprintf(s, sizeof(s), "pide reputación %d", d->frep); else snprintf(s, sizeof(s), "%d", d->price);
            text_right(452, y, s, lock ? UI_DIM : RGBA(236, 214, 150, 255));
        }
        r_text(28, 190, "Venden lo que Kolos no tiene. Más reputación, más artículos.", UI_DIM);
    } else if (g_faro_tab == 1) {
        int n = fart_list(l);
        for (int k = 0; k < n; k++) {
            float y = 44 + k * 16;
            if (k == g_faro_sel) ui_fill(S_UI_SLOT, 20, y - 2, 440, 15, COL_WHITE);
            r_text(28, y, ITEMS[ART_ITEM(l[k])].name, UI_LIGHT);
            snprintf(s, sizeof(s), "%d", fart_pay(l[k]));
            text_right(452, y, s, RGBA(236, 214, 150, 255));
        }
        if (!n) r_text(28, 44, "No llevas artefactos.", UI_DIM);
        r_text(28, 190, "Pagan los artefactos mejor que Kolos.", UI_DIM);
    } else {
        for (int j = 0; j < 4; j++) {   // [jefes2]
            float y = 44 + j * 30;
            if (j == g_faro_sel) ui_fill(S_UI_SLOT, 20, y - 2, 440, 28, COL_WHITE);
            r_text(28, y, FJ_NAME[j], (g_fdone >> j) & 1 ? RGBA(150, 220, 130, 255) : UI_LIGHT);
            r_text(28, y + 12, FJ_DESC[j], UI_DIM);
            if ((g_fdone >> j) & 1) snprintf(s, sizeof(s), "hecho");
            else if (g_fjob == j + 1) snprintf(s, sizeof(s), fjob_ready() ? "listo: entrega" : "en curso (Cuadrado: cancelar)");
            else snprintf(s, sizeof(s), "%d chatarra, +%d rep.", FJ_PAY[j], FJ_REP[j]);
            text_right(452, y, s, RGBA(236, 214, 150, 255));
        }
    }
    r_text_center(SCR_W / 2, 231, "Arriba/abajo: elegir   Cruz: aceptar   L/R: pestaña   Círculo: salir", RGBA(170, 166, 158, 255));
    ui_msg(246);
}
// El final: se elige entre los que tengas disponibles
static int g_end_sel;
static int end_ok(int k) { return k == 0 || (k == 1 && g_frep >= 3) || k == 2; }
static void endsel_input(void)
{
    if (in.pressed & PSP_CTRL_UP) g_end_sel = (g_end_sel + 2) % 3;
    if (in.pressed & PSP_CTRL_DOWN) g_end_sel = (g_end_sel + 1) % 3;
    if (!(in.pressed & PSP_CTRL_CROSS)) return;
    if (!end_ok(g_end_sel)) { msg(g_end_sel == 1 ? "Los Faroleros aún no confían en ti: pide 3 de reputación farolera." : "No conoces lo bastante: pide haber leído 10 notas."); return; }
    g_ending = g_end_sel; g_fin_seen = 1; g_state = GS_FIN; g_state_t = 0;
}
static void draw_endsel(void)
{
    static const char *const T[3] = {"Entregar el registro a Kolos", "Entregar el registro a los Faroleros", "Destruir el registro tú mismo"};
    static const char *const D[3] = {"Siempre disponible.", "Pide 3 de reputación farolera.", "Siempre disponible."};
    char s[96];
    r_rect(0, 0, SCR_W, SCR_H, RGBA(6, 6, 8, 232));
    r_text_center(SCR_W / 2, 26, "REGISTRO DEL INCENDIO", RGBA(214, 80, 66, 255));
    r_text_center(SCR_W / 2, 48, "Tienes el registro en las manos. ¿Qué haces con él?", RGBA(220, 214, 200, 255));
    for (int k = 0; k < 3; k++) {
        float y = 84 + k * 40;
        int ok = end_ok(k);
        if (k == g_end_sel) ui_fill(S_UI_SLOT, 60, y - 6, 360, 34, COL_WHITE);
        r_text_center(SCR_W / 2, y, T[k], ok ? UI_LIGHT : UI_DIM);
        r_text_center(SCR_W / 2, y + 13, D[k], ok ? RGBA(150, 220, 130, 255) : UI_DIM);
    }
    snprintf(s, sizeof(s), "Reputación farolera %d", g_frep);
    r_text_center(SCR_W / 2, 214, s, UI_DIM);
    r_text_center(SCR_W / 2, 231, "Arriba/abajo: elegir   Cruz: decidir", RGBA(170, 166, 158, 255));
    ui_msg(246);
}

static void draw_fin(void)
{
    char s[96];
    float a = clampf(g_state_t * 1.2f, 0, 1);
    int al = (int)(255 * a), t_s = (int)g_play_t;
    u32 txt = RGBA(220, 214, 200, al), dim = RGBA(170, 166, 158, al);
    r_rect(0, 0, SCR_W, SCR_H, RGBA(6, 6, 8, (int)(232 * a)));
    r_text_center(SCR_W / 2, 26, "REGISTRO DEL INCENDIO", RGBA(214, 80, 66, al));
    r_text_center(SCR_W / 2, 48, "Bajo la cuenca, el incendio arde desde hace años y no se va a apagar.", txt);
    r_text_center(SCR_W / 2, 62, "La Ceniza no es una tormenta que pasa: sube desde abajo, y seguirá subiendo.", txt);
    if (g_ending == 1) {   // entregado a los Faroleros
        r_text_center(SCR_W / 2, 76, "Los Faroleros copian el registro y lo reparten, puesto a puesto.", txt);
        r_text_center(SCR_W / 2, 90, "Kolos pierde su secreto; los caminos ganan una luz más cada noche.", txt);
    } else if (g_ending == 2) {   // destruido
        r_text_center(SCR_W / 2, 76, "Quemas el registro en un barril de la galería. Nadie sabrá quién selló.", txt);
        r_text_center(SCR_W / 2, 90, "Cada uno sabrá solo lo que alcance a ver. Quizá baste.", txt);
    } else {
        r_text_center(SCR_W / 2, 76, "Kolos tendrá que aprender a vivir con la Ceniza.", txt);
        r_text_center(SCR_W / 2, 90, "Guardará el registro y racionará los filtros como mejor pueda.", txt);
    }
    snprintf(s, sizeof(s), "Tiempo de juego  %d:%02d", t_s / 60, t_s % 60);
    r_text_center(SCR_W / 2, 110, s, dim);
    snprintf(s, sizeof(s), "Bajas  %d", g_kills);
    r_text_center(SCR_W / 2, 124, s, dim);
    snprintf(s, sizeof(s), "Trabajos cumplidos  %d", g_jobs_done);
    r_text_center(SCR_W / 2, 138, s, dim);
    snprintf(s, sizeof(s), "Chatarra ganada  %d", g_scrap_gain);
    r_text_center(SCR_W / 2, 152, s, dim);
    float w = r_text_w("FIN") * 3;
    r_text_s(SCR_W / 2 - w / 2 + 2, 178, "FIN", RGBA(0, 0, 0, al), 3);
    r_text_s(SCR_W / 2 - w / 2, 176, "FIN", RGBA(236, 214, 150, al), 3);
    if (g_state_t > 1.5f && ((int)(g_time * 2) & 1)) r_text_center(SCR_W / 2, 226, "Cruz: seguir jugando", COL_WHITE);
}


static void draw_zone_fade(void)
{
    float t = g_state_t, k = t < ZFADE ? t / ZFADE : t < ZFADE + ZHOLD ? 1 : 1 - (t - ZFADE - ZHOLD) / ZFADE;
    k = clampf(k, 0, 1);
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, (int)(255 * k)));
    if (k > 0.5f) {
        char buf[80];
        const char *n = zname(g_zto);   // zname ya agrega ", nevado"
        int a = (int)(255 * (k - 0.5f) * 2);
        if (g_zto == 1 && g_zexp) { snprintf(buf, sizeof(buf), "Expedición %d - %s", g_x.num, n); n = buf; }
        r_text_s(SCR_W / 2 - r_text_w(n), 120, n, RGBA(236, 214, 150, a), 2);
        if (g_zto == 1 && g_zexp && (g_x.flags & XF_BEAST)) r_text_center(SCR_W / 2, 146, "Algo grande ronda el charco", RGBA(214, 120, 100, a));
    }
}

// Portón: toma el trabajo elegido, o lo entrega si ya está cumplido
static int job_pay(const JobDef *j) { return g_mode == MODE_EXPL ? j->pay * 5 / 4 : j->pay; }   // Explorador: 25 % más

static void kolos_pick(int i)
{
    const JobDef *j = &JOBS[i];
    char s[96];
    if (g_job == i + 1) {
        if (!job_ready()) { msg("Ese trabajo aún no está cumplido."); return; }
        g_bag[ITEM_SCRAP] += job_pay(j);
        g_rep += j->rep;
        if (i == 0) { P.has_job = 0; g_job_done1 = 1; }   // los repuestos se entregan una sola vez
        if (i == 4) g_job_done5 = 1;                      // y la Bestia se caza una sola vez
        if (i == 5) { g_has_meds = 0; g_job_done6 = 1; }  // las medicinas, una sola vez
        if (i == 6) { g_has_reg = 0; g_job_done7 = 1; g_fin_seen = 1; }   // el registro: lleva al final
        if (i >= 8) g_j2done |= 1 << (i - 8);   // [jefes2] el jefe cobrado no vuelve
        g_scrap_gain += job_pay(j);
        g_jobs_done++;
        g_job = 0;
        g_job_n = 0;
        snprintf(s, sizeof(s), "Trabajo entregado: +%d chatarra, +%d reputación.", job_pay(j), j->rep);
        snd_play(SFX_JOB_DONE, 0.9f, 0);
        msg(s);
        save_game();
        if (i == 6) { g_state = GS_ENDSEL; g_state_t = 0; g_end_sel = 0; }   // el final se elige
    } else if (g_job) msg("Ya tienes un trabajo. Entrégalo primero.");
    else if (i == 0 && g_job_done1) msg("Los repuestos ya se entregaron.");
    else if (i == 4 && g_job_done5) msg("La Bestia ya está cazada.");
    else if (i == 5 && g_job_done6) msg("Las medicinas ya se entregaron.");
    else if (i == 6 && g_job_done7) msg("Ya entregaste el registro.");
    else if (i >= 8 && ((g_j2done >> (i - 8)) & 1)) msg("Ese jefe ya cayó.");   // [jefes2]
    else if ((i == 8 && rank() < 2) || (i == 9 && rank() < 3)) { snprintf(s, sizeof(s), "Aún no te confían ese trabajo: pide rango %d y tienes rango %d.", i == 8 ? 2 : 3, rank()); msg(s); }   // [jefes2]
    else if (i == 6 && rank() < 3) {
        snprintf(s, sizeof(s), "Aún no te confían ese trabajo: pide rango 3 y tienes rango %d.", rank());
        msg(s);
    }
    else {
        g_job = i + 1;
        g_job_n = 0;
        snprintf(s, sizeof(s), "Trabajo tomado: %s.", j->name);
        msg(s);
    }
}

// Lo de Estación Kolos va por pestañas (cada personaje de la base abre las suyas), que se cambian con L y R. Cada una tiene sus botones y su
// dibujo (de y = 60 a 228). Para sumar otra, se agrega una fila a KTAB.
typedef struct {
    const char *name, *help;   // el nombre en la tira y la ayuda de botones al pie (corta: a la izquierda está la vida)
    void (*input)(void);       // sus botones (L, R y Círculo los atiende la pantalla)
    void (*draw)(void);
    int (*modal)(void);        // opcional: si devuelve 1, la pestaña tiene una lista abierta y atiende ella todos los botones
    int who;                   // con quién se abre (KW_): el comerciante, el chatarrero o el arcón
} KolosTab;

#define JOB_ROWS 5   // trabajos a la vista; si hay más, la lista corre
static void jobs_input(void)
{
    if (in.pressed & PSP_CTRL_UP) g_kol_sel = (g_kol_sel + JOB_N - 1) % JOB_N;
    if (in.pressed & PSP_CTRL_DOWN) g_kol_sel = (g_kol_sel + 1) % JOB_N;
    if (g_kol_sel < g_kol_top) g_kol_top = g_kol_sel;
    if (g_kol_sel >= g_kol_top + JOB_ROWS) g_kol_top = g_kol_sel - JOB_ROWS + 1;
    if (in.pressed & PSP_CTRL_CROSS) kolos_pick(g_kol_sel);
    if ((in.pressed & PSP_CTRL_SQUARE) && g_job == g_kol_sel + 1) {   // cancelar el trabajo en curso (cuesta 1 de reputación)
        if (job_ready()) { msg("Ese trabajo ya está cumplido: entrégalo con Cruz."); return; }
        g_job = 0; g_job_n = 0;
        if (g_rep > 0) g_rep--;
        msg("Trabajo cancelado: -1 de reputación. Puedes volver a tomarlo.");
    }
}

static void jobs_draw(void)
{
    char s[96];
    ui_panel(14, 60, 452, 168, "TRABAJOS");
    for (int r = 0; r < JOB_ROWS && g_kol_top + r < JOB_N; r++) {
        int i = g_kol_top + r;
        const JobDef *j = &JOBS[i];
        float y = 76 + r * 30;
        int act = g_job == i + 1, sel = i == g_kol_sel;
        ui_fill(S_UI_SLOT, 22, y, 428, 28, COL_WHITE);
        if (sel) ui_mark(22, y, 428, 28);
        r_text(34, y + 3, j->name, sel ? COL_WHITE : UI_LIGHT);
        snprintf(s, sizeof(s), "Paga %d de chatarra y %d de reputación", job_pay(j), j->rep);
        r_text(438 - r_text_w(s), y + 3, s, RGBA(190, 200, 150, 255));
        r_text(34, y + 15, j->desc, UI_DIM);
        if (act && job_ready()) snprintf(s, sizeof(s), "Listo para entregar");
        else if (act) snprintf(s, sizeof(s), "En curso %d/%d   Cuadrado: cancelar", job_prog(), j->goal);
        else if ((i == 0 && g_job_done1) || (i == 4 && g_job_done5) || (i == 5 && g_job_done6) || (i == 6 && g_job_done7)) snprintf(s, sizeof(s), "Hecho");
        else if (i == 6 && rank() < 3) snprintf(s, sizeof(s), "Pide rango 3");
        else snprintf(s, sizeof(s), "Disponible");
        r_text(438 - r_text_w(s), y + 15, s, act ? RGBA(150, 220, 130, 255) : UI_DIM);
    }
    ui_scroll(458, 76, 148, g_kol_top, JOB_ROWS, JOB_N);
}

// Comerciante: Cruz compra una unidad del artículo elegido y Cuadrado vende una. Siempre avisa qué pasó
static void shop_buy(int i)
{
    const ShopDef *d = &SHOP[i];
    char s[96];
    int s0 = g_bag[ITEM_SCRAP], gi = d->gun ? -1 : gear_of(d->item);   // una prenda (armadura o máscara): viene nueva y solo se puede tener una
    if (rank() < d->rank) snprintf(s, sizeof(s), "Aún no te lo venden: pide rango %d y tienes rango %d.", d->rank, rank());
    else if (d->gun && g_w[d->gun - 1].owned) snprintf(s, sizeof(s), "Ya tienes esa arma.");
    else if (gi >= 0 && gear_owned(gi)) snprintf(s, sizeof(s), "Ya tienes: %s. Solo puedes tener una de cada prenda.", GEAR[gi].name);
    else if (!g_stock[i]) snprintf(s, sizeof(s), "No quedan. El comerciante repone cuando pasa la Ceniza.");
    else if (d->gun && g_bag[ITEM_SCRAP] >= d->price) {   // el arma viene nueva y con el cargador lleno
        g_bag[ITEM_SCRAP] -= d->price;
        g_stock[i]--;
        g_w[d->gun - 1] = (WeaponState){1, WDEF[d->gun - 1].mag, 1.0f};
        snprintf(s, sizeof(s), "Compraste: %s (-%d). Cambia de arma con izquierda o derecha.", d->name, d->price);
    } else if (d->gun) snprintf(s, sizeof(s), "Te falta chatarra: cuesta %d y tienes %d.", d->price, g_bag[ITEM_SCRAP]);
    else if (inv_room(d->item) < d->qty)
        snprintf(s, sizeof(s), "No te cabe: vas muy cargado. Guarda algo en el alijo.");   // [peso]
    else if (g_bag[ITEM_SCRAP] < d->price) snprintf(s, sizeof(s), "Te falta chatarra: cuesta %d y tienes %d.", d->price, g_bag[ITEM_SCRAP]);
    else {
        g_bag[ITEM_SCRAP] -= d->price;
        g_stock[i]--;
        inv_add(d->item, d->qty);
        if (gi >= 0) g_gcond[gi] = 100;
        snprintf(s, sizeof(s), gi >= 0 ? "Compraste: %s (-%d). Póntela desde la mochila." : "Compraste: %s (-%d de chatarra).", d->name, d->price);
    }
    if (g_bag[ITEM_SCRAP] < s0) snd_play(SFX_TRADE, 0.8f, 0);
    msg(s);
}

static void shop_sell(int i)
{
    const ShopDef *d = &SHOP[i];
    char s[96];
    int s0 = g_bag[ITEM_SCRAP], n = inv_count(d->item), pay = shop_sell_price(d), gi = d->gun ? -1 : gear_of(d->item);
    if (d->gun) snprintf(s, sizeof(s), "Las armas no se venden.");
    else if (n < d->qty && gi >= 0 && gear_worn(gi)) snprintf(s, sizeof(s), "La llevas puesta: quítatela desde la mochila para venderla.");
    else if (n < d->qty) {
        if (d->qty > 1 && n > 0) snprintf(s, sizeof(s), "Se vende de a %d y solo tienes %d.", d->qty, n);
        else snprintf(s, sizeof(s), "No tienes de eso para vender.");
    } else {
        inv_take(d->item, d->qty);
        g_bag[ITEM_SCRAP] += pay;
        g_scrap_gain += pay;
        snprintf(s, sizeof(s), "Vendiste: %s (+%d de chatarra).", d->name, pay);
    }
    if (g_bag[ITEM_SCRAP] > s0) snd_play(SFX_TRADE, 0.8f, 0);
    msg(s);
}

#define SHOP_ROWS 7   // filas a la vista; si hay más artículos, la lista corre
static void shop_input(void)
{
    if (in.pressed & PSP_CTRL_UP) g_shop_sel = (g_shop_sel + SHOP_N - 1) % SHOP_N;
    if (in.pressed & PSP_CTRL_DOWN) g_shop_sel = (g_shop_sel + 1) % SHOP_N;
    if (g_shop_sel < g_shop_top) g_shop_top = g_shop_sel;
    if (g_shop_sel >= g_shop_top + SHOP_ROWS) g_shop_top = g_shop_sel - SHOP_ROWS + 1;
    if (in.pressed & PSP_CTRL_CROSS) shop_buy(g_shop_sel);
    else if (in.pressed & PSP_CTRL_SQUARE) shop_sell(g_shop_sel);
}

static void shop_draw(void)
{
    static const u32 RED = RGBA(240, 90, 80, 255);
    char s[96];
    ui_panel(14, 60, 452, 168, "COMERCIANTE");
    text_right(270, 62, "Llevas", UI_DIM);
    text_right(330, 62, "Compra", UI_DIM);
    text_right(386, 62, "Venta", UI_DIM);
    text_right(446, 62, "Quedan", UI_DIM);
    for (int r = 0; r < SHOP_ROWS && g_shop_top + r < SHOP_N; r++) {
        int i = g_shop_top + r, sel = i == g_shop_sel;
        const ShopDef *d = &SHOP[i];
        int locked = rank() < d->rank, gun = d->gun != 0, mine = gun && g_w[d->gun - 1].owned;
        int have = gun ? mine : inv_count(d->item), cap = gun ? 1 : item_cap(d->item);
        int full = !gun && inv_room(d->item) < d->qty;
        float y = 80 + r * 20;
        u32 col = locked ? UI_DIM : sel ? COL_WHITE : UI_LIGHT;
        Spr ic = SPR[d->spr];
        ui_row(22, y, 436, 18, sel);
        if (ic.w > 20) {   // un arma: su icono es ancho y el nombre se corre
            r_spr(ic, 28, y + 9 - ic.h / 2, locked ? RGBA(255, 255, 255, 90) : COL_WHITE);
            r_text(28 + ic.w + 6, y + 3, d->name, col);
        } else {
            r_spr(ic, 36 - ic.w / 2, y + 9 - ic.h / 2, locked ? RGBA(255, 255, 255, 90) : COL_WHITE);
            r_text(52, y + 3, d->name, col);
        }
        if (!gun && !have && gear_of(d->item) >= 0 && gear_owned(gear_of(d->item)))   // una prenda que no va en la mochila
            snprintf(s, sizeof(s), g_stash[d->item] ? "en alijo" : "puesta");
        else if (cap) snprintf(s, sizeof(s), "%d/%d", have, cap);
        else snprintf(s, sizeof(s), "%d", have);
        text_right(270, y + 3, s, locked || !have ? UI_DIM : full ? RED : UI_LIGHT);
        snprintf(s, sizeof(s), "%d", d->price);
        text_right(330, y + 3, s, locked ? UI_DIM : g_bag[ITEM_SCRAP] < d->price ? RED : RGBA(236, 214, 150, 255));
        if (gun) snprintf(s, sizeof(s), "-");
        else snprintf(s, sizeof(s), "%d", shop_sell_price(d));
        text_right(386, y + 3, s, locked ? UI_DIM : RGBA(190, 200, 150, 255));
        if (locked) snprintf(s, sizeof(s), "rango %d", d->rank);
        else if (mine) snprintf(s, sizeof(s), "-");
        else if (!g_stock[i]) snprintf(s, sizeof(s), "agotado");
        else snprintf(s, sizeof(s), "%d", g_stock[i]);
        text_right(446, y + 3, s, locked || mine ? UI_DIM : g_stock[i] ? UI_LIGHT : RED);
    }
    ui_scroll(461, 80, 138, g_shop_top, SHOP_ROWS, SHOP_N);
}

// Alijo: dos columnas (mochila y alijo) con las mismas filas: los objetos que hay en alguna de las dos (lo que no
// tienes en ninguna no se lista; la chatarra no se guarda). Si no caben, la lista corre.
// Cruz pasa un paquete de la columna elegida a la otra (la munición va de a 8 o de a 4)
#define STASH_ROWS 7
static void stash_move(int item, int out)   // out: 0 = de la mochila al alijo, 1 = del alijo a la mochila
{
    char s[96];
    int *bag = &g_bag[item], *box = &g_stash[item], n = ITEMS[item].pack;
    if (!out) {
        if (n > *bag) n = *bag;
        if (!n) { msg("No llevas de eso en la mochila."); return; }
        int libre = stash_cap() - (stash_used() - stash_slots(item, *box));   // lo que queda de tope para este objeto
        while (n > 0 && stash_slots(item, *box + n) > libre) n--;
        if (!n) {
            snprintf(s, sizeof(s), "El alijo está lleno: %d/%d.", stash_used(), stash_cap());
            msg(s);
            return;
        }
        *bag -= n; *box += n;
        snprintf(s, sizeof(s), "Guardaste en el alijo: %d %s.", n, n == 1 ? ITEMS[item].unit : ITEMS[item].many);
    } else {
        int room = inv_room(item);
        if (!*box) { msg("No hay de eso en el alijo."); return; }
        if (n > *box) n = *box;
        if (n > room) n = room;
        if (!n) {
            snprintf(s, sizeof(s), "No te cabe: vas muy cargado (sobre 140 %% de carga).");   // [peso]
            msg(s);
            return;
        }
        *box -= n; *bag += n;
        snprintf(s, sizeof(s), "Sacaste del alijo: %d %s.", n, n == 1 ? ITEMS[item].unit : ITEMS[item].many);
    }
    msg(s);
}

static void stash_input(void)
{
    unsigned char list[ITEM_COUNT];
    int n = inv_list(list, LIST_BAG | LIST_STASH);
    if (in.pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) g_stash_col ^= 1;
    list_keep(&g_stash_sel, &g_stash_top, n, 1, STASH_ROWS);
    if (!n) return;
    if (in.pressed & PSP_CTRL_UP) g_stash_sel = (g_stash_sel + n - 1) % n;
    if (in.pressed & PSP_CTRL_DOWN) g_stash_sel = (g_stash_sel + 1) % n;
    list_keep(&g_stash_sel, &g_stash_top, n, 1, STASH_ROWS);
    if (in.pressed & PSP_CTRL_CROSS) stash_move(list[g_stash_sel], g_stash_col);
}

static void stash_draw(void)
{
    char s[96];
    unsigned char list[ITEM_COUNT];
    int n = inv_list(list, LIST_BAG | LIST_STASH), sel = g_stash_sel, top = g_stash_top;
    list_keep(&sel, &top, n, 1, STASH_ROWS);
    for (int c = 0; c < 2; c++) {
        float x0 = c ? 242 : 14;
        ui_panel(x0, 60, 224, 168, c ? "ALIJO" : "MOCHILA");
        if (c) {
            snprintf(s, sizeof(s), "%d/%d", stash_used(), stash_cap());
            r_text(x0 + 56, 62, s, stash_used() >= stash_cap() ? RGBA(240, 90, 80, 255) : UI_LIGHT);
        }
        if (c == g_stash_col) text_right(x0 + 216, 62, c ? "Cruz: sacar" : "Cruz: guardar", RGBA(190, 200, 150, 255));
        if (!n) r_text(x0 + 12, 84, c ? "El alijo está vacío." : "No llevas nada que guardar.", UI_DIM);
        for (int r = 0; r < STASH_ROWS && top + r < n; r++) {
            int i = top + r, item = list[i], cnt = c ? g_stash[item] : g_bag[item], on = c == g_stash_col && i == sel;
            float y = 80 + r * 20;
            Spr ic = SPR[ITEMS[item].icon];
            ui_row(x0 + 8, y, 208, 18, on);
            r_spr(ic, x0 + 22 - ic.w / 2, y + 9 - ic.h / 2, cnt ? COL_WHITE : RGBA(255, 255, 255, 90));
            r_text(x0 + 38, y + 3, ITEMS[item].name, on ? COL_WHITE : cnt ? UI_LIGHT : UI_DIM);
            if (c || !item_cap(item)) snprintf(s, sizeof(s), "%d", cnt);
            else snprintf(s, sizeof(s), "%d/%d", cnt, item_cap(item));
            text_right(x0 + 206, y + 3, s, cnt ? UI_LIGHT : UI_DIM);
        }
    }
    ui_scroll(461, 80, 138, top, STASH_ROWS, n);
}

// Taller: las piezas de cada arma. Izquierda/derecha elige el arma (de las que tienes) y arriba/abajo la ranura;
// Cruz abre la lista de piezas de la mochila que calzan ahí, más "quitar". Al instalar, la pieza sale de la mochila;
// al quitarla (o al reemplazarla), vuelve. A la derecha, los valores del arma y lo que cambiaría la opción elegida
static int ws_modal(void) { return g_ws_list || g_uq_open; }   // [artef]

// El arma admite alguna pieza en esa ranura
static int ws_fits(int wi, int slot)
{
    for (int i = 0; i < PARTS_N; i++)
        if (PARTS[i].slot == slot && (PARTS[i].fits & FITS(wi))) return 1;
    return 0;
}
// Las opciones de una ranura: las piezas de la mochila que calzan (su lugar en PARTS + 1) y, si hay una puesta,
// 0 = quitarla. Devuelve cuántas son
static int ws_options(int wi, int slot, unsigned char *out)
{
    int n = 0;
    for (int i = 0; i < PARTS_N; i++)
        if (PARTS[i].slot == slot && (PARTS[i].fits & FITS(wi)) && g_bag[PARTS[i].item] > 0 && g_w[wi].part[slot] != i + 1)
            out[n++] = i + 1;
    if (g_w[wi].part[slot]) out[n++] = 0;
    return n;
}
// Pone la pieza `pi` (su lugar en PARTS + 1) en la ranura, o la deja vacía con pi = 0. La que estaba vuelve a la mochila
static void part_set(int wi, int slot, int pi)
{
    WeaponState *w = &g_w[wi];
    char s[96];
    int old = w->part[slot], m;
    if (pi == old || (pi && g_bag[PARTS[pi - 1].item] <= 0)) return;
    if (pi) inv_take(PARTS[pi - 1].item, 1);
    if (old && !inv_add(PARTS[old - 1].item, 1)) {   // la que sale no cabe: no se cambia nada
        if (pi) g_bag[PARTS[pi - 1].item]++;
        snprintf(s, sizeof(s), "No te cabe en la mochila: llevas %d/%d. Guarda algo en el alijo.", inv_count(PARTS[old - 1].item),
                 ITEMS[PARTS[old - 1].item].cap);
        msg(s);
        return;
    }
    w->part[slot] = pi;
    m = w_mag(wi);
    if (w->mag > m) {   // el cargador quedó más chico: las balas que sobran vuelven a la mochila (o al suelo)
        int item = WDEF[wi].ammo[w->load].item, back = inv_add(item, w->mag - m);
        if (back < w->mag - m) spill(item, w->mag - m - back);
        w->mag = m;
    }
    if (pi) snprintf(s, sizeof(s), "Instalaste: %s (%s).", PARTS[pi - 1].name, WDEF[wi].name);
    else snprintf(s, sizeof(s), "Quitaste: %s. Vuelve a la mochila.", PARTS[old - 1].name);
    msg(s);
}

static void ws_input(void)
{
    unsigned char opt[PARTS_N + 1];
    if (artef_ws_input()) return;   // [artef] lista de piezas únicas
    if (armas_scav_input()) return;   // [armas]
    if (!g_w[g_ws_w].owned) g_ws_w = P.weapon;
    if (g_ws_list) {   // la lista de piezas: Cruz instala o quita, Círculo vuelve a las ranuras
        int n = ws_options(g_ws_w, g_ws_slot, opt);
        if (!n || (in.pressed & PSP_CTRL_CIRCLE)) { g_ws_list = 0; return; }
        if (g_ws_opt >= n) g_ws_opt = n - 1;
        if (in.pressed & PSP_CTRL_UP) g_ws_opt = (g_ws_opt + n - 1) % n;
        if (in.pressed & PSP_CTRL_DOWN) g_ws_opt = (g_ws_opt + 1) % n;
        if (in.pressed & PSP_CTRL_CROSS) { part_set(g_ws_w, g_ws_slot, opt[g_ws_opt]); g_ws_list = 0; }
        return;
    }
    if (in.pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {   // recorre las armas que tienes
        int step = (in.pressed & PSP_CTRL_RIGHT) ? 1 : W_COUNT - 1;
        do g_ws_w = (g_ws_w + step) % W_COUNT; while (!g_w[g_ws_w].owned);
    }
    if (in.pressed & PSP_CTRL_UP) g_ws_slot = (g_ws_slot + SLOT_COUNT - 1) % SLOT_COUNT;
    if (in.pressed & PSP_CTRL_DOWN) g_ws_slot = (g_ws_slot + 1) % SLOT_COUNT;
    if (in.pressed & PSP_CTRL_CROSS) {
        if (!ws_fits(g_ws_w, g_ws_slot)) msg("Esta arma no admite piezas en esa ranura.");
        else if (!ws_options(g_ws_w, g_ws_slot, opt)) msg("No llevas piezas para esa ranura. Se compran aquí o salen en cajones.");
        else { g_ws_list = 1; g_ws_opt = 0; }
    }
}

// Las barras del taller: nombre, valor con que la barra se llena y si conviene que sea bajo (el ruido)
typedef struct { const char *name; float max; int low; } StatDef;
enum { ST_DMG, ST_RATE, ST_ACC, ST_RANGE, ST_MAG, ST_NOISE, ST_COND, ST_COUNT };
static const StatDef STATS[ST_COUNT] = {
    [ST_DMG] = {"Daño", 80}, [ST_RATE] = {"Cadencia", 10}, [ST_ACC] = {"Precisión", 100}, [ST_RANGE] = {"Alcance", 40},
    [ST_MAG] = {"Cargador", 30}, [ST_NOISE] = {"Ruido", 32, 1}, [ST_COND] = {"Estado", 100},
};
static float stat_val(const WeaponStats *s, float cond, int k)
{
    switch (k) {
    case ST_DMG: return s->dmg * s->pellets;   // por tiro, con todos sus proyectiles
    case ST_RATE: return 1 / s->rate;          // tiros por segundo
    case ST_ACC:                               // la dispersión media entre quieto, apuntando y en movimiento: 0,25 rad = 0
        return 100 * (1 - clampf(s->spread * (1 + s->aim + s->move) / 3 / 0.25f, 0, 1));
    case ST_RANGE: return s->range / 10;
    case ST_MAG: return s->mag;
    case ST_NOISE: return s->noise / 10;
    }
    return (int)(cond * 100);   // sin decimales, como se lee en la mochila
}
static void stat_text(char *s, int n, float v, int k)
{
    int t = (int)(v * 10 + 0.5f);
    if (k == ST_RATE) snprintf(s, n, "%d,%d", t / 10, t % 10);
    else snprintf(s, n, k == ST_COND ? "%d%%" : "%d", (int)(v + 0.5f));
}

static void ws_draw(void)
{
    static const u32 GREEN = RGBA(120, 214, 110, 255), RED = RGBA(240, 90, 80, 255), OLIVE = RGBA(190, 200, 150, 255);
    char s[96], s2[24];
    unsigned char opt[PARTS_N + 1], next[SLOT_COUNT];
    int wi = g_w[g_ws_w].owned ? g_ws_w : P.weapon, owned = 0, fitted = 0;
    const WeaponDef *d = &WDEF[wi];
    const WeaponState *w = &g_w[wi];
    int n = g_ws_list ? ws_options(wi, g_ws_slot, opt) : 0, sel = g_ws_opt < n ? g_ws_opt : n - 1;
    int cond = (int)(w->cond * 100);
    Spr ic = SPR[d->icon];
    for (int i = 0; i < W_COUNT; i++) owned += g_w[i].owned;
    for (int k = 0; k < SLOT_COUNT; k++) fitted += w->part[k] != 0;

    // izquierda: el arma (icono al doble) y sus cuatro ranuras, o la lista de piezas de la ranura abierta
    ui_panel(14, 60, 196, 168, "TALLER");
    ui_fill(S_UI_SLOT, 22, 80, 180, 40, COL_WHITE);
    r_spr_s(ic, 66 - ic.w, 100 - ic.h, ic.w * 2, ic.h * 2, COL_WHITE);
    r_text(108, 86, d->name, COL_WHITE);
    snprintf(s, sizeof(s), "Estado %d%%", cond);
    r_text(108, 100, s, cond < 30 ? RED : UI_DIM);
    if (owned > 1 && !n) { r_text(26, 93, "<", UI_LIGHT); r_text(192, 93, ">", UI_LIGHT); }
    if (!n) {
        for (int k = 0; k < SLOT_COUNT; k++) {
            float y = 122 + k * 17;   // [armas] cinco ranuras
            int pi = w->part[k], ok = ws_fits(wi, k), on = k == g_ws_slot;
            ui_row(22, y, 180, 16, on);
            r_text(30, y + 2, SLOT_NAME[k], on ? COL_WHITE : ok ? UI_LIGHT : UI_DIM);
            text_right(196, y + 2, pi ? PARTS[pi - 1].name : ok ? "vacía" : "no admite", pi ? artef_pcol(pi - 1, OLIVE) : UI_DIM);
        }
        snprintf(s, sizeof(s), "Piezas puestas: %d", fitted);
        r_text(30, 209, s, UI_DIM);
        armas_ws_extra(wi);   // [armas]
    } else {
        snprintf(s, sizeof(s), "%s: piezas que llevas", SLOT_NAME[g_ws_slot]);
        r_text(30, 127, s, OLIVE);
        for (int j = 0; j < n; j++) {
            float y = 144 + j * 20;
            ui_row(22, y, 180, 18, j == sel);
            if (opt[j]) {
                Spr pc = SPR[ITEMS[PARTS[opt[j] - 1].item].icon];
                r_spr(pc, 36 - pc.w / 2, y + 9 - pc.h / 2, COL_WHITE);
                r_text(52, y + 3, PARTS[opt[j] - 1].name, artef_pcol(opt[j] - 1, j == sel ? COL_WHITE : UI_LIGHT));
            } else {
                snprintf(s, sizeof(s), "Quitar: %s", PARTS[w->part[g_ws_slot] - 1].name);
                r_text(30, y + 3, s, j == sel ? COL_WHITE : UI_LIGHT);
            }
        }
    }

    // derecha: los valores del arma con sus piezas. Con la lista abierta, lo que cambiaría la opción elegida:
    // el tramo que gana o pierde la barra y "antes > después", en verde si mejora y en rojo si empeora
    ui_panel(214, 60, 252, 168, "ESTADÍSTICAS");
    WeaponStats cur = weapon_stats(wi, w->part, w->load), nxt = cur;
    if (n) {
        memcpy(next, w->part, sizeof(next));
        next[g_ws_slot] = opt[sel];
        nxt = weapon_stats(wi, next, w->load);
    }
    for (int k = 0; k < ST_COUNT; k++) {
        float y = 81 + k * 15, a = stat_val(&cur, w->cond, k), b = stat_val(&nxt, w->cond, k);
        float fa = clampf(a / STATS[k].max, 0, 1), fb = clampf(b / STATS[k].max, 0, 1), lo = fa < fb ? fa : fb, hi = fa < fb ? fb : fa;
        stat_text(s, sizeof(s), a, k);
        stat_text(s2, sizeof(s2), b, k);
        int diff = n && strcmp(s, s2) != 0;
        u32 dc = (b > a) != STATS[k].low ? GREEN : RED;
        r_text(224, y, STATS[k].name, UI_LIGHT);
        bar(292, y + 4, 96, diff ? lo : fa, RGBA(196, 190, 176, 255));
        if (diff) {
            float x0 = 292 + floorf(96 * lo), x1 = 292 + floorf(96 * hi);
            if (x1 < x0 + 2) x1 = x0 + 2;   // que se vea aunque el cambio sea chico
            r_rect(x0, y + 4, x1 - x0, 4, dc);
            snprintf(s + strlen(s), sizeof(s) - strlen(s), " > %s", s2);
        }
        text_right(458, y, s, diff ? dc : UI_DIM);
    }
    ui_fill(S_UI_EDGE, 222, 188, 236, 1, COL_WHITE);
    {   // abajo: qué hace la pieza elegida (o la puesta en la ranura)
        int pi = n ? opt[sel] : w->part[g_ws_slot];
        const PartDef *p = pi ? &PARTS[pi - 1] : NULL;
        if (n && !pi) {
            snprintf(s, sizeof(s), "Quitar: %s", PARTS[w->part[g_ws_slot] - 1].name);
            r_text(224, 191, s, COL_WHITE);
            r_text(224, 203, "La pieza vuelve a la mochila.", UI_DIM);
        } else if (p) {
            snprintf(s, sizeof(s), n ? "Instalar: %s" : "Puesta: %s", p->name);
            r_text(224, 191, s, artef_pcol(pi - 1, n ? COL_WHITE : OLIVE));
            r_text(224, 203, p->desc[0], UI_DIM);
            r_text(224, 214, p->desc[1], UI_DIM);
        } else if (ws_fits(wi, g_ws_slot)) {
            r_text(224, 191, "Ranura vacía", UI_LIGHT);
            r_text(224, 203, "Cruz: ver las piezas que llevas.", UI_DIM);
        } else {
            r_text(224, 191, "Ranura sin uso", UI_DIM);
            r_text(224, 203, "Esta arma no admite piezas aquí.", UI_DIM);
        }
    }
    artef_ws_draw();   // [artef] lista de piezas únicas
}

// Fabricar: recetas con hasta tres materiales. Cruz fabrica si tienes todo y cabe lo que sale.
// Para sumar una receta basta una fila aquí (su resultado y sus materiales son objetos de ITEMS).
typedef struct { const char *name; short spr, item, qty; struct { short item, n; } need[3]; char bench; short gun; } Recipe;   // gun: GUN(W_...) = fabrica esa arma (una sola)   // bench: 1 = solo con el chatarrero (en la mochila no aparece); 2 = además pide el banco de munición (mejora de la base)
static const Recipe RECIPES[] = {
    {"Venda", S_IT_BAND, ITEM_BAND, 1, {{ITEM_RAG, 2}}},
    {"Botiquín", S_IT_MED, ITEM_MED, 1, {{ITEM_BAND, 2}, {ITEM_CHEM, 1}}},
    {"Filtro", S_IT_FILTER, ITEM_FILTER, 1, {{ITEM_RAG, 1}, {ITEM_CHEM, 1}}},
    {"Antirradiación", S_IT_ARAD, ITEM_ARAD, 1, {{ITEM_CHEM, 2}}},
    {"Kit de limpieza", S_IT_CLEAN, ITEM_CLEAN, 1, {{ITEM_MECH, 1}, {ITEM_RAG, 1}}},
    {"Kit de costura", S_IT_SEW, ITEM_SEW, 1, {{ITEM_RAG, 2}, {ITEM_MECH, 1}}},
    {"Balas 9x18 x8", S_IT_9MM, ITEM_9MM, 8, {{ITEM_POWDER, 1}, {ITEM_SCRAP, 4}}, 1},
    {"Cartuchos cal. 12 x4", S_IT_12G, ITEM_12G, 4, {{ITEM_POWDER, 1}, {ITEM_SCRAP, 3}}, 1},
    {"Balas 7,62 x10", S_IT_762, ITEM_762, 10, {{ITEM_POWDER, 2}, {ITEM_SCRAP, 5}}, 1},
    // las del banco de munición (mejora de la base)
    {"9x18 perforantes x8", S_IT_9MM_AP, ITEM_9MM_AP, 8, {{ITEM_POWDER, 2}, {ITEM_MECH, 1}, {ITEM_SCRAP, 6}}, 2},
    {"9x18 expansivas x8", S_IT_9MM_HP, ITEM_9MM_HP, 8, {{ITEM_POWDER, 2}, {ITEM_SCRAP, 4}}, 2},
    {"7,62 perforantes x10", S_IT_762_AP, ITEM_762_AP, 10, {{ITEM_POWDER, 3}, {ITEM_MECH, 1}, {ITEM_SCRAP, 8}}, 2},
    {"Bala única cal. 12 x4", S_IT_12G_SLUG, ITEM_12G_SLUG, 4, {{ITEM_POWDER, 2}, {ITEM_SCRAP, 4}}, 2},
    // armas cuerpo a cuerpo, con el chatarrero
    {"Hacha", S_W_AXE, ITEM_SCRAP, 0, {{ITEM_MECH, 2}, {ITEM_SCRAP, 10}}, 1, GUN(W_AXE)},
    {"Tubo de acero", S_W_PIPE, ITEM_SCRAP, 0, {{ITEM_MECH, 1}, {ITEM_SCRAP, 6}}, 1, GUN(W_PIPE)},
    {"Mazo", S_W_MAUL, ITEM_SCRAP, 0, {{ITEM_MECH, 3}, {ITEM_RAG, 2}, {ITEM_SCRAP, 18}}, 1, GUN(W_MAUL)},
    // objetos de mano
    {"Tuercas x4", S_IT_NUT, ITEM_NUT, 4, {{ITEM_MECH, 1}}},
    {"Cóctel incendiario", S_IT_MOLO, ITEM_MOLO, 1, {{ITEM_RAG, 1}, {ITEM_CHEM, 1}}},
    {"Granada", S_IT_GREN, ITEM_GREN, 1, {{ITEM_POWDER, 2}, {ITEM_MECH, 1}}, 1},
    {"Mina", S_IT_MINE, ITEM_MINE, 1, {{ITEM_POWDER, 3}, {ITEM_MECH, 2}}, 1},
    {"Cepo", S_IT_TRAP, ITEM_TRAP, 1, {{ITEM_MECH, 3}, {ITEM_SCRAP, 4}}, 1},
    {"Caja", S_IT_MECH, ITEM_BK_BOX, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 10}}},   // [bunker]
    {"Lámpara", S_IT_NUT, ITEM_BKD_LAMP, 1, {{ITEM_MECH, 1}, {ITEM_SCRAP, 6}}},   // [bunker]
    {"Alfombra", S_IT_RAG, ITEM_BKD_RUG, 1, {{ITEM_RAG, 4}}},   // [bunker]
    {"Estante", S_IT_MECH, ITEM_BKD_SHELF, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 8}}},   // [bunker]
    {"Afiche", S_IT_RAG, ITEM_BKD_POSTER, 1, {{ITEM_RAG, 1}}},   // [bunker]
    {"Planta", S_IT_CHEM, ITEM_BKD_PLANT, 1, {{ITEM_RAG, 1}, {ITEM_CHEM, 1}}},   // [bunker]
    {"Radio", S_IT_DET, ITEM_BKD_RADIO, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 6}}},   // [bunker]
    {"Mesa", S_IT_MECH, ITEM_BKD_TABLE, 1, {{ITEM_MECH, 2}, {ITEM_RAG, 1}}},   // [bunker]
    {"Silla", S_IT_RAG, ITEM_BKD_CHAIR, 1, {{ITEM_MECH, 1}, {ITEM_RAG, 1}}},   // [bunker]
    {"Kit de reparación", S_IT_CLEAN, ITEM_REPAIR, 1, {{ITEM_SCRAP, 6}, {ITEM_MECH, 2}, {ITEM_RAG, 1}}},   // [armas]
    {"Cañón largo", S_IT_SIL, ITEM_LONGB, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 20}}, 1},   // [armas]
    {"Freno de boca", S_IT_COMP, ITEM_BRAKE, 1, {{ITEM_MECH, 1}, {ITEM_SCRAP, 10}}, 1},   // [armas]
    {"Mira telescópica", S_IT_REFLEX, ITEM_SCOPE, 1, {{ITEM_MECH, 3}, {ITEM_SCRAP, 40}}, 1},   // [armas]
    {"Cargador tambor", S_IT_XMAG, ITEM_DRUM, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 25}}, 1},   // [armas]
    {"Culata acolchada", S_IT_STOCK, ITEM_PADSTOCK, 1, {{ITEM_RAG, 3}, {ITEM_MECH, 1}, {ITEM_SCRAP, 12}}, 1},   // [armas]
    {"Linterna táctica", S_IT_NUT, ITEM_LIGHT, 1, {{ITEM_MECH, 1}, {ITEM_SCRAP, 12}}, 1},   // [armas]
    {"Láser", S_IT_DET, ITEM_LASER, 1, {{ITEM_MECH, 2}, {ITEM_CHEM, 1}, {ITEM_SCRAP, 20}}, 1},   // [armas]
    {"Bípode", S_IT_MECH, ITEM_BIPOD, 1, {{ITEM_MECH, 2}, {ITEM_SCRAP, 15}}, 1},   // [armas]
    {"Bayoneta", S_IT_TRAP, ITEM_BAYO, 1, {{ITEM_MECH, 1}, {ITEM_SCRAP, 10}}, 1},   // [armas]
    DURO_RECIPE_ROWS   // [duro]
};
#define RECIPES_N ((int)(sizeof(RECIPES) / sizeof(RECIPES[0])))
#define FAB_ROWS 7
static int g_fab_sel, g_fab_top;

static int recipe_ok(const Recipe *r)
{
    for (int k = 0; k < 3; k++) if (r->need[k].n && inv_count(r->need[k].item) < r->need[k].n) return 0;
    return 1;
}

static void fab_make(int i)
{
    const Recipe *r = &RECIPES[i];
    char s[96];
    if (r->gun && g_w[r->gun - 1].owned) snprintf(s, sizeof(s), "Ya tienes: %s.", r->name);
    else if (!recipe_ok(r)) snprintf(s, sizeof(s), "Te faltan materiales para %s.", r->name);
    else if (r->gun) {
        for (int k = 0; k < 3; k++) if (r->need[k].n) inv_take(r->need[k].item, r->need[k].n);
        g_w[r->gun - 1] = (WeaponState){1, WDEF[r->gun - 1].mag, 1.0f};
        snprintf(s, sizeof(s), "Fabricaste: %s. Cambia de arma con izquierda o derecha.", r->name);
    } else if (inv_room(r->item) < r->qty) snprintf(s, sizeof(s), "No te cabe: mochila llena de %s.", ITEMS[r->item].many);
    else {
        for (int k = 0; k < 3; k++) if (r->need[k].n) inv_take(r->need[k].item, r->need[k].n);
        inv_add(r->item, r->qty);
        snprintf(s, sizeof(s), "Fabricaste: %s.", r->name);
    }
    msg(s);
}

// Las recetas a la vista: todas con el chatarrero (g_fab_all), solo las que no piden banco en la mochila. Deja sus lugares en RECIPES
static int fab_list(int *ids)
{
    int n = 0;
    for (int i = 0; i < RECIPES_N; i++)
        if (g_fab_all ? RECIPES[i].bench <= 1 + g_up[UP_BENCH] : !RECIPES[i].bench) ids[n++] = i;
    return n;
}

static void fab_input(void)
{
    int ids[RECIPES_N], n = fab_list(ids);
    if (g_fab_sel >= n) g_fab_sel = n - 1;
    if (in.pressed & PSP_CTRL_UP) g_fab_sel = (g_fab_sel + n - 1) % n;
    if (in.pressed & PSP_CTRL_DOWN) g_fab_sel = (g_fab_sel + 1) % n;
    list_keep(&g_fab_sel, &g_fab_top, n, 1, FAB_ROWS);
    if (in.pressed & PSP_CTRL_CROSS) fab_make(ids[g_fab_sel]);
}

static void fab_draw(void)
{
    static const u32 RED = RGBA(240, 90, 80, 255);
    char s[64];
    int ids[RECIPES_N], n = fab_list(ids), sel = g_fab_sel < n ? g_fab_sel : n - 1, top = g_fab_top;
    list_keep(&sel, &top, n, 1, FAB_ROWS);
    ui_panel(14, 60, 452, 168, "FABRICAR");
    r_text(176, 62, "Necesita", UI_DIM);
    for (int r = 0; r < FAB_ROWS && top + r < n; r++) {
        int i = top + r, on = i == sel, ok = recipe_ok(&RECIPES[ids[i]]);
        const Recipe *rc = &RECIPES[ids[i]];
        float y = 80 + r * 20;
        Spr ic = SPR[rc->spr];
        ui_row(22, y, 428, 18, on);
        r_spr(ic, 36 - ic.w / 2, y + 9 - ic.h / 2, ok ? COL_WHITE : RGBA(255, 255, 255, 120));
        r_text(52, y + 3, rc->name, on ? COL_WHITE : ok ? UI_LIGHT : UI_DIM);
        float nx = 176;   // los materiales, uno tras otro desde aquí (hasta tres)
        for (int k = 0; k < 3; k++) {
            if (!rc->need[k].n) continue;
            int have = inv_count(rc->need[k].item);
            snprintf(s, sizeof(s), "%d %s (%d)", rc->need[k].n, ITEMS[rc->need[k].item].name, have);
            r_text(nx, y + 3, s, have >= rc->need[k].n ? UI_LIGHT : RED);
            nx += r_text_w(s) + 12;
        }
    }
    ui_scroll(457, 80, 138, top, FAB_ROWS, n);
}

// ---- viaje rápido
static int stop_here(void) { for (int j = 0; j < NSTOP; j++) if (STOPS[j].zone == g_zone) return j; return -1; }
// El tiempo pasa de golpe: la cuenta de la bomba avanza sec segundos
// El tiempo pasa de golpe: el reloj de la Ceniza avanza sec segundos
static void time_jump(float sec)
{
    if (g_bomba_t > 0) { g_bomba_t -= sec; if (g_bomba_t <= 0) g_bomba_t = 0.01f; }   // [bomba] el viaje gasta tiempo, la explosión llega en el siguiente cuadro
}

static void travel_go(int j)
{
    char s[96];
    if (!g_stop_seen[j]) { msg("Aún no conoces esa parada."); return; }
    if (threat_near(260)) { msg("No puedes viajar con enemigos en combate cerca."); return; }
    if (g_bag[ITEM_SCRAP] < TRAVEL_COST) {
        snprintf(s, sizeof(s), "Te falta chatarra: el viaje cuesta %d y tienes %d.", TRAVEL_COST, g_bag[ITEM_SCRAP]);
        msg(s);
        return;
    }
    g_bag[ITEM_SCRAP] -= TRAVEL_COST;
    P.hunger = clampf(P.hunger - 8, 0, 100);
    P.thirst = clampf(P.thirst - 10, 0, 100);
    time_jump(30);
    zone_go(STOPS[j].zone, STOPS[j].tx * TS + 8, STOPS[j].ty * TS + 8, 1);
}

static int travel_list(int *ids)   // los destinos: todas las paradas menos la actual
{
    int n = 0, cur = stop_here();
    for (int j = 0; j < NSTOP; j++) if (j != cur) ids[n++] = j;
    return n;
}

static void travel_input(void)
{
    int ids[NSTOP], n = travel_list(ids);
    if (!n) return;
    if (g_trav_sel >= n) g_trav_sel = 0;
    if (in.pressed & PSP_CTRL_UP) g_trav_sel = (g_trav_sel + n - 1) % n;
    if (in.pressed & PSP_CTRL_DOWN) g_trav_sel = (g_trav_sel + 1) % n;
    if (in.pressed & PSP_CTRL_CROSS) travel_go(ids[g_trav_sel]);
}

static void travel_draw_panel(const char *title)
{
    char s[96];
    int ids[NSTOP], n = travel_list(ids), cur = stop_here();
    ui_panel(14, 60, 452, 168, title);
    snprintf(s, sizeof(s), "Estás en: %s", cur >= 0 ? STOPS[cur].name : zname(g_zone));
    r_text(26, 80, s, UI_LIGHT);
    snprintf(s, sizeof(s), "El viaje cuesta %d de chatarra y toma un rato: pasan 30 segundos de la Ceniza,", TRAVEL_COST);
    r_text(26, 94, s, UI_DIM);
    r_text(26, 106, "y llegas con 8 puntos menos de hambre y 10 menos de sed. Se guarda la partida.", UI_DIM);
    r_text(26, 124, "Destinos", RGBA(190, 200, 150, 255));
    for (int i = 0; i < n; i++) {
        float y = 138 + i * 20;
        int seen = g_stop_seen[ids[i]], sel = i == g_trav_sel;
        ui_row(22, y, 436, 18, sel);
        r_text(34, y + 3, seen ? STOPS[ids[i]].name : "Parada sin descubrir", seen ? (sel ? COL_WHITE : UI_LIGHT) : UI_DIM);
        snprintf(s, sizeof(s), seen ? "%d de chatarra" : "-", TRAVEL_COST);
        text_right(446, y + 3, s, seen && g_bag[ITEM_SCRAP] < TRAVEL_COST ? RGBA(240, 90, 80, 255) : RGBA(236, 214, 150, 255));
    }
    snprintf(s, sizeof(s), "Llevas %d de chatarra", g_bag[ITEM_SCRAP]);
    r_text(26, 208, s, UI_DIM);
}

// Mejoras de la base: las construye el chatarrero (pestaña MEJORAS). Cada una cuesta chatarra y hasta dos materiales de la
// mochila, cambia la base a la vista (base_apply) y da una ventaja. Para sumar una: su UP_ en el enum de arriba (y UP_MAX), su fila
// aquí en el mismo orden, su efecto donde corresponda y, si cambia el mapa, su caso en base_apply
typedef struct {
    const char *name, *desc;   // el nombre y una línea con lo que da
    short scrap;               // costo en chatarra
    struct { short item, n; } need[2];   // y en materiales de la mochila
    signed char req;           // otra mejora que debe estar hecha antes (-1 = ninguna)
} UpDef;
static const UpDef UPS[UP_N] = {
    [UP_STASH1] = {"Alijo ampliado I", "El alijo pasa de 60 a 150 unidades. Aparece un segundo arcón.", 80, {{ITEM_MECH, 2}}, -1},
    [UP_STASH2] = {"Alijo ampliado II", "El alijo pasa a 400 unidades. Aparece una estantería.", 200, {{ITEM_MECH, 4}}, UP_STASH1},
    [UP_BED] = {"Cama de verdad", "Dormir da +70 de vida y cuesta la mitad de hambre y sed.", 60, {{ITEM_RAG, 4}}, -1},
    [UP_CLINIC] = {"Enfermería", "Despeja el cuarto con escombros. Una vez por vuelta: cura todo.", 150, {{ITEM_CHEM, 3}, {ITEM_MECH, 2}}, -1},
    [UP_TOWER] = {"Torre de vigía", "Cada expedición empieza con el mapa de SELECT descubierto.", 120, {{ITEM_MECH, 3}, {ITEM_RAG, 2}}, -1},
    [UP_GARDEN] = {"Huerto y colector de agua", "Sella el charco. Al volver: 1 conserva y 1 agua al alijo.", 200, {{ITEM_CHEM, 4}}, -1},
    [UP_BENCH] = {"Banco de munición", "Banco más grande: balas perforantes, expansivas y de bala única.", 180, {{ITEM_MECH, 4}}, -1},
};
#define UP_ROWS 4   // mejoras a la vista; si hay más, la lista corre
static int g_up_sel, g_up_top;

// Lo que le falta a la mejora i (chatarra y materiales): 0 si puede construirse ya
static int up_missing(int i)
{
    const UpDef *u = &UPS[i];
    int m = g_bag[ITEM_SCRAP] < u->scrap;
    for (int k = 0; k < 2; k++) if (u->need[k].n && inv_count(u->need[k].item) < u->need[k].n) m = 1;
    return m;
}

static void up_build(int i)
{
    const UpDef *u = &UPS[i];
    char s[96];
    if (g_up[i]) { msg("Esa mejora ya está hecha."); return; }
    if (u->req >= 0 && !g_up[u->req]) { snprintf(s, sizeof(s), "Primero hay que hacer: %s.", UPS[u->req].name); msg(s); return; }
    if (up_missing(i)) { msg("Te faltan chatarra o materiales."); return; }
    g_bag[ITEM_SCRAP] -= u->scrap;
    for (int k = 0; k < 2; k++) if (u->need[k].n) inv_take(u->need[k].item, u->need[k].n);
    g_up[i] = 1;
    base_apply(1);   // la base cambia a la vista, ya
    unstick_player();
    bake_lights();
    snd_play(SFX_JOB_DONE, 0.9f, 0);
    snprintf(s, sizeof(s), "Construiste: %s.", u->name);
    msg(s);
    save_game();
}

static void ups_input(void)
{
    if (in.pressed & PSP_CTRL_UP) g_up_sel = (g_up_sel + UP_N - 1) % UP_N;
    if (in.pressed & PSP_CTRL_DOWN) g_up_sel = (g_up_sel + 1) % UP_N;
    list_keep(&g_up_sel, &g_up_top, UP_N, 1, UP_ROWS);
    if (in.pressed & PSP_CTRL_CROSS) up_build(g_up_sel);
}

static void ups_draw(void)
{
    static const u32 RED = RGBA(240, 90, 80, 255), GREEN = RGBA(150, 214, 130, 255);
    char s[96];
    int sel = g_up_sel, top = g_up_top;
    list_keep(&sel, &top, UP_N, 1, UP_ROWS);
    ui_panel(14, 60, 452, 168, "MEJORAS DE LA BASE");
    snprintf(s, sizeof(s), "%d de %d hechas", ups_done(), UP_N);
    text_right(458, 62, s, UI_DIM);
    for (int r = 0; r < UP_ROWS && top + r < UP_N; r++) {
        int i = top + r, on = i == sel;
        const UpDef *u = &UPS[i];
        float y = 77 + r * 36, nx = 34;
        int done = g_up[i], blocked = u->req >= 0 && !g_up[u->req], miss = up_missing(i);
        ui_row(22, y, 428, 34, on);
        r_text(34, y + 2, u->name, done ? UI_DIM : on ? COL_WHITE : UI_LIGHT);
        if (done) text_right(440, y + 2, "Hecha", GREEN);
        else if (blocked) { snprintf(s, sizeof(s), "Pide: %s", UPS[u->req].name); text_right(440, y + 2, s, RED); }
        else if (miss) text_right(440, y + 2, "Te falta algo", RED);
        else text_right(440, y + 2, "Disponible: Cruz", RGBA(236, 214, 150, 255));
        if (!done) {   // el costo: cada parte en rojo si no alcanza
            snprintf(s, sizeof(s), "%d chatarra", u->scrap);
            r_text(nx, y + 13, s, g_bag[ITEM_SCRAP] >= u->scrap ? UI_LIGHT : RED);
            nx += r_text_w(s) + 10;
            for (int k = 0; k < 2; k++) {
                if (!u->need[k].n) continue;
                int have = inv_count(u->need[k].item);
                snprintf(s, sizeof(s), "%d %s (%d)", u->need[k].n, ITEMS[u->need[k].item].name, have);
                r_text(nx, y + 13, s, have >= u->need[k].n ? UI_LIGHT : RED);
                nx += r_text_w(s) + 10;
            }
        }
        r_text(34, y + 24, u->desc, done ? UI_LIGHT : UI_DIM);
    }
    ui_scroll(457, 80, 138, top, UP_ROWS, UP_N);
}

#include "peso.inc"   // [peso] peso de la mochila, armero y comienzo con el cuchillo
#define DURO_PART 2
#include "duro.inc"   // [duro] pantalla de pedidos
#undef DURO_PART
static const KolosTab KTAB[] = {
    {"TRABAJOS", "Arriba/abajo: elegir   Cruz: tomar o entregar   Cuadrado: cancelar   Círculo: salir", jobs_input, jobs_draw, NULL, KW_COM},
    {"COMERCIANTE", "Cruz: comprar   Cuadrado: vender   Círculo: salir", shop_input, shop_draw, NULL, KW_COM},
    {"ALIJO", "Flechas: elegir   Cruz: pasar al otro lado   Círculo: salir", stash_input, stash_draw, NULL, KW_STASH},
    {"ARMERO", "Arriba/abajo: elegir   Cruz: llevar o guardar   Círculo: salir", peso_arm_input, peso_arm_draw, NULL, KW_STASH},   // [peso]
    {"TALLER", "Izq./der.: arma   Arriba/abajo: ranura   Cruz: piezas   Círculo: salir", ws_input, ws_draw, ws_modal, KW_SCAV},
    {"FABRICAR", "Arriba/abajo: elegir   Cruz: fabricar   Círculo: salir", fab_input, fab_draw, NULL, KW_SCAV},
    {"MEJORAS", "Arriba/abajo: elegir   Cruz: construir   Círculo: salir", ups_input, ups_draw, NULL, KW_SCAV},
    DURO_KTAB_ROW   // [duro]
};
#define KTAB_N ((int)(sizeof(KTAB) / sizeof(KTAB[0])))

// Abre el menú de la base para quien habla (KW_): arranca en su primera pestaña. Hablar con el comerciante o abrir el alijo guarda la partida
static void kolos_open(int who)
{
    g_kol_who = who;
    g_kol_tab = 0;
    while (!(KTAB[g_kol_tab].who & who)) g_kol_tab++;
    g_state = GS_KOLOS;
    g_kol_sel = 0;
    g_ws_w = P.weapon; g_ws_slot = g_ws_list = g_ws_opt = 0;
    g_fab_all = 1;
    g_msg_t = 0;
    if (who != KW_SCAV) save_game();
}

#include "bunker.inc"   // [bunker] Mi búnker

static int kolos_step(int d)   // la pestaña siguiente (d = 1) o anterior (d = -1) entre las de este personaje
{
    int t = g_kol_tab;
    do t = (t + d + KTAB_N) % KTAB_N; while (!(KTAB[t].who & g_kol_who));
    return t;
}

static void kolos_input(void)
{
    const KolosTab *t = &KTAB[g_kol_tab];
    if (t->modal && t->modal()) t->input();   // con una lista abierta, la pestaña atiende todo (Círculo la cierra)
    else if (in.pressed & PSP_CTRL_CIRCLE) { bk_close(); save_game(); g_state = GS_PLAY; }   // [bunker]: bk_close devuelve la caja   // lo comprado, lo del alijo y las piezas quedan guardados
    else if (in.pressed & PSP_CTRL_LTRIGGER) g_kol_tab = kolos_step(-1);
    else if (in.pressed & PSP_CTRL_RTRIGGER) g_kol_tab = kolos_step(1);
    else KTAB[g_kol_tab].input();
}

// Pantalla del portón: arriba el rango y la chatarra, la tira de pestañas y debajo la pestaña abierta
static void draw_kolos(void)
{
    char s[96];
    float x;
    r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
    ui_panel(14, 22, 452, 36, g_bk_open >= 0 ? "CAJA DEL BÚNKER" : "ESTACIÓN KOLOS");
    snprintf(s, sizeof(s), "Chatarra %d", g_bag[ITEM_SCRAP]);
    x = 458 - r_text_w(s);
    r_text(x, 24, s, UI_LIGHT);
    rank_text(s, sizeof(s));
    r_text(x - 20 - r_text_w(s), 24, s, RGBA(190, 200, 150, 255));

    int ntab = 0;
    for (int i = 0; i < KTAB_N; i++) ntab += (KTAB[i].who & g_kol_who) != 0;
    if (ntab > 1) r_text(24, 42, "L", UI_DIM);
    x = 40;
    for (int i = 0; i < KTAB_N; i++) {
        if (!(KTAB[i].who & g_kol_who)) continue;
        float w = r_text_w(KTAB[i].name) + 20;
        if (i == g_kol_tab) ui_fill(S_UI_SLOT, x, 40, w, 16, COL_WHITE);
        r_text(x + 10, 42, KTAB[i].name, i == g_kol_tab ? COL_WHITE : UI_DIM);
        x += w + 4;
    }
    if (ntab > 1) r_text(x + 6, 42, "R", UI_DIM);

    KTAB[g_kol_tab].draw();
    r_text_center(SCR_W / 2, 231, KTAB[g_kol_tab].modal && KTAB[g_kol_tab].modal() ? "Arriba/abajo: elegir   Cruz: confirmar   Círculo: volver" :
                  KTAB[g_kol_tab].help, RGBA(170, 166, 158, 255));
    ui_msg(246);
}

enum { D_REFUGE, D_NEW, D_TITLE };
static const char *const DEAD_NAME[] = {"Volver al último refugio", "Partida nueva", "Salir al título"};
static int dead_items(int *ids)
{
    int n = 0;
    if (g_mode != MODE_IRON) ids[n++] = D_REFUGE;
    ids[n++] = D_NEW;
    ids[n++] = D_TITLE;
    return n;
}

static void draw_end(int win)
{
    char s[96];
    float a = clampf(g_state_t * 1.5f, 0, 1);
    r_rect(0, 0, SCR_W, SCR_H, win ? RGBA(10, 14, 8, (int)(190 * a)) : RGBA(40, 4, 4, (int)(200 * a)));
    const char *t = win ? "TRABAJO CUMPLIDO" : "HAS MUERTO";
    float w = r_text_w(t) * 2;
    r_text_s(SCR_W / 2 - w / 2 + 2, 72, t, RGBA(0, 0, 0, 200), 2);
    r_text_s(SCR_W / 2 - w / 2, 70, t, win ? RGBA(160, 214, 130, 255) : RGBA(220, 60, 50, 255), 2);
    if (win) {
        r_text_center(SCR_W / 2, 112, "Entregaste los repuestos en Estación Kolos.", RGBA(220, 214, 200, 255));
        r_text_center(SCR_W / 2, 128, "+120 chatarra     Reputación +1", RGBA(236, 214, 150, 255));
    } else r_text_center(SCR_W / 2, 112, g_bomba_muerte ? "La bomba cayó antes de que volvieras." : "La zona no perdona.", RGBA(220, 214, 200, 255));
    int t_s = (int)g_play_t;
    snprintf(s, sizeof(s), "Tiempo %d:%02d     Bajas %d     Cajones %d     Chatarra %d", t_s / 60, t_s % 60, g_kills, g_opened, g_bag[ITEM_SCRAP]);
    r_text_center(SCR_W / 2, 152, s, RGBA(170, 166, 158, 255));
    if (!win && g_state_t > 1) {
        int ids[3], n = dead_items(ids);
        if (g_mode == MODE_IRON) r_text_center(SCR_W / 2, 168, "Muerte permanente: tu partida fue borrada.", RGBA(236, 150, 130, 255));
        for (int i = 0; i < n; i++) menu_row(184 + i * 20, DEAD_NAME[ids[i]], i == g_dead_sel);
    } else if (win && g_state_t > 1 && ((int)(g_time * 2) & 1)) r_text_center(SCR_W / 2, 200, "START: jugar de nuevo", COL_WHITE);
}

// ------------------------------------------------------------------ un cuadro
// de fondo del título, la cámara pasea por la zona
static void title_pan(void)
{
    g_camx = (WORLD_W - SCR_W) * (0.5f + 0.5f * sinf(g_time * 0.07f));
    g_camy = (WORLD_H - SCR_H) * (0.35f + 0.2f * sinf(g_time * 0.11f));
    g_ox = floorf(g_camx);
    g_oy = floorf(g_camy);
}

// Opciones: arriba/abajo elige, izquierda/derecha cambia (se aplica al momento), Círculo vuelve y guarda
static void opt_input(void)
{
    if (g_opt_ret == GS_TITLE) title_pan();
    if (in.pressed & PSP_CTRL_UP) g_opt_sel = (g_opt_sel + OPT_COUNT - 1) % OPT_COUNT;
    if (in.pressed & PSP_CTRL_DOWN) g_opt_sel = (g_opt_sel + 1) % OPT_COUNT;
    if (in.pressed & PSP_CTRL_LEFT && g_opt[g_opt_sel] > 0) g_opt[g_opt_sel]--;
    if (in.pressed & PSP_CTRL_RIGHT && g_opt[g_opt_sel] < OPT_N[g_opt_sel] - 1) g_opt[g_opt_sel]++;
    opt_apply();
    if (in.pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) { cfg_save(); g_state = g_opt_ret; }
}

#ifdef PEPEL_DEBUG
// Saltos de pruebas: 0-3 cajones, 4 catre, 5 zombi grande, 6 jefe, 7 máscara, 8 almacén, 9 bandidos, 10 salida este, 11 Barrio, 12 Chofer, 13 escotilla, 14 búnker (los de zona van al final: cambian de zona)
static const int DBG_TP[][3] = {{0, 31, 11}, {0, 45, 11}, {0, 41, 12}, {0, 28, 13}, {0, 13, 24}, {0, 43, 17}, {0, 52, 34}, {0, 47, 30}, {0, 16, 33}, {0, 17, 10}, {0, 60, 19}, {0, 4, 19}, {0, 5, 19}, {0, 6, 19}, {2, 0, 0}, {0, 2, 19}, {0, 35, 14}, {0, 42, 14}, {0, 11, 25}};
#define DBG_NTP ((int)(sizeof(DBG_TP) / sizeof(DBG_TP[0])))

// Pruebas del generador: 300 expediciones con semillas, tipos, peligros y marcas distintos. Cada una se arma de verdad
// (zone_build) y se comprueba: entrada libre, todo alcanzable, 10 cajones o más, enemigos dentro del tope y lejos de la
// entrada, nada sobre los muros. El resumen va a dbg_log.txt
static void dbg_log_line(const char *ln)
{
    char lp[170];
    snprintf(lp, sizeof(lp), "%s", g_sav);
    char *sl = strrchr(lp, '/');
    if (!sl) return;
    strcpy(sl + 1, "dbg_log.txt");
    SceUID lf = sceIoOpen(lp, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (lf < 0) return;
    sceIoWrite(lf, ln, strlen(ln));
    sceIoClose(lf);
}
#ifndef DBG_I0
#define DBG_I0 0
#define DBG_I1 80
#endif
static void dbg_exped_test(void)
{
    enum { R_ENTRY, R_REACH, R_LOOT, R_ENEMY, R_FAR, R_WALL, R_HATCH, R_BOSS, R_BLD, R_STAIR, R_FLOOR, R_RET2, R_BODY, R_CAP, R_ANOM, R_EVENT, R_N };
    static const char *const RN[R_N] = {"entrada", "alcance", "cajones", "tope", "cerca", "muros", "escotilla", "bestia", "edificios", "escalera", "pisos", "retirada2", "cuerpos", "topes", "anomalias", "eventos"};
    static unsigned char reach[MHMAX][MWMAX];
    static short q[MWMAX * MHMAX];
    unsigned t_sum = 0, t_max = 0; int t_imax = 0;
    int bad[R_N] = {0}, ok = 0, tot_loot = 0, tot_ne = 0, tot_fix = 0, tot_bld = 0, shown = 0, tot_stb = 0, tot_fl = 0, per_type[XT_N] = {0}, snow_n = 0;
    char ln[300];
    int lay_n[3] = {0, 0, 0}, tot_hs = 0, tot_hcr = 0, tot_hne = 0, bad_hs = 0;   // [mapas] casas con segundo piso
    int cnt_new[E_TYPES] = {0}, ev_n[EV_N] = {0}, dog_n = 0, ev_miss = 0, faro_n = 0;
    Exped keep = g_x;
    unsigned t_ini = sceKernelGetSystemTimeLow();
    for (int i = DBG_I0; i < DBG_I1; i++) {   // [qurbe] 80 expediciones: 10 de cada uno de los 8 tipos
        int fl = (i % 3 == 0 ? XF_HATCH : 0) | (i % 5 == 0 ? XF_JOB : 0) | ((i % XT_N) == XT_FOREST && (i / XT_N) % 2 == 0 ? XF_BEAST : 0) | (i % 4 == 0 ? XF_BANDITS : 0) |
                 (i % 13 == 0 ? XF_MASKBODY : 0) | (i % 2 == 0 ? XF_SAWED : 0) | ((i / XT_N) % 2 ? XF_SNOW : 0) | (i % 10 >= 5 && i % 3 == 0 ? XF_BOSS2 : 0) | (i % 3 == 1 ? XF_FARO : 0);   // [qurbe] la bestia solo en el bosque   // los 6 tipos, con y sin nieve
        g_x = (Exped){(0x9E3779B1u * (u32)(i + 1)) | 1, 1 + i % 20, i % XT_N, fl, i % 10};
        if (g_x.type == XT_ROAD) g_x.type = XT_NUC;   // [mapnuc] la autopista queda fuera del sorteo del juego: la prueba usa la planta en su lugar
        if (g_x.type == XT_NUC) g_x.flags &= ~(XF_SNOW | XF_BEAST);   // [mapnuc]
        if (g_x.type == XT_CURSED) g_x.flags &= ~(XF_SNOW | XF_BEAST);
        g_x.ev0 = 1 + i % 4; g_x.ev1 = g_x.danger >= 4 ? 1 + (i + 1 + (i / 4) % 3) % 4 : 0;   // los cuatro eventos, solos y de a dos
        if (g_x.ev1 == g_x.ev0) g_x.ev1 = 1 + g_x.ev1 % 4;
        if (i % 2 == 0) g_x.flags |= XF_DOG;
        fl = g_x.flags;
        per_type[g_x.type]++; snow_n += (fl & XF_SNOW) != 0;
        g_x_fix = 0;
        unsigned t_a = sceKernelGetSystemTimeLow();
        zone_build(1);
        t_a = sceKernelGetSystemTimeLow() - t_a;
        t_sum += t_a; if (t_a > t_max) { t_max = t_a; t_imax = i; }
        lay_n[g_lay]++;
        int f[R_N] = {0};
        int ex = g_ret_tx, ey = g_ret_ty;
        f[R_ENTRY] = !in_map(ex, ey) || solid(ex, ey) || g_g[ey][ex] != G_ZONE;
        memset(reach, 0, sizeof(reach));
        int h = 0, t = 0;
        if (!f[R_ENTRY]) { reach[ey][ex] = 1; q[t++] = (short)(ey * MW + ex); }
        while (h < t) {
            int c = q[h++], x = c % MW, y = c / MW;
            for (int k = 0; k < 4; k++) {
                int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
                if (in_map(nx, ny) && !reach[ny][nx] && !solid(nx, ny)) { reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }
            }
        }
        #define NEAR(x, y) (reach[y][x] || ((x) > 0 && reach[y][(x) - 1]) || ((x) < MW - 1 && reach[y][(x) + 1]) || ((y) > 0 && reach[(y) - 1][x]) || ((y) < MH - 1 && reach[(y) + 1][x]))
        int loot = 0, hatch = 0, boss = 0, boss2 = 0;
        for (int c = 0; c < g_ncr; c++) {
            const Crate *cr = &g_cr[c];
            if (cr->kind == CK_LOOT || cr->kind == CK_RICH || cr->kind == CK_MIL) loot++;   // [mil]
            if (!NEAR(cr->tx, cr->ty)) f[R_REACH] = 1;
            if (cr->kind != CK_BODY && cr->kind != CK_MASK && cr->kind != CK_SURV && cr->kind != CK_DOGW && cr->kind != CK_FARO && g_o[cr->ty][cr->tx] != O_CRATE) { f[R_WALL] = 1; if (shown < 6) { char lw[100]; snprintf(lw, sizeof(lw), "  muro: cajon %d tipo %d en %d,%d objeto %d\n", c, cr->kind, cr->tx, cr->ty, g_o[cr->ty][cr->tx]); dbg_log_line(lw); } }
            if ((cr->kind == CK_SURV || cr->kind == CK_DOGW || cr->kind == CK_FARO) && g_o[cr->ty][cr->tx] != O_NONE) f[R_WALL] = 1;
            if (cr->kind == CK_BODY || cr->kind == CK_MASK) { if (g_o[cr->ty][cr->tx] != O_NONE) f[R_WALL] = 1; }
            for (int c2 = 0; c2 < c; c2++) if (g_cr[c2].tx == cr->tx && g_cr[c2].ty == cr->ty) { f[R_WALL] = 1; if (shown < 6) { char lw[100]; snprintf(lw, sizeof(lw), "  muro: duplicado %d/%d tipos %d %d en %d,%d\n", c2, c, g_cr[c2].kind, cr->kind, cr->tx, cr->ty); dbg_log_line(lw); } }
        }
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (g_g[y][x] == G_DOOR && !reach[y][x]) f[R_REACH] = 1;
                if (g_o[y][x] == O_HATCH) { hatch++; if (!NEAR(x, y)) f[R_REACH] = 1; }
            }
        for (int e = 0; e < g_ne; e++) {
            const Enemy *en = &g_e[e];
            int tx = (int)(en->x / TS), ty = (int)(en->y / TS);
            if (en->type == E_BOSS) boss++;
            if (en->type == E_BOSS2) boss2++;
            cnt_new[en->type]++;
            if (solid(tx, ty)) { f[R_WALL] = 1; if (shown < 6) { char lw[100]; snprintf(lw, sizeof(lw), "  muro: enemigo tipo %d en %d,%d\n", en->type, tx, ty); dbg_log_line(lw); } }
            if (en->ev != EVN_HORDE && dist(en->x, en->y, ex * TS + 8, ey * TS + 8) < 12 * TS) f[R_FAR] = 1;
        }
        f[R_LOOT] = loot < 30;   // [mapas]
        f[R_RET2] = !in_map(g_ret2_tx, g_ret2_ty) || g_g[g_ret2_ty][g_ret2_tx] != G_ZONE || !reach[g_ret2_ty][g_ret2_tx] || g_ret2_tx < MW - 12 ||
                   g_ret3_tx < 0 || !in_map(g_ret3_tx, g_ret3_ty) || g_g[g_ret3_ty][g_ret3_tx] != G_ZONE || !reach[g_ret3_ty][g_ret3_tx] || (g_ret3_ty > 8 && g_ret3_ty < MH - 9);   // [mapas] el tercero, al norte o al sur
        { int nbody = 0; for (int c = 0; c < g_ncr; c++) nbody += g_cr[c].kind == CK_BODY || g_cr[c].kind == CK_MASK; f[R_BODY] = nbody < 12 || nbody > 24; }
        f[R_CAP] = g_ncr > MAXCR || g_nfire >= MAXFIRE || g_nlamp >= MAXLAMP || g_ndec >= MAXDEC || MW != 192 || MH != 120;
        f[R_ENEMY] = g_ne != g_x_nen || g_ne > MAXE || g_ne < 20;
        f[R_HATCH] = (fl & XF_HATCH) && !hatch;
        f[R_BOSS] = ((fl & XF_BEAST) != 0) != (boss == 1) || ((fl & XF_BOSS2) != 0) != (boss2 == 1);
        f[R_BLD] = g_x_nbld < (g_x.type == XT_PINE ? 2 : 14) || g_x_nbld > (g_x.type == XT_CITY ? 130 : g_x.type == XT_TOWN ? 60 : 26);
        if (g_x.type == XT_CITY) {   // [urbe] 40 edificios o más, 2 o 3 bocas de metro y todas alcanzables arriba
            f[R_BLD] |= g_x_nbld < 40 || g_nmt < 2 || g_nmt > 3;
            for (int k = 0; k < g_nmt; k++) {
                int mx = g_mt_x[k], my = g_mt_y[k];
                if (g_o[my][mx] != O_STUP || !(reach[my][mx] || reach[my + 1][mx] || reach[my - 1][mx] || reach[my][mx + 1] || reach[my][mx - 1])) f[R_BLD] = 1;
            }
        }   // [mapas]   // [armas] el bosque de pinos: cabaña y torre
        {   // anomalías: de 1 a 4 campos según el peligro, en terreno abierto y alcanzable, lejos de la entrada, retiradas, escotilla, escaleras y cajones
            int bad_an = g_nfield < 1 || g_nfield > 1 + g_x.danger / 3 || g_nan < 2;
            for (int q = 0; q < g_nan; q++) {
                int ax = (int)(g_an[q].x / TS), ay = (int)(g_an[q].y / TS);
                if (!in_map(ax, ay) || solid(ax, ay) || !reach[ay][ax]) { bad_an = 1; continue; }
                if (dist(g_an[q].x, g_an[q].y, ex * TS + 8, ey * TS + 8) < 11 * TS || dist(g_an[q].x, g_an[q].y, g_ret2_tx * TS + 8, g_ret2_ty * TS + 8) < 6 * TS) bad_an = 1;
                if (g_hatch_tx >= 0 && dist(g_an[q].x, g_an[q].y, g_hatch_tx * TS + 8, g_hatch_ty * TS + 8) < 5 * TS) bad_an = 1;
                for (int c = 0; c < g_nstb; c++) if (dist(g_an[q].x, g_an[q].y, g_stb_x[c] * TS + 8, g_stb_y[c] * TS + 8) < 5 * TS) bad_an = 1;
                for (int c = 0; c < g_ncr; c++) if (dist(g_an[q].x, g_an[q].y, g_cr[c].tx * TS + 8, g_cr[c].ty * TS + 8) < 3 * TS) bad_an = 1;
            }
            f[R_ANOM] = bad_an;
        }
        {   // eventos: alcanzables, lejos de la entrada, la retirada, la escotilla y las escaleras
            int want = (g_x.ev0 != 0) + (g_x.ev1 != 0), bad_ev = g_nev < want;
            ev_miss += want - g_nev;
            dog_n += g_dogx >= 0;
            if ((fl & XF_DOG) && g_dogx < 0) { bad_ev = 1; ev_miss++; }
            if (fl & XF_FARO) {   // el puesto farolero: existe, es alcanzable, lejos de la entrada, con su comerciante y 2 o 3 faroleros
                int nfa = 0, cz = 0;
                for (int k = 0; k < g_ne; k++) nfa += g_e[k].ev == EVN_FARO && g_e[k].fac == F_FARO;
                for (int c = 0; c < g_ncr; c++) cz += g_cr[c].kind == CK_FARO;
                if (g_fx < 0 || !in_map(g_fx, g_fy) || !NEAR(g_fx, g_fy) || dist(g_fx, g_fy, ex, ey) < 18 || cz != 1 || nfa < 2 || nfa > 3) { bad_ev = 1; ev_miss++; }
                else faro_n++;
            }
            if (g_dogx >= 0 && (!NEAR(g_dogx, g_dogy) || dist(g_dogx, g_dogy, ex, ey) < 18)) bad_ev = 1;
            for (int k = 0; k < g_nev; k++) {
                const Ev *v = &g_ev[k];
                ev_n[v->kind]++;
                if (!in_map(v->tx, v->ty) || !NEAR(v->tx, v->ty)) bad_ev = 1;
                if (v->kind == EV_HORDE) { if (!reach[v->ty][v->tx] || !reach[v->y2][v->x2]) bad_ev = 1; continue; }
                if (dist(v->tx, v->ty, ex, ey) < 18 || dist(v->tx, v->ty, g_ret2_tx, g_ret2_ty) < 10) bad_ev = 1;
                if (g_hatch_tx >= 0 && dist(v->tx, v->ty, g_hatch_tx, g_hatch_ty) < 6) bad_ev = 1;
                for (int c = 0; c < g_nstb; c++) if (dist(v->tx, v->ty, g_stb_x[c], g_stb_y[c]) < 6) bad_ev = 1;
                if (v->kind == EV_SURV && (!in_map(v->x2, v->y2) || !NEAR(v->x2, v->y2))) bad_ev = 1;
            }
            f[R_EVENT] = bad_ev;
        }
        // escaleras de subida y los pisos de cada edificio con escalera
        const int nb = g_nstb, sbi[2] = {g_stb_bi[0], g_stb_bi[1]}, snf[2] = {g_stb_nf[0], g_stb_nf[1]}, sx[2] = {g_stb_x[0], g_stb_x[1]}, sy[2] = {g_stb_y[0], g_stb_y[1]};
        const int ne_ext = g_ne;
        f[R_STAIR] = nb < 1;
        int why = 0;
        static int nwhy;
        for (int k = 0; k < nb; k++) if (g_o[sy[k]][sx[k]] != O_STUP || !NEAR(sx[k], sy[k])) f[R_STAIR] = 1;
        tot_stb += nb;
        (void)sbi;
        for (int k = 0; k < nb; k++)
            for (int p = 2; p <= snf[k] + 1; p++) {
                zone_build(3 + k * 3 + (p - 2));
                tot_fl++;
                int sdx = -1, sdy = -1, nup = 0, ndn = 0;
                for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
                    if (g_o[y][x] == O_STDN) { ndn++; sdx = x; sdy = y; }
                    if (g_o[y][x] == O_STUP) nup++;
                }
                int ax = -1, ay = -1;   // por dónde se llega: la casilla libre junto a la escalera de bajar
                for (int d = 0; d < 4 && ax < 0; d++) { int tx = sdx + (d == 2) - (d == 3), ty = sdy + (d == 0) - (d == 1); if (sdx >= 0 && in_map(tx, ty) && !solid(tx, ty)) { ax = tx; ay = ty; } }
                int bad_f = 0;
                if (ndn != 1) bad_f |= 1;
                if (ax < 0) bad_f |= 2;
                if (nup != (p < snf[k] + 1 ? 1 : 0)) bad_f |= 4;
                memset(reach, 0, sizeof(reach));
                h = t = 0;
                if (ax >= 0) { reach[ay][ax] = 1; q[t++] = (short)(ay * MW + ax); }
                while (h < t) {
                    int c = q[h++], x = c % MW, y = c / MW;
                    for (int d = 0; d < 4; d++) {
                        int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                        if (in_map(nx, ny) && !reach[ny][nx] && !solid(nx, ny)) { reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }
                    }
                }
                int rich = 0;
                for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
                    if (g_o[y][x] == O_STUP && !NEAR(x, y)) bad_f |= 8;
                    if (g_g[y][x] == G_DOOR && !reach[y][x]) bad_f |= 16;
                    if (g_g[y][x] == G_FLOOR && g_o[y][x] == O_NONE && !reach[y][x]) bad_f |= 32;
                }
                for (int c = 0; c < g_ncr; c++) {
                    if (!NEAR(g_cr[c].tx, g_cr[c].ty) || g_o[g_cr[c].ty][g_cr[c].tx] != O_CRATE) bad_f |= 64;
                    rich += g_cr[c].kind == CK_RICH;
                }
                if (g_ncr < 1 || (p == snf[k] + 1 && rich < 1)) bad_f |= 128;   // el último piso trae el mejor cajón
                for (int en = 0; en < g_ne; en++) {
                    int tx = (int)(g_e[en].x / TS), ty = (int)(g_e[en].y / TS);
                    if (solid(tx, ty) || !reach[ty][tx] || dist(g_e[en].x, g_e[en].y, ax * TS + 8, ay * TS + 8) < 0.9f * TS) bad_f |= 256;
                }
                if (g_ne < 1 || g_ne > MAXE) bad_f |= 512;
                if (bad_f) { f[R_FLOOR] = 1; why |= bad_f; if (nwhy < 5) { char l2[120]; snprintf(l2, sizeof(l2), "PISO mal #%d edif %d piso %d/%d: %d (cajones %d buenos %d enem %d)\n", i, k, p, snf[k] + 1, bad_f, g_ncr, rich, g_ne); dbg_log_line(l2); nwhy++; } }
            }
        {   // [mapas] los segundos pisos de las casas: una sola escalera de bajar, 1 a 3 cajones, todo alcanzable
            const int nh = g_nhs;
            int hx[HS_MAX], hy[HS_MAX];
            for (int k = 0; k < nh; k++) { hx[k] = g_hs_x[k]; hy[k] = g_hs_y[k]; }
            tot_hs += nh;
            for (int k = 0; k < nh && k < 3; k++) {   // las primeras 3 casas de cada mapa (cada una rearma el mapa entero)
                g_hs_cur = k;
                zone_build(ZH);
                int sdx = -1, sdy = -1, nup = 0, ndn = 0, bf = 0;
                for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) {
                    if (g_o[y][x] == O_STDN) { ndn++; sdx = x; sdy = y; }
                    if (g_o[y][x] == O_STUP) nup++;
                }
                int ax = -1, ay = -1;
                for (int d = 0; d < 4 && ax < 0; d++) { int tx = sdx + (d == 2) - (d == 3), ty = sdy + (d == 0) - (d == 1); if (sdx >= 0 && in_map(tx, ty) && !solid(tx, ty)) { ax = tx; ay = ty; } }
                if (ndn != 1 || nup != 0 || ax < 0 || g_ncr < 1 || g_ncr > 3 || g_ne > 6) bf = 1;
                memset(reach, 0, sizeof(reach));
                h = t = 0;
                if (ax >= 0) { reach[ay][ax] = 1; q[t++] = (short)(ay * MW + ax); }
                while (h < t) {
                    int c = q[h++], x = c % MW, y = c / MW;
                    for (int d = 0; d < 4; d++) {
                        int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                        if (in_map(nx, ny) && !reach[ny][nx] && !solid(nx, ny)) { reach[ny][nx] = 1; q[t++] = (short)(ny * MW + nx); }
                    }
                }
                for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (g_g[y][x] == G_DOOR && !reach[y][x]) bf = 1;
                for (int c = 0; c < g_ncr; c++) if (!NEAR(g_cr[c].tx, g_cr[c].ty) || g_o[g_cr[c].ty][g_cr[c].tx] != O_CRATE) bf = 1;
                for (int en = 0; en < g_ne; en++) { int tx = (int)(g_e[en].x / TS), ty = (int)(g_e[en].y / TS); if (solid(tx, ty) || !reach[ty][tx]) bf = 1; }
                tot_hcr += g_ncr; tot_hne += g_ne;
                if (bf) { f[R_FLOOR] = 1; bad_hs++; }
            }
            zone_build(1);
            for (int k = 0; k < nh; k++) if (g_hs_x[k] != hx[k] || g_hs_y[k] != hy[k] || g_o[hy[k]][hx[k]] != O_STUP || !x_near(hx[k], hy[k])) f[R_STAIR] = 1;
        }
        int any = 0;
        for (int r = 0; r < R_N; r++) { bad[r] += f[r]; any |= f[r]; }
        if (!any) ok++;
        else if (shown < 6) {
            shown++;
            snprintf(ln, sizeof(ln), "EXPED mal #%d tipo %d d %d fl %d:", i, g_x.type, g_x.danger, fl);
            for (int r = 0; r < R_N; r++) if (f[r]) { strcat(ln, " "); strcat(ln, RN[r]); }
            { char l3[80]; snprintf(l3, sizeof(l3), " [nev %d ev %d/%d perro %d ncr %d]", g_nev, g_x.ev0, g_x.ev1, g_dogx, g_ncr); strcat(ln, l3); }
            strcat(ln, "\n");
            dbg_log_line(ln);
        }
        tot_loot += loot; tot_ne += ne_ext; tot_fix += g_x_fix; tot_bld += g_x_nbld;
        if ((i + 1) % 10 == 0) {   // [qurbe] avance
            snprintf(ln, sizeof(ln), "EXPED avance %d/80: bien %d mal %d (%u s)\n", i + 1, ok, i + 1 - ok, (sceKernelGetSystemTimeLow() - t_ini) / 1000000); dbg_log_line(ln);
        }
    }
    {   // [urbe] el metro: cada estación se alcanza desde la primera y todas las bocas tienen estación
        int mok = 0, mbad = 0;
        for (int i = 0; i < 30; i++) {   // [qurbe]
            g_x = (Exped){(0x9E3779B1u * (u32)(i + 500)) | 1, 1 + i % 20, XT_CITY, 0, i % 10};
            g_x.ev0 = 1;
            zone_build(ZM);
            x_n = x_flood();
            int good = g_mt_ns >= 2 && g_mt_ns == (g_nmt < 2 ? 2 : g_nmt);
            for (int k = 0; k < g_mt_ns; k++) if (!x_reach[g_mt_sy[k] + 1][g_mt_sx[k]]) good = 0;
            if (good) mok++; else mbad++;
        }
        snprintf(ln, sizeof(ln), "EXPED metro: bien %d mal %d\n", mok, mbad); dbg_log_line(ln);
    }
    snprintf(ln, sizeof(ln), "EXPED 80: bien %d mal %d | cajones prom %d enem prom %d edif prom %d arreglos %d | escaleras %d pisos %d nevadas %d tipos %d/%d/%d/%d/%d/%d\n", ok, 80 - ok, tot_loot / 80, tot_ne / 80, tot_bld / 80, tot_fix, tot_stb, tot_fl, snow_n, per_type[0], per_type[1], per_type[2], per_type[3], per_type[4], per_type[5] + per_type[6]);   // [armas] el 7.º tipo suma al último
    { char l3[160]; snprintf(l3, sizeof(l3), "EVENTOS: herido %d caravana %d heli %d horda %d perro %d puestos %d sin_sitio %d\n", ev_n[EV_SURV], ev_n[EV_CARAVAN], ev_n[EV_HELI], ev_n[EV_HORDE], dog_n, faro_n, ev_miss); dbg_log_line(l3); }
    dbg_log_line(ln);
    snprintf(ln, sizeof(ln), "EXPED trazados: cuadricula %d avenida %d rio %d | casas con segundo piso: %d en %d mapas (prom %d), malas %d | cajones arriba prom %d.%d, enemigos arriba prom %d.%d\n", lay_n[0], lay_n[1], lay_n[2], tot_hs, 80, tot_hs / 80, bad_hs, tot_hcr / (tot_hs ? tot_hs : 1), tot_hcr * 10 / (tot_hs ? tot_hs : 1) % 10, tot_hne / (tot_hs ? tot_hs : 1), tot_hne * 10 / (tot_hs ? tot_hs : 1) % 10);
    dbg_log_line(ln);
    snprintf(ln, sizeof(ln), "EXPED enemigos (suma de las 80, sin pisos): zombi chico %d hacha %d grande %d bandido %d jefe %d | corredor %d escupidor %d acorazado %d francotirador %d lobo %d jefe2 %d\n", cnt_new[E_DOG], cnt_new[E_MAR], cnt_new[E_BIG], cnt_new[E_BAN], cnt_new[E_BOSS], cnt_new[E_RUN], cnt_new[E_SPIT], cnt_new[E_ARM], cnt_new[E_SNP], cnt_new[E_WOLF], cnt_new[E_BOSS2]);
    dbg_log_line(ln);
    snprintf(ln, sizeof(ln), "EXPED 192x120 tiempo de generacion: prom %u ms, max %u ms en la #%d (80 mapas, emulador)\n", t_sum / 80 / 1000, t_max / 1000, t_imax);
    dbg_log_line(ln);
    snprintf(ln, sizeof(ln), "EXPED fases (ms acumulados, incluye los pisos): terreno %u edificios %u puertas+arboles %u props %u campamentos %u cajones %u enemigos %u\n", g_xph[0] / 1000, g_xph[1] / 1000, g_xph[2] / 1000, g_xph[3] / 1000, g_xph[4] / 1000, g_xph[5] / 1000, g_xph[6] / 1000);
    dbg_log_line(ln);
    ln[0] = 0;
    for (int r = 0; r < R_N; r++) { char t2[40]; snprintf(t2, sizeof(t2), "%s=%d ", RN[r], bad[r]); strcat(ln, t2); }
    strcat(ln, "\n");
    dbg_log_line(ln);
    jefes_test();   // [jefes] 20 mapas de cada zona de jefe
    pozo_test();   // [pozo] 30 pisos del Pozo y 30 expediciones infernales
    g_x = keep;
    zone_build(g_zone);
}

// Pruebas: chatarra y materiales de sobra (hasta el tope de la mochila) para construir todas las mejoras
static void dbg_mejoras(void)
{
    g_bag[ITEM_SCRAP] += 2000;
    g_bag[ITEM_RAG] = g_bag[ITEM_CHEM] = g_bag[ITEM_MECH] = g_bag[ITEM_POWDER] = 20;   // sin respetar el tope de la mochila: solo para pruebas
}

// Pruebas: si existe el archivo dbg_cmd junto a la partida, se lee UNA vez al empezar a jugar y se aplican sus letras:
// M chatarra y materiales de sobra, S alijo casi lleno, U todas las mejoras hechas ya, s tormenta en 3 s, f vida llena, v guardar, g sin daño, n con daño, t + (0-9, a-f) salta a ese lugar, d muere, h malherido, a escenario, w armas, o abre cajones, p chatarra
static float g_dbg_save_t;
static void dbg_cmd_run(void)
{
    static int done, pos, n;
    static float wt;
    static char cmd[64];
    if (g_dbg_save_t > 0 && (g_dbg_save_t -= 1.0f / 60) <= 0) save_game();
    if (!done) {
        done = 1;
        char path[170];
        snprintf(path, sizeof(path), "%s", g_sav);
        char *sl = strrchr(path, '/');
        if (!sl) return;
        strcpy(sl + 1, "dbg_cmd");
        SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
        if (fd < 0) return;
        n = sceIoRead(fd, cmd, sizeof(cmd) - 1);
        sceIoClose(fd);
        if (n <= 0) { n = 0; return; }
        cmd[n] = 0;
    }
    if (pos >= n) return;
    if (wt > 0) { wt -= 1.0f / 60; return; }
    for (int i = pos; cmd[i]; i++) {
        pos = i + 1;
        switch (cmd[i]) {
        case '_': wt = 3; return;   // espera 3 s antes de seguir con las letras
        case 'm': for (int q = 0; q < g_ne; q++) if (g_e[q].state != ES_DEAD && !is_human(&g_e[q]) && g_e[q].type != E_BOSS) { hurt_enemy(&g_e[q], 9999, P.x, P.y, 1); break; } break;   // mata a un zombi
        case 'M': dbg_mejoras(); break;
        case 'S': stash_add(ITEM_FOOD, 29); stash_add(ITEM_WATER, 29); break;   // llena el alijo hasta quedar a 2 del tope (con 60: 58/60)
        case 'U': if (g_zone == 0) { for (int k = 0; k < UP_N; k++) g_up[k] = 1; base_apply(1); unstick_player(); bake_lights(); } break;
        case 's': g_storm_t = 3; break;
        case 'X': dbg_exped_test(); break;   // prueba del generador: 300 expediciones, resumen en dbg_log.txt
        case 'E': if (g_zone == 0) exped_start(); break;   // sale de expedición ya
        case 'g': g_dbg_god = 1; break;
        case 'x': for (int k = 0; k < W_COUNT; k++) g_w[k].cond = 0.25f; break;   // armas gastadas (para probar trabas y kit)
        case 'j': g_job = 7; g_job_n = 0; g_has_reg = 1; g_rep = 9; break;   // El origen de la Ceniza listo para entregar, rango 3
        case 'u': for (int k = 0; k < NSTOP; k++) g_stop_seen[k] = 1; break;
        case 'c': g_bag[ITEM_BAND] = 1; break;
        case 'b':
            for (int k = 0; k < ITEM_COUNT; k++) if (gear_of(k) < 0 && ITEMS[k].cap) g_bag[k] = ITEMS[k].cap;
            break;
        case 'f': P.hp = 100; P.bleed = 0; P.hunger = P.thirst = 100; break;
        case 'v': g_dbg_save_t = 4; break;   // guarda 4 s después (ya con la zona nueva)
        case 'n': g_dbg_god = 0; break;
        case 'd': hurt_player(999); break;
        case 'h': P.hp = 30; P.hunger = 30; P.thirst = 20; break;
        case 'a': dbg_scenario(); break;
        case 'y': P.x = 54; P.y = 296; P.ang = 0; P.lock = -1; break;   // portón: de frente al guardia del norte
        case 'z': spawn_enemy(E_BIG, 100, 312); g_e[g_ne - 1].hp = 5000; break;   // un zombi grande casi inmortal cerca del portón (los guardias lo atacan)
        case 'i': P.weapon = W_AK; break;
        case 'k': for (int q = 0; q < g_ne; q++) if (g_e[q].type == E_BOSS) g_e[q].hp = 30; break;   // jefe casi muerto
        case 'D': hurt_player(20); break;   // un golpe de 20
        case 'e': P.hp = 100; g_dbg_god = 0; break;
        case 'p': g_bag[ITEM_SCRAP] += 200; g_rep += 2; break;
        case 'w':
            for (int k = 0; k < W_COUNT; k++) { g_w[k].owned = 1; g_w[k].mag = w_mag(k); g_w[k].cond = 0.9f; }
            break;
        case 'o':
            for (int k = 0; k < g_ncr; k++) if (!g_cr[k].open) open_crate(&g_cr[k]);
            break;
        case 't': {
            char c = cmd[++i];
            pos = i + 1;
            int k = c >= 'a' ? 10 + c - 'a' : c - '0';
            if (k < 0 || k >= DBG_NTP) break;
            P.hp = 100; P.bleed = 0;
            if (DBG_TP[k][0] != g_zone) zone_go(DBG_TP[k][0], DBG_TP[k][1] * TS + 8, DBG_TP[k][2] * TS + 8, 0);
            else { P.x = DBG_TP[k][1] * TS + 8; P.y = DBG_TP[k][2] * TS + 8; P.lock = -1; }
            break;
        }
        }
    }
}

// Guion de pruebas: el archivo dbg_script (junto a la partida) trae pasos separados por espacios. b<x|o|s|t|u|d|l|r|L|R|S|E> toca un botón, w<ms> espera,
// u<N> espera el estado N (tocando Cruz), z<N> espera estar jugando en la zona N, y el resto son acciones con nombre (ver dbg_act).
// Todo queda anotado en dbg_log.txt con el prefijo T
static int dbg_ms10;   // lo que tarda un cuadro, en décimas de ms (promedio de 30)
static void dbg_logf(const char *fmt, ...)
{
    char ln[500];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ln, sizeof(ln) - 2, fmt, ap);
    va_end(ap);
    strcat(ln, "\n");
    dbg_log_line(ln);
}
static int dbg_tp_near(int tx, int ty)
{
    static const int D[8][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int k = 0; k < 8; k++) {
        int x = tx + D[k][0], y = ty + D[k][1];
        if (in_map(x, y) && !solid(x, y)) { P.x = x * TS + 8; P.y = y * TS + 8; P.lock = -1; return 1; }
    }
    return 0;
}
static float g_dbg_sx, g_dbg_sy;
static u32 g_dbg_hold_b;   // botones que el guion mantiene apretados (pasos H y J)
static void dbg_act(const char *nm)
{
    char name[48];
    int a = 0;
    snprintf(name, sizeof(name), "%s", nm);
    char *eq = strchr(name, '=');
    if (eq) { *eq = 0; a = atoi(eq + 1); }
    if (!strcmp(name, "mat")) dbg_mejoras();
    else if (m3_dbg(name, a)) {}   // [mapas3]
    else if (!strcmp(name, "up")) { for (int k = 0; k < UP_N; k++) g_up[k] = 1; base_apply(1); unstick_player(); bake_lights(); }
    else if (!strcmp(name, "god")) g_dbg_god = a;
    else if (!strcmp(name, "clima")) g_dbg_clima = a;   // [diseno] 0 nada, 1 niebla, 2 luna, 3 lluvia; -1 = por semilla
    else if (!strcmp(name, "mini")) { if (a >= 1 && a <= 4) mj_spawn_one(a - 1, P.x + 70, P.y); }   // [armas2]
    else if (!strcmp(name, "minis")) { int c = 0; for (int i = 0; i < g_ne; i++) if (g_e[i].mini && g_e[i].state != ES_DEAD) c++; dbg_logf("T minis vivos %d (peligro %d)", c, g_x.danger); }   // [armas2]
    else if (!strcmp(name, "hora")) g_hora = (float)a / 100.0f;   // [diseno] hora x100 (1840 = 18:40)
    else if (!strcmp(name, "xgo")) { exped_start(); zone_go(1, -1, -1, 0); }   // [diseno] sale a una expedición nueva
    else if (!strcmp(name, "plaza")) { if (g_plz_x >= 0) { P.x = g_plz_x * TS + 8; P.y = (g_plz_y + 2) * TS + 8; P.lock = -1; } }   // [diseno]
    else if (!strcmp(name, "semaf")) { for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (g_o[y][x] == O_DTL) { P.x = x * TS + 8; P.y = (y + 2) * TS + 8; P.lock = -1; y = MH; break; } }   // [diseno]
    else if (!strcmp(name, "clr")) { g_bag[ITEM_12G_SLUG] = g_bag[ITEM_9MM_AP] = g_bag[ITEM_9MM_HP] = g_bag[ITEM_762_AP] = 0; }
    else if (!strcmp(name, "kill")) { for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) g_e[i].state = ES_DEAD; }
    else if (!strcmp(name, "killboss")) { for (int i = 0; i < g_ne; i++) if (is_boss(&g_e[i]) && g_e[i].state != ES_DEAD) hurt_enemy(&g_e[i], 99999, P.x, P.y, 1); dbg_logf("T killboss: bossdead %d prog %d", g_boss_dead, job_prog()); }
    else if (!strcmp(name, "killban")) { for (int i = 0; i < g_ne; i++) if (g_e[i].type == E_BAN && g_e[i].state != ES_DEAD) hurt_enemy(&g_e[i], 99999, P.x, P.y, 1); dbg_logf("T killban: job %d n %d ready %d", g_job, g_job_n, job_ready()); }
    else if (!strcmp(name, "m")) dbg_logf("T ---- %s", nm + 2);
    else if (!strcmp(name, "take")) { kolos_pick(a - 1); dbg_logf("T take %d: job %d msg [%s]", a, g_job, g_msg); }
    else if (!strcmp(name, "deliv")) { int sc = g_bag[ITEM_SCRAP], r0 = g_rep, j0 = g_job; if (g_job) kolos_pick(g_job - 1); dbg_logf("T deliv job %d: ahora job %d chatarra %d->%d rep %d->%d msg [%s]", j0, g_job, sc, g_bag[ITEM_SCRAP], r0, g_rep, g_msg); }
    else if (!strcmp(name, "tpr2")) { P.x = g_ret2_tx * TS + 8; P.y = g_ret2_ty * TS + 8; P.lock = -1; dbg_logf("T tpr2: suelo %d", ground_at(P.x, P.y)); }
    else if (!strcmp(name, "ms")) {
        int vivos = 0, lejos = 0;
        for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) { vivos++; lejos += fabsf(g_e[i].x - P.x) > FAR_AI * TS || fabsf(g_e[i].y - P.y) > FAR_AI * TS; }
        dbg_logf("T ms: %d.%d ms por cuadro, zona %d %dx%d, enemigos vivos %d (lejos %d), cajones %d, fuegos %d faroles %d", dbg_ms10 / 10, dbg_ms10 % 10, g_zone, MW, MH, vivos, lejos, g_ncr, g_nfire, g_nlamp);
    }
    else if (!strcmp(name, "tpbusy")) {   // al sitio con más enemigos vivos a menos de 20 casillas
        int best = -1, bx = 0, by = 0;
        for (int i = 0; i < g_ne; i++) {
            if (g_e[i].state == ES_DEAD) continue;
            int c = 0;
            for (int k = 0; k < g_ne; k++) c += g_e[k].state != ES_DEAD && fabsf(g_e[k].x - g_e[i].x) < 20 * TS && fabsf(g_e[k].y - g_e[i].y) < 20 * TS;
            if (c > best) { best = c; bx = (int)(g_e[i].x / TS); by = (int)(g_e[i].y / TS); }
        }
        if (best >= 0) dbg_tp_near(bx, by);
        dbg_logf("T tpbusy: %d enemigos cerca de (%d,%d)", best, bx, by);
    }
    else if (!strcmp(name, "walkr") || !strcmp(name, "walkl") || !strcmp(name, "walku") || !strcmp(name, "walkd")) { g_dbg_walk = a / 1000.0f; g_dbg_wx = name[4] == 'r' ? 1 : name[4] == 'l' ? -1 : 0; g_dbg_wy = name[4] == 'd' ? 1 : name[4] == 'u' ? -1 : 0; }
    else if (!strcmp(name, "ehp")) { int best = -1; float bd = 1e9f; for (int i = 0; i < g_ne; i++) if (dist(P.x, P.y, g_e[i].x, g_e[i].y) < bd && g_e[i].ev == 0) { bd = dist(P.x, P.y, g_e[i].x, g_e[i].y); best = i; } dbg_logf("T ehp: enemigo %d hp %d estado %d a %d px", best, best >= 0 ? (int)g_e[best].hp : -1, best >= 0 ? g_e[best].state : -1, (int)bd); }
    else if (!strcmp(name, "gohome")) zone_go(0, 5 * TS + 8, 19 * TS + 8, 1);
    else if (!strcmp(name, "dogto")) dog_to_player();
    else if (!strcmp(name, "dogcl")) { dbg_tp_near((int)(g_dog.x / TS), (int)(g_dog.y / TS)); dbg_logf("T dogcl: %d [%s]", dog_cross(1), g_msg); }
    else if (!strcmp(name, "stairup")) { if (g_nstb > 0) { dbg_tp_near(g_stb_x[0], g_stb_y[0]); stair_go(1, g_stb_x[0], g_stb_y[0]); } dbg_logf("T stairup: escaleras %d", g_nstb); }
    else if (!strcmp(name, "xe0")) g_dbg_xe0 = a;   // evento 1 de la próxima expedición (EV_)
    else if (!strcmp(name, "xe1")) g_dbg_xe1 = a;
    else if (!strcmp(name, "xdog")) g_dbg_xdog = a;
    else if (!strcmp(name, "armas")) { for (int k = ITEM_LONGB; k <= ITEM_REPAIR; k++) g_bag[k] = 2; g_bag[ITEM_SCRAP] = 60; g_w[P.weapon].cond = 0.3f; }   // [armas] pruebas: piezas nuevas, kits y chatarra
    else if (!strcmp(name, "wnear")) { for (int i = 0; i < MAXK; i++) if (g_k[i].on == 3) g_k[i].on = 1; }   // [armas] pruebas: como si te hubieras alejado y vuelto
    else if (!strcmp(name, "wlog")) { int dr = 0; for (int i = 0; i < WDROP_N; i++) dr += g_wdrop[i].used; int on = 0; for (int i = 0; i < MAXK; i++) on += g_k[i].on && g_k[i].item == ITEM_WDROP;
        dbg_logf("T wlog: arma %d cond %d acc %d botadas %d en suelo %d repair %d scrap %d bush %d", P.weapon, (int)(g_w[P.weapon].cond * 100), g_w[P.weapon].part[SLOT_ACC], dr, on, g_bag[ITEM_REPAIR], g_bag[ITEM_SCRAP], is_bush((int)(P.x / TS), (int)(P.y / TS))); }   // [armas]
    else if (!strcmp(name, "vendas")) { g_bag[ITEM_BAND] = 5; g_bag[ITEM_FOOD] = 3; }
    else if (!strcmp(name, "dogtp")) { int ok = g_dogx >= 0 && dbg_tp_near(g_dogx, g_dogy); dbg_logf("T dogtp: %d perro en (%d,%d)", ok, g_dogx, g_dogy); }
    else if (!strcmp(name, "evtp")) { int ok = a < g_nev && dbg_tp_near(g_ev[a].tx, g_ev[a].ty); dbg_logf("T evtp %d: %d tipo %d (%d,%d)", a, ok, a < g_nev ? g_ev[a].kind : -1, a < g_nev ? g_ev[a].tx : 0, a < g_nev ? g_ev[a].ty : 0); }
    else if (!strcmp(name, "evgo")) { for (int i = 0; i < g_nev; i++) if (g_ev[i].kind == EV_HORDE) horde_wake(i); dbg_logf("T evgo: eventos %d", g_nev); }
    else if (!strcmp(name, "evlog")) { for (int i = 0; i < g_nev; i++) dbg_logf("T ev %d: tipo %d (%d,%d) x2 (%d,%d) hallado %d hecho %d", i, g_ev[i].kind, g_ev[i].tx, g_ev[i].ty, g_ev[i].x2, g_ev[i].y2, (g_x.evfound >> i) & 1, (g_x.evdone >> i) & 1); }
    else if (!strcmp(name, "dogcome")) { memset(&g_dog, 0, sizeof(g_dog)); g_dog.have = 1; g_dog.hp = DOG_HP; g_dog.zone = g_zone; dog_to_player(); dbg_logf("T dogcome"); }
    else if (!strcmp(name, "doghurt")) { dog_hurt(a); dbg_logf("T doghurt %d: hp %d herido %d casa %d skip %d", a, (int)g_dog.hp, g_dog.wounded, g_dog.home, g_dog.skip); }
    else if (!strcmp(name, "dogst")) dbg_logf("T dog: tiene %d zona %d (g_zone %d) pos %d,%d jugador %d,%d dist %d hp %d herido %d casa %d skip %d st %d atk %d", g_dog.have, g_dog.zone, g_zone, (int)g_dog.x, (int)g_dog.y, (int)P.x, (int)P.y, (int)dist(g_dog.x, g_dog.y, P.x, P.y), (int)g_dog.hp, g_dog.wounded, g_dog.home, g_dog.skip, g_dog.st, g_dog.atk);
    else if (!strcmp(name, "ordup")) { dog_order(1); dbg_logf("T ordup: st %d [%s]", g_dog.st, g_msg); }
    else if (!strcmp(name, "orddn")) { dog_order(0); dbg_logf("T orddn: atk %d [%s]", g_dog.atk, g_msg); }
    else if (!strcmp(name, "lockn")) { int best = -1; float bd = 1e9f; for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && !is_post(&g_e[i]) && dist(P.x, P.y, g_e[i].x, g_e[i].y) < bd) { bd = dist(P.x, P.y, g_e[i].x, g_e[i].y); best = i; } P.lock = best; dbg_logf("T lockn: %d a %d", best, (int)bd); }
    else if (!strcmp(name, "evcount")) { int a1 = 0, h = 0, g = 0, b = 0; for (int i = 0; i < g_ne; i++) { if (g_e[i].state == ES_DEAD) continue; a1++; h += g_e[i].ev == EVN_HORDE; g += g_e[i].ev == EVN_GUARD; b += g_e[i].ev == EVN_BANDIT; } dbg_logf("T evcount: vivos %d horda %d guardias %d bandidos %d rep %d", a1, h, g, b, g_rep); }
    else if (!strcmp(name, "xd")) g_dbg_xd = a;   // fuerza el peligro de la próxima expedición
    else if (!strcmp(name, "xb")) g_dbg_xb = a;   // fuerza (1) o quita (0) a Cabeza de Hierro
    else if (!strcmp(name, "hold")) g_dbg_hold = a / 1000.0f;   // mantiene el botón de disparo a milisegundos
    else if (!strcmp(name, "wpn")) { if (a >= 0 && a < W_COUNT) { g_w[a].owned = 1; g_w[a].mag = w_mag(a); g_w[a].cond = 0.9f; P.weapon = a; P.jammed = 0; P.reload_t = 0; } }
    else if (!strcmp(name, "ammo")) { for (int k = 0; k < ITEM_COUNT; k++) if (ITEMS[k].cap && gear_of(k) < 0) g_bag[k] = ITEMS[k].cap; }
    else if (!strcmp(name, "hand")) { g_hand = a; g_bag[HAND_ITEM[a]] = 5; }   // elige el objeto de mano a (y lo llena)
    else if (!strcmp(name, "throw")) hand_use();
    else if (!strcmp(name, "sneak")) g_dbg_sneak = a;
    else if (!strcmp(name, "boomfx")) g_dbg_boomfx = a;
    else if (!strcmp(name, "pingart")) {   // junto al primer artefacto del suelo, y las anomalías de al lado reaccionan como a una tuerca
        for (int i = 0; i < MAXK; i++)
            if (g_k[i].on && art_idx(g_k[i].item) >= 0) { anom_ping(g_k[i].x, g_k[i].y); dbg_tp_near((int)(g_k[i].x / TS) - 2, (int)(g_k[i].y / TS)); P.ang = 0; break; }
    }
    else if (!strcmp(name, "hurt")) { P.hp -= a; }
    else if (!strcmp(name, "kl")) dbg_logf("T kl: bajas %d", g_kills);
    else if (!strcmp(name, "ankind")) { for (int i = 0; i < g_nan; i++) g_an[i].kind = a; }   // todas las anomalías de esa clase
    else if (!strcmp(name, "stl")) {   // golpe sigiloso: un enemigo de tipo a quieto y de espaldas, 25 px delante, y el cuchillo en mano
        float ca = cosf(P.ang), sa = sinf(P.ang);
        g_w[W_KNIFE].owned = 1; P.weapon = W_KNIFE; P.lock = -1; P.fire_cd = 0;
        spawn_enemy(a, P.x + ca * 25, P.y + sa * 25);
        Enemy *e = &g_e[g_ne - 1];
        e->state = ES_IDLE; e->ang = P.ang; e->tx = e->hx = e->x; e->ty = e->hy = e->y; e->t = 99;
        dbg_logf("T stl: tipo %d hp %.0f marca %d", a, e->hp, stealth_ok(e));
    }
    else if (!strcmp(name, "art")) { int gi = gear_of(ART_ITEM(a)); g_bag[GEAR[gi].item] = 1; gear_equip(gi); dbg_logf("T art %d: puesto %d/%d/%d vida_max %.0f dano %.2f recibido %.2f vel %.2f", a, g_eq[EQ_ART1], g_eq[EQ_ART2], g_eq[EQ_ART3], hp_max(), art_dmg(), art_taken(), art_speed()); }
    else if (!strcmp(name, "uq")) { uniq_grant(a); dbg_logf("T uq %d: mask %d n %d", a, g_uniq, uq_n()); }   // [artef]
    else if (!strcmp(name, "radset")) P.rad = (float)a;   // [artef]
    else if (!strcmp(name, "det")) { g_bag[ITEM_DET] = 1; gear_equip(4); }
    else if (!strcmp(name, "peso")) dbg_logf("T peso: %.2f/%.1f pct %.0f arma %d mak %d cuch %d flags %d exped %d cajon %d", peso_total(), peso_cap(), peso_pct(), P.weapon, g_w[W_MAKAROV].owned, g_w[W_KNIFE].owned, g_peso_flags, g_xtotal, peso_first_exped());   // [peso]
    else if (!strcmp(name, "pcarga")) { int t = inv_add(ITEM_MECH, a); dbg_logf("T pcarga: entraron %d de %d", t, a); }   // [peso]
    else if (!strcmp(name, "pkg")) { int r = peso_crate_gun(P.x, P.y); dbg_logf("T pkg: %d mak %d mag %d cond %.2f 9mm %d", r, g_w[W_MAKAROV].owned, g_w[W_MAKAROV].mag, g_w[W_MAKAROV].cond, g_bag[ITEM_9MM]); }   // [peso]
    else if (!strcmp(name, "parm")) { peso_arm_toggle(a); dbg_logf("T parm %d: llevada %d guardada %d n %d", a, g_w[a].owned, g_arm[a].owned, peso_carried()); }   // [peso]
    else if (!strcmp(name, "unart")) { g_eq[EQ_ART1] = g_eq[EQ_ART2] = g_eq[EQ_ART3] = 0; }
    else if (!strcmp(name, "tpan")) {   // al borde de la primera anomalía de la clase a (3 casillas al oeste)
        int ok = 0;
        for (int i = 0; i < g_nan && !ok; i++) if (g_an[i].kind == a) { P.x = g_an[i].x - g_an[i].r - 14; P.y = g_an[i].y; P.lock = -1; ok = 1; P.ang = 0; }
        dbg_logf("T tpan %d: %d (campos %d anomalias %d)", a, ok, g_nfield, g_nan);
    }
    else if (!strcmp(name, "inan")) { for (int i = 0; i < g_nan; i++) if (g_an[i].kind == a) { P.x = g_an[i].x; P.y = g_an[i].y; P.lock = -1; break; } }
    else if (!strcmp(name, "ean")) {   // un enemigo (tipo E_MAR) en el centro de la primera anomalía de la clase a
        for (int i = 0; i < g_nan; i++) if (g_an[i].kind == a) { spawn_enemy(E_MAR, g_an[i].x + 3, g_an[i].y); g_e[g_ne - 1].hp = 90; g_e[g_ne - 1].state = ES_ALERT; g_e[g_ne - 1].tx = g_an[i].x; g_e[g_ne - 1].ty = g_an[i].y; break; }
    }
    else if (!strcmp(name, "anlog")) {
        int arts = 0, vis = 0;
        for (int i = 0; i < MAXK; i++) arts += g_k[i].on && art_idx(g_k[i].item) >= 0;
        for (int i = 0; i < g_nan; i++) vis += g_an[i].vis > 0;
        dbg_logf("T anlog: delatadas %d | campos %d anomalias %d artefactos en el suelo %d | hp %.1f rad %.1f aturdido %.2f hambre %.1f sed %.1f", vis, g_nfield, g_nan, arts, P.hp, P.rad, g_pstun, P.hunger, P.thirst);
    }
    else if (!strcmp(name, "pstat")) dbg_logf("T pstat: hp %.1f rad %.1f frio %.1f sangrado %.2f aguante %.1f hambre %.1f sed %.1f vel %.2f", P.hp, P.rad, g_cold, P.bleed, P.stam, P.hunger, P.thirst, 1.0f);
    else if (!strcmp(name, "face")) P.ang = a * PI / 180;   // grados
    else if (!strcmp(name, "spawn")) {   // un enemigo delante del jugador (a 34 px, o a 60 con spawn60)
        float ca = cosf(P.ang), sa = sinf(P.ang);
        for (int dd = 34; dd >= 14; dd -= 4) if (!box_hits(P.x + ca * dd, P.y + sa * dd, 6)) { spawn_enemy(a, P.x + ca * dd, P.y + sa * dd); g_e[g_ne - 1].state = ES_COMBAT; g_e[g_ne - 1].tx = P.x; g_e[g_ne - 1].ty = P.y; break; }
        dbg_logf("T spawn %d: enemigos %d", a, g_ne);
    }
    else if (!strcmp(name, "wallsetup")) {   // se para a 5 casillas de un muro, mirándolo, con un enemigo (tipo a, 300 de vida) entre medias
        static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
        int ok = 0;
        for (int ty = 4; ty < MH - 4 && !ok; ty++)
            for (int tx = 4; tx < MW - 4 && !ok; tx++)
                for (int dr = 0; dr < 4 && !ok; dr++) {
                    int free_ok = 1;
                    for (int k = 0; k <= 4; k++) if (solid(tx + DX[dr] * k, ty + DY[dr] * k) || solid(tx + DX[dr] * k + DY[dr], ty + DY[dr] * k + DX[dr]) || solid(tx + DX[dr] * k - DY[dr], ty + DY[dr] * k - DX[dr])) free_ok = 0;
                    if (!free_ok || !solid(tx + DX[dr] * 5, ty + DY[dr] * 5)) continue;
                    P.x = tx * TS + 8; P.y = ty * TS + 8; P.ang = dr * PI / 2; P.lock = -1; P.hp = 100;
                    spawn_enemy(a, P.x + DX[dr] * 34, P.y + DY[dr] * 34);
                    Enemy *e = &g_e[g_ne - 1];
                    e->hp = 300; e->state = ES_COMBAT; e->tx = P.x; e->ty = P.y; e->stun = 0.01f;
                    ok = 1;
                    dbg_logf("T wallsetup: casilla %d,%d dir %d, muro a %d px del enemigo", tx, ty, dr, 5 * TS - 34 - 8);
                }
        if (!ok) dbg_logf("T wallsetup: sin sitio");
    }
    else if (!strcmp(name, "ld")) { int r = load_game(); g_state = GS_PLAY; dbg_logf("T ld: %d zona %d", r, g_zone); }   // vuelve a cargar lo guardado
    else if (!strcmp(name, "tank")) { for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && !is_boss(&g_e[i]) && dist(P.x, P.y, g_e[i].x, g_e[i].y) < 160) g_e[i].hp = 300; }   // los cercanos aguantan (para ver los empujones)
    else if (!strcmp(name, "fe")) {   // log de los enemigos vivos cercanos: tipo, vida, distancia y empuje
        for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && dist(P.x, P.y, g_e[i].x, g_e[i].y) < 140) dbg_logf("T fe %d tipo %d hp %.0f dx %.0f dy %.0f stun %.2f k %.0f | golpes %d choques %d (vel %.0f)", i, g_e[i].type, g_e[i].hp, g_e[i].x - P.x, g_e[i].y - P.y, g_e[i].stun, sqrtf(g_e[i].kx * g_e[i].kx + g_e[i].ky * g_e[i].ky), g_dbg_swhits, g_dbg_walls, g_dbg_wsp);
    }
    else if (!strcmp(name, "tpe")) {   // junto al primer enemigo vivo del tipo a (y lo deja en combate)
        int ok = 0;
        for (int i = 0; i < g_ne && !ok; i++) if (g_e[i].type == a && g_e[i].state != ES_DEAD) ok = dbg_tp_near((int)(g_e[i].x / TS), (int)(g_e[i].y / TS));
        dbg_logf("T tpe %d: %d", a, ok);
    }
    else if (!strcmp(name, "kind")) { int n[E_TYPES] = {0}; for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) n[g_e[i].type]++; dbg_logf("T kind zona %d peligro %d: chico %d hacha %d grande %d bandido %d guardia %d jefe %d corredor %d escupidor %d acorazado %d franco %d lobo %d jefe2 %d", g_zone, g_x.danger, n[E_DOG], n[E_MAR], n[E_BIG], n[E_BAN], n[E_GRD], n[E_BOSS], n[E_RUN], n[E_SPIT], n[E_ARM], n[E_SNP], n[E_WOLF], n[E_BOSS2]); }
    else if (!strcmp(name, "killall")) { for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && !is_boss(&g_e[i])) hurt_enemy(&g_e[i], 99999, P.x, P.y, 1); }
    else if (!strcmp(name, "tpnave") || !strcmp(name, "tppor")) {   // [fab2] a la nave numero a con pasarela (adentro) o frente a su porton de abajo (afuera)
        int k = -1, n = 0, ok = 0;
        for (int b = 0; b < g_x_nbld && k < 0; b++) if (x_b[b].y1 - x_b[b].y0 >= 6 && ar_pasa((x_b[b].x0 + x_b[b].x1) / 2, x_b[b].y0 + 3) && n++ == a) k = b;
        if (k >= 0 && name[2] == 'n') ok = dbg_tp_near((x_b[k].x0 + x_b[k].x1) / 2, x_b[k].y0 + 4);
        else if (k >= 0) for (int x = x_b[k].x0; x <= x_b[k].x1 && !ok; x++) if (g_g[x_b[k].y1][x] == G_DOOR) ok = dbg_tp_near(x, x_b[k].y1 + 2);
        dbg_logf("T %s %d: nave %d ok %d (%d,%d)-(%d,%d)", name, a, k, ok, k >= 0 ? x_b[k].x0 : 0, k >= 0 ? x_b[k].y0 : 0, k >= 0 ? x_b[k].x1 : 0, k >= 0 ? x_b[k].y1 : 0);
    }
    else if (!strcmp(name, "tptub")) {   // [fab2] bajo las tuberias elevadas numero a, contando desde el centro del mapa
        int ok = 0, n = 0;
        for (int y = MH / 3; y < MH - 2 && !ok; y++) for (int x = 3; x < MW - 3 && !ok; x++)
            if (g_g[y][x] == G_ROAD && g_g[y][x + 1] == G_ROAD && g_g[y][x - 1] != G_ROAD && g_g[y][x + 2] != G_ROAD && y % 9 == 4 && n++ == a) ok = dbg_tp_near(x, y + 2);
        dbg_logf("T tptub %d: %d", a, ok);
    }
    else if (!strcmp(name, "xt")) g_dbg_xt = a;
    else if (!strcmp(name, "xs")) g_dbg_xs = a;
    else if (!strcmp(name, "cold")) g_cold = (float)a;
    else if (!strcmp(name, "zpan")) { g_job = 5; g_job_n = 0; g_boss_dead = 0; exped_start(); dbg_logf("T zpan: flags %d tipo %d", g_x.flags, g_x.type); }   // [jefes] directo al Pantano
    else if (!strcmp(name, "zbase")) { g_job = 8; g_job_n = 0; exped_start(); dbg_logf("T zbase: flags %d tipo %d", g_x.flags, g_x.type); }   // [jefes] directo a la Base militar
    else if (!strcmp(name, "zlago")) { g_fjob = 4; g_fjn = 0; exped_start(); dbg_logf("T zlago: flags %d tipo %d", g_x.flags, g_x.type); }   // [jefes2]
    else if (!strcmp(name, "zfuerte")) { g_job = 9; g_job_n = 0; exped_start(); dbg_logf("T zfuerte: flags %d tipo %d", g_x.flags, g_x.type); }   // [jefes2]
    else if (!strcmp(name, "zvert")) { g_job = 10; g_job_n = 0; exped_start(); dbg_logf("T zvert: flags %d tipo %d", g_x.flags, g_x.type); }   // [jefes2]
    else if (!strcmp(name, "jzgo")) { for (int i = 0; i < g_ne; i++) if (g_e[i].type >= E_VIUDA && g_e[i].state != ES_DEAD) { P.x = g_e[i].x - (float)a; P.y = g_e[i].y; dbg_logf("T jzgo: jefe %d hp %.0f estado %d atk %d P %.0f,%.0f", g_e[i].type, g_e[i].hp, g_e[i].state, g_e[i].atk, P.x, P.y); break; } }   // [jefes2] al lado del jefe
    else if (!strcmp(name, "jzinfo")) { int n = 0, outs = 0; for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) { n++; if (g_e[i].type >= E_VIUDA) dbg_logf("T jzinfo: jefe %d hp %.0f var %d estado %d atk %d en %.0f,%.0f", g_e[i].type, g_e[i].hp, g_e[i].var, g_e[i].state, g_e[i].atk, g_e[i].x, g_e[i].y); } dbg_logf("T jzinfo: vivos %d zona %d flags %d portón %d trabajo %d/%d faro %d/%d hp %.0f rad %.1f", n, g_zone, g_x.flags, jz_open, g_job, g_job_n, g_fjob, g_fjn, P.hp, P.rad); (void)outs; }   // [jefes2]
    else if (!strcmp(name, "exp")) { exped_start(); dbg_logf("T exp: num %d tipo %d peligro %d flags %d", g_x.num, g_x.type, g_x.danger, g_x.flags); }
    else if (!strcmp(name, "info")) {
        int boss = 0, ban = 0, crj = 0, crm = 0, hat = 0, stairs = 0;
        for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) { boss += g_e[i].type == E_BOSS; ban += g_e[i].type == E_BAN; }
        for (int i = 0; i < g_ncr; i++) { crj += g_cr[i].kind == CK_JOB; crm += g_cr[i].kind == CK_MEDS; }
        for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) { hat += g_o[y][x] == O_HATCH; stairs += g_o[y][x] == O_STAIRS; }
        dbg_logf("T info zona %d st %d: bestia %d bandidos %d cajonJOB %d cajonMEDS %d escotilla %d escalera %d | job %d prog %d listo %d has_job %d has_meds %d", g_zone, g_state, boss, ban, crj, crm, hat, stairs, g_job, g_job ? job_prog() : -1, g_job ? job_ready() : 0, P.has_job, g_has_meds);
    }
    else if (!strcmp(name, "tpc")) { int ok = 0; for (int i = 0; i < g_ncr && !ok; i++) if (g_cr[i].kind == a) ok = dbg_tp_near(g_cr[i].tx, g_cr[i].ty); dbg_logf("T tpc %d: %d", a, ok); }
    else if (!strcmp(name, "tpo")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == a) ok = dbg_tp_near(x, y); dbg_logf("T tpo %d: %d", a, ok); }
    else if (!strcmp(name, "tpr")) { P.x = g_ret_tx * TS + 8; P.y = g_ret_ty * TS + 8; P.lock = -1; dbg_logf("T tpr: suelo %d", ground_at(P.x, P.y)); }
    else if (!strcmp(name, "tpin") || !strcmp(name, "tpout")) {
        int want = !strcmp(name, "tpin"), ok = 0, nb = 0, nin = 0;
        for (int b = 0; b < g_x_nbld; b++) {
            int cx = (x_b[b].x0 + x_b[b].x1) / 2, cy = (x_b[b].y0 + x_b[b].y1) / 2;
            nb++; nin += indoors(cx * TS + 8, cy * TS + 8);
        }
        for (int y = 3; y < MH - 3 && !ok; y++)
            for (int x = 3; x < MW - 3 && !ok; x++) {
                if (solid(x, y) || indoors(x * TS + 8, y * TS + 8) != want) continue;
                int all = 1;
                if (want) { for (int k = 0; k < 4; k++) all &= indoors((x + (k == 0) - (k == 1)) * TS + 8, (y + (k == 2) - (k == 3)) * TS + 8); }
                else { if (g_g[y][x] == G_RAD || g_g[y][x] == G_ZONE) continue; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) all &= !indoors((x + i) * TS + 8, (y + j) * TS + 8) && g_g[y + j][x + i] != G_RAD; }
                if (!all) continue;
                P.x = x * TS + 8; P.y = y * TS + 8; P.lock = -1; ok = 1;
            }
        dbg_logf("T %s: %d (edificios %d con centro bajo techo %d) en %d,%d", name, ok, nb, nin, (int)P.x / TS, (int)P.y / TS);
    }
    else if (!strcmp(name, "kol")) { kolos_open(KW_COM | KW_STASH | KW_SCAV); g_kol_tab = a; }   // [qsist] pruebas: abre la pestaña a de la base
    else if (!strcmp(name, "dped")) g_dped_done = a;   // [qsist] pruebas
    else if (!strcmp(name, "qlog")) dbg_logf("T qlog: zona %d peso %.2f/%.1f art %d/%d/%d hpmax %.0f bomba %.1f hora %.2f dped %d armero %d,%d,%d,%d dret %d fuel %d filt %d uniq %d armas %d%d%d%d", g_zone, peso_total(), peso_cap(), g_eq[EQ_ART1], g_eq[EQ_ART2], g_eq[EQ_ART3], hp_max(), g_bomba_t, g_hora, g_dped_done, g_arm[0].owned, g_arm[1].owned, g_arm[2].owned, g_arm[3].owned, g_dret, g_dfuel, g_dfilt, g_uniq, g_w[0].owned, g_w[1].owned, g_w[2].owned, g_w[3].owned);   // [qsist] pruebas
    else if (!strcmp(name, "bomba")) g_bomba_t = a;   // [bomba] deja la cuenta en a segundos
    else if (!strcmp(name, "bmb")) dbg_logf("T bomba %.1f zona %d estado %d hp %.0f", g_bomba_t, g_zone, g_state, P.hp);
    else if (!strcmp(name, "storm")) { g_storm_on = a; g_storm_left = 999; g_storm_t = 999; g_storm_warned = 1; }
    else if (!strcmp(name, "hp")) { P.hp = 100; P.rad = 0; P.bleed = 0; }
    else if (!strcmp(name, "hpl")) dbg_logf("T hp %.1f rad %.1f techo %d bomba %d dios %d", P.hp, P.rad, indoors(P.x, P.y), (int)g_bomba_t, g_dbg_god);
    else if (!strcmp(name, "away")) { g_dbg_sx = P.x; g_dbg_sy = P.y; if (!dbg_tp_near((int)(P.x / TS) + 3, (int)(P.y / TS))) dbg_tp_near((int)(P.x / TS) - 3, (int)(P.y / TS)); }
    else if (!strcmp(name, "back")) { P.x = g_dbg_sx; P.y = g_dbg_sy; }
    else if (!strcmp(name, "food")) g_bag[ITEM_FOOD] = a;
    else if (!strcmp(name, "bag")) {
        int suelo = 0, bot = 0;
        for (int i = 0; i < MAXK; i++) if (g_k[i].on) { suelo++; bot += g_k[i].on == 3; }
        dbg_logf("T bag: comida %d agua %d chatarra %d slug %d 9ap %d 9hp %d 762ap %d polvo %d mecan %d | alijo comida %d agua %d | suelo %d botados %d | zona %d st %d tab %d col %d sel %d | msg [%s]",
                 g_bag[ITEM_FOOD], g_bag[ITEM_WATER], g_bag[ITEM_SCRAP], g_bag[ITEM_12G_SLUG], g_bag[ITEM_9MM_AP], g_bag[ITEM_9MM_HP], g_bag[ITEM_762_AP], g_bag[ITEM_POWDER], g_bag[ITEM_MECH],
                 g_stash[ITEM_FOOD], g_stash[ITEM_WATER], suelo, bot, g_zone, g_state, g_kol_tab, g_stash_col, g_stash_sel, g_msg);
    }
    else if (!strcmp(name, "sv")) dbg_logf("T sv: %d zona %d exped %d seed %u pos %d,%d cajones %d enem %d comida %d", save_game(), g_zone, g_x.num, (unsigned)g_x.seed, (int)P.x, (int)P.y, g_ncr, g_ne, g_bag[ITEM_FOOD]);
    else if (!strcmp(name, "cmp")) dbg_logf("T cmp: zona %d exp %d seed %u pos %d,%d perro %d/%d/%d/%d hp %d eq %d,%d,%d,%d,%d mano %d granadas %d sk %d%d%d%d%d%d%d%d%d notas %d rep %d frep %d flags %d evf %d evd %d ev0 %d ev1 %d fx %d,%d arma %d", g_zone, g_x.num, (unsigned)g_x.seed, (int)P.x, (int)P.y, g_dog.have, g_dog.zone, g_dog.home, g_dog.st, (int)g_dog.hp, g_eq[0], g_eq[1], g_eq[2], g_eq[3], g_eq[4], g_hand, g_bag[HAND_ITEM[g_hand]], g_sk[0], g_sk[1], g_sk[2], g_sk[3], g_sk[4], g_sk[5], g_sk[6], g_sk[7], g_sk[8], g_notes, g_rep, g_frep, g_x.flags, g_x.evfound, g_x.evdone, g_x.ev0, g_x.ev1, g_fx, g_fy, P.weapon);
    else if (!strcmp(name, "state")) dbg_logf("T state: st %d zona %d exped %d seed %u pos %d,%d cajones %d enem %d comida %d vida %.0f modo %d", g_state, g_zone, g_x.num, (unsigned)g_x.seed, (int)P.x, (int)P.y, g_ncr, g_ne, g_bag[ITEM_FOOD], P.hp, g_mode);
    else if (!strcmp(name, "die")) hurt_player(999);
    else if (!strcmp(name, "bkgo")) { g_bag[ITEM_SCRAP] = 400; g_bag[ITEM_MECH] = 8; g_bag[ITEM_RAG] = 8; g_bag[ITEM_BKD_PLANT] = 2; zone_go(ZB, 0, 0, 1); }   // [bunker] pruebas: baja al búnker con materiales
    else if (!strcmp(name, "bkdoor")) { P.x = 27 * TS + 8; P.y = 19 * TS + 8; P.lock = -1; }                                                                      // [bunker] pruebas: junto a la puerta del almacén
    else if (!strcmp(name, "bkbox")) { P.x = 28 * TS + 8; P.y = 21 * TS + 4; P.lock = -1; P.ang = 1.5708f; }                                                      // [bunker] pruebas: frente a una caja
    else if (!strcmp(name, "bkpos")) dbg_logf("T bk: zona %d st %d salas %d cajas %d/%d planta %d msg [%s]", g_zone, g_state, g_bk_rooms, bk_box_count(), bk_box_cap(), g_bag[ITEM_BKD_PLANT], g_msg);
    else if (!strcmp(name, "bunk")) {   // 200 búnkeres: cada uno tiene que traer su caja de medicinas y el registro
        u32 keep = g_bseed;
        int sin_m = 0, sin_q = 0;
        for (int i = 0; i < 200; i++) {
            g_bseed = ((u32)(i + 1) * 0x9E3779B1u) | 1;
            zone_build(2);
            int m = 0, q = 0;
            for (int c = 0; c < g_ncr; c++) { m += g_cr[c].kind == CK_MEDS; q += g_cr[c].kind == CK_REG; }
            sin_m += !m; sin_q += !q;
        }
        g_bseed = keep;
        zone_build(g_zone);
        dbg_logf("T bunk 200: sin medicinas %d sin registro %d", sin_m, sin_q);
    }
    else if (!strcmp(name, "killz")) { int n = 0; for (int i = 0; i < g_ne && n < a; i++) if (g_e[i].state != ES_DEAD && !is_human(&g_e[i]) && g_e[i].type != E_BOSS) { hurt_enemy(&g_e[i], 99999, P.x, P.y, 1); n++; } dbg_logf("T killz %d: job %d n %d ready %d kills %d", n, g_job, g_job_n, job_ready(), g_kills); }
    else if (!strcmp(name, "skpt")) dbg_logf("T skpt: puntos %d rango %d xtotal %d compradas %d%d%d%d%d%d%d%d%d cupo 9mm %d", sk_points(), rank(), g_xtotal, g_sk[0], g_sk[1], g_sk[2], g_sk[3], g_sk[4], g_sk[5], g_sk[6], g_sk[7], g_sk[8], item_cap(ITEM_9MM));
    else if (!strcmp(name, "skbuy")) { int p0 = sk_points(), c0 = item_cap(ITEM_9MM); g_sk_sel = a; sk_buy(); dbg_logf("T skbuy %d: puntos %d->%d cupo 9mm %d->%d msg [%s]", a, p0, sk_points(), c0, item_cap(ITEM_9MM), g_msg); }
    else if (!strcmp(name, "invtab")) { if (a < 0) g_state = GS_PLAY; else { g_inv_tab = a; g_state = GS_INV; } }
    else if (!strcmp(name, "xfaro")) g_dbg_faro = a;
    else if (!strcmp(name, "xnum")) g_xtotal = a;
    else if (!strcmp(name, "tpfaro")) { if (g_fx >= 0) { P.x = g_fx * TS + 8; P.y = (g_fy + 1) * TS + 8; P.lock = -1; } dbg_logf("T tpfaro: puesto %d,%d zona %d flags %d evfound %d", g_fx, g_fy, g_zone, g_x.flags, g_x.evfound); }
    else if (!strcmp(name, "faropen")) { faro_open(); dbg_logf("T faropen: estado %d msg [%s]", g_state, g_msg); }
    else if (!strcmp(name, "scrap")) g_bag[ITEM_SCRAP] += a;
    else if (!strcmp(name, "giveart")) g_bag[ART_ITEM(a)] = 1;
    else if (!strcmp(name, "farol")) { int nf = 0, nh = 0; for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && g_e[i].ev == EVN_FARO) { nf += g_e[i].fac == F_FARO; nh += g_e[i].fac == F_BAND; } dbg_logf("T farol: amigos %d hostiles %d frep %d trabajo %d avance %d hechos %d chatarra %d evdone %d art0 %d msg [%s]", nf, nh, g_frep, g_fjob, g_fjn, g_fdone, g_bag[ITEM_SCRAP], g_x.evdone, g_bag[ITEM_ART0], g_msg); }
    else if (!strcmp(name, "fjob")) { faro_job(a - 1); dbg_logf("T fjob %d: trabajo %d avance %d hechos %d frep %d chatarra %d msg [%s]", a, g_fjob, g_fjn, g_fdone, g_frep, g_bag[ITEM_SCRAP], g_msg); }
    else if (!strcmp(name, "shootf")) {
        for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD && g_e[i].fac == F_FARO) {
            for (int q = 0; q < MAXB; q++) if (!g_b[q].on) { g_b[q] = (Bullet){g_e[i].x, g_e[i].y, 1, 0, 1, 1, 1, 1, 0, F_KOLOS, 0, 0}; break; }
            break;
        }
    }
    else if (!strcmp(name, "ffield")) { for (int i = 0; i < g_nan; i++) if (g_an[i].r > 0) { P.x = g_an[i].x - g_an[i].r - 4; P.y = g_an[i].y; P.lock = -1; break; } g_bag[ITEM_RAG] = 2; g_bag[ITEM_CHEM] = 1; int u = farol_field_try(); dbg_logf("T ffield: usado %d avance %d campos vivos %d msg [%s]", u, g_fjn, g_nan, g_msg); }
    else if (!strcmp(name, "fsnp")) { spawn_enemy(E_SNP, P.x + 40, P.y); hurt_enemy(&g_e[g_ne - 1], 9999, P.x, P.y, 1); dbg_logf("T fsnp: avance %d msg [%s]", g_fjn, g_msg); }
    else if (!strcmp(name, "fin")) { g_state = GS_ENDSEL; g_end_sel = a; g_state_t = 0; }
    else if (!strcmp(name, "fie")) dbg_logf("T fie: estado %d final %d sel %d frep %d notas %d msg [%s]", g_state, g_ending, g_end_sel, g_frep, g_notes, g_msg);
    else if (!strcmp(name, "frepset")) g_frep = a;
    else if (!strcmp(name, "rep")) { g_rep = a; dbg_logf("T rep %d: rango %d", g_rep, rank()); }
    else if (!strcmp(name, "pozo")) { g_pz_to = a; g_pz_dir = -1; zone_go(2, 0, 0, 0); dbg_logf("T pozo: piso %d", a); }   // [pozo]
    else if (!strcmp(name, "fin")) { g_fin_seen = 1; g_pz_inf = a; dbg_logf("T fin: visto, inf %d", a); }   // [pozo]
    else if (!strcmp(name, "pzst")) dbg_logf("T pzst: ms %d.%d zona %d piso %d lift %d bajada %d,%d ascensor %d,%d ne %d inf %d peligro %d", dbg_ms10 / 10, dbg_ms10 % 10, g_zone, g_pz, g_pz_lift, g_pz_dx, g_pz_dy, g_pz_lx, g_pz_ly, g_ne, g_pz_inf, g_x.danger);   // [pozo]
    else if (!strcmp(name, "tphatch")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == O_HATCH) ok = dbg_tp_near(x, y); dbg_logf("T tphatch: %d", ok); }
    else if (!strcmp(name, "tpstairs")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == O_STAIRS) ok = dbg_tp_near(x, y); dbg_logf("T tpstairs: %d", ok); }
    else if (!strcmp(name, "tpo")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == a) ok = dbg_tp_near(x, y); dbg_logf("T tpo %d: %d zona %d", a, ok, g_zone); }   // salta junto al primer objeto de ese tipo (O_)
    else if (!strcmp(name, "hsup")) { if (g_nhs > 0) { int k = a % g_nhs; dbg_tp_near(g_hs_x[k], g_hs_y[k]); stair_go(1, g_hs_x[k], g_hs_y[k]); } dbg_logf("T hsup: casas %d", g_nhs); }   // [mapas] sube por la escalera de una casa (a = cuál)
    else if (!strcmp(name, "hsdn")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == O_STDN) { dbg_tp_near(x, y); stair_go(-1, x, y); ok = 1; } dbg_logf("T hsdn: %d", ok); }   // [mapas] baja por la escalera de este piso
    else if (!strcmp(name, "hsst")) { int op = 0, de = 0; for (int i = 0; i < g_ncr; i++) op += g_cr[i].open; for (int i = 0; i < g_ne; i++) de += g_e[i].state == ES_DEAD; dbg_logf("T hsst: zona %d casa %d cajones %d (abiertos %d) enemigos %d (muertos %d) mascaras %d/%d", g_zone, g_hs_cur, g_ncr, op, g_ne, de, g_hs_open[g_hs_cur], g_hs_dead[g_hs_cur]); }   // [mapas]
    else if (!strcmp(name, "mtgo")) { if (g_zone == 1 && g_nmt > 0) { int k = (a % 10) % g_nmt; dbg_tp_near(g_mt_x[k], g_mt_y[k]); if (a < 10) stair_go(1, g_mt_x[k], g_mt_y[k]); }  dbg_logf("T mtgo: bocas %d", g_nmt); }   // [urbe] baja por la boca a
    else if (!strcmp(name, "mtup")) { stair_go(-1, g_mt_sx[0], g_mt_sy[0]); }   // [urbe] sube
    else if (!strcmp(name, "mttren")) { int best = 0; for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) if (g_o[y][x] == O_NONE && g_g[y][x] == G_DOOR) { best = 1; P.x = (x - 3) * TS + 8; P.y = y * TS + 8; y = MH; break; } dbg_logf("T mttren: %d", best); }   // [urbe] al primer vagón
    else if (!strcmp(name, "mtplat")) { P.x = (g_mt_sx[a % g_mt_ns] + 3) * TS + 8; P.y = (g_mt_sy[a % g_mt_ns] + 2) * TS + 8; }   // [urbe] al andén de la estación a
    else if (!strcmp(name, "stup")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == O_STUP) { dbg_tp_near(x, y); stair_go(1, x, y); ok = 1; } dbg_logf("T stup: %d", ok); }   // sube por la primera escalera de subir
    else if (!strcmp(name, "duro")) { for (int i = ITEM_D_HERR; i <= ITEM_D_FILTRO; i++) g_bag[i] = 6; g_bk_rooms |= 128; g_dfuel = 1; g_dfilt = 1; dbg_logf("T duro: modo %d", g_mode); }   // [duro] pruebas
    else if (!strcmp(name, "duroret")) { duro_return(); dbg_logf("T duroret: [%s] [%s] luz %d bidones %d filtro %d/%d agua alijo %d", g_dret_l1, g_dret_l2, g_dlit, g_dfuel, g_dfilt, g_dfilt_left, g_stash[ITEM_WATER]); }   // [duro] pruebas
    else if (!strcmp(name, "duroinfo")) dbg_logf("T duroinfo: hechos %d chatarra %d rep %d herr %d", g_dped_done, g_bag[ITEM_SCRAP], g_rep, g_bag[ITEM_D_HERR]);   // [duro] pruebas
    else if (!strcmp(name, "tpmerch")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_o[y][x] == O_MERCH) ok = dbg_tp_near(x, y); dbg_logf("T tpmerch: %d zona %d", ok, g_zone); }
    else if (!strcmp(name, "tpexit")) { int ok = 0; for (int y = 0; y < MH && !ok; y++) for (int x = 0; x < MW && !ok; x++) if (g_g[y][x] == (g_zone == 0 ? G_EXIT : G_ZONE)) { P.x = x * TS + 8; P.y = y * TS + 8; P.lock = -1; ok = 1; } dbg_logf("T tpexit: %d zona %d", ok, g_zone); }
    else if (!strcmp(name, "st")) dbg_logf("T st: st %d zona %d job %d prog %d rep %d rango %d chatarra %d reg %d fin %d hechos %d gan %d msg [%s]", g_state, g_zone, g_job, g_job ? job_prog() : -1, g_rep, rank(), g_bag[ITEM_SCRAP], g_has_reg, g_fin_seen, g_jobs_done, g_scrap_gain, g_msg);
    else if (!strcmp(name, "nojob")) { g_job = g_job_n = 0; P.has_job = 0; g_has_meds = 0; g_job_done1 = g_job_done5 = g_job_done6 = 0; }
    else if (st_dbg(name, a)) {}   // [stalk] stobj=O
    else dbg_logf("T ??? %s", nm);
}
static void dbg_script_run(float dt)
{
    static char sc[2048];
    static int loaded, pos, n;
    in.held |= g_dbg_hold_b;
    static float wt, to, press_t;
    static int wmode, warg;
    if (!loaded) {
        loaded = 1;
        char path[170];
        snprintf(path, sizeof(path), "%s", g_sav);
        char *sl = strrchr(path, '/');
        if (!sl) return;
        strcpy(sl + 1, "dbg_script");
        SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
        if (fd < 0) return;
        n = sceIoRead(fd, sc, sizeof(sc) - 1);
        sceIoClose(fd);
        if (n <= 0) { n = 0; return; }
        sc[n] = 0;
        dbg_logf("T script cargado, %d bytes", n);
    }
    if (pos >= n) return;
    if (wmode) {   // espera de estado
        to += dt; press_t += dt;
        int ok = wmode == 1 ? g_state == warg : (g_state == GS_PLAY && g_zone == warg);
        if (ok) { wmode = 0; wt = 0.5f; dbg_logf("T espera ok (%.1f s) st %d zona %d", to, g_state, g_zone); return; }
        if (to > 40) { wmode = 0; dbg_logf("T ESPERA AGOTADA st %d zona %d (pedido %d)", g_state, g_zone, warg); return; }
        if (press_t > 0.8f && (wmode == 1 || g_state == GS_RET)) { press_t = 0; in.pressed |= PSP_CTRL_CROSS; }
        return;
    }
    if (wt > 0) { wt -= dt; return; }
    while (pos < n && (sc[pos] == ' ' || sc[pos] == '\n' || sc[pos] == '\r')) pos++;
    if (pos >= n) return;
    char tk[48];
    int k = 0;
    while (pos < n && sc[pos] != ' ' && sc[pos] != '\n' && sc[pos] != '\r' && k < 47) tk[k++] = sc[pos++];
    tk[k] = 0;
    wt = 0.3f;
    if (tk[0] == 'w' && tk[1] >= '0' && tk[1] <= '9') wt = atoi(tk + 1) / 1000.0f;
    else if ((tk[0] == 'b' || tk[0] == 'H' || tk[0] == 'J') && tk[2] == 0) {   // b toca; H mantiene apretado un botón; J lo suelta
        static const struct { char c; u32 b; } B[] = {{'x', PSP_CTRL_CROSS}, {'o', PSP_CTRL_CIRCLE}, {'s', PSP_CTRL_SQUARE}, {'t', PSP_CTRL_TRIANGLE}, {'u', PSP_CTRL_UP}, {'d', PSP_CTRL_DOWN}, {'l', PSP_CTRL_LEFT}, {'r', PSP_CTRL_RIGHT}, {'L', PSP_CTRL_LTRIGGER}, {'R', PSP_CTRL_RTRIGGER}, {'S', PSP_CTRL_START}, {'E', PSP_CTRL_SELECT}};
        for (unsigned i = 0; i < sizeof(B) / sizeof(B[0]); i++) if (B[i].c == tk[1]) {
            if (tk[0] == 'b') in.pressed |= B[i].b;
            else if (tk[0] == 'H') { g_dbg_hold_b |= B[i].b; in.pressed |= B[i].b; }
            else g_dbg_hold_b &= ~B[i].b;
        }
    }
    else if (tk[0] == 'u' && tk[1] >= '0' && tk[1] <= '9') { wmode = 1; warg = atoi(tk + 1); to = 0; press_t = 0; }
    else if (tk[0] == 'z' && tk[1] >= '0' && tk[1] <= '9') { wmode = 2; warg = atoi(tk + 1); to = 0; press_t = 0; }
    else dbg_act(tk);
}
#endif

void game_frame(void)
{
    music_tick();   // [armas2]
#ifdef PEPEL_DEBUG
    // medidor de pruebas: lo que tarda un cuadro (actualizar + dibujar, sin la espera de pantalla),
    // promediado en 30 cuadros
    static unsigned dbg_t0, dbg_sum;
    static int dbg_on, dbg_n;
    if (dbg_on) {
        dbg_sum += r_t_done - dbg_t0;
        if (++dbg_n >= 30) { dbg_ms10 = (int)(dbg_sum / (30 * 100)); dbg_sum = 0; dbg_n = 0; }
    }
    dbg_on = 1;
    dbg_t0 = sceKernelGetSystemTimeLow();
#endif
    static unsigned last;
    unsigned vc = r_vcount();
    int steps = last ? (int)(vc - last) : 1;
    last = vc;
    if (steps < 1) steps = 1;
    if (steps > 3) steps = 3;
    float dt = steps / 60.0f;

    read_input();
    // una pulsación que cierra o confirma una pantalla no cuenta en la siguiente: tras cada cambio de estado se
    // ignoran las pulsaciones nuevas durante un instante (la carga de la partida puede tardar y arrastrar un toque)
    if (g_in_guard > 0) { g_in_guard -= dt; in.pressed = 0; }
#ifdef PEPEL_DEBUG
    dbg_script_run(dt);
#endif
    int st_before = g_state;
    g_time += dt;
    g_state_t += dt;
    if (g_msg_t > 0) g_msg_t -= dt;
    if (g_ban_t > 0) g_ban_t -= dt;

    if (g_state != GS_PLAY) {   // fuera de la partida: sonido de menú y los ambientes se callan
        if (in.pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) snd_play(SFX_MENU_MOVE, 0.7f, 0);
        else if (in.pressed & PSP_CTRL_CROSS) snd_play(SFX_MENU_OK, 0.7f, 0);
        snd_loop(SFX_FIRE, 0); snd_loop(SFX_GEIGER, 0); snd_loop(SFX_WIND, 0);
    }

    switch (g_state) {
    case GS_TITLE:
        title_pan();
        if (g_title_page == 0) {
            int ids[3], n = title_items(ids);
            if (g_menu_sel >= n) g_menu_sel = 0;
            if (in.pressed & PSP_CTRL_UP) g_menu_sel = (g_menu_sel + n - 1) % n;
            if (in.pressed & PSP_CTRL_DOWN) g_menu_sel = (g_menu_sel + 1) % n;
            if (in.pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
                if (ids[g_menu_sel] == T_CONT) { load_game(); g_state = GS_PLAY; }
                else if (ids[g_menu_sel] == T_OPT) { g_opt_ret = GS_TITLE; g_opt_sel = 0; g_state = GS_OPT; }
                else { g_title_page = 1; g_mode_sel = MODE_SURV; }
            }
        } else {
            if (in.pressed & PSP_CTRL_UP) g_mode_sel = (g_mode_sel + MODE_COUNT - 1) % MODE_COUNT;
            if (in.pressed & PSP_CTRL_DOWN) g_mode_sel = (g_mode_sel + 1) % MODE_COUNT;
            if (in.pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) g_title_page = 0;
            else if (in.pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
                // partida nueva: el archivo viejo queda hasta el primer guardado, pero ya no se vuelve a él
                g_has_save = 0;
                new_game();
                g_mode = g_mode_sel;
                g_state = GS_PLAY;
            }
        }
        break;
    case GS_PLAY:
#ifdef PEPEL_DEBUG
        dbg_cmd_run();
#endif
        if (jefes_ask(dt)) { update_camera(dt); break; }   // [jefes] la pregunta de la puerta
        update_world(dt);
        update_camera(dt);
        if (g_state == GS_PLAY) {
#ifdef PEPEL_DEBUG
            if ((in.pressed & PSP_CTRL_START) && (in.held & PSP_CTRL_LTRIGGER) && (in.held & PSP_CTRL_RTRIGGER)) { save_game(); msg("Guardado (pruebas)."); }   // L + R + START jugando: guarda ya
            else if ((in.pressed & PSP_CTRL_SELECT) && (in.held & PSP_CTRL_LTRIGGER) && (in.held & PSP_CTRL_RTRIGGER) && g_zone == 0) {   // L + R + SELECT: salta por la base (cama, alijo, huerto, portón, enfermería)
                static const int BTP[][2] = {{13, 24}, {11, 25}, {50, 30}, {5, 19}, {8, 15}};
                static int bt;
                P.x = BTP[bt][0] * TS + 8; P.y = BTP[bt][1] * TS + 8; P.lock = -1;
                bt = (bt + 1) % 5;
            }
            else
#endif
            if (in.pressed & PSP_CTRL_START) g_state = GS_PAUSE;
            else if ((in.pressed & PSP_CTRL_TRIANGLE) && !(in.held & PSP_CTRL_LTRIGGER)) { g_state = GS_INV; g_msg_t = 0; g_inv_focus = 0; g_inv_tab = 0; g_ws_list = 0; }
            else if ((in.pressed & PSP_CTRL_SELECT) && bk_deco_open()) {}   // [bunker] SELECT en el búnker: modo decorar
            else if (in.pressed & PSP_CTRL_SELECT) g_state = GS_MAP;
        }
        break;
    case GS_OPT:
        opt_input();
        break;
    case GS_PAUSE:
        if (in.pressed & PSP_CTRL_START) g_state = GS_PLAY;
#ifdef PEPEL_DEBUG
        if ((in.pressed & PSP_CTRL_CROSS) && (in.held & PSP_CTRL_LTRIGGER) && (in.held & PSP_CTRL_RTRIGGER)) { dbg_mejoras(); g_state = GS_PLAY; break; }   // L + R + Cruz: materiales y chatarra de sobra
#endif
        if (in.pressed & PSP_CTRL_CROSS) { g_opt_ret = GS_PAUSE; g_opt_sel = 0; g_state = GS_OPT; break; }
#ifdef PEPEL_DEBUG
        if (in.pressed & PSP_CTRL_TRIANGLE) { g_state = GS_PLAY; hurt_player(999); }
        if (in.pressed & PSP_CTRL_SQUARE) { g_storm_t = 3; g_state = GS_PLAY; }
        if (in.pressed & PSP_CTRL_CIRCLE) { P.hp = 30; P.hunger = 30; P.thirst = 20; g_state = GS_PLAY; }   // malherido, con hambre y sed
        if (in.pressed & PSP_CTRL_LTRIGGER) { P.hunger = P.thirst = 1; g_state = GS_PLAY; }                 // a punto de morir de hambre y sed
        if (in.pressed & PSP_CTRL_RTRIGGER) { g_bag[ITEM_SCRAP] += 200; g_rep += 2; g_state = GS_PLAY; }              // para probar compras y rangos
        if (in.pressed & PSP_CTRL_UP) {                                                                     // todas las armas, con el cargador lleno
            float cond = g_w[W_COUNT - 1].owned && g_w[W_COUNT - 1].cond > 0.5f ? 0.25f : 0.9f;          // la segunda vez, gastadas (25 %): para probar las trabas y el kit
            for (int i = 0; i < W_COUNT; i++) { g_w[i].owned = 1; g_w[i].mag = w_mag(i); g_w[i].cond = cond; }
            g_state = GS_PLAY;
        }
        if (in.pressed & PSP_CTRL_DOWN) {                                                                   // abre todos los cajones y cuerpos de la zona (para probar botín y trabajos)
            for (int i = 0; i < g_ncr; i++) if (!g_cr[i].open) open_crate(&g_cr[i]);
            g_state = GS_PLAY;
        }
        if (in.pressed & PSP_CTRL_RIGHT) {                                                                  // salta de sitio en sitio: campamento de bandidos, zombi grande del almacén...
            const int (*TP)[3] = DBG_TP;
            static int tp;
            P.hp = 100; P.bleed = 0;   // llega sano
            if (TP[tp][0] != g_zone) zone_go(TP[tp][0], TP[tp][1] * TS + 8, TP[tp][2] * TS + 8, 0);
            else { P.x = TP[tp][1] * TS + 8; P.y = TP[tp][2] * TS + 8; P.lock = -1; }   // misma zona: sigue en pausa (START para seguir), así se puede saltar varios lugares seguidos
            tp = (tp + 1) % DBG_NTP;
        }
        if (in.pressed & PSP_CTRL_LEFT) {                                                                   // vuelve al portón de Estación Kolos
            if (g_zone != 0) zone_go(0, 2 * TS + 8, 19 * TS + 8, 0);
            else { P.x = 2 * TS + 8; P.y = 19 * TS + 8; g_state = GS_PLAY; }
        }
        if ((in.pressed & PSP_CTRL_SELECT) && (in.held & PSP_CTRL_RTRIGGER)) { g_dbg_god = !g_dbg_god; g_state = GS_PLAY; }   // R + SELECT: activa o quita el modo sin daño
        if ((in.pressed & PSP_CTRL_SELECT) && (in.held & PSP_CTRL_LTRIGGER)) {                              // L + SELECT: escenario de guardado (alijo, piezas, equipo puesto, trabajo activo)
            dbg_scenario();
            g_state = GS_PLAY;
        }
        if ((in.pressed & PSP_CTRL_SELECT) && !(in.held & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))) {       // mochila al tope (y sin daño)
            g_dbg_god = 1;
            for (int i = 0; i < ITEM_COUNT; i++) {
                int gi = gear_of(i);
                if (gi >= 0) { if (!gear_owned(gi)) { g_bag[i] = 1; g_gcond[gi] = 100; } }   // equipo: una de cada prenda, nueva
                else if (ITEMS[i].cap) g_bag[i] = ITEMS[i].cap;
            }
            g_state = GS_PLAY;
        }
#endif
        break;
    case GS_INV:   // la mochila: el juego queda en pausa; las flechas mueven el cursor y Cruz usa lo elegido
        inv_input();
        break;
    case GS_MAP:
        if (in.pressed & PSP_CTRL_SELECT) g_state = GS_PLAY;
        break;
    case GS_KOLOS:   // el portón: el juego queda en pausa
        kolos_input();
        break;
    case GS_BK: bk_deco_input(); break;   // [bunker]
    case GS_FARO: faro_input(); break;
    case GS_ENDSEL: endsel_input(); break;
    case GS_ZONE:   // cambio de zona: el mundo queda quieto; a pantalla negra se cambia
        if (!g_zdone && g_state_t >= ZFADE) { g_zdone = 1; zone_switch(); }
        if (g_state_t >= 2 * ZFADE + ZHOLD) {
            g_state = g_ret_pend ? GS_RET : GS_PLAY;   // al volver de una expedición, primero el resumen
            g_state_t = 0; g_ret_pend = 0;
        }
        break;
    case GS_RET:   // el resumen del regreso: se cierra con Cruz
        if (g_state_t > 0.4f && (in.pressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE))) g_state = GS_PLAY;
        break;
    case GS_FIN:   // el final: se puede seguir jugando
        if (g_state_t > 1.5f && (in.pressed & (PSP_CTRL_CROSS | PSP_CTRL_START | PSP_CTRL_CIRCLE))) { g_state = GS_PLAY; g_msg_t = 0; }
        break;
    case GS_CHOFER:   // hablar con el Chofer: el juego queda en pausa
        if (in.pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) g_state = GS_PLAY;
        else travel_input();
        break;
    case GS_SLEEP:   // dormir en un catre: el mundo queda quieto; a pantalla negra pasa el tiempo y se guarda
        if (!g_slept && g_state_t >= SLEEP_FADE) { g_slept = 1; sleep_apply(); }
        if (g_state_t >= 2 * SLEEP_FADE + SLEEP_DARK) { g_state = GS_PLAY; g_msg_t = 4.0f; }   // el aviso se lee al despertar
        break;
    case GS_DEAD:
        update_camera(dt);
        if (g_state_t < 0.1f) g_dead_sel = 0;
        if (g_mode == MODE_IRON && g_has_save) { sceIoRemove(g_sav); sceIoRemove(g_tmp); g_has_save = 0; }   // Hierro: la partida se borra al morir
        if (g_state_t > 1) {
            int ids[3], n = dead_items(ids);
            if (in.pressed & PSP_CTRL_UP) g_dead_sel = (g_dead_sel + n - 1) % n;
            if (in.pressed & PSP_CTRL_DOWN) g_dead_sel = (g_dead_sel + 1) % n;
            if (in.pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
                int id = ids[g_dead_sel];
                if (id == D_REFUGE) { if (g_has_save) load_game(); else new_game(); g_state = GS_PLAY; }
                else { g_state = GS_TITLE; g_menu_sel = 0; g_title_page = id == D_NEW ? 1 : 0; g_mode_sel = g_mode; }
            }
        }
        break;
    }

    if (g_state != st_before) g_in_guard = 0.25f;

    r_begin(RGBA(8, 8, 8, 255));
    gather_lights();
    draw_world();
    draw_ash();
    dz_llu_draw(0);   // [lluvia] charcos
    draw_dark();
    dz_llu_draw(1);   // [lluvia] gotas
    draw_emissive();
    draw_vignette();
    if (g_state == GS_TITLE) draw_title();
    else if (g_state == GS_OPT && g_opt_ret == GS_TITLE) draw_options();
    else {
        draw_hud();
        jefes_draw_ui();   // [jefes]
        if (g_state == GS_PAUSE) draw_pause();
        if (g_state == GS_OPT) draw_options();
        if (g_state == GS_INV) draw_inv();
        if (g_state == GS_MAP) draw_map();
        if (g_state == GS_KOLOS) draw_kolos();
        if (g_state == GS_BK) bk_deco_draw();   // [bunker]
        if (g_state == GS_FARO) draw_faro();
        if (g_state == GS_SLEEP) draw_sleep();
        if (g_state == GS_CHOFER) {
            r_rect(0, 0, SCR_W, SCR_H, RGBA(0, 0, 0, 150));
            travel_draw_panel("EL CHOFER");
            r_text_center(SCR_W / 2, 231, "Arriba/abajo: elegir   Cruz: viajar   Círculo: volver", RGBA(170, 166, 158, 255));
            ui_msg(246);
        }
        if (g_state == GS_ZONE) draw_zone_fade();
        if (g_state == GS_FIN) draw_fin();
        if (g_state == GS_ENDSEL) draw_endsel();
        if (g_state == GS_RET) draw_return();
        if (g_state == GS_DEAD) draw_end(0);
    }
#ifdef PEPEL_DEBUG
    {
        char s[160];
        snprintf(s, sizeof(s), "%d.%d ms  vida %d  hambre %d  sed %d  rad %d", dbg_ms10 / 10, dbg_ms10 % 10, (int)P.hp, (int)P.hunger, (int)P.thirst, (int)P.rad);
        r_text_center(SCR_W / 2, 4, s, COL_WHITE);
        int vivos = 0, llam = 0, abiertos = 0, suelo = 0, chat = 0;
        for (int i = 0; i < g_ne; i++) if (g_e[i].state != ES_DEAD) { vivos++; if (g_e[i].summon) llam++; }
        for (int i = 0; i < g_ncr; i++) abiertos += g_cr[i].open;
        for (int i = 0; i < MAXK; i++) if (g_k[i].on) { suelo++; if (g_k[i].item == ITEM_SCRAP) chat++; }
        int mira = P.lock >= 0 ? g_e[P.lock].type * 10 + g_e[P.lock].fac : -1;
        snprintf(s, sizeof(s), "rep %d vivos %d llam %d caj %d sem %x suelo %d chat %d mira %d zona %d modo %d", g_rep, vivos, llam, abiertos, (unsigned)(g_bseed & 0xFFFF), suelo, chat, mira, g_zone, g_mode);
        r_text_center(SCR_W / 2, 16, s, COL_WHITE);
        // registro de pruebas: cada 0,5 s se anota lo mismo que muestra el medidor (dbg_log.txt junto a la partida)
        static int logn;
        if (++logn >= 30) {
            logn = 0;
            char lp[170], l2[96];
            snprintf(lp, sizeof(lp), "%s", g_sav);
            char *sl = strrchr(lp, '/');
            if (sl) {
                strcpy(sl + 1, "dbg_log.txt");
                SceUID lf = sceIoOpen(lp, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
                if (lf >= 0) {
                    char ln[400];
                    snprintf(l2, sizeof(l2), "st %d hp %.1f rad %.1f px %d py %d tor %d/%d", g_state, P.hp, P.rad, (int)P.x, (int)P.y, g_storm_on, (int)g_storm_t);
                    char l3[120] = "";
                    for (int i = 0, n = 0; i < g_ne && n < 4; i++) {
                        const Enemy *e = &g_e[i];
                        if ((e->type == E_GRD || e->type == E_BOSS || e->hp > 1000) && e->state != ES_DEAD) {
                            char t[40];
                            snprintf(t, sizeof(t), " [t%d f%d %d,%d hp%d]", e->type, e->fac, (int)e->x, (int)e->y, (int)e->hp);
                            strncat(l3, t, sizeof(l3) - strlen(l3) - 1);
                            n++;
                        }
                    }
                    snprintf(ln, sizeof(ln), "%s | %s |%s\n", s, l2, l3);
                    sceIoWrite(lf, ln, strlen(ln));
                    sceIoClose(lf);
                }
            }
        }
    }
#endif
    r_end();
}
