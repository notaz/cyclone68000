
// This file is part of the PicoDrive Megadrive Emulator

// This code is licensed under the GNU General Public License version 2.0 and the MAME License.
// You can choose the license that has the most advantages for you.

// SVN repository can be found at http://code.google.com/p/cyclone68000/

#include "PicoInt.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

static int PicoMemBase(u32 pc)
{
  int membase=0;

  if (pc<Pico.romsize)
  {
    membase=(int)Pico.rom; // Program Counter in Rom
  }
  else if ((pc&0xe00000)==0xe00000)
  {
    membase=(int)Pico.ram-(pc&0xff0000); // Program Counter in Ram
  }
  else
  {
    // Error - Program Counter is invalid
    membase=(int)Pico.rom;
  }

  return membase;
}

#ifdef EMU_A68K
extern "C" u8 *OP_ROM=NULL,*OP_RAM=NULL;

static void CPU_CALL PicoCheckPc(u32 pc)
{
  OP_ROM=(u8 *)PicoMemBase(pc);

  // don't bother calling us back unless it's outside the 64k segment
  M68000_regs.AsmBank=(pc>>16);
}
#endif

#ifdef EMU_C68K
static u32 PicoCheckPc(u32 pc)
{
  pc-=PicoCpu.membase; // Get real pc
  pc&=0xffffff;

  PicoCpu.membase=PicoMemBase(pc);

  return PicoCpu.membase+pc;
}
#endif

#ifdef EMU_NULL
static u32 PicoCheckPc(u32) { return 0; }
#endif


int PicoInitPc(u32 pc)
{
  PicoCheckPc(pc);
  return 0;
}

// -----------------------------------------------------------------
static int PadRead(int i)
{
  int pad=0,value=0;
  pad=~PicoPad[i]; // Get inverse of pad

  if (Pico.m.padSelect[i]) value=0x40|(pad&0x3f); // 01CB RLDU
  else value=((pad&0xc0)>>2)|(pad&3); // 00SA 00DU

  return (value<<8)|value; // Mirror bytes
}

static u32 OtherRead16(u32 a)
{
  u32 d=0;

  if ((a&0xffe000)==0xa00000)
  {
    // Z80 ram
    d=*(u16 *)(Pico.zram+(a&0x1fff));

    if (Pico.m.rotate&2) { d=(Pico.m.rotate>>2)&0xff; d|=d<<8; } // Fake z80
    Pico.m.rotate++;

    goto end;
  }

  if ((a&0xffe000)==0xa00000)
  {
    // Fake Z80 ram
    d=((Pico.m.rotate++)>>2)&0xff; d|=d<<8;
    goto end;
  }
  
  if (a==0xa04000) { d=Pico.m.rotate&3; Pico.m.rotate++; goto end; } // Fudge
  if (a==0xa10000) { d=Pico.m.hardware; goto end; }  // Hardware value
  if (a==0xa10002) { d=PadRead(0); goto end; }
  if (a==0xa10004) { d=PadRead(1); goto end; }
  if (a==0xa11100) { d=((Pico.m.rotate++)&4)<<6; goto end; } // Fudge z80 reset

  if ((a&0xffffe0)==0xc00000) { d=PicoVideoRead(a); goto end; }

end:
  return d;
}

static void OtherWrite8(u32 a,u32 d)
{
  if ((a&0xffe000)==0xa00000) { Pico.zram[(a^1)&0x1fff]=(u8)d; return; } // Z80 ram
  if ((a&0xfffffc)==0xa04000) { PsndFm(a,d); return; } // FM Sound

  if (a==0xa11100) { Pico.m.z80Run=(u8)(d^1); return; }
  if (a==0xa10003) { Pico.m.padSelect[0]=(u8)((d>>6)&1); return; } // Joypad 1 select
  if (a==0xa10005) { Pico.m.padSelect[1]=(u8)((d>>6)&1); return; } // Joypad 2 select

  if ((a&0xffffe0)==0xc00000) { PicoVideoWrite(a,d|(d<<8)); return; } // Byte access gets mirrored
}

static void OtherWrite16(u32 a,u32 d)
{
  if ((a&0xffffe0)==0xc00000) { PicoVideoWrite(a,d); return; }
  if ((a&0xffe000)==0xa00000) { *(u16 *)(Pico.zram+(a&0x1ffe))=(u16)d; return; } // Z80 ram

  OtherWrite8(a,  d>>8);
  OtherWrite8(a+1,d&0xff);
}

// -----------------------------------------------------------------
//                     Read Rom and read Ram

static u8 CPU_CALL PicoRead8(u32 a)
{
  u32 d=0;
  a&=0xffffff;

  if (a<Pico.romsize) return *(u8 *)(Pico.rom+(a^1)); // Rom
  if ((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram

  d=OtherRead16(a&~1); if ((a&1)==0) d>>=8;
  return (u8)d;
}

u16 CPU_CALL PicoRead16(u32 a)
{
  a&=0xfffffe;

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); return pm[0]; } // Rom
  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xffff)); return pm[0]; } // Ram

  return (u16)OtherRead16(a);
}

u32 CPU_CALL PicoRead32(u32 a)
{
  a&=0xfffffe;

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); return (pm[0]<<16)|pm[1]; } // Rom
  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xffff)); return (pm[0]<<16)|pm[1]; } // Ram

  return (OtherRead16(a)<<16)|OtherRead16(a+2);
}

// -----------------------------------------------------------------
//                            Write Ram

static void CPU_CALL PicoWrite8(u32 a,u8 d)
{
  if ((a&0xe00000)==0xe00000) { u8 *pm=(u8 *)(Pico.ram+((a^1)&0xffff)); pm[0]=d; return; } // Ram

  a&=0xffffff;
  OtherWrite8(a,d);
}

static void CPU_CALL PicoWrite16(u32 a,u16 d)
{
  if ((a&0xe00000)==0xe00000) { *(u16 *)(Pico.ram+(a&0xfffe))=d; return; } // Ram

  a&=0xfffffe;
  OtherWrite16(a,d);
}

static void CPU_CALL PicoWrite32(u32 a,u32 d)
{
  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;
  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}


// -----------------------------------------------------------------
int PicoMemInit()
{
#ifdef EMU_C68K
  // Setup memory callbacks:
  PicoCpu.checkpc=PicoCheckPc;
  PicoCpu.fetch8 =PicoCpu.read8 =PicoRead8;
  PicoCpu.fetch16=PicoCpu.read16=PicoRead16;
  PicoCpu.fetch32=PicoCpu.read32=PicoRead32;
  PicoCpu.write8 =PicoWrite8;
  PicoCpu.write16=PicoWrite16;
  PicoCpu.write32=PicoWrite32;
#endif
  return 0;
}

#ifdef EMU_A68K
struct A68KInter
{
  u32 unknown;
  u8  (__fastcall *Read8) (u32 a);
  u16 (__fastcall *Read16)(u32 a);
  u32 (__fastcall *Read32)(u32 a);
  void (__fastcall *Write8)  (u32 a,u8 d);
  void (__fastcall *Write16) (u32 a,u16 d);
  void (__fastcall *Write32) (u32 a,u32 d);
  void (__fastcall *ChangePc)(u32 a);
  u8  (__fastcall *PcRel8) (u32 a);
  u16 (__fastcall *PcRel16)(u32 a);
  u32 (__fastcall *PcRel32)(u32 a);
  u16 (__fastcall *Dir16)(u32 a);
  u32 (__fastcall *Dir32)(u32 a);
};

extern "C" struct A68KInter a68k_memory_intf=
{
  0,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoWrite8,
  PicoWrite16,
  PicoWrite32,
  PicoCheckPc,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoRead16, // unused
  PicoRead32, // unused
};
#endif
