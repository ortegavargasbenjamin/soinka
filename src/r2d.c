// Dibujo 2D en pixeles para la PSP. Todo sale de un atlas de 256x256 que va dentro del EBOOT.
// Los sprites derechos van como GU_SPRITES y los girados como triangulos; se juntan en lotes y
// se mandan a la GE cuando cambia el tipo, asi se respeta el orden en que se dibujo.
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <string.h>
#include <math.h>
#include "r2d.h"
#include "atlas.h"

#define BUF_W 512
#define FB_SIZE (BUF_W * SCR_H * 4)
#define FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define MAXV 3072

typedef struct { float u, v; u32 c; float x, y, z; } VT;

extern const unsigned char atlas_rgba[];   // src/assets.S
static unsigned int __attribute__((aligned(64))) g_list[128 * 1024];
static VT g_v[MAXV];
static int g_n, g_tris, g_page;   // g_page: pagina de atlas activa (dos de 512x512 seguidas)

// Cambia de pagina de atlas; manda antes lo pendiente (el lote usa una sola textura)
static void setpage(int p)
{
    if (p == g_page) return;
    extern void flush_(void);
    flush_();
    g_page = p;
    sceGuTexImage(0, ATLAS_W, ATLAS_H_PX, ATLAS_W, (void *)(atlas_rgba + (size_t)p * ATLAS_W * ATLAS_H_PX * 4));
}

void r_init(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)FB_SIZE, BUF_W);
    sceGuDepthBuffer((void *)(FB_SIZE * 2), BUF_W);
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);   // pixeles duros
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void flush_(void);
static void flush(void) { flush_(); }
void flush_(void)
{
    if (!g_n) return;
    VT *m = (VT *)sceGuGetMemory(g_n * sizeof(VT));
    memcpy(m, g_v, g_n * sizeof(VT));
    sceGuDrawArray(g_tris ? GU_TRIANGLES : GU_SPRITES, FMT, g_n, 0, m);
    g_n = 0;
}

static VT *room(int tris, int n)
{
    if (g_tris != tris || g_n + n > MAXV) { flush(); g_tris = tris; }
    VT *v = &g_v[g_n];
    g_n += n;
    return v;
}

void r_begin(u32 clear)
{
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(clear);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, ATLAS_W, ATLAS_H_PX, ATLAS_W, (void *)atlas_rgba);
    g_n = 0;
    g_page = 0;
}

#ifdef PEPEL_DEBUG
unsigned r_t_done;
#endif

void r_end(void)
{
    flush();
    sceGuFinish();
    sceGuSync(0, 0);
#ifdef PEPEL_DEBUG
    r_t_done = sceKernelGetSystemTimeLow();
#endif
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

unsigned r_vcount(void) { return sceDisplayGetVcount(); }

static void quad(VT *v, const float *px, const float *py, const float *pu, const float *pv, const u32 *pc);

void r_spr_s(Spr s, float x, float y, float w, float h, u32 color)
{
    setpage(s.p);
    VT *v = room(0, 2);
    v[0] = (VT){s.x, s.y, color, x, y, 0};
    v[1] = (VT){s.x + s.w, s.y + s.h, color, x + w, y + h, 0};
}

void r_spr(Spr s, float x, float y, u32 color)
{
    r_spr_s(s, floorf(x + 0.5f), floorf(y + 0.5f), s.w, s.h, color);
}

void r_spr_f(Spr s, float x, float y, int flipx, u32 color)
{
    setpage(s.p);
    x = floorf(x + 0.5f);
    y = floorf(y + 0.5f);
    VT *v = room(0, 2);
    v[0] = (VT){flipx ? s.x + s.w : s.x, s.y, color, x, y, 0};
    v[1] = (VT){flipx ? s.x : s.x + s.w, s.y + s.h, color, x + s.w, y + s.h, 0};
}

void r_rot_f(Spr s, float cx, float cy, float ang, int flipv, u32 color)
{
    setpage(s.p);
    float c = cosf(ang), sn = sinf(ang), hw = s.w * 0.5f, hh = s.h * 0.5f;
    float lx[4] = {-hw, hw, -hw, hw}, ly[4] = {-hh, -hh, hh, hh}, px[4], py[4];
    float v0 = flipv ? s.y + s.h : s.y, v1 = flipv ? s.y : s.y + s.h;
    float pu[4] = {s.x, s.x + s.w, s.x, s.x + s.w}, pv[4] = {v0, v0, v1, v1};
    u32 pc[4] = {color, color, color, color};
    for (int i = 0; i < 4; i++) {
        px[i] = cx + lx[i] * c - ly[i] * sn;
        py[i] = cy + lx[i] * sn + ly[i] * c;
    }
    quad(room(1, 6), px, py, pu, pv, pc);
}

void r_rect(float x, float y, float w, float h, u32 color)
{
    Spr s = SPR[S_WHITE];
    setpage(s.p);
    VT *v = room(0, 2);
    v[0] = (VT){s.x + 1, s.y + 1, color, x, y, 0};
    v[1] = (VT){s.x + 2, s.y + 2, color, x + w, y + h, 0};
}

static void quad(VT *v, const float *px, const float *py, const float *pu, const float *pv, const u32 *pc)
{
    static const int idx[6] = {0, 1, 2, 2, 1, 3};
    for (int i = 0; i < 6; i++) {
        int k = idx[i];
        v[i] = (VT){pu[k], pv[k], pc[k], px[k], py[k], 0};
    }
}

void r_rot(Spr s, float cx, float cy, float ang, float scale, u32 color)
{
    setpage(s.p);
    float c = cosf(ang) * scale, sn = sinf(ang) * scale, hw = s.w * 0.5f, hh = s.h * 0.5f;
    // esquinas: arriba-izq, arriba-der, abajo-izq, abajo-der
    float lx[4] = {-hw, hw, -hw, hw}, ly[4] = {-hh, -hh, hh, hh}, px[4], py[4];
    float pu[4] = {s.x, s.x + s.w, s.x, s.x + s.w}, pv[4] = {s.y, s.y, s.y + s.h, s.y + s.h};
    u32 pc[4] = {color, color, color, color};
    for (int i = 0; i < 4; i++) {
        px[i] = cx + lx[i] * c - ly[i] * sn;
        py[i] = cy + lx[i] * sn + ly[i] * c;
    }
    quad(room(1, 6), px, py, pu, pv, pc);
}

void r_grad(float x, float y, float w, float h, u32 tl, u32 tr, u32 bl, u32 br)
{
    Spr s = SPR[S_WHITE];
    setpage(s.p);
    float px[4] = {x, x + w, x, x + w}, py[4] = {y, y, y + h, y + h};
    float pu[4] = {s.x + 1.5f, s.x + 1.5f, s.x + 1.5f, s.x + 1.5f}, pv[4] = {s.y + 1.5f, s.y + 1.5f, s.y + 1.5f, s.y + 1.5f};
    u32 pc[4] = {tl, tr, bl, br};
    quad(room(1, 6), px, py, pu, pv, pc);
}

void r_line(float x0, float y0, float x1, float y1, float t, u32 color)
{
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) return;
    Spr s = SPR[S_WHITE];
    setpage(s.p);
    float nx = -dy / len * t * 0.5f, ny = dx / len * t * 0.5f;
    float px[4] = {x0 + nx, x1 + nx, x0 - nx, x1 - nx}, py[4] = {y0 + ny, y1 + ny, y0 - ny, y1 - ny};
    float pu[4] = {s.x + 1.5f, s.x + 1.5f, s.x + 1.5f, s.x + 1.5f}, pv[4] = {s.y + 1.5f, s.y + 1.5f, s.y + 1.5f, s.y + 1.5f};
    u32 pc[4] = {color, color, color, color};
    quad(room(1, 6), px, py, pu, pv, pc);
}

void r_glow(float cx, float cy, float radius, u32 center, u32 edge)
{
    Spr s = SPR[S_WHITE];
    setpage(s.p);
    float u = s.x + 1.5f, w = s.y + 1.5f;
    VT *v = room(1, 36);
    for (int i = 0; i < 12; i++) {
        float a0 = i * (6.2831853f / 12), a1 = (i + 1) * (6.2831853f / 12);
        v[i * 3] = (VT){u, w, center, cx, cy, 0};
        v[i * 3 + 1] = (VT){u, w, edge, cx + cosf(a0) * radius, cy + sinf(a0) * radius, 0};
        v[i * 3 + 2] = (VT){u, w, edge, cx + cosf(a1) * radius, cy + sinf(a1) * radius, 0};
    }
}

void r_grid(float x0, float y0, int cols, int rows, float cell, const u32 *colors)
{
    Spr s = SPR[S_WHITE];
    setpage(s.p);
    float u = s.x + 1.5f, w = s.y + 1.5f;
    flush();
    for (int r = 0; r < rows; r++) {   // una tira de triangulos por fila
        VT *v = (VT *)sceGuGetMemory((cols + 1) * 2 * sizeof(VT));
        const u32 *top = colors + r * (cols + 1), *bot = top + cols + 1;
        for (int c = 0; c <= cols; c++) {
            v[c * 2] = (VT){u, w, top[c], x0 + c * cell, y0 + r * cell, 0};
            v[c * 2 + 1] = (VT){u, w, bot[c], x0 + c * cell, y0 + (r + 1) * cell, 0};
        }
        sceGuDrawArray(GU_TRIANGLE_STRIP, FMT, (cols + 1) * 2, 0, v);
    }
}

// ------------------------------------------------------------------ texto
static unsigned utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned c = *p++;
    if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if (c >= 0xE0) { while ((*p & 0xC0) == 0x80) p++; c = '?'; }
    *s = (const char *)p;
    return (c < 32 || c > 255) ? '?' : c;
}

float r_text_w(const char *s)
{
    float w = 0;
    while (*s) w += FONT_ADV[utf8_next(&s) - 32];
    return w;
}

float r_text_s(float x, float y, const char *s, u32 color, int scale)
{
    float x0 = x = floorf(x + 0.5f);
    y = floorf(y + 0.5f);
    while (*s) {
        int k = utf8_next(&s) - 32, adv = FONT_ADV[k];
        if (adv && k) {
            Spr g = {FONT_X[k], FONT_Y[k], adv + 1, FONT_H};
            r_spr_s(g, x, y, g.w * scale, g.h * scale, color);
        }
        x += adv * scale;
    }
    return x - x0;
}

float r_text(float x, float y, const char *s, u32 color) { return r_text_s(x, y, s, color, 1); }

void r_text_shadow(float x, float y, const char *s, u32 color)
{
    r_text(x + 1, y + 1, s, RGBA(0, 0, 0, (color >> 24) * 200 / 255));
    r_text(x, y, s, color);
}

void r_text_center(float cx, float y, const char *s, u32 color)
{
    r_text_shadow(cx - r_text_w(s) / 2, y, s, color);
}

// Dibuja una imagen RGBA de la memoria (ancho de fila `stride`, potencia de 2, alineada a 16 bytes) escalada por `k`.
// Sirve para el mapa de SELECT: una sola textura en vez de miles de rectángulos (que desbordaban la lista de la GE)
void r_image(const u32 *px, int w, int h, int stride, float x, float y, float k)
{
    flush_();
    sceKernelDcacheWritebackRange(px, (unsigned)(stride * h * 4));
    sceGuTexImage(0, stride, 128, stride, (void *)px);
    sceGuTexFlush();
    VT *v = (VT *)sceGuGetMemory(2 * sizeof(VT));
    v[0] = (VT){0, 0, 0xFFFFFFFF, x, y, 0};
    v[1] = (VT){(float)w, (float)h, 0xFFFFFFFF, x + w * k, y + h * k, 0};
    sceGuDrawArray(GU_SPRITES, FMT, 2, 0, v);
    g_page = -1;   // la próxima vez se vuelve a poner la página del atlas
}
