#!/bin/bash
# Compila PEPEL en WSL (pspdev en /usr/local/pspdev) y deja el paquete en release/PEPEL/
# El arte y el mapa se generan antes, en Windows: python tools/make_art.py && python tools/make_map.py
set -e
cd "$(dirname "$0")/.."
source /etc/profile.d/pspdev.sh 2>/dev/null || export PSPDEV=/usr/local/pspdev PATH=/usr/local/pspdev/bin:$PATH
rm -f src/*.o      # el Makefile no sigue los .h: se compila todo cada vez (son pocos archivos)
make -j4 "$@"
mkdir -p release/PEPEL
cp EBOOT.PBP release/PEPEL/EBOOT.PBP
[ -f assets/music.raw ] && cp assets/music.raw release/PEPEL/music.raw   # [armas2] la música va junto al EBOOT
echo "OK -> release/PEPEL/EBOOT.PBP ($(stat -c %s EBOOT.PBP) bytes)"
