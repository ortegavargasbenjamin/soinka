// Dibujo 2D en pixeles para la PSP: un solo atlas, sin suavizado, todo por lotes.
#ifndef R2D_H
#define R2D_H

#include <psptypes.h>

#define SCR_W 480
#define SCR_H 272

// Color en el orden de la GE: 0xAABBGGRR
#define RGBA(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))
#define ALPHA(c, a) (((c) & 0x00FFFFFF) | ((u32)(a) << 24))
#define COL_WHITE 0xFFFFFFFF

typedef struct { short x, y, w, h, p; } Spr;   // un recorte del atlas; p = pagina (0 o 1) de las dos texturas

void r_init(void);
void r_begin(u32 clear);
void r_end(void);            // termina el cuadro y espera a la pantalla
#ifdef PEPEL_DEBUG
extern unsigned r_t_done;    // instante (us) en que termino de dibujarse el cuadro, antes de la espera
#endif
unsigned r_vcount(void);     // cuadros de pantalla desde que partio (para medir el tiempo)

void r_spr(Spr s, float x, float y, u32 color);                              // tal cual, esquina arriba-izquierda
void r_spr_s(Spr s, float x, float y, float w, float h, u32 color);          // estirado
void r_rot(Spr s, float cx, float cy, float ang, float scale, u32 color);    // centrado y girado (radianes)
void r_spr_f(Spr s, float x, float y, int flipx, u32 color);                 // tal cual o espejado
void r_rot_f(Spr s, float cx, float cy, float ang, int flipv, u32 color);    // girado; flipv lo da vuelta de arriba abajo
void r_rect(float x, float y, float w, float h, u32 color);
void r_image(const u32 *px, int w, int h, int stride, float x, float y, float k);   // imagen de la memoria, escalada
void r_grad(float x, float y, float w, float h, u32 tl, u32 tr, u32 bl, u32 br);
void r_line(float x0, float y0, float x1, float y1, float t, u32 color);
// Halo redondo que se desvanece del centro al borde
void r_glow(float cx, float cy, float radius, u32 center, u32 edge);
// Velo por celdas: (cols+1) x (rows+1) colores en las esquinas, celdas de `cell` px desde (x0, y0)
void r_grid(float x0, float y0, int cols, int rows, float cell, const u32 *colors);

// Texto UTF-8 con la letra del atlas. Devuelve el ancho dibujado.
float r_text(float x, float y, const char *s, u32 color);
float r_text_s(float x, float y, const char *s, u32 color, int scale);
float r_text_w(const char *s);
void r_text_shadow(float x, float y, const char *s, u32 color);
void r_text_center(float cx, float y, const char *s, u32 color);

#endif
