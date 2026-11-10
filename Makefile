TARGET = pepel
OBJS = src/main.o src/r2d.o src/game.o src/assets.o src/snd.o src/snd_synth.o

INCDIR = src
CFLAGS = -O2 -G0 -Wall -Wno-unused-function -DPSP_FW_VERSION=660 $(EXTRA)
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpsppower -lpspaudio -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Soinnka
PSP_EBOOT_ICON = assets/ICON0.PNG
PSP_EBOOT_PIC1 = assets/PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
