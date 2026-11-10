# Suelos nuevos por tipo de mapa, dibujados por codigo en una gama apagada y oscura. make_atlas.py los agrega solo.
# [fab2] Cada juego tiene un dibujo propio que se lee a simple vista (losas biseladas, adoquines, grava con brillo,
# hojas sueltas, placas de roca con grietas) y mas contraste interno, sin subir el tono general.
import random
from PIL import Image

K = 1.30   # brillo general: el velo oscuro y el tinte del mapa se comen buena parte


def cl(c):
    return tuple(max(0, min(255, int(v))) for v in c)


def sh(c, d):
    return cl((c[0] + d, c[1] + d, c[2] + d))


class T:
    def __init__(self, base):
        self.im = Image.new('RGBA', (16, 16), cl(base) + (255,))
        self.px = self.im.load()

    def p(self, x, y, c):
        self.px[x % 16, y % 16] = cl(c) + (255,)

    def get(self, x, y):
        return self.px[x % 16, y % 16][:3]

    def noise(self, r, n, d):   # variacion fina alrededor del color que ya hay
        for _ in range(n):
            x, y = r.randrange(16), r.randrange(16)
            self.p(x, y, sh(self.get(x, y), r.randint(-d, d)))

    def done(self):
        for y in range(16):
            for x in range(16):
                c = self.px[x, y]
                self.px[x, y] = cl((c[0] * K, c[1] * K, c[2] * K)) + (255,)
        return self.im


def crack(t, r, col, n=12, x=None):
    x, y = (r.randrange(2, 14) if x is None else x), r.randrange(0, 4)
    for _ in range(n):
        t.p(x, y, col); t.p(x + 1, y, sh(t.get(x + 1, y), 14))   # filo claro al lado de la grieta
        y += 1; x += r.choice((-1, 0, 0, 1))


def asfalto(r, v):
    t = T((38, 39, 43))
    for _ in range(46): t.p(r.randrange(16), r.randrange(16), r.choice(((52, 53, 58), (58, 58, 62), (28, 28, 31))))
    for _ in range(6): t.p(r.randrange(16), r.randrange(16), (76, 76, 80))   # aridos claros
    if v:   # grieta sellada con brea
        crack(t, r, (16, 16, 18), 14)
    if v == 3:
        for x in range(16): t.p(x, 13, (64, 62, 52)); t.p(x, 14, (40, 40, 40))   # resto de pintura gastada
        for _ in range(8): t.p(r.randrange(16), 13, (38, 39, 43))
    return t.done()


def losas(r, v):
    t = T((80, 79, 75))
    for ox in (0, 8):
        for oy in (0, 8):
            d = r.randint(-8, 8)
            for y in range(8):
                for x in range(8):
                    c = sh((80, 79, 75), d)
                    if x == 0 or y == 0: c = sh(c, 14)
                    if x == 7 or y == 7: c = (54, 53, 50)
                    t.p(ox + x, oy + y, c)
    t.noise(r, 40, 9)
    if v == 1: crack(t, r, (36, 35, 33), 7, 4)
    if v == 2:
        for (x, y) in ((10, 10), (11, 10), (10, 11), (12, 11), (11, 12)): t.p(x, y, (58, 56, 50))   # mancha
    return t.done()


def hormigon(r, v, oil=False):
    t = T((66, 66, 67))
    t.noise(r, 90, 8)
    for i in range(16): t.p(15, i, (42, 42, 44)); t.p(i, 15, (42, 42, 44)); t.p(0, i, sh(t.get(0, i), 10)); t.p(i, 0, sh(t.get(i, 0), 10))   # junta de corte
    if v == 2 and not oil: crack(t, r, (34, 34, 36), 9)
    if v == 3 and not oil:
        for (x, y) in ((4, 5), (5, 5), (5, 6), (6, 6), (4, 6)): t.p(x, y, (50, 46, 40))
    if oil:   # mancha de aceite con borde tornasol
        blobs = [(r.randint(3, 12), r.randint(3, 12), r.uniform(2.5, 5), r.uniform(2, 3.5)) for _ in range(3)]   # mancha irregular: tres gotas que se juntan
        for y in range(16):
            for x in range(16):
                q = min(((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 for (cx, cy, rx, ry) in blobs)
                if q < 0.6: t.p(x, y, (22, 22, 28))
                elif q < 1.0: t.p(x, y, r.choice(((36, 38, 50), (42, 36, 52), (32, 46, 48), (44, 44, 46))))
        cx, cy = blobs[0][0], blobs[0][1]
        t.p(cx - 1, cy - 1, (86, 88, 104))   # brillo
    return t.done()


def rejilla(r, v):
    t = T((24, 26, 30))
    for y in range(16):
        for x in range(16):
            if x % 4 == 0 or y % 4 == 0: t.p(x, y, (68, 72, 78))
            elif x % 4 == 1 or y % 4 == 1: t.p(x, y, (16, 16, 18))
    for _ in range(10): t.p(r.randrange(16), r.randrange(16), r.choice(((104, 64, 36), (80, 50, 32))))   # oxido
    return t.done()


def peligro(r, v):
    t = T((28, 28, 30))
    for y in range(16):
        for x in range(16):
            if ((x + y + v) // 4) % 2 == 0: t.p(x, y, (158, 128, 38))
    for _ in range(26): t.p(r.randrange(16), r.randrange(16), r.choice(((70, 70, 70), (96, 84, 46))))   # gastado
    return t.done()


def balasto(r, v):
    t = T((46, 43, 40))
    for _ in range(26):
        x, y = r.randrange(16), r.randrange(16); b = r.randint(-10, 12)
        t.p(x, y, sh((108, 102, 94), b)); t.p(x + 1, y, sh((80, 76, 70), b)); t.p(x, y + 1, sh((76, 72, 66), b)); t.p(x + 1, y + 1, (30, 28, 26))
    return t.done()


def tierra(r, v):
    t = T((64, 46, 32))
    t.noise(r, 60, 10)
    for _ in range(9):
        x, y = r.randrange(16), r.randrange(16)
        t.p(x, y, (92, 70, 48)); t.p(x + 1, y, (84, 62, 42)); t.p(x, y + 1, (40, 28, 20)); t.p(x + 1, y + 1, (40, 28, 20))   # terron
    for _ in range(4): t.p(r.randrange(16), r.randrange(16), (112, 106, 94))   # piedrecita
    return t.done()


def adoquin(r, v):
    t = T((28, 26, 24))
    for row in range(4):
        y0, off = row * 4, (row % 2) * 3 + v
        for k in range(-1, 4):
            x0, b = off + k * 5, r.randint(-10, 10)
            for y in range(3):
                for x in range(4):
                    c = sh((82, 76, 70), b)
                    if y == 0 or x == 0: c = sh(c, 24)
                    if y == 2 or x == 3: c = sh(c, -22)
                    t.p(x0 + x, y0 + y, c)
    t.noise(r, 14, 8)
    return t.done()


def barro(r, v):
    t = T((44, 33, 25))
    t.noise(r, 70, 9)
    for y in (r.randrange(16), r.randrange(16)):   # huellas de rueda
        for x in range(16): t.p(x, y, (28, 20, 15)); t.p(x, y + 1, sh(t.get(x, y + 1), 8))
    cx, cy = r.randrange(3, 12), r.randrange(3, 12)   # charco que brilla
    for (x, y) in ((cx, cy), (cx + 1, cy), (cx + 2, cy), (cx, cy + 1), (cx + 1, cy + 1), (cx + 2, cy + 1), (cx + 3, cy + 1)): t.p(x, y, (40, 44, 50))
    t.p(cx + 1, cy, (104, 108, 116))
    return t.done()


def hojarasca(r, v):
    t = T((32, 27, 20))
    t.noise(r, 40, 6)
    for _ in range(4):   # ramitas
        x, y = r.randrange(16), r.randrange(16)
        for k in range(3): t.p(x + k, y + (k // 2), (58, 44, 30))
    for _ in range(17):   # hojas: dos o tres pixeles con nervio oscuro
        x, y = r.randrange(16), r.randrange(16)
        c = r.choice(((132, 76, 30), (104, 52, 26), (140, 108, 42), (82, 70, 34), (118, 40, 28)))
        t.p(x, y, c); t.p(x + 1, y, sh(c, -30)); t.p(x, y + 1, sh(c, -14))
        if r.random() < 0.5: t.p(x + 1, y + 1, sh(c, 16))
    return t.done()


def pasto(r, v):
    t = T((50, 45, 28))
    for _ in range(30):   # matas: trazos verticales
        x, y = r.randrange(16), r.randrange(16); c = r.choice(((112, 100, 56), (92, 84, 44), (78, 86, 44)))
        t.p(x, y, c); t.p(x, y + 1, sh(c, -20)); t.p(x, y + 2, (34, 30, 18))
    return t.done()


def grava(r, v):
    t = T((78, 75, 70))
    for _ in range(36):
        x, y = r.randrange(16), r.randrange(16)
        t.p(x, y, (118, 114, 106)); t.p(x, y + 1, (48, 46, 43))
    t.noise(r, 50, 12)
    return t.done()


def roca(r, v):
    t = T((60, 57, 56))
    t.noise(r, 70, 9)
    for k in range(2):   # placas separadas por grietas hondas con filo claro arriba
        y = r.randrange(2, 14)
        x = 0
        while x < 16:
            t.p(x, y, (22, 22, 24)); t.p(x, y - 1, (96, 92, 88)); t.p(x, y + 1, (40, 38, 38))
            x += 1
            if r.random() < 0.3: y += r.choice((-1, 1))
    crack(t, r, (26, 26, 28), 8)
    for _ in range(3):   # liquen
        x, y = r.randrange(16), r.randrange(16); t.p(x, y, (74, 84, 56)); t.p(x + 1, y, (62, 70, 48))
    return t.done()


SETS = [('ASF', asfalto), ('LOS', losas), ('HOR', hormigon), ('REJ', rejilla), ('PEL', peligro),
        ('ACE', lambda r, v: hormigon(r, v, True)), ('BAL', balasto), ('TIE', tierra), ('ADO', adoquin),
        ('BAR', barro), ('HOJ', hojarasca), ('PAS', pasto), ('GRA', grava), ('ROC', roca)]
SPRITES = []
for i, (nm, fn) in enumerate(SETS):
    for k in range(4):
        SPRITES.append((f'AR_{nm}{k}', fn(random.Random(i * 31 + k), k)))

import make_fab
SPRITES += make_fab.SPRITES

if __name__ == '__main__':   # hoja de muestra para revisar el contraste
    im = Image.new('RGBA', (4 * 16 * 3, len(SETS) * 16 * 3), (0, 0, 0, 255))
    for n, (nm, sp) in enumerate(SPRITES[:len(SETS) * 4]):
        im.paste(sp.resize((48, 48), Image.NEAREST), ((n % 4) * 48, (n // 4) * 48))
    import sys
    im.save(sys.argv[1] if len(sys.argv) > 1 else 'suelos.png')
