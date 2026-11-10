# PEPEL: arma el mapa de la primera zona ("Patio de maniobras") y lo deja en src/map1.h.
# Dos capas: suelo (G) y objetos (O). Imprime el mapa para revisarlo a ojo.
#   suelo:   . tierra   , pasto seco   = camino   _ piso   D umbral   r via   ~ charco radiactivo   E salida
#   objetos: # muro   C contenedor   K contenedor azul   f reja   T arbol   s sacos   b barril   V v auto
#            c cajon   X cajon del trabajo   W cajon con un arma   P jugador   d zombi   m zombi del hacha
#            F barril con fuego   L farol   * lampara de techo   x tumba   i estaca con calavera
#            B zombi grande   a bandido   g guardia de Kolos   J La Bestia del charco
#            H escotilla al bunker   U el Chofer (parada de viaje)   (suelo) Z salida a otra zona
#            Y comerciante   y chatarrero   h banco de trabajo   A arcon (alijo)   (suelo) E salida de expedicion
#            R escombros (sector tapado de la base: la enfermeria; la mejora los quita)
#            k esqueleto   z cuerpo que se puede registrar   M cuerpo que lleva una mascara de gas   q catre (para dormir)
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 64, 40
G = [['.'] * W for _ in range(H)]
O = [[' '] * W for _ in range(H)]


def gfill(x0, y0, x1, y1, ch):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            G[y][x] = ch


def ofill(x0, y0, x1, y1, ch):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            O[y][x] = ch


def put(x, y, ch):
    O[y][x] = ch


def box(x0, y0, x1, y1):
    """Edificio: muros en el borde y piso adentro."""
    gfill(x0, y0, x1, y1, '_')
    for x in range(x0, x1 + 1):
        O[y0][x] = O[y1][x] = '#'
    for y in range(y0, y1 + 1):
        O[y][x0] = O[y][x1] = '#'


def door(x, y):
    O[y][x] = ' '
    G[y][x] = 'D'


# ---- terreno
gfill(1, 27, 62, 38, ',')          # bosque muerto al sur
gfill(50, 1, 62, 13, ',')          # y al noreste
gfill(1, 19, 62, 20, '=')          # el camino
gfill(1, 5, 49, 5, 'r')            # dos vias
gfill(1, 8, 49, 8, 'r')
gfill(1, 19, 2, 20, 'E')           # porton del asentamiento = salida

# ---- borde: arboles
for x in range(W):
    O[0][x] = O[H - 1][x] = 'T'
for y in range(H):
    O[y][0] = O[y][W - 1] = 'T'
put(0, 19, '#'); put(0, 20, '#')

# ---- muro del asentamiento y puesto del porton
ofill(1, 14, 2, 18, '#')
ofill(1, 21, 2, 25, '#')
for x, y in ((4, 17), (5, 17), (4, 22), (5, 22)):
    put(x, y, 's')
put(4, 19, 'P')
put(4, 18, 'g'); put(4, 21, 'g')    # dos guardias de Kolos junto al porton

# ---- patio de trenes: contenedores
for x, y, k in ((6, 2, 'C'), (12, 2, 'K'), (22, 2, 'C'), (40, 2, 'K'), (6, 10, 'K'), (20, 10, 'C'), (14, 12, 'C')):
    ofill(x, y, x + 3, y + 1, k)
put(3, 10, 'b'); put(35, 2, 'b'); put(36, 2, 'b')
put(47, 6, 'c'); put(27, 12, 'c'); put(10, 3, 'c')
for x in (28, 29, 30):
    put(x, 7, 's')

# ---- caseta cerca del porton (refugio)
box(9, 23, 15, 27)
door(12, 23)
put(14, 25, 'c')
put(14, 26, 'b')
put(14, 24, 'q')                   # catre en el rincon del fondo (la fila de abajo la tapa el muro)

# ---- sector tapado de la base: un cuarto lleno de escombros (la mejora "Enfermeria" lo despeja)
box(5, 12, 11, 16)
door(8, 16)
ofill(6, 13, 10, 15, 'R')
put(8, 16, 'R')
O[13][8] = '*'                     # la lampara de techo (la mejora despeja el resto)

# ---- almacen
box(30, 9, 47, 17)
ofill(39, 10, 39, 16, '#')
O[13][39] = ' '
door(34, 17); door(35, 17)
door(47, 12)
for x, y in ((31, 10), (32, 10), (37, 15), (45, 10), (41, 16)):
    put(x, y, 'c')
put(31, 16, 'b'); put(46, 16, 'b')
put(45, 15, 'X')
put(38, 10, 'q')                   # catre contra el muro del medio
for x in (32, 33, 36, 37):
    put(x, 18, 's')

# ---- camino: autos y barriles
put(22, 19, 'V'); put(23, 19, 'v')
put(43, 20, 'V'); put(44, 20, 'v')
put(26, 22, 'b'); put(49, 22, 'b')

# ---- reja entre el camino y el bosque, con dos pasos
for x in range(17, 46):
    if x not in (29, 30, 31):
        put(x, 25, 'f')
for y in range(14, 19):
    put(50, y, 'f')

# ---- ruina de campamento en el bosque
box(12, 31, 18, 35)
for x, y in ((12, 33), (15, 31), (18, 34), (16, 35), (17, 35)):
    O[y][x] = ' '
gfill(12, 31, 18, 35, '.')
put(14, 33, 'c'); put(16, 32, 'W')

# ---- charco radiactivo con un cajon tentador en medio
for y in range(28, 36):
    for x in range(48, 60):
        if ((x - 54) / 5.6) ** 2 + ((y - 31.5) / 3.6) ** 2 <= 1:
            G[y][x] = '~'
put(54, 31, 'c'); put(55, 32, 'c')

# ---- la zona 0 es la base (Estacion Kolos): sin monstruos. Solo quedan los dos guardias del porton
put(60, 3, 'c')

# ---- personas del almacen y el alijo de la caseta (intocables)
put(35, 13, 'Y')                   # el comerciante, en la sala izquierda
put(42, 13, 'y'); put(43, 13, 'h') # el chatarrero y su banco de trabajo, en la sala derecha
put(10, 24, 'A')                   # el arcon (alijo) de la caseta, junto al catre

# ---- lo siniestro: fuego, faroles, tumbas, estacas, esqueletos y cuerpos
for x, y in ((7, 18), (38, 18), (15, 34), (17, 6), (57, 22)):
    put(x, y, 'F')
for x, y in ((11, 18), (27, 21), (47, 18), (20, 9)):
    put(x, y, 'L')
for x, y in ((34, 13), (43, 12), (12, 25)):
    put(x, y, '*')
for x, y in ((5, 24), (7, 24), (5, 26), (7, 26)):
    put(x, y, 'x')
for x, y in ((30, 18), (39, 18), (27, 29)):
    put(x, y, 'i')
for x, y in ((23, 32), (26, 31), (21, 30), (56, 8), (13, 34), (36, 3), (52, 24)):
    put(x, y, 'k')
for x, y in ((13, 26), (24, 21), (41, 11), (17, 33)):
    put(x, y, 'z')
put(47, 31, 'M')   # al borde del charco radiactivo: murió con la máscara puesta

# ---- arboles sueltos sobre el pasto
rnd = random.Random(7)
n = 0
while n < 46:
    x, y = rnd.randrange(1, W - 1), rnd.randrange(1, H - 1)
    if G[y][x] != ',' or O[y][x] != ' ':
        continue
    if any(O[yy][xx] != ' ' for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2)):
        continue
    put(x, y, 'T')
    n += 1

ZONES = [([''.join(r) for r in G], [''.join(r) for r in O])]

# =====================================================================================
# ZONA 1: "Barrio quemado". Calles en cuadricula, manzanas con casas en ruinas.
G = [['.'] * W for _ in range(H)]
O = [[' '] * W for _ in range(H)]
HS = (6, 19, 32)      # fila superior de las calles horizontales (miden 2 de alto)
VS = (15, 31, 47)     # columna izquierda de las calles verticales
gfill(1, 1, 62, 38, ',')
for y in HS:
    gfill(1, y, 62, y + 1, '=')
for x in VS:
    gfill(x, 1, x + 1, 38, '=')
for x in range(W):
    O[0][x] = O[H - 1][x] = 'T'
for y in range(H):
    O[y][0] = O[y][W - 1] = 'T'
gfill(0, 19, 1, 20, 'Z')                 # salida al oeste: vuelta al Patio de maniobras
put(0, 19, ' '); put(0, 20, ' ')
rnd1 = random.Random(11)

# arriba (filas 2..5)
box(2, 2, 11, 5);    door(6, 5)
box(19, 2, 28, 5);   door(23, 5)
box(35, 2, 44, 5);   door(40, 5)
box(51, 2, 60, 5);   door(55, 5)
# medio-arriba (9..17)
box(2, 9, 12, 17);   door(7, 17)
box(19, 10, 29, 17); door(24, 17)
box(34, 9, 45, 17);  door(39, 17); door(40, 17)      # el edificio grande: bandidos
box(51, 10, 61, 16); door(56, 16)
# medio-abajo (23..30)
box(2, 23, 12, 30);  door(7, 23)
box(19, 23, 29, 30); door(24, 23)
box(34, 23, 45, 30); door(39, 23)
box(51, 25, 61, 31); door(56, 25)
# paredes interiores del edificio grande
ofill(40, 10, 40, 16, '#'); O[13][40] = ' '
ofill(35, 13, 44, 13, '#'); O[13][38] = ' '; O[13][43] = ' '; O[13][40] = ' '
# huecos de derrumbe en algunos muros (ruinas)
for x, y in ((2, 4), (11, 3), (60, 3), (45, 29), (12, 26), (29, 27), (61, 28), (51, 12), (29, 12)):
    if O[y][x] == '#':
        O[y][x] = ' '

# catres
for x, y in ((4, 11), (22, 12), (4, 28), (53, 28)):
    put(x, y, 'q')
# cajones
for x, y in ((9, 3), (21, 3), (37, 3), (57, 3), (10, 15), (27, 12), (49, 16), (6, 25), (22, 28), (43, 26), (58, 28), (59, 12),
             (36, 15), (42, 11)):
    put(x, y, 'c')
# luces de techo
for x, y in ((6, 3), (23, 3), (7, 13), (24, 14), (7, 26), (24, 26), (39, 26), (56, 28), (56, 13), (37, 11), (43, 15), (40, 3)):
    if O[y][x] == ' ':
        put(x, y, '*')
# edificio grande: bandidos y fuego
for x, y in ((38, 12), (42, 12), (37, 15), (42, 15)):
    put(x, y, 'a')
put(36, 11, 'F'); put(44, 16, 'F')
# escotilla al bunker, dentro de la casa del sur
put(42, 28, 'H')
# camion detenido en el este de la calle principal + el Chofer
ofill(52, 21, 55, 22, 'K')
put(56, 22, 'V'); put(57, 22, 'v')
put(58, 22, 'U')
STOP = (58, 21)      # donde aparece el jugador al viajar hasta aqui
put(60, 21, 'L')
# barriles con fuego en las calles
for x, y in ((13, 21), (29, 8), (45, 33), (13, 8), (49, 21), (21, 34), (60, 34)):
    put(x, y, 'F')
# faroles
for x, y in ((9, 18), (9, 21), (22, 18), (22, 21), (37, 18), (37, 22), (54, 18), (14, 7), (46, 6), (30, 33), (48, 33), (5, 8), (60, 8), (6, 34)):
    if O[y][x] == ' ':
        put(x, y, 'L')
# autos
for x, y in ((10, 19), (24, 20), (41, 19), (33, 33), (18, 6), (50, 7), (3, 32), (20, 19), (46, 32), (56, 33)):
    if O[y][x] == ' ' and O[y][x + 1] == ' ':
        put(x, y, 'V'); put(x + 1, y, 'v')
# charco radiactivo chico
for y in range(34, 38):
    for x in range(51, 59):
        if ((x - 54.5) / 3.6) ** 2 + ((y - 36) / 1.9) ** 2 <= 1:
            G[y][x] = '~'
put(53, 36, 'c')
# zombis
for x, y in ((8, 20), (17, 12), (27, 20), (33, 8), (44, 20), (13, 33), (22, 30), (5, 35), (36, 33), (48, 14), (60, 20), (26, 6)):
    if O[y][x] == ' ':
        put(x, y, 'd')
for x, y in ((24, 19), (50, 33), (17, 25)):
    if O[y][x] == ' ':
        put(x, y, 'm')
put(7, 14, 'd'); put(40, 27, 'd'); put(56, 14, 'd')
put(31, 25, 'B')
put(51, 36, 'd'); put(57, 36, 'd'); put(55, 34, 'd')     # junto al charco: nacen irradiados
for x, y in ((12, 22), (35, 7), (56, 20)):
    put(x, y, 'k')
put(30, 21, 'z'); put(20, 8, 'z')
for x, y in ((5, 7), (59, 33)):
    put(x, y, 'x')
# arboles muertos sueltos
n = 0
while n < 22:
    x, y = rnd1.randrange(1, W - 1), rnd1.randrange(1, H - 1)
    if G[y][x] != ',' or O[y][x] != ' ':
        continue
    if any(O[yy][xx] != ' ' for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2)):
        continue
    put(x, y, 'T')
    n += 1


def reach(sx, sy):
    seen = {(sx, sy)}
    st = [(sx, sy)]
    while st:
        x, y = st.pop()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen and O[ny][nx] in ' xkzM':
                seen.add((nx, ny))
                st.append((nx, ny))
    return seen


R = reach(3, 20)
HATCH_ARR = (42, 29)
for nm, (tx, ty) in (('escotilla', (42, 28)), ('chofer', (58, 22)), ('parada', STOP), ('bajo escotilla', HATCH_ARR)):
    print('alcanzable', nm, any((tx + dx, ty + dy) in R for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0), (0, 0))))
print('casas con puerta alcanzables:',
      [(x, y) for x, y in ((6, 5), (23, 5), (40, 5), (55, 5), (7, 17), (24, 17), (39, 17), (56, 16), (7, 23), (24, 23), (39, 23), (56, 25))
       if (x, y) in R or (x, y) not in R and any((x + dx, y + dy) in R for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)))][:0])
ZONES.append(([''.join(r) for r in G], [''.join(r) for r in O]))

with open(os.path.join(ROOT, 'src', 'map1.h'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('// GENERADO por tools/make_map.py: no editar a mano\n')
    f.write(f'#define MW {W}\n#define MH {H}\n#define NZONE_MAP {len(ZONES)}\n')
    f.write(f'#define Z1_STOP_X {STOP[0]}\n#define Z1_STOP_Y {STOP[1]}\n')
    f.write(f'#define Z1_HATCH_X {HATCH_ARR[0]}\n#define Z1_HATCH_Y {HATCH_ARR[1]}\n')
    for nm, idx in (('ZMAP_G', 0), ('ZMAP_O', 1)):
        f.write(f'static const char *{nm}[NZONE_MAP][MH] = {{\n')
        for zg in ZONES:
            f.write('  {\n' + ''.join(f'    "{r}",\n' for r in zg[idx]) + '  },\n')
        f.write('};\n')
for zg in ZONES:
    for g, o in zip(*zg):
        print(''.join(oc if oc != ' ' else gc for gc, oc in zip(g, o)))
    print()
