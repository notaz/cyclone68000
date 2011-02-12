
// This file is part of the PicoDrive Megadrive Emulator

// Copyright (c) 2011 FinalDave (emudave (at) gmail.com)

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "PicoInt.h"

int PicoVer=0x0030;
struct Pico Pico;
int PicoOpt=0;

int PicoPad[2]; // Joypads, format is SACB RLDU

int PicoInit()
{
  // Blank space for state:
  memset(&Pico,0,sizeof(Pico));
  memset(&PicoPad,0,sizeof(PicoPad));
  Pico.m.dirtyPal=1;

  // Init CPU:
  SekInit();

  // Setup memory callbacks:
  PicoMemInit();
  PsndReset();

#ifdef MSOUND
  YM2612Init(1,7670443,PsndRate,NULL,NULL);
#endif
  return 0;
}

void PicoExit()
{
#ifdef MSOUND
  YM2612Shutdown();
#endif

  memset(&Pico,0,sizeof(Pico));
}

int PicoReset()
{
  unsigned int region=0;
  int support=0,hw=0,i=0;
  unsigned char pal=0;

  if (Pico.romsize<=0) return 1;

  SekReset();
  PsndReset();
#ifdef MSOUND
  YM2612ResetChip(0);
#endif

  // Read cartridge region data:
  region=PicoRead32(0x1f0);

  for (i=0;i<4;i++)
  {
    int c=0;
    
    c=region>>(i<<3); c&=0xff;
    if (c<=' ') continue;

         if (c=='J') support|=1;
    else if (c=='U') support|=4;
    else if (c=='E') support|=8;
    else
    {
      // New style code:
      char s[2]={0,0};
      s[0]=(char)c;
      support|=strtol(s,NULL,16);
    }

  }

  // Try to pick the best hardware value for English/60hz:
       if (support&4)   hw=0x80;          // USA
  else if (support&8) { hw=0xc0; pal=1; } // Europe
  else if (support&1)   hw=0x00;          // Japan NTSC
  else if (support&2) { hw=0x40; pal=1; } // Japan PAL
  else hw=0x80; // USA

  Pico.m.hardware=(unsigned char)(hw|0x20); // No disk attached
  Pico.m.pal=pal;

  return 0;
}

static int CheckIdle()
{
  unsigned char state[0x88];

  memset(state,0,sizeof(state));

  // See if the state is the same after 2 steps:
  SekState(state); SekRun(0); SekRun(0); SekState(state+0x44);
  if (memcmp(state,state+0x44,0x44)==0) return 1;

  return 0;
}

// Accurate but slower frame which does hints
static int PicoFrameHints()
{
  struct PicoVideo *pv=&Pico.video;
  int total=0,aim=0;
  int y=0;
  int hint=0x400; // Hint counter

  pv->status|=0x08; // Go into vblank

  for (y=-38;y<224;y++)
  {
    if (y==0)
    {
      hint=pv->reg[10]; // Load H-Int counter
      if (pv->reg[1]&0x40) pv->status&=~8; // Come out of vblank if display enabled
    }

    // H-Interrupts:
    if (hint<0)
    {
      hint=pv->reg[10]; // Reload H-Int counter
      if (pv->reg[0]&0x10) SekInterrupt(4);
    }

    // V-Interrupt:
    if (y==-37)
    {
      pv->status|=0x80; // V-Int happened
      if (pv->reg[1]&0x20) SekInterrupt(6);
    }

    Pico.m.scanline=(short)y;

    // Run scanline:
    aim+=489; total+=SekRun(aim-total);

    hint--;

    if (PicoScan && y>=0) PicoLine(y);
  }

  SekInterrupt(0); // Cancel interrupt

  return 0;
}

// Simple frame without H-Ints
static int PicoFrameSimple()
{
  int total=0,y=0,aim=0;
  
  Pico.m.scanline=-64;

  // V-Blanking period:
  if (Pico.video.reg[1]&0x20) SekInterrupt(6); // Set IRQ
  Pico.video.status|=0x88; // V-Int happened / go into vblank
  total+=SekRun(18560);

  // Active Scan:
  if (Pico.video.reg[1]&0x40) Pico.video.status&=~8; // Come out of vblank if display is enabled
  SekInterrupt(0); // Clear IRQ

  // Run in sections:
  for (aim=18560+6839; aim<=18560+6839*16; aim+=6839)
  {
    int add=0;
    if (CheckIdle()) break;
    add=SekRun(aim-total);
    total+=add;
  }

  if (PicoMask&0x100)
  if (PicoScan)
  {
    // Draw the screen
    for (y=0;y<224;y++) PicoLine(y);
  }

  return 0;
}

int PicoFrame()
{
  int hints=0;

  if (Pico.rom==NULL) return 1; // No Rom plugged in


  PmovUpdate();

  hints=Pico.video.reg[0]&0x10;

  if (hints) PicoFrameHints();
  else PicoFrameSimple();

  PsndRender();

  return 0;
}

static int DefaultCram(int cram)
{
  int high=0x0841;
  // Convert 0000bbbb ggggrrrr
  // to      rrrr1ggg g10bbbb1
  high|=(cram&0x00f)<<12; // Red
  high|=(cram&0x0f0)<< 3; // Green
  high|=(cram&0xf00)>> 7; // Blue
  return high;
}

// Function to convert Megadrive Cram into a native colour:
int (*PicoCram)(int cram)=DefaultCram;
