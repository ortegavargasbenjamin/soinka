// [mapas] Trazados de la expedición: cada mapa se arma con uno de tres trazados según su tipo y su semilla.
//   0 cuadrícula de calles (los caminos de siempre de cada tipo)
//   1 avenida central con plaza (una avenida ancha de norte a sur y una plaza despejada en el centro)
//   2 disperso, con una quebrada contaminada que parte el mapa en dos y solo se cruza por 2 o 3 puentes
// Se incluye justo antes de exped_gen y usa sus funciones x_*.
static int g_plz_x = -1, g_plz_y;   // [diseno] centro de la plaza (-1 = no hay)
static int g_lay;   // trazado del mapa cargado (0, 1 o 2); para las pruebas

// Qué trazados admite cada tipo (dos por tipo): el segundo se elige con la semilla
static int lay_pick(int type)
{
    static const unsigned char LAY[XT_N][2] = {
        [XT_FOREST] = {2, 0}, [XT_TOWN] = {0, 1}, [XT_FACTORY] = {1, 0}, [XT_TRAIN] = {0, 2}, [XT_GRAVE] = {1, 2}, [XT_MINE] = {2, 0}, [XT_PINE] = {0, 0}, [XT_CITY] = {1, 1},   // [diseno]
    };
    return LAY[type][brnd() & 1];
}

// Después de los caminos del tipo y del tercer punto de retirada: añade la avenida y la plaza, o la quebrada con sus puentes
static void lay_apply(int type)
{
    g_lay = lay_pick(type);
    g_plz_x = -1;   // [diseno]
    if (type == XT_PINE) g_lay = 0;
    if (g_lay == 1) {   // avenida central y plaza
        int cx, cy = MH / 2 + brange(-8, 8), tries = 0;
        do { cx = MW / 2 + brange(-10, 10); tries++; } while (tries < 30 && abs(cx - g_ret3_tx) < 6);   // la avenida no pisa el tercer punto de retirada
        x_fill(cx - 1, 3, cx + 2, MH - 4, '=');
        x_fill(cx - 9, cy - 5, cx + 10, cy + 5, '=');
        g_plz_x = cx; g_plz_y = cy;   // [diseno]
        for (int y = cy - 5; y <= cy + 5; y++) for (int x = cx - 9; x <= cx + 10; x++) if (x_inb(x, y)) x_keep[y][x] = 1;   // la plaza queda despejada
    } else if (g_lay == 2) {   // quebrada de norte a sur con 2 o 3 puentes
        int rx, tries = 0;
        do { rx = brange(MW / 2 - 36, MW / 2 + 36); tries++; } while (tries < 30 && abs(rx - g_ret3_tx) < 9);
        int nbr = 2 + (int)(brnd() % 2), by[3];
        for (int k = 0; k < nbr; k++) by[k] = 8 + (MH - 20) * (2 * k + 1) / (2 * nbr) + brange(-5, 5);
        for (int y = 3; y < MH - 3; y++) {
            int bridge = 0;
            for (int k = 0; k < nbr; k++) bridge |= y >= by[k] && y <= by[k] + 2;
            int sw = rx + (int)(brnd() % 3) - 1;   // el cauce serpentea un poco
            for (int x = sw; x < sw + 3; x++) {
                if (!x_inb(x, y)) continue;
                if (bridge) { g_zg[y][x] = '='; g_zo[y][x] = ' '; x_keep[y][x] = 1; }
                else { g_zg[y][x] = '~'; g_zo[y][x] = 'T'; }   // matorral denso sobre el agua: no se cruza
            }
        }
    }
}
