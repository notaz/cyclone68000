
# Makefile for GP32

export CCBASE=D:/Devkitadv
export CUSER=-DLITTLE_ENDIAN -DGP32 -W -Wall
include $(CCBASE)/gp32.mk

OBJS = Area.o Cart.o Disa.o Draw.o Memory.o Pico.o Psnd.o \
       Sek.o Sine.o VideoPort.o ym2612.o ../Cyclone/Cyclone.o

Pico.a: $(OBJS)
	ar -rs $@ $(OBJS)


Draw.o: Draw.cpp
	gcc $(CFLAGS) -S $<
	gcc $(CFLAGS) -c $<
