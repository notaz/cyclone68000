
// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Pico.h"

#if defined(__GNUC__) || defined(_WIN32_WCE)
#define EMU_C68K // Use the Cyclone 68000 emulator
#else
#define EMU_A68K // Use the 'A68K' (Make68K) Assembler 68000 emulator
#endif

//#define MSOUND

// Disa.h also defines CPU_CALL to be fastcall or normal call
#include "Disa.h"


// ----------------------- 68000 CPU -----------------------
#ifdef EMU_A68K
// The format of the data in a68k.asm (at the _M68000_regs location)
struct A68KContext
{
  unsigned int d[8],a[8];
  unsigned int isp,srh,ccr,xc,pc,irq,sr;
  int (*IrqCallback) (int nIrq);
  unsigned int ppc;
  void *pResetCallback;
  unsigned int sfc,dfc,usp,vbr;
  unsigned int AsmBank,CpuVersion;
};
extern "C" struct A68KContext M68000_regs;
#endif

#ifdef EMU_C68K
#include "../Cyclone/Cyclone.h"
extern struct Cyclone PicoCpu;
#endif

#ifdef MSOUND
extern "C" {
#include "ym2612.h"
}
#endif

// ---------------------------------------------------------

struct PicoVideo
{
  unsigned char reg[0x20];
  unsigned int command; // 32-bit Command
  unsigned char pending; // 1 if waiting for second half of 32-bit command
  unsigned char type; // Command type (v/c/vsram read/write)
  unsigned short addr; // Read/Write address
  int status; // Status bits
  unsigned char pad[0x14];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padSelect[2]; // Select high or low bit from joypad
  short scanline; // -38 to 223
  char dirtyPal; // Is the palette dirty
  unsigned char hardware; // Hardware value for country
  unsigned char pal; // 1=PAL 0=NTSC
  unsigned char pad[0x16];
};

struct PicoSound
{
  unsigned char fmsel[2]; // FM selected register
  unsigned char reg[0x100];
  unsigned char pad[0x3e];
};

struct Pico
{
  unsigned char ram[0x10000]; // scratch ram
  unsigned short vram[0x8000];
  unsigned char zram[0x2000]; // Z80 ram
  unsigned int highpal[0x40];
  unsigned short cram[0x40];
  unsigned short vsram[0x40];

  unsigned char *rom;
  unsigned int romsize;

  struct PicoMisc m;
  struct PicoVideo video;
  struct PicoSound s;
};

// Draw.cpp
int PicoLine(int scan);

// Draw2.cpp
int PicoDraw2();

// Memory.cpp
int PicoInitPc(unsigned int pc);
unsigned short CPU_CALL PicoRead16(unsigned int a);
unsigned int CPU_CALL PicoRead32(unsigned int a);
int PicoMemInit();
void PicoDasm(int start,int len);

// Pico.cpp
extern struct Pico Pico;

// Sek.cpp
int SekInit();
int SekReset();
int SekRun(int cyc);
int SekInterrupt(int irq);
int SekPc();
void SekState(unsigned char *data);

// Sine.cpp
extern short Sine[];

// Psnd.cpp
int PsndReset();
int PsndFm(int a,int d);
int PsndRender();

// VideoPort.cpp
void PicoVideoWrite(unsigned int a,unsigned int d);
unsigned int PicoVideoRead(unsigned int a);

// External:
extern "C" int dprintf(char *Format, ...);
