# Piezas de la fabrica abandonada dibujadas por codigo (gama apagada). make_arte.py las suma a SPRITES.
import random
from PIL import Image, ImageDraw

BR = (92, 54, 44); BRD = (62, 38, 32); BRL = (112, 68, 54)
CO = (88, 88, 86); COD = (60, 60, 60); COL_ = (110, 110, 106)


def new(w, h):
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def muro(kind, seed):
    """Muro de nave 16x28: arriba la cubierta (12 px), abajo la fachada (16 px)."""
    r = random.Random(seed)
    im, d = new(16, 28)
    brick = kind in (0, 1, 5)
    base, dark, light = (BR, BRD, BRL) if brick else (CO, COD, COL_)
    d.rectangle((0, 0, 15, 11), fill=(52, 52, 56))
    d.rectangle((0, 0, 15, 2), fill=(70, 70, 74))
    for x in range(0, 16, 5):
        d.line((x, 3, x, 11), fill=(40, 40, 44))
    d.rectangle((0, 12, 15, 27), fill=base)
    d.rectangle((0, 12, 15, 13), fill=light)
    if brick:
        for y in range(14, 28, 3):
            d.line((0, y, 15, y), fill=dark)
            for x in range((y // 3) % 2 * 2, 16, 4):
                d.line((x, y, x, y + 2), fill=dark)
    else:
        for _ in range(26):
            d.point((r.randrange(16), r.randrange(14, 28)), fill=r.choice((dark, light)))
        d.line((8, 14, 8, 27), fill=dark)
    d.rectangle((0, 26, 15, 27), fill=dark)
    if kind == 0:
        d.rectangle((3, 16, 12, 23), fill=(18, 22, 26)); d.rectangle((4, 17, 11, 22), fill=(40, 54, 62))
        d.line((7, 17, 7, 22), fill=(18, 22, 26)); d.line((4, 20, 11, 20), fill=(18, 22, 26))
    elif kind == 1:   # ventanal roto
        d.rectangle((2, 15, 13, 24), fill=(10, 12, 14))
        for (x0, y0, x1, y1) in ((3, 16, 6, 18), (9, 17, 12, 19), (4, 22, 8, 23), (10, 22, 12, 23)):
            d.rectangle((x0, y0, x1, y1), fill=(46, 62, 70))
        for (a, b) in (((3, 16), (6, 20)), ((12, 17), (9, 22)), ((6, 23), (9, 21))):
            d.line(a + b, fill=(150, 170, 176))
        d.line((7, 15, 7, 24), fill=(30, 32, 34)); d.rectangle((2, 25, 13, 25), fill=dark)
    elif kind == 2:   # tuberias
        for y, c in ((16, (110, 80, 50)), (21, (76, 90, 96))):
            d.rectangle((0, y, 15, y + 2), fill=c); d.line((0, y, 15, y), fill=tuple(min(255, v + 36) for v in c))
            for x in (4, 11):
                d.rectangle((x, y - 1, x + 1, y + 3), fill=(40, 40, 42))
        d.rectangle((13, 14, 14, 27), fill=(86, 100, 106)); d.line((13, 14, 13, 27), fill=(130, 144, 150))
    elif kind == 3:   # tablero electrico
        d.rectangle((3, 15, 12, 25), fill=(24, 26, 28)); d.rectangle((4, 16, 11, 24), fill=(70, 78, 82))
        d.rectangle((5, 17, 10, 19), fill=(30, 36, 40)); d.point((6, 18), fill=(90, 200, 100)); d.point((8, 18), fill=(210, 70, 50))
        d.line((5, 21, 10, 21), fill=(40, 44, 48)); d.line((7, 12, 7, 15), fill=(20, 20, 22))
    elif kind == 4:   # franja de peligro y rejilla
        for x in range(16):
            if (x // 3) % 2 == 0:
                d.line((x, 23, x, 25), fill=(120, 104, 40))
        d.rectangle((3, 16, 12, 20), fill=(22, 24, 26))
        for x in range(3, 13, 2):
            d.line((x, 16, x, 20), fill=(74, 78, 82))
    else:
        d.rectangle((4, 15, 7, 17), fill=dark); d.line((10, 14, 11, 27), fill=(46, 30, 26))
    return im


def chimenea():
    im, d = new(16, 84)
    for y in range(84):
        hw = 3.0 + 2.2 * y / 83.0
        x0, x1 = int(8 - hw), int(8 + hw)
        for x in range(x0, x1 + 1):
            c = BRL if x < x0 + 2 else BRD if x > x1 - 2 else BR
            if y % 3 == 0:
                c = tuple(max(0, v - 14) for v in c)
            d.point((x, y), fill=c)
        if y in (16, 17, 40, 41, 64, 65):
            d.line((x0 - 1, y, x1 + 1, y), fill=(36, 34, 34))
    d.rectangle((3, 0, 12, 3), fill=(36, 34, 34)); d.rectangle((4, 0, 11, 1), fill=(16, 16, 16))
    d.rectangle((2, 80, 13, 83), fill=(60, 60, 60))
    return im


def torre():
    im, d = new(32, 48)
    for y in range(48):
        w = 11 + 3.5 * abs(y - 22) / 22 + (1.5 if y > 40 else 0)
        x0, x1 = int(16 - w), int(16 + w)
        for x in range(x0, x1 + 1):
            c = (116, 116, 112) if x < x0 + 5 else (70, 70, 70) if x > x1 - 6 else (92, 92, 90)
            if (y // 4) % 2 == 0:
                c = tuple(max(0, v - 10) for v in c)
            d.point((x, y), fill=c)
    d.ellipse((7, 0, 24, 6), fill=(30, 30, 32)); d.ellipse((9, 1, 22, 4), fill=(14, 14, 16))
    d.rectangle((9, 44, 22, 47), fill=(40, 40, 42))
    return im


def silo():
    im, d = new(32, 48)
    d.rectangle((5, 10, 26, 46), fill=(104, 92, 78))
    d.rectangle((5, 10, 10, 46), fill=(136, 122, 104)); d.rectangle((21, 10, 26, 46), fill=(70, 60, 52))
    for y in (18, 30, 40):
        d.rectangle((5, y, 26, y + 1), fill=(56, 46, 40))
    for x in range(8, 26, 4):
        d.line((x, 11, x, 45), fill=(86, 76, 66))
    d.pieslice((5, 2, 26, 18), 180, 360, fill=(120, 108, 92)); d.arc((5, 2, 26, 18), 180, 360, fill=(40, 36, 32))
    d.rectangle((14, 0, 17, 3), fill=(60, 58, 56))
    d.rectangle((27, 12, 28, 44), fill=(52, 52, 56))
    for y in range(14, 44, 3):
        d.line((26, y, 29, y), fill=(52, 52, 56))
    d.rectangle((4, 45, 27, 47), fill=(40, 36, 32)); d.point((12, 24), fill=(150, 60, 40)); d.point((17, 36), fill=(150, 60, 40))
    return im


def maquina(kind):
    im, d = new(16, 22)
    if kind == 0:   # prensa
        d.rectangle((1, 6, 14, 21), fill=(20, 22, 24)); d.rectangle((2, 7, 13, 20), fill=(74, 82, 86))
        d.rectangle((2, 7, 13, 9), fill=(112, 122, 126)); d.rectangle((6, 0, 9, 8), fill=(120, 124, 128)); d.rectangle((5, 10, 10, 12), fill=(26, 28, 30))
        d.rectangle((3, 15, 12, 19), fill=(54, 60, 64)); d.rectangle((11, 12, 12, 13), fill=(210, 70, 50))
    elif kind == 1:   # armario de tableros
        d.rectangle((2, 0, 13, 21), fill=(18, 20, 22)); d.rectangle((3, 1, 12, 20), fill=(78, 86, 80))
        for y in (3, 9, 15):
            d.rectangle((4, y, 11, y + 4), fill=(30, 34, 36))
            for x in (5, 7, 9):
                d.point((x, y + 1), fill=(90, 200, 110) if (x + y) % 3 else (214, 72, 52))
    elif kind == 2:   # motor con tubo
        d.rectangle((1, 10, 14, 21), fill=(18, 20, 22)); d.rectangle((2, 11, 13, 20), fill=(96, 70, 52))
        d.rectangle((2, 11, 13, 13), fill=(132, 98, 70)); d.rectangle((4, 14, 11, 18), fill=(52, 54, 58))
        d.rectangle((10, 0, 12, 11), fill=(84, 98, 104)); d.rectangle((9, 0, 13, 1), fill=(130, 144, 150)); d.line((2, 15, 13, 15), fill=(40, 40, 42))
    else:   # estanque con visor
        d.rectangle((1, 6, 14, 21), fill=(16, 22, 26)); d.rectangle((2, 7, 13, 20), fill=(62, 96, 108))
        d.rectangle((2, 7, 13, 9), fill=(104, 140, 150)); d.rectangle((2, 14, 13, 14), fill=(40, 62, 70))
        d.rectangle((6, 10, 9, 17), fill=(24, 40, 46)); d.rectangle((6, 14, 9, 17), fill=(60, 110, 70)); d.rectangle((6, 2, 9, 6), fill=(84, 98, 104))
    return im


def cinta(frame):
    im, d = new(16, 14)
    d.rectangle((0, 3, 15, 13), fill=(16, 18, 20)); d.rectangle((0, 4, 15, 11), fill=(48, 50, 54))
    for x in range(-3 + frame * 2, 16, 4):
        if x + 1 >= 0:
            d.rectangle((max(0, x), 5, min(15, x + 1), 9), fill=(88, 90, 92))
    d.rectangle((0, 3, 15, 4), fill=(110, 100, 44)); d.rectangle((0, 12, 15, 13), fill=(110, 100, 44))
    for x in (1, 13):
        d.rectangle((x, 11, x + 1, 13), fill=(30, 30, 34))
    return im


def valla(kind):
    im, d = new(16, 22)
    if kind == 0:   # losas de hormigon
        d.rectangle((0, 8, 15, 21), fill=(60, 60, 60)); d.rectangle((0, 9, 15, 20), fill=(96, 96, 92))
        d.rectangle((0, 8, 15, 10), fill=(122, 122, 116)); d.line((8, 10, 8, 20), fill=(64, 64, 62))
        for x in (3, 12):
            d.line((x, 14, x, 20), fill=(78, 78, 74))
        d.rectangle((0, 20, 15, 21), fill=(44, 44, 44))
        for x in range(16):   # [fab2] rollo de alambre de puas encima
            y = 4 + int(2.2 * __import__('math').sin(x * 1.3))
            d.point((x, y), fill=(120, 120, 126)); d.point((x, y + 1), fill=(46, 46, 50))
        for x in (2, 7, 12):
            d.point((x, 2), fill=(140, 140, 146)); d.point((x + 1, 3), fill=(140, 140, 146))
        for x in (4, 11):
            d.line((x, 4, x, 8), fill=(52, 52, 54))
    else:   # poste
        d.rectangle((5, 2, 10, 21), fill=(60, 60, 60)); d.rectangle((6, 3, 9, 20), fill=(104, 104, 100)); d.rectangle((6, 3, 7, 20), fill=(124, 124, 118))
        d.rectangle((4, 0, 11, 3), fill=(80, 80, 78)); d.rectangle((4, 20, 11, 21), fill=(44, 44, 44))
    return im


def techo(k):
    """[fab2] Diente de sierra de 16x16 sobre el muro de arriba de la nave: faldon de chapa y lucernario de vidrio."""
    im, d = new(16, 16)
    for x in range(16):
        top = 15 - x * 12 // 15   # faldon que sube hacia la derecha
        for y in range(top, 16):
            c = (74, 76, 82) if x % 3 else (52, 54, 58)   # ondas de la chapa
            if y == top: c = (150, 152, 158)
            if k == 1 and 4 <= x <= 8 and y > top + 2: c = (14, 14, 16) if (x + y) % 5 else (40, 40, 44)   # chapa caida: un hueco
            if k == 2 and (x * 7 + y * 3) % 11 == 0: c = (104, 60, 36)   # oxido
            d.point((x, y), fill=c)
    d.rectangle((14, 3, 15, 15), fill=(26, 30, 34))   # cara vertical: el lucernario mira al norte
    for y in range(4, 15, 3):
        d.point((14, y), fill=(120, 150, 160)); d.point((15, y + 1), fill=(80, 104, 114)); d.point((14, y + 1), fill=(60, 80, 90))
    d.line((0, 15, 15, 15), fill=(30, 30, 34))
    return im


def porton():
    """[fab2] Porton de corredera abierto, corrido sobre el muro: chapa ondulada y franja de peligro abajo."""
    im, d = new(16, 28)
    d.rectangle((0, 0, 15, 2), fill=(30, 30, 34)); d.line((0, 1, 15, 1), fill=(120, 120, 124))   # riel
    d.rectangle((0, 3, 15, 27), fill=(70, 78, 72))
    for x in range(0, 16, 2):
        d.line((x, 3, x, 25), fill=(50, 56, 52))
        d.point((x + 1, 3 + (x * 5) % 20), fill=(110, 70, 40))
    d.line((15, 3, 15, 27), fill=(24, 24, 26)); d.line((0, 3, 0, 27), fill=(104, 112, 106))
    for x in range(16):
        c = (164, 132, 40) if (x // 3) % 2 == 0 else (24, 24, 26)
        d.line((x, 23, x, 26), fill=c)
    d.rectangle((6, 12, 9, 14), fill=(30, 30, 32)); d.line((0, 27, 15, 27), fill=(20, 20, 22))
    return im


def maq2(kind):
    """[fab2] Mas maquinas: caldera, torno y casilleros."""
    im, d = new(16, 24)
    if kind == 0:   # caldera: cilindro con manometro, remaches y tubo
        d.rectangle((1, 6, 14, 23), fill=(16, 14, 14)); d.rectangle((2, 7, 13, 22), fill=(110, 58, 40))
        d.rectangle((2, 7, 4, 22), fill=(146, 82, 56)); d.rectangle((11, 7, 13, 22), fill=(70, 36, 26))
        for y in (9, 15, 20):
            for x in range(3, 13, 2): d.point((x, y), fill=(52, 30, 22))
        d.ellipse((6, 10, 10, 14), fill=(200, 196, 180)); d.line((8, 12, 9, 11), fill=(180, 40, 30))
        d.rectangle((7, 0, 9, 7), fill=(84, 90, 94)); d.rectangle((6, 0, 10, 1), fill=(126, 132, 136))
        d.rectangle((4, 17, 11, 21), fill=(26, 18, 16)); d.rectangle((5, 18, 10, 20), fill=(150, 70, 30))   # boca del fogon
    elif kind == 1:   # torno: bancada larga con cabezal
        d.rectangle((0, 12, 15, 23), fill=(16, 18, 20)); d.rectangle((1, 13, 14, 18), fill=(84, 96, 88))
        d.rectangle((1, 13, 14, 13), fill=(122, 136, 126)); d.rectangle((1, 7, 6, 14), fill=(70, 82, 74)); d.rectangle((1, 7, 6, 8), fill=(110, 124, 114))
        d.rectangle((7, 10, 13, 11), fill=(150, 150, 150)); d.rectangle((12, 8, 14, 13), fill=(60, 66, 62))
        d.rectangle((2, 19, 3, 23), fill=(40, 44, 42)); d.rectangle((12, 19, 13, 23), fill=(40, 44, 42)); d.point((4, 10), fill=(220, 180, 60))
    else:   # casilleros con puertas abiertas
        d.rectangle((0, 2, 15, 23), fill=(14, 16, 18))
        for i, x in enumerate((1, 6, 11)):
            d.rectangle((x, 3, x + 3, 22), fill=(64, 78, 92)); d.rectangle((x, 3, x + 3, 4), fill=(92, 108, 122))
            for y in (6, 8, 10): d.line((x + 1, y, x + 2, y), fill=(30, 36, 42))
            if i == 1: d.rectangle((x, 3, x + 3, 22), fill=(10, 10, 12)); d.line((x + 4, 3, x + 4, 22), fill=(86, 100, 114))
        d.point((3, 20), fill=(120, 50, 40))
    return im


SPRITES = [(f'FAB_W{k}', muro(k, 900 + k)) for k in range(6)]
SPRITES += [(f'FAB_TECHO{k}', techo(k)) for k in range(3)] + [('FAB_PORT', porton())]
SPRITES += [(f'FAB_N{k}', maq2(k)) for k in range(3)]
SPRITES += [('FAB_CHIM', chimenea()), ('FAB_TORRE', torre()), ('FAB_SILO', silo())]
SPRITES += [(f'FAB_M{k}', maquina(k)) for k in range(4)]
SPRITES += [('FAB_CINTA0', cinta(0)), ('FAB_CINTA1', cinta(1)), ('FAB_VALLA0', valla(0)), ('FAB_VALLA1', valla(1))]
