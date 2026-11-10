# Soinnka

RPG de disparos y supervivencia para PSP, vista desde arriba, en pixel art. Juego ORIGINAL: mundo,
nombres y dibujos propios (inspirado en el genero de Stalker / Metro / Scavland, sin copiar ninguno).

Mundo: una cuenca industrial de la vieja Union, sellada tras un incendio que no se apaga. Cada cierto
tiempo cae la Ceniza y hay que estar bajo techo. Trabajas para el asentamiento Estacion Kolos.

## Compilar y probar

    python tools/make_atlas.py    # arma el atlas (assets/atlas.rgba, src/atlas.h) con los paquetes de arte
    python tools/make_map.py      # arma la zona (src/map1.h)
    wsl -d Ubuntu-24.04 -u root -- bash -lc "cd /mnt/c/.../pepel-psp && bash tools/build.sh"
    tools/emu.ps1                 # lo abre OCULTO en el PPSSPP de noctis-psp
    (desde noctis-psp) python tools/emu/psp_eye.py "start:150" shot:nombre

`bash tools/build.sh EXTRA=-DPEPEL_DEBUG` agrega atajos de prueba en la pausa (START): SELECT = mochila al tope (con piezas,
armaduras nuevas, máscara y kits) y modo sin daño; Triángulo = morir; Cuadrado = tormenta en 3 s; Círculo = vida 30, hambre 30
y sed 20; L = hambre y sed en 1; R = +200 de chatarra y +2 de reputación; Arriba = todas las armas con el cargador lleno
(la segunda vez, gastadas al 25 %); Abajo = abre todos los cajones y cuerpos de la zona; Derecha = salta al siguiente lugar de
la lista (cajones, catre, zombi grande, jefe, máscara, almacén, bandidos, salida este y, al final, Barrio quemado, Chofer,
escotilla y búnker; los de misma zona dejan la pausa puesta, así se encadenan con varias pulsaciones y START al final);
Izquierda = vuelve al portón. Si existe un archivo vacío llamado dbg_all junto al EBOOT, la partida nueva ya empieza con
todas las armas, la mochila al tope y sin daño. Arriba se ve el medidor de milisegundos por cuadro, la vida, el hambre, la
sed y la radiación. La versión normal no trae nada de esto (en el emulador, 3 a 6 ms por cuadro en todas las zonas).

## Archivos

- src/r2d.c: dibujo 2D por lotes (un atlas de 512x256, sin suavizado)
- src/game.c: todo el juego; src/snd.c y src/snd_synth.c: el sonido (efectos sintetizados por código, con hilo de audio)
- tools/make_atlas.py: arma el atlas con el pixel art de los paquetes (ver Arte); tools/make_map.py: el mapa
- tools/make_art.py: dibuja por codigo lo que los paquetes no traen (make_atlas.py lo importa). Si se corre
  solo, deja el atlas viejo, todo dibujado por codigo, que ya no calza con src/game.c

## Arte

El pixel art viene de tres paquetes hechos a mano por otros autores. No estan en este repo: tools/make_atlas.py
los lee desde `%USERPROFILE%\Downloads\_soinnka_arte` (o desde la carpeta que diga la variable SOINNKA_ARTE).

- Personaje, enemigos y objetos (barriles, cajones, farol, basura, iconos de botin en el suelo):
  "Post-Apocalyptic Pixel Art Asset Pack" v1.1.2, de TheLazyStone
  (https://thelazystone.itch.io/post-apocalyptic-pixel-art-asset-pack).
- Mapa (tierra, pasto, camino, piso, muros, rejas, arboles, arbustos, tumbas, sangre):
  "Zombie Apocalypse Tileset", de Ittai Manero.
- Armas, autos e interfaz: "StalkerLike" (carpeta `StalkerLike\Sprites`, 49 PNG). ORIGEN Y LICENCIA SIN IDENTIFICAR:
  no se sabe quien lo hizo ni que permite, asi que es SOLO PARA USO PERSONAL. De ese paquete se usan solo cuatro cosas:
  - armas: la pistola, la escopeta y el fusil AK en la mano (la escopeta, cortada para que sea la recortada; el fusil,
    sin la culata; las manos son las del personaje de TheLazyStone), el fogonazo, la bala, el casquillo que salta y los
    iconos de arma y de munición (Weapons, Weapons_Icons, Ammo);
  - autos: los dos sedanes de costado de la hoja Cars, oscurecidos y oxidados. Miden 50 px y el auto del mapa ocupa
    2 casillas (32 px): se dibujan enteros, centrados en ellas, y asoman 9 px por lado sin cambiar lo solido;
  - interfaz: la mochila (casilleros y paneles de InventoryUI_Overhoul, SlotMarker, iconos de ItemsIcons y Ammo), la
    barra de vida (HP) y la mira del apuntado (CrossHair, a la mitad);
  - el catre: la cama de InteriorObjects.
  Lo demas de StalkerLike (personajes, casas, suelos, arboles, Equipment...) no se usa.
- Sigue dibujado por codigo (tools/make_art.py): letras, iconos del HUD (radiacion, aguante, sangrado, chatarra),
  llamas, sombras, contenedores, vias, umbral, charco radiactivo, huesos, ceniza y la estaca. La botella de agua
  (ningun paquete trae una) se dibuja por codigo en tools/make_atlas.py.

Licencia: el paquete de TheLazyStone es gratis SOLO para uso no comercial (el uso comercial se paga) y
PROHIBE redistribuirlo, tal cual o modificado. assets/atlas.rgba y el EBOOT llevan ese arte adentro, asi que
ESTE REPO NO DEBE PUBLICARSE NI COMPARTIRSE CON ESE ARTE (ni el repo ni el EBOOT), y el juego no puede
venderse asi. El paquete StalkerLike suma otro motivo: sin saber su licencia, tampoco se puede publicar ni compartir.
Para publicar: volver al arte propio con `python tools/make_art.py` (y al src/game.c de antes
de este cambio) o reemplazar los paquetes por arte con licencia que lo permita.
