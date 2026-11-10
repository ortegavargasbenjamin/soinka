# Soinnka

RPG de disparos y supervivencia en pixel art para PSP, en vista 3/4, oscuro y siniestro, al estilo de STALKER y Misery.
Juego original (no copia nombres, mundo ni arte de otros juegos). Proyecto personal de BortegaING.

- Repositorio local: `pepel-psp` (sin remoto). El nombre interno del código sigue siendo PEPEL.
- En la PSP: `PSP/GAME/SOINNKA/` con `EBOOT.PBP` y `music.raw`.
- Versión de guardado actual: 34 (`SAVE_VER` en `src/game.c`). Una partida de otra versión no carga.

---

## Cómo se juega

Partes solo con un cuchillo en **Estación Kolos**, la base segura. Sales por el portón a una **expedición** que se
genera nueva cada vez, juntas lo que puedas y vuelves antes de que caiga **la bomba**: tienes 15 minutos. Si sigues
afuera cuando llega a cero, mueres.

En la base están el comerciante (trabajos, tienda, pedidos) y el chatarrero (taller, fabricar, mejoras). Bajo la
estación está **tu búnker**, que vas ampliando.

### Supervivencia
- Hambre, sed, sangrado, radiación, frío en la nieve y vida máxima variable (los artefactos la cambian).
- **Mochila por peso:** 30 kg de base, más con la habilidad Mula, dos mochilas y la Semilla hueca. Sobre el 100 % no
  corres y vas lento; sobre el 140 % no recoges nada.
- **Armero:** las armas se guardan en la base; solo pesan las que llevas.
- Curarse tarda 1,5 s y se corta si te pegan.
- **Escasez:** poca munición, medicinas y comida. Las cajas normales no dan balas; la munición y las armas salen de las
  **cajas militares verdes**, de los bandidos y del comerciante.

### Reloj del día y clima
- La luz sigue la hora: mañana gris, tarde, crepúsculo rojizo, noche cerrada, madrugada azul. Dormir lleva a las 07:00.
- Climas ocasionales: niebla, luna verdosa, **lluvia** (tapa tus pasos) y nieve.
- Los enemigos te ven poco a oscuras; con luz cerca (fuego, farol, linterna) te ven de más lejos.

### Modos
- **Superviviente:** el normal, con escasez.
- **Explorador:** el accesible.
- **Hierro:** como Superviviente, pero la partida se borra al morir.

---

## Mapas

Expediciones de 192x120 casillas, con tres puntos de retirada, sectores más peligrosos lejos de la entrada, edificios
grandes con pisos, casas con segundo piso, anomalías, eventos y tres trazados posibles (cuadrícula, avenida con plaza,
quebrada con puentes).

| Mapa | Lo propio |
|---|---|
| Patio de trenes | Vías, vagones, contenedores |
| Barrio en ruinas | 40 a 50 edificios, casas de pueblo |
| Bosque muerto | Árboles, hojarasca |
| Fábrica abandonada | Naves en diente de sierra, chimeneas, pasarelas, sodio y balizas |
| Cementerio | Tumbas, grava |
| Pueblo minero | Casas de tablas, roca |
| Bosque de pinos | Senderos, cabaña, lobos |
| Ciudad abandonada | Más de 110 edificios, semáforos, buses, kioscos y el **metro** debajo |
| Hospital | Edificio enorme con 12 salas, farmacia, morgue, ambulancias |
| Puerto | Muelles, grúas, contenedores, barco encallado |
| Planta atómica | Torres de enfriamiento, reactor, radiación; peligro +3 y el mejor botín |
| Tierra maldita | Río, lagos, puentes y ruinas del paquete Cursed Land |

La Autopista existe en el código pero está **fuera del sorteo** (al dueño no le gustó).

Otras zonas: tu búnker, el búnker de las expediciones, **El Pozo** (búnker por pisos sin fin, con ascensor y guardián
cada 5 pisos), los segundos pisos de las casas y el metro.

---

## Combate

- **Cuerpo a cuerpo con empuje:** cuchillo, hacha, tubo, mazo. Golpe sigiloso por la espalda.
- **Armas de fuego:** Makarov, recortada, AK, revólver, subfusil, escopeta de corredera, fusil de cerrojo y ocho del
  Pixel Gun Pack (Víbora, Chacal, Zumbador, Tormenta, Cuervo, Halcón, Centinela, Aplanadora).
- **Piezas:** cinco ranuras (boca, cargador, mira, culata, accesorio) y **ocho piezas únicas**, una sola de cada una.
- Armas que se gastan, se traban, se reparan con kit o con el chatarrero, y se pueden botar al suelo.
- **Objeto de mano:** tuerca, granada, cóctel, mina, cepo (L + Izq/Der elige, L + Triángulo lanza).

### Enemigos
- Zombis (chico, del hacha, grande, corredor, escupidor, acorazado, irradiados), lobos, bandidos y francotiradores.
- **Minijefes sueltos:** Capitán Chamusco, Teniente Garrote, Alfa Colmillo, El Brillante.
- **Jefes con su zona** (se llega con un trabajo; en la puerta aparecen aliados y preguntan si van): La Bestia del
  charco (Pantano), Cabeza de Hierro (Base militar), La Viuda blanca (Lago helado), El Capataz (Fuerte), El Fermentado
  (Vertedero).
- **Niveles infernales:** tras el final, el peligro sube hasta 15, con jefes sueltos y reliquias.

---

## Progreso

- Trabajos de Kolos y de los Faroleros (se cancelan con Cuadrado), pedidos del comerciante, reputación con cada bando.
- Nueve habilidades por rango.
- Diez artefactos, cada uno con ventaja y costo (por ejemplo, más vida máxima a cambio de radiación), en tres ranuras.
- **Tu búnker:** salas selladas que se abren con chatarra, cajas propias, decoraciones, trofeos de jefes, generador con
  combustible y filtro de agua.
- Hollín, el perro: vive en el búnker y solo sale a las batallas con jefes.
- Tres finales.

Se quitaron a pedido del dueño: la cinemática de entrada, las notas del diario y el manual del README.

---

## Controles

| Botón | Acción |
|---|---|
| Stick | Moverse (a medias: sigilo) |
| Círculo | Correr |
| L | Fijar blanco y apuntar |
| R | Disparar o golpear |
| Cuadrado | Recargar o destrabar (mantenido: tipo de munición) |
| Cruz | Abrir, recoger, usar |
| Arriba / Abajo | Curarse / comer o beber |
| Izq. / Der. | Cambiar arma |
| L + Izq./Der. | Elegir objeto de mano |
| L + Triángulo | Lanzar o poner el objeto de mano |
| Triángulo | Mochila |
| SELECT | Mapa (en tu búnker: modo decorar) |
| START | Pausa |

---

## Código

C para PSP con pspdev, sin motor externo. Dibujo 2D propio con un atlas de dos páginas de 512x512 y sonido sintetizado
por código, más la música desde archivo.

| Archivo | Qué tiene |
|---|---|
| `src/game.c` | Casi todo el juego (más de 9.000 líneas) |
| `src/r2d.c`, `r2d.h` | Dibujo 2D por lotes, páginas de atlas, imagen desde memoria |
| `src/snd.c`, `snd_synth.c` | Mezcla, efectos sintetizados y música en bucle desde `music.raw` |
| `src/*.inc` | Cada sistema por tema: `peso`, `duro`, `artef`, `bomba`, `diseno`, `arte`, `stalk`, `mapas`, `mapas3`, `mapnuc`, `urbe`, `pozo`, `jefes`, `jefes2`, `armas`, `armas2`, `bunker` |
| `tools/make_atlas.py` | Arma el atlas desde los paquetes de arte y los módulos `make_art`, `make_arte`, `make_fab` |
| `tools/build.sh` | Compila y deja `release/PEPEL/` |
| `tools/emu.ps1` | Lanza el emulador oculto |

### Compilar
```
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -u root -- bash -lc "cd /mnt/c/Users/Usuario/Desktop/Proyectos_Claude/pepel-psp && bash tools/build.sh"
```
Con `EXTRA=-DPEPEL_DEBUG` sale la versión de pruebas (guion `dbg_script`, registro `dbg_log.txt`, prueba del generador
con la letra X en `dbg_cmd`). Si cambia el arte: `python tools/make_atlas.py`.

### Arte
Los paquetes no van en el repositorio (sus licencias no permiten redistribuir); se leen desde
`Downloads/_soinnka_arte`: PostApocalypse AssetPack (TheLazyStone), Zombie Apocalypse Tileset (Ittai Manero),
StalkerLike, Abandoned City (Glerf), Free Pixel Gun Pack y Cursed Land. Uso solo personal.
