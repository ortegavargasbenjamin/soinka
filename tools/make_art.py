# PEPEL: dibuja todo el arte del juego en pixeles y lo empaqueta en un atlas de 512x256.
# Vista 3/4: el suelo se ve desde arriba; muros, objetos y personajes se ven de frente, con altura.
#   assets/atlas.rgba  -> textura cruda (va dentro del EBOOT)
#   src/atlas.h        -> donde quedo cada sprite y cada letra
#   build/atlas_x3.png -> el atlas ampliado, para mirarlo
# Uso: python tools/make_art.py
import math
import os
import random

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_TTF = os.path.join(ROOT, 'assets', 'fonts', 'DejaVuSans-Bold-Latin.ttf')
AW, AH = 512, 256
atlas = Image.new('RGBA', (AW, AH), (0, 0, 0, 0))
entries = []


class Packer:
    def __init__(self):
        self.x = self.y = self.rowh = 0

    def add(self, name, im, pad=1):
        w, h = im.size
        if self.x + w > AW:
            self.newline(pad)
        if self.y + h > AH:
            raise SystemExit(f'el atlas se lleno en {name}')
        atlas.paste(im, (self.x, self.y))
        if name:
            entries.append((name, self.x, self.y, w, h))
        pos = (self.x, self.y)
        self.x += w + pad
        self.rowh = max(self.rowh, h)
        return pos

    def newline(self, pad=1):
        self.x = 0
        self.y += self.rowh + pad
        self.rowh = 0


P = Packer()


def new(w=16, h=16, fill=(0, 0, 0, 0)):
    return Image.new('RGBA', (w, h), fill)


def c(rgb, a=255):
    return (rgb[0], rgb[1], rgb[2], a)


def shade(rgb, k):
    return tuple(max(0, min(255, int(v * k))) for v in rgb[:3])


def outline(im, col=(18, 15, 14, 255)):
    """Borde oscuro de 1 px alrededor de lo dibujado (hacia afuera)."""
    w, h = im.size
    src = im.load()
    out = im.copy()
    o = out.load()
    for y in range(h):
        for x in range(w):
            if src[x, y][3] != 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and src[nx, ny][3] > 200:
                    o[x, y] = col
                    break
    return out


def edged(im):
    """Le agrega 1 px de margen y el borde oscuro."""
    big = new(im.size[0] + 2, im.size[1] + 2)
    big.paste(im, (1, 1))
    return outline(big)


def flat(base, seed, dark=None, light=None, nd=7, nl=4):
    """Suelo: color plano con unos pocos grupos de pixeles mas oscuros y mas claros."""
    rnd = random.Random(seed)
    im = new(fill=c(base))
    px = im.load()
    dark = dark or shade(base, 0.84)
    light = light or shade(base, 1.18)
    for _ in range(nd):
        x, y = rnd.randrange(16), rnd.randrange(16)
        px[x, y] = c(dark)
        if rnd.random() < 0.6:
            px[(x + 1) % 16, y] = c(dark)
        if rnd.random() < 0.3:
            px[x, (y + 1) % 16] = c(dark)
    for _ in range(nl):
        x, y = rnd.randrange(16), rnd.randrange(16)
        px[x, y] = c(light)
        if rnd.random() < 0.4:
            px[(x + 1) % 16, y] = c(shade(light, 0.9))
    return im


def px(d, pts, col):
    for p in pts:
        d.point(p, fill=c(col))


# ================================================================ pinceles
from PIL import ImageChops

RUST, RUST_D = (108, 64, 40), (62, 38, 30)
ASH, STONE, STONE_S = (112, 110, 106), (122, 118, 110), (38, 35, 34)


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def mul(p, x, y, k):
    p[x, y] = c(shade(p[x, y], k), p[x, y][3])


def tint(p, x, y, col, t):
    p[x, y] = c(mix(p[x, y], col, t), p[x, y][3])


def noise(rnd, w=16, h=16, passes=2):
    """Ruido suavizado que envuelve por los bordes, llevado a 0..1."""
    g = [[rnd.random() for _ in range(w)] for _ in range(h)]
    for _ in range(passes):
        g = [[(2 * g[y][x] + g[y][x - 1] + g[y][(x + 1) % w] + g[y - 1][x] + g[(y + 1) % h][x]) / 6
              for x in range(w)] for y in range(h)]
    lo, hi = min(map(min, g)), max(map(max, g))
    return [[(v - lo) / (hi - lo) for v in row] for row in g]


def tex(tones, seed, cuts=(0.3, 0.56, 0.8), w=16, h=16):
    """Manchones de pocos tonos (del mas oscuro al mas claro) que calzan al repetirse."""
    rnd = random.Random(seed)
    g = noise(rnd, w, h)
    im = new(w, h)
    p = im.load()
    for y in range(h):
        for x in range(w):
            p[x, y] = c(tones[sum(g[y][x] > t for t in cuts)])
    return im, rnd


def dots(im, rnd, n, col, shadow=None, big=0.0):
    """Motas sueltas; con sombra abajo a la derecha son piedritas."""
    p = im.load()
    w, h = im.size
    for _ in range(n):
        x, y = rnd.randrange(w), rnd.randrange(h)
        p[x, y] = c(col)
        if rnd.random() < big:
            p[(x + 1) % w, y] = c(shade(col, 0.84))
        if shadow:
            p[(x + 1) % w, (y + 1) % h] = c(shadow)


def crack(im, rnd, x, y, n, col, dx=1, dy=1, wrap=True):
    """Grieta: baja en diagonal, a tropezones."""
    p = im.load()
    w, h = im.size
    for _ in range(n):
        if wrap:
            p[x % w, y % h] = c(col)
        elif 0 <= x < w and 0 <= y < h:
            p[x, y] = c(col)
        r = rnd.random()
        if r < 0.4:
            x += dx
        elif r < 0.7:
            y += dy
        else:
            x += dx
            y += dy


def blob(im, cx, cy, rx, ry, col, t=1.0):
    """Mancha ovalada que tine lo que hay debajo."""
    p = im.load()
    w, h = im.size
    for dy in range(-int(ry) - 1, int(ry) + 2):
        for dx in range(-int(rx) - 1, int(rx) + 2):
            if (dx / rx) ** 2 + (dy / ry) ** 2 <= 1.0:
                tint(p, (cx + dx) % w, (cy + dy) % h, col, t)


# ================================================================ suelos (se ven desde arriba)
DIRT_T = [(50, 47, 45), (63, 59, 54), (70, 66, 60), (82, 78, 71)]
for i in range(3):
    im, rnd = tex(DIRT_T, 10 + i)
    d = ImageDraw.Draw(im)
    if i == 0:     # charco de barro
        blob(im, 10, 5, 3.3, 2.3, (40, 35, 33), 0.7)
        blob(im, 10, 5, 2.0, 1.2, (33, 30, 30), 0.8)
        px(d, ((9, 4), (10, 4)), (90, 88, 88))
    elif i == 1:   # tierra agrietada
        crack(im, rnd, 2, 3, 10, (40, 37, 36))
        crack(im, rnd, 8, 8, 5, (44, 41, 39), dx=-1)
    else:          # la huella de una bota
        HU = (47, 44, 42)
        for (x0, x1, y) in ((6, 7, 3), (5, 7, 4), (5, 7, 5), (5, 7, 6), (5, 6, 7), (5, 6, 9), (5, 6, 10)):
            d.line((x0, y, x1, y), fill=c(HU))
        px(d, ((6, 5), (6, 9)), DIRT_T[1])
        px(d, ((8, 5), (8, 6), (7, 7), (7, 10)), DIRT_T[3])
    dots(im, rnd, 3, STONE, STONE_S, big=0.4)
    dots(im, rnd, 5, ASH)
    dots(im, rnd, 4, DIRT_T[0])
    P.add(f'DIRT{i}', im)

GRASS_T = [(46, 49, 39), (58, 61, 47), (65, 67, 52), (75, 75, 57)]
for i in range(3):
    im, rnd = tex(GRASS_T, 20 + i)
    d = ImageDraw.Draw(im)
    if i == 1:     # peladero
        blob(im, 4, 11, 2.7, 1.8, DIRT_T[1], 0.9)
        blob(im, 4, 11, 1.3, 0.9, DIRT_T[2], 0.9)
    if i == 2:
        dots(im, rnd, 2, STONE, STONE_S, big=0.5)
    for _ in range(10):   # matas secas
        x, y = rnd.randrange(1, 15), rnd.randrange(3, 16)
        col = rnd.choice([(100, 98, 64), (86, 82, 56), (112, 106, 74), (48, 52, 38)])
        hh = rnd.choice([1, 2, 2, 3])
        d.point((x, y), fill=c((38, 42, 33)))
        d.line((x, y - 1, x, y - hh), fill=c(col))
        if rnd.random() < 0.6:
            d.point((x + rnd.choice([-1, 1]), y - 1), fill=c(shade(col, 0.8)))
    dots(im, rnd, 3, ASH)
    P.add(f'GRASS{i}', im)

ROAD_T = [(40, 40, 45), (50, 50, 55), (54, 54, 59), (63, 63, 68)]
GRIT, PIT = (78, 78, 84), (30, 30, 34)
im, rnd = tex(ROAD_T, 40)
blob(im, 11, 10, 3.2, 2.3, (36, 34, 38), 0.55)   # aceite viejo
crack(im, rnd, 1, 2, 6, PIT)
crack(im, rnd, 9, 14, 4, PIT, dy=-1)
dots(im, rnd, 8, GRIT)
dots(im, rnd, 5, PIT)
P.add('ROAD0', im)
im, rnd = tex(ROAD_T, 41)
d = ImageDraw.Draw(im)
crack(im, rnd, 2, 0, 21, PIT)
crack(im, rnd, 6, 9, 7, (36, 36, 40), dx=-1)
blob(im, 11, 4, 3.2, 2.3, (30, 29, 31))          # bache
blob(im, 11, 5, 1.6, 1.0, (46, 42, 39))
px(d, ((10, 7), (11, 7), (12, 7), (13, 6), (14, 5)), GRIT)
d.line((4, 13, 4, 11), fill=c((86, 82, 56)))
px(d, ((5, 12),), (64, 66, 52))
dots(im, rnd, 7, GRIT)
dots(im, rnd, 4, PIT)
P.add('ROAD1', im)

FLOOR_T = [(74, 72, 71), (87, 85, 82), (92, 90, 86), (101, 99, 95)]
for i in range(2):
    im, rnd = tex(FLOOR_T, 50 + i, cuts=(0.24, 0.6, 0.86))
    d = ImageDraw.Draw(im)
    if i == 0:
        blob(im, 10, 10, 3.2, 2.3, (70, 60, 52), 0.4)
        crack(im, rnd, 3, 5, 4, (60, 58, 58))
    else:
        blob(im, 5, 6, 3.2, 2.3, (62, 56, 50), 0.5)
        blob(im, 5, 6, 2.0, 1.2, (84, 80, 74), 0.6)
        crack(im, rnd, 15, 3, 14, (48, 46, 47), dx=-1, wrap=False)
        crack(im, rnd, 9, 9, 4, (56, 54, 54), wrap=False)
        d.polygon(((15, 11), (15, 15), (10, 15)), fill=c((52, 50, 50)))   # esquina rota
        px(d, ((13, 14), (14, 13)), (72, 68, 64))
        px(d, ((12, 12), (9, 14)), (114, 112, 106))
    d.line((0, 0, 15, 0), fill=c((58, 56, 56)))
    d.line((0, 0, 0, 15), fill=c((58, 56, 56)))
    for k in range(1, 16):
        if rnd.random() < 0.75:
            d.point((k, 1), fill=c((108, 106, 102)))
        if rnd.random() < 0.5:
            d.point((1, k), fill=c((104, 102, 98)))
    dots(im, rnd, 3, (114, 112, 108))
    dots(im, rnd, 3 + 3 * i, FLOOR_T[0])
    P.add(f'FLOOR{i}', im)

im, rnd = tex([(62, 60, 58), (74, 72, 69), (80, 78, 74), (88, 86, 82)], 55)   # umbral de puerta, franjas gastadas
d = ImageDraw.Draw(im)
p = im.load()
for x in range(-16, 16, 6):
    for k in (0, 1):
        d.line((x + k, 2, x + k + 2, 0), fill=c((158, 132, 54)))
        d.line((x + k, 15, x + k + 2, 13), fill=c((158, 132, 54)))
for y in (0, 1, 2, 13, 14, 15):
    for x in range(16):
        if rnd.random() < 0.3:
            tint(p, x, y, (66, 64, 60), 0.65)
d.line((3, 7, 9, 7), fill=c((64, 62, 60)))
d.line((6, 9, 12, 9), fill=c((92, 90, 86)))
dots(im, rnd, 3, (56, 54, 52))
P.add('DOOR', im)

base, rnd = tex([(30, 58, 38), (42, 78, 44), (50, 90, 48), (74, 124, 56)], 60, cuts=(0.3, 0.55, 0.76))
for i in range(2):   # charco radiactivo: el fondo se corre y las burbujas se turnan
    im = ImageChops.offset(base, 2 * i, i)
    d = ImageDraw.Draw(im)
    for n, (x, y) in enumerate(((3, 4), (11, 3), (7, 11), (13, 12))):
        if n % 2 == i:
            px(d, ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)), (150, 198, 84))
            px(d, ((x, y),), (36, 66, 40))
            px(d, ((x - 1, y - 1),), (210, 238, 136))
        else:
            px(d, ((x, y),), (172, 212, 98))
            px(d, ((x + 1, y + 1),), (30, 58, 38))
    P.add(f'RAD{i}', im)

im, rnd = tex([(50, 47, 45), (64, 60, 57), (72, 68, 64), (90, 88, 84)], 70, cuts=(0.3, 0.52, 0.74))   # via del tren
dots(im, rnd, 5, (106, 104, 100), (40, 38, 37))
d = ImageDraw.Draw(im)
for x in (1, 9):
    d.rectangle((x, 2, x + 2, 13), fill=c((60, 44, 34)))
    d.line((x, 2, x, 13), fill=c((84, 64, 47)))
    d.line((x + 2, 2, x + 2, 13), fill=c((44, 32, 26)))
    d.line((x + 3, 3, x + 3, 14), fill=c((44, 41, 40)))
    px(d, ((x + 1, 6), (x + 1, 7), (x, 9), (x + 1, 2), (x + 1, 13)), (40, 29, 24))
for y in (4, 11):
    d.line((0, y, 15, y), fill=c((138, 138, 144)))
    d.line((0, y + 1, 15, y + 1), fill=c((70, 66, 70)))
    for x in range(16):
        if rnd.random() < 0.3:
            d.point((x, y), fill=c((112, 78, 58)))
        if rnd.random() < 0.4:
            d.point((x, y + 1), fill=c((94, 58, 42)))
    for x in (1, 9):
        px(d, ((x + 1, y - 1), (x + 1, y + 2)), (34, 32, 32))
P.add('RAIL', im)

# ---------------------------------------------------------------- restos tirados en el suelo
BONE, BONE_D, INK = (196, 188, 166), (128, 120, 102), (28, 24, 22)
OLD = (60, 20, 18)

im = new()   # esqueleto tendido
d = ImageDraw.Draw(im)
d.line((5, 8, 11, 8), fill=c(BONE))
for x in (6, 8, 10):
    d.line((x, 6, x, 10), fill=c(BONE_D))
d.line((11, 8, 14, 6), fill=c(BONE))
d.line((11, 8, 14, 11), fill=c(BONE))
d.line((6, 6, 4, 3), fill=c(BONE_D))
d.line((6, 10, 5, 13), fill=c(BONE_D))
d.ellipse((1, 6, 4, 9), fill=c(BONE))
px(d, ((2, 7), (2, 9)), INK)
P.add('SKEL', im)

im = new()   # huesos sueltos
d = ImageDraw.Draw(im)
d.line((3, 4, 7, 6), fill=c(BONE))
px(d, ((2, 3), (8, 7)), BONE)
d.line((9, 11, 12, 10), fill=c(BONE_D))
d.ellipse((10, 3, 12, 5), fill=c(BONE))
d.point((11, 4), fill=c(INK))
d.line((4, 11, 5, 13), fill=c(BONE_D))
P.add('BONES', im)

for i in range(2):   # sangre vieja
    rnd = random.Random(120 + i)
    im = new()
    d = ImageDraw.Draw(im)
    for _ in range(4):
        x, y, r = rnd.randrange(4, 12), rnd.randrange(4, 12), rnd.choice([1, 2, 2, 3])
        d.ellipse((x - r, y - r, x + r, y + r), fill=c(OLD, 165))
    if i == 1:
        d.line((1, 13, 14, 3), fill=c(OLD, 150), width=2)
        d.line((2, 14, 12, 7), fill=c(OLD, 110))
    P.add(f'STAIN{i}', im)

im = new()   # basura
d = ImageDraw.Draw(im)
d.polygon(((2, 3), (6, 2), (7, 5), (3, 6)), fill=c((146, 142, 126)))
d.line((3, 4, 5, 4), fill=c((96, 94, 84)))
d.polygon(((9, 9), (13, 10), (12, 13), (8, 12)), fill=c((120, 118, 106)))
d.rectangle((10, 3, 11, 5), fill=c((104, 106, 112)))
d.point((10, 3), fill=c((150, 152, 158)))
P.add('TRASH0', im)
im = new()
d = ImageDraw.Draw(im)
d.polygon(((3, 9), (7, 8), (9, 11), (6, 13), (2, 12)), fill=c((74, 60, 54)))
d.line((4, 10, 7, 11), fill=c((54, 44, 40)))
d.rectangle((10, 4, 12, 5), fill=c((134, 130, 116)))
px(d, ((6, 3),), (104, 106, 112))
px(d, ((7, 3),), (84, 86, 92))
P.add('TRASH1', im)

rnd = random.Random(140)   # ceniza amontonada
im = new()
d = ImageDraw.Draw(im)
for _ in range(6):
    x, y, r = rnd.randrange(3, 13), rnd.randrange(4, 12), rnd.choice([1, 2, 2])
    d.ellipse((x - r - 1, y - r, x + r + 1, y + r), fill=c((126, 124, 120), 120))
for _ in range(5):
    d.point((rnd.randrange(2, 14), rnd.randrange(3, 13)), fill=c((158, 156, 150), 170))
P.add('ASH0', im)

im = new()   # el cuerpo de otro chatarrero, sobre su charco
d = ImageDraw.Draw(im)
d.ellipse((1, 3, 14, 13), fill=c((72, 18, 18), 190))
d.line((10, 7, 14, 5), fill=c((48, 50, 46)), width=2)
d.line((10, 9, 14, 12), fill=c((48, 50, 46)), width=2)
d.line((6, 5, 4, 2), fill=c((64, 74, 82)))
d.line((8, 10, 9, 14), fill=c((64, 74, 82)))
d.rectangle((5, 6, 10, 10), fill=c((64, 74, 82)))
d.line((5, 6, 10, 6), fill=c((84, 96, 104)))
d.ellipse((2, 6, 5, 9), fill=c((56, 52, 46)))
d.point((3, 7), fill=c((150, 164, 150)))
d.rectangle((6, 11, 8, 13), fill=c((90, 64, 44)))
P.add('BODY', im)

for i in range(3):   # sangre fresca
    rnd = random.Random(80 + i)
    im = new(12, 12)
    d = ImageDraw.Draw(im)
    for _ in range(5):
        x, y, r = rnd.randrange(3, 9), rnd.randrange(3, 9), rnd.choice([1, 2, 2, 3])
        d.ellipse((x - r, y - r, x + r, y + r), fill=c((104, 20, 22), 215))
    for _ in range(6):
        d.point((rnd.randrange(12), rnd.randrange(12)), fill=c((126, 26, 26), 200))
    P.add(f'BLOOD{i}', im)

im = new(12, 5)   # sombra a los pies
d = ImageDraw.Draw(im)
d.ellipse((0, 0, 11, 4), fill=(0, 0, 0, 96))
P.add('SHADOW', im)


# ================================================================ lo que tiene altura (se apoya en el borde de abajo de la casilla)
# La luz viene de arriba a la izquierda: lo de arriba claro, el frente oscuro, la base mas oscura todavia.
def block3(top, face, name, h, ribs=False):
    """Bloque: la cara de arriba (16x16) y debajo la cara del frente, de alto h, bastante mas oscura."""
    im = new(16, 16 + h)
    t, rnd = tex([shade(top, 0.84), shade(top, 0.95), top, shade(top, 1.07)], sum(top) + h)
    im.paste(t, (0, 0))
    p = im.load()
    d = ImageDraw.Draw(im)
    g = noise(rnd, 16, h)
    for y in range(h):
        for x in range(16):
            p[x, 16 + y] = c(shade(face, (1.06 - 0.3 * y / h) * (0.92 + 0.16 * g[y][x])))
    B = 15 + h
    if ribs:   # contenedor: chapa acanalada, largueros con remaches, oxido que chorrea
        for (cx, cy) in ((4, 4), (12, 10), (9, 3)):
            blob(im, cx, cy, 2.3, 1.6, RUST_D, 0.55)
            blob(im, cx, cy, 1.1, 0.8, RUST, 0.5)
        for y in range(1, B - 3):
            if 14 <= y <= 17:
                continue
            for x in range(16):
                if x % 4 == 1:
                    mul(p, x, y, 0.72 if y < 16 else 0.62)
                elif x % 4 == 2:
                    mul(p, x, y, 1.13 if y < 16 else 1.2)
        for _ in range(7):
            tint(p, rnd.randrange(16), rnd.randrange(1, 14), (156, 152, 146), 0.4)
        d.line((0, 0, 15, 0), fill=c(shade(top, 1.14)))
        d.line((0, 14, 15, 14), fill=c(shade(top, 0.8)))
        d.line((0, 15, 15, 15), fill=c(shade(top, 1.24)))
        d.line((0, 16, 15, 16), fill=c(shade(face, 1.12)))
        d.line((0, 17, 15, 17), fill=c(shade(face, 0.6)))
        d.line((0, B - 3, 15, B - 3), fill=c(shade(face, 0.9)))
        d.rectangle((0, B - 2, 15, B - 1), fill=c(shade(face, 0.72)))
        for x in range(3, 16, 4):
            p[x, 16] = c(shade(face, 1.55))
            p[x, B - 2] = c(shade(face, 1.15))
        for x0 in rnd.sample(range(16), 6):
            n = rnd.randrange(3, h - 4)
            for j in range(n):
                tint(p, x0, 17 + j, RUST_D if j < 2 else RUST, 0.75 * (1 - j / n) + 0.1)
        for y in range(B - 3, B):
            for x in range(16):
                if rnd.random() < 0.45:
                    tint(p, x, y, rnd.choice([RUST, RUST_D]), 0.6)
        for (x, y) in ((5, 20), (6, 21), (6, 22)):   # abolladura
            mul(p, x, y, 0.6)
            mul(p, x + 1, y, 1.3)
        for (x, y) in ((12, 20), (13, 20), (12, 21), (2, 23), (3, 23)):   # pintura saltada
            mul(p, x, y, 0.55)
    else:      # muro de bloques: juntas, grietas, humedad
        J = shade(top, 0.66)
        blob(im, 4, 12, 2.6, 1.7, (72, 82, 70), 0.45)
        blob(im, 12, 3, 1.8, 1.3, (72, 82, 70), 0.35)
        d.line((0, 7, 15, 7), fill=c(J))
        for x in range(16):
            if rnd.random() < 0.6:
                mul(p, x, 8, 1.12)
            if rnd.random() < 0.6:
                mul(p, x, 0, 1.12)
        for (x, y0, y1) in ((7, 0, 6), (3, 8, 14), (11, 8, 14)):
            d.line((x, y0, x, y1), fill=c(J))
            for y in range(y0, y1 + 1):
                if rnd.random() < 0.5:
                    mul(p, x + 1, y, 1.1)
        crack(im, rnd, 10, 1, 6, shade(top, 0.58), wrap=False)
        crack(im, rnd, 15, 10, 4, shade(top, 0.62), dx=-1, wrap=False)
        for _ in range(5):
            mul(p, rnd.randrange(16), rnd.randrange(1, 14), 0.76)
        for _ in range(4):
            tint(p, rnd.randrange(16), rnd.randrange(1, 14), (152, 150, 144), 0.45)
        d.line((0, 15, 15, 15), fill=c(shade(top, 1.2)))
        px(d, ((5, 15), (12, 15)), shade(top, 0.92))
        for r in range((h + 3) // 4):   # hiladas del frente
            y0 = 16 + r * 4
            for x in range(16):
                if y0 + 3 < B:
                    mul(p, x, y0 + 3, 0.6)
                if rnd.random() < 0.5:
                    mul(p, x, y0, 1.14)
            for x in ((5, 13) if r % 2 == 0 else (1, 9)):
                for y in range(y0, min(y0 + 3, B)):
                    mul(p, x, y, 0.6)
        for (x0, n) in ((3, 7), (10, 10), (11, 6), (14, 4)):   # humedad que baja
            for j in range(n):
                tint(p, x0, 16 + j, (26, 38, 36), 0.5 - 0.35 * j / n)
        crack(im, rnd, 6, 17, 10, shade(face, 0.4), wrap=False)
        for (x, y) in ((2, 21), (14, 17), (8, 25)):
            mul(p, x, y, 1.3)
        for x in range(16):
            mul(p, x, 16, 0.86)
    for x in range(16):
        mul(p, x, B - 1, 0.8)
        p[x, B] = c(shade(face, 0.4))
    P.add(name, im)


block3((106, 104, 100), (62, 60, 62), 'WALL', 12)
block3((114, 58, 40), (76, 38, 28), 'CONT', 14, ribs=True)
block3((52, 78, 84), (32, 50, 54), 'CONT2', 14, ribs=True)

IRON = (74, 72, 76)


def crate3(top, front, edge, mark=None, opened=False):
    rnd = random.Random(7)
    im = new(14, 17)
    d = ImageDraw.Draw(im)
    p = im.load()
    d.rectangle((0, 0, 13, 8), fill=c(top), outline=c(edge))
    d.rectangle((0, 9, 13, 16), fill=c(front), outline=c(edge))
    for y in range(1, 8):
        for x in range(1, 13):
            mul(p, x, y, 0.94 + 0.12 * rnd.random())
    for y in range(10, 16):
        for x in range(1, 13):
            mul(p, x, y, (1.08 - 0.07 * (y - 10)) * (0.95 + 0.1 * rnd.random()))
    d.line((1, 1, 12, 1), fill=c(shade(top, 1.2)))
    if opened:   # saqueado: adentro oscuro y un poco de paja
        d.rectangle((2, 2, 11, 7), fill=c((34, 27, 22)))
        d.line((2, 2, 11, 2), fill=c((14, 11, 10)))
        d.line((2, 3, 2, 7), fill=c((20, 16, 14)))
        d.line((3, 7, 11, 7), fill=c((54, 43, 32)))
        px(d, ((5, 6), (8, 5), (9, 6)), (116, 100, 62))
        px(d, ((6, 6),), (84, 72, 46))
    else:        # tapa de tablas clavadas
        d.line((1, 2, 1, 7), fill=c(shade(top, 1.1)))
        d.line((5, 2, 5, 7), fill=c(shade(top, 0.7)))
        d.line((9, 2, 9, 7), fill=c(shade(top, 0.7)))
        px(d, ((3, 2), (7, 2), (11, 2), (3, 7), (7, 7), (11, 7)), shade(top, 0.52))
        px(d, ((2, 4), (3, 5), (10, 4), (7, 5)), shade(top, 0.82))
        px(d, ((6, 4), (11, 5), (12, 3)), (150, 146, 138))
    d.line((1, 12, 12, 12), fill=c(shade(front, 0.62)))
    px(d, ((5, 10), (5, 11), (8, 13), (8, 14), (8, 15)), shade(front, 0.68))
    px(d, ((9, 10), (10, 11), (3, 14)), shade(front, 0.8))
    px(d, ((1, 10), (2, 10), (1, 11), (12, 10), (11, 10), (12, 11), (1, 15), (2, 15), (1, 14), (12, 15), (11, 15), (12, 14)), IRON)
    px(d, ((2, 10), (11, 15)), RUST)
    px(d, ((1, 10),), (112, 110, 114))
    if mark == 'star':   # cajon militar: franja ocre gastada y estrella roja apagada
        d.rectangle((3, 12, 10, 13), fill=c((150, 128, 58)))
        px(d, ((4, 13), (9, 12)), shade(front, 0.9))
        d.rectangle((5, 11, 8, 14), fill=c(shade(front, 0.9)))
        px(d, ((6, 11), (7, 11), (5, 12), (6, 12), (7, 12), (8, 12), (6, 13), (7, 13), (5, 14), (8, 14)), (156, 46, 42))
    for x in range(1, 13):
        mul(p, x, 15, 0.74)
    return im


P.add('CRATE', crate3((122, 92, 58), (88, 64, 40), (44, 32, 20)))
P.add('CRATE_OPEN', crate3((122, 92, 58), (88, 64, 40), (44, 32, 20), opened=True))
P.add('CRATE_JOB', crate3((78, 96, 66), (54, 70, 48), (28, 38, 24), mark='star'))
P.add('CRATE_JOB_OPEN', crate3((78, 96, 66), (54, 70, 48), (28, 38, 24), mark='star', opened=True))


def barrel3(body, top, fire=False):
    rnd = random.Random(sum(body))
    im = new(10, 15)
    d = ImageDraw.Draw(im)
    d.ellipse((0, 9, 9, 14), fill=c(body))
    d.rectangle((0, 3, 9, 12), fill=c(body))
    p = im.load()
    K = (0.56, 0.82, 1.3, 1.12, 1.0, 0.96, 0.9, 0.78, 0.64, 0.5)   # cilindro: brillo a la izquierda
    for y in range(3, 15):
        for x in range(10):
            if not p[x, y][3]:
                continue
            hoop = y in ((6, 10) if x in (0, 9) else (7, 11))
            p[x, y] = c(shade(body, K[x] * (1.04 - 0.035 * (y - 3)) * (0.6 if hoop else 1.0) * (0.8 if y > 12 else 1.0)))
            r = rnd.random()
            if not fire and not hoop and r < 0.1 + 0.02 * (y - 3):
                tint(p, x, y, RUST_D if r < 0.08 else RUST, 0.65)
    mul(p, 6, 8, 0.66)   # abolladura
    mul(p, 6, 9, 1.25)
    d.ellipse((0, 0, 9, 5), fill=c(top), outline=c(shade(body, 0.45)))
    d.ellipse((2, 1, 7, 4), fill=c(shade(top, 0.84)))
    d.arc((0, 0, 9, 5), 170, 300, fill=c(shade(top, 1.3)))
    if fire:
        for y in range(3, 7):
            for x in range(10):
                if p[x, y][3] and y > 5:
                    tint(p, x, y, (124, 54, 30), 0.3)
        d.ellipse((2, 1, 7, 4), fill=c((220, 104, 36)))
        d.line((3, 2, 6, 2), fill=c((255, 214, 110)))
        px(d, ((4, 3),), (255, 238, 170))
        for (x, y) in ((3, 8), (6, 9), (4, 11)):   # agujeros por donde se ve la brasa
            for (ax, ay) in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                tint(p, x + ax, y + ay, (170, 70, 30), 0.4)
            p[x, y] = c((240, 136, 46))
    else:
        px(d, ((6, 2),), shade(body, 0.4))
        px(d, ((3, 3), (4, 3)), RUST_D)
        px(d, ((4, 8), (5, 8), (4, 9), (5, 9)), (146, 136, 116))   # etiqueta vieja
        px(d, ((5, 9),), (104, 96, 84))
        for (x, y) in ((3, 8), (3, 9), (7, 12), (7, 13)):          # chorreo
            tint(p, x, y, RUST_D, 0.6)
    return edged(im)


P.add('BARREL', barrel3((116, 58, 44), (138, 72, 54)))
P.add('FBARREL', barrel3((54, 48, 46), (36, 28, 24), fire=True))

im = new(16, 11)   # sacos de arena apilados; los de abajo, en sombra
d = ImageDraw.Draw(im)
SB, SB_H, SB_L = (132, 118, 90), (166, 152, 118), (98, 86, 64)
for n, (x0, y0, x1, y1) in enumerate(((0, 0, 7, 5), (8, 0, 15, 5), (0, 5, 4, 10), (5, 5, 11, 10), (12, 5, 15, 10))):
    k = 1.0 if n < 2 else 0.82
    d.rounded_rectangle((x0, y0, x1, y1), radius=2, fill=c(shade(SB, k)), outline=c((50, 44, 34)))
    d.line((x0 + 2, y0 + 1, x1 - 2, y0 + 1), fill=c(shade(SB_H, k)))
    d.point((x0 + 1, y0 + 2), fill=c(shade(SB_H, k)))
    d.line((x0 + 2, y1 - 1, x1 - 1, y1 - 1), fill=c(shade(SB_L, k)))
    d.line((x1 - 1, y0 + 2, x1 - 1, y1 - 2), fill=c(shade(SB_L, k)))
    for x in range(x0 + 2, x1 - 1, 2):
        d.point((x, y0 + 3), fill=c(shade(SB_L, k * 0.92)))   # la costura
px(d, ((3, 2), (12, 3), (9, 8)), (104, 94, 74))                # mugre
px(d, ((6, 8), (7, 9)), (56, 48, 36))                          # un saco roto
px(d, ((7, 10), (8, 10), (8, 9)), (172, 158, 122))
P.add('SANDBAG', im)

im = new(16, 14)   # reja de frente: poste, malla rota y oxidada, alambre arriba
d = ImageDraw.Draw(im)
for x in range(-12, 16, 4):
    d.line((x, 12, x + 10, 2), fill=c((86, 88, 90), 175))
    d.line((x, 2, x + 10, 12), fill=c((58, 60, 62), 175))
p = im.load()
for y in range(6, 11):
    for x in range(8 + (y % 2), 13 - (y // 9)):
        p[x, y] = (0, 0, 0, 0)
px(d, ((8, 6), (13, 9), (9, 10)), (120, 122, 124))
rnd = random.Random(4)
for _ in range(9):
    x, y = rnd.randrange(2, 16), rnd.randrange(3, 12)
    if p[x, y][3]:
        p[x, y] = c(RUST, 200)
d.line((0, 2, 15, 2), fill=c((118, 120, 122)))
d.line((0, 3, 15, 3), fill=c((44, 44, 46), 200))
d.line((0, 12, 15, 12), fill=c((80, 80, 82)))
px(d, ((4, 12), (5, 12), (11, 12)), RUST_D)
px(d, ((6, 2), (13, 2)), RUST)
for x in range(2, 16):
    d.point((x, 1 if x % 4 < 2 else 0), fill=c((98, 98, 102)))
d.rectangle((0, 0, 1, 13), fill=c((40, 40, 42)))
d.line((0, 1, 0, 13), fill=c((100, 100, 104)))
px(d, ((0, 0), (1, 0)), (124, 124, 128))
px(d, ((0, 6), (0, 7), (1, 9)), RUST)
px(d, ((0, 13), (1, 13)), (26, 26, 28))
P.add('FENCE_H', im)

im = new(4, 24)   # reja que se aleja: un poste y los alambres
d = ImageDraw.Draw(im)
d.line((1, 0, 1, 10), fill=c((104, 106, 108)))
d.line((2, 0, 2, 10), fill=c((58, 60, 62)))
px(d, ((1, 3), (2, 6), (1, 8)), RUST)
d.rectangle((1, 10, 2, 23), fill=c((40, 40, 42)))
d.line((1, 11, 1, 23), fill=c((100, 100, 104)))
d.line((0, 10, 3, 10), fill=c((124, 124, 128)))
px(d, ((1, 14), (1, 15), (2, 18)), RUST)
px(d, ((1, 23), (2, 23)), (26, 26, 28))
px(d, ((0, 23), (3, 23)), (40, 36, 34))
P.add('FENCE_V', im)

EARTH_H, EARTH, EARTH_L = (88, 80, 70), (60, 54, 48), (42, 38, 35)
WOOD_H, WOOD, WOOD_L = (108, 86, 60), (78, 60, 42), (50, 38, 28)

im = new(16, 20)   # tumba: monticulo de tierra y cruz de palos amarrados
d = ImageDraw.Draw(im)
d.ellipse((1, 13, 14, 19), fill=c(EARTH))
d.arc((1, 13, 14, 19), 180, 300, fill=c(EARTH_H))
d.arc((1, 13, 14, 19), 0, 120, fill=c(EARTH_L))
d.line((4, 18, 11, 18), fill=c(EARTH_L))
px(d, ((4, 15), (5, 15), (10, 16)), EARTH_H)
px(d, ((6, 17), (11, 15), (3, 17)), EARTH_L)
px(d, ((3, 16), (12, 17)), STONE)
px(d, ((4, 17), (13, 18)), STONE_S)
d.rectangle((7, 1, 8, 15), fill=c(WOOD_L))
d.line((7, 1, 7, 15), fill=c(WOOD_H))
px(d, ((7, 3), (7, 9), (7, 12)), WOOD)
for x in range(3, 13):   # el travesano, chueco
    y = 6 if x < 8 else 5
    d.point((x, y), fill=c(WOOD_H))
    d.point((x, y + 1), fill=c(WOOD_L))
px(d, ((3, 6), (12, 5)), WOOD)
px(d, ((7, 5), (8, 6), (8, 4), (7, 7)), (150, 136, 100))   # la amarra
d.line((11, 7, 11, 9), fill=c((112, 108, 100)))            # un trapo
px(d, ((11, 10),), (84, 80, 76))
P.add('CROSS', outline(im))

im = new(8, 24)   # estaca con una calavera
d = ImageDraw.Draw(im)
d.ellipse((0, 20, 7, 23), fill=c(EARTH_L))
px(d, ((2, 21), (3, 21)), EARTH)
px(d, ((1, 21),), EARTH_H)
d.rectangle((3, 7, 4, 22), fill=c(WOOD_L))
d.line((3, 7, 3, 21), fill=c(WOOD_H))
px(d, ((3, 11), (3, 16), (3, 17)), WOOD)
for (x0, x1, y) in ((2, 5, 0), (1, 6, 1), (1, 6, 2), (1, 6, 3), (1, 6, 4), (2, 5, 5), (2, 5, 6)):
    d.line((x0, y, x1, y), fill=c(BONE))
px(d, ((2, 0), (3, 0), (1, 1), (2, 1), (1, 2)), (228, 222, 204))
px(d, ((6, 2), (6, 3), (6, 4), (5, 5), (5, 0), (4, 1), (5, 2), (3, 6), (5, 6)), BONE_D)
px(d, ((2, 3), (2, 4), (5, 3), (5, 4), (3, 5), (4, 5)), INK)
px(d, ((3, 7), (4, 8), (4, 9), (3, 12), (4, 14), (4, 15), (3, 19)), (132, 24, 24))
P.add('STAKE', outline(im))

im = new(12, 34)   # farol: poste oxidado, brazo y pantalla
d = ImageDraw.Draw(im)
MET_H, MET, MET_L = (100, 102, 108), (64, 66, 72), (38, 40, 44)
d.rectangle((3, 31, 8, 33), fill=c(MET_L))
d.line((3, 31, 8, 31), fill=c(MET))
px(d, ((3, 31), (4, 31)), MET_H)
px(d, ((4, 32), (7, 32)), (20, 20, 22))
d.rectangle((5, 2, 6, 31), fill=c(MET_L))
d.line((5, 2, 5, 30), fill=c(MET_H))
d.rectangle((4, 24, 7, 25), fill=c(MET))
px(d, ((4, 24),), MET_H)
px(d, ((7, 25),), MET_L)
rnd = random.Random(8)
for y in range(5, 31):
    r = rnd.random()
    if r < 0.32:
        d.point((5, y), fill=c(RUST if r < 0.2 else RUST_D))
    if rnd.random() < 0.2:
        d.point((6, y), fill=c(RUST_D))
px(d, ((5, 18), (6, 18)), (150, 146, 130))   # un aviso pegado
px(d, ((5, 19),), (118, 114, 102))
d.line((6, 3, 11, 3), fill=c(MET_H))
px(d, ((7, 4), (6, 5)), MET)
d.rectangle((8, 4, 11, 6), fill=c(MET), outline=c(MET_L))
px(d, ((8, 4), (9, 4)), MET_H)
d.line((9, 7, 10, 7), fill=c((255, 240, 190)))
P.add('LAMP', outline(im))

im = new(32, 20)   # auto abandonado, de costado: sin vidrios, comido por el oxido, una rueda menos
d = ImageDraw.Draw(im)
p = im.load()
rnd = random.Random(5)
PAINT_H, PAINT, PAINT_L = (128, 122, 110), (100, 94, 86), (72, 67, 63)
GLASS, SHARD, HOLE = (20, 24, 28), (128, 146, 150), (22, 18, 18)
d.polygon(((7, 9), (10, 3), (22, 3), (26, 9)), fill=c(PAINT_L))
d.line((10, 3, 22, 3), fill=c(PAINT_H))
d.line((8, 8, 10, 4), fill=c(PAINT))
d.polygon(((9, 8), (11, 4), (15, 4), (15, 8)), fill=c(GLASS))
d.polygon(((17, 8), (17, 4), (21, 4), (24, 8)), fill=c(GLASS))
d.polygon(((9, 8), (10, 6), (13, 7), (14, 8)), fill=c((52, 64, 70)))   # lo que queda del vidrio de atras
d.line((10, 6, 12, 8), fill=c(SHARD))
px(d, ((13, 7), (12, 4)), SHARD)
d.rectangle((19, 6, 20, 8), fill=c((46, 37, 34)))                       # el respaldo del asiento
px(d, ((18, 8), (18, 7), (22, 8), (23, 8), (20, 4)), SHARD)
px(d, ((12, 2), (13, 2), (14, 2), (18, 2), (19, 2), (11, 3), (16, 3)), (148, 146, 140))   # ceniza en el techo
d.rounded_rectangle((0, 9, 31, 16), radius=2, fill=c(PAINT), outline=c((32, 26, 24)))
d.line((2, 10, 29, 10), fill=c(PAINT_H))
d.line((1, 14, 30, 14), fill=c(PAINT_L))
d.line((1, 15, 30, 15), fill=c(shade(PAINT_L, 0.8)))
for (cx, cy, r) in ((3, 14, 2.3), (12, 15, 2.6), (20, 14, 2.2), (28, 13, 2.4), (8, 11, 1.4), (24, 11, 1.5), (15, 12, 1.2)):
    for y in range(10, 16):   # manchones de oxido que suben desde abajo
        for x in range(1, 31):
            dd = math.hypot(x - cx, (y - cy) * 1.2) + rnd.uniform(-0.6, 0.6)
            if dd < r:
                p[x, y] = c(RUST_D if dd < r * 0.45 else RUST)
px(d, ((12, 15), (13, 14), (28, 14)), HOLE)
for (x, n) in ((11, 3), (14, 2), (19, 3), (23, 2)):   # chorreo bajo las ventanas
    for j in range(n):
        tint(p, x, 10 + j, RUST, 0.7 - 0.2 * j)
d.line((16, 4, 16, 8), fill=c(PAINT))
d.line((16, 10, 16, 15), fill=c((48, 40, 38)))
px(d, ((18, 11), (19, 11), (13, 11)), (40, 34, 32))
px(d, ((26, 11), (27, 12)), PAINT_L)                  # abolladura
px(d, ((27, 11), (28, 12)), PAINT_H)
px(d, ((26, 8), (27, 8), (28, 7), (29, 7), (30, 7)), PAINT_H)   # el capo levantado
px(d, ((28, 8), (29, 8), (30, 8)), HOLE)
px(d, ((2, 9), (3, 9), (5, 9)), (148, 146, 140))
px(d, ((30, 12),), HOLE)
px(d, ((30, 11),), (140, 138, 124))
px(d, ((1, 12),), (104, 38, 34))
d.line((28, 15, 31, 15), fill=c((88, 88, 92)))        # parachoques colgando
px(d, ((30, 16), (31, 17)), (70, 70, 74))
d.line((0, 15, 2, 15), fill=c((78, 78, 82)))
d.ellipse((4, 13, 10, 19), fill=c((24, 22, 22)))       # rueda de atras, desinflada
d.line((3, 19, 11, 19), fill=c((24, 22, 22)))
d.arc((4, 13, 10, 19), 180, 290, fill=c((60, 58, 58)))
d.ellipse((6, 15, 8, 17), fill=c(RUST))
px(d, ((7, 16),), (128, 128, 132))
d.chord((22, 12, 28, 18), 180, 360, fill=c((16, 14, 14)))   # adelante no hay rueda: el eje sobre un bloque
d.ellipse((24, 14, 26, 16), fill=c(RUST_D))
px(d, ((25, 15),), RUST)
d.rectangle((23, 17, 27, 19), fill=c((74, 72, 70)))
d.line((23, 17, 27, 17), fill=c((106, 104, 100)))
px(d, ((25, 19), (24, 18)), (46, 44, 44))
im = outline(im)
P.add('CAR_L', im.crop((0, 0, 16, 20)))
P.add('CAR_R', im.crop((16, 0, 32, 20)))
P.newline()


def tree3(seed):
    """Arbol muerto, de frente: tronco grueso y nudoso, ramas peladas."""
    rnd = random.Random(seed)
    im = new(40, 56)
    d = ImageDraw.Draw(im)
    p = im.load()
    K, DARK, MID, LITE = (24, 20, 20), (40, 33, 30), (62, 52, 45), (92, 80, 68)

    def limb(x, y, ang, length, w, depth):
        x2, y2 = x + math.cos(ang) * length, y + math.sin(ang) * length
        if depth == 0 or length < 2.2 or not (1 <= x2 <= 38 and 1 <= y2 <= 52):
            return
        d.line((x, y, x2, y2), fill=c(DARK if w > 1 else MID), width=w)
        if w > 1:   # el lado que mira a la luz
            ox, oy = (0, -1) if abs(math.cos(ang)) > 0.7 else (-1, 0)
            d.line((x + ox, y + oy, x2 + ox, y2 + oy), fill=c(LITE if w > 2 else MID))
        for _ in range(rnd.choice([2, 2, 3])):
            limb(x2, y2, ang + rnd.uniform(-0.85, 0.85), length * rnd.uniform(0.58, 0.84), max(1, w - 1), depth - 1)

    top = 29 + rnd.randrange(-2, 3)
    ph = rnd.uniform(0, 6)

    def cxs(y):
        return 20 + math.sin(y * 0.22 + ph) * 1.3

    up = -math.pi / 2
    for k in (-1, 0, 1):
        limb(cxs(top + 1), top + 2, up + k * 0.8 + rnd.uniform(-0.2, 0.2), rnd.uniform(9, 12), 3, 5)
    s = rnd.choice([-1, 1])
    limb(cxs(top + 8), top + 8, up + s * rnd.uniform(1.0, 1.3), rnd.uniform(8, 10), 2, 4)
    limb(cxs(top + 13), top + 13, up - s * rnd.uniform(1.0, 1.35), rnd.uniform(6, 9), 2, 3)
    d.line((cxs(top + 18), top + 18, cxs(top + 18) + s * 5, top + 16), fill=c(DARK), width=2)   # un munon
    knots = [top + rnd.randrange(5, 9), top + rnd.randrange(12, 17)]
    for y in range(top, 56):
        t = (y - top) / (55 - top)
        half = 1.9 + 2.3 * t ** 1.4 + (0.9 if any(0 <= y - k < 3 for k in knots) else 0) + max(0, y - 51) * 1.1
        x0, x1 = int(round(cxs(y) - half)), int(round(cxs(y) + half))
        for x in range(x0, x1 + 1):
            r = (x - x0) / max(1, x1 - x0)
            n = 0 if r < 0.2 else 1 if r < 0.5 else 2
            if (x * 5 + (y // 3) * 3 + seed) % 7 == 0:   # la corteza, a tiras
                n = (1, 2, 1)[n]
            p[x, y] = c((LITE, MID, DARK)[n])
    hy = knots[1]
    hx = int(round(cxs(hy)))
    d.ellipse((hx - 1, hy, hx + 1, hy + 3), fill=c(K))   # un hueco
    px(d, ((hx - 2, hy + 1), (hx - 2, hy + 2), (hx - 1, hy + 4)), LITE)
    px(d, ((17, 55), (22, 55), (22, 54), (25, 55), (14, 55)), K)   # entre las raices
    for _ in range(18):   # las ultimas hojas, con ceniza
        x, y = rnd.randrange(3, 37), rnd.randrange(2, 30)
        if im.getpixel((x, y))[3] or im.getpixel((min(39, x + 1), y))[3] or im.getpixel((x, min(55, y + 1)))[3]:
            px(d, ((x, y),), rnd.choice([(104, 92, 62), (88, 84, 70), (124, 118, 104)]))
    return im


P.add('TREE0', tree3(3))
P.add('TREE1', tree3(11))
P.newline()


# ================================================================ personajes, de cuerpo entero
def person3(pal, view, frame):
    """12x20. view: 0 de frente, 1 de espalda, 2 de perfil (mirando a la derecha). frame: 0 quieto, 1 y 2 caminando.
    Cada material lleva tres tonos: claro arriba a la izquierda, oscuro abajo a la derecha."""
    CAP, SKIN, SCARF, COAT, PACK, PANTS, mask = pal['cap'], pal['skin'], pal['scarf'], pal['coat'], pal['pack'], pal['pants'], pal['mask']

    def hi(col):
        return shade(col, 1.28)

    def lo(col):
        return shade(col, 0.7)

    CAP_H, CAP_L, COAT_H, COAT_L, SCARF_H, SCARF_L = hi(CAP), lo(CAP), hi(COAT), lo(COAT), hi(SCARF), lo(SCARF)
    SKIN_H, SKIN_L, PANTS_H, PANTS_L = hi(SKIN), shade(SKIN, 0.78), hi(PANTS), lo(PANTS)
    BOOT, BOOT_M, BOOT_H = (20, 18, 18), (56, 44, 36), (90, 72, 56)
    BELT, BUCKLE, DARK, LENS = (30, 26, 24), (150, 136, 100), (20, 18, 18), (178, 204, 188)
    GLOVE = (58, 50, 44) if mask else SKIN
    ROLL_H, ROLL, ROLL_L = (120, 128, 104), (92, 100, 82), (64, 70, 58)
    STRAP = shade(PACK, 0.6) if PACK else (62, 48, 36)
    CUFF = shade(COAT, 0.55)
    im = new(12, 20)
    d = ImageDraw.Draw(im)
    R = d.rectangle

    def leg(x0, x1, y1, boot0, boot1, lit):   # pantalon hasta y1 - 1, la cana de la bota en y1 y la suela debajo
        R((x0, 15, x0, y1 - 1), fill=c(PANTS_H if lit else PANTS))
        R((x1, 15, x1, y1 - 1), fill=c(PANTS if lit else PANTS_L))
        px(d, ((x0, 15),), PANTS if lit else PANTS_L)
        px(d, ((x1, 15),), PANTS_L)
        R((x0, y1, x1, y1), fill=c(BOOT_M))
        px(d, ((x0, y1),), BOOT_H)
        R((boot0, y1 + 1, boot1, y1 + 1), fill=c(BOOT))

    def arms():
        R((1, 8, 1, 12), fill=c(COAT))
        px(d, ((1, 8), (1, 9)), COAT_H)
        R((10, 8, 10, 12), fill=c(COAT_L))
        px(d, ((1, 12), (10, 12)), CUFF)
        px(d, ((1, 13), (10, 13)), GLOVE)

    def band(x0, x1, y):   # el borde de piel del gorro
        for x in range(x0, x1 + 1):
            d.point((x, y), fill=c(CAP_L if x % 2 else CAP))

    if view == 0:
        leg(3, 4, 17 if frame == 1 else 18, 2, 4, True)
        leg(7, 8, 17 if frame == 2 else 18, 7, 9, False)
        R((2, 8, 9, 14), fill=c(COAT))
        R((2, 8, 4, 8), fill=c(COAT_H))
        R((2, 9, 2, 11), fill=c(COAT_H))
        R((9, 8, 9, 14), fill=c(COAT_L))
        R((8, 13, 8, 14), fill=c(COAT_L))
        d.line((6, 9, 6, 14), fill=c(COAT_L))             # el cierre
        px(d, ((3, 13), (4, 14), (7, 10)), COAT_L)        # pliegues
        px(d, ((3, 14), (5, 13)), COAT_H)
        R((2, 12, 9, 12), fill=c(BELT))
        px(d, ((6, 12),), BUCKLE)
        if PACK:                                          # las correas de la mochila
            R((3, 8, 3, 11), fill=c(STRAP))
            R((8, 8, 8, 11), fill=c(STRAP))
        else:                                             # canana cruzada
            px(d, ((8, 8), (7, 9), (6, 10), (5, 11), (3, 13)), STRAP)
            px(d, ((7, 9), (5, 11)), (150, 122, 62))
        arms()
        R((3, 6, 8, 6), fill=c(SCARF))
        R((2, 7, 9, 7), fill=c(SCARF))
        px(d, ((3, 6), (4, 6), (2, 7)), SCARF_H)
        px(d, ((8, 6), (9, 7), (5, 7)), SCARF_L)
        if not mask:                                      # la punta de la bufanda, sobre el pecho
            R((4, 8, 5, 9), fill=c(SCARF))
            px(d, ((5, 9), (4, 10)), SCARF_L)
        R((3, 3, 8, 5), fill=c(SKIN))
        if mask:
            px(d, ((3, 3), (4, 3), (5, 3)), SKIN_H)
            R((3, 4, 8, 4), fill=c(DARK))
            px(d, ((4, 4), (7, 4)), LENS)
            px(d, ((5, 5), (6, 5)), DARK)
            px(d, ((8, 5),), SKIN_L)
        else:
            R((3, 3, 8, 3), fill=c(SKIN_L))
            px(d, ((4, 4), (7, 4)), INK)
            px(d, ((8, 4), (8, 5)), SKIN_L)
            px(d, ((5, 5), (6, 5)), shade(SKIN, 0.86))
        R((2, 2, 2, 6), fill=c(CAP))
        R((9, 2, 9, 6), fill=c(CAP_L))
        px(d, ((2, 6),), CAP_L)
        R((2, 1, 9, 1), fill=c(CAP))
        R((3, 0, 8, 0), fill=c(CAP))
        px(d, ((3, 0), (4, 0), (5, 0), (2, 1), (3, 1), (4, 1)), CAP_H)
        px(d, ((8, 0), (9, 1)), CAP_L)
        band(2, 9, 2)
    elif view == 1:
        leg(3, 4, 17 if frame == 1 else 18, 2, 4, True)
        leg(7, 8, 17 if frame == 2 else 18, 7, 9, False)
        R((2, 8, 9, 14), fill=c(COAT))
        R((2, 8, 2, 11), fill=c(COAT_H))
        R((9, 8, 9, 14), fill=c(COAT_L))
        px(d, ((4, 14), (7, 14)), COAT_L)
        px(d, ((3, 14),), COAT_H)
        R((2, 12, 9, 12), fill=c(BELT))
        arms()
        if PACK:
            R((3, 9, 8, 13), fill=c(PACK))
            R((3, 9, 7, 9), fill=c(hi(PACK)))
            R((3, 10, 3, 12), fill=c(hi(PACK)))
            R((8, 9, 8, 13), fill=c(lo(PACK)))
            R((3, 13, 8, 13), fill=c(lo(PACK)))
            R((4, 11, 7, 11), fill=c(lo(PACK)))           # la tapa del bolsillo
            px(d, ((5, 12),), BUCKLE)
            R((1, 8, 10, 8), fill=c(ROLL))                # el rollo de la manta
            px(d, ((1, 8), (2, 8), (5, 8), (6, 8)), ROLL_H)
            px(d, ((10, 8), (9, 8)), ROLL_L)
            px(d, ((3, 8), (8, 8)), STRAP)
        else:
            d.line((5, 9, 5, 14), fill=c(COAT_L))
            px(d, ((3, 10), (7, 11), (7, 13)), COAT_L)
            px(d, ((3, 9), (4, 9)), COAT_H)
            px(d, ((3, 8), (4, 9), (5, 10), (6, 11), (8, 13)), STRAP)
        R((3, 6, 8, 6), fill=c(SCARF))
        R((2, 7, 9, 7), fill=c(SCARF))
        px(d, ((3, 6), (2, 7), (3, 7)), SCARF_H)
        px(d, ((8, 6), (9, 7), (6, 7)), SCARF_L)
        R((2, 1, 9, 5), fill=c(CAP))
        R((3, 0, 8, 0), fill=c(CAP))
        px(d, ((3, 0), (4, 0), (5, 0), (2, 1), (3, 1), (4, 1), (2, 2), (3, 2)), CAP_H)
        R((9, 1, 9, 5), fill=c(CAP_L))
        px(d, ((8, 0), (6, 2), (6, 3)), CAP_L)
        band(2, 9, 5)
        px(d, ((2, 6), (9, 6)), CAP_L)
    else:
        if frame == 0:
            leg(5, 6, 18, 5, 7, True)
        elif frame == 1:
            leg(3, 4, 17, 2, 4, False)
            leg(7, 8, 18, 7, 9, True)
        else:
            leg(4, 5, 18, 4, 6, False)
            leg(6, 7, 17, 6, 8, True)
        if PACK:
            R((1, 9, 3, 13), fill=c(PACK))
            R((1, 9, 1, 12), fill=c(hi(PACK)))
            R((1, 13, 3, 13), fill=c(lo(PACK)))
            px(d, ((2, 11),), lo(PACK))
            R((1, 8, 3, 8), fill=c(ROLL))
            px(d, ((1, 7), (2, 7)), ROLL_H)
            px(d, ((3, 8),), ROLL_L)
        R((4, 8, 8, 14), fill=c(COAT))
        px(d, ((4, 8), (5, 8), (6, 8)), COAT_H)
        if not PACK:
            R((4, 9, 4, 11), fill=c(COAT_H))
        R((8, 11, 8, 14), fill=c(COAT_L))
        px(d, ((6, 13), (5, 14), (7, 14)), COAT_L)
        px(d, ((4, 13), (4, 14)), COAT_H)
        R((4, 12, 8, 12), fill=c(BELT))
        px(d, ((5, 9), (5, 10), (4, 11)) if PACK else ((5, 9), (6, 11)), STRAP)
        R((7, 9, 10, 9), fill=c(COAT))                    # el brazo, hacia el arma
        R((7, 10, 10, 10), fill=c(COAT_L))
        px(d, ((7, 9),), COAT_H)
        px(d, ((10, 9), (10, 10)), CUFF)
        px(d, ((11, 10),), GLOVE)
        R((4, 6, 8, 7), fill=c(SCARF))
        px(d, ((4, 6), (5, 6)), SCARF_H)
        px(d, ((8, 7), (6, 7)), SCARF_L)
        if not mask:                                      # la punta de la bufanda, al viento
            R((3, 7, 3, 9), fill=c(SCARF))
            px(d, ((3, 10),), SCARF_L)
            px(d, ((2, 9) if frame == 2 else (2, 10),), SCARF)
        R((6, 3, 8, 5), fill=c(SKIN))
        if mask:
            px(d, ((6, 3), (7, 3)), SKIN_H)
            R((6, 4, 8, 4), fill=c(DARK))
            px(d, ((8, 4),), LENS)
            px(d, ((8, 5),), DARK)
        else:
            R((6, 3, 8, 3), fill=c(SKIN_L))
            px(d, ((8, 4),), INK)
            px(d, ((6, 5),), SKIN_L)
            px(d, ((9, 4),), SKIN)
        R((3, 3, 5, 5), fill=c(CAP_L))                    # la orejera
        px(d, ((3, 3), (4, 3), (3, 4)), CAP)
        R((3, 1, 8, 1), fill=c(CAP))
        R((4, 0, 8, 0), fill=c(CAP))
        px(d, ((4, 0), (5, 0), (6, 0), (3, 1), (4, 1), (5, 1)), CAP_H)
        px(d, ((8, 0),), CAP_L)
        band(3, 8, 2)
        px(d, ((9, 2),), CAP_L)
    return edged(im)


CAST = {
    # chatarrero: gorro con orejeras, bufanda roja, abrigo verde gris, mochila con manta
    'PLY': dict(cap=(70, 64, 58), skin=(198, 160, 126), scarf=(176, 40, 36), coat=(72, 86, 66), pack=(98, 72, 48), pants=(50, 48, 58), mask=False),
    # merodeador: pasamontanas con antiparras, abrigo rojo oscuro, sin mochila
    'MAR': dict(cap=(42, 42, 48), skin=(42, 42, 48), scarf=(98, 54, 48), coat=(98, 54, 48), pack=None, pants=(38, 38, 44), mask=True),
}
for name, pal in CAST.items():
    for v, vn in enumerate('FBS'):
        for f in range(3):
            P.add(f'{name}_{vn}{f}', person3(pal, v, f))


def dog3(frame):
    """Perro mutante de perfil, mirando a la derecha: flaco, con el costillar y el craneo a la vista."""
    FUR_H, FUR, FUR_D, FUR_K = (124, 108, 92), (92, 79, 68), (56, 47, 42), (34, 29, 27)
    FLESH, FLESH_D, RAW = (152, 50, 48), (98, 32, 34), (200, 96, 86)
    im = new(18, 12)
    d = ImageDraw.Draw(im)
    if frame == 0:   # estirado
        d.line((10, 8, 11, 11), fill=c(FUR_K))
        d.line((3, 7, 1, 11), fill=c(FUR_K))
        d.line((11, 8, 13, 11), fill=c(FUR_D))
        d.line((5, 7, 4, 11), fill=c(FUR_D))
        px(d, ((12, 9), (5, 9)), BONE_D)
    else:            # recogido
        d.line((10, 8, 9, 11), fill=c(FUR_K))
        d.line((4, 7, 5, 11), fill=c(FUR_K))
        d.line((11, 8, 11, 11), fill=c(FUR_D))
        d.line((5, 7, 7, 11), fill=c(FUR_D))
        px(d, ((11, 9), (6, 9)), BONE_D)
    d.line((0, 1, 2, 4), fill=c(FUR_D))                  # cola pelada
    px(d, ((0, 1), (1, 3)), BONE_D)
    d.polygon(((2, 4), (5, 3), (11, 2), (13, 4), (12, 8), (9, 8), (6, 6), (3, 7)), fill=c(FUR))
    d.line((2, 4, 5, 3, 11, 2), fill=c(FUR_H))
    d.line((3, 7, 6, 6, 9, 8, 12, 8), fill=c(FUR_D))     # el vientre, hundido
    px(d, ((4, 2), (6, 2), (8, 1), (10, 1)), BONE_D)     # las vertebras
    px(d, ((3, 5),), FUR_D)
    d.polygon(((6, 4), (10, 3), (11, 7), (8, 6)), fill=c(FLESH_D))   # el costillar, en carne viva
    for x, y0, y1 in ((6, 4, 5), (8, 3, 6), (10, 3, 7)):
        d.line((x, y0, x, y1), fill=c(BONE))
    px(d, ((7, 4), (9, 5), (11, 6)), FLESH)
    px(d, ((9, 4),), RAW)
    d.ellipse((11, 1, 16, 6), fill=c(FUR))
    px(d, ((12, 2), (12, 3)), FUR_H)
    px(d, ((13, 1), (14, 1)), BONE)                      # el craneo pelado
    px(d, ((13, 2),), FLESH)
    px(d, ((14, 2), (15, 2), (14, 3)), FUR_K)
    px(d, ((15, 3),), (240, 74, 50))
    px(d, ((12, 5),), FLESH)
    d.rectangle((15, 4, 17, 5), fill=c(FUR_D))
    px(d, ((17, 4),), FUR_K)
    px(d, ((12, 0), (11, 1)), FUR_D)                     # la oreja rota
    px(d, ((16, 6), (17, 6)), (224, 218, 204))
    if frame == 0:
        px(d, ((15, 6),), FUR_D)
        px(d, ((17, 7),), (150, 28, 28))
    else:            # con el hocico abierto
        px(d, ((15, 6),), FLESH_D)
        px(d, ((14, 7), (15, 7), (16, 7)), FUR_D)
    return edged(im)


for f in range(2):
    P.add(f'DOG{f}', dog3(f))

# armas en la mano, apuntando a la derecha
im = new(7, 4)
d = ImageDraw.Draw(im)
d.rectangle((0, 0, 6, 1), fill=c((38, 38, 42)))
d.line((0, 0, 6, 0), fill=c((92, 92, 98)))
d.rectangle((0, 2, 1, 3), fill=c((30, 28, 28)))
P.add('PISTOL', im)

im = new(10, 4)
d = ImageDraw.Draw(im)
d.rectangle((3, 0, 9, 1), fill=c((52, 52, 56)))
d.line((3, 0, 9, 0), fill=c((104, 104, 110)))
d.rectangle((0, 0, 3, 2), fill=c((112, 76, 46)))
d.rectangle((0, 2, 1, 3), fill=c((84, 56, 34)))
P.add('SHOTGUN', im)

im = new(13, 5)
d = ImageDraw.Draw(im)
d.rectangle((0, 1, 3, 2), fill=c((106, 72, 44)))
d.rectangle((3, 0, 8, 2), fill=c((40, 40, 44)))
d.line((3, 0, 8, 0), fill=c((88, 88, 94)))
d.rectangle((8, 1, 12, 1), fill=c((52, 52, 56)))
d.line((6, 3, 7, 4), fill=c((30, 30, 34)))
d.rectangle((4, 3, 4, 3), fill=c((30, 28, 28)))
P.add('RIFLE', im)

# ---------------------------------------------------------------- lo que brilla
im = new(6, 2)
d = ImageDraw.Draw(im)
d.line((0, 0, 5, 0), fill=c((255, 214, 120)))
d.line((0, 1, 5, 1), fill=c((232, 150, 60)))
px(d, ((5, 0),), (255, 255, 230))
px(d, ((4, 0),), (255, 244, 190))
P.add('BULLET', im)

im = new(12, 12)
d = ImageDraw.Draw(im)
d.polygon(((0, 6), (5, 4), (11, 6), (5, 8)), fill=c((255, 190, 70)))
d.polygon(((2, 2), (6, 5), (2, 10), (4, 6)), fill=c((255, 150, 50)))
d.polygon(((1, 6), (5, 5), (9, 6), (5, 7)), fill=c((255, 250, 210)))
P.add('FLASH', im)


def flame(seed):
    rnd = random.Random(seed)
    im = new(8, 12)
    d = ImageDraw.Draw(im)
    tip = rnd.choice([2, 3, 4, 5])
    d.polygon(((0, 11), (1, 6), (tip, 0), (6, 5), (7, 11)), fill=c((214, 70, 30)))
    d.polygon(((1, 11), (2, 7), (tip, 3), (5, 7), (6, 11)), fill=c((246, 150, 40)))
    d.polygon(((2, 11), (3, 8), (4, 6), (5, 9), (5, 11)), fill=c((255, 230, 130)))
    return im


for i in range(3):
    P.add(f'FIRE{i}', flame(100 + i))

im = new(6, 10)   # ampolleta colgando de un cable
d = ImageDraw.Draw(im)
d.line((3, 0, 3, 4), fill=c((40, 40, 42)))
d.ellipse((0, 4, 5, 9), fill=c((120, 118, 110)), outline=c((50, 50, 50)))
d.rectangle((2, 6, 3, 7), fill=c((255, 244, 200)))
P.add('CEIL', im)

im = new()   # la mira del apuntado automatico: cuatro esquinas
d = ImageDraw.Draw(im)
RED = c((228, 62, 52))
for (x, y, sx, sy) in ((0, 0, 1, 1), (15, 0, -1, 1), (0, 15, 1, -1), (15, 15, -1, -1)):
    d.line((x, y, x + 3 * sx, y), fill=RED)
    d.line((x, y, x, y + 3 * sy), fill=RED)
P.add('RETICLE', im)


# ---------------------------------------------------------------- iconos
def icon(draw_fn, size=8):
    im = new(size, size)
    draw_fn(ImageDraw.Draw(im))
    return im


def i_ammo(d):
    d.rectangle((1, 2, 6, 6), fill=c((78, 90, 58)), outline=c((34, 40, 26)))
    d.line((2, 4, 5, 4), fill=c((208, 176, 70)))


def i_med(d):
    d.rectangle((0, 1, 7, 6), fill=c((220, 220, 212)), outline=c((60, 60, 60)))
    d.line((3, 2, 4, 2), fill=c((200, 40, 40)))
    d.line((2, 3, 5, 3), fill=c((200, 40, 40)))
    d.line((2, 4, 5, 4), fill=c((200, 40, 40)))
    d.line((3, 5, 4, 5), fill=c((200, 40, 40)))


def i_band(d):
    d.ellipse((1, 1, 6, 6), fill=c((226, 220, 204)), outline=c((90, 84, 74)))
    d.rectangle((3, 3, 4, 4), fill=c((150, 140, 124)))


def i_food(d):
    d.rectangle((1, 1, 6, 6), fill=c((150, 152, 156)), outline=c((60, 60, 64)))
    d.rectangle((2, 3, 5, 5), fill=c((176, 60, 48)))
    d.line((2, 1, 5, 1), fill=c((208, 208, 212)))


def i_arad(d):
    d.rectangle((2, 0, 5, 7), fill=c((214, 184, 52)), outline=c((50, 44, 20)))
    d.line((3, 3, 4, 3), fill=c((30, 30, 30)))
    d.line((3, 5, 4, 5), fill=c((30, 30, 30)))


def i_scrap(d):
    d.ellipse((1, 1, 6, 6), fill=c((124, 126, 132)), outline=c((50, 50, 54)))
    d.rectangle((3, 3, 4, 4), fill=c((40, 40, 44)))
    px(d, ((3, 0), (0, 4), (7, 3), (4, 7)), (124, 126, 132))


def i_heart(d):
    for (x0, y0, x1, y1) in ((1, 1, 2, 3), (5, 1, 6, 3), (0, 2, 7, 3), (1, 4, 6, 4), (2, 5, 5, 5), (3, 6, 4, 6)):
        d.rectangle((x0, y0, x1, y1), fill=c((204, 50, 50)))
    d.point((1, 2), fill=c((250, 150, 140)))


def i_rad(d):
    d.ellipse((0, 0, 7, 7), fill=c((226, 196, 54)), outline=c((60, 50, 16)))
    d.rectangle((3, 3, 4, 4), fill=c((36, 32, 20)))
    d.line((3, 1, 4, 1), fill=c((36, 32, 20)))
    px(d, ((1, 5), (6, 5), (2, 5), (5, 5)), (36, 32, 20))


def i_bolt(d):
    d.polygon(((4, 0), (1, 4), (3, 4), (2, 7), (6, 3), (4, 3)), fill=c((120, 190, 226)))


def i_drop(d):
    d.polygon(((3, 0), (1, 4), (1, 5), (3, 7), (4, 7), (6, 5), (6, 4), (4, 0)), fill=c((170, 26, 30)))
    d.point((2, 4), fill=c((236, 120, 110)))


def i_bullet(d):
    d.rectangle((2, 3, 5, 7), fill=c((196, 156, 62)))
    d.polygon(((2, 3), (3, 0), (4, 0), (5, 3)), fill=c((150, 96, 60)))
    d.line((2, 3, 2, 7), fill=c((240, 210, 120)))


for nm, fn in (('I_AMMO', i_ammo), ('I_MED', i_med), ('I_BAND', i_band), ('I_FOOD', i_food), ('I_ARAD', i_arad),
               ('I_SCRAP', i_scrap), ('I_HEART', i_heart), ('I_RAD', i_rad), ('I_BOLT', i_bolt), ('I_DROP', i_drop),
               ('I_BULLET', i_bullet)):
    P.add(nm, icon(fn))

P.add('WHITE', new(4, 4, (255, 255, 255, 255)))

# ---------------------------------------------------------------- letras (Latin-1), sin suavizado
FONT_PX = 9
font = ImageFont.truetype(FONT_TTF, FONT_PX)
asc, desc = font.getmetrics()
FONT_H = asc + desc
P.newline()
glyphs = {}
for code in list(range(32, 127)) + list(range(161, 256)):
    ch = chr(code)
    adv = max(2, int(round(font.getlength(ch))))
    im = Image.new('L', (adv + 1, FONT_H), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    d.text((0, 0), ch, fill=255, font=font)
    rgba = new(adv + 1, FONT_H)
    rgba.paste((255, 255, 255, 255), mask=im.point(lambda v: 255 if v > 127 else 0))
    x, y = P.add(None, rgba, pad=0)
    glyphs[code] = (x, y, adv)

# ---------------------------------------------------------------- salida
# (al importarlo desde tools/make_atlas.py no escribe nada: solo deja listos atlas, entries y glyphs)
if __name__ == '__main__':
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    with open(os.path.join(ROOT, 'assets', 'atlas.rgba'), 'wb') as f:
        f.write(atlas.tobytes())
    atlas.save(os.path.join(ROOT, 'build', 'atlas.png'))
    bg = Image.new('RGBA', (AW, AH), (60, 30, 70, 255))
    bg.alpha_composite(atlas)
    bg.resize((AW * 3, AH * 3), Image.NEAREST).save(os.path.join(ROOT, 'build', 'atlas_x3.png'))

    with open(os.path.join(ROOT, 'src', 'atlas.h'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('// GENERADO por tools/make_art.py: no editar a mano\n#ifndef ATLAS_H\n#define ATLAS_H\n\n')
        f.write(f'#define ATLAS_W {AW}\n#define ATLAS_H_PX {AH}\n#define FONT_H {FONT_H}\n\n')
        f.write('enum {\n' + ''.join(f'    S_{n},\n' for n, *_ in entries) + '    S_COUNT\n};\n\n')
        f.write('static const Spr SPR[S_COUNT] __attribute__((unused)) = {\n')
        for n, x, y, w, h in entries:
            f.write(f'    {{{x}, {y}, {w}, {h}}},   // {n}\n')
        f.write('};\n\n// letras: codigo 32..255 -> x, y, avance\n')
        for nm, idx, typ in (('FONT_X', 0, 'unsigned short'), ('FONT_Y', 1, 'unsigned char'), ('FONT_ADV', 2, 'unsigned char')):
            vals = [glyphs.get(code, (0, 0, 0))[idx] for code in range(32, 256)]
            f.write(f'static const {typ} {nm}[224] __attribute__((unused)) = {{' + ', '.join(map(str, vals)) + '};\n')
        f.write('\n#endif\n')
    print(f'atlas: {len(entries)} sprites, {len(glyphs)} letras, alto usado {P.y + P.rowh} de {AH}, letra {FONT_H} px')
