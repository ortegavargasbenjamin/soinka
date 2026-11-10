# Soinnka: arma el atlas con pixel art hecho a mano, sacado de tres paquetes descargados.
#   A = PostApocalypse_AssetPack (TheLazyStone): personaje, enemigos, armas y objetos
#   B = Zombie Apocalypse Tileset (Ittai Manero): suelo, camino, pasto, rejas, vegetacion, muros
#   C = StalkerLike (origen y licencia sin identificar; solo uso personal): SOLO las armas (en la mano, fogonazo,
#       bala, casquillo, iconos de armas y de municion), los autos, la interfaz (mochila, barra de vida, mira), el catre (la cama de
#       InteriorObjects) y el equipo (la mascara de gas y los tres chalecos de Equipment, la caja del kit de ItemsIcons).
#       [stalk] Ademas: arboles (Tree), muros de casa (HouseVillage3 y HouseWalls), rejas (WoodFence, ConcretFence),
#       el cofre (Chest), objetos de patio (OutsideObjects1 y 2) y los bandidos (Bandit1-3). Lo demas de C no se usa.
# Los paquetes NO van en el repo (la licencia de A prohibe redistribuir): se leen desde su carpeta en
# Descargas, o desde la que diga la variable SOINNKA_ARTE. Lo que los paquetes no traen (letras, iconos,
# efectos, vias, contenedores...) lo sigue dibujando tools/make_art.py, que aqui se importa. La botella de agua
# no viene en ningun paquete: se dibuja aqui mismo, por codigo (ver CODE).
#   assets/atlas.rgba  -> textura cruda de 512x256 (va dentro del EBOOT)
#   src/atlas.h        -> donde quedo cada sprite y cada letra (mismo formato que make_art.py)
#   build/atlas_x3.png -> lo usado del atlas, ampliado, para mirarlo
# Uso: python tools/make_atlas.py
import glob
import os
import random
import sys

from PIL import Image, ImageOps

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_art as code   # noqa: E402  (dibuja por codigo en su propio atlas; al importarlo no escribe nada)

ROOT = code.ROOT
ARTE = os.environ.get('SOINNKA_ARTE', os.path.join(os.path.expanduser('~'), 'Downloads', '_soinnka_arte'))
A = os.path.join(ARTE, 'PostApocalypse_AssetPack_v1.1.2')
B = os.path.join(ARTE, 'Zombie Apocalypse Tileset', 'Zombie Apocalypse Tileset', 'Organized separated sprites')
C = os.path.join(ARTE, 'StalkerLike', 'Sprites')
AW, AH = 512, 512   # entra de sobra; si algun dia no, puede crecer a 512x512 (el motor usa estas constantes)

for d in (A, B, C):
    if not os.path.isdir(d):
        raise SystemExit(f'falta el paquete de arte: {d}\n(los paquetes no van en el repo; ver "Arte" en el README)')


# ---------------------------------------------------------------- leer y retocar
def a(rel):
    """Una imagen del paquete A."""
    return Image.open(os.path.join(A, *rel.split('/'))).convert('RGBA')


def b(idx):
    """Una pieza del paquete B por su numero (los archivos se llaman ..._0123_Capa-124.png)."""
    hits = glob.glob(os.path.join(B, '*', f'*_{idx:04d}_*.png'))
    if not hits:
        raise SystemExit(f'no encuentro la pieza {idx} del paquete B')
    return Image.open(hits[0]).convert('RGBA')


def frames(rel, n):
    """Corta una tira de n cuadros iguales del paquete A."""
    im = a(rel)
    w = im.size[0] // n
    return [im.crop((k * w, 0, (k + 1) * w, im.size[1])) for k in range(n)]


def trim(im):
    return im.crop(im.getbbox())


def cell(im, w, h, bottom=False):
    """Centra la imagen en un recuadro de w x h (o la apoya abajo)."""
    out = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    out.paste(im, ((w - im.size[0]) // 2, h - im.size[1] if bottom else (h - im.size[1]) // 2))
    return out


def dark(im, k):
    """Oscurece: el juego es de noche y los grises claros de B encandilan."""
    r, g, bl, al = im.split()
    f = lambda v: int(v * k)   # noqa: E731
    return Image.merge('RGBA', (r.point(f), g.point(f), bl.point(f), al))


def solid(im, fill):
    """Tapa los pixeles transparentes de una baldosa."""
    out = Image.new('RGBA', im.size, fill + (255,))
    out.alpha_composite(im)
    return out


def swap(im, old, new):
    p = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            if p[x, y][:3] == old and p[x, y][3]:
                p[x, y] = new
    return im


def cut_corners(im, col):
    """Vuelve transparente el fondo de color `col` que toca las esquinas (los arbustos de B traen tierra detras)."""
    w, h = im.size
    p = im.load()
    todo = [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]
    while todo:
        x, y = todo.pop()
        if 0 <= x < w and 0 <= y < h and p[x, y][3] and p[x, y][:3] == col:
            p[x, y] = (0, 0, 0, 0)
            todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return im


def stack(*parts):
    """Apila piezas de arriba hacia abajo, centradas."""
    w = max(p.size[0] for p in parts)
    out = Image.new('RGBA', (w, sum(p.size[1] for p in parts)), (0, 0, 0, 0))
    y = 0
    for p in parts:
        out.paste(p, ((w - p.size[0]) // 2, y))
        y += p.size[1]
    return out


def opened(im):
    """Cajon abierto: sin tapa, se ve el hueco oscuro."""
    im = im.copy()
    p = im.load()
    w = im.size[0]
    for y in range(1, 6):
        for x in range(2, w - 2):
            p[x, y] = (24, 17, 22, 255) if y < 5 else (44, 31, 34, 255)
    return im


# ---------------------------------------------------------------- paquete C (StalkerLike): armas, autos e interfaz
def c(name):
    """Una hoja del paquete C."""
    return Image.open(os.path.join(C, name + '.png')).convert('RGBA')


INK = (0x05, 0x04, 0x03, 255)                                    # el contorno de las armas de C
SKIN, SKIN2 = (0xe6, 0xc2, 0xb0, 255), (0xba, 0x86, 0x8f, 255)   # las manos del personaje de A


def saw(im, x0, x1):
    """Corta un arma a lo largo y cierra los cortes: la escopeta de C es entera y la del juego es recortada."""
    im = im.crop((x0, 0, x1, im.size[1]))
    p = im.load()
    for y in range(im.size[1]):
        for x in (0, im.size[0] - 1):
            if p[x, y][3]:
                p[x, y] = INK
    return trim(im)


def hands(im, *spots):
    """Le pone manos al arma (el cuerpo de A viene sin manos): cada una son 2x2 px con la esquina en (x, y)."""
    out = Image.new('RGBA', (im.size[0], max(im.size[1], max(y for _, y in spots) + 2)), (0, 0, 0, 0))
    out.paste(im, (0, 0))
    for x, y in spots:
        for dx in (0, 1):
            out.putpixel((x + dx, y), SKIN)
            out.putpixel((x + dx, y + 1), SKIN2)
    return out


def half(im):
    """A la mitad: en cada bloque de 2x2 gana el color que mas se repite (si empatan, el mas claro)."""
    out = Image.new('RGBA', (im.size[0] // 2, im.size[1] // 2), (0, 0, 0, 0))
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            px = [im.getpixel((2 * x + dx, 2 * y + dy)) for dx in (0, 1) for dy in (0, 1)]
            px = [q for q in px if q[3]]
            if len(px) >= 2:
                out.putpixel((x, y), max(px, key=lambda q: (px.count(q), sum(q[:3]))))
    return out


def lit(im, k):
    """Aclara el metal (los grises) de un arma: las de C son casi negras y de noche, o sobre un casillero, se pierden."""
    im = im.copy()
    p = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            q = p[x, y]
            if q[3] and q != INK and max(q[:3]) - min(q[:3]) < 24:
                p[x, y] = tuple(min(255, int(v * k)) for v in q[:3]) + (q[3],)
    return im


def retip(im, rows, dark, light):
    """Otra punta para una bala (los tipos de municion): en las primeras `rows` filas, lo claro pasa a `light` y lo
    demas a `dark`; el contorno queda."""
    im = im.copy()
    p = im.load()
    for y in range(rows):
        for x in range(im.size[0]):
            q = p[x, y]
            if q[3] and sum(q[:3]) > 120:
                p[x, y] = (light if sum(q[:3]) > 480 else dark) + (255,)
    return im


def widen(im, at, n):
    """Alarga una pieza repitiendo n veces la columna `at`."""
    w, h = im.size
    out = Image.new('RGBA', (w + n, h), (0, 0, 0, 0))
    out.paste(im.crop((0, 0, at, h)), (0, 0))
    out.paste(im.crop((at, 0, at + 1, h)).resize((n, h), Image.NEAREST), (at, 0))
    out.paste(im.crop((at, 0, w, h)), (at + n, 0))
    return out


def abandoned(im, seed, k, grey, blobs):
    """Auto abandonado: mas oscuro (es de noche), destenido, con manchas de oxido (mas abajo) y mugre."""
    rnd = random.Random(seed)
    im = im.copy()
    w, h = im.size
    p = im.load()
    spots = [(rnd.randrange(w), rnd.randrange(h // 3, h - 4), rnd.choice((2, 2, 3, 4))) for _ in range(blobs)]
    for y in range(h):
        for x in range(w):
            r, g, bl, al = p[x, y]
            if al < 255:
                continue   # la sombra queda como viene
            lum = (r * 3 + g * 6 + bl) / 10
            col = [v + (lum - v) * grey for v in (r, g, bl)]
            if lum > 40:   # ni el contorno ni los vidrios
                if any((x - sx) ** 2 + (y - sy) ** 2 <= rad * rad and rnd.random() < 0.8 for sx, sy, rad in spots):
                    col = [v * 0.35 + q * 0.65 for v, q in zip(col, (0x86, 0x48, 0x2a))]
                elif rnd.random() < 0.10:
                    col = [v * 0.78 for v in col]
            p[x, y] = tuple(int(v * k) for v in col) + (255,)
    return im


weapons, wicons = c('Weapons'), c('Weapons_Icons')
pistol = lit(trim(weapons.crop((128, 0, 160, 16))), 1.4)       # la pistola oscura
sawed = lit(saw(weapons.crop((64, 0, 96, 16)), 4, 23), 1.4)    # la escopeta, sin media culata ni medio canon
rifle = lit(saw(weapons.crop((0, 16, 32, 32)), 7, 29), 1.4)    # el fusil AK, sin la culata (en la mano quedaria muy largo)
muzzle, casing = c('muzzleflash-sheet'), c('casing')
arrow = half(c('CrossHair').crop((26, 3, 38, 25)))             # una punta de la mira (mira hacia abajo)
GAP = 14                                                       # hueco entre puntas: ahi cabe un zombi
reticle = Image.new('RGBA', (2 * arrow.size[1] + GAP,) * 2, (0, 0, 0, 0))
for k, (ax, ay) in enumerate(((0.5, 0), (0, 0.5), (0.5, 1), (1, 0.5))):   # arriba, izquierda, abajo, derecha
    tip = arrow.rotate(90 * k, expand=True)
    reticle.paste(tip, (int((reticle.size[0] - tip.size[0]) * ax), int((reticle.size[1] - tip.size[1]) * ay)))
hp = c('HP')
inv = c('InventoryUI_Overhoul1')
items, ammo, equip = c('ItemsIcons'), c('Ammo'), c('Equipment')
b9, b762, shell = trim(ammo.crop((48, 0, 64, 16))), trim(ammo.crop((0, 0, 16, 16))), trim(ammo.crop((32, 0, 48, 16)))
STEEL, RED = ((52, 58, 70), (150, 162, 180)), ((150, 40, 36), (236, 88, 60))   # punta perforante y punta expansiva
slug = swap(swap(shell.copy(), (210, 31, 58), (70, 150, 84, 255)), (97, 38, 56), (38, 86, 58, 255))   # cartucho verde: bala unica
cars = c('Cars')
cars = cars.resize((cars.size[0] // 5, cars.size[1] // 5), Image.NEAREST)   # la hoja viene ampliada x5

EXTRA = [   # sprites nuevos, todos de C (van al final del enum)
    ('CAR0', abandoned(cars.crop((25, 9, 75, 40)), 7, 0.62, 0.35, 9)),      # sedan rojo de costado: 50x31 con su sombra
    ('CAR1', abandoned(cars.crop((148, 10, 198, 41)), 3, 0.55, 0.6, 16)),   # sedan azul: mas oxidado, casi chatarra
    ('RETICLE', reticle),
    ('HP_FRAME', widen(hp.crop((1, 16, 59, 32)), 30, 26)),                  # marco de la barra de vida, alargado
    ('HP_FILL', widen(hp.crop((1, 41, 63, 55)), 30, 26)),
    ('UI_SLOT', inv.crop((3, 17, 15, 29))),                                 # un casillero (plano: se estira)
    ('UI_BG', inv.crop((1, 1, 7, 7))),                                      # fondo de panel
    ('UI_EDGE', inv.crop((0, 0, 6, 1)).resize((6, 6), Image.NEAREST)),      # borde de panel
    ('UI_MARK', c('SlotMarker')),                                           # esquinas que marcan un casillero
    ('IT_MED', trim(items.crop((0, 0, 16, 16)))), ('IT_ARAD', trim(items.crop((16, 0, 32, 16)))),
    ('IT_BAND', trim(items.crop((0, 16, 16, 32)))), ('IT_FOOD', trim(items.crop((48, 16, 64, 32)))),
    ('IT_12G', trim(ammo.crop((32, 0, 48, 16)))), ('IT_9MM', trim(ammo.crop((48, 0, 64, 16)))),
    ('W_PISTOL', lit(trim(wicons.crop((64, 0, 96, 32))), 1.8)), ('W_SHOTGUN', lit(saw(wicons.crop((32, 0, 64, 32)), 4, 23), 1.8)),
    ('W_AK', lit(trim(wicons.crop((0, 32, 32, 64))), 1.8)),                    # el fusil AK entero, con su culata
    ('IT_762', b762),                                                          # bala de fusil, de vaina verde
    ('IT_9MM_AP', retip(b9, 3, *STEEL)), ('IT_9MM_HP', retip(b9, 3, *RED)),        # tipos de municion: cambia la punta
    ('IT_762_AP', retip(b762, 4, *STEEL)), ('IT_762_HP', retip(b762, 4, *RED)),
    ('IT_12G_SLUG', slug),
    ('CASING', casing.crop((10, 12, 20, 20)).resize((5, 4), Image.NEAREST)),   # vienen al doble
    ('SHELL', casing.crop((40, 44, 56, 52)).resize((8, 4), Image.NEAREST)),
    ('COT', trim(c('InteriorObjects').crop((0, 0, 16, 32)))),                  # el catre: la cama con la manta doblada, 10x19
    # equipo: la mascara de gas y los tres chalecos de Equipment (cafe = chaqueta reforzada, verde = chaleco ligero,
    # negro = chaleco pesado, aclarado para que no se pierda en el casillero) y la caja del kit de limpieza
    ('IT_MASK', trim(equip.crop((16, 0, 32, 16)))),
    ('IT_JACKET', trim(equip.crop((48, 0, 64, 16)))), ('IT_VEST', trim(equip.crop((80, 0, 96, 16)))),
    ('IT_HVEST', lit(trim(equip.crop((64, 0, 80, 16))), 1.7)),
    ('IT_CLEAN', trim(items.crop((32, 0, 48, 16)))),
]

# ---------------------------------------------------------------- dibujado por codigo aqui
def pixels(rows, pal):
    """Un dibujo chico a partir de filas de letras: `pal` da el color de cada letra ('.' = transparente)."""
    im = Image.new('RGBA', (len(rows[0]), len(rows)), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != '.':
                im.putpixel((x, y), pal[ch])
    return im


# La botella de agua: tinta de los iconos de C, tapa clara, vidrio y agua con su brillo
WATER = {'K': (0x0b, 0x10, 0x16, 255), 'w': (206, 210, 214, 255), 'g': (118, 146, 164, 255),
         'l': (186, 222, 244, 255), 'B': (62, 132, 200, 255), 'D': (40, 96, 160, 255)}
PART = {'K': (0x0b, 0x10, 0x16, 255), 'h': (176, 184, 196, 255), 'm': (112, 122, 138, 255), 'd': (66, 74, 90, 255),
        'g': (120, 170, 190, 255), 'R': (240, 70, 50, 255), 'r': (176, 44, 40, 255),
        'w': (150, 100, 62, 255), 'W': (180, 128, 80, 255), 'b': (104, 66, 44, 255),
        'f': (96, 124, 92, 255), 'F': (140, 170, 124, 255), 'D': (60, 84, 62, 255),
        'v': (104, 122, 96, 255), 'V': (150, 166, 136, 255),
        'c': (214, 208, 190, 255), 'C': (160, 152, 134, 255), 'e': (96, 214, 120, 255), 'E': (52, 150, 82, 255),
        'p': (70, 66, 62, 255), 'P': (118, 112, 104, 255)}
XP = dict(PART, y=(255, 222, 90, 255), o=(240, 150, 50, 255), O=(190, 100, 34, 255), l=(210, 236, 244, 255))
ART_ROWS = (    # artefacto: un cristal facetado; tres tonos (a claro, A medio, z oscuro) cambian por artefacto
    '....K....', '...KaK...', '..KaAAK..', '.KaAAAAK.', 'KaAAaAAzK', '.KAAAAzK.', '..KAAzK..', '...KzK...', '....K....')
ART_COL = (((255, 214, 110), (238, 150, 40), (160, 90, 24)), ((255, 150, 120), (220, 60, 40), (120, 28, 28)),
           ((210, 220, 235), (140, 155, 175), (80, 92, 110)), ((170, 220, 255), (70, 140, 230), (34, 70, 140)),
           ((220, 190, 255), (150, 100, 220), (80, 52, 130)), ((190, 255, 170), (90, 200, 80), (40, 110, 48)))
def snow_tile(seed, road=False):
    """Baldosa de nieve de 16x16 dibujada por codigo, en tonos frios que combinan con la noche. road=True: camino nevado
    (nieve pisada y gris con dos huellas de rueda)."""
    rng = random.Random(seed)
    base = (96, 106, 124) if road else (142, 158, 184)
    lite = (124, 134, 152) if road else (196, 210, 230)
    dark_ = (70, 78, 96) if road else (112, 128, 156)
    im = Image.new('RGBA', (16, 16), base + (255,))
    px = im.load()
    for _ in range(26):   # motas claras y sombras suaves
        x, y = rng.randrange(16), rng.randrange(16)
        px[x, y] = (lite if rng.random() < 0.55 else dark_) + (255,)
    for _ in range(5):    # algun grumo de dos pixeles
        x, y = rng.randrange(15), rng.randrange(16)
        px[x, y] = px[x + 1, y] = lite + (255,)
    if road:
        for x in range(16):
            for y in (4, 5, 10, 11):
                px[x, y] = (62, 70, 88, 255) if y in (5, 11) else (78, 86, 104, 255)
    return im


CODE = [   # sprites nuevos dibujados por codigo (van despues de los de C)
    ('SNOW0', snow_tile(11)), ('SNOW1', snow_tile(23)), ('SNOW2', snow_tile(37)), ('SNOWROAD', snow_tile(51, True)),   # suelo de las expediciones nevadas
    ('I_WATER', pixels((   # chica: tirada en el suelo y en la barra de sed
        '...ww...',
        '..KggK..',
        '.KlBBBK.',
        '.KlBBBK.',
        '.KBBBBK.',
        '.KBBBDK.',
        '.KBBDDK.',
        '..KKKK..'), WATER)),
    ('IT_WATER', pixels((  # la del casillero de la mochila
        '..KKKK..',
        '..KwwK..',
        '..KwwK..',
        '..KggK..',
        '.KggggK.',
        'KglggggK',
        'KBlBBBBK',
        'KBlBBBBK',
        'KBBBBBBK',
        'KBBBBBDK',
        'KBBBBBDK',
        'KBBBBDDK',
        '.KKKKKK.'), WATER)),
    # piezas de armas (ningun paquete las trae sueltas): metal en tres tonos, madera y el punto rojo de la mira
    ('IT_SIL', pixels((    # silenciador: un tubo con anillos y la rosca
        '..KKKKKKKKKKKKK.',
        'KKKhmhhmhhmhhmhK',
        'KhKmmmmmmmmmmmmK',
        'KmKmdmmdmmdmmdmK',
        'KKKddddddddddddK',
        '..KKKKKKKKKKKKK.'), PART)),
    ('IT_COMP', pixels((   # compensador: corto, con ranuras
        '..KKKKKKKK',
        'KKKhKhKhKK',
        'KhKhKhKhhK',
        'KmKmmmmmmK',
        'KmKmKmKmdK',
        'KKKdKdKdKK',
        '..KKKKKKKK'), PART)),
    ('IT_XMAG', pixels((   # cargador ampliado: largo y curvo
        'KKKKKK...',
        'KhmmdK...',
        'KhmmdK...',
        'KhmmdK...',
        '.KhmmdK..',
        '.KhmmdK..',
        '.KhKKdK..',
        '..KhmmdK.',
        '..KhmmdK.',
        '..KhKKdK.',
        '...KhmmdK',
        '...KhmmdK',
        '...KKKKKK'), PART)),
    ('IT_REFLEX', pixels(( # mira reflex: el marco con el vidrio y el punto rojo, sobre su base
        '..KKKKKKK...',
        '..KhhhhhK...',
        '..KhgggmK...',
        '..KhgRgmK...',
        '..KhgggmK...',
        '..KmmmmmKKK.',
        '.KKKKKKKhmK.',
        'KhmmmmmmmmdK',
        'KKdKKKKKKdKK',
        '.KKK....KKK.'), PART)),
    ('IT_STOCK', pixels((  # culata de madera con su cantonera
        'KKKKKK..........',
        'KdwwwwKKKKK.....',
        'KdwWwwwwwwwKKKK.',
        'KdwwwwWwwwwwwhmK',
        'KdwbwwwwwWwwwhmK',
        'KdwbbbwwwwwwwhmK',
        'KdbbbKKKKbbbbbmK',
        'KdbbK....KKKKKKK',
        'KKKK............'), PART)),
    ('IT_FILTER', pixels(( # filtro de la mascara: un bote con rejilla
        '..KKKKKK..',
        '.KhhhhhhK.',
        'KKKKKKKKKK',
        'KfFfFfFfFK',
        'KFfFfFfFfK',
        'KfFfFfFfDK',
        'KKKKKKKKKK',
        '.KmmmmmdK.',
        '..KKKKKK..'), PART)),
    ('IT_SEW', pixels((    # kit de costura: carrete de hilo rojo con la aguja
        '.KKKKKKKK..K.',
        '.KwwwwwwK.KhK',
        '..KKKKKK.KhK.',
        '..KrRrrKKhK..',
        '..KRrrRKhK...',
        '..KrrRrhK....',
        '..KrRrhKK....',
        '..KKKhKK.....',
        '.KwwhwwwK....',
        '.KKKKKKKK....'), PART)),
    ('IT_RAG', pixels((    # trapos: un fardo de tela doblada
        '..KKKKKKKK..',
        '.KccccCccccK',
        'KcCccccccCcK',
        'KccccKKccccK',
        'KCcccKccccCK',
        'KccCcccccccK',
        '.KcccCcccCK.',
        '..KKKKKKKK..'), PART)),
    ('IT_CHEM', pixels((    # quimicos: un frasco con liquido verde
        '...KKKK...',
        '...KwwK...',
        '...KhhK...',
        '..KhggmK..',
        '.KheEeEmK.',
        '.KeEeeEEmK',
        '.KEeeEeEdK',
        '.KEEeEEEdK',
        '..KKKKKKK.'), PART)),
    ('IT_MECH', pixels((   # piezas mecanicas: un engranaje
        '....KKK....',
        '.K..KhK..K.',
        '.KKKKmKKKK.',
        '..KhmmmmmK.',
        'KKhmK...KmK',
        'KhmmK...KdK',
        'KKmmdKKKdKK',
        '..KmddddmK.',
        '.KKKKdKKKK.',
        '.K..KdK..K.',
        '....KKK....'), PART)),
    ('IT_POWDER', pixels(( # polvora: un saquito atado con su monton negro
        '...KKKK...',
        '....KK....',
        '..KwWWwK..',
        '.KwWwwWwK.',
        'KwWpPpwWwK',
        'KwpPppPpwK',
        'KwpppPppwK',
        '.KbbbbbbK.',
        '..KKKKKK..'), PART)),
    ('IT_NUT', pixels((    # tuerca: se lanza para hacer ruido y delatar anomalias
        '...KKKK...',
        '..KhhmmK..',
        '.KhmKKmmK.',
        'KhmK..KmdK',
        'KmmK..KddK',
        '.KmmKKddK.',
        '..KmddddK.',
        '...KKKK...'), XP)),
    ('IT_GREN', pixels((   # granada
        '...K.KK...',
        '...KKmK...',
        '..KKKKKK..',
        '.KFFfffDK.',
        'KFfFffffDK',
        'KfFfffffDK',
        'KfffffffDK',
        'KfffffffDK',
        '.KfffffDK.',
        '..KKKKKK..'), XP)),
    ('IT_MOLO', pixels((   # cocktel incendiario: botella con trapo encendido
        '...yy...',
        '..yoyy..',
        '..KrrK..',
        '..KKKK..',
        '..KggK..',
        '.KgggoK.',
        'KgogooOK',
        'KgoooOOK',
        'KgoooOOK',
        'KgooOOOK',
        '.KOOOOK.',
        '..KKKK..'), XP)),
    ('IT_MINE', pixels((   # mina
        '....KK....',
        '..KKrrKK..',
        '.KmmmmmmK.',
        'KhmmddmmdK',
        'KmmdKKdmdK',
        '.KdmmmmdK.',
        '..KKKKKK..'), XP)),
    ('IT_TRAP', pixels((   # cepo
        '.K.K.K.K.K..',
        '.KhKhKhKhKh.',
        'KKmmmmmmmmKK',
        'KdKKKrrKKKdK',
        'KdmmmmmmmmdK',
        '.KKKKKKKKKK.'), XP)),
    ('IT_DET', pixels((    # detector de anomalias: caja con antena y pantalla verde
        '.......K.',
        '......KK.',
        '.KKKKKKK.',
        'KmmmmmmmK',
        'KmKKKKKmK',
        'KmKeeEKmK',
        'KmKeEeKmK',
        'KmKKKKKmK',
        'KmrmmgmmK',
        'KmmmmmmmK',
        '.KKKKKKK.'), XP)),
    *[(f'IT_ART{i}', pixels(ART_ROWS, dict(XP, a=c[0] + (255,), A=c[1] + (255,), z=c[2] + (255,)))) for i, c in enumerate(ART_COL)],
    # chicos (8x8): lo tirado en el suelo y las barras del HUD
    ('I_PART', pixels((    # una pieza: tuerca
        '..KKKK..',
        '.KhhmmK.',
        'KhmKKmmK',
        'KhK..KmK',
        'KmK..KdK',
        'KmmKKddK',
        '.KmmddK.',
        '..KKKK..'), PART)),
    ('I_VEST', pixels((    # armadura
        '.KK..KK.',
        'KvvKKvvK',
        'KvVvvVvK',
        '.KvVVvK.',
        '.KvvvvK.',
        '.KvVVvK.',
        '.KvvvvK.',
        '..KKKK..'), PART)),
    ('I_MASK', pixels((    # mascara de gas
        '..KKKK..',
        '.KmmmmK.',
        'KmggggmK',
        'KmgKKgmK',
        '.KmmmmK.',
        '.KKFFKK.',
        '..KffK..',
        '..KKKK..'), PART)),
]

# ---- armas nuevas dibujadas por codigo (cuerpo a cuerpo y de fuego): se usan en la mano y como icono
def weapon_art(w, h, shapes):
    """Dibuja un arma horizontal (empunadura a la izquierda, punta a la derecha) con rectangulos y poligonos y le pone borde oscuro."""
    from PIL import ImageDraw
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for kind, pts, col in shapes:
        if kind == 'r':
            d.rectangle(pts, fill=col + (255,))
        else:
            d.polygon(pts, fill=col + (255,))
    out = Image.new('RGBA', (w + 2, h + 2), (0, 0, 0, 0))
    out.alpha_composite(im, (1, 1))
    px, src = out.load(), im.load()
    for y in range(h):
        for x in range(w):
            if src[x, y][3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and src[nx, ny][3]:
                    px[x + 1, y + 1] = (11, 16, 22, 255)
                    break
    return out
_ST, _MD, _DK, _WD, _WB, _SK, _BK, _RS, _LN = (176, 184, 196), (112, 122, 138), (66, 74, 90), (150, 100, 62), (104, 66, 44), (222, 172, 132), (34, 34, 42), (150, 84, 56), (120, 170, 190)
CODE += [
    ('W_KNIFE', weapon_art(12, 3, [('r', (0, 1, 4, 2), _WD), ('r', (4, 0, 4, 2), _DK), ('r', (5, 0, 11, 1), _ST), ('r', (5, 2, 10, 2), _MD), ('r', (1, 1, 2, 2), _SK)])),
    ('W_AXE', weapon_art(16, 9, [('r', (0, 4, 12, 5), _WD), ('p', [(10, 2), (14, 0), (15, 0), (15, 8), (14, 8), (10, 6)], _ST), ('r', (10, 3, 13, 5), _MD), ('r', (15, 1, 15, 7), (230, 236, 244)), ('r', (2, 4, 3, 5), _SK)])),
    ('W_PIPE', weapon_art(20, 4, [('r', (0, 1, 19, 2), _MD), ('r', (0, 1, 19, 1), _ST), ('r', (13, 1, 15, 2), _RS), ('r', (1, 1, 2, 2), _SK)])),
    ('W_MAUL', weapon_art(22, 10, [('r', (0, 4, 15, 5), _WD), ('r', (15, 1, 21, 8), _MD), ('r', (15, 1, 21, 2), _ST), ('r', (15, 7, 21, 8), _DK), ('r', (17, 1, 17, 8), _DK), ('r', (19, 1, 19, 8), _DK), ('r', (2, 4, 3, 5), _SK)])),
    ('W_REVOLVER', weapon_art(13, 7, [('r', (6, 1, 12, 2), _DK), ('r', (6, 1, 12, 1), _MD), ('r', (3, 1, 7, 4), _MD), ('r', (4, 0, 6, 3), _ST), ('p', [(1, 3), (4, 3), (4, 6), (2, 6)], _WB), ('r', (1, 3, 2, 4), _SK)])),
    ('W_SMG', weapon_art(17, 8, [('r', (3, 1, 14, 3), _DK), ('r', (14, 1, 16, 2), _MD), ('r', (4, 0, 12, 0), _MD), ('r', (7, 4, 9, 7), _MD), ('r', (3, 4, 5, 6), _BK), ('r', (0, 2, 3, 3), _DK), ('r', (3, 4, 4, 5), _SK)])),
    ('W_PUMP', weapon_art(26, 6, [('r', (0, 2, 6, 4), _WD), ('r', (6, 1, 12, 3), _DK), ('r', (12, 1, 25, 2), _MD), ('r', (12, 3, 22, 3), _DK), ('r', (13, 3, 19, 5), _WB), ('r', (7, 3, 8, 4), _SK), ('r', (15, 4, 16, 5), _SK)])),
    ('W_BOLT', weapon_art(30, 7, [('r', (9, 0, 19, 1), _DK), ('r', (19, 0, 19, 1), _LN), ('r', (8, 2, 13, 4), _DK), ('r', (13, 2, 29, 3), _MD), ('r', (13, 2, 29, 2), _ST), ('r', (0, 3, 8, 5), _WD), ('r', (9, 4, 10, 5), _SK), ('r', (17, 4, 18, 5), _SK)])),
]

# ---------------------------------------------------------------- personajes del paquete A
def cast(name, idle, run, nrun):
    """3 vistas (F frente, B espalda, S perfil); por vista 6 cuadros de quieto (I) y nrun de andar (R)."""
    out = []
    for v, fi, fr in zip('FBS', idle, run):
        out += [(f'{name}_{v}I{k}', im) for k, im in enumerate(frames(fi, 6))]
        out += [(f'{name}_{v}R{k}', im) for k, im in enumerate(frames(fr, nrun))]
    return out


CH, EN = 'Character/Main/', 'Enemies/'
player = cast('PLY',
              [CH + f'Idle/Character_{d}_idle_no-hands-Sheet6.png' for d in ('down', 'up', 'side')],
              [CH + f'Run/Character_{d}_run_no-hands-Sheet6.png' for d in ('down', 'up', 'side')], 6)
zsmall = cast('ZS',   # zombi (chico): el que ataca cuerpo a cuerpo (E_DOG en el codigo)
              [EN + f'Zombie_Small/Zombie_Small_{d}_Idle-Sheet6.png' for d in ('Down', 'Up', 'Side')],
              [EN + f'Zombie_Small/Zombie_Small_{d}-Sheet6.png' for d in ('Down_walk', 'Up_Walk', 'Side_Walk')], 6)
# al morir caen de lado (solo hay muerte de perfil): 6 cuadros, el ultimo queda de cadaver
zsmall += [(f'ZS_DIE{k}', im) for k, im in enumerate(frames(EN + 'Zombie_Small/Zombie_Small_Side_First-Death-Sheet6.png', 6))]
zaxe = cast('ZA',     # zombi del hacha: el que ataca de lejos, tirando hachas (E_MAR en el codigo)
            [EN + f'Zombie_Axe/Zombie_Axe_{d}_Idle-Sheet6.png' for d in ('Down', 'Up', 'Side')],
            [EN + f'Zombie_Axe/Zombie_Axe_{d}_Walk-Sheet8.png' for d in ('Down', 'Up', 'Side')], 8)
zaxe += [(f'ZA_DIE{k}', im) for k, im in enumerate(frames(EN + 'Zombie_Axe/Zombie_Axe_Side_First-Death-Sheet6.png', 6))]
zbig = cast('ZB',     # zombi grande: lento y pesado (E_BIG en el codigo; el jefe es el mismo, mas grande y verde)
            [EN + f'Zombie_Big/Zombie_Big_{d}_Idle-Sheet6.png' for d in ('Down', 'Up', 'Side')],
            [EN + f'Zombie_Big/Zombie_Big_{d}_Walk-Sheet8.png' for d in ('Down', 'Up', 'Side')], 8)
_bd = frames(EN + 'Zombie_Big/Zombie_Big_Side_First-Death-Sheet7.png', 7)
zbig += [(f'ZB_DIE{k}', _bd[j]) for k, j in enumerate((0, 1, 2, 3, 4, 6))]


def recolor(im, base):
    """Cambia la ropa verde del jugador por otro color, conservando sus luces y sombras (para bandidos y guardias)."""
    im = im.copy()
    p = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, bl, al = p[x, y]
            if al and g > r + 8 and g > bl + 8:
                k = g / 115.0
                p[x, y] = (min(255, int(base[0] * k)), min(255, int(base[1] * k)), min(255, int(base[2] * k)), al)
    return im


# bandidos (ropa rojo oscuro) y guardias de Kolos (gris azulado): los mismos cuadros y orden que el jugador
bandits = [(n.replace('PLY_', 'BAN_'), recolor(im, (122, 34, 32))) for n, im in player]
guards = [(n.replace('PLY_', 'GRD_'), recolor(im, (98, 112, 134))) for n, im in player]
# el comerciante (ocre) y el chatarrero (verde azulado) de la base: tambien son el cuerpo del jugador con otra ropa
merchants = [(n.replace('PLY_', 'MER_'), recolor(im, (176, 128, 46))) for n, im in player]
scavs = [(n.replace('PLY_', 'CHA_'), recolor(im, (40, 124, 120))) for n, im in player]
axes = [(f'AXE{k}', im) for k, im in enumerate(frames(EN + 'Zombie_Axe/Axe/Axe_Side_Thrown-Sheet9.png', 9))]

# ---------------------------------------------------------------- que reemplaza a que
# nombre que dibujaba make_art.py -> lista de (nombre, imagen) que ocupa su lugar en el enum
DIRT, TAN, GREY, ASPHALT = (0x47, 0x35, 0x2c), (0x77, 0x66, 0x4e), (0xb6, 0xae, 0xae), (0x21, 0x18, 0x18)
wood = a('Objects/Buildable/Wooden/Wooden-wall_Horizontal.png')
iron = a('Objects/Buildable/Reinforced/Reinforced_wooden-wall_Horizontal.png')
wall_face = dark(b(95).crop((0, 1, 16, 13)), 0.5)
for x in range(16):
    wall_face.putpixel((x, 11), (16, 12, 12, 255))
post = b(209)

SWAP = {
    # --- suelo (B)
    'DIRT0': [('DIRT0', b(77))], 'DIRT1': [('DIRT1', b(78))], 'DIRT2': [('DIRT2', b(79))],
    'GRASS0': [('GRASS0', b(64))], 'GRASS1': [('GRASS1', ImageOps.mirror(b(64)))], 'GRASS2': [('GRASS2', b(80))],
    'ROAD0': [('ROAD0', solid(b(53), ASPHALT))],
    'ROAD1': [('ROAD1', swap(b(44), GREY, ASPHALT + (255,)))],          # el mismo asfalto, con piedrecitas
    'FLOOR0': [('FLOOR0', dark(b(95), 0.5))], 'FLOOR1': [('FLOOR1', dark(b(95).rotate(180), 0.5))],
    'STAIN0': [('STAIN0', cell(dark(b(384), 0.6), 16, 16))], 'STAIN1': [('STAIN1', cell(dark(b(385), 0.6), 16, 16))],
    'BODY': [('BODY', cell(b(290), 16, 16))],
    'BLOOD0': [('BLOOD0', b(379))], 'BLOOD1': [('BLOOD1', b(380))], 'BLOOD2': [('BLOOD2', b(381))],
    # --- muros y cosas del mapa (B)
    'WALL': [('WALL', stack(dark(b(6), 0.78), wall_face))],              # techo de tablas + frente de concreto
    'SANDBAG': [('SANDBAG', b(123))],
    'FENCE_H': [('FENCE_H', b(206))],
    'FENCE_V': [('FENCE_V', stack(post.crop((0, 0, 3, 8)), post))],
    'CROSS': [('CROSS', b(357))],
    'CAR_L': [], 'CAR_R': [],                                           # los autos ahora son de C (EXTRA)
    'TREE0': [('TREE0', stack(b(136), b(137))), ('TREE1', b(138)), ('TREE2', cut_corners(b(82), DIRT))],
    'TREE1': [],
    # --- objetos (A)
    'CRATE': [('CRATE', wood)], 'CRATE_OPEN': [('CRATE_OPEN', opened(wood))],
    'CRATE_JOB': [('CRATE_JOB', iron)], 'CRATE_JOB_OPEN': [('CRATE_JOB_OPEN', opened(iron))],
    'BARREL': [('BARREL', a('Objects/Barrel_rust_red_1.png'))],
    'FBARREL': [('FBARREL', a('Objects/Barrel_rust_blue_1.png'))],
    'LAMP': [('LAMP', a('Objects/Street-Light_3_Down.png'))],
    'TRASH0': [('TRASH0', cell(a('Objects/Iron-beam.png'), 16, 16))],      # restos chicos y apagados: se repiten mucho
    'TRASH1': [('TRASH1', cell(a('Objects/Nature/Flowers_Mashrooms_Other-nature-stuff/Stick.png'), 16, 16))],
    'I_AMMO': [('I_AMMO', cell(a('Objects/Pickable/Ammo-crate_Green.png'), 9, 8))],
    'I_BAND': [('I_BAND', cell(a('Objects/Pickable/Bandage.png'), 8, 8))],
    'I_FOOD': [('I_FOOD', cell(a('Objects/Pickable/Canned-food.png'), 8, 8))],
    # --- personajes (A) y armas (C, con las manos de A)
    'PLY_F0': player + bandits + guards + merchants + scavs + zbig,
    'MAR_F0': zaxe + axes,
    'DOG0': zsmall,
    'PISTOL': [('PISTOL', hands(pistol, (1, 4)))],
    'SHOTGUN': [('SHOTGUN', hands(sawed, (3, 3), (13, 3)))],
    'RIFLE': [('RIFLE', hands(rifle, (1, 5), (11, 4)))],
    'BULLET': [('BULLET', trim(c('bullet1')).resize((12, 3), Image.BOX))],   # la estela viene enorme
    'FLASH': [('FLASH', trim(muzzle.crop((0, 0, 16, 16)))), ('FLASH1', trim(muzzle.crop((16, 0, 32, 16))))],
    'RETICLE': [],                                                      # la mira va en EXTRA (es alta)
}
GONE = ('PLY_', 'MAR_', 'DOG')   # los cuadros viejos de estos ya no existen

# ---------------------------------------------------------------- empaquetar
# Dos paginas de 512x512: se llena la 1 y lo que no quepa pasa solo a la 2 (quien agregue un sprite no tiene que saberlo).
# Las letras van siempre en la pagina 1 (se arman primero, mas abajo).
pages = [Image.new('RGBA', (AW, AH), (0, 0, 0, 0))]
atlas = pages[0]
entries = []
origin = {}   # de donde salio cada sprite: A, B, C o codigo
pos = [0, 0, 0]   # x, y, alto de la fila


def add(name, im, pad=1):
    w, h = im.size
    if pos[0] + w > AW:
        newline(pad)
    if pos[1] + h > AH:
        nextpage(name)
    atlas.paste(im, (pos[0], pos[1]))
    if name:
        entries.append((name, pos[0], pos[1], w, h, len(pages) - 1))
    xy = (pos[0], pos[1])
    pos[0] += w + pad
    pos[2] = max(pos[2], h)
    return xy


def nextpage(name):
    global atlas
    if len(pages) >= 2:
        raise SystemExit(f'las dos paginas del atlas se llenaron en {name}')
    used_p0.append(pos[1] + pos[2])
    pages.append(Image.new('RGBA', (AW, AH), (0, 0, 0, 0)))
    atlas = pages[-1]
    pos[0] = pos[1] = pos[2] = 0


used_p0 = []


def newline(pad=1):
    pos[0] = 0
    pos[1] += pos[2] + pad
    pos[2] = 0


FROM_B = ('DIRT', 'GRASS', 'ROAD', 'FLOOR', 'STAIN', 'BODY', 'BLOOD', 'WALL', 'SANDBAG', 'FENCE', 'CROSS', 'TREE')
FROM_C = ('PISTOL', 'SHOTGUN', 'RIFLE', 'BULLET', 'FLASH')
glyphs = {}
for ch, (x, y, adv) in code.glyphs.items():
    glyphs[ch] = add(None, code.atlas.crop((x, y, x + adv + 1, y + code.FONT_H)), pad=0) + (adv,)
newline()

for name, x, y, w, h in code.entries:
    if name in SWAP:
        for n, im in SWAP[name]:
            add(n, im)
            origin[n] = 'C' if n.startswith(FROM_C) else 'B' if n.startswith(FROM_B) else 'A'
    elif not name.startswith(GONE):
        add(name, code.atlas.crop((x, y, x + w, y + h)))
        origin[name] = 'codigo'

newline()
for n, im in EXTRA:
    add(n, im)
    origin[n] = 'C'
for n, im in CODE:
    add(n, im)
    origin[n] = 'codigo'

# [diseno] Objetos de calle y vegetacion del paquete AbandonedCity (Glerf). Se reducen y se limpia el borde con alfa duro.
CIUDAD = os.path.join(ARTE, 'AbandonedCity_Glerf', 'AbandonedCity_Assets_Glerf_PAID', 'OriginalSize')


def dz(nombre, k):
    im = Image.open(os.path.join(CIUDAD, nombre + '.png')).convert('RGBA')
    im = im.crop(im.getbbox())
    if k != 1:
        im = im.resize((max(1, round(im.size[0] * k)), max(1, round(im.size[1] * k))), Image.LANCZOS)
    r, g, bl, al = im.split()
    return Image.merge('RGBA', (r, g, bl, al.point(lambda v: 255 if v >= 110 else 0)))


DZ = [('DZ_TL0', 'TrafficLight_Moss', .5), ('DZ_TL1', 'TrafficLight_WGrass', .5), ('DZ_TL2', 'TrafficLight_Clean', .5),
      ('DZ_CK0', 'Clock_Moss', .6), ('DZ_CK1', 'Clock_WGrass', .6), ('DZ_CK2', 'Clock_Clean', .6),
      ('DZ_ST0', 'StopSign_Flowers_Moss', .6), ('DZ_ST1', 'StopSign_WGrass_Flower', .6), ('DZ_ST2', 'StopSign_Clean', .6),
      ('DZ_MP0', 'Map_Moss', .6), ('DZ_MP1', 'Map_WGrass', .6), ('DZ_MP2', 'Map_Clean', .6),
      ('DZ_VD0', 'VendingMachine_Moss', .6), ('DZ_VD1', 'VendingMachine_WGrass', .6), ('DZ_VD2', 'VendingMachine_Clean', .6),
      ('DZ_BO0', 'Bollard_Moss', .75), ('DZ_BO1', 'Bollard_Clean', .75),
      ('DZ_FLB', 'Flower_Blue', .8), ('DZ_FLW', 'Flower_White', .65), ('DZ_FLA', 'Flowers_Assorted', .55),
      ('DZ_FXB', 'Foxtail_Blue', .6), ('DZ_FXP', 'Foxtail_Pink', .6), ('DZ_FX1', 'Foxtail_Yellow1', .55), ('DZ_FX2', 'Foxtail_Yellow2', .6),
      ('DZ_GRS', 'Grass', .6), ('DZ_GRT', 'Grass_WTrash', .6),
      ('DZ_CAN1', 'Trash_BlueCan', 1), ('DZ_CAN2', 'Trash_RedCan', 1), ('DZ_BOT', 'Trash_Bottle', 1)]

import make_arte   # [arte] suelos nuevos
newline()
for n, im in make_arte.SPRITES:   # [arte]
    add(n, im)
    origin[n] = 'codigo'

newline()
for n, f, k in DZ:   # [diseno]
    add(n, dz(f, k))
    origin[n] = 'codigo'


# [stalk] Lo mejor de StalkerLike que no se usaba: arboles, casas de pueblo, rejas, cofre, bandidos y objetos de patio.
# Todo pasa por apaga(): menos saturacion y menos brillo, para que case con la gama gris y fria del resto del atlas.
def apaga(im, sat=.62, k=.80, tinte=(1.0, 1.0, 1.0)):
    """Baja saturacion y brillo (y opcionalmente corre el tono) sin tocar el alfa."""
    px = im.load()
    out = im.copy()
    po = out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, bl, al = px[x, y]
            if not al:
                continue
            gr = .3 * r + .59 * g + .11 * bl
            r, g, bl = (gr + (v - gr) * sat for v in (r, g, bl))
            po[x, y] = (min(255, int(r * k * tinte[0])), min(255, int(g * k * tinte[1])), min(255, int(bl * k * tinte[2])), al)
    return out


def reduce(im, f):
    """Achica con suavizado y deja el borde con alfa duro (sin halos sobre el suelo)."""
    im = im.resize((max(1, round(im.size[0] * f)), max(1, round(im.size[1] * f))), Image.LANCZOS)
    r, g, bl, al = im.split()
    return Image.merge('RGBA', (r, g, bl, al.point(lambda v: 255 if v >= 100 else 0)))


def contorno(im, base=90, k=1.35, borde=(20, 17, 14)):
    """Mas contraste y un borde oscuro por dentro: al achicarlos, los arboles quedaban lavados contra el suelo."""
    out = im.copy()
    px, po = im.load(), out.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, bl, al = px[x, y]
            if not al:
                continue
            if any(not (0 <= x + dx < w and 0 <= y + dy < h) or not px[x + dx, y + dy][3]
                   for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                po[x, y] = borde + (255,)
            else:
                po[x, y] = tuple(max(0, min(255, int(base + (v - base) * k))) for v in (r, g, bl)) + (255,)
    return out


STALK = []
# arboles: el pino alto, el arbol ancho y el retorcido, en dos tamanos; ocre apagado y una version mas verde-olivo
_tr = c('Tree')
for k, box in enumerate(((40, 2, 91, 118), (152, 22, 238, 118), (284, 36, 365, 118))):
    t = _tr.crop(box)
    STALK.append((f'ST_TREE{k}', contorno(apaga(reduce(t, .5), .62, .74, (1.0, .95, .80)))))
    STALK.append((f'ST_TREE{k + 3}', contorno(apaga(reduce(t, .38), .55, .66, (.84, 1.0, .78)))))
# cofre: la caja larga recortada a 18 px (las dos puntas), cerrada y abierta (la tapa se ve hueca y oscura)
_ch = c('Chest').crop((0, 6, 48, 29))
_cofre = Image.new('RGBA', (18, 23), (0, 0, 0, 0))
_cofre.paste(_ch.crop((0, 0, 9, 23)), (0, 0))
_cofre.paste(_ch.crop((39, 0, 48, 23)), (9, 0))
_cofre = apaga(_cofre, .75, .92)
STALK.append(('ST_CHEST', _cofre))
_ab = _cofre.copy()
for y in range(2, 8):
    for x in range(2, 16):
        if _ab.getpixel((x, y))[3]:
            _ab.putpixel((x, y), (16, 13, 11, 255) if y > 2 else (52, 46, 40, 255))
STALK.append(('ST_CHEST_OPEN', _ab))
# rejas: madera (pueblos) y concreto (zonas industriales); tramo horizontal de 16 y poste
_wf, _cf = c('WoodFence'), c('ConcretFence')
STALK += [('ST_WFENCE_H', apaga(_wf.crop((64, 16, 80, 32)), .6, .78)), ('ST_WFENCE_V', apaga(_wf.crop((38, 7, 43, 32)), .6, .78)),
          ('ST_CFENCE_H', apaga(_cf.crop((20, 33, 36, 48)), .55, .82)), ('ST_CFENCE_V', apaga(_cf.crop((38, 7, 43, 32)), .55, .82))]
# muros de casa (16x28 como S_WALL): techo arriba (12 px) y fachada de tablas abajo (16 px); con y sin ventana
_hv, _hw = c('HouseVillage3'), c('HouseWalls')


def muro_casa(techo, fachada, sat, k):
    m = Image.new('RGBA', (16, 28), (0, 0, 0, 255))
    m.paste(techo, (0, 0))
    m.paste(fachada, (0, 12))
    for x in range(16):   # alero oscuro entre el techo y la fachada
        m.putpixel((x, 12), (14, 10, 9, 255))
        m.putpixel((x, 11), tuple(int(v * .7) for v in m.getpixel((x, 11))[:3]) + (255,))
    return apaga(m, sat, k)


_t_mina, _t_barrio = _hv.crop((40, 64, 56, 76)), _hv.crop((440, 40, 456, 52))
STALK += [('ST_HMINA0', muro_casa(_t_mina, _hw.crop((32, 32, 48, 48)), .62, .82)),
          ('ST_HMINA1', muro_casa(_t_mina, _hw.crop((80, 32, 96, 48)), .62, .82)),
          ('ST_HBARR0', muro_casa(_t_barrio, _hw.crop((32, 16, 48, 32)), .5, .72)),
          ('ST_HBARR1', muro_casa(_t_barrio, _hw.crop((80, 32, 96, 48)), .5, .72)),
          # techo solo: el muro que tiene otro muro debajo se ve como techo corrido (la fachada la tapa el de abajo)
          ('ST_HMINA_R', apaga(_hv.crop((40, 60, 56, 88)), .62, .82)),
          ('ST_HBARR_R', apaga(_hv.crop((440, 30, 456, 58)), .5, .72))]
# objetos de patio: la pila de cajones rojos (a la mitad) y el balde
STALK += [('ST_PALET', apaga(reduce(c('OutsideObjects1').crop((9, 14, 56, 59)), .5), .5, .78)),
          ('ST_BALDE', apaga(trim(c('OutsideObjects2')), .5, .8))]
# bandidos: tres hojas de 32x32 (4 quieto, 6 caminando, 1 tendido), mirando a la derecha; el juego espeja
for bk in (1, 2, 3):
    hb = c(f'Bandit{bk}')
    for f in range(4):
        STALK.append((f'ST_B{bk}_I{f}', apaga(hb.crop((f * 32 + 6, 6, f * 32 + 26, 24)), .7, .9)))
    for f in range(6):
        STALK.append((f'ST_B{bk}_W{f}', apaga(hb.crop((f * 32 + 6, 38, f * 32 + 26, 56)), .7, .9)))
    STALK.append((f'ST_B{bk}_D', apaga(hb.crop((6, 76, 30, 92)), .7, .8)))

newline()
for n, im in STALK:   # [stalk]
    add(n, im)
    origin[n] = 'C'


# ---------------------------------------------------------------- [armas2] ocho armas del Pixel Gun Pack
# Cada una: (nombre, numero de imagen del paquete, largo final en px, manos [(fraccion del largo, fraccion del alto)])
GUNS_DIR = os.path.join(ARTE, 'FreePixelGunPack', 'FreePixelGunPack', 'Guns')
ARMAS2 = [
    ('W_A2_VIBORA', 8, 14, [(0.22, 0.55)]), ('W_A2_CHACAL', 4, 15, [(0.22, 0.55)]),
    ('W_A2_ZUMBA', 28, 20, [(0.25, 0.60), (0.62, 0.55)]), ('W_A2_TORM', 11, 22, [(0.22, 0.55), (0.6, 0.55)]),
    ('W_A2_CUERVO', 23, 26, [(0.14, 0.55), (0.5, 0.55)]), ('W_A2_HALCON', 34, 24, [(0.22, 0.60), (0.62, 0.55)]),
    ('W_A2_CENTINELA', 48, 30, [(0.18, 0.55), (0.55, 0.5)]), ('W_A2_APLANA', 46, 28, [(0.2, 0.62), (0.55, 0.62)]),
]
newline()
for n, num, largo, manos in ARMAS2:
    g = Image.open(os.path.join(GUNS_DIR, f'{num}.png')).convert('RGBA')
    g = g.crop(g.getbbox())
    h = max(3, round(g.size[1] * largo / g.size[0]))
    g = g.resize((largo, h), Image.BOX)
    gp = g.load()
    for yy in range(g.size[1]):
        for xx in range(g.size[0]):
            r_, g_, b_, a_ = gp[xx, yy]
            gp[xx, yy] = (r_, g_, b_, 255 if a_ >= 110 else 0)
    g = apaga(g, .75, 1.0)
    g = lit(g, 1.5)
    spots = [(min(g.size[0] - 2, int(fx * g.size[0])), min(g.size[1] - 2, int(fy * g.size[1]))) for fx, fy in manos]
    g = hands(g, *spots)
    add(n, g)
    origin[n] = 'C'


# [mapnuc] Tierra maldita (paquete CursedLand): suelo, agua y obstaculos/adornos. Todo pasa por apaga() para la gama oscura del juego.
CURSED = os.path.join(ARTE, 'CursedLand', 'PNG')


def cl_img(rel):
    return Image.open(os.path.join(CURSED, *(rel + '.png').split('/'))).convert('RGBA')


def cl_tiles(sheet, n, flat):
    """n bloques opacos de 16x16 de una hoja, alineados a la cuadricula; flat: los mas lisos (suelo), si no los mas texturados."""
    im = sheet
    cand = []
    for ty in range(im.size[1] // 16):
        for tx in range(im.size[0] // 16):
            t = im.crop((tx * 16, ty * 16, tx * 16 + 16, ty * 16 + 16))
            px = list(t.getdata())
            if any(p[3] < 255 for p in px):
                continue
            m = [sum(p[k] for p in px) / 256 for k in range(3)]
            var = sum(sum((p[k] - m[k]) ** 2 for k in range(3)) for p in px) / 256
            cand.append((var, tx, ty, t))
    cand.sort(key=lambda c: c[0])
    pick = cand[len(cand) // 8:len(cand) // 8 + n] if flat else cand[-n * 3:-n * 2 or None][:n]
    return [c[3] for c in pick]


def cl_obj(nombre, ancho, sat=.72, k=.82):
    """Un objeto suelto, recortado al contenido y reducido a un ancho (el alto sigue la proporcion)."""
    im = Image.open(os.path.join(CURSED, 'Objects_separetely', nombre + '.png')).convert('RGBA')
    im = im.crop(im.getbbox())
    f = ancho / im.size[0]
    return apaga(reduce(im, f), sat, k)


MNLIST = []
_g = cl_tiles(cl_img('Ground'), 6, True)
for i in range(4):
    MNLIST.append((f'MN_GR{i}', apaga(_g[i % len(_g)], .7, .78)))
for i in range(2):
    MNLIST.append((f'MN_GR{4 + i}', apaga(_g[(i + 4) % len(_g)], .6, .62)))
_wt = cl_img('water_detilazation')
_base = (_wt.getpixel((680, 568))[:3])
for i in range(4):
    t = Image.new('RGBA', (16, 16), tuple(int(c * .5) for c in _base) + (255,))
    rip = _wt.crop((20 + i * 40, 8 + i * 4, 36 + i * 40, 24 + i * 4))
    t.alpha_composite(apaga(rip, .8, .8))
    MNLIST.append((f'MN_WA{i}', t))
for i, (nom, w) in enumerate([('Rock1_shadow1_2', 30), ('Rock3_shadow1_3', 28), ('Rock_eyes_shadow1_2', 28), ('Spike_plant_shadow1_3', 36),
                              ('Jaws_plant_shadow1_2', 34), ('Tentacle_plant_shadow1_2', 36), ('Meat_flower_shadow1_2', 26),
                              ('Many_eyes_plant_shadow1_1', 28), ('Tubular_plant_shadow1_1', 28), ('Eye_plant_shadow1_1', 28),
                              ('Ruins_shadow2_3', 30), ('Ruins_shadow2_1', 44), ('Fetus_shadow1_2', 26)]):
    MNLIST.append((f'MN_OB{i}', cl_obj(nom, w)))
for i, (nom, w) in enumerate([('Bones_shadow1_3', 20), ('Bones_shadow1_5', 22), ('Pustules_shadow1_1', 18), ('Bones_shadow1_7', 14), ('Veins_shadow1_4', 40)]):
    MNLIST.append((f'MN_DC{i}', cl_obj(nom, w)))
newline()
for n, im in MNLIST:   # [mapnuc]
    add(n, im)
    origin[n] = 'codigo'


# ---------------------------------------------------------------- salida
used = pos[1] + pos[2]
os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
with open(os.path.join(ROOT, 'assets', 'atlas.rgba'), 'wb') as f:   # siempre las dos paginas seguidas
    for pg in pages + [Image.new('RGBA', (AW, AH), (0, 0, 0, 0))] * (2 - len(pages)):
        f.write(pg.tobytes())
for k, pg in enumerate(pages):
    pg.save(os.path.join(ROOT, 'build', 'atlas.png' if k == 0 else f'atlas{k + 1}.png'))
bg = Image.new('RGBA', (AW, used), (60, 30, 70, 255))
bg.alpha_composite(atlas.crop((0, 0, AW, used)))
bg.resize((AW * 3, used * 3), Image.NEAREST).save(os.path.join(ROOT, 'build', 'atlas_x3.png'))

font_y = 'unsigned char' if max(g[1] for g in glyphs.values()) < 256 else 'unsigned short'
with open(os.path.join(ROOT, 'src', 'atlas.h'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('// GENERADO por tools/make_atlas.py: no editar a mano\n#ifndef ATLAS_H\n#define ATLAS_H\n\n')
    f.write(f'#define ATLAS_W {AW}\n#define ATLAS_H_PX {AH}\n#define FONT_H {code.FONT_H}\n\n')
    f.write('enum {\n' + ''.join(f'    S_{n},\n' for n, *_ in entries) + '    S_COUNT\n};\n\n')
    f.write('static const Spr SPR[S_COUNT] __attribute__((unused)) = {\n')
    for n, x, y, w, h, pg in entries:
        f.write(f'    {{{x}, {y}, {w}, {h}, {pg}}},   // {n}\n')
    f.write('};\n\n// letras: codigo 32..255 -> x, y, avance\n')
    for nm, idx, typ in (('FONT_X', 0, 'unsigned short'), ('FONT_Y', 1, font_y), ('FONT_ADV', 2, 'unsigned char')):
        vals = [glyphs.get(ch, (0, 0, 0))[idx] for ch in range(32, 256)]
        f.write(f'static const {typ} {nm}[224] __attribute__((unused)) = {{' + ', '.join(map(str, vals)) + '};\n')
    f.write('\n#endif\n')

count = {k: sum(1 for v in origin.values() if v == k) for k in ('A', 'B', 'C', 'codigo')}
print(f'paginas usadas: {len(pages)}')
print(f'atlas {AW}x{AH}: {len(entries)} sprites (A {count["A"]}, B {count["B"]}, C {count["C"]}, por codigo {count["codigo"]}), '
      f'{len(glyphs)} letras, alto usado {used}, letra {code.FONT_H} px')
