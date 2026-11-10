#ifndef GAME_H
#define GAME_H
void game_init(const char *dir);   // dir: carpeta del EBOOT (ahi va la partida guardada); NULL = la de siempre
void game_frame(void);   // un cuadro: leer botones, avanzar y dibujar
#endif
